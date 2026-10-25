# Guia mestre de continuidade — IPTV C no Azamerica S1009+

Atualizado em 25/10/2026 para a próxima sessão com Luna em esforço alto. Leia
antes de agir e confira o Git e o CI atualizados. O objetivo é abrir o
aplicativo **em C**, escolher um canal e obter **imagem e som**. O AZ IPTV em
Python funciona como referência, mas não é o produto a implementar. Este guia
substitui o texto antigo, que continha afirmações não comprovadas.

## Estado ao interromper o trabalho

- Repositório: `D:\EMULADORES\Git\AlencarSoftTVBOX`.
- Branch: `codex/native-iptv-repair`; último commit de código: `a6a3114`.
- GitHub Actions `build_tvbox.yml`, run `40200018458`: testes Linux, builds
  C-SKY/MIPS e job de release terminaram com **success**. A publicação real
  de release só ocorre em `main`; esta branch não foi publicada como release.
- A instalação anterior na box foi **restaurada**. O binário
  `/mnt/plugin/alencarsoft_tvbox` tem MD5
  `4b9f99a5105c21a5b8497e9e61f010ae`; a cópia local exata está em
  `bin/alencarsoft_tvbox_csky`. O certificado copiado durante esta sessão em
  `/mnt/plugin/iptv/cacert.pem` foi removido após conferir o MD5. O novo
  binário **não está instalado** na box.
- O usuário mandou **parar os testes e alterações no aparelho** e preparar a
  documentação. Não instalar nem executar builds no receptor até ele retomar
  essa autorização. Revisão de código, documentação e CI não tocam a box.
- Preserve todos os arquivos não rastreados em `bin/`, `build-artifacts/` e
  `tools/`; muitos são pesquisas e scripts de diagnóstico anteriores.

## O que realmente deu imagem e som

O usuário confirmou **imagem e som** em dois testes com o renderizador
DLNA/UPnP do firmware `/app/dvbapp`. O teste decisivo, de 25 segundos, foi:

```text
TV Cultura HLS HTTPS
  -> ponte HLS C-SKY em C na própria box
  -> FIFO /tmp/live_stream.ts
  -> HTTP local BusyBox CGI de diagnóstico
  -> DLNA AVTransport do dvbapp
  -> imagem e som observados pelo usuário
```

O Python no computador só enviou comandos de diagnóstico e controle DLNA.
O download HTTPS e o processamento HLS ocorreram **em C no receptor**. O
plugin Python AZ IPTV não foi modificado. Um teste anterior com amostra
MPEG-TS servida do computador também deu imagem e som via DLNA. Isso valida
o caminho ponte C + player do firmware por 25 segundos, **não** a interface
integrada, o controle remoto ou estabilidade prolongada.

O primeiro teste do **aplicativo C integrado** (commit `686cdd8`) falhou antes
do canal: `Renderizador local nao encontrado`. SSDP unicast não respondeu de
dentro da própria box. O commit `212cf5a` passou a enumerar portas `LISTEN`
em `/proc/net/tcp`, ler `/description.xml` e localizar `AVTransport`, com
SSDP como fallback. A correção passou em mock Linux e compilou para C-SKY,
mas **ainda não foi testada na TV**. Não declarar o aplicativo concluído.

## Arquitetura atual do produto

- `AlencarSoftTVBOX-Clang/src/player.c` usa `RECEIVER_DLNA_BACKEND` no build
  C-SKY. O build MIPS conserva o backend legado `showcartoon` sem validação
  audiovisual equivalente.
- `src/firmware_dlna.c` descobre o renderizador **do próprio receptor**,
  obtém `controlURL` da descrição UPnP, serve HLS ou arquivo local em
  `127.0.0.1:<porta dinâmica>/live.ts` e envia `SetAVTransportURI`, `Play` e
  `Stop`. O DIDL-Lite contém título, `object.item.videoItem` e recurso
  `http-get:*:video/mpeg:*`; metadados vazios causaram timeout no diagnóstico.
- Para HLS `.m3u8`, `src/hls_bridge.c` baixa playlists e segmentos por
  HTTP(S) em C, alimenta `/tmp/live_stream.ts`, e o HTTP interno serve o TS
  como `video/mpeg`.
- Para URL HTTP direta sem `.m3u`, o commit `a6a3114` passa a URL original ao
  renderizador. Foi testado com renderizador simulado, **não** na TV. A lista
  local de exemplo é majoritariamente composta desse tipo de URL.
- `src/hls_tls.c` embute `certs/cacert.pem` **no executável** via `.incbin`.
  Não instalar certificado separado. O TLS verifica cadeia e hostname.
  `HLS_CA_BUNDLE` permanece apenas como override de teste. O uso de
  `/dev/urandom` evita a espera sem fim que ocorreu com `/dev/random` no
  Linux 3.0.8 da box.
- `src/main.c` **não mata `dvbapp` nem desmonta `/app` no backend DLNA** e não
  inicia outra instância ao sair. Limpa o framebuffer com alfa zero durante
  reprodução. `--play-url URL` permite diagnóstico sem controle remoto.
- `src/input.c` lê `/dev/gxirr0` e teclado. A competição com o `dvbapp`
  ativo ainda precisa ser observada na TV.

Na rede do teste, a descrição do renderer era
`http://192.168.18.23:49152/description.xml`, o serviço
`urn:schemas-upnp-org:service:AVTransport:1` e o controle
`/_urn:schemas-upnp-org:service:AVTransport_control`. O IP e a porta podem
mudar. **Nunca fixá-los no produto.** A descrição tinha 1929 bytes; a porta
49152 aparecia como `C000` em `/proc/net/tcp`. A box conseguiu obter essa
descrição via BusyBox `wget` no próprio IP.

## Evidências e erros que não se deve repetir

| Ensaio | Resultado | Conclusão permitida |
| --- | --- | --- |
| XBMC com H.264/AAC e MPEG-TS da Cultura | Imagem e som | AAC **não** é comprovadamente incompatível. |
| `showcartoon` com `dvbapp` pausado | Tela preta, sem som, `gxav_ioctl : 962`; reboot necessário | Não resolveu a posse do áudio. |
| DVB direto demux/DVR, duas execuções | Sem imagem nem som | Bytes e ioctls não configuraram a reprodução completa. |
| Ponte C-SKY HTTPS | 188000 bytes MPEG-TS, MD5 igual ao servidor | Transporte funciona; não comprova imagem/som. |
| Ponte C + CGI + DLNA, 25 s | **Imagem e som**, observados | Caminho audiovisual válido em teste curto. |
| Aplicativo C integrado no commit `686cdd8` | Falha na descoberta DLNA | Sem teste audiovisual do aplicativo integrado. |
| CI `40200018458` no commit `a6a3114` | Build e testes simulados passaram | Não confirma TV, IR ou estabilidade longa. |

Não ressuscitar a alegação de que a varredura `GXAV_IOCTL_FIFO_UNLINK`
resolve o áudio: a rotina antiga passava ponteiro de canal nulo, incompatível
com a função decompilada de `av.ko`, e foi retirada. Não repetir como fato a
suposta flag `--disable-network` do FFmpeg de `showcartoon`: ela não apareceu
na configuração extraída. O fato comprovado é que os testes diretos falharam.
O relato do usuário de vídeo e som **antes de iniciar o `dvbapp`** é uma pista
legítima, ainda não reproduzida nesta investigação.

## Limites do código e critério de sucesso

- HLS master, criptografia, fMP4, redirecionamentos e respostas HTTP chunked
  ainda não são suportados. Não anunciar compatibilidade com qualquer canal.
- HTTP direto entrega a URL ao renderer. Redirecionamento, autenticação,
  formato da mídia, HTTPS direto e canais reais ainda precisam ser medidos.
- Servidor HTTP C atende um cliente de cada vez. Testar reconexão, troca de
  canal, desconexão do leitor e Stop repetido.
- `Play` aceito, processo vivo ou `hls_bridge_has_data()` não comprovam
  imagem nem som. Só a observação na TV confirma.
- Framebuffer e IR podem competir com `dvbapp`. Verificar seleção, OK, alfa
  transparente, BACK, Stop, volta à grade e saída.
- MIPS compila, mas o backend correspondente não foi validado em aparelhos.

## Restrições de preservação

1. Não alterar firmware original `/app/dvbapp`, `/app`, MTD, drivers,
   configuração satelital nem `plugin.video.az_iptv`. Não matar `dvbapp` no
   backend DLNA: ele foi o decodificador que deu imagem e som.
2. Não instalar/executar builds na box até o usuário retomar os testes. A
   ordem atual é documentar e deixar o código preparado.
3. Não executar scripts antigos de `tools/` ou `build-artifacts/` sem revisar:
   alguns fixam IP, alteram plugin ou pressupõem certificado separado.
   Preserve os arquivos não rastreados.
4. Não publicar esta branch como estável nem usar a release antiga
   `v1.0.8-native` como se contivesse o backend novo.
5. Quando o usuário voltar a autorizar instalação, trocar **somente** o
   aplicativo próprio em `/mnt/plugin`, conferir hash e manter rollback. O
   certificado já está no binário C; a cópia antiga está em `bin/`.

## Próximos passos para Luna em esforço alto

1. Ler os commits `686cdd8..a6a3114`, conferir Git/CI e revisar descoberta
   via `/proc/net/tcp`, SOAP, HTTP local e tratamento de Stop. Os testes
   estão em `AlencarSoftTVBOX-Clang/tests/`.
2. Usar Linux/CI para testar DIDL-Lite, URLs HLS e HTTP direto, HEAD/GET,
   interrupções e Stop limitado. O workflow é `.github/workflows/build_tvbox.yml`;
   `push` nesta branch não o dispara, usar `workflow_dispatch` manual.
3. Preparar um único binário C-SKY com CA embutida e atualização reversível
   do aplicativo próprio. Artefato de Actions não é release publicada.
4. **Só quando o usuário autorizar testes na TV:** executar brevemente
   `--play-url` com TV Cultura e `dvbapp` ativo; verificar log de descoberta,
   SOAP, HTTP local e **imagem e som com o usuário**. Parar normalmente e
   confirmar que o sistema original permanece ativo. Nunca usar
   `killall -9 dvbapp` para esse backend.
5. Testar grade/controle remoto: OK, vídeo e áudio, BACK, outro canal,
   abrir/parar repetidamente, saída e retorno ao firmware. Testar uma URL
   HTTP direta real e sessão longa. Investigar IR/OSD só se houver conflito.
6. Atualizar este guia e `STATUS_REPRODUCAO_NATIVA_C.md` com observação
   audiovisual, não só logs. Só depois propor merge ou release.

Scripts históricos em `build-artifacts/`: `discover_receiver_upnp.py`,
`test_firmware_dlna.py`, `deploy_c_firmware_test.py`. Têm IP fixo e são
diagnósticos, não instaladores de produto. `tools/receiver_command.py`
descobre IP nas duas sub-redes. `REA/` ao lado contém pesquisas e Python
funcional: consultá-lo somente quando uma dúvida concreta exigir.
