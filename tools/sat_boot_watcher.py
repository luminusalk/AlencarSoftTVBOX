"""Restore the receiver's SAT daemon after reboot while this notebook is on.

The S1009+ boot filesystem is read-only and has no /mnt/plugin autorun hook.
This host-side watcher leaves the original firmware untouched.
"""

import argparse
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
import logging
from pathlib import Path
import re
import socket
import telnetlib
import time


PROMPT = b"\x1b[m "
SUBNETS = ("192.168.137", "192.168.18")


def reachable(ip):
    try:
        with socket.create_connection((ip, 23), timeout=0.3):
            return True
    except OSError:
        return False


def discover(previous=None):
    if previous and reachable(previous):
        return previous
    candidates = (f"{subnet}.{i}" for subnet in SUBNETS for i in range(2, 255))
    with ThreadPoolExecutor(max_workers=64) as pool:
        for ip, up in zip((f"{subnet}.{i}" for subnet in SUBNETS for i in range(2, 255)), pool.map(reachable, candidates)):
            if up:
                return ip
    return None


def command(tn, value, timeout=6):
    tn.write(value.encode("ascii") + b"\n")
    return tn.read_until(PROMPT, timeout=timeout).decode("latin1", "replace")


def ensure_daemon(ip):
    with telnetlib.Telnet(ip, 23, timeout=5) as tn:
        banner = tn.read_until(PROMPT, timeout=5)
        if b"[root@GX6622:" not in banner:
            raise RuntimeError("Unexpected Telnet prompt")
        years = re.findall(r"(?m)^\s*(20\d{2}|19\d{2})\s*$", command(tn, "date +%Y"))
        if years and int(years[-1]) < 2024:
            utc = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M:%S")
            command(tn, f"date -s '{utc}'")
            logging.info("receiver=%s clock set from notebook UTC", ip)
        state = command(tn, "ps | grep '[a]lencarsoft_tvbox' | head -n 8")
        if "--sat-daemon" in state:
            return "running"
        if "--headless-osd-ir" in state:
            return "app-running"
        if "dvbapp" not in command(tn, "ps | grep '[d]vbapp' | head -n 2"):
            return "firmware-starting"
        command(tn, "nohup /mnt/plugin/alencarsoft_tvbox --sat-daemon </dev/null >/tmp/alencarsoft_sat.log 2>&1 &")
        time.sleep(1)
        if "--sat-daemon" not in command(tn, "ps | grep '[a]lencarsoft_tvbox' | head -n 8"):
            raise RuntimeError("SAT daemon exited after startup")
        return "started"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--once", action="store_true")
    parser.add_argument("--interval", type=int, default=15)
    args = parser.parse_args()
    log = Path.home() / "AppData" / "Local" / "AlencarSoftTVBOX" / "sat_boot_watcher.log"
    log.parent.mkdir(parents=True, exist_ok=True)
    logging.basicConfig(filename=log, level=logging.INFO,
                        format="%(asctime)s %(levelname)s %(message)s")
    last_ip = None
    last_state = None
    while True:
        try:
            ip = discover(last_ip)
            state = ensure_daemon(ip) if ip else "not-found"
            if (ip, state) != last_state:
                logging.info("receiver=%s state=%s", ip, state)
                print(f"receiver={ip} state={state}", flush=True)
                last_state = (ip, state)
            last_ip = ip
        except Exception as exc:
            logging.exception("SAT watcher failed: %s", exc)
            print(f"error: {exc}", flush=True)
            last_ip = None
        if args.once:
            break
        time.sleep(max(args.interval, 5))


if __name__ == "__main__":
    main()
