#ifndef ALENCARSOFT_HLS_TLS_H
#define ALENCARSOFT_HLS_TLS_H

#include <stddef.h>
#include <sys/types.h>

typedef struct hls_tls hls_tls_t;

/* The caller retains ownership of fd. Hostname and certificate validation
 * are mandatory; this function fails if the CA bundle is unavailable. */
hls_tls_t *hls_tls_open(int fd, const char *hostname);
ssize_t hls_tls_read(hls_tls_t *tls, void *buffer, size_t length);
ssize_t hls_tls_write(hls_tls_t *tls, const void *buffer, size_t length);
void hls_tls_close(hls_tls_t *tls);

#endif
