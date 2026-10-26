# -*- coding: utf-8 -*-
"""Gera e valida catalogo perfeito com canais selecionados para o AlencarSoft TVBOX.
Categorias em ordem alfabetica e apenas uma categoria Noticias.
"""
import urllib.request
import socket
from concurrent.futures import ThreadPoolExecutor

socket.setdefaulttimeout(2.5)

# 1. Carrega Mapa BR03
with open(r'D:\EMULADORES\Git\Iptv-Brasil-2026\Iptv-Brasil-2026\CanaisBR03.m3u8', 'r', encoding='utf-8', errors='ignore') as f:
    lines = [l.strip() for l in f if l.strip()]

br03 = {}
for i, l in enumerate(lines):
    if l.startswith('#EXTINF:'):
        title = l.split(',', 1)[1].strip() if ',' in l else l.strip()
        for j in range(i+1, min(i+5, len(lines))):
            if lines[j].startswith(('http://', 'https://')):
                br03[title] = lines[j]
                break

# 2. Categorias Organizadas em Ordem Alfabetica (apenas 1 Noticias, sem Apuracao)
catalog_def = {
    "Canais Abertos": [
        ("TV Cultura HD", "https://player-tvcultura.stream.uol.com.br/live/tvcultura_lsd.m3u8?vhost=player-tvcultura.stream.uol.com.br"),
        ("Globo SP HD", "http://dns.explouddev.com:80/632270679/F823j7133R/289883"),
        ("Globo RJ HD", "http://dns.explouddev.com:80/632270679/F823j7133R/2374"),
        ("Globo Minas HD", "http://64.31.23.207:80/96795792/50165825/5298257.ts"),
        ("SBT HD", "http://31.43.191.125:8080/restream/restream/4898"),
        ("Band SP FHD", "http://governosptx.org:80/3llohwk4g9/kwf099q30o/20525.ts"),
        ("RedeTV FHD", "http://31.43.191.125:8080/restream/restream/4896")
    ],
    "Documentários e Variedades": [
        ("Discovery Channel HD", br03.get("Discovery Channel HD")),
        ("Discovery Science HD", br03.get("Discovery Science HD")),
        ("Discovery Theater HD", br03.get("Discovery Theater HD")),
        ("Discovery Turbo HD", br03.get("Discovery Turbo HD")),
        ("Discovery World HD", br03.get("Discovery World HD")),
        ("Animal Planet HD", br03.get("Animal Planet HD")),
        ("History HD", br03.get("History HD")),
        ("ID HD", br03.get("ID HD")),
        ("TLC HD", br03.get("TLC HD")),
        ("HGTV HD", br03.get("HGTV HD")),
        ("Modo Viagem HD", br03.get("Modo Viagem HD")),
        ("Canal Brasil HD", br03.get("Canal Brasil HD")),
        ("Multishow HD", br03.get("Multishow HD")),
        ("BIS HD", br03.get("BIS HD")),
        ("Canal UOL HD", br03.get("Canal UOL HD")),
        ("OFF HD", br03.get("OFF HD"))
    ],
    "Esportes": [
        ("sportv HD", br03.get("sportv HD")),
        ("sportv2 HD", br03.get("sportv2 HD")),
        ("sportv3 HD", br03.get("sportv3 HD")),
        ("ESPN 4 HD", br03.get("ESPN 4 HD")),
        ("ESPN 5 HD", br03.get("ESPN 5 HD")),
        ("ESPN 6 HD", br03.get("ESPN 6 HD")),
        ("Premiere 2 HD", br03.get("Premiere 2 HD")),
        ("Premiere 3 HD", br03.get("Premiere 3 HD")),
        ("Premiere 4 HD", br03.get("Premiere 4 HD")),
        ("Premiere 5 HD", br03.get("Premiere 5 HD")),
        ("Premiere 6 HD", br03.get("Premiere 6 HD")),
        ("Premiere 7 HD", br03.get("Premiere 7 HD")),
        ("Premiere 8 HD", br03.get("Premiere 8 HD")),
        ("Premiere Clubes HD", br03.get("Premiere Clubes HD")),
        ("Combate HD", br03.get("Combate HD")),
        ("Band Sports HD", br03.get("Band Sports HD")),
        ("GE TV HD", br03.get("GE TV HD")),
        ("Eurosport HD", br03.get("Eurosport HD"))
    ],
    "Filmes e Séries": [
        ("HBO HD", br03.get("HBO HD")),
        ("HBO2 HD", br03.get("HBO2 HD")),
        ("HBO+ HD", br03.get("HBO+ HD")),
        ("HBO Family HD", br03.get("HBO Family HD")),
        ("HBO MUNDI HD", br03.get("HBO MUNDI HD")),
        ("HBO POP HD", br03.get("HBO POP HD")),
        ("HBO Signature HD", br03.get("HBO Signature HD")),
        ("HBO XTREME HD", br03.get("HBO XTREME HD")),
        ("Telecine Premium HD", br03.get("Telecine Premium HD")),
        ("Telecine Action HD", br03.get("Telecine Action HD")),
        ("Telecine Touch HD", br03.get("Telecine Touch HD")),
        ("Telecine Pipoca HD", br03.get("Telecine Pipoca HD")),
        ("Telecine Cult HD", br03.get("Telecine Cult HD")),
        ("Telecine Fun HD", br03.get("Telecine Fun HD")),
        ("Warner Channel HD", br03.get("Warner Channel HD")),
        ("Universal TV HD", br03.get("Universal TV HD")),
        ("Studio Universal HD", br03.get("Studio Universal HD")),
        ("AXN HD", br03.get("AXN HD")),
        ("Cinemax HD", br03.get("Cinemax HD")),
        ("Megapix HD", br03.get("Megapix HD")),
        ("Canal Sony HD", br03.get("Canal Sony HD")),
        ("AMC HD", br03.get("AMC HD")),
        ("USA HD", br03.get("USA HD"))
    ],
    "Infantil e Desenhos": [
        ("Cartoonito HD", br03.get("Cartoonito HD")),
        ("Discovery Kids HD", br03.get("Discovery Kids HD")),
        ("Gloob HD", br03.get("Gloob HD")),
        ("Gloobinho HD", br03.get("Gloobinho HD")),
        ("Adult Swim HD", br03.get("Adult Swim HD")),
        ("TV Rá Tim Bum HD", br03.get("TV RÆ Tim Bum HD"))
    ],
    "Notícias": [
        ("TV Cultura HD", "https://player-tvcultura.stream.uol.com.br/live/tvcultura_lsd.m3u8?vhost=player-tvcultura.stream.uol.com.br"),
        ("GloboNews HD", "http://dns.explouddev.com:80/632270679/F823j7133R/284589"),
        ("Band News HD", br03.get("Band News HD")),
        ("CNN Brasil FHD", "http://dns.explouddev.com:80/632270679/F823j7133R/474"),
        ("CNN International HD", br03.get("CNN International HD")),
        ("Record News HD", br03.get("Record News HD")),
        ("SBT News HD", br03.get("SBT News HD")),
        ("JP News HD", br03.get("JP News HD")),
        ("BM&C HD", br03.get("BM&C HD")),
        ("CNBC Times HD", br03.get("TIMES | EXCLUSIVO CNBC"))
    ]
}

# 3. Testar todos os canais para garantir 100% de sucesso
print("Validando todos os canais selecionados...")
all_items = []
for cat_name in sorted(catalog_def.keys()):
    for title, url in catalog_def[cat_name]:
        if url:
            all_items.append((cat_name, title, url))

def probe_one(item):
    cat, title, url = item
    try:
        req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
        with urllib.request.urlopen(req, timeout=2.5) as resp:
            data = resp.read(1024)
            ct = resp.headers.get('content-type', '')
            if resp.status == 200 and 'html' not in ct:
                return (True, cat, title, url)
    except Exception as e:
        return (False, cat, title, url, str(e))
    return (False, cat, title, url, 'invalid')

with ThreadPoolExecutor(max_workers=30) as pool:
    results = list(pool.map(probe_one, all_items))

valid_by_cat = {}
ok_count = 0
fail_count = 0
for r in results:
    if r[0]:
        cat, title, url = r[1], r[2], r[3]
        if cat not in valid_by_cat:
            valid_by_cat[cat] = []
        valid_by_cat[cat].append((title, url))
        ok_count += 1
    else:
        fail_count += 1
        print(f"  [FAIL] {r[1]} -> {r[2]}: {r[4]}")

print(f"\nResumo: {ok_count} OK, {fail_count} Falhas.")

# 4. Gerar iptv.m3u8 final
m3u_lines = ["#EXTM3U"]
total = 0
for cat_name in sorted(valid_by_cat.keys()):
    for title, url in valid_by_cat[cat_name]:
        m3u_lines.append(f'#EXTINF:-1 tvg-logo="" group-title="{cat_name}",{title}')
        m3u_lines.append(f'#EXTGRP:{cat_name}')
        m3u_lines.append(url)
        total += 1

out_path = r"D:\EMULADORES\Git\AlencarSoftTVBOX\AlencarSoftTVBOX-Clang\iptv.m3u8"
with open(out_path, "w", encoding="utf-8") as f:
    f.write("\n".join(m3u_lines) + "\n")

print(f"Sucesso! Gerado {out_path} com {total} canais 100% verificados e funcionais!")
