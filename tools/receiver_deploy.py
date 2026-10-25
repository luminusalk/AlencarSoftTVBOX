# -*- coding: utf-8 -*-
"""Tool to upload files to Azamerica receiver via telnet using base64."""

import base64
import os
import sys
import telnetlib
import time

CANDIDATES = ['192.168.18.24', '192.168.18.23', '192.168.137.235']


def find_receiver():
    for ip in CANDIDATES:
        try:
            tn = telnetlib.Telnet(ip, 23, timeout=1.5)
            tn.read_until(b'#', timeout=2)
            return tn, ip
        except Exception:
            continue
    return None, None


def upload_file(local_path, remote_path):
    tn, ip = find_receiver()
    if not tn:
        print('Erro: Receptor nao encontrado na rede.')
        sys.exit(1)
    
    with open(local_path, 'rb') as f:
        data = f.read()
    
    b64_data = base64.b64encode(data).decode('ascii')
    print('Enviando %s (%d bytes) para %s:%s' % (os.path.basename(local_path), len(data), ip, remote_path))
    
    # Write in chunks to prevent telnet buffer overflows
    tn.write(b'cat << "EOF" > /tmp/upload.b64\n')
    chunk_size = 512
    for i in range(0, len(b64_data), chunk_size):
        chunk = b64_data[i:i+chunk_size]
        tn.write((chunk + '\n').encode('ascii'))
        time.sleep(0.02)
    tn.write(b'EOF\n')
    time.sleep(0.2)
    
    # Decode to target path
    cmd = ('base64 -d /tmp/upload.b64 > %s && rm -f /tmp/upload.b64\n' % remote_path).encode('ascii')
    tn.write(cmd)
    time.sleep(0.2)
    tn.write(('ls -l %s\n' % remote_path).encode('ascii'))
    time.sleep(0.5)
    
    output = tn.read_very_eager().decode('latin-1', 'ignore')
    print(output)
    tn.close()


if __name__ == '__main__':
    if len(sys.argv) < 3:
        print('Uso: python receiver_deploy.py <local_path> <remote_path>')
        sys.exit(1)
    upload_file(sys.argv[1], sys.argv[2])
