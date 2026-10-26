import telnetlib, time

tn = telnetlib.Telnet("192.168.137.237", 23, timeout=5)
tn.read_until(b"login: ", timeout=3)
tn.write(b"root\n")
time.sleep(0.5)
tn.read_until(b"# ", timeout=3)

# Testar o probe interno do canal GloboNews
tn.write(b"/mnt/plugin/alencarsoft_tvbox --probe-source http://dns.explouddev.com:80/632270679/F823j7133R/284589; echo PROBE_EXIT=$?\n")
time.sleep(1.5)
out = tn.read_very_eager().decode("latin1", errors="replace")
print("Probe GloboNews:\n", out)

tn.close()
