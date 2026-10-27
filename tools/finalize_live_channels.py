"""Merge successful scans into a balanced C-compatible live TV catalog."""

from collections import Counter, defaultdict
from pathlib import Path
import re
import unicodedata


ROOT = Path(__file__).resolve().parents[1]
INPUTS = (ROOT / "build-artifacts/verified_channels.m3u8",
          ROOT / "build-artifacts/verified_open.m3u8")
OUTPUT = ROOT / "AlencarSoftTVBOX-Clang/iptv.m3u8"
CATEGORIES = ("Notícias", "Canais Abertos", "Esportes", "Variedades")
TARGETS = {"Notícias": 60, "Canais Abertos": 100, "Esportes": 100, "Variedades": 40}
GROUP = re.compile(r'group-title="([^"]*)"')


def read(path):
    metadata = ""
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("#EXTINF:"):
            metadata = line
        elif line.startswith(("http://", "https://")) and metadata:
            group = GROUP.search(metadata)
            if group:
                yield group.group(1), metadata.rsplit(",", 1)[-1], line
            metadata = ""


def identity(name):
    base = unicodedata.normalize("NFKD", name).encode("ascii", "ignore").decode().lower()
    base = re.sub(r"^(br|brazil)\s*:\s*", "", base)
    base = re.sub(r"\b(1080p|720p|full hd|fhd|uhd|4k|hd|sd)\b", "", base)
    return re.sub(r"[^a-z0-9]", "", base)


def quality(name):
    if re.search(r"1080p|full hd|fhd", name, re.I):
        return "FHD"
    if re.search(r"720p|\bHD\b", name, re.I):
        return "HD"
    return "SD"


def main():
    by_category = defaultdict(list)
    urls = set()
    for path in INPUTS:
        if not path.exists():
            raise SystemExit(f"Missing verified scan: {path}")
        for category, name, url in read(path):
            if category not in CATEGORIES or url in urls:
                continue
            urls.add(url)
            by_category[category].append((name.replace("�", "").strip(), url))

    selected = defaultdict(list)
    chosen_urls = set()
    variants = Counter()
    available_qualities = defaultdict(set)
    selected_qualities = defaultdict(set)
    for category in CATEGORIES:
        for name, _ in by_category[category]:
            if quality(name) in ("HD", "FHD"):
                available_qualities[(category, identity(name))].add(quality(name))

    def take(category, maximum):
        for name, url in by_category[category]:
            if len(selected[category]) >= maximum:
                break
            key = (category, identity(name))
            if not key[1] or variants[key] >= 2 or url in chosen_urls:
                continue
            if (variants[key] == 1 and quality(name) in selected_qualities[key]
                    and available_qualities[key] - selected_qualities[key]):
                continue
            selected[category].append((name, url))
            variants[key] += 1
            selected_qualities[key].add(quality(name))
            chosen_urls.add(url)

    for category in CATEGORIES:
        take(category, TARGETS[category])
    remaining = 300 - sum(map(len, selected.values()))
    if remaining:
        for category in CATEGORIES:
            take(category, len(selected[category]) + remaining)
            remaining = 300 - sum(map(len, selected.values()))
            if not remaining:
                break

    with OUTPUT.open("w", encoding="utf-8", newline="\n") as file:
        file.write("#EXTM3U\n")
        for category in CATEGORIES:
            for name, url in selected[category]:
                clean = re.sub(r"[\r\n,\"]", " ", name)[:62]
                file.write(f'#EXTINF:-1 group-title="{category}",{clean}\n{url}\n')
    print(f"Wrote {OUTPUT} with {sum(map(len, selected.values()))} verified channels")
    for category in CATEGORIES:
        print(f"  {category}: {len(selected[category])} ({dict(Counter(quality(n) for n, _ in selected[category]))})")


if __name__ == "__main__":
    main()
