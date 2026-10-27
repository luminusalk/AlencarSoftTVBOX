# SAT após reiniciar e catálogo de canais

No S1009+, `/etc/init.d/rcS` executa `/etc/appS.d/S00app`; ambos estão na raiz
SquashFS somente leitura. `S00app` inicia `dvbapp` e não executa scripts em
`/mnt/plugin`. Portanto, gravar `start_sat_daemon.sh` em `/mnt/plugin` **não**
faz a tecla SAT funcionar depois de um reboot. Não substituir nem remontar o
firmware original para contornar isso.

Com o notebook conectado, `tools/install_sat_watcher.ps1` registra um monitor
no início da sessão Windows (`HKCU Run`) e o inicia imediatamente. O monitor
`tools/sat_boot_watcher.py` descobre o IP atual da box, espera o `dvbapp` subir
e executa o modo C `--sat-daemon` quando necessário. O binário continua em
`/mnt/plugin/alencarsoft_tvbox`. O log fica em
`%LOCALAPPDATA%\AlencarSoftTVBOX\sat_boot_watcher.log`. Em teste com reboot,
o IP mudou de `192.168.137.35` para `192.168.137.149` e o monitor reativou o
daemon. O monitor também ajusta o relógio UTC quando a box volta para 1970;
sem isso a validação HTTPS do certificado da TV Cultura falha com `-9984`.
O atalho depende do notebook ligado, conectado e com a sessão iniciada.
Para remover só o monitor do notebook, rode `tools/uninstall_sat_watcher.ps1`.

`tools/scan_live_channels.py` percorre todos os M3U do repositório local
`Iptv-Brasil-2026`, descarta VOD e testa apenas candidatos a canais ao vivo.
O teste exige resposta MPEG-TS ou HLS com segmento MPEG-TS; HTTP 200 sozinho
não basta. Os relatórios em `build-artifacts/channel_scan*.json` mostram totais
e falhas sem copiar URLs completas. `tools/finalize_live_channels.py` combina
os resultados em até 300 canais, privilegiando notícias, canais abertos e
esportes, e mantendo versões HD/FHD quando verificadas. A lista final é
`AlencarSoftTVBOX-Clang/iptv.m3u8`.

`tools/install_verified_channels.py AlencarSoftTVBOX-Clang/iptv.m3u8` transfere
essa lista para `/mnt/plugin/iptv/iptv.m3u8`, compara MD5, preserva a lista
anterior em `/mnt/plugin/iptv/iptv.before-verified.m3u8` e atualiza a cópia de
fallback em `/mnt/plugin/lists/Canais ao Vivo.m3u`. O app C lê o catálogo quando
é aberto. Fluxo válido no computador é uma triagem; imagem e som de cada canal
dependem de confirmação na TV e podem mudar quando a fonte expirar.

A versão C do run `40563865386` foi vista na TV abrindo a Cultura com imagem,
som e sem flicker no teste curto. No teste prolongado, porém, o `Lavf` fechou
o HTTP após 1,27 MB; o UPnP continuou anunciando `PLAYING` e a interface do
firmware voltou a piscar sobre o aplicativo. Logo, o estado UPnP isolado não
prova reprodução contínua.

O reparo posterior monitora se o HTTP local entregou bytes e se o cliente
desconectou. Quando o canal cai, o C inicia
`/mnt/plugin/iptv/fallback_particles.ts`, um vídeo H.264 de partículas com
trilha AAC silenciosa, mantém a mensagem de perda de conexão e tenta reabrir
o mesmo canal a cada oito segundos. MENU/EXIT cancela a reconexão e mostra a
grade. A grade e a tela de conexão são redesenhadas quando o `dvbapp` altera
o framebuffer; a transição para DLNA mantém um quadro opaco. O arquivo local
vem do projeto, não do firmware. O vídeo precisa ser instalado junto com o
executável: `tools/install_verified_channels.py` transfere e confere os dois
arquivos por MD5. O comportamento visual desse reparo no hardware ainda deve
ser confirmado antes de anunciá-lo como resolvido.

## Ajustes de 27/10/2026 para quedas e flicker

O commit `23c0aa4` (com testes corrigidos nos commits seguintes) muda a
transição de canal do aplicativo C. Antes, `app_start_channel()` limpava o
framebuffer com alfa zero **antes** de parar e trocar o fluxo DLNA. O plano do
firmware ficava exposto nessa janela. A transição e o estado `BUFFERING` agora
desenham um quadro opaco; o vídeo só recebe transparência quando o player C
considera a reprodução iniciada. Enquanto não há vídeo, a proteção do
framebuffer continua restaurando as linhas que o `dvbapp` sobrescrever.

A ponte HLS já possuía ring buffer de 4 MiB, mas liberava `Play` após o
primeiro bloco de 64 KiB. Agora espera cerca de três segundos de mídia,
estimados pelo `#EXTINF` e pela fração do segmento baixado. Também libera com
1 MiB e pelo menos dois segundos estimados; com 3 MiB libera por segurança
para não encher o ring buffer de 4 MiB antes de existir consumidor. Quando a
playlist não informa duração, usa 1 MiB. Isso é um pré-buffer de **início**,
não garantia de que a internet manterá o bitrate do canal por horas.

A reprodução HLS agora segue redirecionamentos HTTP de playlist e segmento,
usando a URL final como base dos caminhos relativos. Antes o probe aceitava
um redirect, mas a reprodução falhava na mesma URL. A capacidade temporária
de segmentos da playlist subiu de 16 para 64. A checagem de estado DLNA passa
a tolerar `STOPPED` transitório enquanto nosso servidor ainda entrega bytes,
mas por no máximo cinco segundos; queda persistente continua acionando a
recuperação. Desenho e navegação consultam o estado em cache, sem bloquear em
SOAP a cada tecla. O relógio exibido pelo C usa `BRT3` (Brasília, UTC−3),
sem alterar o relógio UTC do sistema usado por TLS.

O run `40576280679` passou nos testes Linux e nas compilações C-SKY e MIPS.
O executável C-SKY foi instalado em `/mnt/plugin/alencarsoft_tvbox` com MD5
`a5d3f25fb33acfe38fb26eaf16c9015d`. Após reboot, a box recebeu
`192.168.137.247`; o watcher do notebook iniciou `--sat-daemon` e o log da
box registrou `SAT 0x6a` abrindo `--headless-osd-ir`. Na retomada da Cultura,
o pré-buffer liberou o Play com 418.148 bytes e 3.648 ms estimados; depois o
HTTP entregou pelo menos 4,19 MiB em 77 segundos com estado `PLAYING`.
Esses logs não confirmam por si só imagem, áudio ou ausência de flicker na TV.

O atalho SAT após reboot continua dependendo do watcher do notebook descrito
acima; não existe hook de inicialização confirmado em `/mnt/plugin` e o
firmware original não deve ser modificado.
