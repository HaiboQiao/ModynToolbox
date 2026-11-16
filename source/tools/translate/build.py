"""Build a complete, immutable offline tool using pinned public model/wheel artifacts."""
from pathlib import Path
import importlib.util,json,hashlib,shutil,zipfile,sys,subprocess,urllib.request
ROOT=Path(__file__).resolve().parents[2];SOURCE=Path(__file__).resolve().parent
lock=json.loads((SOURCE/'offline.lock.json').read_text(encoding='utf-8'));cache=ROOT/'.local/offline-translation';cache.mkdir(parents=True,exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def artifact(a):
    p=cache/a['filename']
    if not p.exists():
        temp=p.with_suffix(p.suffix+'.download');proxy=urllib.request.getproxies().get('https')
        args=['curl.exe']+(['--proxy',proxy] if proxy else [])+['--location','--fail','--retry','3','--connect-timeout','30','--max-time','1200','--output',str(temp),a['url']]
        subprocess.run(args,check=True);temp.replace(p)
    if sha(p)!=a['sha256']:raise ValueError('Artifact checksum mismatch: '+p.name)
    return p
for a in lock['wheels']+lock['models']:artifact(a)
spec=importlib.util.spec_from_file_location('builder',ROOT/'scripts/build-tool.py');builder=importlib.util.module_from_spec(spec);spec.loader.exec_module(builder);target=builder.build(SOURCE)
site=target/'runtime/Lib/site-packages';site.mkdir(parents=True,exist_ok=True)
for a in lock['wheels']:
    with zipfile.ZipFile(artifact(a)) as z:
        if any('..' in Path(n).parts or Path(n).is_absolute() for n in z.namelist()):raise ValueError('Unsafe wheel path')
        if z.testzip():raise ValueError('Wheel CRC failure')
        z.extractall(site)
for a in lock['models']:
    with zipfile.ZipFile(artifact(a)) as z:
        prefix=next(n for n in z.namelist() if n.endswith('/metadata.json')).rsplit('/',1)[0]+'/'
        for name in z.namelist():
            rel=name.removeprefix(prefix)
            if name.startswith(prefix) and not name.endswith('/') and (rel.startswith('model/') or rel in ('sentencepiece.model','metadata.json','README.md')):
                if '..' in Path(rel).parts:raise ValueError('Unsafe model path')
                dst=target/'backend/models'/a['direction']/rel;dst.parent.mkdir(parents=True,exist_ok=True);dst.write_bytes(z.read(name))
shutil.copy2(SOURCE/'offline.lock.json',target/'offline.lock.json')
shutil.copytree(SOURCE/'licenses',target/'LICENSES/offline',dirs_exist_ok=True)
checks={p.relative_to(target).as_posix():sha(p) for p in sorted(target.rglob('*')) if p.is_file() and p.name not in ('manifest.json','checksums.json')}
# Include model metadata even when named manifest/checksums in the future: exclusions refer only to top level.
checks={p.relative_to(target).as_posix():sha(p) for p in sorted(target.rglob('*')) if p.is_file() and p not in (target/'manifest.json',target/'checksums.json')}
builder.write_json(target/'checksums.json',checks)
m=json.loads((target/'manifest.json').read_text(encoding='utf-8'));m['integrity_sha256']=sha(target/'checksums.json');builder.write_json(target/'manifest.json',m)
print('OFFLINE_TOOL_PACKAGE='+str(target))
