import sys, time
sys.path.insert(0, r"D:\EMULADORES\Git\AlencarSoftTVBOX\tools")
import gh_api

time.sleep(3)
runs = gh_api.api_get("actions/runs?per_page=3")
run = runs["workflow_runs"][0]
run_id = run["id"]
print(f"Run ID: {run_id} | {run['head_commit']['message'][:40]}")

for i in range(25):
    time.sleep(3)
    try:
        jobs = gh_api.api_get(f"actions/runs/{run_id}/jobs")
        csky = next(j for j in jobs["jobs"] if "Azamerica" in j["name"])
        print(f"[{i*3}s] C-SKY: {csky['status']} -> {csky['conclusion']}")
        if csky["status"] == "completed":
            if csky["conclusion"] == "success":
                print("C-SKY SUCCESS!")
                sys.exit(0)
            else:
                print("C-SKY FAILED!")
                gh_api.get_job_log(run_id)
                sys.exit(1)
    except Exception as e:
        print("Erro:", e)

print("Timeout")
sys.exit(2)
