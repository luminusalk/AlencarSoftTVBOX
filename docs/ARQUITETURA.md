# Arquitetura e Engenharia Reversa — Azamerica S1009+

## 1. Visão Geral do Hardware e Sistema

O **Azamerica S1009+** é baseado no SoC **NationalChip GX6622** com arquitetura de CPU **C-SKY Linux** de 32 bits.
- **Armazenamento Interno:** Flash NAND particionada em MTD (partição `/mnt/plugin` com sistema de arquivos YAFFS2 gravável com mais de 60 MB livres).
- **Processo Principal:** `/app/dvbapp` (executável ELF estaticamente compilado de ~17 MB).
- **Ambiente de Script:** O `dvbapp` possui um interpretador **Python 2.7 estaticamente embutido** em seu binário, expondo a API de complementos do XBMC (`xbmc`, `xbmcgui`, `xbmcplugin`, `xbmcvfs`, `xbmcaddon`).
- **Acesso Remoto:** Servidor Telnet nativo na porta 23 com login automático de `root`.

---

## 2. O Problema da GIL e a Descoberta da Solução

### Sintoma:
Ao tentar reproduzir transmissões HLS ao vivo através de uma thread Python comum (`threading.Thread`) iniciada pelo menu XBMC, a reprodução travava em *Start to open stream please wait...* ou ficava em tela preta.

### Causa Raiz Revelada:
Analisando as chamadas C/C++ internas do `dvbapp`:
1. O `dvbapp` executa o script do add-on (`default.py`) de forma estritamente síncrona.
2. O player de vídeo só é aberto pelo C++ do aparelho **após** o script Python retornar (`endOfDirectory`).
3. Assim que o script Python retorna, o `dvbapp` limpa o dicionário `sys.modules` e **mantém travada a GIL (Global Interpreter Lock)** do Python enquanto o usuário está navegando ou no player.
4. Qualquer thread criada em Python ficava 100% congelada sem receber tempo de CPU do kernel.

### A Solução via Daemon Desacoplado (`os.fork`):
Como a tabela de símbolos POSIX do `dvbapp` inclui chamadas completas de sistema Unix (`posix_fork`, `posix_setsid`, `posix__exit`), implementamos uma ponte em background:
1. O add-on verifica se a porta local `8978` já está respondendo ao endpoint `/health`.
2. Se não estiver, realiza um `os.fork()`:
   - **Processo Pai (`dvbapp`):** Continua normalmente e entrega ao player nativo o link `http://127.0.0.1:8978/live.ts?src=<url_encoded>`.
   - **Processo Filho (Daemon):** Executa `os.setsid()`, desacopla os descritores padrão e roda um servidor HTTP puro (`server.serve_forever()`) com seu próprio espaço de memória e sua própria GIL, completamente independente do ciclo de vida do XBMC.

---

## 3. Streaming HLS para MPEG-TS com Fila Assíncrona

O player nativo (`gxplayer`) do receptor tem máxima estabilidade e aceleração de hardware ao receber fluxos **MPEG-TS contínuos**.
O daemon `hls_bridge.py`:
- Conecta-se à URL remota (mesmo sob HTTPS/TLS) usando `urllib2`.
- Analisa a playlist `.m3u8` e baixa os segmentos `.ts`.
- Alinha o fluxo no byte de sincronismo `0x47` do protocolo MPEG-TS.
- Utiliza uma fila thread-safe (`Queue.Queue`) com pré-carregamento de blocos de vídeo, garantindo que o player nunca sofra com oscilações momentâneas de latência da internet.
