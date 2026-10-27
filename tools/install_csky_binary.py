"""Install a verified C-SKY build in the receiver's own plugin directory."""

import argparse
from hashlib import md5
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
import re
import socket
import telnetlib
import threading

from install_verified_channels import find_receiver, run


DEST = "/mnt/plugin/alencarsoft_tvbox"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("binary", type=Path)
    args = parser.parse_args()
    data = args.binary.read_bytes()
    if not data.startswith(b"\x7fELF") or len(data) < 100_000:
        raise RuntimeError("File is not a plausible receiver ELF binary")
    digest = md5(data).hexdigest()
    receiver = find_receiver()
    route = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    route.connect((receiver, 80))
    local_ip = route.getsockname()[0]
    route.close()

    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            if self.path != "/alencarsoft_tvbox_csky":
                self.send_error(404)
                return
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)

        def log_message(self, *_):
            pass

    server = HTTPServer((local_ip, 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        with telnetlib.Telnet(receiver, 23, timeout=6) as tn:
            banner = tn.read_until(b"\x1b[m ", timeout=6)
            if b"[root@GX6622:" not in banner:
                raise RuntimeError("Unexpected receiver shell")
            url = f"http://{local_ip}:{server.server_port}/alencarsoft_tvbox_csky"
            run(tn, f"wget -q -T 30 -O {DEST}.new {url}", timeout=45)
            output = run(tn, f"md5sum {DEST}.new")
            if not re.search(rf"\b{digest}\b", output):
                raise RuntimeError("Transfer checksum mismatch; old binary preserved")
            before = run(tn, f"md5sum {DEST}")
            old = re.search(r"\b([0-9a-f]{32})\s+" + re.escape(DEST), before)
            if old and old.group(1) != digest:
                run(tn, f"cp {DEST} {DEST}.before-fallback")
            run(tn, f"chmod 755 {DEST}.new && mv {DEST}.new {DEST} && sync")
            installed = run(tn, f"md5sum {DEST}")
            if digest not in installed:
                raise RuntimeError("Installed binary checksum mismatch")
            print(f"Installed C-SKY build on {receiver}; MD5 {digest}")
    finally:
        server.shutdown()
        server.server_close()


if __name__ == "__main__":
    main()
