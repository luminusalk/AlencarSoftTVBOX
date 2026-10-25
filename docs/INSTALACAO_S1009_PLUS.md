# Guia de Instalação Passo a Passo — Azamerica S1009+

Este guia detalha como instalar e configurar o aplicativo **Az IPTV (AlencarSoftTVBOX)** no receptor de satélite **Azamerica S1009+** (processador NationalChip GX6622, C-SKY Linux).

---

## 📋 Pré-requisitos

1. **Receptor Azamerica S1009+** com firmware oficial (ex.: V1.76 ou compatível com XBMC).
2. **Conexão de Rede:**
   - O receptor deve estar conectado à internet (via cabo de rede Ethernet no roteador ou conectado diretamente à porta de rede do notebook com compartilhamento de internet ICS).
3. **Computador (Windows ou Linux)** na mesma rede para o método automático, OU um **pendrive USB formatado em FAT32** para o método manual.

---

## 🚀 Método 1: Instalação Automática via Rede / Telnet (Recomendado)

O Azamerica S1009+ possui um servidor Telnet ativo na porta 23 com login automático como `root`. Podemos enviar o add-on diretamente para a memória flash interna (`/mnt/plugin/`) sem precisar de pendrive.

### Passo 1: Descobrir o IP do Receptor
1. No controle remoto do Azamerica, vá em:
   **Menu** -> **Expansão** / **Configuração de Rede** -> **Configuração IP**.
2. Anote o endereço IP (exemplo: `192.168.137.235` ou `192.168.1.100`).
   *(Se estiver usando cabo direto no notebook, o script `find_direct_receiver.py` localiza o IP automaticamente).*

### Passo 2: Empacotar e Enviar o Aplicativo
No computador, abra o PowerShell ou terminal dentro da pasta `AlencarSoftTVBOX`:

```powershell
# 1. Gerar o pacote ZIP do complemento
python tools/package_plugin.py

# 2. Iniciar servidor temporário de transferência (na pasta de arquivos)
# O receptor baixará o add-on diretamente do seu computador:
python tools/receiver_command.py "mkdir -p /mnt/plugin/plugin.video.az_iptv"
```

Em seguida, basta descompactar os arquivos do diretório `plugin.video.az_iptv` para `/mnt/plugin/plugin.video.az_iptv/` no receptor:

```powershell
# Envia os scripts Python atualizados diretamente para o aparelho:
python tools/receiver_command.py "ls -la /mnt/plugin/plugin.video.az_iptv"
```

A estrutura de arquivos no receptor deve ficar exatamente assim:
```text
/mnt/plugin/plugin.video.az_iptv/
  ├── addon.xml
  ├── default.py
  ├── hls_bridge.py
  ├── plugin_catalog.py
  ├── playlist.py
  ├── app_update.py
  ├── demo.mp4
  ├── sample720.mp4
  ├── catalog_cache/
  └── lists/
```

---

## 💾 Método 2: Instalação via Pendrive USB (Sem PC conectado à rede)

Se o receptor estiver na sala longe do computador, você pode instalar diretamente pelo controle remoto usando um pendrive:

1. **No computador:**
   - Gere o arquivo ZIP executando:
     ```powershell
     python tools/package_plugin.py
     ```
   - O arquivo `plugin.video.az_iptv-0.4.0.zip` será gerado.
   - Copie esse arquivo ZIP para a raiz de um pendrive formatado em **FAT32**.

2. **No Azamerica S1009+:**
   - Conecte o pendrive na porta USB (frontal ou traseira).
   - No controle remoto, acerte:
     **Menu** -> **APP** -> **XBMC**.
   - No XBMC, navegue até:
     **Sistema / Configurações** -> **Complementos (Add-ons)** -> **Instalar a partir de um arquivo ZIP**.
   - Selecione a unidade USB (geralmente identificada como `usb0_1` ou `Disco Removível`).
   - Clique em `plugin.video.az_iptv-0.4.0.zip`.
   - O XBMC exibirá uma notificação: *Complemento ativado*.

---

## 📺 Como Usar e Assistir aos Canais

1. No controle remoto do Azamerica, entre em:
   **Menu** -> **APP** -> selecione o ícone **XBMC**.
2. Dentro do XBMC, vá em **Vídeos** -> **Complementos de Vídeo** (Video Add-ons).
3. Selecione **Az IPTV**.
4. Você verá o menu principal com:
   - **Canais do GitHub (revisão X):** Canais organizados por categoria (Abertos, Esportes, Filmes, Notícias, etc.) puxados diretamente do catálogo oficial assinado.
   - **TV Cultura 540p ao vivo:** Canal de alta velocidade com inicialização direta.
   - **Minhas listas M3U:** Listas locais salvas em `/mnt/plugin/plugin.video.az_iptv/lists/`.
   - **Atualizar catálogo do GitHub:** Força a sincronização imediata dos canais remotos.
5. Selecione qualquer canal e aguarde de 1 a 2 segundos: o streaming ao vivo começará em tela cheia com áudio e vídeo acelerados por hardware.

---

## 🛠️ Como Funciona nos Bastidores (Arquitetura)

O firmware do Azamerica S1009+ tem decodificadores de hardware poderosos para H.264/AAC, mas o player nativo (`gxplayer`) tem limitações com transmissões modernas em formato HLS HTTPS (`.m3u8`).

Para resolver isso de forma invisível:
- O add-on dispara um processo independente em segundo plano (`hls_bridge.py`) usando chamada de sistema `os.fork()`.
- O daemon escuta localmente em `http://127.0.0.1:8978/live.ts?src=...`.
- Ele baixa os blocos de vídeo do canal via HTTPS e alimenta o chip do Azamerica com um fluxo MPEG-TS contínuo e sincronizado via fila (`Queue.Queue`), garantindo reprodução contínua e sem travamentos.

---

## ❓ Resolução de Problemas (FAQ)

### O canal ficou com tela preta ou travou:
- Pressione o botão **EXIT** ou **Voltar** no controle remoto para interromper a reprodução e retornar ao menu.
- Se o aparelho não responder, desligue no botão frontal ou chave traseira e ligue novamente.
- Verifique se o cabo de rede está conectado e com internet ativa.

### O canal fica pausando periodicamente:
- A versão atual do `hls_bridge.py` possui fila assíncrona com pré-buffer de 3 blocos. Certifique-se de que o arquivo `hls_bridge.py` mais recente foi instalado no receptor.

### Como adicionar minhas próprias listas M3U:
- Coloque os arquivos `.m3u` ou `.m3u8` na pasta `/mnt/plugin/plugin.video.az_iptv/lists/` (via Telnet ou pendrive). Eles aparecerão automaticamente na opção **Minhas listas M3U**.
