"""Small DHCP fallback for the S1009+ on the notebook's ICS Ethernet link.

Only the receiver's known MAC is answered. Internet sharing/NAT remains the
Windows ICS service; this process provides only the missing DHCP lease.
"""

import argparse
import ipaddress
import socket
import struct


BOX_MAC = bytes.fromhex("060d447ca63b")
SERVER_IP = ipaddress.IPv4Address("192.168.137.1").packed
LEASE_IP = ipaddress.IPv4Address("192.168.137.2").packed
MASK = ipaddress.IPv4Address("255.255.255.0").packed
DNS = ipaddress.IPv4Address("1.1.1.1").packed
COOKIE = b"c\x82Sc"


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
        return None
    packet = bytearray(request[:236])
    packet[0] = 2
    packet[12:16] = b"\x00\x00\x00\x00"
    packet[16:20] = LEASE_IP
    packet[20:24] = SERVER_IP
    packet[24:28] = b"\x00\x00\x00\x00"
    packet += COOKIE
    packet += b"5\x01" + bytes([2 if kind == 1 else 5])
    packet += b"6\x04" + SERVER_IP
    packet += b"\x01\x04" + MASK
    packet += b"\x03\x04" + SERVER_IP
    packet += b"\x06\x04" + DNS
    packet += b"\x1c\x04" + ipaddress.IPv4Address("192.168.137.255").packed
    packet += b"3\x04" + struct.pack("!I", 86400)
    packet += b":\x04" + struct.pack("!I", 43200)
    packet += b";\x04" + struct.pack("!I", 75600)
    packet += b"\xff"
    return bytes(packet).ljust(300, b"\x00")


def serve():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as server:
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        server.bind(("0.0.0.0", 67))
        print("DHCP da box ativo: 192.168.137.2 via Ethernet", flush=True)
        while True:
            request, _ = server.recvfrom(2048)
            kind = request_kind(request)
            if kind is None:
                continue
            reply = build_reply(request)
            server.sendto(reply, ("192.168.137.255", 68))
            print(f"DHCP {'OFFER' if kind == 1 else 'ACK'} enviado para a box (xid={request[4:8].hex()})",
                  flush=True)


if __name__ == "__main__":
    argparse.ArgumentParser(description=__doc__).parse_args()
    serve()
