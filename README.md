# AlencarSoft TVBOX

> **Aplicativo Nativo Standalone em C Puro para Receptores DVB Satélite e TV Boxes Legadas.**  
> Desenvolvido para rodar direto no hardware (Bare-Metal Linux), sem depender de scripts `.sh`, XBMC, Python ou interpretadores externos.

[![Build Status](https://github.com/luminusalk/AlencarSoftTVBOX/actions/workflows/build_tvbox.yml/badge.svg)](https://github.com/luminusalk/AlencarSoftTVBOX/actions)
[![Latest Release](https://img.shields.io/github/v/release/luminusalk/AlencarSoftTVBOX?label=Release%20Nativa)](https://github.com/luminusalk/AlencarSoftTVBOX/releases)
[![C Standard](https://img.shields.io/badge/Language-C99%20%2F%20C%20Puro-blue.svg)]()
[![Platform](https://img.shields.io/badge/Arch-C--SKY%20CK610%20%7C%20MIPS32r2-orange.svg)]()
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

---

## 📺 Visão Geral

O **AlencarSoft TVBOX** é uma solução de engenharia reversa e desenvolvimento de baixo nível criada para transformar receptores de satélite e TV boxes antigas em terminais modernos de IPTV de alta performance.

Muitos desses aparelhos (como a linha **Azamerica S1009+**, **Cinebox Power Q**, **Duosat Prodigy S** e **Tuning Origins**) possuem processadores com aceleração gráfica e decodificação de vídeo H.264/H.265 por hardware, mas tornaram-se obsoletos pelo abandono de suporte oficial, sistemas fechados e falta de aplicativos modernos.

O **AlencarSoft TVBOX** roda como um **binário estático autônomo** diretamente sobre o kernel Linux do aparelho, assumindo o controle total da tela através do Framebuffer (`/dev/fb0`), gerenciando os daemons de fábrica (`dvbapp`), exibindo informações no visor frontal do aparelho (`/dev/ttyS1`) e respondendo diretamente ao controle remoto infravermelho (`/dev/gxirr0`).

---

## ⚡ Conquistas Técnicas e Inovações

1. **Executável Único e 100% Autônomo:**
   - Zero dependências de bibliotecas dinâmicas externas (`.so`) ou scripts de inicialização (`.sh`).
   - O próprio binário pausa daemons de satélite em segundo plano (`killall -STOP dvbapp`) e os restaura ao fechar (`killall -CONT dvbapp`).

2. **Renderização Direta via Framebuffer (`/dev/fb0`):**
   - Resolução nativa `1280x720` em cores 32-bit ARGB.
   - Motor gráfico próprio em C sem dependências de bibliotecas pesadas (SDL, DirectFB ou FreeType).
   - Fonte bitmap embutida de alto contraste `8x16` para renderização nítida em qualquer televisor HDMI ou AV.

3. **Grade de Canais com Selo de Qualidade Dupla:**
   - 9 canais por página com navegação fluida por categorias (Todos, Abertos, Esportes, Filmes, Séries, Infantis, etc.).
   - Detecção automática de resolução com tags posicionadas após o nome do canal:
     - `[1080p FULL HD]`
     - `[720p HD]`

4. **Gerenciamento Dinâmico de Memória em Heap:**
   - Migração completa de estruturas de dados para ponteiros dinâmicos (`calloc`/`free`), garantindo footprint de memória inferior a 1 MB e evitando estouros de pilha no uClibc.

5. **Compatibilidade Cirúrgica de ISA e Kernel Linux 3.0.8:**
   - Compilação usando o toolchain oficial da fabricante (**NationalChip GCC 4.5.1 uClibc**), alinhando o cabeçalho ELF (`e_machine=0x27`, `e_flags=0x10002002`) e tabela clássica de chamadas de sistema (`__NR_exit=1`, `__NR_write=4`, `__NR_brk=45`).

6. **Controle Remoto e Visor Frontal Integrados:**
   - Leitura direta do sensor infravermelho nativo (`/dev/gxirr0`).
   - Painel frontal VFD/LED atualizado em tempo real via serial UART (`/dev/ttyS1`) exibindo texto de status e canais.

---

## 🛠️ Plataformas Suportadas

| Aparelho | Processador / SoC | Arquitetura | Binário Gerado |
| :--- | :--- | :--- | :--- |
| **Azamerica S1009+** | NationalChip GX6622 | C-SKY CK610 (ABIv1) | `alencarsoft_tvbox_csky` |
| **Cinebox Power Q** | Availink AVL2569D | MIPS32r2 (Big Endian) | `alencarsoft_tvbox_mips_be` |
| **Duosat Prodigy S** | Availink AVL2569D | MIPS32r2 (Big / Little) | `alencarsoft_tvbox_mips_be` / `le` |
| **Tuning Origins** | Availink AVL2569D | MIPS32r2 (Little Endian)| `alencarsoft_tvbox_mips_le` |

---

## 📂 Estrutura do Projeto

```text
AlencarSoftTVBOX/
├── .github/workflows/
│   └── build_tvbox.yml           # Pipeline multi-arquitetura (C-SKY + MIPS)
│
├── AlencarSoftTVBOX-Clang/       # Código-fonte do aplicativo nativo em C
│   ├── include/                  # Cabeçalhos do sistema
│   │   ├── app.h                 # Estruturas centrais e contexto global
│   │   ├── framebuffer.h         # Motor de desenho e Framebuffer /dev/fb0
│   │   ├── font.h                # Tabela de glifos bitmap 8x16
│   │   ├── input.h               # Driver de controle remoto infravermelho
│   │   ├── iptv_catalog.h        # Gerenciador de listas M3U e categorias
│   │   ├── display_vfd.h         # Driver de display frontal (VFD / LED)
│   │   ├── platform.h            # Detecção de hardware em runtime
│   │   └── player.h              # Integração de player e aceleração
│   ├── src/                      # Implementação dos módulos em C
│   │   ├── main.c                # Ponto de entrada, ciclo de vida e eventos
│   │   ├── framebuffer.c         # Renderizador gráfico OSD
│   │   ├── font.c                # Dados da fonte 8x16
│   │   ├── input.c               # Leitor de /dev/gxirr0
│   │   ├── iptv_catalog.c        # Parser M3U e paginação em heap
│   │   ├── display_vfd.c         # Comunicação serial /dev/ttyS1
│   │   ├── platform.c            # Mapeamento do SoC
│   │   └── player.c              # Gerenciador de streaming
│   └── mini_test.c               # Utilitário de diagnóstico de sistema
│
├── docs/                         # Documentação técnica e engenharia reversa
│   ├── ARQUITETURA_SISTEMA.md    # Especificação detalhada da arquitetura
│   ├── ACHADOS_E_ENGENHARIA_REVERSA.md # Registro completo de falhas e soluções
│   └── HANDOFF_RESUMO.md         # Resumos técnicos de transição
│
└── tools/                        # Ferramentas auxiliares de desenvolvimento
    ├── receiver_command.py       # Automação de comandos remotos via Telnet
    ├── deploy_now.py             # Script de implantação rápida na box
    └── gh_api.py                 # Integração autenticada com GitHub API
```

---

## 🚀 Como Executar no Aparelho

### 1. Baixar o Binário Compilado

Você pode baixar os binários prontos diretamente na aba de [Releases do Repositório](https://github.com/luminusalk/AlencarSoftTVBOX/releases).

Para a **Azamerica S1009+**, baixe o arquivo `alencarsoft_tvbox_csky`.

### 2. Transferir e Executar via Telnet

Conecte no receptor via Telnet (porta 23, usuário `root`) e execute:

```bash
# 1. Baixar o executável para a memória RAM (/tmp)
wget https://github.com/luminusalk/AlencarSoftTVBOX/releases/download/v1.0.8-native/alencarsoft_tvbox_csky -O /tmp/alencarsoft_tvbox

# 2. Conceder permissão de execução
chmod +x /tmp/alencarsoft_tvbox

# 3. Iniciar o AlencarSoft TVBOX diretamente na sua TV!
/tmp/alencarsoft_tvbox < /dev/null > /tmp/alencar_app.log 2>&1 &
```

Assim que executado, o visor frontal do aparelho mostrará `ALENCARSOFT`, o sistema pausará a transmissão de satélite e a tela azul com a grade de canais e as tags de alta definição abrirá na sua televisão.

---

## 🎮 Comandos do Controle Remoto

| Tecla do Controle | Função no Aplicativo |
| :--- | :--- |
| **▲ / ▼ (Cima / Baixo)** | Navega entre os canais da página atual |
| **◄ / ► (Esquerda / Direita)** | Alterna entre as abas de categorias ou avança páginas |
| **OK** | Seleciona e reproduz o canal em tela cheia |
| **MENU** | Atualiza a lista de canais via internet |
| **VOL+ / VOL-** | Ajusta o volume de reprodução |
| **EXIT / VOLTAR** | Sai do aplicativo e restaura a interface de fábrica do receptor |

---

## 📜 Licença

Distribuído sob a licença **MIT**. Consulte o arquivo `LICENSE` para mais informações.
