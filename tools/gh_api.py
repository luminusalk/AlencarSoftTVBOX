import subprocess
import urllib.request
import json
import time
import sys

def get_token():
    p = subprocess.Popen(['git', 'credential', 'fill'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out, _ = p.communicate(b'protocol=https\nhost=github.com\n\n')
    for line in out.decode('latin1', errors='replace').splitlines():
        if line.startswith('password='):
            return line.split('=', 1)[1].strip()
    return None

def api_get(endpoint):
    token = get_token()
    url = f"https://api.github.com/repos/luminusalk/AlencarSoftTVBOX/{endpoint}"
    headers = {'User-Agent': 'Mozilla/5.0'}
    if token:
        headers['Authorization'] = f'Bearer {token}'
    req = urllib.request.Request(url, headers=headers)
    with urllib.request.urlopen(req) as resp:
        return json.loads(resp.read().decode('utf-8'))

def get_job_log(run_id):
    token = get_token()
    jobs = api_get(f'actions/runs/{run_id}/jobs')
    for j in jobs.get('jobs', []):
        if 'Azamerica' in j.get('name'):
            jid = j.get('id')
            url = f"https://api.github.com/repos/luminusalk/AlencarSoftTVBOX/actions/jobs/{jid}/logs"
            class NoAuthRedirect(urllib.request.HTTPRedirectHandler):
                def redirect_request(self, req, fp, code, msg, headers, newurl):
                    new_req = super().redirect_request(req, fp, code, msg, headers, newurl)
                    if new_req and 'Authorization' in new_req.headers:
                        del new_req.headers['Authorization']
                    return new_req

            opener = urllib.request.build_opener(NoAuthRedirect)
            req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0', 'Authorization': f'Bearer {token}'})
            try:
                with opener.open(req) as resp:
                    log_txt = resp.read().decode('utf-8', errors='replace')
                    for line in log_txt.splitlines():
                        if any(k in line for k in ['SUCESSO', 'Compilando', 'ck610', 'mabi', 'error:']):
                            print(line)
            except Exception as e:
                print("Erro ao baixar log:", e)

def wait_latest_run():
    for i in range(12):
        data = api_get('actions/runs?per_page=1')
        r = data['workflow_runs'][0]
        rid = r.get('id')
        st = r.get('status')
        conc = r.get('conclusion')
        msg = r.get('head_commit', {}).get('message', '').split('\n')[0]
        print(f"[{i+1}/12] Run {rid}: status={st} conc={conc} ({msg})")
        if st == 'completed':
            return conc
        time.sleep(5)
    return None

if __name__ == '__main__':
    wait_latest_run()
