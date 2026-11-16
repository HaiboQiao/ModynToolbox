"""Install the project's pinned SDK/runtime locally, verifying all archive hashes."""
from pathlib import Path
import argparse,hashlib,json,subprocess,urllib.request,zipfile,time
ROOT=Path(__file__).resolve().parents[1]

def fetch(url,path,digest):
    path.parent.mkdir(parents=True,exist_ok=True)
    if path.exists() and hashlib.sha256(path.read_bytes()).hexdigest()==digest:return
    for attempt in range(3):
        try:
            with urllib.request.urlopen(url,timeout=120) as src,path.open('wb') as dst:
                while block:=src.read(1024*1024):dst.write(block)
            if hashlib.sha256(path.read_bytes()).hexdigest()!=digest:raise ValueError('Archive hash mismatch')
            return
        except Exception:
            if attempt==2:raise
            time.sleep(2)

def setup(sdk_only=False):
    lock=json.loads((ROOT/'shared/frontend-sdk/dependencies.lock.json').read_text(encoding='utf-8'))
    sdk=lock['sdk'];base=ROOT/'.local/toolchains/WebView2';archive=base/Path(sdk['url']).name
    fetch(sdk['url'],archive,sdk['sha256']);target=base/sdk['version']
    with zipfile.ZipFile(archive) as z:
        for item in z.infolist():
            if not (target/item.filename).resolve().is_relative_to(target.resolve()):raise ValueError('Archive path escape')
        z.extractall(target)
    (base/'sdk.json').write_text(json.dumps({**sdk,'root':str(target)},indent=2),encoding='utf-8')
    if sdk_only:return
    runtime=lock['runtime'];base=ROOT/'.local/toolchains/WebView2Runtime';archive=base/Path(runtime['url']).name
    fetch(runtime['url'],archive,runtime['sha256']);target=base/runtime['version'];target.mkdir(exist_ok=True)
    expected=target/('Microsoft.WebView2.FixedVersionRuntime.'+runtime['version']+'.x64')
    if not (expected/'msedgewebview2.exe').exists():subprocess.run(['expand.exe',str(archive),'-F:*',str(target)],check=True)
    if not (expected/'msedgewebview2.exe').is_file():raise ValueError('Runtime missing')
    (base/'runtime.json').write_text(json.dumps({**runtime,'root':str(expected)},indent=2),encoding='utf-8')
    print('Verified local WebView2 SDK and Fixed Runtime')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--sdk-only',action='store_true');a=p.parse_args();setup(a.sdk_only)
