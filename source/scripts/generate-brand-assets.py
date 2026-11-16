from pathlib import Path
import os,json,subprocess
ROOT=Path(__file__).resolve().parents[1]
base=ROOT/'.local/brand-generator';src=base/'src';src.mkdir(parents=True,exist_ok=True)
(src/'main.cpp').write_text('#include "ui/components/icons.h"\n#include <QGuiApplication>\nint main(int argc,char **argv){QGuiApplication app(argc,argv);auto icon=brandIcon();return icon.pixmap(256).save(argv[1],"ICO")&&icon.pixmap(256).save(argv[2],"PNG")?0:1;}\n',encoding='utf-8')
(src/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.24)\nproject(BrandAssets LANGUAGES CXX)\nset(CMAKE_CXX_STANDARD 20)\nfind_package(Qt6 REQUIRED COMPONENTS Gui)\nadd_executable(BrandAssets main.cpp "'+(ROOT/'app/ui/components/icons.cpp').as_posix()+'")\ntarget_include_directories(BrandAssets PRIVATE "'+(ROOT/'app').as_posix()+'")\ntarget_link_libraries(BrandAssets PRIVATE Qt6::Gui)\n',encoding='utf-8')
vswhere=Path(os.environ.get('ProgramFiles(x86)','C:/Program Files (x86)'))/'Microsoft Visual Studio/Installer/vswhere.exe';vs=Path(subprocess.check_output([str(vswhere),'-latest','-products','*','-property','installationPath'],text=True).strip());cmake=vs/'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe';qt=Path(os.environ.get('MODYN_QT_ROOT',str(ROOT/'.local/toolchains/Qt/6.8.3/msvc2022_64')))
subprocess.run([str(cmake),'-S',str(src),'-B',str(base/'build'),'-DCMAKE_PREFIX_PATH='+str(qt)],check=True)
subprocess.run([str(cmake),'--build',str(base/'build'),'--config','Release'],check=True)
env=os.environ.copy();env.update(PATH=str(qt/'bin')+os.pathsep+env.get('PATH',''),QT_QPA_PLATFORM='offscreen',QT_PLUGIN_PATH=str(qt/'plugins'))
subprocess.run([str(base/'build/Release/BrandAssets.exe'),str(ROOT/'app/resources/modyn.ico'),str(ROOT/'app/resources/logo.png')],env=env,check=True)
