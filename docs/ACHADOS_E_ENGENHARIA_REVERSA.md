# Achados Técnicos, Engenharia Reversa e Soluções (AlencarSoftTVBOX)

Este documento registra de forma exaustiva todos os achados científicos, diagnósticos de hardware, investigações de baixo nível (kernel/ISA), falhas enfrentadas e as respectivas soluções definitivas desenvolvidas para o **AlencarSoft TVBOX**.

---

## 1. Mapeamento de Hardware e Plataformas Alvo

| Plataforma / Aparelho | SoC / Processador | Arquitetura / ISA | Kernel | Memória / Armazenamento | Display / Periféricos |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Azamerica S1009+** | NationalChip GX6622 | C-SKY CK610 (ABIv1) | Linux 3.0.8 PREEMPT | 256MB RAM / 128MB NAND | `/dev/fb0`, `/dev/ttyS1` (VFD), `/dev/gxirr0` |
| **Cinebox Power Q** | Availink AVL2569D | MIPS32r2 (Big Endian) | Linux 3.10.x | 256MB RAM / Flash SPI | Framebuffer, Visor Frontal, IR |
| **Duosat Prodigy S** | Availink AVL2569D | MIPS32r2 (Big/Little) | Linux 3.10.x | 256MB RAM / Flash SPI | Framebuffer, Painel VFD, IR |
| **Tuning Origins** | Availink AVL2569D | MIPS32r2 | Linux 3.10.x | 256MB RAM / Flash SPI | Framebuffer, Painel LED, IR |

---

## 2. Linha do Tempo de Falhas e Soluções Definitivas

### Falha 1: Corrupção Silenciosa na Flash NAND (`/mnt/plugin`)
* **Sintoma:** Binários transferidos para `/mnt/plugin` falhavam com `Segmentation fault` ou `Illegal instruction`, mesmo quando compilados corretamente.
* **Diagnóstico:** Ao calcular o `md5sum` de um binário conhecido (`busybox`) copiado para `/mnt/plugin`, o hash diverge do original. O sistema de arquivos YAFFS2 no kernel 3.0.8 do GX6622 possui caching assíncrono de blocos NAND que corrompe gravações via rede sem sincronização forçada de blocos.
* **Solução:**
  1. Durante testes e desenvolvimento, os binários são executados diretamente em `/tmp/` (`tmpfs` em RAM), onde a integridade por MD5 é 100% idêntica.
  2. Para instalação definitiva na flash, deve-se usar sempre `cp <origem> /mnt/plugin/<destino> && sync && echo 3 > /proc/sys/vm/drop_caches`.

---

### Falha 2: Rejeição de Execução do Cabeçalho ELF (`syntax error: unexpected "("`)
* **Sintoma:** O shell Linux 3.0.8 retornava `-sh: /mnt/plugin/alencarsoft_tvbox: line 1: syntax error: unexpected "("`.
* **Diagnóstico:** O compilador moderno C-SKY emitia o identificador de máquina `e_machine = 0xFC` (252 decimal - C-SKY moderno). O kernel Linux 3.0.8 é anterior à padronização moderna e reconhece estritamente `e_machine = 0x27` (39 decimal).
* **Solução:** Identificado o byte 18 (`0x12`) do cabeçalho ELF32, o qual deve ser obrigatoriamente `0x27` (`EM_CSKY_LEGACY`).

---

### Falha 3: Falha de Biblioteca Compartilhada (`not found`)
* **Sintoma:** O executável retornava `-sh: ./alencarsoft: not found`, apesar de o arquivo existir e ter permissão `+x`.
* **Diagnóstico:** O cabeçalho ELF possuía a seção `.interp` apontando para `/lib/ld-uClibc.so.0`. Como o firmware da Azamerica não disponibiliza interpretador dinâmico em `/lib` (todos os binários de fábrica são estáticos), o kernel não encontrava o loader.
* **Solução:** Configuração estrita de compilação estática (`-static`) em todas as receitas de build.

---

### Falha 4: O Bug do Offset de Entry Point vs Flags ELF
* **Sintoma:** Após tentar aplicar um patch de cabeçalho ELF, o binário sofria segfault imediato no primeiro ciclo com `PC: 10002002`.
* **Diagnóstico:** Um script anterior executava `f.seek(24)` para gravar as flags do processador (`0x10002002`). Na estrutura ELF32:
  - Offset `0x18` (24 decimal) é o `e_entry` (endereço virtual do ponto de entrada!).
  - Offset `0x24` (36 decimal) é o `e_flags` (flags de arquitetura da CPU).
  - O script sobrescrevia o ponto de entrada da CPU com o número de flags, fazendo a CPU saltar para um endereço de memória não mapeado.
* **Solução:** Correção cirúrgica dos offsets:
  - Byte 18..19: `0x27, 0x00` (`e_machine = 39`)
  - Byte 24..27: Preservação intacta do `e_entry` original (ex: `0x80e0` / `0x8148`)
  - Byte 36..39: `0x02, 0x20, 0x00, 0x10` (`e_flags = 0x10002002` em little-endian).

---

### Falha 5: Incompatibilidade da Tabela de Syscalls (ABIv1 vs ABIv2 Generic)
* **Sintoma:** O aplicativo compilado com o toolchain T-Head (GCC 6.3) travava em `0x159b8` / `0x10360` ao chamar `sbrk()` / `brk()`.
* **Diagnóstico Revelador:**
  1. No C-SKY moderno (ABIv2), as syscalls seguem o `asm-generic` (`write=64`, `exit=93`, `brk=214`) e passam o número do sistema em `r7`.
  2. No C-SKY clássico (ABIv1) do Linux 3.0.8, o número da chamada de sistema é passado no registrador **`r1`**, e os números são da tabela clássica Linux:
     - `__NR_exit = 1`
     - `__NR_fork = 2`
     - `__NR_read = 3`
     - `__NR_write = 4`
     - `__NR_open = 5`
     - `__NR_close = 6`
     - `__NR_getpid = 20` (testado ao vivo na box, retornou PID 119!)
     - `__NR_brk = 45`
  3. Quando a libc moderna chamava a syscall 214 (`brk` genérico), o kernel retornava erro `SIGILL` (código 132). A libc tentava tratar o erro chamando `__set_errno()`, que lia o ponteiro TLS via registrador de controle (`mfcr r2, cr<...>`). Como o kernel 3.0.8 não inicializa TLS em user-space, o registrador retornava zero, resultando em desreferência de `NULL + 12 = 0x0000000c` e `Segmentation fault`.
* **Solução Definitiva:**
  - Migração de compilação para o toolchain oficial da própria fabricante do chip: **`csky-linux-tools-uclibc-2.8.01`** (NationalChip GCC 4.5.1 com uClibc nativo para Linux 3.0.8).
  - Esse toolchain utiliza nativamente os números de syscall corretos (`exit=1`, `write=4`, `brk=45`), o cabeçalho ELF `e_machine=0x27` e as flags `0x10002002`, dispensando qualquer patch binário!

---

### Falha 6: Bloqueio por Terminal de Segundo Plano (`SIGTTOU`)
* **Sintoma:** Ao iniciar o aplicativo em segundo plano via Telnet (`/tmp/alencarsoft_tvbox &`), o processo pausava imediatamente com `[1]+ Stopped (tty output)`.
* **Diagnóstico:** O módulo de entrada (`src/input.c`) continha chamadas a `tcgetattr(STDIN_FILENO)` e `tcsetattr(STDIN_FILENO)`. No Linux, quando um processo em background sem terminal controlador tenta manipular atributos de terminal, o kernel envia o sinal `SIGTTOU` para suspender o processo. Como o processo ficava pausado e o `dvbapp` havia sido parado com `killall -STOP`, o watchdog de hardware reiniciava a box após ~60 segundos.
* **Solução:**
  1. Remoção de manipulação de `STDIN` em `src/input.c` (o aplicativo em receptor utiliza controle remoto infravermelho nativo).
  2. Configuração de `signal(SIGTTOU, SIG_IGN)` e `signal(SIGTTIN, SIG_IGN)` em `src/main.c`.
  3. Redirecionamento correto na inicialização: `/tmp/alencarsoft_tvbox < /dev/null > /tmp/alencar_app.log 2>&1 &`.

---

## 3. Descobertas Cruciais do Subsistema de Hardware

### 3.1 Framebuffer (`/dev/fb0`)
* **Resolução Nativa:** `1280x720` (720p HD).
* **Formato de Cor:** 32-bit ARGB (`rgba 8/16,8/8,8/0,8/24`).
* **Stride:** 5120 bytes por linha (`1280 * 4`).
* **Memória Mapeada:** ~3.686.400 bytes por tela cheia via `mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0)`.

### 3.2 Visor Frontal VFD (`/dev/ttyS1`)
* **Dispositivo:** UART serial a 9600 bps.
* **Comandos Aceitos:** Envio direto de strings ASCII para exibição no display de 4 a 12 dígitos frontais do receptor (ex: `"ALEN"`, `"CH01"`, `"1080"`).

### 3.3 Controle Remoto Infravermelho (`/dev/gxirr0` e `BaseRemoteCtl`)
* **Drivers Identificados:**
  - `/dev/gxirr0`: Dispositivo de caracteres (`major 10, minor 62`), driver de baixo nível da NationalChip.
  - `/dev/lirc0`: Dispositivo de caracteres (`major 252, minor 0`), mapeado pelo kernel como `BaseRemoteCtl`.
  - Subsistema Linux input: `Name="goxceed remote control"` em `/devices/virtual/input/input0`.
* **Configuração da Box (`/etc/gx.cfg`):**
  - `MODEID=S1009+;`
  - `RCUTYPE=61;` (Identificador do protocolo de controle remoto da Azamerica)
  - `FRANTPANNELTYPE=6;` (Painel frontal de LED/VFD)

### 3.4 Watchdog de Hardware (`/dev/gxwdt0`)
* **Dispositivo:** `/dev/gxwdt0` (NationalChip Hardware Watchdog).
* **Comportamento:** Se o processo `dvbapp` for pausado ou finalizado sem que o watchdog seja alimentado ou desativado, o circuito de supervisão reinicia o aparelho automaticamente em aproximadamente 30 a 60 segundos.
* **Mitigação:** Nosso aplicativo alimenta o watchdog periodicamente em seu loop nativo de eventos (`while (running)`).
