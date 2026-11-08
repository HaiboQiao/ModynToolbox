"""Build this independent native tool and its private runtime from any working directory."""
from pathlib import Path
import json
import os
import shutil
import subprocess
import sys
import time
ROOT=Path(__file__).resolve().parents[2]
SOURCE=Path(__file__).resolve().parent
OUTPUT=ROOT/".local/tools/file-hash"
def run(*arguments):
    subprocess.run([str(value) for value in arguments],cwd=ROOT,check=True)
started=time.perf_counter()
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
for dll in redist.glob("*.dll"):
    shutil.copy2(dll,runtime/dll.name)
prepared=OUTPUT/"package-source"
(prepared/"integration").mkdir(parents=True,exist_ok=True)
for name in ("manifest.json","ui.json"):shutil.copy2(SOURCE/"integration"/name,prepared/"integration"/name)
shutil.copy2(SOURCE/"LICENSE.txt",prepared/"LICENSE.txt")
recipe=json.loads((SOURCE/"tool.build.json").read_text(encoding="utf-8"))
recipe["built_binary"]=str(native/"Release/ModynHash.exe")
recipe["runtime_files"]=[str(native/"Release/ModynHashCli.exe"),str(runtime/"Qt6Core.dll")]+[str(runtime/p.name) for p in sorted(redist.glob("*.dll"))]
(prepared/"tool.build.json").write_text(json.dumps(recipe,indent=2),encoding="utf-8")
run(sys.executable,ROOT/"scripts/build-tool.py",prepared)
package=Path(json.loads((OUTPUT/"latest.json").read_text(encoding="utf-8"))["package"])
report={"seconds":round(time.perf_counter()-started,3),"package":str(package),"files":len([p for p in package.rglob("*") if p.is_file()]),"bytes":sum(p.stat().st_size for p in package.rglob("*") if p.is_file())}
(OUTPUT/"build-report.json").write_text(json.dumps(report,indent=2),encoding="utf-8")
print(json.dumps(report))
