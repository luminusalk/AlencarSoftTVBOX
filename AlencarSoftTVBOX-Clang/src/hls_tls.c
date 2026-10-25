#include "hls_tls.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>

struct hls_tls {
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config config;
    mbedtls_x509_crt ca;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context drbg;
    mbedtls_net_context net;
};

static void tls_trace(const char *stage, int rc) {
    const char *enabled = getenv("HLS_TLS_TRACE");
    if (enabled && *enabled && strcmp(enabled, "0") != 0)
        fprintf(stderr, "[HLS TLS] %s rc=%d\n", stage, rc);
}

void hls_tls_close(hls_tls_t *tls) {
    if (!tls) return;
    mbedtls_ssl_free(&tls->ssl);
    mbedtls_ssl_config_free(&tls->config);
    mbedtls_x509_crt_free(&tls->ca);
    mbedtls_ctr_drbg_free(&tls->drbg);
    mbedtls_entropy_free(&tls->entropy);
    /* net.fd belongs to the caller; mbedtls_net_free would close it. */
    free(tls);
}

hls_tls_t *hls_tls_open(int fd, const char *hostname) {
    static const char *const ca_paths[] = {
        "/mnt/plugin/iptv/cacert.pem",
        "/etc/ssl/certs/ca-certificates.crt",
        "/etc/ssl/cert.pem",
        "/mnt/plugin/cacert.pem"
    };
    const char personalization[] = "AlencarSoftTVBOX HLS";
    const char *override_ca = getenv("HLS_CA_BUNDLE");
    hls_tls_t *tls = (hls_tls_t *)calloc(1, sizeof(*tls));
    int rc = -1;
    size_t i;
    if (!tls || fd < 0 || !hostname || !*hostname) {
        free(tls);
        return NULL;
    }
    mbedtls_ssl_init(&tls->ssl);
    mbedtls_ssl_config_init(&tls->config);
    mbedtls_x509_crt_init(&tls->ca);
    mbedtls_entropy_init(&tls->entropy);
    mbedtls_ctr_drbg_init(&tls->drbg);
    mbedtls_net_init(&tls->net);
    tls->net.fd = fd;
    tls_trace("open", 0);

    if (override_ca && *override_ca) {
        rc = mbedtls_x509_crt_parse_file(&tls->ca, override_ca);
        i = rc == 0 ? 0 : sizeof(ca_paths) / sizeof(ca_paths[0]);
    } else {
        for (i = 0; i < sizeof(ca_paths) / sizeof(ca_paths[0]); i++) {
            rc = mbedtls_x509_crt_parse_file(&tls->ca, ca_paths[i]);
            if (rc == 0) break;
            mbedtls_x509_crt_free(&tls->ca);
            mbedtls_x509_crt_init(&tls->ca);
        }
    }
    if (i == sizeof(ca_paths) / sizeof(ca_paths[0])) {
        fprintf(stderr, "[HLS TLS] CA bundle ausente ou invalido.\n");
        goto failed;
    }
    tls_trace("ca-parsed", rc);
    rc = mbedtls_ctr_drbg_seed(&tls->drbg, mbedtls_entropy_func, &tls->entropy,
                               (const unsigned char *)personalization,
                               strlen(personalization));
    tls_trace("entropy-seeded", rc);
    if (rc != 0) goto failed;
    rc = mbedtls_ssl_config_defaults(&tls->config, MBEDTLS_SSL_IS_CLIENT,
                                     MBEDTLS_SSL_TRANSPORT_STREAM,
                                     MBEDTLS_SSL_PRESET_DEFAULT);
    if (rc != 0) goto failed;
    mbedtls_ssl_conf_authmode(&tls->config, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&tls->config, &tls->ca, NULL);
    mbedtls_ssl_conf_rng(&tls->config, mbedtls_ctr_drbg_random, &tls->drbg);
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
