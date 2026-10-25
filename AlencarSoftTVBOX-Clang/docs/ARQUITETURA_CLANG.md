# Aplicativo C: módulos e estado de validação

O S1009+ usa o backend `RECEIVER_DLNA_BACKEND`, selecionado no build C-SKY.
Ele preserva `/app/dvbapp`, pois o renderizador DLNA do firmware produziu
imagem e som no teste com ponte C. MIPS ainda compila o caminho legado
`showcartoon` sem teste audiovisual equivalente.

| Arquivo | Função atual |
| --- | --- |
| `src/main.c` | Grade, seleção, OSD, `--play-url`, saída sem reiniciar `dvbapp` no C-SKY. |
| `src/input.c` | IR `/dev/gxirr0` e teclado não bloqueante; concorrência com firmware pendente. |
| `src/player.c` | Escolha de backend e estado; `Play` aceito não comprova imagem/som. |
| `src/firmware_dlna.c` | Descoberta local por `/proc/net/tcp`/SSDP, servidor HTTP loopback, SOAP AVTransport. |
| `src/hls_bridge.c` | Download HLS em C, ring buffer e FIFO MPEG-TS. |
| `src/hls_tls.c` | Mbed TLS com CA embutida, verificação de hostname e `/dev/urandom`. |
| `src/framebuffer.c` | Grade e OSD; alfa zero na reprodução. |
| `src/iptv_catalog.c` | Leitura de listas M3U. |

O CI `40200018458` compilou C-SKY/MIPS e passou nos testes Linux, incluindo
mock de renderer DLNA. O aplicativo integrado **não foi confirmado na TV**:
a primeira execução falhou na descoberta; a correção compilou, mas o usuário
suspendeu novos testes na box. A ponte C com CGI diagnóstico e DLNA do
firmware já deu imagem e som por 25 segundos.

O build oficial está em `.github/workflows/build_tvbox.yml`. No `Makefile`,
`TARGET=csky TLS=1` requer toolchain antigo e Mbed TLS estático. `certs/cacert.pem`
é embutido por `.incbin` durante a compilação: executar a receita a partir de
`AlencarSoftTVBOX-Clang/`. Não instalar a CA separada no receptor. Consulte o
[guia mestre](../../docs/GUIA_MESTRE_CONTINUIDADE_E_HANDOVER.md) para restrições
e próximos testes.
