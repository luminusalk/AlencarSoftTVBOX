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
    print("[ERRO] Receptor Azamerica nao encontrado na rede local.")
    sys.exit(1)

s = socket.create_connection((ip, 23), timeout=5)
time.sleep(0.5)
s.recv(4096)
s.sendall(b"root\n")
time.sleep(0.5)
s.recv(4096)

s.sendall(b"/mnt/plugin/stop_sat_daemon.sh\n")
time.sleep(1)
out = s.recv(4096).decode("latin1", errors="replace")
print(out)
s.close()
print("Daemon SAT finalizado!")
