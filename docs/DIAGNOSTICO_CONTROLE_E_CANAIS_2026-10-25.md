# Diagnóstico: controle compartilhado e canais

## Situação verificada

O usuário confirmou anteriormente Cultura com imagem e som em C, incluindo
OSD e controle. O controle, porém, também aciona o firmware. Reprodução de
fundo não isola teclas e não constitui solução para o flicker.

Nesta investigação, HEAD era `36c4bf3`, com aplicativo instalado MD5
`b9a909e0135af71d35eaed27d6f0214a`. Foi encontrado em execução como
`--headless-osd-ir`, PID 1095, com stdout/stderr em terminal, sem log salvo.
O firmware era PID 696, key_task 707. PIDs devem ser redescobertos.

Três entradas de notícias retornaram HTTP 200 e bytes iniciais MPEG-TS no
computador. Uma delas redireciona por HTTP 302 para outro servidor. O wget
da própria box também baixou dados desse canal. Isto não comprova reprodução.
A Cultura usa HLS servido pelo HTTP C; os canais HTTP sem extensão são
passados diretamente ao renderer em `firmware_dlna_start()`.

## Testes desta investigação

1. Encerrado somente nosso processo 1095 com TERM e enviado SOAP Stop.
2. Executado GloboNews por 20 segundos com o executável instalado,
   `--headless-fb`. Log: `/tmp/alencar-news-direct.log`.
   **Usuário confirmou imagem e som.** PLAYING sozinho não seria prova de AV.
3. Reaberto aplicativo instalado em modo OSD+IR, log salvo em
   `/tmp/alencar-control.log`, duração 86400 s.
4. Compilado diagnóstico C `tools/ir_guard_probe.c`, commit `59a573b`.
   CI 40257203865: C-SKY e testes Linux existentes passaram; naquele momento
   MIPS ainda estava executando. Isso não testa automaticamente o ptrace.
5. Instalado SOMENTE diagnóstico em `/tmp/ir_guard_probe`, MD5
   `cfb22335adade699e0a797a10286c1a4`. Executável principal não foi substituído.
6. Diagnóstico descobriu key_task, usou PTRACE_ATTACH nessa thread por janela
   curta e depois PTRACE_DETACH. Log confirmou ACTIVE e RELEASED.
   `/proc` confirmou key_task em tracing-stop e thread principal em sleeping.
   Stream C continuou enviando bytes durante a janela.
   A primeira janela não teve teclas. Na repetição solicitada pelo usuário,
   houve navegação e ele confirmou: **só no C, com imagem e som**, e depois
   reforçou que o controle funcionou bem. Isso valida o teste curto de isolamento.

## Integração em andamento

`src/ir_guard.c` encapsula a proteção em processo filho do aplicativo.
O filho mantém somente key_task em ptrace-stop; um pipe e a identidade do pai
limitam a duração à vida do app. A saída normal fecha o pipe; morte abrupta do
pai também libera a thread. O app encerra sua interface se perder a proteção.
Não instala helper externo: tudo é vinculado ao próprio executável.

Teste Linux `tests/ir_guard_test.c` usa firmware simulado com duas threads:
verifica congelamento apenas das teclas, progresso da outra thread e liberação
normal e após SIGKILL do aplicativo. A primeira execução CI detectou retorno
de write ignorado no teste; corrigido antes da instalação. A integração ainda
passou no CI **40258211797** (commit `ca9979b`), incluindo builds C-SKY/MIPS e
todos os testes Linux. Foi instalada com backup preservado, MD5
`1c0bbc188de7d0e4c8f92f242b1a0078`, e aberta em `--interactive`.
Log: `/tmp/alencar-integrated-ir.log`. A proteção foi confirmada ativa em
`/proc`. Falta a observação do usuário para a troca de canais e flicker.

O diagnóstico não escreve memória/registros do firmware, não altera dispositivos
e não envia SIGSTOP ao grupo. Ainda assim, suspender uma thread pode reter
locks: validar ações DLNA, navegação, parada e recuperação antes de integrar.
Não usar PTRACE_O_EXITKILL. O helper tem prazo e handlers para liberação.

## Prioridade e restrições

Usuário autorizou correções e testes, mas pediu deixar Wi-Fi por último.
Priorizar controle exclusivo, flicker, canais e navegação. Não reiniciar ou
matar dvbapp para obter controle; preservar firmware e plugin Python.
Não executar scripts antigos de otimização/deploy sem revisão.

Wi-Fi: adaptador confirmado conectado pelo usuário, porém nesta leitura não
havia interface Wi-Fi de rede e a rota ativa era eth0. `/etc/hosts` continha
somente localhost: os IPs fixos do script não estavam presentes. O arquivo
RT2870STA.dat tinha CountryRegion=1, PktAggregate=0, BeaconLostTime=4 e
PSMode=CAM. Nenhuma configuração de rede foi alterada nesta investigação.

## Continuidade

Obter respostas das duas observações pendentes. Se necessário repetir o teste
de controle com usuário pronto e janela curta. Se o isolamento funcionar,
projetar posse com helper ligado à vida do app e liberação na saída/falha,
validando que nenhuma tecla antiga é reproduzida ao liberar.
Se falhar, não declarar o problema resolvido nem aplicar patch de kernel.

Separar investigação dos canais: comparar URL original com redirecionamento
resolvido e/ou transporte local controlado, sem mudar a ponte HLS funcional.
Não registrar credenciais das URLs em novos logs ou relatórios.
