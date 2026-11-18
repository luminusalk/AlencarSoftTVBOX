"""Small DHCP fallback for the S1009+ on the notebook's Ethernet link.

Works both when Windows ICS is active (192.168.137.1 -> leases 192.168.137.2/24)
and when Ethernet is in link-local APIPA mode (169.254.x.y -> leases 169.254.172.2/16)
so Telnet/PuTTY and local HTTP transfers work even without UAC elevation.
"""

import argparse
import ipaddress
import socket
import struct
import subprocess
import re


BOX_MAC = bytes.fromhex("060d447ca63b")
DNS = ipaddress.IPv4Address("1.1.1.1").packed
COOKIE = b"c\x82Sc"


def detect_ethernet_config():
    """Return (server_ip_bytes, lease_ip_bytes, mask_bytes, broadcast_str, lease_str)."""
    try:
        out = subprocess.check_output(["ipconfig"], universal_newlines=True, errors="ignore")
        if "192.168.137.1" in out:
            return (
                ipaddress.IPv4Address("192.168.137.1").packed,
                ipaddress.IPv4Address("192.168.137.2").packed,
                ipaddress.IPv4Address("255.255.255.0").packed,
                "192.168.137.255",
                "192.168.137.2",
            )
        m = re.search(r"169\.254\.\d+\.\d+", out)
        if m:
            srv = m.group(0)
            return (
                ipaddress.IPv4Address(srv).packed,
                ipaddress.IPv4Address("169.254.172.2").packed,
                ipaddress.IPv4Address("255.255.0.0").packed,
                "169.254.255.255",
                "169.254.172.2",
            )
    except Exception:
        pass
    return (
        ipaddress.IPv4Address("192.168.137.1").packed,
        ipaddress.IPv4Address("192.168.137.2").packed,
        ipaddress.IPv4Address("255.255.255.0").packed,
        "192.168.137.255",
        "192.168.137.2",
    )


def options(packet):
    if len(packet) < 240 or packet[236:240] != COOKIE:
        return {}
    result = {}
    pos = 240
    while pos < len(packet):
        tag = packet[pos]
        pos += 1
        if tag == 255:
            break
        if tag == 0:
            continue
        if pos >= len(packet) or pos + 1 + packet[pos] > len(packet):
            return {}
        size = packet[pos]
        pos += 1
        result[tag] = packet[pos:pos + size]
        pos += size
    return result


def request_kind(packet):
    if len(packet) < 240 or packet[0:3] != b"\x01\x01\x06":
        return None
    if packet[28:34] != BOX_MAC:
        return None
    kind = options(packet).get(53)
    if kind and len(kind) == 1 and kind[0] in (1, 3):
        return kind[0]
    return None


def build_reply(request):
    kind = request_kind(request)
    if kind is None:
        return None, None, None
    server_ip, lease_ip, mask, bcast_str, lease_str = detect_ethernet_config()
    packet = bytearray(request[:236])
    packet[0] = 2
    packet[12:16] = b"\x00\x00\x00\x00"
    packet[16:20] = lease_ip
    packet[20:24] = server_ip
    packet[24:28] = b"\x00\x00\x00\x00"
    packet += COOKIE
    packet += b"5\x01" + bytes([2 if kind == 1 else 5])
    packet += b"6\x04" + server_ip
    packet += b"\x01\x04" + mask
    packet += b"\x03\x04" + server_ip
    packet += b"\x06\x04" + DNS
    packet += b"\x1c\x04" + ipaddress.IPv4Address(bcast_str).packed
    packet += b"3\x04" + struct.pack("!I", 86400)
    packet += b":\x04" + struct.pack("!I", 43200)
    packet += b";\x04" + struct.pack("!I", 75600)
    packet += b"\xff"
    return bytes(packet).ljust(300, b"\x00"), bcast_str, lease_str


def serve():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as server:
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        server.bind(("0.0.0.0", 67))
        print("DHCP da box ativo em 0.0.0.0:67 via Ethernet", flush=True)
        while True:
            request, _ = server.recvfrom(2048)
            kind = request_kind(request)
            if kind is None:
                continue
            reply, bcast_str, lease_str = build_reply(request)
            for target_bcast in (bcast_str, "255.255.255.255"):
                try:
                    server.sendto(reply, (target_bcast, 68))
                except OSError:
                    pass
            print(f"DHCP {'OFFER' if kind == 1 else 'ACK'} ({lease_str}) enviado para a box (xid={request[4:8].hex()})",
                  flush=True)


if __name__ == "__main__":
    argparse.ArgumentParser(description=__doc__).parse_args()
    serve()
