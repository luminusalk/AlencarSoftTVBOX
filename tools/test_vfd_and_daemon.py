import telnetlib, time

tn = telnetlib.Telnet("192.168.137.237", 23, timeout=5)
tn.read_until(b"login: ", timeout=3)
tn.write(b"root\n")
time.sleep(0.5)
tn.read_until(b"# ", timeout=3)

# 1. Checar processos
tn.write(b"ps | grep alencarsoft_tvbox | grep -v grep\n")
time.sleep(0.5)
out1 = tn.read_very_eager().decode("latin1", errors="replace")
print("Processos ativos:\n", out1)

# 2. Testar VFD diretamente na porta serial ttyS1
tn.write(b"printf 'N01 TV CULTURA\\r\\n' > /dev/ttyS1\n")
time.sleep(0.5)
out2 = tn.read_very_eager().decode("latin1", errors="replace")
print("VFD test output:\n", out2)

tn.close()
