# Preparação de instalação do aplicativo C no S1009+

**Não executar esta instalação agora.** O usuário pediu trabalhar no C sem
novas alterações no receptor. Houve instalações por outro modelo depois do
guia inicial; **não presumir** qual binário está na box nem usar o MD5 antigo
como estado atual. O trabalho novo está na branch `codex/native-iptv-repair`. Consulte o
[guia mestre](GUIA_MESTRE_CONTINUIDADE_E_HANDOVER.md) ao retomar.

## O que será instalado quando o usuário autorizar

- **Um único executável C-SKY** `alencarsoft_tvbox_csky`, compilado no GitHub
  Actions a partir da branch revisada. Ele contém o pacote de CAs embutido;
  **não** instalar `cacert.pem` separado.
- Destino do aplicativo próprio: `/mnt/plugin/alencarsoft_tvbox`. O firmware
  `/app/dvbapp`, `/app`, os drivers e o plugin Python AZ IPTV permanecem
  intactos. O backend DLNA depende do `dvbapp` ativo.
- Artefatos de `workflow_dispatch` são obtidos na página do run. A release
  `v1.0.8-native` é antiga e **não contém** o backend DLNA atual. O job de
  release não publica esta branch.

## Procedimento reversível para a sessão futura

1. Confirmar autorização para atualizar/testar, descobrir o IP atual da box
   (não assumir `192.168.18.23`) e verificar que `dvbapp` está ativo. Checar
   `/dev/gxirr0`: outro modelo deixou um FIFO no lugar do nó de caracteres;
   talvez seja preciso reiniciar para recuperar o estado original. O script
   `tools/receiver_command.py` descobre o receptor em duas sub-redes.
2. Baixar o artefato C-SKY do CI aprovado e calcular hash no PC. Fazer backup
   e registrar hash do binário **atualmente instalado** antes de substituí-lo.
   Não usar scripts de `build-artifacts/` sem revisão:
   alguns fixam IP e copiam certificado separado.
3. Transferir o novo executável pela rede para um arquivo temporário dentro
   de `/mnt/plugin`, comparar hash no receptor, ajustar `chmod 755` e fazer
   `mv` para `/mnt/plugin/alencarsoft_tvbox` somente após verificar a cópia.
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
