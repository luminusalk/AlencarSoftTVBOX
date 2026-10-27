"""Show only the tail of failed jobs for a GitHub Actions run."""

import sys
from urllib.error import HTTPError
from urllib.request import Request, build_opener, urlopen

from get_csky_ci_artifact import API, NoRedirect, api, token


def main():
    run_id = sys.argv[1]
    credential = token()
    jobs = api(f"/actions/runs/{run_id}/jobs?per_page=30", credential)["jobs"]
    for job in jobs:
        if job["conclusion"] != "failure":
            continue
        request = Request(API + f"/actions/jobs/{job['id']}/logs", headers={
            "Authorization": "Bearer " + credential,
            "User-Agent": "AlencarSoftTVBOX-build-check",
        })
        try:
            build_opener(NoRedirect()).open(request, timeout=30)
        except HTTPError as error:
            if error.code != 302:
                raise
            signed_url = error.headers["Location"]
        else:
            raise RuntimeError("Expected logs redirect")
        with urlopen(signed_url, timeout=30) as response:
            lines = response.read().decode("utf-8", "replace").splitlines()
        print(job["name"])
        print("\n".join(lines[-100:]))


if __name__ == "__main__":
    main()
