from pathlib import Path
import argparse,subprocess,json,shutil,os
SOURCE=Path(__file__).resolve().parent
p=argparse.ArgumentParser();p.add_argument('--sdk',type=Path,required=True);p.add_argument('--qt-root',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
vswhere=Path(os.environ['ProgramFiles(x86)'])/'Microsoft Visual Studio/Installer/vswhere.exe'
vs=Path(subprocess.check_output([str(vswhere),'-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath'],text=True).strip());cmake=vs/'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
build=SOURCE/'.local/build';subprocess.run([str(cmake),'-S',str(SOURCE),'-B',str(build),'-A','x64','-DCMAKE_PREFIX_PATH='+str(a.qt_root.resolve()),'-DMODYN_INTEGRATION_SDK='+str(a.sdk.resolve())],check=True);subprocess.run([str(cmake),'--build',str(build),'--config','Release'],check=True)
runtime=SOURCE/'.local/runtime';runtime.mkdir(parents=True,exist_ok=True);shutil.copy2(a.qt_root/'bin/Qt6Core.dll',runtime/'Qt6Core.dll');redist=sorted((vs/'VC/Redist/MSVC').glob('*/x64/Microsoft.VC*.CRT'))[-1]
for dll in redist.glob('*.dll'):shutil.copy2(dll,runtime/dll.name)
prepared=SOURCE/'.local/package-source';prepared.mkdir(parents=True,exist_ok=True)
for name in ('integration','frontend'):shutil.copytree(SOURCE/name,prepared/name,dirs_exist_ok=True)
shutil.copy2(SOURCE/'LICENSE.txt',prepared/'LICENSE.txt');recipe={'language':'cpp','built_binary':str(build/'Release/ExampleBytes.exe'),'runtime_files':[str(x) for x in runtime.glob('*.dll')],'license_files':[str(a.sdk/'licenses/LGPL-3.0-only.txt'),str(a.sdk/'licenses/Qt-GPL-exception-1.0.txt')]};(prepared/'tool.build.json').write_text(json.dumps(recipe),encoding='utf-8')
subprocess.run([os.sys.executable,str(a.sdk/'scripts/build-tool.py'),str(prepared),'--output',str(a.output.resolve())],check=True)
