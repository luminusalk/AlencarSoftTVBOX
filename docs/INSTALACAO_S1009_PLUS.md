# Preparação de instalação do aplicativo C no S1009+

Estado conferido em 27/10/2026: o usuário autorizou a instalação no diretório
próprio `/mnt/plugin`. O binário C-SKY do CI `40563865386` está em
`/mnt/plugin/alencarsoft_tvbox` com MD5
`311eeb0e5473b9e455338509602c129c`. O catálogo de 300 canais está em
`/mnt/plugin/iptv/iptv.m3u8`, com MD5
`266996e45994e35dc5bf4b7cb0c817e8`. O IP pode mudar a cada reboot.
Leia [SAT e lista verificada](SAT_E_LISTA_VERIFICADA.md) para o mecanismo de
inicialização via notebook e a limitação de validação audiovisual.

## Componentes do aplicativo instalado

- **Um único executável C-SKY** `alencarsoft_tvbox_csky`, compilado no GitHub
  Actions a partir da branch revisada. Ele contém o pacote de CAs embutido;
  **não** instalar `cacert.pem` separado.
- Destino do aplicativo próprio: `/mnt/plugin/alencarsoft_tvbox`. O firmware
  `/app/dvbapp`, `/app`, os drivers e o plugin Python AZ IPTV permanecem
  intactos. O backend DLNA depende do `dvbapp` ativo.
- O vídeo local de recuperação fica em
  `/mnt/plugin/iptv/fallback_particles.ts`. Instale-o com
  `python tools/install_verified_channels.py AlencarSoftTVBOX-Clang/iptv.m3u8`
  antes de trocar para um binário que o use. O arquivo contém H.264 e áudio
  silencioso para manter a compatibilidade do player do firmware.
- Artefatos de `workflow_dispatch` são obtidos na página do run. A release
  `v1.0.8-native` é antiga e **não contém** o backend DLNA atual. O job de
  release não publica esta branch.

## Procedimento reversível para futuras atualizações

1. Confirmar autorização para atualizar/testar, descobrir o IP atual da box
   (não assumir `192.168.18.23`) e verificar que `dvbapp` está ativo. Checar
   `/dev/gxirr0`: outro modelo deixou um FIFO no lugar do nó de caracteres;
   talvez seja preciso reiniciar para recuperar o estado original. O script
   `tools/receiver_command.py` descobre o receptor em duas sub-redes.
2. Baixar o artefato C-SKY do CI aprovado e calcular hash no PC. Fazer backup
   e registrar hash do binário **atualmente instalado** antes de substituí-lo.
   Não usar scripts de `build-artifacts/` sem revisão:
   alguns fixam IP e copiam certificado separado.
3. Transferir o novo executável com
   `python tools/install_csky_binary.py <caminho-do-artefato-csky>`. O script
   grava um arquivo temporário em `/mnt/plugin`, compara MD5 no receptor,
   ajusta `chmod 755` e só então troca `/mnt/plugin/alencarsoft_tvbox`.
   Conferir novamente o hash final. Manter uma única versão ativa.
4. Executar primeiro um ensaio limitado de TV Cultura via `--play-url` com
   `dvbapp` ativo. Observar imagem e som na TV, não apenas logs. Parar por
   SIGTERM normal, confirmar `Stop` e o retorno do firmware.
5. Testar grade, controle remoto, seleção, troca, BACK e sessão longa. Se
   falhar, recolocar a cópia antiga do PC com verificação de MD5. Não matar
   `dvbapp` nem desmontar `/app`.

O plugin Python `plugin.video.az_iptv` tem ciclo de instalação separado;
não alterá-lo para viabilizar o produto C. A documentação histórica de XBMC
está em `HANDOFF_RESUMO.md` e não deve ser confundida com esta instalação.
