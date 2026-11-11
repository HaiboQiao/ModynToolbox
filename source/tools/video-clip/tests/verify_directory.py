"""Live directory drop/refresh/removal without changing the host executable."""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time,uuid
ROOT=Path(__file__).resolve().parents[3]
parser=argparse.ArgumentParser();parser.add_argument("--binary",type=Path,required=True);args=parser.parse_args();binary=args.binary.resolve()
base=ROOT/".local/video-directory-tests"/str(uuid.uuid4());base.mkdir(parents=True)
package=Path(json.loads((ROOT/".local/tools/video-clip/latest.json").read_text(encoding="utf-8"))["package"])
source=json.loads((ROOT/".local/video-tests/latest.json").read_text(encoding="utf-8"))["sample"]
metadata=json.loads((package/"manifest.json").read_text(encoding="utf-8"))
target=base/"tools"/metadata["tool_id"]/metadata["package_version"]
retained=base/"data/tools/video-clip/user-preference.txt";retained.parent.mkdir(parents=True);retained.write_text("keep",encoding="utf-8")
ready=base/"ready";idle=base/"idle";report=base/"report.json";script=base/"steps.json"
steps=[{"op":"mark","path":str(ready),"delay_ms":7000},{"op":"refresh"},{"op":"open","tool":"video-clip"},
       {"op":"input","value":{"video":{"source":source,"start_ms":1000,"end_ms":3000},"params":{"output_dir":str(base/"outputs")}}},
       {"op":"wait","state":"succeeded"},{"op":"run","action":"export"},{"op":"wait","state":"succeeded"},
       {"op":"close","confirmed":True},{"op":"mark","path":str(idle),"delay_ms":5000},
       {"op":"refresh"},{"op":"open","tool":"video-clip","expect_error":True}]
script.write_text(json.dumps({"report":str(report),"steps":steps}),encoding="utf-8")
env=os.environ.copy();env["QT_QPA_PLATFORM"]="offscreen";env["PATH"]=str(Path(env.get("SystemRoot","C:/Windows"))/"System32")
digest=hashlib.sha256(binary.read_bytes()).hexdigest();log=(base/"host.log").open("wb")
proc=subprocess.Popen([str(binary),"--data-root",str(base/"data"),"--packages-root",str(base/"tools"),"--automation",str(script)],env=env,cwd=base,stdout=log,stderr=log,creationflags=subprocess.CREATE_NO_WINDOW)
def wait(marker):
    deadline=time.monotonic()+60
    while not marker.exists():
        assert proc.poll() is None,"host exited before synchronization marker"
        assert time.monotonic()<deadline,"synchronization timeout"
        time.sleep(.02)
try:
    wait(ready);started=time.perf_counter();shutil.copytree(package,target);copied=time.perf_counter()-started
    wait(idle)
    # Only the exact test-owned package tree is removed after its session/job closes.
    assert target.resolve().is_relative_to(base.resolve()) and target.name==metadata["package_version"]
    shutil.rmtree(target)
    assert proc.wait(timeout=60)==0
    result=json.loads(report.read_text(encoding="utf-8"));assert result["success"],result
    assert retained.read_text(encoding="utf-8")=="keep"
    assert digest==hashlib.sha256(binary.read_bytes()).hexdigest()
    summary={"success":True,"steps":len(steps),"copy_seconds":round(copied,3),"host_unchanged":True,"tool_data_retained":True,"root":str(base)}
    (base/"summary.json").write_text(json.dumps(summary,indent=2),encoding="utf-8");print(json.dumps(summary,indent=2))
finally:
    if proc.poll() is None:proc.kill();proc.wait()
    log.close()
