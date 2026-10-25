# Guia mestre de continuidade — IPTV C no Azamerica S1009+

Atualizado em 25/10/2026 após a análise do teste integrado. Leia
antes de agir e confira o Git e o CI atualizados. O objetivo é abrir o
aplicativo **em C**, escolher um canal e obter **imagem e som**. O AZ IPTV em
Python funciona como referência, mas não é o produto a implementar. Este guia
substitui o texto antigo, que continha afirmações não comprovadas.

## Estado ao interromper o trabalho

- Repositório: `D:\EMULADORES\Git\AlencarSoftTVBOX`.
- Branch: `codex/native-iptv-repair`. Conferir `git log -1` e a execução
  mais recente do workflow antes de instalar qualquer artefato; os números
  antigos abaixo são históricos.
- GitHub Actions run `40221253923` passou no commit `7f20eef`, incluindo o
  novo teste HLS C → FIFO → HTTP C → renderizador simulado e build C-SKY.
  Alterações posteriores em `main.c`, `input.c` e `hls_tls.c` precisam de
  novo build. A publicação de release só ocorre em `main`.
- Após o guia original, outro modelo instalou um aplicativo e copiou
  `/mnt/plugin/cacert.pem` para a box. **Não assumir** o MD5 antigo ou que
  a instalação tenha sido restaurada. Na última inspeção apenas de leitura,
  `dvbapp` estava ativo e `/dev/gxirr0` era um FIFO criado pelo aplicativo
  anterior, enquanto `/dev/gxirr_real` era o dispositivo de caracteres.
- O usuário pediu trabalhar no **C sem alterar a box agora**. Não instalar,
  reiniciar ou executar testes no receptor até combinar a validação na TV.
  Revisão de código, documentação e CI não tocam a box.
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
do canal: `Renderizador local nao encontrado`. A descoberta por
`/proc/net/tcp` passou a localizar o renderer. No teste posterior com o
aplicativo instalado (`c694046`), o log comprovou `SetAVTransportURI`/`Play`
aceitos e `GET /live.ts` pelo `Lavf/58.7`, mas o usuário viu **tela preta**.
Esse log **não comprovou que bytes TS saíram pelo socket**. Não declarar o
aplicativo concluído. O palpite de "starvation pelo FIFO" não é diagnóstico:
o FIFO fez parte do teste que deu imagem e som.

O código de `c694046` substituía `/dev/gxirr0` por FIFO e matava/reiniciava
`dvbapp` na entrada e saída. Isso interferia justamente no player do teste
bem-sucedido e contrariava a preservação do firmware. A revisão atual remove
esse manejo para o backend DLNA. A tela também pulava a limpeza transparente
ao reabrir o mesmo canal depois de voltar à grade; o cache do render foi
corrigido. **O impacto audiovisual dessas correções ainda depende de teste na
TV.**

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
  como `video/mp2t` no cabeçalho HTTP; o DIDL-Lite conserva `video/mpeg`,
  como no teste bem-sucedido.
- Para URL HTTP direta sem `.m3u`, o commit `a6a3114` passa a URL original ao
  renderizador. Foi testado com renderizador simulado, **não** na TV. A lista
  local de exemplo é majoritariamente composta desse tipo de URL.
- `src/hls_tls.c` volta a embutir `certs/cacert.pem` **no executável** via `.incbin`.
  Não instalar certificado separado. O TLS verifica cadeia e hostname.
  `HLS_CA_BUNDLE` permanece apenas como override de teste. O uso de
  `/dev/urandom` evita a espera sem fim que ocorreu com `/dev/random` no
  Linux 3.0.8 da box.
- `src/main.c` **não mata `dvbapp` nem desmonta `/app` no backend DLNA** e não
  inicia outra instância ao sair. Limpa o framebuffer com alfa zero em cada
  nova abertura. `--play-url URL` permite diagnóstico sem controle remoto.
- `src/input.c` abre somente dispositivos de caracteres já existentes; não
  modifica `/dev/gxirr0`. A competição de leitura IR com o `dvbapp` ativo
  ainda precisa ser observada na TV.

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
| Aplicativo C integrado no commit `c694046` | Play aceito e GET recebido; usuário viu tela preta | Faltava comprovar bytes TS e revisar interferência do reinício de `dvbapp`. |
| CI `40221253923` no commit `7f20eef` | Build e teste completo de transporte simulados passaram | Não confirma TV, IR ou estabilidade longa. |

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
- `Play` aceito, `GET` recebido, bytes TS enviados ou `hls_bridge_has_data()`
  não comprovam imagem nem som. Só a observação na TV confirma.
- Framebuffer e IR podem competir com `dvbapp`. Verificar seleção, OK, alfa
  transparente, BACK, Stop, volta à grade e saída.
- MIPS compila, mas o backend correspondente não foi validado em aparelhos.

## Restrições de preservação

1. Não alterar firmware original `/app/dvbapp`, `/app`, MTD, drivers,
   configuração satelital nem `plugin.video.az_iptv`. Não matar `dvbapp` no
   backend DLNA: ele foi o decodificador que deu imagem e som.
2. Não instalar/executar builds na box até o usuário retomar os testes. A
   ordem atual é corrigir o C, documentar e validar no CI.
3. Não executar scripts antigos de `tools/` ou `build-artifacts/` sem revisar:
   alguns fixam IP, alteram plugin ou pressupõem certificado separado.
   Preserve os arquivos não rastreados.
4. Não publicar esta branch como estável nem usar a release antiga
   `v1.0.8-native` como se contivesse o backend novo.
5. Quando o usuário voltar a autorizar instalação, trocar **somente** o
   aplicativo próprio em `/mnt/plugin`, conferir hash e manter rollback. O
   certificado já está no binário C; a cópia antiga está em `bin/`.

## Próximos passos para Luna em esforço alto

1. Conferir o diff após `7f20eef`, Git/CI e logs reais. O teste completo de
   transporte está em `AlencarSoftTVBOX-Clang/tests/test_firmware_dlna_live.py`.
2. Usar Linux/CI para testar DIDL-Lite, URLs HLS e HTTP direto, HEAD/GET,
   interrupções e Stop limitado. O workflow é `.github/workflows/build_tvbox.yml`;
   `push` nesta branch não o dispara, usar `workflow_dispatch` manual.
3. Preparar um único binário C-SKY com CA embutida e atualização reversível
   do aplicativo próprio. Não depender de `/mnt/plugin/cacert.pem`.
4. **Só quando o usuário autorizar testes na TV:** primeiro confirmar estado
   de `/dev/gxirr0` e `dvbapp`; o FIFO deixado pelo aplicativo anterior pode
   exigir reboot para recuperar o nó original. Executar brevemente
   `--play-url` com TV Cultura e `dvbapp` ativo; verificar log de playlist,
   segmento, `GET`, contagem de bytes TS e **imagem e som com o usuário**.
   Parar normalmente e confirmar que o sistema original permanece ativo.
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
