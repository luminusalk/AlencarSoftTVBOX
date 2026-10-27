"""Check the exact HEAD Actions run and download its C-SKY artifact."""

import argparse
from pathlib import Path
import subprocess
from urllib.error import HTTPError
from urllib.request import HTTPRedirectHandler, Request, build_opener, urlopen
import json
import zipfile
from io import BytesIO


API = "https://api.github.com/repos/luminusalk/AlencarSoftTVBOX"


class NoRedirect(HTTPRedirectHandler):
    def redirect_request(self, *args):
        return None


def token():
    result = subprocess.run(["git", "credential", "fill"],
                            input="protocol=https\nhost=github.com\n\n",
                            text=True, capture_output=True, check=True)
    values = dict(line.split("=", 1) for line in result.stdout.splitlines() if "=" in line)
    if not values.get("password"):
        raise RuntimeError("GitHub credential is unavailable")
    return values["password"]


def api(path, credential):
    request = Request(API + path, headers={"Authorization": "Bearer " + credential,
                                            "Accept": "application/vnd.github+json",
                                            "User-Agent": "AlencarSoftTVBOX-build-check"})
    with urlopen(request, timeout=20) as response:
        return json.load(response)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--download", action="store_true")
    args = parser.parse_args()
    sha = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    credential = token()
    runs = api("/actions/runs?branch=codex/native-iptv-repair&per_page=10", credential)["workflow_runs"]
    run = next((r for r in runs if r["head_sha"] == sha), None)
    if not run:
        raise SystemExit("No Actions run yet for current HEAD")
    print(f"run={run['id']} sha={sha[:8]} status={run['status']} conclusion={run['conclusion']}")
    jobs = api(f"/actions/runs/{run['id']}/jobs?per_page=30", credential)["jobs"]
    for job in jobs:
        print(f"  {job['name']}: {job['status']} {job['conclusion']}")
    if not args.download:
        return
    csky = next((j for j in jobs if "C-SKY" in j["name"]), None)
    if not csky or csky["conclusion"] != "success":
        raise SystemExit("C-SKY build is not successful yet")
    artifacts = api(f"/actions/runs/{run['id']}/artifacts", credential)["artifacts"]
    artifact = next((a for a in artifacts if a["name"] == "alencarsoft_tvbox_csky"), None)
    if not artifact:
        raise SystemExit("C-SKY artifact not uploaded yet")
    request = Request(artifact["archive_download_url"],
                      headers={"Authorization": "Bearer " + credential,
                               "User-Agent": "AlencarSoftTVBOX-build-check"})
    try:
        build_opener(NoRedirect()).open(request, timeout=20)
    except HTTPError as error:
        if error.code != 302:
            raise
        signed_url = error.headers["Location"]
    else:
        raise RuntimeError("Expected signed artifact redirect")
    with urlopen(signed_url, timeout=90) as response:
        archive = BytesIO(response.read())
    target = Path("build-artifacts") / f"ci-{run['id']}"
    target.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive) as file:
        for member in file.namelist():
            if Path(member).name != "alencarsoft_tvbox_csky":
                continue
            (target / "alencarsoft_tvbox_csky").write_bytes(file.read(member))
    binary = target / "alencarsoft_tvbox_csky"
    if not binary.exists():
        raise RuntimeError("C-SKY binary missing from artifact")
    print(f"Downloaded {binary} ({binary.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
