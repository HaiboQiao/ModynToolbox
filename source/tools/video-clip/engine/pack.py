"""Package a locally rebuilt engine and lock it for a future release. Never changes remote assets."""
from pathlib import Path
import argparse,hashlib,json,shutil,zipfile,subprocess
ROOT=Path(__file__).resolve().parents[3]
parser=argparse.ArgumentParser();parser.add_argument('--release-version',required=True);parser.add_argument('--engine-version',default='9.0.2-modyn1');parser.add_argument('--replace-unpublished',action='store_true');a=parser.parse_args()
dist=ROOT/'.local/media-engine/dist'
imports={}
objdump=ROOT/'.local/toolchains/msys64/ucrt64/bin/objdump.exe'
for name in ['ffmpeg.exe','ffprobe.exe']:
    output=subprocess.check_output([str(objdump),'-p',str(dist/name)],text=True,creationflags=subprocess.CREATE_NO_WINDOW)
    imports[name]=[line.split('DLL Name:',1)[1].strip() for line in output.splitlines() if 'DLL Name:' in line]
    if any(n.lower().startswith(('libgcc','libwinpthread','libstdc++','msys-')) for n in imports[name]):raise ValueError('Processing engine has a non-system runtime dependency')
(dist/'runtime-imports.json').write_text(json.dumps(imports,indent=2),encoding='utf-8')

archive=ROOT/'.local/downloads'/('ModynToolbox-media-engine-'+a.engine_version+'-windows-x64.zip')
if archive.exists() and not a.replace_unpublished:raise FileExistsError('Use a new engine version, or explicitly replace an unpublished local draft')
source_url='https://github.com/HaiboQiao/ModynToolbox/releases/download/v'+a.release_version+'/ModynToolbox-'+a.release_version+'-third-party-source.zip'
(dist/'README.txt').write_text('ModynToolbox media processing engine '+a.engine_version+'\n\nExact FFmpeg/x264 source inputs are in inputs.lock.json. Only libx264 is an external media library; networking, device capture and unused encoders are disabled. Internal lavfi is retained for validation. Binaries are GPL-3.0-or-later.\n\nCorresponding source/build material: '+source_url+'\n\n'+(dist/'build-version.txt').read_text(encoding='utf-8'),encoding='utf-8')
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
    for p in sorted(dist.glob('*')):
        if p.is_file():z.write(p,'media-engine/'+p.name)
lock={'url':'https://github.com/HaiboQiao/ModynToolbox/releases/download/v'+a.release_version+'/'+archive.name,'version':a.engine_version,'sha256':hashlib.sha256(archive.read_bytes()).hexdigest(),'files':{n:hashlib.sha256((dist/n).read_bytes()).hexdigest() for n in ['ffmpeg.exe','ffprobe.exe']},'publisher':'ModynToolbox rebuildable source build','source_inputs':'engine/inputs.lock.json','source_bundle':source_url,'license':'GPL-3.0-or-later'}
(ROOT/'tools/video-clip/dependencies.lock.json').write_text(json.dumps(lock,indent=2)+'\n',encoding='utf-8')
deps=ROOT/'.local/tools/video-clip/deps';deps.mkdir(parents=True,exist_ok=True)
for name in ['ffmpeg.exe','ffprobe.exe','LICENSE','README.txt']:shutil.copy2(dist/name,deps/name)
print(json.dumps({'archive':str(archive),'bytes':archive.stat().st_size,'sha256':lock['sha256']},indent=2))
