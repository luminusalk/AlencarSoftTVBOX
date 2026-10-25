import argparse
from concurrent.futures import ThreadPoolExecutor
import socket
import telnetlib


parser = argparse.ArgumentParser()
parser.add_argument("command")
args = parser.parse_args()

def accepts_telnet(index):
    address = "192.168.137.%d" % index
    try:
        with socket.create_connection((address, 23), timeout=0.35):
            return address
    except (OSError, TimeoutError):
        return None


with ThreadPoolExecutor(max_workers=48) as pool:
    hosts = [host for host in pool.map(accepts_telnet, range(2, 255)) if host]
if len(hosts) != 1:
    raise RuntimeError("Expected one receiver on the direct cable; found %r" % hosts)

print("Receiver:", hosts[0])
with telnetlib.Telnet(hosts[0], 23, timeout=8) as connection:
    prompt_end = b"\x1b[m "
    banner = connection.read_until(prompt_end, timeout=8)
    if b"[root@GX6622:" not in banner:
        raise RuntimeError(f"No shell prompt: {banner!r}")
    connection.write(args.command.encode("ascii") + b"\n")
    result = connection.read_until(prompt_end, timeout=20)
    print(result.decode("utf-8", errors="replace"))
