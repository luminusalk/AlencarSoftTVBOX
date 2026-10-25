# -*- coding: utf-8 -*-
"""Generate native M3U playlist for Azamerica S1009+ native IPTV app."""

import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CATALOG_PATH = os.path.join(ROOT, 'plugin.video.az_iptv', 'catalog_cache', 'catalog.json')
OUTPUT_M3U = os.path.join(ROOT, 'AlencarSoftTVBOX-Clang', 'iptv.m3u8')


def build_m3u():
    with open(CATALOG_PATH, 'r', encoding='utf-8') as f:
        raw = json.load(f)

    data = raw.get('payload', raw)
    lines = ['#EXTM3U']
    total = 0
    for category in data.get('lists', []):
        cat_title = category.get('title', 'Geral')
        # Clean title for M3U group
        clean_cat = cat_title.split('(')[0].strip()
        for ch in category.get('channels', []):
            name = ch.get('name', 'Canal')
            url = ch.get('url', '')
            logo = ch.get('logo', '')
            if not url:
                continue
            lines.append('#EXTINF:-1 tvg-logo="%s" group-title="%s",%s' % (logo, clean_cat, name))
            lines.append('#EXTGRP:%s' % clean_cat)
            lines.append(url)
            total += 1

    content = '\n'.join(lines) + '\n'
    with open(OUTPUT_M3U, 'w', encoding='utf-8') as f:
        f.write(content)
    print('Gerado M3U nativo com %d canais em %s' % (total, OUTPUT_M3U))
    return OUTPUT_M3U


if __name__ == '__main__':
    build_m3u()
