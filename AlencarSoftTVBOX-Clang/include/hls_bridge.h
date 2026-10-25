/**
 * @file hls_bridge.h
 * @brief Ponte nativa em C para converter listas HLS (.m3u8) em fluxo MPEG-TS continuo para o decodificador.
 */

#ifndef HLS_BRIDGE_H
#define HLS_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicia a ponte HLS local em segundo plano para o URL .m3u8 especificado.
 * @param m3u8_url URL HTTP do stream HLS (.m3u8).
 * @return 0 em caso de sucesso, negativo em caso de falha.
 */
int hls_bridge_start(const char *m3u8_url);

/**
 * @brief Encerra a ponte HLS e libera recursos de rede e threads.
 */
void hls_bridge_stop(void);

/**
 * @brief Verifica se a ponte HLS esta em execucao.
 * @return 1 se estiver ativa, 0 se parada.
 */
int hls_bridge_is_running(void);

/**
 * @brief Retorna o URL local servido pela ponte para o player de hardware.
 * @return String com URL local, ex: "http://127.0.0.1:8978/live.ts"
 */
const char *hls_bridge_get_url(void);

#ifdef __cplusplus
}
#endif

#endif /* HLS_BRIDGE_H */
