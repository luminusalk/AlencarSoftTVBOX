from concurrent.futures import ThreadPoolExecutor
import socket


def has_telnet(index):
    address = f"192.168.137.{index}"
    try:
        with socket.create_connection((address, 23), timeout=0.35):
            return address
    except (OSError, TimeoutError):
        return None


with ThreadPoolExecutor(max_workers=48) as pool:
    for address in pool.map(has_telnet, range(2, 255)):
        if address:
            print(address)
