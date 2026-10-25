# GUIA MESTRE DE CONTINUIDADE, ENGENHARIA REVERSA E HANDOVER
## Projeto AlencarSoft TVBOX — Azamerica S1009+ (NationalChip GX6622)

> **Destinatário:** Engenheiro de Sistemas Embarcados / Agente Sênior de Baixo Nível.  
> **Objetivo:** Assumir o projeto, entender a arquitetura interna do hardware, dominar as ferramentas de conexão, compilação no GitHub e dar sequência imediata para reproduzir IPTV com áudio e vídeo 100% nativos (igual ao Netlink e XBMC).

---

## 1. Conexão e Controle do Aparelho (Telnet e Reboot)

### 1.1. Conexão Telnet Direta
O receptor **Azamerica S1009+** possui um servidor Telnet nativo rodando na porta `23`, com login automático de `root` (sem senha).
- **IP do Aparelho na Rede Local:** `192.168.18.23`
- **Porta:** `23`
- **Usuário:** `root`
- **Senha:** *(nenhuma / pressione Enter)*

#### Comando via PowerShell / CMD / Terminal Linux:
```bash
telnet 192.168.18.23 23
```

#### Automação em Python (sem travar prompt):
```python
import telnetlib, time

tn = telnetlib.Telnet("192.168.18.23", 23, timeout=5)
time.sleep(1)
tn.write(b"ls -la /tmp\n")
time.sleep(2)
print(tn.read_very_eager().decode("latin1", errors="ignore"))
tn.close()
```

### 1.2. Como Reiniciar a TV Box com Segurança
No terminal Telnet, execute:
```bash
# Sincroniza buffers na Flash NAND e reinicia o processador:
sync && reboot
```
*Se o sistema estiver travado:*
```bash
# Reinicialização forçada imediata pelo kernel:
echo 1 > /proc/sys/kernel/sysrq
echo b > /proc/sysrq-trigger
```

---

## 2. Integração com o GitHub (CI/CD, Código e Releases)

### 2.1. Onde Fica o Código-Fonte
- **Repositório:** `https://github.com/luminusalk/AlencarSoftTVBOX`
- **Diretório Local:** `D:\EMULADORES\Git\AlencarSoftTVBOX`
- **Branch Ativa:** `main`
- **Pasta do Código C:** `AlencarSoftTVBOX-Clang/`

### 2.2. Como Compilar no GitHub Actions
Como o processador da Azamerica é um **C-SKY CK610 (ABIv1)** com kernel legado Linux 3.0.8, compilar em GCCs modernos para x86_64 gera binários incompatíveis. Por isso, a compilação oficial roda no **GitHub Actions** usando o container oficial com o toolchain da fabricante:
- **Arquivo de Workflow:** `.github/workflows/build_tvbox.yml`
- **Toolchain C-SKY:** `csky-linux-tools-uclibc-2.8.01` (NationalChip GCC 4.5.1 uClibc).
- **Como disparar uma nova compilação:**
  Basta fazer um commit e dar `git push origin main`. O GitHub Actions compila em ~40 segundos e gera o binário `alencarsoft_tvbox_csky` na release `v1.0.8-native`.

```bash
git add .
git commit -m "feat(player): sua alteracao aqui"
git push origin main
```

### 2.3. Como Baixar e Rodar o Binário Novo na Box
Dentro do Telnet da box (`192.168.18.23`):
```bash
# 1. Derruba qualquer instancia anterior
killall -9 alencarsoft_tvbox 2>/dev/null
rm -f /tmp/alencarsoft_tvbox

# 2. Baixa a versao mais recente direto da release
wget https://github.com/luminusalk/AlencarSoftTVBOX/releases/download/v1.0.8-native/alencarsoft_tvbox_csky -O /tmp/alencarsoft_tvbox

# 3. Dá permissão de execução
chmod +x /tmp/alencarsoft_tvbox

# 4. Executa na TV
/tmp/alencarsoft_tvbox < /dev/null > /tmp/alencar_app.log 2>&1 &
```

---

## 3. Por Que Netlink e XBMC Funcionam? (Engenharia Reversa Revelada)

### 3.1. Como o Firmware de Fábrica (`dvbapp`) Opera
O executável principal da Azamerica é `/app/dvbapp` (um binário de 17.1 MB em C++ montado a partir de `/dev/mtdblock4`).
1. **O Netlink:** É um módulo interno compilado dentro do próprio `dvbapp`. Ele não é um aplicativo externo. Ele utiliza as chamadas internas diretas da API NationalChip (`libgxav` / `gxplayer`).
2. **O XBMC:** É uma interface Python 2.7 estática embutida no `dvbapp`. Quando o XBMC abre um canal via script, ele não toca o vídeo por Python. Ele entrega a URL para o `dvbapp` em C++, que fecha o menu e chama o player nativo do SoC.

### 3.2. Os 3 Pilares do Sucesso do Netlink/XBMC:
1. **Deseleção de Satélite e Limpeza de FIFOs:** O `dvbapp` gerencia o demux de satélite. Quando você entra no Netlink, o `dvbapp` desvincula os tuners DVB-S2 do demux e conecta a entrada de rede no módulo de decodificação `GXAV_MOD_DEMUX`.
2. **Aceleração do DSP de Áudio:** O `dvbapp` envia para o DSP (`/dev/gxav0`) os PIDs corretos de áudio e configura o codec de hardware.
3. **Sincronismo de Clock PTS (Audio PTS Sync):** O VPU de vídeo da NationalChip só avança os quadros de imagem se receber pulsos de sincronismo de relógio (PTS) do canal de áudio. Se o áudio falhar ou não sincronizar, o vídeo **congela em tela preta**!

---

## 4. O Hardware da Azamerica S1009+ e Suas Armadilhas

### 4.1. Arquitetura de Camadas de Vídeo e OSD
- **Plano de Vídeo (VPU HD):** Fica no fundo físico da tela (endereço `0x1d000000`, virtual `0x9d000000`).
- **Plano OSD (Framebuffer):** Fica na frente (`/dev/fb0`, 1280x720, 32-bit ARGB).
- **O Segredo da Transparência:** Quando o player toca vídeo, o Framebuffer deve ser pintado com **transparência total (`0x00000000`)**. Se tiver qualquer cor sólida (ex: preto `0xFF000000`), ela cobre fisicamente a camada de vídeo e você só verá tela preta! Ao voltar para a grade de canais, o Framebuffer deve ser repintado com `COLOR_DARK_BG` (`0xFF12131A`) para cobrir o último quadro congelado.

### 4.2. Codecs Suportados no DSP de Áudio por Hardware
Ao inicializar o kernel, o `dmesg` lista os firmwares carregados no DSP:
- `MPEG: date 2021.04.26 ver 1.3.2.4` (MP1, MP2, MP3)
- `DOBLY: date 2015.10.08 ver 1.1.0.0` (AC3)
- `SBC date 2020.09.16 ver 1.3.1.1`
- **ATENÇÃO CRÍTICA:** O chip **NÃO tem microcódigo de AAC** no DSP!
Streams com áudio AAC (`acodec = 0xb`) falham ao iniciar áudio no DSP. Como o decodificador de vídeo aguarda os pulsos de sincronismo de áudio (`audio pts_sync = 1`), o vídeo congela em tela preta. Canais com áudio em **MP3 ou AC3 funcionam perfeitamente com som nas caixas de som da TV!**

### 4.3. O Bloqueio da FIFO de Áudio (`in_channel[1] is linked to c0707b9c`)
Se o `dvbapp` original for finalizado de forma abrupta (`killall -9`), o driver do kernel `av.ko` mantém os canais de áudio vinculados. Qualquer novo player que tente abrir áudio receberá erro `gxav_ioctl : 962` e travará.
- **Solução Implementada em `src/player.c`:**
  A função `gxav_cleanup_audio()` abre `/dev/gxav0` e emite `ioctl(fd, GXAV_IOCTL_FIFO_UNLINK, &req)` nos módulos 1 a 6 e pinos 0 a 7, desobstruindo o driver.

### 4.4. A Limitação do `showcartoon`
O utilitário `/usr/local/bin/showcartoon` é o reprodutor nativo da NationalChip acoplado ao VPU.
- **Ele NÃO aceita URLs de rede (`http://` ou `https://`):** O FFmpeg dele foi compilado com `--disable-network`.
- **Ele aceita arquivos locais (`file://`) e FIFOs (Named Pipes):**
  Por isso, criamos o **HLS Bridge em C Puro (`src/hls_bridge.c`)**, que baixa a stream `.m3u8` via sockets HTTP em uma thread de fundo e grava os dados MPEG-TS continuamente em uma FIFO (`/tmp/live_stream.ts`). O `showcartoon` lê a FIFO achando que é um arquivo local e decodifica direto pelo silício do chip!
- **A Flag `-p`:** Nunca use `showcartoon -p`! A flag `-p` ativa o *Picture Mode* (somente vídeo, desativa a thread de áudio). Use sempre `showcartoon <arquivo> 60`.

---

## 5. Mapeamento do Controle Remoto (NEC `0xBFBF`, `RCUTYPE=61`)

O driver `/dev/gxirr0` emite pacotes de 4 bytes onde `data[0]` é o código do botão:
- `0xD0`: **CIMA (▲)** — Sobe na grade de canais.
- `0x70`: **BAIXO (▼)** — Desce na grade de canais.
- `0x88`: **ESQUERDA (◄)** — Aba anterior / Seleciona `[SIM]` no diálogo de saída.
- `0x08`: **DIREITA (►)** — Próxima aba / Seleciona `[NAO]` no diálogo de saída.
- `0xB0`: **OK** — Inicia reprodução do canal selecionado.
- `0x68`: **EXIT / BACK** — 
  - *Durante reprodução:* Encerra o vídeo imediatamente e retorna para a grade.
  - *Na grade:* Abre o modal `Deseja sair para o sistema original? [SIM] [NAO]`.
    - Ao escolher `[SIM]`, remonta `/app`, inicia `/app/dvbapp &` e fecha nosso app.

---

## 6. O Roteiro Exato Para Fazer Funcionar Qualquer Canal IPTV

Para fazer qualquer canal abrir instantaneamente com áudio e vídeo como no Netlink:

1. **Garantir que a URL do canal seja MPEG-TS ou HLS com áudio compatível (MP3/AC3):**
   - No catálogo `iptv.m3u8`, certifique-se de que o fluxo forneça segmentos `.ts` acessíveis por HTTP.
   - O canal local de teste comprovadamente funcional na TV é `/mnt/plugin/video_mp3.mp4` (720p H.264 + MP3 áudio).

2. **Fluxo da Ponte HLS (`src/hls_bridge.c`):**
   - Ao clicar no canal, `player_play(url)` detecta se é `.m3u8` ou `.ts`.
   - Se for `.m3u8`:
     1. Cria a FIFO `/tmp/live_stream.ts` (`mkfifo`).
     2. Thread 1 conecta via socket HTTP, baixa o chunk `.ts` e joga no Ring Buffer (4MB).
     3. Thread 2 lê do Ring Buffer e alimenta `/tmp/live_stream.ts`.
     4. Executa `/usr/local/bin/showcartoon /tmp/live_stream.ts 60 &`.
     5. O Framebuffer `/dev/fb0` é limpo com `0x00000000` (transparente).

3. **Se o canal vier com áudio AAC:**
   - Como o hardware NationalChip não tem decodificador de AAC no silício, para canais com AAC é necessário fazer a transcodificação do áudio para MP3/MP2 ou usar decodificação de áudio por software na thread em C antes de jogar para a FIFO de saída.

4. **Verificação de Logs em Tempo Real na Box:**
   ```bash
   # Acompanhe o log do app:
   tail -f /tmp/alencar_app.log
   
   # Acompanhe o log do reprodutor de hardware:
   tail -f /tmp/showcartoon.log
   
   # Verifique mensagens do kernel/driver:
   dmesg | tail -n 25
   ```
