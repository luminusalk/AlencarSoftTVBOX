# Achados Técnicos, Engenharia Reversa e Soluções (AlencarSoft TVBOX)

Este documento é o registro definitivo e exaustivo de todos os achados científicos, diagnósticos de baixo nível, engenharia reversa no kernel e no hardware, falhas encontradas e suas respectivas soluções implementadas no desenvolvimento do **AlencarSoft TVBOX**.

---

## 1. Mapeamento de Hardware e Plataformas Alvo

| Plataforma / Aparelho | SoC / Processador | Arquitetura / ISA | Kernel | Memória / Armazenamento | Periféricos & Drivers |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Azamerica S1009+** | NationalChip GX6622 | C-SKY CK610 (ABIv1 little-endian) | Linux 3.0.8 PREEMPT | 128MB DDR3 RAM / 128MB NAND | `/dev/fb0`, `/dev/ttyS1` (VFD), `/dev/gxirr0`, `/dev/gxav0`, `/dev/gxwdt0` |
| **Cinebox Power Q** | Availink AVL2569D | MIPS32r2 (Big Endian) | Linux 3.10.x | 256MB RAM / Flash SPI | Framebuffer `/dev/fb0`, LED 4 dígitos, IR `/dev/input/event0` |
| **Duosat Prodigy S** | Availink AVL2569D | MIPS32r2 (Big/Little) | Linux 3.10.x | 256MB RAM / Flash SPI | Framebuffer `/dev/fb0`, Painel VFD, IR |
| **Tuning Origins** | Availink AVL2569D | MIPS32r2 (Little Endian)| Linux 3.10.x | 256MB RAM / Flash SPI | Framebuffer `/dev/fb0`, Painel LED, IR |

---

## 2. Linha do Tempo Completa de Erros, Diagnósticos e Soluções

### Falha 1: Corrupção Silenciosa na Flash NAND (`/mnt/plugin`)
* **Sintoma:** Binários transferidos para `/mnt/plugin` falhavam de forma imprevisível com `Segmentation fault` ou `Illegal instruction`, mesmo quando o binário estava correto.
* **Diagnóstico:** Ao calcular o `md5sum` de um executável copiado para `/mnt/plugin`, o hash divergia do original. O sistema de arquivos YAFFS2 no kernel 3.0.8 do GX6622 possui caching assíncrono de blocos NAND que corrompe gravações via rede sem sincronização forçada de blocos.
* **Solução:**
  1. Durante testes rápidos e execução em memória, os binários são alocados diretamente em `/tmp/` (`tmpfs` em RAM), onde a integridade por MD5 é garantida.
  2. Para gravação definitiva na memória Flash, utiliza-se a sequência obrigatória de flush:  
     `cp <origem> /mnt/plugin/<destino> && sync && echo 3 > /proc/sys/vm/drop_caches`.

---

### Falha 2: Rejeição de Execução do Cabeçalho ELF (`syntax error: unexpected "("`)
* **Sintoma:** O shell Linux 3.0.8 retornava `-sh: /mnt/plugin/alencarsoft_tvbox: line 1: syntax error: unexpected "("`.
* **Diagnóstico:** O compilador moderno C-SKY emitia o identificador de máquina `e_machine = 0xFC` (252 decimal - padrão C-SKY moderno registrado após 2017). O kernel Linux 3.0.8 da NationalChip é anterior à padronização moderna e reconhece estritamente `e_machine = 0x27` (39 decimal, `EM_CSKY_LEGACY`).
* **Solução:** Identificado o byte 18 (`0x12`) do cabeçalho ELF32, o qual deve ser obrigatoriamente `0x27` (`EM_CSKY_LEGACY`).

---

### Falha 3: Falha de Carregador Dinâmico (`not found`)
* **Sintoma:** O executável retornava `-sh: ./alencarsoft: not found`, apesar de o arquivo existir fisicamente e possuir permissão `+x`.
* **Diagnóstico:** O cabeçalho ELF continha uma seção `.interp` apontando para `/lib/ld-uClibc.so.0`. Como o firmware de fábrica da Azamerica não disponibiliza interpretador dinâmico em `/lib` (todos os binários de fábrica são estáticos), o kernel não encontrava o loader.
* **Solução:** Configuração estrita de compilação estática (`-static`) em todas as receitas de compilação do Makefile e do GitHub Actions.

---

### Falha 4: O Bug do Offset de Entry Point vs Flags ELF
* **Sintoma:** Após tentar aplicar um patch de cabeçalho ELF no binário, ocorria `Segmentation fault` imediato no ciclo 0 com o Program Counter apontando para `PC: 10002002`.
* **Diagnóstico:** O script de patching executava `f.seek(24)` para gravar as flags do processador (`0x10002002`). Na especificação ELF32:
  - Offset `0x18` (24 decimal) é o `e_entry` (endereço virtual do ponto de entrada da CPU).
  - Offset `0x24` (36 decimal) é o `e_flags` (flags de arquitetura da CPU).
  - O script havia sobrescrito o ponto de entrada da CPU com o número de flags, fazendo a CPU saltar para um endereço de memória não mapeado (`0x10002002`).
* **Solução:** Correção cirúrgica dos offsets:
  - Byte 18..19: `0x27, 0x00` (`e_machine = 39`)
  - Byte 24..27: Preservação intacta do `e_entry` original
  - Byte 36..39: `0x02, 0x20, 0x00, 0x10` (`e_flags = 0x10002002` em little-endian).

---

### Falha 5: Incompatibilidade da Tabela de Syscalls (ABIv1 vs ABIv2 Generic)
* **Sintoma:** Aplicativos compilados com o toolchain T-Head C-SKY (GCC 6.3) travavam em `0x159b8` / `0x10360` ao invocar `sbrk()` / `brk()`.
* **Diagnóstico Revelador:**
  1. No C-SKY moderno (ABIv2), as syscalls seguem o padrão `asm-generic` (`write=64`, `exit=93`, `brk=214`) e passam o número do sistema no registrador `r7`.
  2. No C-SKY clássico (ABIv1) do Linux 3.0.8, o número da chamada de sistema é passado no registrador **`r1`**, e os números são da tabela clássica Linux:
     - `__NR_exit = 1`
     - `__NR_fork = 2`
     - `__NR_read = 3`
     - `__NR_write = 4`
     - `__NR_open = 5`
     - `__NR_close = 6`
     - `__NR_getpid = 20`
     - `__NR_brk = 45`
  3. Quando a libc moderna tentava chamar a syscall 214 (`brk` genérico), o kernel retornava erro `SIGILL` (código 132). A libc tentava tratar o erro chamando `__set_errno()`, que lia o ponteiro TLS via registrador de controle (`mfcr r2, cr<...>`). Como o kernel 3.0.8 não inicializa TLS em user-space, o registrador continha zero, resultando em desreferência de ponteiro nulo (`NULL + 12 = 0x0000000c`) e `Segmentation fault`.
* **Solução Definitiva:**
  - Migração de compilação para o toolchain oficial da própria fabricante do chip: **`csky-linux-tools-uclibc-2.8.01`** (NationalChip GCC 4.5.1 com uClibc nativo para Linux 3.0.8).
  - Esse toolchain gera nativamente o padrão correto de ABIv1 (`r1` para syscalls, `exit=1`, `write=4`, `brk=45`), o cabeçalho ELF `e_machine=0x27` e as flags `0x10002002`, com 100% de estabilidade.

---

### Falha 6: Bloqueio por Terminal de Segundo Plano (`SIGTTOU`)
* **Sintoma:** Ao iniciar o aplicativo em segundo plano via Telnet (`/tmp/alencarsoft_tvbox &`), o processo pausava imediatamente com a mensagem `[1]+ Stopped (tty output)`.
* **Diagnóstico:** O módulo de entrada continha chamadas a `tcgetattr(STDIN_FILENO)` e `tcsetattr(STDIN_FILENO)`. No Linux, quando um processo em background sem terminal controlador tenta manipular atributos de terminal, o kernel envia o sinal `SIGTTOU` para suspender o processo.
* **Solução:**
  1. Remoção de manipulação de `STDIN` em `src/input.c` (leitura direta de `/dev/gxirr0`).
  2. Configuração de `signal(SIGTTOU, SIG_IGN)` e `signal(SIGTTIN, SIG_IGN)` em `src/main.c`.
  3. Redirecionamento padrão: `/tmp/alencarsoft_tvbox < /dev/null > /tmp/alencar_app.log 2>&1 &`.

---

### Falha 7: Bloqueio da FIFO de Áudio no Driver do Kernel (`av.ko`)
* **Sintoma:** Ao executar reprodução com áudio via hardware, o terminal acusava:
  ```text
  module->in_channel[1] is linked to c0707b9c
  [ERROR] gxav_ioctl : 962
  output/linuxdvb.c[990]:LinuxDvbPlay###################error##########
  ```
  O vídeo ficava travado ou mudo.
* **Diagnóstico:**
  1. No boot, o daemon de fábrica `/app/dvbapp` vincula os canais de hardware do demux ao módulo `audio_out` pino 1 (`GXAV_PIN_ESA = 1`, `GXAV_PIN_INPUT = 1`).
  2. Ao finalizar o `dvbapp` com `killall -9 dvbapp`, o encerramento abrupto (SIGKILL) não executa o unlinking dos canais. O driver `av.ko` mantém `module->in_channel[1] = 0xc0707b9c`.
  3. Quando outro reprodutor tenta vincular o decodificador de áudio ao hardware via `ioctl(fd, GXAV_IOCTL_FIFO_LINK, ...)`, o driver detecta a colisão e rejeita com código de erro 962.
* **Solução Implementada:**
  1. Desmontagem da partição `/app` na inicialização (`umount -l /app`), impedindo que o `dvbapp` tome posse do DSP.
  2. Implementação da rotina cirúrgica `gxav_cleanup_audio()` em `src/player.c`:
     Abre `/dev/gxav0` e percorre os módulos 1 a 6 e pinos 0 a 7 executando `ioctl(fd, GXAV_IOCTL_FIFO_UNLINK, &req)` tanto para entrada quanto saída, liberando qualquer canal preso antes de cada reprodução.

---

### Falha 8: A Trava da Flag `-p` no Player de Hardware
* **Sintoma:** O comando `showcartoon` executava o vídeo na TV, mas sem áudio.
* **Diagnóstico:**
  A engenharia reversa do executável `/usr/local/bin/showcartoon` revelou dois modos distintos:
  - `showcartoon -p <arquivo> <fps>`: Inicia apenas o decodificador de vídeo (`v decoder of showcartoon`). A thread de áudio é desabilitada propositalmente pela Azamerica para exibir a animação de boot instantaneamente sem esperar sincronização de áudio.
  - `showcartoon <arquivo> <fps>`: Inicia o decodificador completo de áudio e vídeo (`av decoder of showcartoon`).
* **Solução:**
  Remoção definitiva da flag `-p` em `src/player.c`. O aplicativo chama `/usr/local/bin/showcartoon "<url>" 60`, ativando som estéreo e vídeo simultâneos.

---

### Falha 9: Incompatibilidade de Codecs no DSP de Áudio (MP3 vs AAC)
* **Sintoma:** Arquivos MP4 com áudio AAC travavam a reprodução em tela preta, enquanto arquivos com MP3 tocavam áudio e vídeo perfeitamente.
* **Diagnóstico:**
  Os módulos carregados no DSP de áudio da NationalChip registrados em `dmesg` são:
  - `MPEG: date 2021.04.26 ver 1.3.2.4` (MP1 / MP2 / MP3)
  - `DOBLY: date 2015.10.08 ver 1.1.0.0` (AC3)
  - `SBC date 2020.09.16 ver 1.3.1.1`
  O microcódigo do DSP **não possui decodificador por hardware para AAC**. Ao receber AAC (`acodec = 0xb`), o hardware falha ao sincronizar o clock de áudio. Como o decodificador de vídeo aguarda os timestamps de apresentação de áudio (`audio pts_sync = 1`), o vídeo congela.
* **Solução:**
  Fluxos IPTV e arquivos de mídia devem utilizar áudio em formato **MPEG Audio (MP2/MP3)** ou **AC3 (Dolby Digital)** para usufruir da aceleração total de hardware no SoC.

---

### Falha 10: Limitação de Rede do Decodificador Nativo (`showcartoon`)
* **Sintoma:** Passar URLs diretas `http://...` ou `https://...` para o `showcartoon` causava terminação imediata.
* **Diagnóstico:**
  A biblioteca FFmpeg compilada no firmware de fábrica foi gerada com `--disable-network --disable-protocols`. O player só abre protocolos locais de arquivo (`file://`).
* **Solução:**
  Implementação da ponte nativa **HLS Bridge em C Puro (`src/hls_bridge.c`)**:
  - Uma thread de produção (`hls_producer_thread`) conecta via sockets HTTP, lê a lista `.m3u8` e faz download sequencial dos chunks `.ts`.
  - Os dados são armazenados em um Ring Buffer circular thread-safe em memória RAM.
  - Uma thread de alimentação (`hls_feeder_thread`) grava continuamente o fluxo MPEG-TS em uma FIFO nomeada (`/tmp/live_stream.ts`).
  - O player de hardware lê `/tmp/live_stream.ts` como arquivo local, decodificando o fluxo ao vivo por hardware sem saber que se trata de uma transmissão de rede.

---

### Falha 11: Deadlock ao Sair da Reprodução (Pressionar EXIT)
* **Sintoma:** Ao pressionar **EXIT** durante a reprodução de um canal, o sistema congelava e a grade de canais não voltava à tela.
* **Diagnóstico:**
  Na função `hls_bridge_stop()`, o código alterava `s_bridge_running = 0` e chamava `ring_reset(&s_ring)`. Porém, a função `ring_reset()` emitia apenas o sinal de condição `pthread_cond_broadcast(&rb->can_write)`. A thread `hls_feeder_thread` estava bloqueada aguardando `can_read` dentro de `ring_read()`. Como nunca recebia o broadcast de leitura, a thread permanecia em espera infinita e a chamada `pthread_join(s_feeder_thread, NULL)` travava o loop de eventos principal da aplicação.
* **Solução:**
  Inclusão de `pthread_cond_broadcast(&rb->can_read)` dentro de `ring_reset()` e ao sinalizar parada, liberando instantaneamente a thread de alimentação e retornando para a grade em menos de 100ms.

---

## 3. Descobertas Cruciais de Arquitetura de Vídeo e OSD

### 3.1. Sobreposição de Planos Gráficos
O hardware gráfico do SoC GX6622 organiza a tela em camadas físicas de silício:
1. **Camada Inferior (Video Plane):** Faixa de memória física `0x1d000000` (mapeada no kernel em `0x9d000000`). Gerenciada pelo VPU e pelo módulo `gxav_vpu`.
2. **Camada Superior (OSD Framebuffer):** Controlada por `/dev/fb0` em resolução `1280x720` em cores 32-bit ARGB (`0xAARRGGBB`).

| Estado do Aplicativo | Valor Escrito no Framebuffer (`/dev/fb0`) | Visibilidade na TV |
| :--- | :--- | :--- |
| **Grade de Canais / Menus** | Cor sólida opaca `COLOR_DARK_BG` (`0xFF12131A`) | Interface nativa do AlencarSoft visível; vídeo oculto |
| **Reprodução de Vídeo Ativa** | Transparência total `0x00000000` (Alpha = 0) | OSD 100% invisível; vídeo decodificado pelo VPU exibido em tela cheia |
| **Retorno da Reprodução** | Repintura imediata com `COLOR_DARK_BG` (`0xFF12131A`) | Cobre o último quadro congelado do VPU e reexibe a grade de canais |

---

## 4. Mapeamento Definitivo do Controle Remoto IR (Azamerica S1009+)

O receptor utiliza o protocolo NEC com identificador `RCUTYPE=61` configurado em `/etc/gx.cfg`.  
Cada pacote lido do driver `/dev/gxirr0` possui 4 bytes:
`[Comando] [~Comando (Invertido)] [0xBF (Endereço LSB)] [0xBF (Endereço MSB)]`

| Tecla Física | Código Hex (`data[0]`) | Função no Menu / Grade | Função Durante Reprodução | Função no Modal de Saída |
| :--- | :--- | :--- | :--- | :--- |
| **CIMA (▲)** | `0xD0` | Move cursor para o canal anterior | Sem ação | Sem ação |
| **BAIXO (▼)** | `0x70` | Move cursor para o próximo canal | Sem ação | Sem ação |
| **ESQUERDA (◄)**| `0x88` | Volta para categoria anterior | Sem ação | Seleciona `[SIM]` |
| **DIREITA (►)** | `0x08` | Avança para próxima categoria | Sem ação | Seleciona `[NAO]` |
| **OK** | `0xB0` | Abre e reproduz o canal selecionado | Sem ação | Confirma opção escolhida |
| **EXIT / VOLTAR** | `0x68` | Abre diálogo modal de confirmação de saída | Para o vídeo e volta imediatamente à grade | Fecha o modal e permanece na grade |
| **MENU** | `0x48` | Atualiza o catálogo M3U via internet | Sem ação | Sem ação |

---

## 5. Procedimento de Transição Limpa para o Sistema de Fábrica

Ao confirmar a opção `[SIM]` no diálogo modal de saída:
1. `player_stop()`: Finaliza o `showcartoon` e a ponte HLS em C.
2. `gxav_cleanup_audio()`: Desvincula todas as FIFOs de áudio e vídeo do driver `av.ko`.
3. `fb_clear(&app->fb, 0x00000000)`: Limpa o framebuffer com transparência para a transição gráfica.
4. `mount /dev/mtdblock4 /app -t squashfs`: Remonta a partição de fábrica contendo o sistema original.
5. `/app/dvbapp &`: Executa o software original de satélite em segundo plano.
6. `app->running = 0`: Finaliza o executável do AlencarSoft TVBOX, liberando totalmente a CPU e memória RAM para a TV Box original.
