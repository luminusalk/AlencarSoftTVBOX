# Guia de Operação e Remoção: Daemon da Tecla SAT (AlencarSoft TVBOX)

Este documento descreve o funcionamento do daemon da tecla **SAT** no Azamerica S1009+, como utilizá-lo no dia a dia e como **remover tudo completamente** caso queira voltar o aparelho ao estado original de fábrica.

---

## 1. Como Funciona a Tecla SAT?

* **Daemon em Segundo Plano:** O executável nativo `/mnt/plugin/alencarsoft_tvbox --sat-daemon` fica rodando em segundo plano no receptor (consumo de CPU: 0.0%, menos de 1MB de RAM).
* **Escuta do Infravermelho:** Ele monitora o driver `/dev/gxirr0`. Quando você aperta a tecla **SAT** no controle remoto (código de hardware `0x6a`), ele dispara a interface IPTV em primeiro plano.
* **Ao fechar o aplicativo (EXIT / MENU):** O aplicativo encerra sua tela e o daemon principal reassume o controle do infravermelho imediatamente.
* **Ao reabrir:** Ao pressionar **SAT** novamente, o aplicativo abre na mesma hora. Você pode abrir e fechar quantas vezes quiser.

---

## 2. O que Acontece se Desligar o Receptor da Tomada (Reboot)?

A partição raiz do Azamerica S1009+ (`/`) fica gravada em memória Flash somente-leitura (`squashfs`), por isso processos em memória RAM param quando a energia é cortada.

Para religar o daemon após reiniciar o aparelho, você pode escolher **qualquer uma** das opções abaixo:

### Opção A — Pelo seu Computador (1 clique via Python)
No terminal do seu computador, execute:
```bash
python tools/start_sat_daemon.py
```
O script encontra automaticamente o IP do Azamerica na rede local, conecta via Telnet e inicia o daemon.

### Opção B — Direto pelo Telnet
Acesse o receptor pelo terminal ou Putty:
```bash
telnet 192.168.137.237
```
(Login: `root`, sem senha)

E execute:
```bash
/mnt/plugin/start_sat_daemon.sh
```

---

## 3. Como Parar o Daemon Temporariamente

Para pausar o daemon sem remover os arquivos:

* **Pelo PC:**
  ```bash
  python tools/stop_sat_daemon.py
  ```
* **Pelo Telnet na Box:**
  ```bash
  /mnt/plugin/stop_sat_daemon.sh
  ```
  ou:
  ```bash
  killall -9 alencarsoft_tvbox
  ```

---

## 4. Como Remover Definitivamente (Deixar a Box 100% de Fábrica)

Se você quiser apagar todos os inicializadores, logs e daemons instalados na partição flash do receptor:

Conecte no Telnet do receptor (`telnet 192.168.137.237`, login `root`) e execute os comandos abaixo:

```bash
# 1. Encerra qualquer processo em execucao
killall -9 alencarsoft_tvbox 2>/dev/null

# 2. Remove os scripts de inicializacao e parada do daemon
rm -f /mnt/plugin/start_sat_daemon.sh
rm -f /mnt/plugin/stop_sat_daemon.sh
rm -f /mnt/plugin/start_exit3_daemon.sh
rm -f /mnt/plugin/exit3_daemon.py

# 3. Remove os arquivos de instrucao e logs temporarios
rm -f /mnt/plugin/README_DAEMON_SAT.txt
rm -f /tmp/alencarsoft_sat.log
rm -f /tmp/exit3_daemon.log

# 4. Sincroniza a memoria flash
sync
```

Após executar esses comandos, nenhum processo ou script do AlencarSoft permanecerá em execução ou agendado no aparelho. O receptor fica **100% no padrão de fábrica**.
