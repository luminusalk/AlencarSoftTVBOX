import os
import time
import threading
from http.server import SimpleHTTPRequestHandler, HTTPServer
import telnetlib

ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
BIN_DIR = os.path.join(ROOT_DIR, "bin")
TARGET_IP = "192.168.18.23"
LOCAL_IP = "192.168.18.3"

server = HTTPServer(('0.0.0.0', 8765), lambda *args, **kwargs: SimpleHTTPRequestHandler(*args, directory=BIN_DIR, **kwargs))
threading.Thread(target=server.serve_forever, daemon=True).start()

tn = telnetlib.Telnet(TARGET_IP, 23, timeout=5)
time.sleep(1)

def run(c, w=1.0):
    print(">>>", c)
    tn.write(c.encode() + b"\r\n")
    time.sleep(w)
    out = tn.read_very_eager().decode('latin1', errors='replace')
    print(out)
    return out

print("--- 1. TRANSFERINDO BINARIO V1.0.8 CORRIGIDO ---")
run(f"wget http://{LOCAL_IP}:8765/alencarsoft_tvbox_csky -O /tmp/alencarsoft_tvbox", w=2.5)
run("chmod +x /tmp/alencarsoft_tvbox")
run("ls -lh /tmp/alencarsoft_tvbox")
run("rm -f /tmp/alencar_status.txt /tmp/alencar_app.log")

print("--- 2. EXECUTANDO ALENCARSOFT TVBOX NA TV ---")
run("/tmp/alencarsoft_tvbox < /dev/null > /tmp/alencar_app.log 2>&1 &", w=2.0)
run("cat /tmp/alencar_status.txt")
run("cat /tmp/alencar_app.log")
run("ps | grep alencarsoft")

server.shutdown()
tn.close()
print("--- CONCLUIDO! ---")
