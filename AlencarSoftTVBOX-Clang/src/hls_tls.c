#include "hls_tls.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/platform.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>

struct hls_tls {
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config config;
    mbedtls_net_context net;
};

static pthread_once_t s_entropy_source_once = PTHREAD_ONCE_INIT;
static mbedtls_x509_crt s_global_ca;
static mbedtls_entropy_context s_global_entropy;
static mbedtls_ctr_drbg_context s_global_drbg;
static pthread_mutex_t s_global_tls_lock = PTHREAD_MUTEX_INITIALIZER;
static int s_global_initialized = 0;
static char s_last_override_ca[256] = {0};

/* GNU assembler includes the pinned Mozilla CA store in the executable.
 * The final byte is required by mbedtls_x509_crt_parse for PEM input. */
__asm__(".section .rodata\n"
        ".global hls_embedded_ca_start\n"
        "hls_embedded_ca_start:\n"
        ".incbin \"certs/cacert.pem\"\n"
        ".byte 0\n"
        ".global hls_embedded_ca_end\n"
        "hls_embedded_ca_end:\n"
        ".previous\n");
extern const unsigned char hls_embedded_ca_start[];
extern const unsigned char hls_embedded_ca_end[];

static void use_nonblocking_entropy_source(void) {
    /* GX6622 runs Linux 3.0, where /dev/random may wait indefinitely even
     * hours after boot. Mbed TLS supports selecting /dev/urandom at runtime. */
    mbedtls_platform_dev_random = "/dev/urandom";
}

static void tls_trace(const char *stage, int rc) {
    const char *enabled = getenv("HLS_TLS_TRACE");
    if (enabled && *enabled && strcmp(enabled, "0") != 0)
        fprintf(stderr, "[HLS TLS] %s rc=%d\n", stage, rc);
}

static int init_global_tls(const char *override_ca) {
    const char personalization[] = "AlencarSoftTVBOX HLS";
    int rc;

    pthread_mutex_lock(&s_global_tls_lock);
    if (s_global_initialized) {
        if (!override_ca && !s_last_override_ca[0]) {
            pthread_mutex_unlock(&s_global_tls_lock);
            return 0;
        }
        if (override_ca && strcmp(override_ca, s_last_override_ca) == 0) {
            pthread_mutex_unlock(&s_global_tls_lock);
            return 0;
        }
        mbedtls_x509_crt_free(&s_global_ca);
        s_global_initialized = 0;
    }

    mbedtls_x509_crt_init(&s_global_ca);
    if (override_ca && *override_ca) {
        rc = mbedtls_x509_crt_parse_file(&s_global_ca, override_ca);
        strncpy(s_last_override_ca, override_ca, sizeof(s_last_override_ca) - 1);
    } else {
        rc = mbedtls_x509_crt_parse(&s_global_ca, hls_embedded_ca_start,
                                    (size_t)(hls_embedded_ca_end - hls_embedded_ca_start));
        s_last_override_ca[0] = '\0';
    }
    if (rc != 0) {
        fprintf(stderr, "[HLS TLS] CA bundle invalido (%d).\n", rc);
        pthread_mutex_unlock(&s_global_tls_lock);
        return -1;
    }
    tls_trace("ca-parsed", rc);

    if (pthread_once(&s_entropy_source_once, use_nonblocking_entropy_source) != 0) {
        pthread_mutex_unlock(&s_global_tls_lock);
        return -1;
    }
    mbedtls_entropy_init(&s_global_entropy);
    mbedtls_ctr_drbg_init(&s_global_drbg);
    rc = mbedtls_ctr_drbg_seed(&s_global_drbg, mbedtls_entropy_func, &s_global_entropy,
                               (const unsigned char *)personalization,
                               strlen(personalization));
    tls_trace("entropy-seeded", rc);
    if (rc != 0) {
        pthread_mutex_unlock(&s_global_tls_lock);
        return -1;
    }

    s_global_initialized = 1;
    pthread_mutex_unlock(&s_global_tls_lock);
    return 0;
}

void hls_tls_close(hls_tls_t *tls) {
    if (!tls) return;
    mbedtls_ssl_free(&tls->ssl);
    mbedtls_ssl_config_free(&tls->config);
    /* net.fd belongs to the caller; mbedtls_net_free would close it. */
    free(tls);
}

hls_tls_t *hls_tls_open(int fd, const char *hostname) {
    const char *override_ca = getenv("HLS_CA_BUNDLE");
    hls_tls_t *tls = (hls_tls_t *)calloc(1, sizeof(*tls));
    int rc = -1;
    if (!tls || fd < 0 || !hostname || !*hostname) {
        free(tls);
        return NULL;
    }

    if (init_global_tls(override_ca) != 0) {
        free(tls);
        return NULL;
    }

    mbedtls_ssl_init(&tls->ssl);
    mbedtls_ssl_config_init(&tls->config);
    mbedtls_net_init(&tls->net);
    tls->net.fd = fd;
    tls_trace("open", 0);

    rc = mbedtls_ssl_config_defaults(&tls->config, MBEDTLS_SSL_IS_CLIENT,
                                     MBEDTLS_SSL_TRANSPORT_STREAM,
                                     MBEDTLS_SSL_PRESET_DEFAULT);
    if (rc != 0) goto failed;
    mbedtls_ssl_conf_authmode(&tls->config, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&tls->config, &s_global_ca, NULL);
    mbedtls_ssl_conf_rng(&tls->config, mbedtls_ctr_drbg_random, &s_global_drbg);
    rc = mbedtls_ssl_setup(&tls->ssl, &tls->config);
    if (rc != 0) goto failed;
    rc = mbedtls_ssl_set_hostname(&tls->ssl, hostname);
    if (rc != 0) goto failed;
    mbedtls_ssl_set_bio(&tls->ssl, &tls->net, mbedtls_net_send, mbedtls_net_recv, NULL);
    tls_trace("handshake-start", 0);
    do {
        rc = mbedtls_ssl_handshake(&tls->ssl);
    } while (rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE);
    tls_trace("handshake-end", rc);
    if (rc != 0 || mbedtls_ssl_get_verify_result(&tls->ssl) != 0) goto failed;
    return tls;
failed:
    fprintf(stderr, "[HLS TLS] Handshake ou validacao de certificado falhou (%d).\n", rc);
    hls_tls_close(tls);
    return NULL;
}

ssize_t hls_tls_read(hls_tls_t *tls, void *buffer, size_t length) {
    int rc = mbedtls_ssl_read(&tls->ssl, (unsigned char *)buffer, length);
    return (ssize_t)rc;
}

ssize_t hls_tls_write(hls_tls_t *tls, const void *buffer, size_t length) {
    int rc = mbedtls_ssl_write(&tls->ssl, (const unsigned char *)buffer, length);
    return (ssize_t)rc;
}
