# AlencarSoftTVBOX

> Sistema inteligente e modular de IPTV para Set-Top Boxes, receptores DVB legados (Azamerica S1009+) e TV Boxes antigas.

[![Python](https://img.shields.io/badge/Python-2.7%20%7C%203.x-blue.svg)](https://www.python.org/)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Status](https://img.shields.io/badge/Status-Funcional%20em%20Produção-brightgreen.svg)]()

---

## 📺 Visão Geral

O **AlencarSoftTVBOX** nasceu com a missão de dar vida nova a aparelhos de TV Box e receptores de satélite antigos que possuem hardware capaz de decodificar vídeo H.264/H.265 em tempo real via hardware, mas que foram abandonados pelos fabricantes ou sofrem com sistemas fechados e sem suporte a protocolos modernos de streaming (HLS, HTTPS, m3u8).

A primeira plataforma suportada e homologada em produção é o receptor **Azamerica S1009+** (SoC NationalChip GX6622, C-SKY Linux), integrando diretamente ao seu subsistema XBMC nativo.

---

## ⚡ Conquistas e Destaques Técnicos

1. **Reprodução Contínua com Decodificação por Hardware:**
   - Suporte a vídeo H.264 (até 1080p/720p/540p) e áudio AAC em tempo real.
   - Canais ao vivo tocam diretamente no player nativo do aparelho com aceleração gráfica.

2. **HLS Bridge Daemon em Background (`os.fork`):**
   - **Desafio encontrado:** O player nativo do firmware (`gxplayer`) falha ao abrir transmissões HLS remotas em HTTPS (`.m3u8` com query string e chunks relativos). Além disso, o binário principal (`/app/dvbapp`) congela a GIL (Global Interpreter Lock) do Python embutido assim que os menus do XBMC fecham, impedindo o uso de threads comuns.
   - **Solução inovadora:** O `default.py` inicializa um daemon desacoplado em segundo plano usando chamadas diretas de sistema POSIX (`os.fork()`, `os.setsid()`). Esse daemon atua como proxy transparente em `http://127.0.0.1:8978/live.ts?src=<url_encoded>`, baixando os segmentos HLS via HTTPS (`urllib2` com TLS) e entregando MPEG-TS puro alinhado no byte de sincronismo `0x47`.

3. **Buffer Assíncrono com Producer/Consumer (Fim dos Engasgos):**
   - O daemon utiliza uma fila thread-safe (`Queue.Queue`) com pré-carregamento de segmentos.
   - Enquanto o player consome dados em velocidade de exibição, uma thread produtora baixa antecipadamente os próximos blocos do streaming, eliminando completamente os travamentos periódicos causados por variações de latência de rede.

4. **Catálogo Remoto Seguro e Assinado (RSA-2048 + SHA-256):**
   - Atualização automática de canais via GitHub sem depender de alteração no firmware.
   - Verificação criptográfica completa escrita em Python puro (compatível com Python 2.7 padrão, sem bibliotecas externas).
   - Preservação total de listas M3U locais do usuário.

---

## 📂 Estrutura do Repositório

```text
AlencarSoftTVBOX/
├── README.md                      # Documentação principal
├── .gitignore                     # Filtros de arquivos temporários e binários
│
├── plugin.video.az_iptv/          # Add-on XBMC para o receptor
│   ├── addon.xml                  # Manifesto do complemento XBMC
│   ├── default.py                 # Menu principal, navegação e launcher
│   ├── hls_bridge.py              # Daemon HLS->MPEG-TS com queue assíncrona
│   ├── plugin_catalog.py          # Gerenciamento de catálogo e validação RSA
│   ├── playlist.py                # Leitor e paginador de listas M3U locais
│   ├── app_update.py              # Mecanismo de auto-atualização do add-on
│   ├── catalog_cache/             # Cache local de catálogo assinado
│   └── lists/                     # Listas M3U locais do usuário
│
├── tools/                         # Ferramentas de desenvolvimento e deploy
│   ├── receiver_command.py        # Automação remota via Telnet (porta 23)
│   ├── find_direct_receiver.py    # Scanner automático de IP do receptor
│   ├── package_plugin.py          # Gerador de ZIP para distribuição
│   ├── create_catalog_release.py  # Assinador de catálogo RSA
│   ├── create_app_code_bundle.py  # Empacotador de código seguro
│   └── configure_ics.ps1          # Automação de compartilhamento de rede (Windows ICS)
│
├── tests/                         # Testes unitários automatizados
│   ├── test_hls_bridge.py         # Testes de parser HLS e sequenciamento
│   ├── test_catalog.py            # Testes de validação criptográfica RSA
│   ├── test_playlist.py           # Testes de parsing e paginação M3U
│   └── test_app_update.py         # Testes de verificação de pacotes e hash
│
└── docs/                          # Registros técnicos e engenharia reversa
    ├── HANDOFF_RESUMO.md          # Diagnóstico detalhado da GIL e áudio/vídeo
    ├── design.md                  # Especificação original do probe
    ├── plan.md                    # Plano de testes e validação
    └── native_iptv_design.md      # Planejamento do aplicativo standalone
```

---

## 🚀 Como Executar e Testar

### 1. Testes Unitários no Computador

Execute os testes com Python 2.7 ou Python 3:

```bash
python -m unittest discover tests
```

### 2. Deploy Remoto para o Receptor

Com o notebook conectado diretamente ao receptor via cabo de rede:

```bash
# Executar comando ou script no Azamerica via Telnet:
python tools/receiver_command.py "ls -la /mnt/plugin/plugin.video.az_iptv"

# Empacotar e gerar ZIP do add-on:
python tools/package_plugin.py
```

### 3. Utilização na TV

1. Ligue o Azamerica e acesse o menu **APP** -> **XBMC**.
2. Abra o **Az IPTV**.
3. Selecione o canal desejado (ex.: **TV Cultura 540p ao vivo** ou canais do catálogo remoto do GitHub).
4. O streaming se iniciará de forma contínua com imagem e som sincronizados.

---

## 🗺️ Roadmap

- [x] Validação de decodificação de vídeo e áudio nativo (H.264 + AAC MPEG-TS).
- [x] Bridge HLS para MPEG-TS local via daemon independente (`os.fork`).
- [x] Pré-bufferização assíncrona por fila (fim dos travamentos de 15s).
- [x] Catálogo criptografado e atualizável remotamente via GitHub.
- [ ] Interface visual personalizada (tema próprio para o add-on).
- [ ] Aplicativo nativo standalone sem depender da camada XBMC.
- [ ] Porte e criação de imagem/firmware para TV Boxes baseadas em Amlogic e Allwinner.

---

## 👤 Autor

Desenvolvido por **Pedro Alencar** ([@luminusalk](https://github.com/luminusalk)).
