# AlencarSoft TVBOX

> **Aplicativo Nativo Standalone em C (C99) para Receptores DVB Satélite e TV Boxes Legadas.**  
> Desenvolvido em C puro para operar direto sobre o kernel Linux e integrar aceleração audiovisual por hardware.

[![Build Status](https://github.com/luminusalk/AlencarSoftTVBOX/actions/workflows/build_tvbox.yml/badge.svg)](https://github.com/luminusalk/AlencarSoftTVBOX/actions)
[![Language](https://img.shields.io/badge/Language-C99%20%2F%20C%20Puro-blue.svg)]()
[![Platform](https://img.shields.io/badge/Arch-C--SKY%20CK610%20%7C%20MIPS32r2-orange.svg)]()
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

---

## 📌 Contexto e Fonte da Verdade

Toda a documentação técnica, histórico de engenharia reversa e diretrizes de desenvolvimento deste projeto são governados estritamente pelo diretório [`docs/`](docs/):

- 📖 **[Guia Mestre de Continuidade e Handover](docs/GUIA_MESTRE_CONTINUIDADE_E_HANDOVER.md)** — **Fonte da verdade absoluta e contexto principal do projeto.**
- 🔬 **[Achados, Acertos e Erros de Engenharia Reversa](docs/ACHADOS_E_ENGENHARIA_REVERSA.md)** — Registro factual de ensaios, diagnósticos no hardware e correções.
- 🏛️ **[Arquitetura do Sistema](docs/ARQUITETURA_SISTEMA.md)** — Diagramas e fluxo de execução do produto C.
- 📊 **[Estado da Reprodução Nativa C](docs/STATUS_REPRODUCAO_NATIVA_C.md)** — Resumo do status atual de validação audiovisual.
- 📋 **[Plano de Testes](docs/PLANO_DE_TESTES.md)** — Critérios e procedimentos de validação na TV e em CI.
- ⚙️ **[Preparação e Instalação S1009+](docs/INSTALACAO_S1009_PLUS.md)** — Procedimentos de atualização reversível.

---

## 📺 Visão Geral da Arquitetura

O **AlencarSoft TVBOX** executa como um binário estático autônomo sobre o Linux do receptor:

1. **Interface e Framebuffer (`/dev/fb0`):**
   - Renderização gráfica direta em 1280x720 32-bit ARGB com double-buffering.
   - Camada alfa transparente (`0x00000000`) ativada durante a reprodução de vídeo para permitir a passagem direta do plano de vídeo do hardware.
   - Grade de canais com fundo escuro (`COLOR_DARK_BG`) e navegação por abas.

2. **Controle Remoto e Visor Frontal:**
   - Leitura nativa de pacotes NEC em `/dev/gxirr0` (protocolo Azamerica, endereço `0xBFBF`).
   - Visor frontal VFD/LED via serial `/dev/ttyS1` exibindo o nome do canal em tempo real.
   - Diálogo modal de confirmação de saída (`[SIM] [NAO]`) ao pressionar `EXIT` ou `BACK` na grade de canais.

3. **Ponte HLS Nativa em C (`src/hls_bridge.c`):**
   - Download de playlists `.m3u8` e segmentos MPEG-TS via HTTP e HTTPS em C nativo.
   - Suporte a HTTPS com Mbed TLS estático e pacote de certificados CA Mozilla embutido no próprio binário (`.incbin`).
   - Alimentação contínua de fluxo MPEG-TS via ring buffer multithread e FIFO `/tmp/live_stream.ts`.

4. **Backend Audiovisual do Receptor (`src/firmware_dlna.c`):**
   - No Azamerica S1009+ (C-SKY), utiliza o renderizador DLNA/UPnP integrado do `dvbapp` (`AVTransport:1`), preservando o software original.
   - Descoberta local dinâmica de portas através de `/proc/net/tcp` e `/description.xml`.
   - Servidor HTTP loopback servindo o fluxo MPEG-TS para o player do firmware via requisições UPnP SOAP (`SetAVTransportURI`, `Play`, `Stop`).

---

## 🎮 Controles

| Tecla do Controle | Na Grade de Canais | Durante a Reprodução | No Diálogo de Saída |
| :--- | :--- | :--- | :--- |
| **▲ / ▼ (Cima / Baixo)** | Navega entre os canais | Sem ação | Sem ação |
| **◄ / ► (Esquerda / Direita)** | Alterna categorias | Sem ação | Alterna entre `[SIM]` e `[NAO]` |
| **OK** | Abre o canal selecionado | Sem ação | Confirma a opção selecionada |
| **EXIT / VOLTAR** | Abre o diálogo de saída | **Para o vídeo e volta à grade** | Cancela e volta à grade |
| **MENU** | Atualiza grade de canais | Sem ação | Sem ação |

---

## 📜 Licença

Distribuído sob a licença **MIT**. Consulte o arquivo [LICENSE](LICENSE) para mais detalhes.
