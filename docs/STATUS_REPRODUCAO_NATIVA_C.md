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

Com alguém observando a TV: começar por um arquivo local sabidamente
reproduzível no XBMC e testar o caminho nativo `DVB demux/DVR` do diagnóstico
`dvb_native_probe_csky` com tempo limitado. Registrar tela, áudio, retorno dos
ioctls e `dmesg`. Se o driver continuar ocupado, investigar uma transferência
limpa do hardware do `dvbapp` antes de substituir o backend de reprodução do
aplicativo. Não anunciar suporte audiovisual com base apenas no início de um
processo, na presença de bytes na FIFO ou no sucesso da compilação.
