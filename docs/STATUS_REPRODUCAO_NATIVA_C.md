# Estado da reprodução C no S1009+ — 25/10/2026

**Há imagem e som comprovados para a ponte HLS C-SKY ligada ao player DLNA do
firmware em um teste de 25 segundos. O aplicativo C integrado ainda não teve
imagem e som confirmados na TV.** Esta distinção governa qualquer relato de
sucesso. Leia o [guia mestre](GUIA_MESTRE_CONTINUIDADE_E_HANDOVER.md) antes de
continuar.

## Comprovado no receptor

- O XBMC reproduziu H.264/AAC com imagem e som, inclusive MPEG-TS da TV
  Cultura por HTTP local. Logo, não declarar AAC incompatível.
- Duas tentativas de DVB direto e uma de `showcartoon` com `dvbapp` pausado
  falharam em imagem e som. Houve `gxav_ioctl : 962` e reboot para recuperar
  a interface original. A varredura antiga de FIFO por ioctl foi removida
  por não corresponder à implementação de `av.ko`.
- A ponte C-SKY baixou segmento HTTP/HTTPS, validou certificado e hostname,
  produziu MPEG-TS íntegro e não travou na entropia após usar `/dev/urandom`.
- O caminho C HLS -> FIFO -> CGI HTTP local -> DLNA do `dvbapp` exibiu
  **imagem e som**, confirmados pelo usuário em 25 segundos. O Python no PC
  controlou o ensaio; o stream foi baixado/processado em C na box.
- A descrição DLNA estava no IP da box, porta 49152, e o serviço
  `AVTransport:1` respondeu a `SetAVTransportURI`, `Play` e `Stop` com
  DIDL-Lite completo. IP e porta podem mudar.

## Estado do código integrado

- `src/firmware_dlna.c` implementa descoberta local, HTTP loopback e SOAP.
  A primeira versão falhou na descoberta dentro da box. A correção por
  `/proc/net/tcp` foi testada em mock Linux, mas **não na TV**.
- `src/player.c` seleciona DLNA no build C-SKY. `src/main.c` preserva o
  `dvbapp`; o OSD usa alfa zero na reprodução. O controle remoto com os dois
  processos ativos ainda não foi observado.
- `src/hls_tls.c` embute o CA bundle no executável; não há arquivo de
  certificado para instalar. HTTP direto sem `.m3u` é entregue ao renderer;
  este caminho só foi testado com mock.
- CI run `40200018458` no commit `a6a3114`: testes Linux e builds C-SKY/MIPS
  passaram. Isso não é confirmação audiovisual.
- A box está com o **binário anterior** em `/mnt/plugin/alencarsoft_tvbox`
  (MD5 `4b9f99a5105c21a5b8497e9e61f010ae`). O novo build não foi
  instalado. O usuário pediu pausa de testes/alterações na box.

## Trabalho pendente

1. Revisar a descoberta e o servidor HTTP C, inclusive timeouts, reconexão
   e parada. Manter testes simulados no CI.
2. Após nova autorização do usuário, instalar **somente** o aplicativo C
   próprio, confirmar Cultura via `--play-url` com imagem e som, então validar
   grade, IR, troca de canal, BACK/Stop e saída sem matar `dvbapp`.
3. Testar canal HTTP direto real e sessão longa. HLS master, criptografia,
   fMP4, chunked e redirecionamentos ainda não têm suporte geral.

Não alterar o plugin Python, firmware, `/app`, drivers ou configuração de
satélite. Não publicar uma release estável antes da validação integrada.
