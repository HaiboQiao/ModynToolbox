import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from modyn_sdk import serve
import json
import os
from pathlib import Path
import subprocess
import sys
import time
def handler(action,data,context):
    mode=data.get("params",{}).get("mode","cancel")
    if mode=="crash":
        os._exit(9)
    if mode=="flood":
        os.write(sys.stdout.fileno(),b"x"*70000+b"\n")
        time.sleep(2)
    if mode=="wrong-id":
        print(json.dumps({"protocol":1,"tool_id":"wrong","job_id":"wrong","seq":1}),flush=True)
        time.sleep(2)
    if mode=="tree":
        child=subprocess.Popen([sys.executable,"-B","-c","import time;time.sleep(60)"],creationflags=subprocess.CREATE_NO_WINDOW)
        (context.data_root/"child.pid").write_text(str(child.pid))
        time.sleep(60)
    if mode=="memory":
        allocations=[]
        while True:allocations.append(bytearray(16*1024*1024))
    if mode=="critical":
        context.critical()
        time.sleep(0.4)
        return {"message":"critical-complete"}
    for index in range(1000):
        context.check()
        context.progress(index,1000,"waiting")
        time.sleep(0.01)
    return {"message":"complete"}
raise SystemExit(serve(handler))
