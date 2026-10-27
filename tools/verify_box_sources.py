"""Spot-check catalog URLs through the receiver's C source probe."""

from concurrent.futures import ThreadPoolExecutor
from hashlib import sha256
import argparse
from pathlib import Path
import re
import shlex
import socket
import telnetlib


PROMPT = b"\x1b[m "
GROUP = re.compile(r'group-title="([^"]*)"')
M3U = Path(__file__).resolve().parents[1] / "AlencarSoftTVBOX-Clang/iptv.m3u8"


def discover():
    targets = [f"192.168.{n}.{i}" for n in (137, 18) for i in range(2, 255)]

    def check(ip):
        try:
            with socket.create_connection((ip, 23), timeout=0.3):
                return ip
        except OSError:
            return None

    with ThreadPoolExecutor(max_workers=64) as pool:
        found = [ip for ip in pool.map(check, targets) if ip]
    if len(found) != 1:
        raise RuntimeError(f"Expected one receiver; found {found}")
    return found[0]


def samples():
    groups = {}
    metadata = ""
    for line in M3U.read_text(encoding="utf-8").splitlines():
        if line.startswith("#EXTINF:"):
            metadata = line
        elif line.startswith(("http://", "https://")):
            match = GROUP.search(metadata)
            if match:
                category = match.group(1)
                groups.setdefault(category, []).append((metadata.rsplit(",", 1)[-1], line))
            metadata = ""
    chosen = []
    for category, entries in groups.items():
        chosen.extend((category, name, url) for name, url in entries[:2])
        fhd = next(((category, name, url) for name, url in entries
                    if re.search(r"1080p|full hd|fhd", name, re.I)), None)
        if fhd and fhd not in chosen:
            chosen.append(fhd)
    return chosen


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--name", help="Probe matching channel name from the entire list")
    args = parser.parse_args()
    ip = discover()
    failures = []
    if args.name:
        metadata = ""
        chosen = []
        for line in M3U.read_text(encoding="utf-8").splitlines():
            if line.startswith("#EXTINF:"):
                metadata = line
            elif line.startswith(("http://", "https://")):
                name = metadata.rsplit(",", 1)[-1]
                match = GROUP.search(metadata)
                if match and args.name.lower() in name.lower():
                    chosen.append((match.group(1), name, line))
                metadata = ""
    else:
        chosen = samples()
    with telnetlib.Telnet(ip, 23, timeout=6) as tn:
        if b"[root@GX6622:" not in tn.read_until(PROMPT, timeout=6):
            raise RuntimeError("Unexpected shell")
        for category, name, url in chosen:
            command = ("timeout -t 12 /mnt/plugin/alencarsoft_tvbox --probe-source "
                       + shlex.quote(url) + "; printf '\\n__PROBE_RC=%d__\\n' $?")
            tn.write(command.encode("ascii") + b"\n")
            output = tn.read_until(PROMPT, timeout=18).decode("latin1", "replace")
            match = re.search(r"__PROBE_RC=(\d+)__", output)
            working = bool(match and match.group(1) == "0")
            print(f"{category}: {name}: {'OK' if working else 'FAIL'}", flush=True)
            if not working:
                failures.append(sha256(url.encode("utf-8")).hexdigest())
    output = Path(__file__).resolve().parents[1] / "build-artifacts/receiver_failed_url_sha256.txt"
    previous = set(output.read_text(encoding="ascii").splitlines()) if output.exists() else set()
    output.write_text("\n".join(sorted(previous | set(failures))) + "\n", encoding="ascii")
    print(f"Failed receiver probes recorded as {len(failures)} URL hashes", flush=True)


if __name__ == "__main__":
    main()
