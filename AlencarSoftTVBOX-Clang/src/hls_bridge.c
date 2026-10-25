/**
 * @file hls_bridge.c
 * @brief Implementacao nativa da ponte HLS (.m3u8 -> FIFO continuo /tmp/live_stream.ts para o chip).
 */

#include "hls_bridge.h"

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

#define FIFO_STREAM_PATH "/tmp/live_stream.ts"

#define RING_BUFFER_SIZE (4 * 1024 * 1024) /* 4 MB buffer */
#define MAX_SEGMENT_SIZE (1 * 1024 * 1024) /* 1 MB max por segmento */

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
static char s_current_m3u8[1024] = {0};
static pthread_t s_producer_thread;
static pthread_t s_feeder_thread;
static ring_buffer_t s_ring;
static int s_fifo_fd = -1;

/* Funcoes do Ring Buffer */
static void ring_init(ring_buffer_t *rb, size_t size) {
    rb->data = (uint8_t *)malloc(size);
    rb->size = size;
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
    pthread_mutex_init(&rb->lock, NULL);
    pthread_cond_init(&rb->can_read, NULL);
    pthread_cond_init(&rb->can_write, NULL);
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
static int parse_http_url(const char *url, char *host, int *port, char *path) {
    if (strncmp(url, "http://", 7) == 0) {
        url += 7;
    }
    *port = 80;

    const char *slash = strchr(url, '/');
    const char *colon = strchr(url, ':');

    if (colon && (!slash || colon < slash)) {
        size_t hlen = colon - url;
        strncpy(host, url, hlen);
        host[hlen] = '\0';
        *port = atoi(colon + 1);
    } else if (slash) {
        size_t hlen = slash - url;
        strncpy(host, url, hlen);
        host[hlen] = '\0';
    } else {
        strcpy(host, url);
    }

    if (slash) {
        strcpy(path, slash);
    } else {
        strcpy(path, "/");
    }
    return 0;
}

/* Cliente HTTP basico para baixar dados */
static int http_fetch(const char *url, uint8_t *out_buf, size_t max_size, size_t *out_size) {
    char host[256];
    int port = 80;
    char path[1024];

    parse_http_url(url, host, &port, path);

    struct hostent *he = gethostbyname(host);
    if (!he) return -1;

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct timeval tv;
    tv.tv_sec = 4;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    memcpy(&saddr.sin_addr, he->h_addr_list[0], he->h_length);

    if (connect(fd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
        close(fd);
        return -1;
    }

    char req[2048];
    snprintf(req, sizeof(req),
             "GET %s HTTP/1.0\r\n"
             "Host: %s\r\n"
             "User-Agent: Mozilla/5.0 (SmartTV; Azamerica/GX6622)\r\n"
             "Connection: close\r\n\r\n", path, host);

    if (send(fd, req, strlen(req), 0) < 0) {
        close(fd);
        return -1;
    }

    size_t total = 0;
    int in_body = 0;
    char header_buf[4096];
    size_t header_pos = 0;

    while (s_bridge_running && total < max_size) {
        uint8_t chunk[2048];
        int r = recv(fd, chunk, sizeof(chunk), 0);
        if (r <= 0) break;

        if (!in_body) {
            size_t copy_h = (header_pos + r < sizeof(header_buf) - 1) ? r : (sizeof(header_buf) - 1 - header_pos);
            memcpy(header_buf + header_pos, chunk, copy_h);
            header_pos += copy_h;
            header_buf[header_pos] = '\0';

            char *split = strstr(header_buf, "\r\n\r\n");
            if (split) {
                in_body = 1;
                size_t header_len = (split + 4) - header_buf;
                size_t body_start_in_chunk = 0;
                if (header_len <= header_pos) {
                    body_start_in_chunk = r - (header_pos - header_len);
                }
                if (body_start_in_chunk < (size_t)r) {
                    size_t body_bytes = r - body_start_in_chunk;
                    memcpy(out_buf + total, chunk + body_start_in_chunk, body_bytes);
                    total += body_bytes;
                }
            }
        } else {
            memcpy(out_buf + total, chunk, r);
            total += r;
        }
    }
    close(fd);
    *out_size = total;
    return (total > 0) ? 0 : -1;
}

/* Produtor HLS: baixa a playlist periodicamente e alimenta os segmentos TS no ring buffer */
static void *hls_producer_thread(void *arg) {
    (void)arg;
    uint8_t *playlist_buf = (uint8_t *)malloc(128 * 1024);
    uint8_t *segment_buf = (uint8_t *)malloc(MAX_SEGMENT_SIZE);
    char last_segment[512] = {0};

    char base_dir[1024];
    strcpy(base_dir, s_current_m3u8);
    char *last_slash = strrchr(base_dir, '/');
    if (last_slash) *(last_slash + 1) = '\0';

    fprintf(stderr, "[HLS Bridge] Produtor iniciado para %s\n", s_current_m3u8);

    while (s_bridge_running) {
        size_t p_size = 0;
        if (http_fetch(s_current_m3u8, playlist_buf, 128 * 1024 - 1, &p_size) == 0 && p_size > 0) {
            playlist_buf[p_size] = '\0';

            char *line = strtok((char *)playlist_buf, "\r\n");
            char current_segments[16][512];
            int seg_count = 0;

            while (line) {
                while (*line == ' ' || *line == '\t') line++;
                if (*line != '#' && strlen(line) > 0 && strstr(line, ".ts")) {
                    char full_seg_url[1024];
                    if (strncmp(line, "http://", 7) == 0) {
                        strncpy(full_seg_url, line, sizeof(full_seg_url) - 1);
                    } else if (line[0] == '/') {
                        char host[256];
                        int port = 80;
                        char path[1024];
                        parse_http_url(s_current_m3u8, host, &port, path);
                        snprintf(full_seg_url, sizeof(full_seg_url), "http://%s%s", host, line);
                    } else {
                        snprintf(full_seg_url, sizeof(full_seg_url), "%s%s", base_dir, line);
                    }

                    if (seg_count < 16) {
                        strncpy(current_segments[seg_count++], full_seg_url, sizeof(current_segments[0]) - 1);
                    }
                }
                line = strtok(NULL, "\r\n");
            }

            int start_idx = 0;
            if (last_segment[0] != '\0') {
                for (int i = 0; i < seg_count; i++) {
                    if (strcmp(current_segments[i], last_segment) == 0) {
                        start_idx = i + 1;
                        break;
                    }
                }
            } else {
                start_idx = (seg_count > 2) ? (seg_count - 2) : 0;
            }

            for (int i = start_idx; i < seg_count && s_bridge_running; i++) {
                size_t seg_size = 0;
                if (http_fetch(current_segments[i], segment_buf, MAX_SEGMENT_SIZE, &seg_size) == 0 && seg_size > 0) {
                    ring_write(&s_ring, segment_buf, seg_size);
                    strncpy(last_segment, current_segments[i], sizeof(last_segment) - 1);
                }
            }
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
    unlink(FIFO_STREAM_PATH);
    if (mkfifo(FIFO_STREAM_PATH, 0666) < 0 && errno != EEXIST) {
        perror("mkfifo");
        return NULL;
    }

    /* O_RDWR garante que open nao bloqueie esperando o leitor */
    s_fifo_fd = open(FIFO_STREAM_PATH, O_RDWR);
    if (s_fifo_fd < 0) {
        perror("open FIFO");
        return NULL;
    }

    fprintf(stderr, "[HLS Bridge] FIFO /tmp/live_stream.ts pronto para streaming.\n");

    uint8_t out_chunk[16 * 1024];
    while (s_bridge_running) {
        int n = ring_read(&s_ring, out_chunk, sizeof(out_chunk));
        if (n > 0) {
            ssize_t written = write(s_fifo_fd, out_chunk, n);
            if (written < 0) {
                if (errno == EPIPE) usleep(100000);
            }
        } else {
            usleep(50000);
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
    if (!m3u8_url) return -1;
    hls_bridge_stop();

    strncpy(s_current_m3u8, m3u8_url, sizeof(s_current_m3u8) - 1);
    s_bridge_running = 1;

    ring_init(&s_ring, RING_BUFFER_SIZE);

    pthread_create(&s_feeder_thread, NULL, hls_feeder_thread, NULL);
    pthread_create(&s_producer_thread, NULL, hls_producer_thread, NULL);

    /* Aguarda o pre-buffer de segmentos antes de retornar */
    usleep(800000); /* 800ms */
    return 0;
}

void hls_bridge_stop(void) {
    if (!s_bridge_running) return;
    s_bridge_running = 0;

    ring_reset(&s_ring);

    if (s_fifo_fd >= 0) {
        close(s_fifo_fd);
        s_fifo_fd = -1;
    }
    unlink(FIFO_STREAM_PATH);

    pthread_join(s_producer_thread, NULL);
    pthread_join(s_feeder_thread, NULL);

    ring_free(&s_ring);
    s_current_m3u8[0] = '\0';
}

int hls_bridge_is_running(void) {
    return s_bridge_running;
}

const char *hls_bridge_get_url(void) {
    return FIFO_STREAM_PATH;
}
