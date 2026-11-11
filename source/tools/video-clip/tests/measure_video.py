"""Reuse the project's Windows sampler against a local validation package."""
from pathlib import Path
import argparse,json
ROOT=Path(__file__).resolve().parents[3]
parser=argparse.ArgumentParser();parser.add_argument("--package",type=Path,required=True);parser.add_argument("--baseline",type=Path);args=parser.parse_args()
# Only the existing sampler definitions are evaluated; its original scenario run is excluded.
source=(ROOT/"scripts/measure-resources.py").read_text(encoding="utf-8").split("results=[]",1)[0]
space={"__file__":str(ROOT/"scripts/measure-resources.py")};exec(compile(source,"measure-resources.py","exec"),space)
space["RELEASE"]=args.package.resolve();space["BINARY"]=space["RELEASE"]/"app/ModynToolbox.App.exe"
sample=space["sample"]
media=json.loads((ROOT/".local/video-tests/latest.json").read_text(encoding="utf-8"))["sample"]
base=[{"op":"open","tool":"video-clip"},{"op":"input","value":{"video":{"source":media,"start_ms":0,"end_ms":6000}}},{"op":"wait","state":"succeeded"}]
import shutil,os,platform,hashlib
baseline_tools=space["BASE"]/"baseline-tools";baseline_tools.mkdir()
for folder in (space["RELEASE"]/"tools").iterdir():
    if folder.is_dir() and folder.name!="video-clip":shutil.copytree(folder,baseline_tools/folder.name)
results=[]
new_binary=space["BINARY"]
if args.baseline:
    space["BINARY"]=args.baseline.resolve()/"app/ModynToolbox.App.exe"
    results.append(sample("previous-public-host-idle",[],packages=baseline_tools))
    space["BINARY"]=new_binary
results.append(sample("new-host-without-video-package",[],packages=baseline_tools))
results += [sample("installed-idle",[]),sample("loaded-paused",base),sample("playing",base+[{"op":"preview","play":True,"delay_ms":200}]),sample("paused-after-play",base+[{"op":"preview","play":True,"delay_ms":500},{"op":"preview","play":False}]),sample("exporting",base+[{"op":"run","action":"export"},{"op":"wait","state":"succeeded"}])]
report={"environment":{"platform":"Windows x64, Release, Qt offscreen, warm cache; generated 640x360 25fps 6s sample", "memory":"Private commit, complete descendant tree; these are observations, not universal budgets", "processor":os.environ.get("PROCESSOR_IDENTIFIER"),"logical_cpus":os.cpu_count(),"os":platform.platform(),"host_sha256":hashlib.sha256(new_binary.read_bytes()).hexdigest()},"results":results,"root":str(space["BASE"])}
path=space["BASE"]/"video-summary.json";path.write_text(json.dumps(report,indent=2),encoding="utf-8");print(json.dumps(report,indent=2));print("REPORT",path)
