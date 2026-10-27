"""Install the verified M3U into the C app's writable plugin directory."""

import argparse
from concurrent.futures import ThreadPoolExecutor
from hashlib import md5
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
import re
import socket
import telnetlib
import threading


PROMPT = b"\x1b[m "
DEST = "/mnt/plugin/iptv/iptv.m3u8"
BACKUP = "/mnt/plugin/iptv/iptv.before-verified.m3u8"


def reachable(ip):
    try:
        with socket.create_connection((ip, 23), timeout=0.3):
            return ip
    except OSError:
        return None


def find_receiver():
    addresses = [f"192.168.{net}.{i}" for net in (137, 18) for i in range(2, 255)]
    with ThreadPoolExecutor(max_workers=64) as pool:
        hosts = [ip for ip in pool.map(reachable, addresses) if ip]
    if len(hosts) != 1:
        raise RuntimeError(f"Expected one receiver; found {hosts}")
    return hosts[0]


def run(tn, command, timeout=10):
    tn.write(command.encode("ascii") + b"\n")
    result = tn.read_until(PROMPT, timeout=timeout).decode("latin1", "replace")
    if not result.endswith("\x1b[1;32m[root@GX6622:~]#\x1b[m "):
        raise RuntimeError("Receiver command timed out")
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("playlist", type=Path)
    args = parser.parse_args()
    data = args.playlist.read_bytes()
    count = data.count(b"#EXTINF:")
    if not (1 <= count <= 300):
        raise RuntimeError(f"Invalid channel count: {count}")
    digest = md5(data).hexdigest()
    receiver = find_receiver()
    route = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    route.connect((receiver, 80))
    local_ip = route.getsockname()[0]
    route.close()

    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            if self.path != "/verified.m3u8":
                self.send_error(404)
                return
            self.send_response(200)
            self.send_header("Content-Type", "application/x-mpegURL")
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
            banner = tn.read_until(PROMPT, timeout=6)
            if b"[root@GX6622:" not in banner:
                raise RuntimeError("Unexpected receiver shell")
            run(tn, "mkdir -p /mnt/plugin/iptv /mnt/plugin/lists")
            url = f"http://{local_ip}:{server.server_port}/verified.m3u8"
            run(tn, f"wget -q -T 20 -O {DEST}.new {url}", timeout=30)
            output = run(tn, f"md5sum {DEST}.new")
            match = re.search(r"\b([0-9a-f]{32})\s+" + re.escape(DEST + ".new"), output)
            if not match or match.group(1) != digest:
                raise RuntimeError("Transfer checksum mismatch; active playlist preserved")
            run(tn, f"test -e {BACKUP} || cp {DEST} {BACKUP}")
            run(tn, f"mv {DEST}.new {DEST}")
            run(tn, f"cp {DEST} '/mnt/plugin/lists/Canais ao Vivo.m3u'")
            output = run(tn, f"md5sum {DEST}")
            if digest not in output:
                raise RuntimeError("Installed playlist checksum mismatch")
            print(f"Installed {count} verified channel entries on {receiver}; MD5 {digest}")
    finally:
        server.shutdown()
        server.server_close()


if __name__ == "__main__":
    main()
