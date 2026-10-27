"""Scan local M3U files, test prioritized live streams, and build a TVBOX list.

Every source entry is inspected locally. Network probes are bounded and only
visit likely Brazilian live TV entries; a successful HTTP response alone does
not count as a working channel.
"""

import argparse
from collections import Counter, defaultdict
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass
import json
from pathlib import Path
import re
import ssl
import sys
import time
from urllib.parse import urljoin, urlsplit
from urllib.request import Request, urlopen


DEFAULT_SOURCE = Path(r"D:\EMULADORES\Git\Iptv-Brasil-2026\Iptv-Brasil-2026")
DEFAULT_EXISTING = Path(__file__).resolve().parents[1] / "AlencarSoftTVBOX-Clang" / "iptv.m3u8"
GROUP_RE = re.compile(r'group-title="([^"]*)"', re.I)
VOD_RE = re.compile(r"film|filme|s[eé]rie|novela|temporada|episode|epis[oó]dio|netflix|prime video|amazon|disney|globoplay|adult", re.I)
NEWS_RE = re.compile(r"cultura|globo\s?news|cnn\s?brasil|band\s?news|record\s?news|jovem\s?pan|jp\s?news|sbt\s?news|tv\s?senado|tv\s?c[aâ]mara|bloomberg|bbc\s?news|euronews", re.I)
SPORTS_RE = re.compile(r"sport|esporte|espn|premiere|sportv|combate|caz[eé]|futebol|nossa tv|band\s?sports", re.I)
OPEN_RE = re.compile(r"cultura|globo|record|sbt|band|rede\s?tv|tv\s?brasil|gazeta|tv\s?aparecida|can[cç][aã]o\s?nova|tv\s?senado|tv\s?c[aâ]mara|futura|naza|tv\s?pai\s?eterno", re.I)
BRAZIL_RE = re.compile(r"(^|[^a-z])(br|brasil|brazil)([^a-z]|$)|canais|abertos|not[ií]cias|esport", re.I)
BAD_PREFIX_RE = re.compile(r"^(?:es|usa|us|uk|ar|tr|fr|de|it|pt|mx|arab)[: |]", re.I)


@dataclass(frozen=True)
class Entry:
    name: str
    url: str
    group: str
    source: str
    score: int
    category: str


def classify(name, group):
    if VOD_RE.search(group) or VOD_RE.search(name):
        return None
    if BAD_PREFIX_RE.search(name) and not BRAZIL_RE.search(group):
        return None
    if NEWS_RE.search(name):
        return "Notícias"
    if SPORTS_RE.search(name):
        return "Esportes"
    if OPEN_RE.search(name):
        return "Canais Abertos"
    if BRAZIL_RE.search(group) and not VOD_RE.search(group):
        return "Variedades"
    return None


def score_entry(name, group, source, category):
    score = {"Notícias": 450, "Esportes": 350, "Canais Abertos": 300, "Variedades": 100}[category]
    if source == "existing":
        score += 250
    elif source == "CanaisBR03.m3u8":
        score += 80
    if "720" in name or re.search(r"\bHD\b", name, re.I):
        score += 10
    if "1080" in name or "FHD" in name.upper():
        score -= 5  # favour HD reliability on this receiver
    if "BR" in group.upper() or "BRASIL" in group.upper():
        score += 20
    return score


def entries(path, source):
    name = group = ""
    with path.open("rb") as sample:
        encoding = "utf-8-sig"
        try:
            sample.read(65536).decode(encoding)
        except UnicodeDecodeError:
            encoding = "cp1252"
    with path.open("r", encoding=encoding, errors="replace") as file:
        for raw in file:
            line = raw.strip()
            if line.startswith("#EXTINF:"):
                name = line.rsplit(",", 1)[-1].strip()
                match = GROUP_RE.search(line)
                group = match.group(1).strip() if match else ""
            elif line.startswith(("http://", "https://")):
                if name and len(line) <= 510:
                    category = classify(name, group)
                    if category:
                        yield Entry(name, line, group, source,
                                    score_entry(name, group, source, category), category)
                name = group = ""


def inventory(source, existing):
    paths = [(existing, "existing")] + [(p, p.name) for p in sorted(source.glob("*.m3u8"))]
    seen = set()
    candidates = []
    scanned = Counter()
    for path, origin in paths:
        if not path.exists():
            continue
        total = 0
        for item in entries(path, origin):
            total += 1
            if item.url in seen:
                continue
            seen.add(item.url)
            candidates.append(item)
        scanned[origin] = total
    candidates.sort(key=lambda e: -e.score)
    return candidates, scanned


def fetch(url, timeout, maximum=65536):
    request = Request(url, headers={"User-Agent": "AlencarSoftTVBOX-ChannelScanner/1.0",
                                    "Range": f"bytes=0-{maximum - 1}"})
    with urlopen(request, timeout=timeout, context=ssl.create_default_context()) as response:
        data = response.read(maximum)
        return response.status, response.headers.get("Content-Type", ""), data, response.url


def is_ts(data):
    return len(data) >= 376 and any(data[offset] == 0x47 and data[offset + 188] == 0x47
                                    for offset in range(min(188, len(data) - 376 + 1)))


def probe(item, timeout):
    url = item.url
    for _ in range(3):
        try:
            status, content_type, data, resolved = fetch(url, timeout)
        except Exception as exc:
            return False, type(exc).__name__
        if status not in (200, 206):
            return False, f"HTTP {status}"
        if is_ts(data):
            return True, "MPEG-TS"
        if data.startswith(b"#EXTM3U") or "mpegurl" in content_type.lower():
            content = data.decode("utf-8-sig", "replace")
            if "#EXT-X-KEY" in content or "#EXT-X-MAP" in content:
                return False, "unsupported HLS"
            links = [line.strip() for line in content.splitlines()
                     if line.strip() and not line.startswith("#")]
            if not links:
                return False, "empty HLS"
            url = urljoin(resolved, links[0])
            continue
        return False, "not TS/HLS"
    return False, "nested HLS"


def safe_name(name):
    return re.sub(r"[\r\n,]", " ", name).strip()[:62]


def write_outputs(verified, output, report, scanned, attempted, errors):
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", encoding="utf-8", newline="\n") as file:
        file.write("#EXTM3U\n")
        for item in verified:
            file.write(f'#EXTINF:-1 group-title="{item.category}",{safe_name(item.name)}\n{item.url}\n')
    result = {
        "source_candidates": scanned,
        "network_attempts": attempted,
        "verified_count": len(verified),
        "categories": dict(Counter(item.category for item in verified)),
        "failure_reasons": dict(errors),
        "channels": [{"name": item.name, "category": item.category, "source": item.source,
                      "host": urlsplit(item.url).netloc} for item in verified],
    }
    report.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")


def main():
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    parser.add_argument("--existing", type=Path, default=DEFAULT_EXISTING)
    parser.add_argument("--output", type=Path, default=Path("build-artifacts/verified_channels.m3u8"))
    parser.add_argument("--report", type=Path, default=Path("build-artifacts/channel_scan_report.json"))
    parser.add_argument("--limit", type=int, default=300)
    parser.add_argument("--max-probes", type=int, default=1200)
    parser.add_argument("--workers", type=int, default=12)
    parser.add_argument("--timeout", type=float, default=4.0)
    parser.add_argument("--categories", default="", help="Comma-separated category filter")
    parser.add_argument("--quality", choices=("any", "FHD"), default="any")
    args = parser.parse_args()
    candidates, scanned = inventory(args.source, args.existing)
    if args.categories:
        allowed = set(args.categories.split(","))
        candidates = [item for item in candidates if item.category in allowed]
    if args.quality == "FHD":
        candidates = [item for item in candidates if re.search(r"1080p|full hd|fhd", item.name, re.I)]
    print(f"Inspected {sum(scanned.values())} live-looking entries; {len(candidates)} unique candidate URLs", flush=True)
    print("Per source:", dict(scanned), flush=True)
    # Keep network work bounded and prefer diverse channels over duplicate resolutions.
    selected = []
    names = Counter()
    hosts = Counter()
    for item in candidates:
        key = re.sub(r"\W+", "", re.sub(r"\b(?:1080p|720p|fhd|full hd|hd)\b", "", item.name, flags=re.I)).lower()
        host = urlsplit(item.url).netloc
        if names[key] >= 4 or hosts[host] >= 500:
            continue
        selected.append(item)
        names[key] += 1
        hosts[host] += 1
        if len(selected) >= args.max_probes:
            break
    verified = []
    failures = Counter()
    attempted = 0
    start = time.monotonic()
    with ThreadPoolExecutor(max_workers=args.workers) as executor:
        pending = {executor.submit(probe, item, args.timeout): item for item in selected}
        for future in as_completed(pending):
            attempted += 1
            item = pending[future]
            try:
                working, reason = future.result()
            except Exception as exc:
                working, reason = False, type(exc).__name__
            if working:
                verified.append(item)
            else:
                failures[reason] += 1
            if attempted % 100 == 0:
                print(f"Probed {attempted}/{len(selected)}; working={len(verified)}; {time.monotonic()-start:.0f}s", flush=True)
    verified.sort(key=lambda e: -e.score)
    verified = verified[:args.limit]
    write_outputs(verified, args.output, args.report, dict(scanned), attempted, failures)
    print(f"Verified {len(verified)} streams; wrote {args.output} and {args.report}", flush=True)


if __name__ == "__main__":
    main()
