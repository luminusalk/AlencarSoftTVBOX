"""Instala o lancador nativo em /mnt/plugin/plugin.video.az_iptv/default.py e atualiza a lista de canais."""

from concurrent.futures import ThreadPoolExecutor
import socket
import subprocess
import sys
import telnetlib
from pathlib import Path

PROMPT = b"\x1b[m "
ROOT = Path(__file__).resolve().parents[1]
PLAYLIST = ROOT / "AlencarSoftTVBOX-Clang" / "iptv.m3u8"

NATIVE_DEFAULT_PY = r'''# -*- coding: utf-8 -*-
"""Lancador nativo C do AlencarSoft TVBOX (independente de notebook)."""

import os
import sys
import time
import urllib2
from email.utils import parsedate_tz, mktime_tz

import xbmcgui
import xbmcplugin


def sync_clock_if_needed():
    if time.time() >= 1735689600:
        return
    for url in ("http://connectivitycheck.gstatic.com/generate_204",
                "http://www.google.com",
                "http://1.1.1.1"):
        try:
            req = urllib2.Request(url, headers={"User-Agent": "Mozilla/5.0"})
            resp = urllib2.urlopen(req, timeout=3)
            date_hdr = resp.info().getheader("Date")
            if date_hdr:
                ts = int(mktime_tz(parsedate_tz(date_hdr)))
                utc_str = time.strftime("%Y-%m-%d %H:%M:%S", time.gmtime(ts))
                os.system("date -u -s '%s' >/dev/null 2>&1 || date -s '%s' >/dev/null 2>&1" % (utc_str, utc_str))
                return
        except Exception:
            pass
    os.system("date -s '2026-11-18 19:00:00' >/dev/null 2>&1")


def launch_native_c_app():
    script = (
        "#!/bin/sh\n"
        "sleep 1\n"
        "killall -9 alencarsoft_tvbox 2>/dev/null\n"
        "sleep 1\n"
        "/mnt/plugin/alencarsoft_tvbox --headless-osd-ir </dev/null >/tmp/alencar_app.log 2>&1\n"
        "nohup /mnt/plugin/alencarsoft_tvbox --sat-daemon </dev/null >/tmp/alencarsoft_sat.log 2>&1 &\n"
    )
    try:
        with open("/tmp/run_alencarsoft_native.sh", "w") as f:
            f.write(script)
        os.chmod("/tmp/run_alencarsoft_native.sh", 0755)
        os.system("nohup /tmp/run_alencarsoft_native.sh </dev/null >/dev/null 2>&1 &")
    except Exception:
        pass


def main():
    handle = int(sys.argv[1]) if len(sys.argv) > 1 else 0
    sync_clock_if_needed()
    launch_native_c_app()
    item = xbmcgui.ListItem("Iniciando AlencarSoft TVBOX (Tecla SAT Ativa)...")
    xbmcplugin.addDirectoryItem(handle, sys.argv[0], item, False)
    xbmcplugin.endOfDirectory(handle)


if __name__ == "__main__":
    main()
'''


def reachable(ip):
    try:
        with socket.create_connection((ip, 23), timeout=0.3):
            return ip
    except OSError:
        return None


def find_receiver():
    addresses = ["169.254.172.2"] + [f"192.168.{net}.{i}" for net in (137, 18) for i in range(2, 255)]
    with ThreadPoolExecutor(max_workers=64) as pool:
        hosts = [ip for ip in pool.map(reachable, addresses) if ip]
    return hosts[0] if hosts else None


def run(tn, cmd, timeout=10):
    tn.write(cmd.encode("ascii") + b"\n")
    return tn.read_until(PROMPT, timeout=timeout).decode("latin1", "replace")


def main():
    ip = find_receiver()
    if not ip:
        print("ERRO: Receptor nao encontrado em 192.168.137.x ou 192.168.18.x.")
        sys.exit(1)
    print(f"Receptor encontrado em {ip}!")

    # 1. Instalar lista verificada e fallback_particles.ts
    subprocess.run([sys.executable, str(ROOT / "tools" / "install_verified_channels.py"), str(PLAYLIST)], check=True)

    # 2. Instalar lancador nativo em /mnt/plugin/plugin.video.az_iptv/default.py
    with telnetlib.Telnet(ip, 23, timeout=6) as tn:
        tn.read_until(PROMPT, timeout=6)
        run(tn, "mkdir -p /mnt/plugin/plugin.video.az_iptv")
        run(tn, "test -f /mnt/plugin/plugin.video.az_iptv/default.py.orig || "
                "cp /mnt/plugin/plugin.video.az_iptv/default.py /mnt/plugin/plugin.video.az_iptv/default.py.orig 2>/dev/null || true")
        eof_cmd = "cat << 'EOF' > /mnt/plugin/plugin.video.az_iptv/default.py\n" + NATIVE_DEFAULT_PY + "\nEOF"
        run(tn, eof_cmd)
        run(tn, "rm -f /mnt/plugin/plugin.video.az_iptv/default.pyc && sync")
        # Garantir que o --sat-daemon esteja rodando agora
        state = run(tn, "ps | grep '[a]lencarsoft_tvbox'")
        if "--sat-daemon" not in state and "--headless-osd-ir" not in state:
            run(tn, "nohup /mnt/plugin/alencarsoft_tvbox --sat-daemon </dev/null >/tmp/alencarsoft_sat.log 2>&1 &")
        print("Lancador nativo instalado em /mnt/plugin/plugin.video.az_iptv/default.py e daemon SAT ativo!")


if __name__ == "__main__":
    main()
