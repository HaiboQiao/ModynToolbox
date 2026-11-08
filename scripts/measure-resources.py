"""Windows process resource samples for private offscreen acceptance instances."""
from pathlib import Path
import ctypes as c
from ctypes import wintypes as w
import json
import os
import platform
import statistics
import subprocess
import time
import uuid
ROOT=Path(__file__).resolve().parents[1]
RELEASE=Path(json.loads((ROOT/".local/releases/latest.json").read_text(encoding="utf-8"))["root"])
BINARY=RELEASE/"app/ModynToolbox.App.exe"
BASE=ROOT/".local/performance"/str(uuid.uuid4())[:8]
BASE.mkdir(parents=True)
kernel=c.WinDLL("kernel32",use_last_error=True)
psapi=c.WinDLL("psapi",use_last_error=True)
class Memory(c.Structure):
    _fields_=[("cb",w.DWORD),("faults",w.DWORD)]+[(key,c.c_size_t) for key in ("peak_ws","ws","peak_paged","paged","peak_nonpaged","nonpaged","pagefile","peak_pagefile","private")]
class ProcessEntry(c.Structure):
    _fields_=[("dwSize",w.DWORD),("cntUsage",w.DWORD),("pid",w.DWORD),("heap",c.c_size_t),("module",w.DWORD),("threads",w.DWORD),("parent",w.DWORD),("priority",w.LONG),("flags",w.DWORD),("name",w.WCHAR*260)]
kernel.OpenProcess.argtypes=[w.DWORD,w.BOOL,w.DWORD];kernel.OpenProcess.restype=w.HANDLE
kernel.CloseHandle.argtypes=[w.HANDLE]
kernel.CreateToolhelp32Snapshot.argtypes=[w.DWORD,w.DWORD];kernel.CreateToolhelp32Snapshot.restype=w.HANDLE
kernel.Process32FirstW.argtypes=[w.HANDLE,c.POINTER(ProcessEntry)]
kernel.Process32NextW.argtypes=[w.HANDLE,c.POINTER(ProcessEntry)]
kernel.GetProcessTimes.argtypes=[w.HANDLE,c.POINTER(w.FILETIME),c.POINTER(w.FILETIME),c.POINTER(w.FILETIME),c.POINTER(w.FILETIME)]
psapi.GetProcessMemoryInfo.argtypes=[w.HANDLE,c.POINTER(Memory),w.DWORD]
def details(pid):
    handle=kernel.OpenProcess(0x410,False,pid)
    if not handle:return None
    try:
        memory=Memory();memory.cb=c.sizeof(memory)
        if not psapi.GetProcessMemoryInfo(handle,c.byref(memory),memory.cb):return None
        created,exited,system,user=(w.FILETIME() for _ in range(4))
        cpu=0
        if kernel.GetProcessTimes(handle,c.byref(created),c.byref(exited),c.byref(system),c.byref(user)):
            cpu=(((system.dwHighDateTime<<32)|system.dwLowDateTime)+((user.dwHighDateTime<<32)|user.dwLowDateTime))/10000000
        return {"private":memory.private/1048576,"ws":memory.ws/1048576,"cpu":cpu,"created":(created.dwHighDateTime<<32)|created.dwLowDateTime}
    finally:kernel.CloseHandle(handle)
def tree(pid):
    snapshot=kernel.CreateToolhelp32Snapshot(2,0)
    if snapshot==c.c_void_p(-1).value:return [pid]
    parents={}
    try:
        entry=ProcessEntry();entry.dwSize=c.sizeof(entry)
        ok=kernel.Process32FirstW(snapshot,c.byref(entry))
        while ok:
            parents[entry.pid]=entry.parent
            ok=kernel.Process32NextW(snapshot,c.byref(entry))
    finally:kernel.CloseHandle(snapshot)
    info=details(pid)
    if not info:return []
    found={pid:info["created"]}
    while True:
        children={}
        for child,parent in parents.items():
            if parent in found and child not in found:
                value=details(child)
                if value and value["created"]>=found[parent]:children[child]=value["created"]
        if not children:return list(found)
        found.update(children)
def sample(name,steps,packages=None):
    folder=BASE/name;folder.mkdir()
    marker=folder/"ready";report=folder/"report.json";script=folder/"script.json"
    steps=steps+[{"op":"mark","path":str(marker),"delay_ms":2200},{"op":"save"}]
    script.write_text(json.dumps({"report":str(report),"steps":steps}),encoding="utf-8")
    env={k:os.environ[k] for k in ("SystemRoot","WINDIR","SystemDrive","COMSPEC") if k in os.environ}
    temp=folder/"temp";temp.mkdir()
    env.update(PATH=os.environ["SystemRoot"]+"/System32",TEMP=str(temp),TMP=str(temp),QT_QPA_PLATFORM="offscreen")
    start=time.perf_counter()
    process=subprocess.Popen([str(BINARY),"--data-root",str(folder/"data"),"--packages-root",str(packages or RELEASE/"tools"),"--automation",str(script)],env=env,cwd=folder,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,creationflags=subprocess.CREATE_NO_WINDOW)
    samples=[];ready=None
    try:
        while process.poll() is None:
            now=time.perf_counter()
            if now-start>90:raise TimeoutError(name)
            if ready is None and marker.exists():ready=now
            main=details(process.pid)
            values=([main] if main else [])+[v for p in tree(process.pid) if p!=process.pid and (v:=details(p))]
            if main:
                samples.append({"seconds":now-start,"host_private":main["private"],"host_ws":main["ws"],"cpu":main["cpu"],"tree_private":sum(v["private"] for v in values),"tree_ws":sum(v["ws"] for v in values),"descendants":max(0,len(values)-1)})
            time.sleep(0.025)
        assert process.returncode==0 and report.exists(),(name,process.returncode)
        result=json.loads(report.read_text(encoding="utf-8"));assert result["success"],result["error"]
    finally:
        if process.poll() is None:process.terminate();process.wait(timeout=5)
    idle=[v for v in samples if ready and v["seconds"]>ready-start+0.3]
    cpu=(idle[-1]["cpu"]-idle[0]["cpu"])/(idle[-1]["seconds"]-idle[0]["seconds"])*100 if len(idle)>1 else 0
    summary={"name":name,"ready_after_s":round(ready-start,3) if ready else None,"samples":len(samples),"sessions":len(result["saved_state"]["sessions"]),"idle_private_mib":round(statistics.median(v["host_private"] for v in idle),2),"idle_ws_mib":round(statistics.median(v["host_ws"] for v in idle),2),"idle_cpu_one_core_percent":round(cpu,3),"idle_cpu_machine_percent":round(cpu/(os.cpu_count() or 1),3),"peak_host_private_mib":round(max(v["host_private"] for v in samples),2),"peak_tree_private_mib":round(max(v["tree_private"] for v in samples),2),"peak_tree_ws_mib":round(max(v["tree_ws"] for v in samples),2),"max_descendants":max(v["descendants"] for v in samples)}
    (folder/"samples.json").write_text(json.dumps(samples,indent=2),encoding="utf-8")
    return summary
results=[]
results.append(sample("idle",[]))
metadata=BASE/"metadata-100"
for number in range(100):
    folder=metadata/("tool-"+str(number).zfill(3))/"1.0.0";folder.mkdir(parents=True)
    (folder/"manifest.json").write_text(json.dumps({"manifest_version":1,"tool_id":folder.parent.name,"package_version":"1.0.0","name":"Registered "+str(number),"icon":"code","view_file":"ui.json"}),encoding="utf-8")
    (folder/"ui.json").write_text(json.dumps({"view_version":1,"kind":"text"}),encoding="utf-8")
results.append(sample("100-metadata-only",[],metadata))
steps=[]
for index in range(20):steps.append({"op":"open","tool":"json","fresh":True})
results.append(sample("20-parked-1-main",steps))
steps=[]
for index in range(24):
    steps.append({"op":"open","tool":"json","fresh":True})
    if index>=19:steps.append({"op":"detach"})
results.append(sample("20-parked-5-floating",steps))
steps=[]
for index in range(50):steps.extend([{"op":"open","tool":"json","fresh":True},{"op":"close","confirmed":True}])
results.append(sample("50-open-close",steps))
large=BASE/"256MiB.bin"
with large.open("wb") as stream:
    block=bytes(1024*1024)
    for _ in range(256):stream.write(block)
steps=[{"op":"open","tool":"file-hash"},{"op":"run","action":"hash","input":{"files":[str(large)],"params":{"algorithm":"SHA-256"}}},
       {"op":"open","tool":"json"},{"op":"input","value":{"text":'"'+"x"*1048576+'"'}},{"op":"run","action":"format"},{"op":"wait","state":"succeeded"}]
results.append(sample("native-plus-python",steps))
# A full result table at the common 2000-row budget.
small=BASE/"table-files";small.mkdir();paths=[]
for number in range(2000):
    path=small/(str(number).zfill(4)+".bin");path.write_bytes(b"");paths.append(str(path))
results.append(sample("2000-result-rows",[{"op":"open","tool":"file-hash"},{"op":"run","action":"hash","input":{"files":paths,"params":{"algorithm":"SHA-256"}}},{"op":"wait","state":"succeeded"}]))
output={"environment":{"os":platform.platform(),"processor":os.environ.get("PROCESSOR_IDENTIFIER"),"logical_cpus":os.cpu_count(),"qt":"6.8.3","configuration":"Release","platform":"Qt offscreen, warm filesystem cache, 25ms sampling","memory_note":"Private bytes are commit charge. Summed working sets can double-count shared DLL pages."},"results":results,"release":str(RELEASE),"release_bytes":sum(p.stat().st_size for p in RELEASE.rglob("*") if p.is_file()),"root":str(BASE)}
(ROOT/".local/logs/performance.json").write_text(json.dumps(output,indent=2),encoding="utf-8")
print(json.dumps(output,ensure_ascii=True))
