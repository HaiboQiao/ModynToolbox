"""Simulate removal/restoration of an idle installed tool folder in a private workspace."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess
import time
import uuid
ROOT=Path(__file__).resolve().parents[3]
base=ROOT/".local/manual-removal"/str(uuid.uuid4())[:8];base.mkdir(parents=True)
package=Path(json.loads((ROOT/".local/tools/file-hash/latest.json").read_text(encoding="utf-8"))["package"])
data=base/"data";tools=base/"tools";version=json.loads((package/"manifest.json").read_text(encoding="utf-8"))["package_version"];installed=tools/"file-hash"/version
source=base/"source.txt";source.write_bytes(b"preserve tool and session")
digest=hashlib.sha512(source.read_bytes()).hexdigest()
delete_marker=base/"delete-now";restore_marker=base/"restore-now"
report=base/"report.json";script=base/"script.json"
steps=[{"op":"install","path":str(package)},{"op":"open","tool":"file-hash"},
       {"op":"input","value":{"files":[str(source)],"params":{"algorithm":"SHA-512"}}},
       {"op":"run","action":"hash"},{"op":"wait","state":"succeeded"},
       {"op":"mark","path":str(delete_marker),"delay_ms":1500},
       {"op":"refresh"},{"op":"expect","sessions":1},
       {"op":"screenshot","path":str(base/"missing-tool.png")},
       {"op":"mark","path":str(restore_marker),"delay_ms":1500},
       {"op":"refresh"},{"op":"run","action":"hash"},{"op":"wait","state":"succeeded"},
       {"op":"expect","result_contains":digest}]
script.write_text(json.dumps({"report":str(report),"steps":steps}),encoding="utf-8")
env=os.environ.copy();env["QT_QPA_PLATFORM"]="offscreen"
process=subprocess.Popen([str(ROOT/".local/build/windows-release/bin/Release/ModynToolbox.App.exe"),"--data-root",str(data),"--packages-root",str(tools),"--automation",str(script)],env=env,cwd=base,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,creationflags=subprocess.CREATE_NO_WINDOW)
def wait_marker(path):
    deadline=time.monotonic()+15
    while not path.exists() and process.poll() is None and time.monotonic()<deadline:time.sleep(0.01)
    assert path.exists(),str(path)
try:
    wait_marker(delete_marker)
    resolved=installed.resolve()
    assert resolved.is_relative_to(base.resolve()) and resolved!=base.resolve()
    shutil.rmtree(resolved)
    assert (data/"tools/file-hash").is_dir()
    wait_marker(restore_marker)
    shutil.copytree(package,installed)
    process.wait(timeout=20)
    result=json.loads(report.read_text(encoding="utf-8"));assert process.returncode==0 and result["success"],result.get("error")
    output={"success":True,"steps":len(steps),"missing_session_preserved":True,"parameters_and_files_restored":True,"tool_data_preserved":True,"report":str(report)}
    (ROOT/".local/logs/manual-removal.json").write_text(json.dumps(output,indent=2),encoding="utf-8")
    print(json.dumps(output))
finally:
    if process.poll() is None:process.terminate();process.wait(timeout=5)
