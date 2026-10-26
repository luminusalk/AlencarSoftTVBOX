/**
 * @file hls_bridge.c
 * @brief Implementacao nativa da ponte HLS (.m3u8 -> FIFO continuo /tmp/live_stream.ts para o chip).
 */

#include "hls_bridge.h"
#include "hls_playlist.h"
#ifdef HLS_WITH_MBEDTLS
#include "hls_tls.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <strings.h>
#include <sys/time.h>

#define FIFO_STREAM_PATH "/tmp/live_stream.ts"

#define RING_BUFFER_SIZE (4 * 1024 * 1024) /* 4 MB buffer */
#define MAX_SEGMENT_SIZE (8 * 1024 * 1024) /* Limite explicito por segmento */

typedef struct {
    uint8_t *data;
    size_t   size;
    size_t   head;
    size_t   tail;
    size_t   count;
    pthread_mutex_t lock;
    pthread_cond_t  can_read;
    pthread_cond_t  can_write;
} ring_buffer_t;

static volatile int s_bridge_running = 0;
static volatile int s_data_ready = 0;
static char s_current_m3u8[1024] = {0};
static pthread_t s_producer_thread;
static pthread_t s_feeder_thread;
static ring_buffer_t s_ring;
static int s_fifo_fd = -1;

/* Funcoes do Ring Buffer */
static int ring_init(ring_buffer_t *rb, size_t size) {
    rb->data = (uint8_t *)malloc(size);
    if (!rb->data) return -1;
    rb->size = size;
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
    pthread_mutex_init(&rb->lock, NULL);
    pthread_cond_init(&rb->can_read, NULL);
    pthread_cond_init(&rb->can_write, NULL);
    return 0;
}

static void ring_free(ring_buffer_t *rb) {
    if (rb->data) {
        free(rb->data);
        rb->data = NULL;
    }
    pthread_mutex_destroy(&rb->lock);
    pthread_cond_destroy(&rb->can_read);
    pthread_cond_destroy(&rb->can_write);
}

static void ring_reset(ring_buffer_t *rb) {
    pthread_mutex_lock(&rb->lock);
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
    pthread_cond_broadcast(&rb->can_write);
    pthread_cond_broadcast(&rb->can_read);
    pthread_mutex_unlock(&rb->lock);
}

static int ring_write(ring_buffer_t *rb, const uint8_t *buf, size_t len) {
    pthread_mutex_lock(&rb->lock);
    while (s_bridge_running && (rb->size - rb->count < len)) {
        pthread_cond_wait(&rb->can_write, &rb->lock);
    }
    if (!s_bridge_running) {
        pthread_mutex_unlock(&rb->lock);
        return -1;
    }

    size_t first_part = rb->size - rb->head;
    if (len <= first_part) {
        memcpy(rb->data + rb->head, buf, len);
        rb->head = (rb->head + len) % rb->size;
    } else {
        memcpy(rb->data + rb->head, buf, first_part);
        memcpy(rb->data, buf + first_part, len - first_part);
        rb->head = len - first_part;
    }
    rb->count += len;
    pthread_cond_broadcast(&rb->can_read);
    pthread_mutex_unlock(&rb->lock);
    return (int)len;
}

static int ring_read(ring_buffer_t *rb, uint8_t *buf, size_t max_len) {
    pthread_mutex_lock(&rb->lock);
    while (s_bridge_running && rb->count == 0) {
        pthread_cond_wait(&rb->can_read, &rb->lock);
    }
    if (rb->count == 0) {
        pthread_mutex_unlock(&rb->lock);
        return 0;
    }

    size_t to_read = (max_len < rb->count) ? max_len : rb->count;
    size_t first_part = rb->size - rb->tail;
    if (to_read <= first_part) {
        memcpy(buf, rb->data + rb->tail, to_read);
        rb->tail = (rb->tail + to_read) % rb->size;
    } else {
        memcpy(buf, rb->data + rb->tail, first_part);
        memcpy(buf + first_part, rb->data, to_read - first_part);
        rb->tail = to_read - first_part;
    }
    rb->count -= to_read;
    pthread_cond_broadcast(&rb->can_write);
    pthread_mutex_unlock(&rb->lock);
    return (int)to_read;
}

/* Parser simples de URL HTTP */
static int parse_http_url(const char *url, char *host, size_t host_size,
                          int *port, int *use_tls, char *path, size_t path_size) {
    const char *authority, *end, *colon, *path_start;
    size_t length;
    char *port_end;
    long parsed_port;
    if (strncmp(url, "http://", 7) == 0) {
        *use_tls = 0;
        authority = url + 7;
    } else if (strncmp(url, "https://", 8) == 0) {
        *use_tls = 1;
        authority = url + 8;
    } else return -1;
    end = strpbrk(authority, "/?#");
    if (!end) end = authority + strlen(authority);
    colon = memchr(authority, ':', (size_t)(end - authority));
    length = (size_t)((colon ? colon : end) - authority);
    if (!length || length >= host_size || memchr(authority, '@', (size_t)(end - authority))) return -1;
    memcpy(host, authority, length);
    host[length] = '\0';
    *port = *use_tls ? 443 : 80;
    if (colon) {
        errno = 0;
        parsed_port = strtol(colon + 1, &port_end, 10);
        if (errno || port_end != end || parsed_port < 1 || parsed_port > 65535) return -1;
        *port = (int)parsed_port;
    }
    path_start = end;
    if (*path_start == '/' && strlen(path_start) < path_size) {
        strcpy(path, path_start);
    } else if ((*path_start == '?' || *path_start == '#') && strlen(path_start) + 1 < path_size) {
        path[0] = '/';
        strcpy(path + 1, path_start);
    } else if (!*path_start && path_size >= 2) {
        strcpy(path, "/");
    } else return -1;
    return 0;
}

/* Cliente HTTP basico para baixar dados */
static int http_fetch_mode(const char *url, uint8_t *out_buf, size_t max_size,
                           size_t *out_size, int mode, int redirects, char *effective_url) {
    char redirect_url[1024] = {0};
    char host[256];
    int port = 80;
    char path[1024];
    int fd, status = 0, has_length = 0, flags, so_error = 0, use_tls = 0;
#ifdef HLS_WITH_MBEDTLS
    hls_tls_t *tls = NULL;
#endif
    size_t total = 0, content_length = 0, header_pos = 0;
    char header_buf[8192];
    struct hostent *he;
    struct timeval tv;
    struct sockaddr_in saddr;
    char req[2048];
    int req_len;
    size_t sent = 0;
    int in_body = 0, complete = 0;

    *out_size = 0;
    if (parse_http_url(url, host, sizeof(host), &port, &use_tls,
                       path, sizeof(path)) < 0) {
        fprintf(stderr, "[HLS Bridge] URL HTTP(S) invalida.\n");
        return -1;
    }
#ifndef HLS_WITH_MBEDTLS
    if (use_tls) {
        fprintf(stderr, "[HLS Bridge] Binario sem suporte HTTPS.\n");
        return -1;
    }
#endif

    he = gethostbyname(host);
    if (!he || he->h_addrtype != AF_INET ||
        (size_t)he->h_length != sizeof(saddr.sin_addr)) return -1;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    tv.tv_sec = 4;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    memcpy(&saddr.sin_addr, he->h_addr_list[0], he->h_length);

    flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        close(fd); return -1;
    }
    if (connect(fd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
        struct pollfd pfd;
        socklen_t so_len = sizeof(so_error);
        if (errno != EINPROGRESS) { close(fd); return -1; }
        pfd.fd = fd;
        pfd.events = POLLOUT;
        pfd.revents = 0;
        if (poll(&pfd, 1, 4000) <= 0 ||
            getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_error, &so_len) < 0 || so_error) {
            close(fd); return -1;
        }
    }
    if (fcntl(fd, F_SETFL, flags) < 0) { close(fd); return -1; }

#ifdef HLS_WITH_MBEDTLS
    if (use_tls) {
        tls = hls_tls_open(fd, host);
        if (!tls) { close(fd); return -1; }
    }
#endif

    req_len = snprintf(req, sizeof(req),
             "GET %s HTTP/1.0\r\n"
             "Host: %s:%d\r\n"
             "User-Agent: Mozilla/5.0 (SmartTV; Azamerica/GX6622)\r\n"
             "Accept-Encoding: identity\r\n"
             "Connection: close\r\n\r\n", path, host, port);
    if (req_len < 0 || (size_t)req_len >= sizeof(req)) goto done;

    while (sent < (size_t)req_len) {
        ssize_t n;
#ifdef HLS_WITH_MBEDTLS
        if (tls) n = hls_tls_write(tls, req + sent, (size_t)req_len - sent);
        else
#endif
            n = send(fd, req + sent, (size_t)req_len - sent, 0);
        if (n <= 0) goto done;
        sent += (size_t)n;
    }

    while (mode || s_bridge_running) {
        uint8_t chunk[2048];
        ssize_t r;
#ifdef HLS_WITH_MBEDTLS
        if (tls) r = hls_tls_read(tls, chunk, sizeof(chunk));
        else
#endif
            r = recv(fd, chunk, sizeof(chunk), 0);
        if (r < 0 && errno == EINTR) continue;
        if (r < 0) break;
        if (r == 0) { complete = 1; break; }

        if (!in_body) {
            char *split, *line;
            size_t header_len, body_bytes;
            if ((size_t)r > sizeof(header_buf) - 1 - header_pos) break;
            memcpy(header_buf + header_pos, chunk, (size_t)r);
            header_pos += (size_t)r;
            header_buf[header_pos] = '\0';
            split = strstr(header_buf, "\r\n\r\n");
            if (!split) continue;
            header_len = (size_t)(split + 4 - header_buf);
            if (sscanf(header_buf, "HTTP/%*d.%*d %d", &status) != 1) break;
            if (mode && status >= 300 && status < 400 && redirects < 4) {
                char *location = strcasestr(header_buf, "\r\nLocation:");
                if (location && location < split) {
                    char *end;
                    location += 11;
                    while (*location == ' ' || *location == '\t') location++;
                    end = strstr(location, "\r\n");
                    if (end && end <= split) {
                        *end = '\0';
                        if (hls_resolve_url(url, location, redirect_url, sizeof(redirect_url)) < 0)
                            redirect_url[0] = '\0';
                    }
                }
                goto done;
            }
            if (status != 200) break;
            *split = '\0';
            line = strstr(header_buf, "\r\n");
            if (!line) break;
            line += 2;
            while (*line) {
                char *next = strstr(line, "\r\n");
                if (next) *next = '\0';
                if (strncasecmp(line, "Content-Length:", 15) == 0) {
                    char *endptr;
                    unsigned long long parsed;
                    const char *value = line + 15;
                    while (*value == ' ' || *value == '\t') value++;
                    errno = 0;
                    parsed = strtoull(value, &endptr, 10);
                    while (*endptr == ' ' || *endptr == '\t') endptr++;
                    if (errno || endptr == value || *endptr ||
                        (mode != 1 && parsed > max_size) ||
                        (unsigned long long)(size_t)parsed != parsed || has_length) goto done;
                    content_length = (size_t)parsed;
                    has_length = 1;
                }
                if (strncasecmp(line, "Transfer-Encoding:", 18) == 0) {
                    const char *value = line + 18;
                    while (*value == ' ' || *value == '\t') value++;
                    if (strcasecmp(value, "identity") != 0) goto done;
                }
                if (!next) break;
                line = next + 2;
            }
            in_body = 1;
            body_bytes = header_pos - header_len;
            if (mode == 1 && body_bytes > max_size) body_bytes = max_size;
            if (body_bytes > max_size) break;
            if (body_bytes) {
                memcpy(out_buf, header_buf + header_len, body_bytes);
                total = body_bytes;
            }
        } else {
            if (mode == 1 && (size_t)r > max_size - total) r = (ssize_t)(max_size - total);
            if ((size_t)r > max_size - total) break;
            memcpy(out_buf + total, chunk, (size_t)r);
            total += (size_t)r;
        }
        if (mode == 1 && total >= 564) { complete = 1; break; }
        if (has_length && total > content_length) break;
        if (has_length && total == content_length) {
            complete = 1;
            break;
        }
    }
done:
#ifdef HLS_WITH_MBEDTLS
    hls_tls_close(tls);
#endif
    close(fd);
    if (redirect_url[0])
        return http_fetch_mode(redirect_url, out_buf, max_size, out_size, mode, redirects + 1, effective_url);
    if ((!mode && !s_bridge_running) || !complete || !in_body || !total ||
        (mode != 1 && has_length && total != content_length)) return -1;
    *out_size = total;
    if (effective_url) snprintf(effective_url, 1024, "%s", url);
    return 0;
}

static int http_fetch(const char *url, uint8_t *buf, size_t cap, size_t *size) {
    return http_fetch_mode(url, buf, cap, size, 0, 0, NULL);
}

/* Independent probe. It never changes the active FIFO, producer or renderer. */
int hls_source_probe(const char *url) {
    uint8_t prefix[4096];
    size_t size = 0;
    char media_url[1024];
    if (!url || strlen(url) >= sizeof(media_url)) return -1;
    strcpy(media_url, url);
    if (strstr(url, ".m3u")) {
        uint8_t *playlist = malloc(128 * 1024);
        hls_segment_t segments[32];
        uint64_t sequence = 0;
        size_t count = 0;
        int rc;
        if (!playlist) return -1;
        rc = http_fetch_mode(url, playlist, 128 * 1024 - 1, &size, 2, 0, media_url);
        playlist[size] = 0;
        if (rc == 0) rc = hls_parse_media_playlist((char *)playlist, media_url,
                                      &sequence, segments, 32, &count);
        free(playlist);
        if (rc < 0 || !count) return -1;
        strcpy(media_url, segments[count - 1].url);
    }
    if (http_fetch_mode(media_url, prefix, sizeof(prefix), &size, 1, 0, NULL) < 0) return -1;
    for (size_t i = 0; i + 376 < size && i < 188; i++)
        if (prefix[i] == 0x47 && prefix[i + 188] == 0x47 && prefix[i + 376] == 0x47)
            return 0;
    if (size >= 12 && memcmp(prefix + 4, "ftyp", 4) == 0) return 0;
    return -1;
}

/* Produtor HLS: baixa a playlist periodicamente e alimenta os segmentos TS no ring buffer */
static void *hls_producer_thread(void *arg) {
    (void)arg;
    uint8_t *playlist_buf = (uint8_t *)malloc(128 * 1024);
    uint8_t *segment_buf = (uint8_t *)malloc(MAX_SEGMENT_SIZE);
    uint64_t next_sequence = 0;
    int started = 0;
    unsigned int playlist_failures = 0;
    if (!playlist_buf || !segment_buf) {
        fprintf(stderr, "[HLS Bridge] Memoria insuficiente para segmentos.\n");
        free(playlist_buf);
        free(segment_buf);
        return NULL;
    }

    fprintf(stderr, "[HLS Bridge] Produtor iniciado.\n");

    while (s_bridge_running) {
        size_t p_size = 0;
        if (http_fetch(s_current_m3u8, playlist_buf, 128 * 1024 - 1, &p_size) == 0 && p_size > 0) {
            hls_segment_t segments[16];
            size_t count = 0, start_idx, i;
            uint64_t parsed_next = next_sequence;
            playlist_failures = 0;
            playlist_buf[p_size] = '\0';
            if (hls_parse_media_playlist((const char *)playlist_buf, s_current_m3u8,
                                         &parsed_next, segments, 16, &count) < 0) {
                fprintf(stderr, "[HLS Bridge] Playlist invalida ou formato HLS nao suportado.\n");
            } else {
                if (!started)
                    fprintf(stderr, "[HLS Bridge] Playlist recebida: %lu bytes, %lu segmentos novos.\n",
                            (unsigned long)p_size, (unsigned long)count);
                start_idx = !started && count > 2 ? count - 2 : 0;
                for (i = start_idx; i < count && s_bridge_running; i++) {
                    size_t seg_size = 0, offset = 0;
                    if (http_fetch(segments[i].url, segment_buf, MAX_SEGMENT_SIZE, &seg_size) < 0) {
                        fprintf(stderr, "[HLS Bridge] Falha no segmento de sequencia %llu.\n",
                                (unsigned long long)segments[i].sequence);
                        break;
                    }
                    if (!started)
                        fprintf(stderr, "[HLS Bridge] Primeiro segmento: %lu bytes.\n",
                                (unsigned long)seg_size);
                    while (offset < seg_size && s_bridge_running) {
                        size_t chunk = seg_size - offset;
                        if (chunk > 64 * 1024) chunk = 64 * 1024;
                        if (ring_write(&s_ring, segment_buf + offset, chunk) < 0) break;
                        offset += chunk;
                        s_data_ready = 1;
                    }
                    if (offset != seg_size) break;
                    next_sequence = segments[i].sequence + 1;
                    started = 1;
                }
            }
        } else if (++playlist_failures == 1 || playlist_failures % 10 == 0) {
            fprintf(stderr, "[HLS Bridge] Falha ao baixar playlist (%u tentativas).\n",
                    playlist_failures);
        }

        for (int w = 0; w < 10 && s_bridge_running; w++) {
            usleep(100000);
        }
    }

    free(playlist_buf);
    free(segment_buf);
    fprintf(stderr, "[HLS Bridge] Produtor finalizado.\n");
    return NULL;
}

/* Alimentador do FIFO /tmp/live_stream.ts para o showcartoon */
static void *hls_feeder_thread(void *arg) {
    (void)arg;
    fprintf(stderr, "[HLS Bridge] FIFO /tmp/live_stream.ts pronto para streaming.\n");

    uint8_t out_chunk[16 * 1024];
    while (s_bridge_running) {
        int n = ring_read(&s_ring, out_chunk, sizeof(out_chunk));
        if (n > 0) {
            size_t offset = 0;
            while (offset < (size_t)n && s_bridge_running) {
                struct pollfd pfd;
                ssize_t written;
                pfd.fd = s_fifo_fd;
                pfd.events = POLLOUT;
                pfd.revents = 0;
                if (poll(&pfd, 1, 200) <= 0) continue;
                written = write(s_fifo_fd, out_chunk + offset, (size_t)n - offset);
                if (written > 0) offset += (size_t)written;
                else if (written < 0 && errno != EAGAIN && errno != EINTR) break;
            }
        }
    }

    if (s_fifo_fd >= 0) {
        close(s_fifo_fd);
        s_fifo_fd = -1;
    }
    unlink(FIFO_STREAM_PATH);
    return NULL;
}

int hls_bridge_start(const char *m3u8_url) {
    int rc;
    if (!m3u8_url || !*m3u8_url || strlen(m3u8_url) >= sizeof(s_current_m3u8)) return -1;
    if (strncmp(m3u8_url, "http://", 7) != 0 &&
        strncmp(m3u8_url, "https://", 8) != 0) {
        fprintf(stderr, "[HLS Bridge] Esquema HTTP(S) invalido.\n");
        return -1;
    }
#ifndef HLS_WITH_MBEDTLS
    if (strncmp(m3u8_url, "https://", 8) == 0) {
        fprintf(stderr, "[HLS Bridge] Binario sem suporte HTTPS.\n");
        return -1;
    }
#endif
    hls_bridge_stop();

    strcpy(s_current_m3u8, m3u8_url);
    unlink(FIFO_STREAM_PATH);
    if (mkfifo(FIFO_STREAM_PATH, 0666) < 0) {
        perror("mkfifo");
        return -1;
    }
    /* The feeder alone closes this descriptor after stop; O_NONBLOCK bounds stop. */
    s_fifo_fd = open(FIFO_STREAM_PATH, O_RDWR | O_NONBLOCK);
    if (s_fifo_fd < 0) {
        perror("open FIFO");
        unlink(FIFO_STREAM_PATH);
        return -1;
    }
    if (ring_init(&s_ring, RING_BUFFER_SIZE) < 0) {
        close(s_fifo_fd);
        s_fifo_fd = -1;
        unlink(FIFO_STREAM_PATH);
        return -1;
    }
    s_data_ready = 0;
    s_bridge_running = 1;
    rc = pthread_create(&s_feeder_thread, NULL, hls_feeder_thread, NULL);
    if (rc != 0) goto start_failed;
    rc = pthread_create(&s_producer_thread, NULL, hls_producer_thread, NULL);
    if (rc != 0) {
        s_bridge_running = 0;
        ring_reset(&s_ring);
        pthread_join(s_feeder_thread, NULL);
        ring_free(&s_ring);
        s_current_m3u8[0] = '\0';
        return -1;
    }
    return 0;
start_failed:
    s_bridge_running = 0;
    ring_free(&s_ring);
    close(s_fifo_fd);
    s_fifo_fd = -1;
    unlink(FIFO_STREAM_PATH);
    s_current_m3u8[0] = '\0';
    return -1;
}

void hls_bridge_stop(void) {
    if (!s_bridge_running) return;
    s_bridge_running = 0;

    ring_reset(&s_ring);

    pthread_join(s_producer_thread, NULL);
    pthread_join(s_feeder_thread, NULL);

    ring_free(&s_ring);
    s_current_m3u8[0] = '\0';
    s_data_ready = 0;
}

int hls_bridge_is_running(void) {
    return s_bridge_running;
}

int hls_bridge_has_data(void) {
    return s_data_ready;
}

const char *hls_bridge_get_url(void) {
    return FIFO_STREAM_PATH;
}
