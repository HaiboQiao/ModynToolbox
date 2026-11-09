"""Record/check host files before developing an independent extension."""
from pathlib import Path
import argparse
import datetime
import hashlib
import json
ROOT=Path(__file__).resolve().parents[1]
REPORT=ROOT/".local/integration-proof/frozen-host.json"
def files():
    paths=[]
    for folder in ("app","shared",".vscode"):
        paths.extend(p for p in (ROOT/folder).rglob("*") if p.is_file() and "__pycache__" not in p.parts)
    paths.extend(ROOT/p for p in ("CMakeLists.txt","CMakePresets.json","scripts/dev.ps1","scripts/build-tool.py","scripts/runtime.py","scripts/prepare-tools.py","scripts/package.py"))
    binary=ROOT/".local/build/windows-release/bin/Release"
    paths.extend(p for p in binary.rglob("*") if p.is_file() and p.suffix.lower() in (".exe",".dll") and (p.name.startswith("Modyn") or p.suffix.lower()==".dll"))
    return {p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(paths)}
p=argparse.ArgumentParser();p.add_argument("action",choices=("freeze","check"));p.add_argument("--record",type=Path,default=REPORT);a=p.parse_args();REPORT=a.record.resolve()
current=files()
if a.action=="freeze":
    if REPORT.exists():raise RuntimeError("Preserve the first freeze record. Use a new proof directory for another experiment.")
    REPORT.parent.mkdir(parents=True,exist_ok=True)
    REPORT.write_text(json.dumps({"at":datetime.datetime.now(datetime.timezone.utc).isoformat(),"files":current},indent=2),encoding="utf-8")
    print("FROZEN",len(current),"files")
else:
    recorded=json.loads(REPORT.read_text(encoding="utf-8"))
    changed=sorted(k for k in current.keys()|recorded["files"].keys() if current.get(k)!=recorded["files"].get(k))
    report={"at":datetime.datetime.now(datetime.timezone.utc).isoformat(),"frozen_at":recorded["at"],"files":len(current),"changed":changed,"unchanged":not changed}
    (REPORT.parent/"host-hash-check.json").write_text(json.dumps(report,indent=2),encoding="utf-8")
    print(json.dumps(report))
    if changed:raise SystemExit(1)
