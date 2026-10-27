import telnetlib, time

tn = telnetlib.Telnet("192.168.137.237", 23, timeout=5)
tn.read_until(b"login: ", timeout=3)
tn.write(b"root\n")
time.sleep(0.5)
tn.read_until(b"# ", timeout=3)

# 1. Script de inicializacao do Daemon SAT
start_sh = """#!/bin/sh
# AlencarSoft TVBOX - Iniciar Daemon SAT
killall -9 alencarsoft_tvbox 2>/dev/null
sleep 1
nohup /mnt/plugin/alencarsoft_tvbox --sat-daemon >/tmp/alencarsoft_sat.log 2>&1 &
echo "[OK] Daemon SAT iniciado em segundo plano."
"""
tn.write(b"cat << 'EOF' > /mnt/plugin/start_sat_daemon.sh\n")
tn.write(start_sh.encode('utf-8'))
tn.write(b"\nEOF\n")
time.sleep(0.5)
tn.write(b"chmod +x /mnt/plugin/start_sat_daemon.sh\n")
time.sleep(0.5)

# 2. Script de parada do Daemon SAT
stop_sh = """#!/bin/sh
# AlencarSoft TVBOX - Parar Daemon SAT
killall -9 alencarsoft_tvbox 2>/dev/null
echo "[OK] Daemon SAT e processos AlencarSoft finalizados."
"""
tn.write(b"cat << 'EOF' > /mnt/plugin/stop_sat_daemon.sh\n")
tn.write(stop_sh.encode('utf-8'))
tn.write(b"\nEOF\n")
time.sleep(0.5)
tn.write(b"chmod +x /mnt/plugin/stop_sat_daemon.sh\n")
time.sleep(0.5)

# 3. README salvo diretamente dentro da box em /mnt/plugin/README_DAEMON_SAT.txt
readme_box = """===============================================================
       ALENCARSOFT TVBOX - GUIA DO DAEMON DA TECLA SAT
===============================================================

1. O QUE E ESTE DAEMON?
-----------------------
O processo /mnt/plugin/alencarsoft_tvbox --sat-daemon roda
em segundo plano consumindo 0% de CPU.
Ele escuta o sensor de infravermelho (/dev/gxirr0).
Ao apertar o botao SAT (codigo 0x6a) no controle do Azamerica,
ele abre instantaneamente a interface IPTV na tela.

2. AO REINICIAR O APLICATIVO:
-----------------------------
Quando voce fecha o aplicativo (com EXIT ou MENU), o app fecha
e o daemon continua vivo na memoria. Ao apertar SAT novamente,
ele reabre o aplicativo na mesma hora.

3. AO REINICIAR O APARELHO (DESLIGAR DA TOMADA):
------------------------------------------------
A memoria RAM e resetada. Para ligar novamente o daemon:
Pelo Telnet (porta 23):
  sh /mnt/plugin/start_sat_daemon.sh

Ou pelo seu computador (1 clique):
  python tools/start_sat_daemon.py

4. COMO REMOVER COMPLETAMENTE (VOLTAR AO PADRAO DE FABRICA):
------------------------------------------------------------
Para remover tudo e deixar a box 100% original:
  killall -9 alencarsoft_tvbox
  rm -f /mnt/plugin/start_sat_daemon.sh
  rm -f /mnt/plugin/stop_sat_daemon.sh
  rm -f /mnt/plugin/start_exit3_daemon.sh
  rm -f /mnt/plugin/exit3_daemon.py
  rm -f /mnt/plugin/README_DAEMON_SAT.txt
  rm -f /tmp/alencarsoft_sat.log
===============================================================
"""
tn.write(b"cat << 'EOF' > /mnt/plugin/README_DAEMON_SAT.txt\n")
tn.write(readme_box.encode('utf-8'))
tn.write(b"\nEOF\n")
time.sleep(0.5)

# Verificar
tn.write(b"ls -la /mnt/plugin/*.sh /mnt/plugin/README_DAEMON_SAT.txt\n")
time.sleep(0.5)
out = tn.read_very_eager().decode('latin1', errors='replace')
print("Arquivos criados na box:\n", out)
tn.close()
