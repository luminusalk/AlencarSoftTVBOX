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
  No teste integrado de `c694046`, o renderer foi descoberto, recebeu Play e
  requisitou `GET /live.ts`, mas o usuário viu tela preta. O log anterior
  não tinha contagem de bytes enviados. O commit `7f20eef` passou a registrar
  esses bytes, erros de abertura e etapas da ponte HLS, aguardar um primeiro
  segmento e servir TS como `video/mp2t`.
- `src/player.c` seleciona DLNA no build C-SKY. O código posterior de outro
  modelo trocava `/dev/gxirr0` por FIFO e reiniciava o `dvbapp`, inclusive
  na saída. A revisão atual remove essa interferência e lê apenas dispositivos
  IR existentes. Também corrige a transparência ao reabrir o mesmo canal.
  O efeito no hardware ainda não foi observado.
- `src/hls_tls.c` voltou a embutir o CA bundle no executável, conforme o
  pedido do usuário; a versão intermediária dependia de um arquivo copiado
  para `/mnt/plugin/cacert.pem`. HTTP direto sem `.m3u` é entregue ao renderer;
  este caminho só foi testado com mock.
- CI run `40221895862` no commit `3b506e4`: testes Linux e builds C-SKY/MIPS
  passaram após a revisão de firmware/IR/CA. Isso não é confirmação audiovisual.
- Não assumir que o binário antigo está instalado: outro modelo instalou
  versões após o guia anterior e copiou um certificado para a box. Na última
  inspeção apenas de leitura, `dvbapp` estava ativo e `/dev/gxirr0` era FIFO.
  O usuário pediu trabalhar no C sem novas alterações na box.

## Trabalho pendente

1. Concluir novo build/CI; o teste sintético completo de HLS C → FIFO → HTTP
   C → renderer em `tests/test_firmware_dlna_live.py` já passou no commit
   `7f20eef`. Não atribuir a tela preta a "starvation" do FIFO sem medir:
   ele participou do teste audiovisual bem-sucedido.
2. Após nova autorização do usuário, instalar **somente** o aplicativo C
   próprio, confirmar Cultura via `--play-url` com imagem e som, então validar
   grade, IR, troca de canal, BACK/Stop e saída sem matar `dvbapp`.
3. Testar canal HTTP direto real e sessão longa. HLS master, criptografia,
   fMP4, chunked e redirecionamentos ainda não têm suporte geral.

Não alterar o plugin Python, firmware, `/app`, drivers ou configuração de
satélite. Não publicar uma release estável antes da validação integrada.
