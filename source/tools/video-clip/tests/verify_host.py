"""Exercise a frozen host through actual package installation, jobs, cancellation and crash recovery."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess,time,uuid
ROOT=Path(__file__).resolve().parents[3]
parser=argparse.ArgumentParser();parser.add_argument("--binary",type=Path,required=True);parser.add_argument("--tool-package",type=Path);args=parser.parse_args()
BINARY=args.binary.resolve()
BASE=ROOT/".local/video-host-tests"/str(uuid.uuid4());BASE.mkdir(parents=True)
PACKAGE=args.tool_package.resolve() if args.tool_package else Path(json.loads((ROOT/".local/tools/video-clip/latest.json").read_text(encoding="utf-8"))["package"])
SAMPLE=Path(json.loads((ROOT/".local/video-tests/latest.json").read_text(encoding="utf-8"))["sample"])
ENV=os.environ.copy();ENV["QT_QPA_PLATFORM"]="offscreen"
ENV["PATH"]=str(Path(ENV.get("SystemRoot","C:/Windows"))/"System32")
ENV["TEMP"]=ENV["TMP"]=str(BASE/"temp");(BASE/"temp").mkdir()
initial_hash=hashlib.sha256(BINARY.read_bytes()).hexdigest()
reports=[]
def execute(name,steps,data=None,packages=None,spawn=False):
    report=BASE/(name+"-report.json");script=BASE/(name+".json")
    script.write_text(json.dumps({"report":str(report),"steps":steps}),encoding="utf-8")
    out=(BASE/(name+".log")).open("wb")
    p=subprocess.Popen([str(BINARY),"--data-root",str(data or BASE/"data"),"--packages-root",str(packages or BASE/"tools"),"--automation",str(script)],cwd=BASE,env=ENV,stdout=out,stderr=out,creationflags=subprocess.CREATE_NO_WINDOW)
    if spawn:return p,out,report
    try:
        code=p.wait(timeout=120)
    finally:out.close()
    result=json.loads(report.read_text(encoding="utf-8")) if report.exists() else {}
    assert code==0 and result.get("success"),(name,code,result)
    reports.append({"name":name,"steps":len(result["steps"]),"report":str(report)})
    print(name,"PASS",len(result["steps"]),flush=True)
    return result
def selection(start=1240,end=4240,source=SAMPLE):
    return {"video":{"source":str(source),"start_ms":start,"end_ms":end},"params":{"mode":"精确裁剪","quality":"均衡","size":"原尺寸","output_dir":str(BASE/"outputs")}}
try:
    first=execute("install-export-sessions",[
        {"op":"install","path":str(PACKAGE)}, {"op":"open","tool":"video-clip"},
        {"op":"input","value":selection()}, {"op":"wait","state":"succeeded"},
        {"op":"run","action":"export"},{"op":"wait","state":"succeeded"},
        {"op":"expect","result_contains":"3000"},{"op":"detach"},{"op":"expect","detached":True},{"op":"attach"},
        {"op":"open","tool":"video-clip","fresh":True},{"op":"input","value":selection(2000,4000)},
        {"op":"wait","state":"succeeded"},{"op":"run","action":"export"},{"op":"wait","state":"succeeded"},
        {"op":"expect","result_contains":"2000"},{"op":"expect","sessions":2},
        {"op":"draft","enabled":True},{"op":"save"}, {"op":"screenshot","path":str(BASE/"actual-main.png")}
    ])
    second=execute("draft-restart",[{"op":"open","tool":"video-clip"},{"op":"wait","state":"succeeded"},{"op":"run","action":"export"},{"op":"wait","state":"succeeded"},{"op":"expect","result_contains":"2000"}])
    # Generate a larger fixture so cancellation/crash happen inside actual encoding, not a mock process.
    long=BASE/"long-video.mp4"
    ff=PACKAGE/"backend/ffmpeg.exe"
    subprocess.run([str(ff),"-v","error","-nostdin","-f","lavfi","-i","testsrc2=size=1920x1080:rate=30","-t","20","-c:v","libx264","-threads","2","-preset","ultrafast","-crf","18",str(long)],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE,creationflags=subprocess.CREATE_NO_WINDOW)
    third=execute("cancel-real-job",[{"op":"open","tool":"video-clip"},{"op":"input","value":selection(0,20000,long)},{"op":"wait","state":"succeeded"},{"op":"run","action":"export","delay_ms":400},{"op":"cancel"},{"op":"wait","state":"cancelled"}])
    # Only this test's child is terminated. Windows Job Object owns its encoder process tree.
    p,log,report=execute("crash-real-job",[{"op":"open","tool":"video-clip"},{"op":"input","value":selection(0,20000,long)},{"op":"wait","state":"succeeded"},{"op":"run","action":"export"},{"op":"wait","timeout_ms":120000}],spawn=True)
    until=time.monotonic()+30;temporary=None
    while time.monotonic()<until and p.poll() is None:
        paths=list((BASE/"outputs").glob(".modyn-video-*.mp4"))
        if paths and paths[0].stat().st_size>20000:
            temporary=paths[0];p.kill();p.wait(timeout=10);break
        time.sleep(.02)
    if p.poll() is None:p.kill();p.wait()
    log.close();assert temporary is not None,"encoder never produced a temporary before crash"
    assert temporary.exists(),"crash fixture disappeared unexpectedly"
    recovery=execute("recover-owned-temporary",[{"op":"open","tool":"video-clip"},{"op":"input","value":selection()},{"op":"wait","state":"succeeded"}])
    assert not temporary.exists(),"owned temporary was not recovered"
    assert not list((BASE/"outputs").glob(".modyn-video-*"))
    assert initial_hash==hashlib.sha256(BINARY.read_bytes()).hexdigest(),"host changed during tool integration"
    result={"success":True,"reports":reports,"crash_recovery":True,"host_sha256":initial_hash,"host_unchanged":True,"root":str(BASE)}
except Exception as e:
    result={"success":False,"reports":reports,"error":str(e),"root":str(BASE)}
    raise
finally:
    (BASE/"summary.json").write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding="utf-8")
    print("SUMMARY",BASE/"summary.json",flush=True)
