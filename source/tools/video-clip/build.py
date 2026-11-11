"""Build independent video tool, restoring only its pinned private FFmpeg dependencies."""
from pathlib import Path
import hashlib,json,os,shutil,subprocess,sys,urllib.request,zipfile,time
ROOT=Path(__file__).resolve().parents[2]
SOURCE=Path(__file__).resolve().parent
OUTPUT=ROOT/".local/tools/video-clip"
def run(*args):subprocess.run([str(a) for a in args],cwd=ROOT,check=True)
lock=json.loads((SOURCE/"dependencies.lock.json").read_text(encoding="utf-8"))
deps=OUTPUT/"deps";deps.mkdir(parents=True,exist_ok=True)
if any(not (deps/n).is_file() or hashlib.sha256((deps/n).read_bytes()).hexdigest()!=v for n,v in lock["files"].items()):
    archive=ROOT/".local/downloads"/Path(lock["url"]).name
    archive.parent.mkdir(parents=True,exist_ok=True)
    if not archive.exists() or hashlib.sha256(archive.read_bytes()).hexdigest()!=lock["sha256"]:
        with urllib.request.urlopen(lock["url"],timeout=120) as response,archive.open("wb") as output:
            while chunk:=response.read(1024*1024):output.write(chunk)
    if hashlib.sha256(archive.read_bytes()).hexdigest()!=lock["sha256"]:raise ValueError("FFmpeg archive hash mismatch")
    with zipfile.ZipFile(archive) as z:
        for name in z.namelist():
            if name.endswith(("/ffmpeg.exe","/ffprobe.exe","/LICENSE","/README.txt")):(deps/Path(name).name).write_bytes(z.read(name))
for name,digest in lock["files"].items():
    if hashlib.sha256((deps/name).read_bytes()).hexdigest()!=digest:raise ValueError("FFmpeg executable hash mismatch")
vswhere=Path(os.environ.get("ProgramFiles(x86)","C:/Program Files (x86)"))/"Microsoft Visual Studio/Installer/vswhere.exe"
vs=Path(subprocess.check_output([str(vswhere),"-latest","-products","*","-requires","Microsoft.VisualStudio.Component.VC.Tools.x86.x64","-property","installationPath"],text=True).strip())
cmake=vs/"Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
qt=Path(os.environ.get("MODYN_QT_ROOT",str(ROOT/".local/toolchains/Qt/6.8.3/msvc2022_64")))
native=OUTPUT/"native"
run(cmake,"-S",SOURCE,"-B",native,"-A","x64","-DCMAKE_PREFIX_PATH="+str(qt))
run(cmake,"--build",native,"--config","Release")
runtime=OUTPUT/"runtime";runtime.mkdir(parents=True,exist_ok=True)
shutil.copy2(qt/"bin/Qt6Core.dll",runtime/"Qt6Core.dll")
redist=sorted((vs/"VC/Redist/MSVC").glob("*/x64/Microsoft.VC*.CRT"))[-1]
for dll in redist.glob("*.dll"):shutil.copy2(dll,runtime/dll.name)
for item in [*deps.glob("*.exe"),SOURCE/"dependencies.lock.json"]:shutil.copy2(item,native/"Release"/item.name)
shutil.copy2(runtime/"Qt6Core.dll",native/"Release/Qt6Core.dll")
for dll in runtime.glob("*.dll"):shutil.copy2(dll,native/"Release"/dll.name)
prepared=OUTPUT/"package-source";(prepared/"integration").mkdir(parents=True,exist_ok=True)
for name in ("manifest.json","ui.json"):shutil.copy2(SOURCE/"integration"/name,prepared/"integration"/name)
(prepared/"LICENSE.txt").write_text((SOURCE/"LICENSE.txt").read_text(encoding="utf-8")+"\n\nFFmpeg build license\n"+(deps/"LICENSE").read_text(encoding="utf-8"),encoding="utf-8")
recipe={"language":"cpp","built_binary":str(native/"Release/ModynVideo.exe"),"runtime_files":[str(native/"Release/ModynVideoCli.exe"),str(SOURCE/"dependencies.lock.json"),str(deps/"README.txt")]+[str(p) for p in sorted(runtime.glob("*.dll"))]+[str(deps/n) for n in lock["files"]]}
recipe["license_files"]=[str(ROOT/"licenses/LGPL-3.0-only.txt"),str(ROOT/"licenses/GPL-3.0-only.txt"),str(SOURCE/"licenses/README.md")]
(prepared/"tool.build.json").write_text(json.dumps(recipe,indent=2),encoding="utf-8")
run(sys.executable,ROOT/"scripts/build-tool.py",prepared)
print("Video tool build complete")
