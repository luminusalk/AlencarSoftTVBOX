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
daemon. O atalho depende do notebook ligado, conectado e com a sessão iniciada.
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
