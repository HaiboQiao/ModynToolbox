"""Send the real JSONL cancellation only after the real encoder has written video bytes."""
from pathlib import Path
from ctypes import wintypes as w
import ctypes as c,json,os,subprocess,time,uuid,argparse
ROOT=Path(__file__).resolve().parents[3]
parser=argparse.ArgumentParser();parser.add_argument("--tool-package",type=Path);args=parser.parse_args()
package=args.tool_package.resolve() if args.tool_package else Path(json.loads((ROOT/".local/tools/video-clip/latest.json").read_text(encoding="utf-8"))["package"])
base=ROOT/".local/video-cancel-tests"/str(uuid.uuid4());base.mkdir(parents=True)
source=base/"source.mp4"
subprocess.run([str(package/"backend/ffmpeg.exe"),"-v","error","-f","lavfi","-i","testsrc2=size=1920x1080:rate=30","-t","20","-c:v","libx264","-threads","2","-preset","ultrafast","-crf","18",str(source)],check=True,capture_output=True,creationflags=subprocess.CREATE_NO_WINDOW)
class Basic(c.Structure):
    _fields_=[("process_time",c.c_int64),("job_time",c.c_int64),("flags",w.DWORD),("minimum_ws",c.c_size_t),("maximum_ws",c.c_size_t),("active_limit",w.DWORD),("affinity",c.c_size_t),("priority",w.DWORD),("scheduling",w.DWORD)]
class IO(c.Structure):_fields_=[(n,c.c_ulonglong) for n in ("read_ops","write_ops","other_ops","read_bytes","write_bytes","other_bytes")]
class Limits(c.Structure):_fields_=[("basic",Basic),("io",IO),("process_memory",c.c_size_t),("job_memory",c.c_size_t),("peak_process",c.c_size_t),("peak_job",c.c_size_t)]
k=c.WinDLL("kernel32",use_last_error=True)
k.CreateJobObjectW.argtypes=[c.c_void_p,w.LPCWSTR];k.CreateJobObjectW.restype=w.HANDLE
k.SetInformationJobObject.argtypes=[w.HANDLE,c.c_int,c.c_void_p,w.DWORD];k.CloseHandle.argtypes=[w.HANDLE]
k.AssignProcessToJobObject.argtypes=[w.HANDLE,w.HANDLE]
job=k.CreateJobObjectW(None,None);limits=Limits();limits.basic.flags=0x2000
assert k.SetInformationJobObject(job,9,c.byref(limits),c.sizeof(limits))
run={"protocol":1,"kind":"run","tool_id":"video-clip","session_id":"cancel-proof","job_id":"cancel-proof","revision":1,"action":"export","job_root":str(base),"data_root":str(base/"data"),"dynamic_resources":True}
(base/"input.json").write_text(json.dumps({"files":[str(source)],"params":{"start_ms":0,"end_ms":20000,"output_dir":str(base/"outputs")}}),encoding="utf-8")
p=subprocess.Popen([str(package/"backend/ModynVideo.exe")],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,creationflags=subprocess.CREATE_NO_WINDOW,cwd=base)
try:
    assert k.AssignProcessToJobObject(job,w.HANDLE(p._handle))
    p.stdin.write((json.dumps(run)+"\n").encode());p.stdin.flush()
    deadline=time.monotonic()+30;writing=None
    while time.monotonic()<deadline and p.poll() is None:
        files=list((base/"outputs").glob(".modyn-video-*.mp4"))
        if files and files[0].stat().st_size>20000:writing=files[0];break
        time.sleep(.02)
    assert writing is not None,"encoder did not reach actual file writing"
    started=time.monotonic();p.stdin.write((json.dumps({"kind":"cancel","job_id":"cancel-proof"})+"\n").encode());p.stdin.flush()
    stdout,stderr=p.communicate(timeout=15);delay=time.monotonic()-started
    result=json.loads((base/"result.json").read_text(encoding="utf-8"));assert p.returncode==0 and result["status"]=="cancelled",result
    assert not list((base/"outputs").glob("*.mp4")) and not list((base/"outputs").glob(".modyn-video-*"))
    summary={"success":True,"cancel_after_actual_encoding":True,"cancel_seconds":round(delay,3),"temporary_removed":True,"root":str(base)}
    (base/"summary.json").write_text(json.dumps(summary,indent=2),encoding="utf-8");print(json.dumps(summary,indent=2))
finally:
    k.CloseHandle(job)
    if p.poll() is None:p.wait(timeout=10)
