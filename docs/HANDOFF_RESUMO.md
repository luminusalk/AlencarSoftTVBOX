# Histórico do AZ IPTV Python no XBMC

Este arquivo registra a **referência funcional anterior**, não o plano de
implementação do aplicativo C. O usuário quer o produto em C para S1009+ e
futuramente outros aparelhos. O plugin `plugin.video.az_iptv` não foi
modificado nos ensaios recentes e deve ser preservado.

No player XBMC do `dvbapp`, foram observados **imagem e som** com MP4
H.264/AAC e com MPEG-TS da TV Cultura servido por HTTP local. Essa evidência
invalida a conclusão antiga de que o aparelho não consegue reproduzir AAC.
Uma URL HLS HTTPS aberta diretamente pelo XBMC havia parado após obter a
playlist; a ponte local permitiu entregar MPEG-TS ao player do firmware.

O registro anterior estudava uma ponte Python desacoplada por `os.fork()`
porque uma thread do add-on parava quando o script retornava. Isso é contexto
histórico do XBMC. O caminho atualmente comprovado para o projeto C usa
`hls_bridge.c` no receptor e o renderizador DLNA/UPnP do `dvbapp`, sem Python
no produto. Em um ensaio de 25 segundos, o usuário confirmou imagem e som
para C HLS -> FIFO -> HTTP local de diagnóstico -> DLNA.

O aplicativo C integrado ainda aguarda validação audiovisual. A primeira
versão falhou na descoberta DLNA; a correção por `/proc/net/tcp` passou em
teste simulado e compilação, sem novo ensaio na TV. O guia atualizado é
[GUIA_MESTRE_CONTINUIDADE_E_HANDOVER.md](GUIA_MESTRE_CONTINUIDADE_E_HANDOVER.md).
