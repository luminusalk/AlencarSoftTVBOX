#include "firmware_dlna.h"
#include "hls_bridge.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#define AV_SERVICE "urn:schemas-upnp-org:service:AVTransport:1"

static volatile sig_atomic_t s_running;
static int s_listener = -1;
static pthread_t s_server;
static int s_server_started;
static int s_bridge_started;
static int s_transport_owned;
static char s_file_path[1024];
static char s_renderer_ip[64];
static int s_renderer_port;
static char s_control_path[256];

static int send_all(int fd, const char *data, size_t length) {
    size_t sent = 0;
    while (sent < length && s_running) {
        struct pollfd pfd;
        ssize_t n;
        pfd.fd = fd;
        pfd.events = POLLOUT;
        pfd.revents = 0;
        if (poll(&pfd, 1, 400) <= 0) continue;
        n = send(fd, data + sent, length - sent, MSG_NOSIGNAL);
        if (n < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) continue;
        if (n <= 0) return -1;
        sent += (size_t)n;
    }
    return sent == length ? 0 : -1;
}

static void serve_client(int client) {
    char request[2048], bytes[16384];
    char header[256];
    int source = -1;
    ssize_t n;
    size_t used = 0;
    int is_head = 0;
    struct pollfd pfd;
    while (used < sizeof(request) - 1 && s_running) {
        pfd.fd = client;
        pfd.events = POLLIN;
        pfd.revents = 0;
        if (poll(&pfd, 1, 1000) <= 0) return;
        n = recv(client, request + used, sizeof(request) - used - 1, 0);
        if (n <= 0) return;
        used += (size_t)n;
        request[used] = '\0';
        if (strstr(request, "\r\n\r\n")) break;
    }
    if (strncmp(request, "GET /live.ts ", 13) != 0 &&
        strncmp(request, "HEAD /live.ts ", 14) != 0) {
        static const char bad[] = "HTTP/1.0 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        send_all(client, bad, sizeof(bad) - 1);
        return;
    }
    is_head = request[0] == 'H';
    snprintf(header, sizeof(header),
             "HTTP/1.0 200 OK\r\nContent-Type: %s\r\n"
             "Cache-Control: no-store\r\nConnection: close\r\n\r\n",
             s_file_path[0] && strstr(s_file_path, ".mp4") ?
             "video/mp4" : "video/mp2t");
    if (send_all(client, header, strlen(header)) < 0 || is_head) return;
    source = open(s_file_path[0] ? s_file_path : hls_bridge_get_url(), O_RDONLY | O_NONBLOCK);
    if (source < 0) return;
    while (s_running) {
        pfd.fd = source;
        pfd.events = POLLIN;
        pfd.revents = 0;
        if (poll(&pfd, 1, 200) <= 0) continue;
        n = read(source, bytes, sizeof(bytes));
        if (n < 0 && (errno == EINTR || errno == EAGAIN)) continue;
        if (n < 0 || (n == 0 && s_file_path[0])) break;
        if (n > 0 && send_all(client, bytes, (size_t)n) < 0) break;
    }
    close(source);
}

static void *server_thread(void *unused) {
    (void)unused;
    while (s_running) {
        struct pollfd pfd;
        int client;
        pfd.fd = s_listener;
        pfd.events = POLLIN;
        pfd.revents = 0;
        if (poll(&pfd, 1, 200) <= 0) continue;
        client = accept(s_listener, NULL, NULL);
        if (client < 0) continue;
        {
            int flags = fcntl(client, F_GETFL);
            if (flags < 0 || fcntl(client, F_SETFL, flags | O_NONBLOCK) < 0) {
                close(client);
                continue;
            }
        }
        serve_client(client);
        close(client);
    }
    return NULL;
}

static int local_ip(char *out, size_t cap) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in peer, local;
    socklen_t length = sizeof(local);
    if (fd < 0) return -1;
    memset(&peer, 0, sizeof(peer));
    peer.sin_family = AF_INET;
    peer.sin_port = htons(53);
    inet_pton(AF_INET, "1.1.1.1", &peer.sin_addr);
    if (connect(fd, (struct sockaddr *)&peer, sizeof(peer)) < 0 ||
        getsockname(fd, (struct sockaddr *)&local, &length) < 0 ||
        !inet_ntop(AF_INET, &local.sin_addr, out, cap)) {
        close(fd);
        if (cap > strlen("127.0.0.1")) {
            strcpy(out, "127.0.0.1");
            return 0;
        }
        return -1;
    }
    close(fd);
    return 0;
}

static int parse_location(const char *response, const char *own_ip,
                          char *host, size_t host_cap, int *port,
                          char *path, size_t path_cap) {
    const char *loc = strcasestr(response, "\nLOCATION:");
    const char *start, *end, *slash, *colon;
    size_t length;
    if (!loc) return -1;
    start = loc + 10;
    while (*start == ' ') start++;
    if (strncmp(start, "http://", 7) != 0) return -1;
    start += 7;
    end = strchr(start, '\r');
    if (!end) end = strchr(start, '\n');
    if (!end) return -1;
    slash = memchr(start, '/', (size_t)(end - start));
    if (!slash) return -1;
    colon = memchr(start, ':', (size_t)(slash - start));
    length = (size_t)((colon ? colon : slash) - start);
    if (!length || length >= host_cap || strcmp(own_ip, "127.0.0.1") == 0) return -1;
    memcpy(host, start, length);
    host[length] = '\0';
    if (strcmp(host, own_ip) != 0) return -1;
    *port = colon ? atoi(colon + 1) : 80;
    if (*port < 1 || *port > 65535 || (size_t)(end - slash) >= path_cap) return -1;
    memcpy(path, slash, (size_t)(end - slash));
    path[end - slash] = '\0';
    return 0;
}

static int http_request(const char *host, int port, const char *path,
                        const char *method, const char *soap_action,
                        const char *body, char *reply, size_t reply_cap,
                        int timeout_seconds) {
    int fd = -1, request_length, result = -1;
    char request[22000];
    size_t size = 0;
    struct sockaddr_in peer;
    struct timeval timeout;
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    timeout.tv_sec = timeout_seconds;
    timeout.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    memset(&peer, 0, sizeof(peer));
    peer.sin_family = AF_INET;
    peer.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &peer.sin_addr) != 1 ||
        connect(fd, (struct sockaddr *)&peer, sizeof(peer)) < 0) goto done;
    if (soap_action && *soap_action) {
        request_length = snprintf(request, sizeof(request),
            "%s %s HTTP/1.0\r\nHost: %s:%d\r\nContent-Type: text/xml; charset=\"utf-8\"\r\n"
            "SOAPAction: \"%s\"\r\nContent-Length: %lu\r\nConnection: close\r\n\r\n%s",
            method, path, host, port, soap_action,
            (unsigned long)(body ? strlen(body) : 0), body ? body : "");
    } else {
        request_length = snprintf(request, sizeof(request),
            "%s %s HTTP/1.0\r\nHost: %s:%d\r\nConnection: close\r\n\r\n",
            method, path, host, port);
    }
    if (request_length < 0 || (size_t)request_length >= sizeof(request)) goto done;
    while (size < (size_t)request_length) {
        ssize_t n = send(fd, request + size, (size_t)request_length - size, MSG_NOSIGNAL);
        if (n <= 0) goto done;
        size += (size_t)n;
    }
    size = 0;
    while (size < reply_cap - 1) {
        ssize_t n = recv(fd, reply + size, reply_cap - size - 1, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) goto done;
        if (n == 0) break;
        size += (size_t)n;
    }
    reply[size] = '\0';
    if (!strstr(reply, "\r\n\r\n") ||
        (strncmp(reply, "HTTP/1.0 200", 12) && strncmp(reply, "HTTP/1.1 200", 12))) goto done;
    result = 0;
done:
    close(fd);
    return result;
}

static int try_renderer(const char *host, int port, const char *path) {
    char description[16384];
    const char *service, *service_end, *control, *end;
    size_t length;
    if (http_request(host, port, path, "GET", NULL, NULL,
                     description, sizeof(description), 2) < 0) return -1;
    service = strstr(description, "<serviceType>" AV_SERVICE "</serviceType>");
    if (!service) return -1;
    service_end = strstr(service, "</service>");
    control = strstr(service, "<controlURL>");
    end = control ? strstr(control, "</controlURL>") : NULL;
    if (!service_end || !control || !end || end > service_end) return -1;
    control += strlen("<controlURL>");
    length = (size_t)(end - control);
    if (!length || length + (*control == '/' ? 0 : 1) >= sizeof(s_control_path)) return -1;
    if (*control == '/') {
        memcpy(s_control_path, control, length);
        s_control_path[length] = '\0';
    } else {
        s_control_path[0] = '/';
        memcpy(s_control_path + 1, control, length);
        s_control_path[length + 1] = '\0';
    }
    if (strlen(host) >= sizeof(s_renderer_ip)) return -1;
    strcpy(s_renderer_ip, host);
    s_renderer_port = port;
    fprintf(stderr, "[DLNA] Renderizador local em %s:%d%s\n",
            s_renderer_ip, s_renderer_port, s_control_path);
    return 0;
}

static int scan_local_listeners(const char *own_ip) {
    char line[256], address[9];
    unsigned int port, state;
    FILE *tcp = fopen("/proc/net/tcp", "r");
    if (!tcp) return -1;
    while (fgets(line, sizeof(line), tcp)) {
        if (sscanf(line, " %*d: %8[0-9A-Fa-f]:%x %*s %x",
                   address, &port, &state) != 3 || state != 0x0a ||
            port < 1024 || port > 65535) continue;

        /* Se /proc/net/tcp contem o IP da interface (ex: 1712A8C0 -> 192.168.18.23),
         * converte dinamicamente para texto. Isso garante funcionamento se o IP mudar! */
        unsigned int ip_hex = 0;
        if (sscanf(address, "%x", &ip_hex) == 1 && ip_hex != 0) {
            struct in_addr in;
            char ip_buf[64];
            in.s_addr = (in_addr_t)ip_hex;
            if (inet_ntop(AF_INET, &in, ip_buf, sizeof(ip_buf))) {
                if (try_renderer(ip_buf, (int)port, "/description.xml") == 0) {
                    fclose(tcp);
                    return 0;
                }
            }
        }

        if (own_ip && *own_ip && try_renderer(own_ip, (int)port, "/description.xml") == 0) {
            fclose(tcp);
            return 0;
        }
        if (try_renderer("127.0.0.1", (int)port, "/description.xml") == 0) {
            fclose(tcp);
            return 0;
        }
    }
    fclose(tcp);
    return -1;
}

static int discover_renderer(void) {
    static const char search[] =
        "M-SEARCH * HTTP/1.1\r\nHOST:239.255.255.250:1900\r\n"
        "MAN:\"ssdp:discover\"\r\nMX:1\r\nST:ssdp:all\r\n\r\n";
    char own_ip[64], response[8192], path[256], host[64];
    struct sockaddr_in target;
    struct pollfd pfd;
    int fd, port, attempts;
    if (local_ip(own_ip, sizeof(own_ip)) < 0) return -1;
    if (scan_local_listeners(own_ip) == 0) return 0;
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return -1;
    memset(&target, 0, sizeof(target));
    target.sin_family = AF_INET;
    target.sin_port = htons(1900);
    inet_pton(AF_INET, own_ip, &target.sin_addr);
    if (sendto(fd, search, sizeof(search) - 1, 0,
               (struct sockaddr *)&target, sizeof(target)) < 0) goto failed;
    pfd.fd = fd;
    pfd.events = POLLIN;
    for (attempts = 0; attempts < 5; attempts++) {
        ssize_t n;
        pfd.revents = 0;
        if (poll(&pfd, 1, 1000) <= 0) continue;
        n = recv(fd, response, sizeof(response) - 1, 0);
        if (n <= 0) continue;
        response[n] = '\0';
        if (parse_location(response, own_ip, host, sizeof(host), &port,
                           path, sizeof(path)) < 0) continue;
        if (try_renderer(host, port, path) == 0) {
            close(fd);
            return 0;
        }
    }
failed:
    close(fd);
    return -1;
}

static int xml_escape(const char *src, char *dst, size_t cap) {
    size_t used = 0;
    while (*src) {
        const char *piece = *src == '&' ? "&amp;" :
            *src == '<' ? "&lt;" : *src == '>' ? "&gt;" :
            *src == '"' ? "&quot;" : NULL;
        size_t n = piece ? strlen(piece) : 1;
        if (used + n >= cap) return -1;
        if (piece) memcpy(dst + used, piece, n);
        else dst[used] = *src;
        used += n;
        src++;
    }
    dst[used] = '\0';
    return 0;
}

static int soap(const char *action, const char *args) {
    char body[18000], reply[8192], header[160];
    int n = snprintf(body, sizeof(body),
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
        "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
        "<s:Body><u:%s xmlns:u=\"" AV_SERVICE "\">%s</u:%s></s:Body></s:Envelope>",
        action, args, action);
    if (n < 0 || (size_t)n >= sizeof(body)) return -1;
    snprintf(header, sizeof(header), AV_SERVICE "#%s", action);
    return http_request(s_renderer_ip, s_renderer_port, s_control_path,
                        "POST", header, body, reply, sizeof(reply), 8);
}

int firmware_dlna_start(const char *source, const char *title) {
    struct sockaddr_in address;
    socklen_t address_size = sizeof(address);
    char uri[1024], uri_escaped[4096], title_escaped[512], metadata[6144],
         metadata_escaped[10000], args[14000];
    int n, direct_http;
    const char *mime;
    if (!source || !*source) return -1;
    firmware_dlna_stop();
    if (discover_renderer() < 0) {
        fprintf(stderr, "[DLNA] Renderizador local nao encontrado.\n");
        return -1;
    }
    direct_http = strncmp(source, "http://", 7) == 0 &&
                  !strstr(source, ".m3u8") && !strstr(source, ".m3u");
    mime = (direct_http || source[0] == '/') && strstr(source, ".mp4") ?
           "video/mp4" : "video/mpeg";
    s_file_path[0] = '\0';
    if (source[0] == '/') {
        if (strlen(source) >= sizeof(s_file_path)) return -1;
        strcpy(s_file_path, source);
    } else if (strstr(source, ".m3u8") || strstr(source, ".m3u")) {
        if (hls_bridge_start(source) < 0) return -1;
        s_bridge_started = 1;
    } else if (!direct_http) return -1;
    s_listener = socket(AF_INET, SOCK_STREAM, 0);
    if (s_listener < 0) goto failed;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s_listener, (struct sockaddr *)&address, sizeof(address)) < 0 ||
        listen(s_listener, 2) < 0 ||
        getsockname(s_listener, (struct sockaddr *)&address, &address_size) < 0)
        goto failed;
    s_running = 1;
    if (pthread_create(&s_server, NULL, server_thread, NULL) != 0) goto failed;
    s_server_started = 1;
    if (direct_http) n = snprintf(uri, sizeof(uri), "%s", source);
    else n = snprintf(uri, sizeof(uri), "http://127.0.0.1:%u/live.ts",
                      ntohs(address.sin_port));
    if (n < 0 || (size_t)n >= sizeof(uri)) goto failed;
    if (xml_escape(uri, uri_escaped, sizeof(uri_escaped)) < 0 ||
        xml_escape(title ? title : "Canal IPTV", title_escaped, sizeof(title_escaped)) < 0)
        goto failed;
    n = snprintf(metadata, sizeof(metadata),
        "<DIDL-Lite xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\" "
        "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" "
        "xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\">"
        "<item id=\"1\" parentID=\"0\" restricted=\"1\">"
        "<dc:title>%s</dc:title><upnp:class>object.item.videoItem</upnp:class>"
        "<res protocolInfo=\"http-get:*:%s:*\">%s</res>"
        "</item></DIDL-Lite>", title_escaped, mime, uri_escaped);
    if (n < 0 || (size_t)n >= sizeof(metadata) ||
        xml_escape(metadata, metadata_escaped, sizeof(metadata_escaped)) < 0)
        goto failed;
    n = snprintf(args, sizeof(args),
        "<InstanceID>0</InstanceID><CurrentURI>%s</CurrentURI>"
        "<CurrentURIMetaData>%s</CurrentURIMetaData>", uri_escaped, metadata_escaped);
    if (n < 0 || (size_t)n >= sizeof(args) || soap("SetAVTransportURI", args) < 0)
        goto failed;
    s_transport_owned = 1;
    if (soap("Play", "<InstanceID>0</InstanceID><Speed>1</Speed>") < 0)
        goto failed;
    fprintf(stderr, "[DLNA] Player do firmware recebeu Play.\n");
    return 0;
failed:
    firmware_dlna_stop();
    return -1;
}

void firmware_dlna_stop(void) {
    if (s_renderer_ip[0] && s_transport_owned)
        soap("Stop", "<InstanceID>0</InstanceID>");
    s_transport_owned = 0;
    s_running = 0;
    if (s_server_started) {
        pthread_join(s_server, NULL);
        s_server_started = 0;
    }
    if (s_listener >= 0) { close(s_listener); s_listener = -1; }
    if (s_bridge_started) { hls_bridge_stop(); s_bridge_started = 0; }
    s_file_path[0] = '\0';
}

int firmware_dlna_active(void) { return s_running != 0; }
