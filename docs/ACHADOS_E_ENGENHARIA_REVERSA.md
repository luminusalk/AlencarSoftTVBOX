# Achados, acertos e erros — S1009+ e aplicativo C

Este é um registro de **evidência**, não uma coleção de hipóteses tratadas
como fatos. O objetivo é reproduzir vídeo e áudio pelo aplicativo C sem
alterar o firmware original. O estado e as próximas ações estão no
[guia mestre](GUIA_MESTRE_CONTINUIDADE_E_HANDOVER.md).

## Plataforma e ferramentas

- Receptor observado: Azamerica S1009+, NationalChip GX6622, C-SKY CK610,
  Linux 3.0.8. O `dvbapp` de fábrica fica em `/app/dvbapp`.
- O acesso Telnet root porta 23 foi usado no diagnóstico. O IP mudou após
  reinicialização; `192.168.18.23` foi o último observado, **não constante**.
  `tools/receiver_command.py` procura a box nas sub-redes conhecidas.
- O compilador usado no CI é o antigo NationalChip C-SKY/uClibc. O build
  estático no GitHub Actions passou; builds x86 ou toolchains C-SKY modernos
  não são substitutos automaticamente compatíveis.
- `REA/` ao lado do repositório guarda binários/pesquisas; uma cópia local
  de `showcartoon` tinha MD5 igual à da box, e estudos Ghidra/REA foram feitos.
  Não há necessidade de reinstalar essas ferramentas só para continuar.

## Sequência de diagnóstico audiovisual

1. **Python/XBMC:** H.264/AAC e MPEG-TS da Cultura via HTTP local produziram
   imagem e som. AAC incompatível era uma inferência errada do guia antigo.
2. **`showcartoon`:** comandos com `dvbapp` pausado e OSD limpo não mostraram
   imagem nem som; o driver registrou `in_channel[1] is linked` e
   `gxav_ioctl : 962`. Foi necessário reboot. Não usar esse teste como prova
   de que AAC ou a FIFO HLS falham; é um problema de controle/posse A/V.
3. **DVB direto:** filtros de áudio/vídeo e `/dev/dvb/adapter0/dvr0` aceitaram
   operação e bytes MPEG-TS foram escritos, mas o usuário não viu imagem nem
   ouviu som em duas tentativas. Escrever TS não inicializa todo o player.
4. **Ponte C HLS:** overflow e cópias sem limite, respostas HTTP inválidas,
   parada bloqueante da FIFO, URL/HTTPS e estado falso de reprodução foram
   corrigidos na branch de reparo. O teste C-SKY obteve 188000 bytes via
   HTTPS com certificado/hostname validados e MD5 correto.
5. **Entropia TLS:** Mbed TLS aguardava `/dev/random` com entropia baixa no
   kernel antigo. Selecionar `/dev/urandom` pela API Mbed TLS permitiu o
   handshake. Em seguida, o CA bundle foi embutido no executável C; não há
   arquivo de CA para instalar separadamente.
6. **Renderizador DLNA do firmware:** descrição UPnP em porta dinâmica,
   `AVTransport:1`, controle SOAP. Metadados DIDL-Lite vazios causaram
   timeout; título, classe `object.item.videoItem` e recurso
   `http-get:*:video/mpeg:*` permitiram `SetAVTransportURI` e `Play`.
7. **Primeiro sucesso com C:** TV Cultura HTTPS -> HLS em C-SKY -> FIFO ->
   BusyBox CGI HTTP -> DLNA do `dvbapp` deu **imagem e som** por 25 segundos,
   confirmados pelo usuário. Python no PC só controlou o experimento.
8. **Integração ao aplicativo:** o primeiro build C (`686cdd8`) não achou o
   renderer usando SSDP unicast de dentro da box. A descrição HTTP era
   acessível a partir da própria box, e `/proc/net/tcp` mostrava a porta
   49152 em `LISTEN`. O código passou a enumerar portas locais e verificar
   `/description.xml`, com SSDP como fallback. O teste posterior (`c694046`)
   encontrou o renderer, recebeu Play e `GET /live.ts`, mas o usuário viu
   **tela preta**. Não havia contagem de bytes TS no log desse teste.
9. **Catálogo HTTP direto:** a lista de exemplo tem muitas URLs HTTP sem
   `.m3u`. O backend inicialmente rejeitava essas URLs. O commit `a6a3114`
   passou a URI diretamente ao renderizador; mock Linux passou, mas canais
   reais ainda não foram observados.
10. **Interferência do aplicativo anterior:** o código de `c694046` removia
    `/dev/gxirr0`, criava FIFO no lugar e matava/reiniciava `dvbapp` ao entrar
    e sair. O DLNA dependia desse mesmo `dvbapp`. A revisão atual remove o
    manejo para o backend DLNA. O impacto real ainda não foi medido na TV.
11. **Transporte integrado:** `7f20eef` adicionou contagem de bytes TS no
    HTTP local, logs de playlist/segmentos e teste sintético HLS C → FIFO →
    HTTP C → renderer. O CI `40221253923` passou. O FIFO não pode ser
    declarado causa da falha sem medição: ele estava no caminho que deu som
    e imagem. O cache do OSD também foi corrigido para limpar a tela ao
    reabrir o mesmo canal. O CI `40221895862` passou após a revisão de
    firmware/IR/CA, ainda sem novo teste audiovisual.

## Alegações antigas corrigidas

- A rotina `gxav_cleanup_audio()` antiga não era solução comprovada: a
  função decompilada de `av.ko` exige ponteiro de canal válido, mas a rotina
  passava nulo. Foi removida. Não repetir a varredura cega de ioctls.
- Não afirmar que `showcartoon` foi compilado com `--disable-network`:
  essa flag não apareceu na configuração FFmpeg extraída. A falha observada
  de reprodução é real; essa explicação específica não foi demonstrada.
- Não afirmar que vídeo depende sempre de PTS de áudio nem que o DSP não
  contém AAC só porque algumas linhas de firmware listam outros codecs.
  O player do firmware reproduziu AAC com imagem e som.
- Não afirmar que simples transparência do framebuffer resolve toda tela
  preta. Ela é necessária para revelar o plano de vídeo, mas falhas de URI,
  player, áudio, rede e posse dos dispositivos podem produzir o mesmo sintoma.
- CI, PID ativo, estado DLNA `PLAYING` e bytes na FIFO não equivalem a
  observação de imagem e som.

## Descobertas ainda abertas

- A disputa entre aplicativo C e `dvbapp` por `/dev/gxirr0` e `/dev/fb0`
  ainda não foi verificada na TV.
- HLS master, criptografia, fMP4, chunked e redirecionamentos não têm suporte
  geral na ponte atual. O player do firmware pode ou não aceitar formatos
  de URLs HTTP diretas do catálogo; medir caso a caso.
- O usuário relatou um teste anterior com vídeo e som antes de iniciar
  `dvbapp` após boot. É pista para um backend alternativo futuro, mas não foi
  reproduzida aqui e não justifica matar o `dvbapp` do caminho DLNA comprovado.
- O teste de 25 segundos não mede estabilidade longa, troca de canal nem
  repetição de Start/Stop.

## Preservação

O usuário pediu não fazer novas alterações na box enquanto o C é corrigido.
Outro modelo instalou versões depois da restauração histórica, inclusive um
CA em `/mnt/plugin/cacert.pem`; não presumir o estado antigo como atual.
Na última inspeção apenas de leitura, `dvbapp` estava ativo e `/dev/gxirr0`
era FIFO. Não modificar `/app`,
firmware, plugin Python, MTD ou canais de satélite. Não rodar scripts antigos
de deploy sem revisão; preservar `bin/`, `build-artifacts/` e `tools/` não
rastreados. A próxima instalação depende de nova autorização do usuário.
