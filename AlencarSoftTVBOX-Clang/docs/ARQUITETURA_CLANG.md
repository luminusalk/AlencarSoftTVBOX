# AlencarSoftTVBOX-Clang: Arquitetura e Manual do Aplicativo Nativo

## 1. Visao Geral
O **AlencarSoftTVBOX-Clang** e um aplicativo IPTV nativo de alta performance escrito em linguagem C pura, projetado para operar diretamente sobre o Linux Framebuffer (`/dev/fb0`) e drivers de hardware de receptores de satelite e TV boxes.

Ele foi concebido para unificar o ecossistema e rodar nos seguintes aparelhos:
- **Azamerica S1009+** (SoC NationalChip GX6622, C-SKY 32-bit, 128 MB NAND Flash, 256-512 MB RAM)
- **Cinebox Power Q** (SoC Availink AVL2569D, RISC 32-bit, 16 MB SPI Flash, 128-256 MB RAM)
- **Duosat Prodigy S** (SoC Availink AVL2569D, RISC 32-bit, 32 MB SPI Flash, 256 MB RAM)
- **Tuning Origins** (SoC Availink AVL2569D, RISC 32-bit, 16-32 MB SPI Flash, 256 MB RAM)

---

## 2. Funcionalidades Principais

### A. Grade de Canais e Categorias (Grade Completa)
- **Navegacao por Categorias:** Abas horizontais superiores com selecao rapida (*Canais Abertos, Esportes, Filmes e Series, Documentarios, Noticias, Infantil*).
- **Lista Vertical de Canais:** Itens formatados com numero, nome do canal e badges visuais de qualidade (`[1080p FULL HD]` e `[720p HD]`).
- **Controle Remoto Intuitivo:**
  - **Setas CIMA / BAIXO:** Navega entre os canais da grade.
  - **Setas ESQUERDA / DIREITA:** Alterna entre as categorias.
  - **Botao OK:** Entra no canal e inicia a reproducao.

### B. Entrar e Sair a Vontade
- **Durante a reproducao:** Pressionar a tecla **VOLTAR (BACK / EXIT)** interrompe a reproducao e retorna suavemente para a grade de canais, sem fechar o aplicativo.
- **Na grade principal:** Pressionar a tecla **VOLTAR (BACK / EXIT)** ou **Q** encerra o aplicativo de forma limpa, liberando o framebuffer e restaurando o menu principal do receptor.

### C. Atualizacao da Grade em Tempo Real
- **Tecla MENU:** Ao pressionar a tecla MENU no controle remoto, o aplicativo faz o download automatico da grade mais recente hospedada no GitHub (`https://raw.githubusercontent.com/luminusalk/Iptv-Brasil-2026/master/...`), salva na flash local e recarrega imediatamente as categorias e canais.

### D. Controle do Visor Frontal (VFD / LED)
- O aplicativo comunica-se diretamente com o driver de painel frontal (`display_vfd.c`):
  - No **Azamerica S1009+**: exibe o nome do canal selecionado em caracteres alfanumericos no visor VFD PT6393.
  - Nos modelos com display de 4 digitos LED (Cinebox, Duosat, Tuning): exibe o numero do canal (`CH 0001`, `0252`).

---

## 3. Estrutura de Diretorios

```text
AlencarSoftTVBOX-Clang/
├── include/
│   ├── app.h               # Maquina de estados da aplicacao
│   ├── config.h            # Constantes e caminhos de arquivos
│   ├── display_vfd.h       # Driver do visor frontal (VFD e LED)
│   ├── font_default.h      # Declaracao da fonte bitmap embutida
│   ├── framebuffer.h       # Motor grafico em Linux Framebuffer (/dev/fb0)
│   ├── input.h             # Leitor de controle remoto infravermelho
│   ├── iptv_catalog.h      # Parser M3U/JSON e gerenciador de grade
│   ├── platform.h          # Deteccao automatica de hardware (SoC e painel)
│   └── player.h            # Interface com decodificador de video por hardware
├── src/
│   ├── display_vfd.c       # Implementacao do controle de visor
│   ├── font.c              # Tabela bitmap 8x16 monocromatica
│   ├── framebuffer.c       # Renderizacao grafica com double-buffering
│   ├── input.c             # Leitura de /dev/gxirr0, /dev/lirc0 e teclado
│   ├── iptv_catalog.c      # Leitura e download de listas M3U
│   ├── main.c              # Loop de eventos, renderizacao da grade e navegacao
│   ├── platform.c          # Identificacao de NationalChip vs Availink
│   └── player.c            # Reproducao de midia acelerada
├── docs/
│   └── ARQUITETURA_CLANG.md
└── Makefile                # Script de compilacao nativa e cross-compilacao
```

---

## 4. Como Compilar

### Compilacao para testes no Host:
```bash
make
./alencarsoft_tvbox
```

### Cross-compilacao para NationalChip GX6622 (Azamerica S1009+):
```bash
make TARGET=csky
```

### Cross-compilacao para Availink AVL2569D (Cinebox, Duosat, Tuning):
```bash
make TARGET=mips
```
