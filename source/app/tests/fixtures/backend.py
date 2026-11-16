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
def ready_children(count, context, prefix):
    # Process creation is not Python/DLL initialization. Wait for child-side readiness.
    release=context.job_root/(prefix+".release")
    program=("import sys,time;from pathlib import Path;"
             "Path(sys.argv[1]).write_text('ready');"
             "exec('while not Path(sys.argv[2]).exists(): time.sleep(0.01)')")
    children=[];markers=[]
    for index in range(count):
        marker=context.job_root/(prefix+str(index)+".ready")
        children.append(subprocess.Popen([sys.executable,"-B","-c",program,str(marker),str(release)],creationflags=subprocess.CREATE_NO_WINDOW))
        markers.append(marker)
    deadline=time.monotonic()+12
    while not all(p.exists() for p in markers):
        context.check()
        for child in children:
            if child.poll() is not None:raise RuntimeError(f"Test child initialization failed: 0x{child.returncode & 0xffffffff:08X}")
        if time.monotonic()>deadline:raise RuntimeError('Test children did not initialize in time')
        time.sleep(0.01)
    return children,release

def handler(action,data,context):
    mode=data.get("params",{}).get("mode","cancel")
    if mode=="environment":
        keys=("USERPROFILE","LOCALAPPDATA","APPDATA","USERNAME")
        return {"text":"ok" if all(os.environ.get(k) for k in keys) else "missing"}
    if mode=="error-mode":
        import ctypes
        return {"text":str(ctypes.windll.kernel32.GetErrorMode() & 1)}
    if mode=="budget":
        children,release=ready_children(20,context,"budget")
        initialized=sum(child.poll() is None for child in children)
        release.write_text('stop')
        for child in children:
            if child.wait(timeout=5)!=0:raise RuntimeError('Test child did not exit normally')
        return {"text":str(initialized),"initialized_children":initialized}
    if mode=="crash":
        os._exit(9)
    if mode=="flood":
        os.write(sys.stdout.fileno(),b"x"*70000+b"\n")
        time.sleep(2)
    if mode=="wrong-id":
        print(json.dumps({"protocol":1,"tool_id":"wrong","job_id":"wrong","seq":1}),flush=True)
        time.sleep(2)
    if mode=="tree":
        children,_=ready_children(1,context,"tree")
        child=children[0]
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
