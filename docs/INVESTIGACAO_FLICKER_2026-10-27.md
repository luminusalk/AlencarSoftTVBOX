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

## Handover para GPT-6 Sol — ponto exato ao trocar modelo

### Prioridade e restricoes

Eliminar flicker na conexao/falha de canal. Cultura conectada ja funciona com
imagem e som. Nao repetir teste apenas com video saudavel e anunciar sucesso.
Usuario rejeitou depender do video local de particulas. Nao usar agents.
Preservar firmware, /app/dvbapp, MTD e plugin Python. Testes temporarios e deploy
do nosso C estao autorizados. Nao pausar dvbapp inteiro.

### Git, CI e aparelho

- Branch codex/native-iptv-repair, HEAD 263f2b4, commit/push realizados.
- Esse commit adiciona apenas diagnostico thread_snapshot.c, build e docs;
  nao contem correcao de flicker e nao foi instalado como aplicativo.
- CI 40613267287: ultima consulta: testes Linux e MIPS aprovados, C-SKY em andamento.
- Aplicativo instalado continua 4062dc2 (MD5 af16a8a7799dc675d1a8d251c70d58db).
- Box 192.168.137.140, dvbapp PID696, app1261, guardian1263, SAT daemon858 na ultima
  observacao. Redescobrir PIDs/IP antes de agir; tools/receiver_command.py descobre IP.
- Streaming seguia entregando bytes no /tmp/alencar_app.log. Nenhum novo teste
  visual realizado nesta rodada. Nenhum isolamento novo de threads foi executado.

### Proximo passo concreto: baixar e executar snapshot

Criado AlencarSoftTVBOX-Clang/tools/thread_snapshot.c. Compila como
thread_snapshot_csky dentro do artifact alencarsoft_tvbox_csky do CI acima.
Uso: /tmp/thread_snapshot PID TID. Valida processo dvbapp, recusa key_task,
faz PTRACE_ATTACH em uma thread, GETREGS, DETACH antes de imprimir. Alarm de4s
encerra tracer se necessario. Nao escreve memoria ou registradores. Ainda NAO
foi executado na box; verificar resultado da compilacao antes do deploy em /tmp.

1. Consultar CI usando tools/gh_api.py (gh nao esta no PATH):
   python -c "import sys;sys.path.insert(0,'tools');import gh_api;print([(j['name'],j['status'],j['conclusion']) for j in gh_api.api_get('actions/runs/40613267287/jobs')['jobs']])"
2. Baixar artifact com credencial via git credential fill, sem imprimir segredo.
   tools/get_csky_ci_artifact.py tem implementacao segura de redirect/download,
   mas extrai SOMENTE alencarsoft_tvbox_csky. Adaptar um script temporario para
   extrair thread_snapshot_csky. Nao instalar novamente o aplicativo por engano.
3. Servir probe no notebook por HTTP temporario, baixar em /tmp, conferir MD5.
4. Capturar separadamente threads candidatas (comece 706 e723; conferir nome/PID).
   Sao capturas breves, nao um teste de suspensao prolongada. Relacionar PC/LR ao
   Ghidra antes de decidir qualquer isolamento. Pode capturar outras threads
   ativas se necessario. Nao confundir nome mainloop com prova de thread GUI.
5. So depois planejar teste curto de isolamento e verificar simultaneamente
   ausencia de sobrescrita do framebuffer, resposta DLNA e confirmacao visual.

### Novas conclusoes estaticas

- Main wrapper0x763238 chama0x2ad118.
- 0x2ad118 instala handlers e chama0x2aa490.
- 0x2aa490 cria socketloop, hotplug e mainloop; chama0x7624c4 antes de cleanup.
- 0x7624c4 espera contador de threads usando0xa7237c com argumento3. Nao assumir
  que thread principal seja GUI somente porque apresentou atividade em /proc.
- mainloop criada por0x2aa490 tem entrada0x2a996c, que chama0xb40c;
  0xb40c chama0x2dccfc. Esta ultima e grande, decompilacao30s expirou. Possui
  chamadas de controle/eventos; papel exato e independencia do DLNA nao comprovados.
- fb1 confirmado via sysfs como gxstill.0, nao assumir camada OSD independente.

Novos arquivos de evidencia locais (build-artifacts, ignorados):
- dvbapp-real-main.txt, dvbapp-event-loop.txt, dvbapp-dispatch-calls.txt.
- ResolveCskyCalls.java: percorre corpo da funcao e resolve literal pools C-SKY.
- firmware-thread-stats.txt: captura /proc atual. Campos devem ser interpretados
  corretamente; a thread principal tem muito cminflt/cmajflt, nao confundir com CPU.
- read_ptrace_header.py: abre tar externo e interno da toolchain com Python tarfile.
  Windows tar nao conseguiu xz. Header asm/ptrace.h confirma GETREGS12,
  REGNO_PC33, SR32, SP R0 para ABIV1, LR R15. Conferir layout efetivo do dump
  GETREGS antes de interpretar; struct pt_regs e layout de indices nao sao iguais.

Ghidra: JAVA_HOME D:/EMULADORES/Git/REA/research/tooling/jdk-21.0.12.1+1;
analyzeHeadless.bat em tooling/ghidra_12.1.2_PUBLIC/support.
Projeto build-artifacts/dvbapp-osd, -process dvbapp_s1009_v176 -noanalysis,
-scriptPath caminho absoluto build-artifacts -postScript InspectUiLoop.java ADDRESS.
ResolveCskyCalls.java serve para chamadas/constantes sem decompilar funcao gigante.
Nao imprimir instrucoes adjacentes como se todas pertencessem ao mesmo corpo.

### Pendencias preservadas

Catalogo local265 ainda nao instalado/commitado; manter modificacoes do scanner.
SAT persistente atualmente depende de watcher Windows no notebook; nao afirmar
que ja existe startup autonomo instalado no firmware. Nao alterar esse mecanismo
no meio da investigacao do flicker.

## Correcao da interpretacao visual

Correcao do usuario: ele observou os dois testes na TV e so depois voltou
a outra entrada HDMI. Portanto as observacoes SAO validas: a cor solida
escrita em fb1 nao apareceu sobre a Cultura; apos Stop, o flicker habitual
continuou. Conclusao: usar fb1 desse modo nao resolve a disputa visual.
Os comandos terminaram, `fb1_restored` foi confirmado, e os logs mostram
retorno de bytes TS/PLAYING. Nao repetir teste visual agora que o usuario
esta assistindo a outra entrada HDMI.

Os snapshots de registradores/pilha sao dados independentes dessa observacao.
CI 40613820688 passou C-SKY para tool com pilha. Capturas:
`build-artifacts/snapshot-*.txt`, `thread-snapshot-706.txt` e `-723.txt`.
706 dormia com PC0x00a72134, com retornos em 0x002802e6,
0x0027a3ea e 0x0027a5e4. 723 dormia com PC0x00ab5638,
retorno 0x002f85b4. Varias threads compartilham os mesmos PCs de espera,
logo o PC isolado nao identifica quem desenha a GUI.

HEAD 31ecaf8 amplia ferramenta diagnostica com hold de 0..8000ms
(tempo limitado e detach). CI 40614213873 aprovado (Linux, C-SKY e MIPS) na ultima
consulta. NAO executado na box. Nao testar hold ou SOAP Stop enquanto usuario
nao estiver acompanhando a entrada da box. Aplicativo instalado permanece
4062dc2; firmware nao modificado. A ferramenta anterior de snapshot
ficou em `/tmp/thread_snapshot`; desaparece no reboot.

## Estado apos teste visual confirmado

O usuario confirmou que observou os testes e voltou a outra entrada HDMI
SO DEPOIS. A conclusao visual de fb1 ineficaz e flicker apos Stop e valida.
A ferramenta com hold foi compilada em CI 40614213873, mas ainda nao foi
executada na box. Binario baixado localmente em
build-artifacts/ci-40614213873/thread_snapshot_csky. Testar somente
quando usuario voltar a observar a entrada HDMI da box.

Analise do C: firmware_dlna_start chama firmware_dlna_stop logo no inicio,
antes de iniciar nova ponte HLS e esperar prebuffer de ate12s. Isso cria um
intervalo sem video nas trocas. Em queda, app_recover_failed_channel para
o fluxo para iniciar fallback local; o usuario relata que esse caminho e lento.
Uma solucao precisa eliminar a lacuna sem stream ou impedir o repaint do
firmware sem quebrar DLNA. Ainda nao ha prova de que pausar a thread
prog_sys_proces (805) ou mainloop (706) faca isso; foram identificadas
como candidatas pelos nomes/pilhas e devem passar por teste limitado.
