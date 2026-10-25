# Estado da reprodução nativa em C — 25/10/2026

Objetivo: selecionar um canal no aplicativo C do Azamerica S1009+ e obter
imagem **e** som. A versão Python funciona dentro do player XBMC do `dvbapp`;
isso não demonstra que o executável separado `showcartoon` consiga reproduzir
o mesmo fluxo.

## Evidência no aparelho

- O `/app/dvbapp` está ativo após a última reinicialização. Dois ensaios curtos
  com `showcartoon` e arquivo local H.264/MP3 terminaram em tela preta, inclusive
  depois de parar o `dvbapp` e limpar o framebuffer. O segundo ensaio registrou
  `gxav_ioctl : 962` e um canal de áudio já vinculado no driver. O aparelho foi
  reiniciado e o `dvbapp` voltou a funcionar. **Não há reprodução audiovisual
  confirmada do aplicativo C nesta sessão.**
- O guia mestre atribui a um `GXAV_IOCTL_FIFO_UNLINK` em `player.c` a liberação
  desse vínculo. A decompilação de `gxav_module_unlink` em `av.ko` mostra que a
  operação exige ponteiro de canal não nulo; a requisição anterior passava
  `channel = NULL`. Essa varredura foi removida, pois não podia cumprir o
  propósito descrito e seus erros eram ignorados.
- O relato de reprodução H.264/AAC com imagem e som no XBMC, registrado em
  `HANDOFF_RESUMO.md`, contradiz a afirmação de que o receptor não suporta AAC.
  O codec de áudio ainda deve ser medido no caminho C escolhido.
- Em 25/10, o teste limitado de `dvb_native_probe_csky` com
  `/mnt/plugin/cultura_10s.ts` foi repetido duas vezes com o `dvbapp` ativo.
  Os filtros de vídeo/áudio e `/dev/dvb/adapter0/dvr0` abriram, e os bytes MPEG-TS
  foram escritos. O usuário confirmou que **não houve imagem nem som** em nenhum
  dos dois testes. O sucesso dos ioctls e da escrita não prova que os módulos
  de decodificação e saída tenham sido configurados.
- A ponte C-SKY baixou 188000 bytes de um segmento MPEG-TS servido por HTTP na
  rede local do receptor, sem encerrar o `dvbapp`.
- O HTTPS parava na inicialização da entropia: Mbed TLS recorria ao
  `/dev/random` do Linux 3.0.8, enquanto `entropy_avail` no receptor era 7.
  O código seleciona `/dev/urandom` pela API de plataforma do Mbed TLS. Após a
  correção, o teste HTTPS local validou o certificado e recebeu 188000 bytes
  com MD5 exatamente igual ao segmento servido. O receptor também recebeu
  188000 bytes MPEG-TS pela URL HTTPS ao vivo da TV Cultura; os quatro pacotes
  inspecionados tinham byte de sincronismo `0x47`. **Ainda não houve reprodução
  audiovisual.**
- `kill -TERM` no `dvbapp` foi capturado/ignorado pelo processo, que continuou
  ativo e sintonizando satélite. Por isso não foi feito novo ensaio de
  `showcartoon` após essa tentativa.

## Correções disponíveis na branch `codex/native-iptv-repair`

- O aplicativo controla o PID do `showcartoon`, detecta a saída do processo e
  interrompe apenas o processo que iniciou. A interface diferencia espera por
  dados HLS de falha de abertura. Processo vivo e dados disponíveis ainda não
  constituem prova de imagem ou som na TV.
- A ponte limita cópias HTTP, rejeita respostas interrompidas e códigos HTTP
  incorretos, processa segmentos MPEG-TS por sequência HLS, e para a escrita
  da FIFO sem bloquear indefinidamente quando não há consumidor.
- A compilação C-SKY usa Mbed TLS com verificação obrigatória de certificado e
  hostname. O arquivo `AlencarSoftTVBOX-Clang/certs/cacert.pem` deve ser copiado
  para `/mnt/plugin/iptv/cacert.pem` na instalação do receptor.
- A playlist master, criptografia HLS, segmentos fMP4, respostas HTTP chunked e
  redirecionamentos ainda não são aceitos. A ponte retorna erro ou permanece em
  espera até o limite; não deve interpretar esses dados como MPEG-TS.

## Próxima validação no receptor

Identificar no `dvbapp`/`gxplayer` a sequência de inicialização dos módulos de
decodificação e saída antes de repetir o teste audiovisual. Não anunciar suporte audiovisual com base
apenas no início de um processo, na presença de bytes na FIFO, no sucesso dos
ioctls ou na compilação.
