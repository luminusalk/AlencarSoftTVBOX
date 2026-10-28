# Investigacao do flicker — 27/10/2026

## Estado real

O usuario confirma imagem/som estaveis quando o canal conecta, mas flicker durante
falha/conexao. O commit instalado 4062dc2 nao resolveu esse caso. Aumentar a
frequencia de fb_flip disputa com o firmware; nao constitui isolamento de OSD.
O usuario rejeitou video de particulas como solucao principal. Nao usar agents.

Nenhuma thread adicional do firmware foi suspensa nesta investigacao. O isolamento
existente de key_task permanece. Nao modificar o binario original ou usar SIGSTOP
no processo inteiro.

## Evidencias de leitura

- Receptor encontrado em 192.168.137.140; dvbapp PID 696 nesta sessao.
- MD5 dvbapp: d9321878bbca1a50cd33ff8004839dff, igual ao arquivo local REA.
- fb0: gxfb.0, 1280x720, 32bpp, stride 5120.
- fb1: gxstill.0, 1920x1080, 16bpp, stride 3840. Sua existencia NAO prova
  que seja uma camada OSD independente utilizavel. O C atual limpa esse dispositivo.
- As threads 706 e 723 chamam-se mainloop; nome nao basta para identificar GUI.
- Leitura direta /proc/696/mem foi recusada pelo kernel. Nenhuma memoria foi escrita.

## Analise estatica reproduzivel

Ghidra instalado em REA/research/tooling, linguagem csky:LE:32:v1.
Projeto local build-artifacts/dvbapp-osd.gpr, programa dvbapp_s1009_v176.
REA CLI nao reconheceu essa arquitetura ELF; nao reinstalar ferramentas.
Scripts InspectOsd.java e InspectUiLoop.java em build-artifacts.
As saidas do decompilador mostram _UNK nas chamadas indiretas: resolver valores
das literal pools antes de atribuir nomes. Trechos impressos apos o final de uma
funcao podem pertencer a outra funcao ja analisada; nao presumir continuidade.

- entry 0xb310 -> main wrapper 0x763238 -> 0x2ad118.
- 0x2aa490 cria thread de nome mainloop, entrada 0x2a996c.
- 0x2a996c configura Video/Audio/PVR, chama 0xb40c e termina em loop de espera.
- 0xb40c encadeia inicializacoes, incluindo 0x2dccfc; esta ultima teve timeout
  de decompilacao. Nao concluir que todas sejam GUI ou que possam ser suspensas.
- Resultados em dvbapp-ui-loop.txt, dvbapp-ui-entry.txt,
  dvbapp-ui-dispatch.txt, dvbapp-process-entry.txt, dvbapp-main.txt.

## Catalogo pendente

A lista local foi regenerada com 265 entradas apos rechecagem de 592 fontes
(433 respostas aprovadas antes de deduplicacao/selecao). Ainda nao instalada.
Esse teste de amostras nao comprova estabilidade prolongada. Preservar alteracoes
em tools/scan_live_channels.py, finalize_live_channels.py, test_channel_scanner.py,
recheck_live_sequence.py e AlencarSoftTVBOX-Clang/iptv.m3u8.
