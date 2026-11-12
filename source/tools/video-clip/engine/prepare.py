"""Fetch verified source inputs and run the reproducible engine recipe in a private MSYS2."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess,urllib.request
ROOT=Path(__file__).resolve().parents[3]
parser=argparse.ArgumentParser();parser.add_argument('--msys-root',type=Path,default=ROOT/'.local/toolchains/msys64');args=parser.parse_args()
lock=json.loads(Path(__file__).with_name('inputs.lock.json').read_text(encoding='utf-8'))
folder=ROOT/'.local/media-source';folder.mkdir(parents=True,exist_ok=True)
for item in lock['inputs']:
    target=folder/item['name']
    if not target.is_file() or hashlib.sha256(target.read_bytes()).hexdigest()!=item['sha256']:
        with urllib.request.urlopen(item['url'],timeout=180) as response,target.open('wb') as stream:
            while block:=response.read(1024*1024):stream.write(block)
    if hashlib.sha256(target.read_bytes()).hexdigest()!=item['sha256']:raise ValueError('Engine source mismatch: '+item['name'])
env=os.environ.copy();env['MSYSTEM']='UCRT64'
temporary=ROOT/'.local/media-engine/temp';temporary.mkdir(parents=True,exist_ok=True)
env['TEMP']=env['TMP']=env['TMPDIR']=str(temporary)
subprocess.run([str(args.msys_root/'usr/bin/bash.exe'),'--noprofile','--norc',str(Path(__file__).with_name('build-msys2.sh')),str(ROOT)],env=env,cwd=ROOT,check=True)
