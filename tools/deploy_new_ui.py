import os, sys, time, socket, hashlib, threading, subprocess
from http.server import SimpleHTTPRequestHandler, HTTPServer
import telnetlib

sys.path.insert(0, r"D:\EMULADORES\Git\AlencarSoftTVBOX\tools")
import gh_api

runs = gh_api.api_get("actions/runs?per_page=3")
RUN_ID = runs["workflow_runs"][0]["id"]
LOCAL_ARTIFACT = os.path.normpath(r"D:\EMULADORES\Git\AlencarSoftTVBOX\build-artifacts\alencarsoft_tvbox_csky")

# 1. Download artifact
print(f"Baixando artefato do run {RUN_ID}...")
os.chdir(r"D:\EMULADORES\Git\AlencarSoftTVBOX")
res = subprocess.run([sys.executable, "build-artifacts/download_app_artifact.py", str(RUN_ID)], capture_output=True, text=True)
print(res.stdout)
if res.returncode != 0:
    print(res.stderr)
    sys.exit(1)

# 2. Determinar IP do receptor
RECEIVER_IP = None
for candidate in ["192.168.137.15", "192.168.137.237", "192.168.18.23", "192.168.137.77"]:
    try:
        s = socket.create_connection((candidate, 23), timeout=1)
        s.close()
        RECEIVER_IP = candidate
        print(f"Receptor encontrado em {RECEIVER_IP}:23")
        break
    except Exception:
        pass

if not RECEIVER_IP:
    print("Buscando IP do receptor na sub-rede...")
    for i in range(1, 255):
        try:
            s = socket.create_connection((f"192.168.137.{i}", 23), timeout=0.15)
            s.close()
            RECEIVER_IP = f"192.168.137.{i}"
            print(f"Receptor encontrado em {RECEIVER_IP}:23")
            break
        except Exception:
            pass

if not RECEIVER_IP:
    print("ERRO: Receptor nao encontrado na rede!")
    sys.exit(1)

# 3. Servidor HTTP local para deploy
def get_local_ip(target_ip):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect((target_ip, 80))
        return s.getsockname()[0]
    finally:
        s.close()

local_ip = get_local_ip(RECEIVER_IP)
class H(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=os.path.dirname(LOCAL_ARTIFACT), **kwargs)
    def log_message(self, format, *args): pass

server = HTTPServer((local_ip, 0), H)
port = server.server_port
t = threading.Thread(target=server.serve_forever, daemon=True)
t.start()

local_md5 = hashlib.md5(open(LOCAL_ARTIFACT, "rb").read()).hexdigest()
print(f"MD5 local: {local_md5}")

# 4. Deploy via Telnet
print(f"Conectando a {RECEIVER_IP}...")
tn = telnetlib.Telnet(RECEIVER_IP, 23, timeout=5)
prompt = b"\x1b[m "
tn.read_until(prompt, timeout=5)

# Matar processos
tn.write(b"killall -9 alencarsoft_tvbox 2>/dev/null; rm -f /mnt/plugin/alencarsoft_tvbox_new; sleep 1\n")
tn.read_until(prompt, timeout=3)

# Download
dl_cmd = f"wget http://{local_ip}:{port}/alencarsoft_tvbox_csky -O /mnt/plugin/alencarsoft_tvbox_new\n"
tn.write(dl_cmd.encode("ascii"))
tn.read_until(prompt, timeout=10)

# Checar MD5
tn.write(b"md5sum /mnt/plugin/alencarsoft_tvbox_new\n")
md5_out = tn.read_until(prompt, timeout=3).decode("latin1", "ignore")
print("MD5 na box:", md5_out.strip())
if local_md5 not in md5_out:
    print("ERRO: MD5 diverge!")
    server.shutdown()
    sys.exit(1)

# Aplicar
tn.write(b"chmod 755 /mnt/plugin/alencarsoft_tvbox_new && mv /mnt/plugin/alencarsoft_tvbox_new /mnt/plugin/alencarsoft_tvbox && sync\n")
tn.read_until(prompt, timeout=3)
server.shutdown()

# Iniciar o daemon
print("Iniciando SAT daemon com a nova interface...")
tn.write(b"/mnt/plugin/alencarsoft_tvbox --sat-daemon </dev/null > /tmp/sat_daemon.log 2>&1 &\n")
tn.read_until(prompt, timeout=2)
time.sleep(1)

tn.write(b"ps | grep alencarsoft_tvbox | grep -v grep\n")
ps_out = tn.read_until(prompt, timeout=3).decode("latin1", "ignore")
print("Processos:", ps_out.strip())

tn.close()
print("Deploy concluido com sucesso!")
