# AlencarSoft TVBOX

> **Aplicativo Nativo Standalone em C Puro (C99) para Receptores DVB Satélite e TV Boxes Legadas.**  
> Desenvolvido para operar direto no hardware (Bare-Metal Linux), sem depender de Python, XBMC, scripts shell (`.sh`) ou interpretadores externos.

[![Build Status](https://github.com/luminusalk/AlencarSoftTVBOX/actions/workflows/build_tvbox.yml/badge.svg)](https://github.com/luminusalk/AlencarSoftTVBOX/actions)
[![Latest Release](https://img.shields.io/github/v/release/luminusalk/AlencarSoftTVBOX?label=Release%20Nativa)](https://github.com/luminusalk/AlencarSoftTVBOX/releases)
[![C Standard](https://img.shields.io/badge/Language-C99%20%2F%20C%20Puro-blue.svg)]()
[![Platform](https://img.shields.io/badge/Arch-C--SKY%20CK610%20%7C%20MIPS32r2-orange.svg)]()
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

---

## 📺 Visão Geral

O **AlencarSoft TVBOX** é uma solução de engenharia reversa e desenvolvimento embarcado de baixo nível criada para transformar receptores de satélite e TV boxes legadas em terminais modernos de IPTV de altíssima performance.

Aparelhos como **Azamerica S1009+**, **Cinebox Power Q**, **Duosat Prodigy S** e **Tuning Origins** possuem hardware gráfico e decodificação H.264/H.265 por silício dedicada, mas tornaram-se inutilizáveis pelo abandono de suporte de fábrica e falta de aplicativos atualizados.

O **AlencarSoft TVBOX** executa como um **binário estático autônomo** diretamente sobre o kernel Linux:
- Controla diretamente o Framebuffer (`/dev/fb0`) em 720p HD com transparência para a camada de vídeo.
- Gerencia o hardware de áudio e vídeo via ioctl de baixo nível no driver `av.ko`.
- Converte streams IPTV (.m3u8) em tempo real através de uma ponte multithread nativa em C (`/tmp/live_stream.ts` FIFO).
- Responde nativamente ao controle remoto infravermelho (`/dev/gxirr0`) sob protocolo NEC Azamerica (`RCUTYPE=61`).
- Atualiza em tempo real o visor frontal VFD/LED (`/dev/ttyS1`).
- Permite alternância suave e segura entre o IPTV e o software original de fábrica (`dvbapp`).

---

## ⚡ Conquistas Técnicas e O Que Funciona

1. **Reprodução Simultânea de Áudio e Vídeo por Hardware:**
   - Desbloqueio dos canais e pinos de áudio no driver do kernel (`GXAV_IOCTL_FIFO_UNLINK`), eliminando colisões com o firmware de fábrica.
   - Decodificação acelerada de vídeo pelo VPU da NationalChip acoplada ao DSP de áudio integrado.
   - Suporte nativo por hardware a fluxos de áudio **MPEG Audio (MP1/MP2/MP3)** e **Dolby Digital (AC3)** diretamente nas caixas de som da TV.

2. **Ponte HLS Nativa em C Puro (`src/hls_bridge.c`):**
   - Transmissões IPTV HLS (`.m3u8`) baixadas via sockets HTTP nativos e repassadas por Ring Buffer contínuo a uma FIFO (`/tmp/live_stream.ts`).
   - Permite que players embutidos sem suporte a rede decodifiquem transmissões ao vivo como arquivos locais contínuos.

3. **Interface OSD Fluida no Framebuffer (`/dev/fb0`):**
   - Resolução nativa `1280x720` em 32-bit ARGB.
   - Transparência alfa (`0x00000000`) durante a reprodução para exibir a camada física do VPU.
   - Máscara escura instantânea (`COLOR_DARK_BG`) ao voltar para a grade, cobrindo o último quadro congelado.

4. **Navegação do Controle Remoto e Modal de Saída:**
   - **Durante a reprodução:** Pressionar `EXIT` interrompe a mídia instantaneamente e retorna para a grade de canais.
   - **Na grade de canais:** Pressionar `EXIT` abre o diálogo de confirmação:
     `"Deseja sair para o sistema original? [SIM] [NAO]"`.
     - `[SIM]`: Encerra o aplicativo, remonta `/app` e inicia o `/app/dvbapp` original de fábrica.
     - `[NAO]`: Fecha o diálogo e permanece na grade IPTV.

5. **Toolchain Oficial e Compatibilidade Cirúrgica de ISA:**
   - Compilado com o toolchain da fabricante (**NationalChip GCC 4.5.1 uClibc 2.8.01**).
   - Syscalls clássicas ABIv1 em registrador `r1` (`exit=1`, `write=4`, `brk=45`).
   - Identificador ELF `e_machine=0x27` e flags `0x10002002` aceitos nativamente pelo Linux 3.0.8.

---

## 🛠️ Plataformas Suportadas

| Aparelho | Processador / SoC | Arquitetura | Binário Gerado |
| :--- | :--- | :--- | :--- |
| **Azamerica S1009+** | NationalChip GX6622 | C-SKY CK610 (ABIv1) | `alencarsoft_tvbox_csky` |
| **Cinebox Power Q** | Availink AVL2569D | MIPS32r2 (Big Endian) | `alencarsoft_tvbox_mips_be` |
| **Duosat Prodigy S** | Availink AVL2569D | MIPS32r2 (Big / Little) | `alencarsoft_tvbox_mips_be` / `le` |
| **Tuning Origins** | Availink AVL2569D | MIPS32r2 (Little Endian)| `alencarsoft_tvbox_mips_le` |

---

## 📂 Estrutura do Repositório

```text
AlencarSoftTVBOX/
├── .github/workflows/
│   └── build_tvbox.yml           # Pipeline multi-arquitetura (C-SKY + MIPS)
│
├── AlencarSoftTVBOX-Clang/       # Código-fonte do aplicativo nativo em C
│   ├── include/                  # Headers
│   │   ├── app.h                 # Estados, views e contexto global
│   │   ├── framebuffer.h         # Motor gráfico OSD (/dev/fb0)
│   │   ├── font_default.h        # Fonte bitmap 8x16
│   │   ├── hls_bridge.h          # Ponte HLS HTTP -> FIFO
│   │   ├── input.h               # Driver NEC IR (/dev/gxirr0)
│   │   ├── iptv_catalog.h        # Parser M3U e catálogo dinâmico em heap
│   │   ├── display_vfd.h         # Driver do display frontal (/dev/ttyS1)
│   │   ├── platform.h            # Detecção de hardware em runtime
│   │   └── player.h              # Gerenciador de hardware A/V
│   └── src/                      # Implementações C
│       ├── main.c                # Loop de eventos, modal de saída, ciclo de vida
│       ├── framebuffer.c         # Renderizador gráfico
│       ├── font.c                # Glifos bitmap
│       ├── hls_bridge.c          # Ring buffer multithread e streamer FIFO
│       ├── input.c               # Leitor de controle remoto
│       ├── iptv_catalog.c        # Paginação e catálogo em heap
│       ├── display_vfd.c         # Serial VFD/LED
│       ├── platform.c            # Mapeamento do SoC
│       └── player.c              # Controle de aceleração A/V e unlinking de FIFOs
│
├── docs/                         # Documentação Técnica e Engenharia Reversa
│   ├── ACHADOS_E_ENGENHARIA_REVERSA.md # Registro exaustivo de erros, diagnósticos e soluções
│   ├── ARQUITETURA_SISTEMA.md    # Especificação detalhada da arquitetura do sistema
│   └── HANDOFF_RESUMO.md         # Histórico de transição técnica
│
└── tools/                        # Ferramentas de suporte e deployment
    ├── receiver_command.py       # Automação de comandos remotos via Telnet
    └── gh_api.py                 # Integração com GitHub API
```

---

## 🚀 Como Executar no Receptor (Azamerica S1009+)

### 1. Conexão via Telnet
Conecte-se ao receptor pelo terminal Telnet (porta 23, usuário `root` sem senha):
```bash
telnet 192.168.18.23
```

### 2. Baixar e Executar
Para executar a versão nativa mais recente diretamente na memória RAM (`/tmp`):
```bash
# 1. Finaliza instâncias anteriores
killall -9 alencarsoft_tvbox 2>/dev/null
rm -f /tmp/alencarsoft_tvbox

# 2. Faz o download do binário compilado
wget https://github.com/luminusalk/AlencarSoftTVBOX/releases/download/v1.0.8-native/alencarsoft_tvbox_csky -O /tmp/alencarsoft_tvbox

# 3. Concede permissão de execução
chmod +x /tmp/alencarsoft_tvbox

# 4. Inicia o aplicativo na TV
/tmp/alencarsoft_tvbox < /dev/null > /tmp/alencar_app.log 2>&1 &
```

---

## 🎮 Comandos do Controle Remoto

| Tecla do Controle | Na Grade de Canais | Durante a Reprodução | No Diálogo de Saída |
| :--- | :--- | :--- | :--- |
| **▲ / ▼ (Cima / Baixo)** | Navega entre os canais | Sem ação | Sem ação |
| **◄ / ► (Esquerda / Direita)** | Alterna abas de categoria | Sem ação | Alterna entre `[SIM]` e `[NAO]` |
| **OK** | Abre o canal selecionado | Sem ação | Confirma a opção selecionada |
| **EXIT / VOLTAR** | Abre o modal de saída | **Para o vídeo e volta à grade** | Cancela e volta à grade |
| **MENU** | Atualiza grade pelo GitHub | Sem ação | Sem ação |

---

## 📚 Documentação Técnica Aprofundada

Para detalhes completos de engenharia reversa, análise de registradores, desmontagem de firmware e arquitetura:
- 📖 [Achados e Engenharia Reversa](docs/ACHADOS_E_ENGENHARIA_REVERSA.md)
- 🏛️ [Arquitetura Técnica do Sistema](docs/ARQUITETURA_SISTEMA.md)

---

## 📜 Licença

Distribuído sob a licença **MIT**. Consulte o arquivo `LICENSE` para mais detalhes.
