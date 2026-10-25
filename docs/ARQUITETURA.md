# Contexto do receptor e do player

O Azamerica S1009+ usa NationalChip GX6622, Linux 3.0.8 e `dvbapp` como
aplicação de fábrica. O `dvbapp` contém o XBMC/Python do firmware e possui um
renderizador DLNA/UPnP `AVTransport:1`. O AZ IPTV Python funcional entrega
URLs ao player do firmware; ele é referência de comportamento, não a solução
em C pedida pelo usuário.

A experiência antiga com um servidor Python em thread dentro do XBMC mostrou
travamento após o script retornar. Esse registro histórico não exige Python
na solução atual: a ponte HLS, o servidor HTTP local e o cliente DLNA são C.
O CGI BusyBox foi usado apenas para comprovar o caminho audiovisual antes
da integração. O produto atual já tem servidor HTTP em `firmware_dlna.c`.

O framebuffer `/dev/fb0` é um plano de interface acima do vídeo. O código
limpa esse plano com `0x00000000` durante reprodução. O benefício dessa
transparência foi inferido da arquitetura e aplicado; o resultado com o
aplicativo C integrado ainda deve ser observado na TV. Não presumir que
qualquer tela preta seja exclusivamente problema de alfa: áudio, URI,
metadados, requisição HTTP e disputa com `dvbapp` também importam.

O resumo dos ensaios, erros e limites está em
[STATUS_REPRODUCAO_NATIVA_C.md](STATUS_REPRODUCAO_NATIVA_C.md), e as instruções
de continuidade estão no
[GUIA_MESTRE_CONTINUIDADE_E_HANDOVER.md](GUIA_MESTRE_CONTINUIDADE_E_HANDOVER.md).
