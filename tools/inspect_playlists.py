from collections import Counter
from pathlib import Path
import re


root = Path(__file__).resolve().parent
for name in ("CanaisBR03.m3u8", "CanaisIPTV.m3u8"):
    lines = (root / name).read_text(encoding="utf-8", errors="replace").splitlines()
    urls = [line.strip() for line in lines if line.startswith(("http://", "https://"))]
    groups = Counter(re.findall(r'group-title="([^"]+)', "\n".join(lines)))
    print(name)
    print("lines", len(lines), "entries", sum(line.startswith("#EXTINF") for line in lines))
    print("urls", len(urls), "http", sum(url.startswith("http://") for url in urls),
          "https", sum(url.startswith("https://") for url in urls))
    print("groups", groups.most_common(10))
    print("first lines", lines[:8])
