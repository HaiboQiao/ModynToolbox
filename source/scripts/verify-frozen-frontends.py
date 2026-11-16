"""Build two tool-owned frontends/backends using only an exported SDK against a frozen host."""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,sys,time,uuid
ROOT=Path(__file__).resolve().parents[1]
def digest(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def freeze(binary=None):
    binary=Path(binary or ROOT/'.local/build/windows-release/bin/Release/ModynToolbox.App.exe').resolve()
    if not binary.is_file():raise FileNotFoundError('Build the Release host before freezing: '+str(binary))
    base=ROOT/'.local/p4'/str(uuid.uuid4())[:8];base.mkdir(parents=True,exist_ok=False)
    paths=[p for p in (ROOT/'app').rglob('*') if p.is_file() and 'tests' not in p.relative_to(ROOT/'app').parts]+[ROOT/'CMakeLists.txt',ROOT/'shared/frontend-sdk/modyn.js']
    record={'root':str(base),'host_binary':str(binary),'host_sha256':digest(binary),'host_sources':{p.relative_to(ROOT).as_posix():digest(p) for p in paths}}
    (base/'freeze.json').write_text(json.dumps(record,indent=2),encoding='utf-8')
    pointer=ROOT/'.local/independent-proof/latest.json';pointer.parent.mkdir(parents=True,exist_ok=True);pointer.write_text(json.dumps(record,indent=2),encoding='utf-8')
    return record

def run():
    record=json.loads((ROOT/'.local/independent-proof/latest.json').read_text(encoding='utf-8'));base=Path(record['root']);binary=Path(record['host_binary'])
    assert digest(binary)==record['host_sha256']
    for name,value in record['host_sources'].items():assert digest(ROOT/name)==value,name
    sdk=base/'tool-sdk';subprocess.run([sys.executable,str(ROOT/'scripts/export-tool-sdk.py'),str(sdk)],check=True)
    cache=sdk/'.local/downloads';cache.mkdir(parents=True)
    for pattern in ('python-*-embed-amd64.zip','pillow-*.json','pillow-*.whl'):
        for file in (ROOT/'.local/downloads').glob(pattern):shutil.copy2(file,cache/file.name)
    qt=Path(os.environ.get('MODYN_QT_ROOT',str(ROOT/'.local/toolchains/Qt/6.8.3/msvc2022_64')));timings={};packages={}
    for name in ('python-statistics','native-bytes'):
        source=base/'workspaces'/name;shutil.copytree(ROOT/'examples/independent-frontends'/name,source,ignore=shutil.ignore_patterns('.local','__pycache__'))
        output=base/'built'/name;started=time.perf_counter()
        if name=='python-statistics':command=[sys.executable,str(sdk/'scripts/build-tool.py'),str(source),'--output',str(output)]
        else:command=[sys.executable,str(source/'build.py'),'--sdk',str(sdk),'--qt-root',str(qt),'--output',str(output)]
        subprocess.run(command,cwd=source,check=True);timings[name]=round(time.perf_counter()-started,3);packages[name]=output
    marker=base/'ready.json';report=base/'report.json';script=base/'steps.json'
    steps=[{'op':'mark','path':str(base/'drop-statistics.ready'),'delay_ms':7000},{'op':'open','tool':'example-statistics'},
        {'op':'web_wait','script':"Boolean(document.querySelector('#analyze'))"},
        {'op':'web_eval','script':"document.querySelector('#document').value='hello 世界 hello';document.querySelector('#analyze').click()"},
        {'op':'web_wait','script':"Boolean(window.statistics)&&window.statistics.words===3"},
        {'op':'web_capture','path':str(base/'statistics.png'),'full':True},{'op':'web_flush'},{'op':'draft','enabled':True},
        {'op':'detach'},{'op':'web_wait','script':"Boolean(window.statistics)&&window.statistics.words===3"},{'op':'attach'},
        {'op':'mark','path':str(base/'drop-bytes.ready'),'delay_ms':7000},{'op':'open','tool':'example-bytes'},
        {'op':'web_wait','script':'Boolean(window.byteApp)'},
        {'op':'web_eval','script':"window.byteApp.content='你好A';window.byteApp.run()"},
        {'op':'web_wait','script':'window.byteApp.count===7'},{'op':'web_capture','path':str(base/'bytes.png'),'full':True},
        {'op':'web_flush'},{'op':'save'},{'op':'mark','path':str(marker),'delay_ms':1000}]
    script.write_text(json.dumps({'report':str(report),'steps':steps},ensure_ascii=False),encoding='utf-8')
    env=os.environ.copy();env.update(QT_QPA_PLATFORM='windows',MODYN_BACKGROUND_VALIDATION='1',TEMP=str(base),TMP=str(base))
    env['MODYN_WEBVIEW2_RUNTIME']=json.loads((ROOT/'.local/toolchains/WebView2Runtime/runtime.json').read_text(encoding='utf-8'))['root']
    with (base/'host.log').open('wb') as log:
        started=time.perf_counter();process=subprocess.Popen([str(binary),'--data-root',str(base/'data'),'--packages-root',str(base/'installed'),'--automation',str(script)],cwd=base,env=env,stdout=log,stderr=log,creationflags=subprocess.CREATE_NO_WINDOW)
        copied={}
        while process.poll() is None and time.perf_counter()-started<120:
            for name,marker_name in [('python-statistics','drop-statistics.ready'),('native-bytes','drop-bytes.ready')]:
                if name not in copied and (base/marker_name).exists():
                    metadata=json.loads((packages[name]/'manifest.json').read_text(encoding='utf-8'));destination=base/'installed'/metadata['tool_id']/metadata['package_version'];stage=destination.parent/'.incoming';destination.parent.mkdir(parents=True,exist_ok=True)
                    begin=time.perf_counter();shutil.copytree(packages[name],stage);stage.rename(destination);copied[name]=round(time.perf_counter()-begin,3)
            time.sleep(.02)
        result=process.wait(timeout=10)
    actual=json.loads(report.read_text(encoding='utf-8'));assert result==0 and actual['success'],actual
    assert digest(binary)==record['host_sha256']
    for name,value in record['host_sources'].items():assert digest(ROOT/name)==value,name
    receipt={'success':True,'host_unchanged':True,'host_sha256':record['host_sha256'],'independent_sdk_has_no_app_source':not (sdk/'app').exists(),'frameworks':['Vanilla HTML/JS','Vue'],'backends':['Python','C++'],'build_seconds':timings,'directory_copy_seconds':copied,'automatic_discovery_without_refresh':True,'host_acceptance_seconds':round(time.perf_counter()-started,3),'steps':len(steps),'root':str(base),'report':str(report)}
    (base/'receipt.json').write_text(json.dumps(receipt,indent=2),encoding='utf-8');(ROOT/'.local/logs/IF1-frozen-integration.json').write_text(json.dumps(receipt,indent=2),encoding='utf-8');print(json.dumps(receipt))
if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--freeze',action='store_true');parser.add_argument('--binary',type=Path);args=parser.parse_args()
    if args.freeze:freeze(args.binary)
    elif args.binary:parser.error('--binary requires --freeze')
    run()
