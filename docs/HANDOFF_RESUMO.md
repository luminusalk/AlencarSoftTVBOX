# Resumo Técnico e Handoff — Azamerica S1009+ (`plugin.video.az_iptv`)

## 1. Ambiente e Conexão
- **Receptor:** Azamerica S1009+ (firmware V1.76, C-SKY Linux, binário principal `/app/dvbapp` com Python 2.7 estaticamente compilado dentro dele).
- **Rede Direta (Cabo no Notebook):**
  - PC (Notebook): `192.168.137.1` (compartilhando internet via ICS e servindo arquivos de teste na porta `8765` a partir de `D:\EMULADORES\Git\REA\research\azamerica_probe\serve`).
  - Receptor (Telnet porta 23, login automático `root`): IP descoberto automaticamente via [receiver_command.py](file:///D:/EMULADORES/Git/REA/research/azamerica_probe/receiver_command.py) (atualmente `192.168.137.235`).
- **Caminho do Add-on no Receptor:** `/mnt/plugin/plugin.video.az_iptv/`
- **Caminho do Add-on no PC:** `D:\EMULADORES\Git\REA\research\azamerica_probe\plugin.video.az_iptv\`

---

## 2. O que já foi feito e validado na TV
1. **Menu XBMC e Catálogo Assinado do GitHub (`plugin_catalog.py`):**
   - O add-on abre normalmente no XBMC do receptor, busca `catalog.json` via HTTPS (`urllib2` do Python 2.7 embutido no `dvbapp` suporta HTTPS), valida assinatura RSA-2048/SHA-256 em Python puro e salva em `/mnt/plugin/plugin.video.az_iptv/catalog_cache/catalog.json` sem tocar nas listas locais `/mnt/plugin/plugin.video.az_iptv/lists/`.
2. **Formatos de Vídeo que TOCAM com Imagem e Som no Player do S1009+:**
   - `demo.mp4` local (H.264 + AAC) -> **Funcionou**.
   - `sample720.mp4` local (TV Cultura 720p MP4) -> **Funcionou**.
   - `http://192.168.137.235:8977/cultura_540_copy.ts` (MPEG-TS 960x540 H.264 + AAC servido por HTTP local via `busybox httpd`) -> **Funcionou perfeitamente com imagem e som**.
3. **O que travou / deu erro:**
   - **Abrir HLS HTTPS direto no player (`https://player-tvcultura.stream.uol.com.br/live/tvcultura_lsd.m3u8?vhost=...`):** O player nativo (`gxplayer` dentro do `dvbapp`) conecta na porta 443, baixa a playlist `.m3u8`, fecha a conexão (`TIME_WAIT`) e fica preso em **tela preta** (não baixa os segmentos `.ts?ps=12&...` relativos com query string em HTTPS).
   - **Rodar `HlsBridge` (`hls_bridge.py`) como `threading.Thread` dentro do `default.py`:** Dá timeout / `Start to open stream please wait...` infinito.

---

## 3. A Grande Descoberta Técnica (Causa Raiz de tudo!)
Analisando o binário [dvbapp_s1009_v176](file:///D:/EMULADORES/Git/REA/research/azamerica_probe/dvbapp_s1009_v176) (função `XBMCexecute`) e os logs de `live_debug.txt`:

1. **O `dvbapp` é o único interpretador Python funcional no aparelho:**
   - O binário `/usr/bin/python` no rootfs falha com `not found` porque `/lib/ld.so.1` e `libpython2.7.so.1.0` não existem no rootfs; o Python 2.7 completo (incluindo `urllib2`, `ssl`, `socket`, `posix.fork`, `posix._exit`, `posix.setsid`, `posix.closerange`) está **estaticamente compilado dentro de `/app/dvbapp`**.
2. **Por que `threading.Thread` no `default.py` congela:**
   - `dvbapp` executa `default.py` de forma **síncrona** e **só abre o player de vídeo DEPOIS que `default.py` termina**.
   - Assim que `default.py` termina, o C++ do `dvbapp` limpa `sys.modules` e **NÃO libera o Python GIL (`PyEval_SaveThread`)** enquanto está fora do Python!
   - Resultado: qualquer `threading.Thread` iniciada pelo `default.py` fica 100% congelada assim que o menu termina, e só acorda na próxima vez que o usuário clica em algum item que chama o Python (confirmado pelos timestamps no `live_debug.txt`: os `HTTP GET /live.ts` ficaram na fila do kernel Linux e só foram processados 208 segundos depois, quando `default.py` rodou de novo!).

---

## 4. Próximo Passo Imediato (Receita Pronta para o Modelo 3.8 Flash)

Como o `posix` embutido no `/app/dvbapp` possui **`os.fork()`**, **`os.setsid()`**, **`os.closerange()`** e **`os._exit()`** (confirmado na tabela de símbolos de `initposix` em `dvbapp_s1009_v176`):

### Solução para o `HlsBridge` rodar em background de verdade:
Em vez de usar `threading.Thread` dentro do processo pai `dvbapp`, o `HlsBridge` deve iniciar um **processo filho via `os.fork()`**!
1. Quando `default.py` precisar iniciar a ponte HTTP local (`127.0.0.1:8978` ou `<local_ip>:8978`):
   - Verifica primeiro se `127.0.0.1:8978/health` já responde.
   - Se não responder, cria o socket do servidor (`_ThreadingServer` ou `HTTPServer` com `allow_reuse_address = True` e o fix de `server_bind` sem `getfqdn()`), e chama **`pid = os.fork()`**:
     - **No processo pai (`pid > 0`):** fecha o `server.socket` (para apenas o filho manter o descritor aberto) e retorna imediatamente do `default.py` para o `dvbapp` abrir o player!
     - **No processo filho (`pid == 0`):**
       - Chama `os.setsid()`
       - Importante: **NÃO** fechar os file descriptors do `server.socket`, mas nunca retornar para o C++ do `dvbapp`! Envolver tudo em `try: ... finally: os._exit(0)` (nunca `sys.exit()`, para não rodar destrutores C++ do `dvbapp` nem mexer nos dispositivos DVB do processo pai!).
       - O processo filho tem seu próprio espaço de memória e seu próprio GIL do Python 2.7! Ele continuará rodando `server.serve_forever()`, baixando a playlist `.m3u8` e os segmentos `.ts` via `urllib2` (HTTPS) e entregando MPEG-TS contínuo em `http://127.0.0.1:8978/live.ts?url=<url_encoded>` para o player do `dvbapp`!
2. **Dica extra para o `HlsBridge`:**
   - Passar a URL do canal como query parameter no endpoint local: `http://127.0.0.1:8978/live.ts?src=<urllib.quote(channel_url)>`, assim **um único daemon `fork()` na porta `8978`** serve **qualquer canal HLS** do catálogo ou da lista M3U sem precisar reiniciar o servidor a cada troca de canal!
   - Usar `HTTPServer` simples (ou fechar a conexão anterior quando um novo `GET /live.ts` chegar) para não acumular streams antigos.
   - Antes de entregar os pacotes `.ts` ou se o player demorar a sincronizar, garantir que o primeiro segmento entregue comece num pacote TS alinhado (`0x47` a cada 188 bytes) que contenha PAT/PMT/IDR (todo segmento HLS da TV Cultura já começa com PAT/PMT/IDR).
