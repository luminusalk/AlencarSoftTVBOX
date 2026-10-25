#!/bin/sh
# ==============================================================================
# AlencarSoft TVBOX - Launcher Nativo Embarcado
# Executa de forma totalmente independente fora do XBMC/Kodi.
# Pausa os daemons de fundo da Azamerica para liberar Framebuffer e Controle Remoto,
# e restaura tudo com seguranca ao sair.
# ==============================================================================

APP_DIR="/mnt/plugin"
APP_BIN="$APP_DIR/alencarsoft_tvbox"

echo "============================================="
echo "   Iniciando AlencarSoft TVBOX Nativo...     "
echo "============================================="

if [ ! -f "$APP_BIN" ]; then
    echo "Erro: Binario $APP_BIN nao encontrado!"
    exit 1
fi

chmod +x "$APP_BIN"

# 1. Pausa o dvbapp para liberar acesso total a /dev/fb0 e /dev/gxirr0
echo "[AlencarSoft] Pausando daemons de TV satelite..."
killall -STOP dvbapp 2>/dev/null

# 2. Executa o aplicativo nativo AlencarSoft TVBOX
$APP_BIN

# 3. Restaura os daemons satelite ao sair do aplicativo
echo "[AlencarSoft] Restaurando interface padrao do receptor..."
killall -CONT dvbapp 2>/dev/null

echo "[AlencarSoft] Finalizado com sucesso."
