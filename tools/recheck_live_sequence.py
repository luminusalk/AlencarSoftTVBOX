"""Recheck fresh scan for HLS regression/stale segments. Reports contain no URLs."""
from concurrent.futures import ThreadPoolExecutor, as_completed
from collections import Counter
from hashlib import sha256
import json
from pathlib import Path
import re
import time
from urllib.parse import urljoin

from scan_live_channels import entries, fetch, is_ts, probe, write_outputs

ROOT = Path(__file__).resolve().parents[1]
SCAN = ROOT / "build-artifacts/verified_refresh.m3u8"


def snapshot(url):
    _, _, data, resolved = fetch(url, 5, 131072)
    if not data.startswith(b"#EXTM3U"):
        return None
    text = data.decode("utf-8-sig", "replace")
    match = re.search(r"#EXT-X-MEDIA-SEQUENCE:(\d+)", text)
    if not match:
        raise ValueError("missing_sequence")
    links = [urljoin(resolved, line.strip()) for line in text.splitlines()
             if line.strip() and not line.startswith("#")]
    return int(match.group(1)), links


def check(item):
    try:
        first = snapshot(item.url) if ".m3u" in item.url.lower() else None
        if first is None:
            status, _, data, _ = fetch(item.url, 5, 65536)
            if status not in (200, 206) or not is_ts(data):
                return False, "direct_TS_invalid_first"
            time.sleep(7)
            status, _, later, _ = fetch(item.url, 5, 65536)
            if status not in (200, 206) or not is_ts(later):
                return False, "direct_TS_invalid_later"
            if sha256(data).digest() == sha256(later).digest():
                return False, "direct_TS_same_prefix_7s"
            return True, "direct_TS_changed_7s"
        ok, reason = probe(item, 5)
        if not ok:
            return False, reason
        time.sleep(7)
        second = snapshot(item.url)
        if second is None or second[0] < first[0]:
            return False, "sequence_regression"
        if second == first:
            # Long segments can legitimately leave a playlist unchanged for 7s.
            return True, "sequence_unchanged_7s_inconclusive"
        if not second[1]:
            return False, "empty_playlist"
        if second[0] > first[0] and first[1] == second[1]:
            return False, "sequence_advanced_same_segments"
        return True, "sequence_advanced" if second[0] > first[0] else "playlist_updated"
    except Exception as exc:
        return False, type(exc).__name__


def main():
    candidates = list(entries(SCAN, "fresh_scan"))
    verified, results = [], []
    with ThreadPoolExecutor(max_workers=16) as pool:
        pending = {pool.submit(check, item): item for item in candidates}
        for future in as_completed(pending):
            item = pending[future]
            ok, reason = future.result()
            results.append({"name": item.name, "passed": ok, "result": reason,
                            "url_sha256": sha256(item.url.encode()).hexdigest()})
            if ok:
                verified.append(item)
    verified.sort(key=lambda item: (-item.score, item.name))
    write_outputs(verified, SCAN, ROOT / "build-artifacts/channel_recheck_report.json",
                  {}, len(candidates), Counter(r["result"] for r in results if not r["passed"]))
    (ROOT / "build-artifacts/channel_sequence_report.json").write_text(
        json.dumps(results, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({"checked": len(candidates), "retained": len(verified),
                      "results": dict(Counter(r["result"] for r in results))}, ensure_ascii=False))


if __name__ == "__main__":
    main()
