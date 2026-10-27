"""Summarize local M3U sources without loading the large files into memory."""

from collections import Counter
from pathlib import Path
import re
import sys


SOURCE = Path(r"D:\EMULADORES\Git\Iptv-Brasil-2026\Iptv-Brasil-2026")
GROUP = re.compile(r'group-title="([^"]*)"', re.I)


def main():
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    for path in sorted(SOURCE.glob("*.m3u8")):
        count = 0
        groups = Counter()
        hosts = Counter()
        meta = ""
        for line in path.open("r", encoding="utf-8", errors="replace"):
            line = line.strip()
            if line.startswith("#EXTINF:"):
                meta = line
            elif line.startswith(("http://", "https://")):
                count += 1
                match = GROUP.search(meta)
                groups[match.group(1) if match else "(sem grupo)"] += 1
                hosts[line.split("/", 3)[2]] += 1
                meta = ""
        print(f"{path.name}: {count} links, {len(groups)} categorias, {len(hosts)} hosts")
        print("  categorias:", groups.most_common(12))
        print("  hosts:", hosts.most_common(8))


if __name__ == "__main__":
    main()
