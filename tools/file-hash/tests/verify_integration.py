"""Install/run/remove/reinstall an extension using the already frozen host."""
from pathlib import Path
import datetime
import hashlib
import json
import os
import subprocess
import time
import uuid
ROOT=Path(__file__).resolve().parents[3]
PROOF=ROOT/".local/integration-proof"
base=PROOF/("run-"+str(uuid.uuid4())[:8]);base.mkdir(parents=True)
package=Path(json.loads((ROOT/".local/tools/file-hash/latest.json").read_text(encoding="utf-8"))["package"])
version=json.loads((package/"manifest.json").read_text(encoding="utf-8"))["package_version"]
binary=ROOT/".local/build/windows-release/bin/Release/ModynToolbox.App.exe"
first=base/"中文 sample.txt";first.write_bytes(b"ModynToolbox native extension\n")
second=base/"empty.bin";second.write_bytes(b"")
sha256=hashlib.sha256(first.read_bytes()).hexdigest()
sha512=hashlib.sha512(second.read_bytes()).hexdigest()
env={k:os.environ[k] for k in ("SystemRoot","WINDIR","SystemDrive","COMSPEC") if k in os.environ}
(base/"temp").mkdir();env.update(PATH=os.environ["SystemRoot"]+"/System32",TEMP=str(base/"temp"),TMP=str(base/"temp"),QT_QPA_PLATFORM="offscreen")
standalone=subprocess.run([str(package/"backend/ModynHashCli.exe"),"--file",str(first)],env=env,cwd=base,capture_output=True,timeout=10,creationflags=subprocess.CREATE_NO_WINDOW,check=True)
assert json.loads(standalone.stdout)["sha256"]==sha256
sentinel=base/"data/tools/file-hash/keep-me.json";sentinel.parent.mkdir(parents=True);sentinel.write_text('{"preserved":true}',encoding="utf-8")
report=base/"report.json";script=base/"script.json"
screen=ROOT/".local/logs/final-file-hash.png"
steps=[
    {"op":"install","path":str(package)},
    {"op":"open","tool":"file-hash"},
    {"op":"input","value":{"files":[str(first)],"params":{"algorithm":"SHA-256"}}},
    {"op":"run","action":"hash"},{"op":"wait","state":"succeeded"},{"op":"expect","result_contains":sha256},
    {"op":"detach"},{"op":"expect","detached":True},
    {"op":"open","tool":"file-hash","fresh":True},
    {"op":"input","value":{"files":[str(second)],"params":{"algorithm":"SHA-512"}}},
    {"op":"run","action":"hash"},{"op":"wait","state":"succeeded"},{"op":"expect","result_contains":sha512},
    {"op":"pair","session":1},{"op":"expect","sessions":2},{"op":"screenshot","path":str(screen)},
    {"op":"remove","tool":"file-hash","version":version,"expect_error":True},
    {"op":"close","confirmed":True},{"op":"open","tool":"file-hash"},{"op":"close","confirmed":True},
    {"op":"remove","tool":"file-hash","version":version},{"op":"expect","sessions":0,"exists":str(sentinel)},
    {"op":"install","path":str(package)},{"op":"open","tool":"file-hash"},
    {"op":"input","value":{"files":[str(first)],"params":{"algorithm":"SHA-256"}}},
    {"op":"run","action":"hash"},{"op":"wait","state":"succeeded"},{"op":"expect","result_contains":sha256,"exists":str(sentinel)}]
script.write_text(json.dumps({"report":str(report),"steps":steps},ensure_ascii=False),encoding="utf-8")
started=time.perf_counter()
completed=subprocess.run([str(binary),"--data-root",str(base/"data"),"--packages-root",str(base/"tools"),"--automation",str(script)],env=env,cwd=base,capture_output=True,timeout=60,creationflags=subprocess.CREATE_NO_WINDOW)
if not report.exists():raise RuntimeError((completed.returncode,completed.stderr.decode("utf-8",errors="replace")))
result=json.loads(report.read_text(encoding="utf-8"))
assert completed.returncode==0 and result["success"],result
install_ms=[step["elapsed_ms"] for step in result["steps"] if step["operation"]=="install"]
output={"success":True,"steps":len(steps),"seconds":round(time.perf_counter()-started,3),"first_install_ms":install_ms[0],"reinstall_ms":install_ms[1],"independent_standalone":True,"package":str(package),"data_preserved":sentinel.exists(),"report":str(report),"at":datetime.datetime.now(datetime.timezone.utc).isoformat()}
(PROOF/"integration-result.json").write_text(json.dumps(output,indent=2),encoding="utf-8")
print(json.dumps(output))
subprocess.run([os.sys.executable,str(ROOT/"scripts/freeze-host.py"),"check","--record",str(PROOF/"release-host-final2.json")],cwd=ROOT,check=True)
