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
