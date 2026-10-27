import socket
import sys
import time

CANDIDATE_IPS = ["192.168.137.237", "192.168.18.23", "192.168.137.77"]

def find_box():
    for ip in CANDIDATE_IPS:
        try:
            s = socket.create_connection((ip, 23), timeout=1)
            s.close()
            return ip
        except Exception:
            pass
    return None

ip = find_box()
if not ip:
    print("[ERRO] Receptor Azamerica nao encontrado na rede local (porta 23).")
    print("Verifique se o aparelho esta ligado e conectado no Wi-Fi/Rede.")
    sys.exit(1)

print(f"[OK] Receptor encontrado em {ip}:23")
s = socket.create_connection((ip, 23), timeout=5)
time.sleep(0.5)
s.recv(4096)

s.sendall(b"root\n")
time.sleep(0.5)
s.recv(4096)

# Executa o inicializador permanente na box
s.sendall(b"/mnt/plugin/start_sat_daemon.sh\n")
time.sleep(1.5)
out = s.recv(4096).decode("latin1", errors="replace")
print(out)

# Checa processo
s.sendall(b"ps | grep alencarsoft_tvbox | grep -v grep\n")
time.sleep(0.5)
proc_out = s.recv(4096).decode("latin1", errors="replace")
print("Processos ativos no receptor:\n", proc_out)

s.close()
print("Sucesso! Daemon SAT esta ativo e pronto para uso.")
