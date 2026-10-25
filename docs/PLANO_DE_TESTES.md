# Plano de Testes e Validação — Azamerica S1009+

## 🎯 Objetivo
Garantir o funcionamento contínuo, confiável e com alta qualidade de vídeo/áudio do add-on **Az IPTV** no receptor **Azamerica S1009+**, mantendo a integridade da memória interna do aparelho e permitindo atualizações automáticas via GitHub.

---

## 🧪 Matriz de Testes

### 1. Testes de Decodificação de Hardware Local
- [x] **Vídeo Demo Local (MP4 H.264/AAC):** Validar se o player abre arquivos MP4 padrão com áudio estéreo.
- [x] **Amostra 720p MPEG-TS Local:** Validar se o decodificador de hardware GX6622 renderiza 1280x720 sem perda de frames.
- [x] **Áudio AAC e MP3:** Confirmar sincronismo labial entre áudio e vídeo por mais de 5 minutos contínuos.

### 2. Testes de Rede e Telnet
- [x] **Conexão Direta PC-Receptor (ICS):** Descoberta automática de IP no range `192.168.137.x`.
- [x] **Acesso Telnet (porta 23):** Execução remota não-interativa de comandos de verificação de partição e processos.
- [x] **Espaço em Disco em `/mnt/plugin`:** Monitoramento para manter uso abaixo de 20 MB livres.

### 3. Testes do Daemon HLS Bridge (`hls_bridge.py`)
- [x] **Inicialização via `os.fork`:** Confirmar que o processo filho continua em execução após o menu fechar (`ps | grep dvbapp`).
- [x] **Abertura do Endpoint Local (`127.0.0.1:8978/live.ts`):** Validar headers HTTP 200 e `Content-Type: video/MP2T`.
- [x] **Fila Assíncrona e Pré-buffer:** Monitorar ausência de pausas periódicas (estabilidade após 15s de reprodução).
- [x] **Encerramento Limpo:** Garantir que quando o player fechar, o processo filho libere os sockets sem travar o receptor.

### 4. Testes de Catálogo Criptográfico Remoto (`plugin_catalog.py`)
- [x] **Assinatura RSA-2048 / SHA-256:** Verificação de manifestos válidos e rejeição de assinaturas adulteradas.
- [x] **Download Seguro via HTTPS:** Obtenção de `catalog.json` do GitHub sem travar a thread de interface.
- [x] **Preservação de Listas Locais:** Garantir que nenhuma lista colocada pelo usuário em `lists/` seja apagada na atualização do catálogo.

---

## 🔄 Procedimento de Rollback de Segurança
Caso seja necessário reverter qualquer alteração no aparelho:
1. Remover a pasta `/mnt/plugin/plugin.video.az_iptv`.
2. Encerrar qualquer processo daemon remanescente (`killall` no processo forked).
3. O firmware original e todas as configurações de canais de satélite permanecem 100% intactos.
