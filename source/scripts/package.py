"""Assemble a fresh portable release folder. Existing releases are never overwritten."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import uuid
import re
import datetime
ROOT=Path(__file__).resolve().parents[1]
def project_version():
    match=re.search(r"project\(ModynToolbox\s+VERSION\s+(\d+\.\d+\.\d+)", (ROOT/"CMakeLists.txt").read_text(encoding="utf-8"))
    if not match:raise RuntimeError("Project version missing from CMakeLists.txt")
    return match.group(1)

def package(binary_root, qt_root):
    version=project_version()
    output=ROOT/".local/releases"/("ModynToolbox-"+version+"-"+str(uuid.uuid4())[:8])
    app=output/"app";app.mkdir(parents=True)
    for name in ("ModynToolbox.App.exe","ModynToolbox.WorkerHost.exe"):
        shutil.copy2(binary_root/name,app/name)
    subprocess.run([str(qt_root/"bin/windeployqt.exe"),"--release","--no-translations","--compiler-runtime","--no-system-d3d-compiler",str(app/"ModynToolbox.App.exe")],check=True)
    shutil.copy2(qt_root/"bin/Qt6Concurrent.dll",app/"Qt6Concurrent.dll")
    shutil.copy2(qt_root/"plugins/platforms/qoffscreen.dll",app/"platforms/qoffscreen.dll")
    vswhere=Path(os.environ.get("ProgramFiles(x86)","C:/Program Files (x86)"))/"Microsoft Visual Studio/Installer/vswhere.exe"
    visual_studio=Path(subprocess.check_output([str(vswhere),"-latest","-products","*","-requires","Microsoft.VisualStudio.Component.VC.Tools.x86.x64","-property","installationPath"],text=True).strip())
    candidates=sorted((visual_studio/"VC/Redist/MSVC").glob("*/x64/Microsoft.VC*.CRT"))
    if not candidates:raise RuntimeError("MSVC x64 app-local redistributables missing")
    for runtime in candidates[-1].glob("*.dll"):shutil.copy2(runtime,app/runtime.name)
    # Keep only the database driver used by the application.
    for path in (app/"sqldrivers").glob("*.dll"):
        if path.name!="qsqlite.dll":path.unlink()
    shutil.copy2(binary_root/"ModynToolbox.exe",output/"ModynToolbox.exe")
    for recipe in sorted((ROOT/"tools").glob("*/tool.build.json")):
        record=json.loads((ROOT/".local/tools"/recipe.parent.name/"latest.json").read_text(encoding="utf-8"))
        source=Path(record["package"]);metadata=json.loads((source/"manifest.json").read_text(encoding="utf-8"))
        shutil.copytree(source,output/"tools"/metadata["tool_id"]/metadata["package_version"])
    try:
        git_root=Path(subprocess.check_output(["git","rev-parse","--show-toplevel"],cwd=ROOT,text=True,stderr=subprocess.DEVNULL).strip()).resolve()
        if git_root not in (ROOT, ROOT.parent if ROOT.name=="source" else ROOT):raise RuntimeError("Unrelated repository")
        commit=subprocess.check_output(["git","rev-parse","HEAD"],cwd=ROOT,text=True).strip()
        dirty=bool(subprocess.check_output(["git","status","--porcelain"],cwd=ROOT,text=True).strip())
    except (subprocess.CalledProcessError,RuntimeError):
        revision=ROOT/"SOURCE-REVISION.json"
        commit=json.loads(revision.read_text(encoding="utf-8")).get("commit") if revision.exists() else None
        dirty=None
    metadata={"product":"ModynToolbox","version":version,"platform":"windows-x64","source_commit":commit,"source_dirty":dirty,"built_at":datetime.datetime.now(datetime.timezone.utc).isoformat(),"tools":{}}
    for manifest_path in sorted((output/"tools").glob("*/*/manifest.json")):
        manifest=json.loads(manifest_path.read_text(encoding="utf-8"));metadata["tools"][manifest["tool_id"]]=manifest["package_version"]
    (output/"release-metadata.json").write_text(json.dumps(metadata,indent=2),encoding="utf-8")
    (output/"data").mkdir()
    shutil.copy2(ROOT/"docs/user-guide.md",output/"使用说明.md")
    shutil.copytree(ROOT/"licenses",output/"licenses")
    files={p.relative_to(output).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(output.rglob("*")) if p.is_file()}
    (output/"release-checksums.json").write_text(json.dumps(files,indent=2),encoding="utf-8")
    (ROOT/".local/releases/latest.json").write_text(json.dumps({"root":str(output),"relative_root":output.relative_to(ROOT).as_posix(),"version":version},indent=2),encoding="utf-8")
    print("PORTABLE_RELEASE="+str(output))
    return output
if __name__=="__main__":
    p=argparse.ArgumentParser();p.add_argument("--binary-root",type=Path,required=True);p.add_argument("--qt-root",type=Path,required=True);a=p.parse_args()
    package(a.binary_root.resolve(),a.qt_root.resolve())
