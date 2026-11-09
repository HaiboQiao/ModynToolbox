"""Build changed independent tools and install them through the actual application."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import time
import uuid
ROOT=Path(__file__).resolve().parents[1]
def fingerprint(source):
    digest=hashlib.sha256()
    for base in (source,ROOT/"shared/sdk"):
        for path in sorted(base.rglob("*")):
            if path.is_file() and not any(p in (".local","__pycache__") for p in path.relative_to(base).parts):
                digest.update(path.relative_to(base).as_posix().encode())
                digest.update(path.read_bytes())
    return digest.hexdigest()
def prepare(binary, packages):
    spec=importlib.util.spec_from_file_location("builder",ROOT/"scripts/build-tool.py")
    builder=importlib.util.module_from_spec(spec);spec.loader.exec_module(builder)
    operations=[]
    for recipe in sorted((ROOT/"tools").glob("*/tool.build.json")):
        source=recipe.parent
        latest=ROOT/".local/tools"/source.name/"latest.json"
        record=json.loads(latest.read_text(encoding="utf-8")) if latest.exists() else {}
        value=fingerprint(source)
        if record.get("fingerprint")!=value or not Path(record.get("package","missing")).is_dir():
            recipe=json.loads((source/"tool.build.json").read_text(encoding="utf-8"))
            if recipe.get("build_script"):
                script=source/recipe["build_script"]
                if not script.resolve().is_relative_to(source.resolve()):raise ValueError("Tool build script must stay within its source directory")
                subprocess.run([sys.executable,str(script)],cwd=ROOT,check=True)
            else:
                builder.build(source)
            record=json.loads(latest.read_text(encoding="utf-8"));record["fingerprint"]=value
            latest.write_text(json.dumps(record,indent=2),encoding="utf-8")
        package=Path(record["package"])
        manifest=json.loads((package/"manifest.json").read_text(encoding="utf-8"))
        destination=packages/manifest["tool_id"]/manifest["package_version"]
        if destination.exists():
            existing=json.loads((destination/"manifest.json").read_text(encoding="utf-8"))
            if existing.get("integrity_sha256")==manifest["integrity_sha256"]:
                continue
            raise RuntimeError("Installed same-version content differs. Remove the idle installed version or increment version: "+str(destination))
        operations.append({"op":"install","path":str(package)})
    if not operations:
        print("All tool packages are current")
        return
    folder=ROOT/".local/bootstrap"/str(uuid.uuid4());folder.mkdir(parents=True)
    script=folder/"install.json";report=folder/"report.json"
    script.write_text(json.dumps({"report":str(report),"steps":operations}),encoding="utf-8")
    env=os.environ.copy();env["QT_QPA_PLATFORM"]="offscreen"
    result=subprocess.run([str(binary),"--data-root",str(folder/"data"),"--packages-root",str(packages),"--automation",str(script)],env=env,cwd=ROOT,creationflags=subprocess.CREATE_NO_WINDOW,timeout=120,capture_output=True)
    if result.returncode:
        raise RuntimeError("Tool install failed. Read "+str(report)+" "+result.stderr.decode("utf-8",errors="replace")[:1000])
    print("Installed",len(operations),"tool packages")
if __name__=="__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("--binary",type=Path,required=True)
    parser.add_argument("--packages",type=Path,default=ROOT/".local/installed/tools")
    args=parser.parse_args();prepare(args.binary.resolve(),args.packages.resolve())
