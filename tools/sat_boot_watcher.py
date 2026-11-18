"""Restore the receiver's SAT daemon after reboot and auto-provision updated channels/launcher."""

import argparse
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
import logging
from pathlib import Path
import re
import socket
import subprocess
import sys
import telnetlib
import threading
import time


PROMPT = b"\x1b[m "
SUBNETS = ("192.168.137", "192.168.18")
ROOT = Path(__file__).resolve().parents[1]


def reachable(ip):
    try:
        with socket.create_connection((ip, 23), timeout=0.3):
            return True
    except OSError:
        return False


def discover(previous=None):
    if previous and reachable(previous):
        return previous
    if reachable("169.254.172.2"):
        return "169.254.172.2"
    candidates = [f"{subnet}.{i}" for subnet in SUBNETS for i in range(2, 255)]
    with ThreadPoolExecutor(max_workers=64) as pool:
        for ip, up in zip(candidates, pool.map(reachable, candidates)):
            if up:
                return ip
    return None


def command(tn, value, timeout=6):
    tn.write(value.encode("ascii") + b"\n")
    return tn.read_until(PROMPT, timeout=timeout).decode("latin1", "replace")


def start_dhcp_background():
    try:
        import receiver_dhcp
        t = threading.Thread(target=receiver_dhcp.serve, daemon=True)
        t.start()
        logging.info("Embedded DHCP server started on 0.0.0.0:67")
    except Exception as exc:
        logging.warning("DHCP background thread notice: %s", exc)


def ensure_daemon(ip, provisioned_ips):
    with telnetlib.Telnet(ip, 23, timeout=5) as tn:
        banner = tn.read_until(PROMPT, timeout=5)
        if b"[root@GX6622:" not in banner:
            raise RuntimeError("Unexpected Telnet prompt")
        years = re.findall(r"(?m)^\s*(20\d{2}|19\d{2})\s*$", command(tn, "date +%Y"))
        if years and int(years[-1]) < 2024:
            utc = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M:%S")
            command(tn, f"date -s '{utc}'")
            logging.info("receiver=%s clock set from notebook UTC", ip)

    if ip not in provisioned_ips:
        try:
            subprocess.run([sys.executable, str(ROOT / "tools" / "install_native_launcher.py")],
                           timeout=45, check=True)
            provisioned_ips.add(ip)
            logging.info("receiver=%s provisioned 274 channels and native XBMC launcher", ip)
        except Exception as exc:
            logging.warning("receiver=%s auto-provision error: %s", ip, exc)

    with telnetlib.Telnet(ip, 23, timeout=5) as tn:
        tn.read_until(PROMPT, timeout=5)
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
    parser.add_argument("--interval", type=int, default=10)
    args = parser.parse_args()
    log = Path.home() / "AppData" / "Local" / "AlencarSoftTVBOX" / "sat_boot_watcher.log"
    log.parent.mkdir(parents=True, exist_ok=True)
    logging.basicConfig(filename=log, level=logging.INFO,
                        format="%(asctime)s %(levelname)s %(message)s")
    start_dhcp_background()
    last_ip = None
    last_state = None
    provisioned_ips = set()
    while True:
        try:
            ip = discover(last_ip)
            state = ensure_daemon(ip, provisioned_ips) if ip else "not-found"
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
