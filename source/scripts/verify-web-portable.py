"""Exercise the actual complete Web frontends in a copied portable package, including its bundled engine."""
from pathlib import Path
import argparse,ctypes,hashlib,json,os,shutil,subprocess,time,uuid
ROOT=Path(__file__).resolve().parents[1]
def browser_paths(pid):
    from ctypes import wintypes as w
    class Entry(ctypes.Structure):
        _fields_=[('size',w.DWORD),('usage',w.DWORD),('pid',w.DWORD),('heap',ctypes.c_size_t),('module',w.DWORD),('threads',w.DWORD),('parent',w.DWORD),('priority',w.LONG),('flags',w.DWORD),('exe',w.WCHAR*260)]
    k=ctypes.WinDLL('kernel32',use_last_error=True);k.CreateToolhelp32Snapshot.restype=w.HANDLE;k.CloseHandle.argtypes=[w.HANDLE];k.Process32FirstW.argtypes=[w.HANDLE,ctypes.POINTER(Entry)];k.Process32NextW.argtypes=k.Process32FirstW.argtypes;k.OpenProcess.restype=w.HANDLE;k.OpenProcess.argtypes=[w.DWORD,w.BOOL,w.DWORD];k.QueryFullProcessImageNameW.argtypes=[w.HANDLE,w.DWORD,w.LPWSTR,ctypes.POINTER(w.DWORD)]
    snap=k.CreateToolhelp32Snapshot(2,0);e=Entry();e.size=ctypes.sizeof(e);parents={}
    if k.Process32FirstW(snap,ctypes.byref(e)):
        while True:
            parents[e.pid]=e.parent
            if not k.Process32NextW(snap,ctypes.byref(e)):break
    k.CloseHandle(snap);ids={pid}
    for _ in range(8):ids.update(n for n,parent in parents.items() if parent in ids)
    paths=[]
    for n in ids:
        h=k.OpenProcess(0x1000,False,n)
        if not h:continue
        text=ctypes.create_unicode_buffer(32768);size=w.DWORD(len(text))
        if k.QueryFullProcessImageNameW(h,0,text,ctypes.byref(size)):paths.append(text.value)
        k.CloseHandle(h)
    return paths

def verify(source):
    source=Path(source).resolve();metadata=json.loads((source/'release-metadata.json').read_text(encoding='utf-8'));assert not any((source/'data').iterdir())
    base=ROOT/'.local/web-portable-tests'/str(uuid.uuid4())[:8];base.mkdir(parents=True);portable=base/'ModynToolbox'
    for name,digest in json.loads((source/'release-checksums.json').read_text(encoding='utf-8')).items():assert hashlib.sha256((source/name).read_bytes()).hexdigest()==digest,name
    shutil.copytree(source,portable);file=base/'sample.txt';file.write_text('original',encoding='utf-8')
    from importlib.util import spec_from_file_location,module_from_spec
    spec=spec_from_file_location('portable_helpers',ROOT/'scripts/verify-portable.py');helpers=module_from_spec(spec);spec.loader.exec_module(helpers);picture=base/'sample.png';helpers.png(picture)
    video=next((portable/'tools/video-clip').glob('*/backend/ffmpeg.exe'));clip=base/'source.mp4'
    subprocess.run([str(video),'-v','error','-f','lavfi','-i','testsrc2=size=320x180:rate=24','-t','4','-c:v','libx264','-pix_fmt','yuv420p',str(clip)],check=True,creationflags=subprocess.CREATE_NO_WINDOW)
    env={k:os.environ[k] for k in ('SystemRoot','WINDIR','COMSPEC','USERPROFILE','USERNAME','LOCALAPPDATA','APPDATA','ProgramData') if k in os.environ};env.update(PATH=str(Path(os.environ.get('SystemRoot','C:/Windows'))/'System32'),QT_QPA_PLATFORM='windows',MODYN_BACKGROUND_VALIDATION='1',TEMP=str(base),TMP=str(base))
    expected='123456789012345678901234567890';checksum=hashlib.sha256(b'original').hexdigest();data=portable/'data';runtime_paths=[]
    def execute(name,steps):
        script=base/(name+'.json');report=base/(name+'-report.json');marker=base/(name+'-engine.ready')
        steps=steps+[{'op':'mark','path':str(marker),'delay_ms':2000}]
        script.write_text(json.dumps({'report':str(report),'steps':steps},ensure_ascii=False),encoding='utf-8')
        with (base/(name+'.log')).open('wb') as log:
            launcher=subprocess.Popen([str(portable/'ModynToolbox.exe'),'--automation',str(script)],cwd=base,env=env,stdout=log,stderr=log,creationflags=subprocess.CREATE_NO_WINDOW)
            assert launcher.wait(timeout=15)==0
            deadline=time.monotonic()+120;checked=False
            while not report.exists() and time.monotonic()<deadline:
                if marker.exists() and not checked:
                    lock=data/'host/instance.lock';pid=int(lock.read_text(encoding='utf-8').splitlines()[0]);paths=browser_paths(pid);runtime_paths.extend(p for p in paths if Path(p).name.lower()=='msedgewebview2.exe');checked=True
                time.sleep(.03)
        assert report.exists(),'native portable host timed out: '+str(base)
        result=json.loads(report.read_text(encoding='utf-8'));assert result['success'],result
        deadline=time.monotonic()+15
        while (data/'host/instance.lock').exists() and time.monotonic()<deadline:time.sleep(.03)
        assert not (data/'host/instance.lock').exists()
        return len(steps),report
    steps=[{'op':'open','tool':'json'},{'op':'web_wait','script':'Boolean(window.toolApp)'},
        {'op':'web_eval','script':"window.toolApp.text='{\"n\":"+expected+"}';window.toolApp.run('format')"},
        {'op':'web_wait','script':"window.toolApp.output.includes('"+expected+"')"},{'op':'draft','enabled':True},{'op':'web_flush'},
        {'op':'detach'},{'op':'attach'},{'op':'web_wait','script':"window.toolApp.text.includes('"+expected+"')"},{'op':'web_capture','path':str(base/'json.png'),'full':True},
        {'op':'open','tool':'rename'},{'op':'web_wait','script':"Boolean(document.querySelector('#preview'))"},
        {'op':'input','value':{'ui_state':{'files':[{'path':str(file)}],'params':{'prefix':'new_'}},'input':{'files':[str(file)]}}},
        {'op':'web_wait','script':"document.querySelector('#prefix').value==='new_'"},{'op':'web_eval','script':"document.querySelector('#preview').click()"},
        {'op':'web_wait','script':"document.querySelector('#results').textContent.includes('new_sample')"},{'op':'web_flush'},
        {'op':'run','action':'apply','confirmed':True},{'op':'wait','state':'succeeded'},{'op':'expect','exists':str(base/'new_sample.txt')},
        {'op':'run','action':'undo','confirmed':True},{'op':'wait','state':'succeeded'},{'op':'expect','exists':str(file)},
        {'op':'open','tool':'image'},{'op':'web_wait','script':"Boolean(document.querySelector('#compress'))"},
        {'op':'input','value':{'ui_state':{'files':[{'path':str(picture)}]},'input':{'files':[str(picture)]}}},
        {'op':'web_wait','script':"document.querySelector('#before').naturalWidth>0"},{'op':'web_eval','script':"document.querySelector('#compress').click()"},
        {'op':'web_wait','script':"document.querySelector('#after').naturalWidth>0"},{'op':'expect','exists':str(data/'tools/image/exports/sample.webp')},
        {'op':'open','tool':'file-hash'},{'op':'web_wait','script':"Boolean(document.querySelector('#calculate'))"},
        {'op':'input','value':{'ui_state':{'files':[{'path':str(file)}]},'input':{'files':[str(file)]}}},
        {'op':'web_eval','script':"document.querySelector('#calculate').click()"},{'op':'web_wait','script':"document.querySelector('#results').textContent.includes('"+checksum+"')"},
        {'op':'open','tool':'video-clip'},{'op':'web_wait','script':"Boolean(document.querySelector('#export'))"},
        {'op':'input','value':{'ui_state':{'file':{'path':str(clip)},'params':{'start_s':1,'end_s':2}},'input':{'files':[str(clip)]}}},
        {'op':'web_wait','script':"document.querySelector('#player').readyState>=1"},{'op':'web_eval','script':"document.querySelector('#export').click()"},
        {'op':'web_wait','script':"document.querySelector('#results').querySelectorAll('tbody tr').length===1"},
        {'op':'web_capture','path':str(base/'video.png'),'full':True},{'op':'save'}]
    available=set(metadata['tools'])
    filtered=[];active=None
    for step in steps:
        if step.get('op')=='open':active=step['tool']
        if active in available:filtered.append(step)
    steps=filtered
    if 'video-clip' in available:
        # Exercise the shipped editor and worker, not an SDK mock or a developer CLI.
        steps += [
            {'op':'web_wait','script':"Boolean(window.videoEditor)&&!document.querySelector('#export').disabled"},
            {'op':'web_eval','script':"document.querySelector('#snapshot').click()"},
            {'op':'web_wait','script':"document.querySelector('#results').textContent.includes('.png')&&!document.querySelector('#snapshot').disabled"},
            {'op':'web_eval','script':"document.querySelector('#add-clip').click();document.querySelector('#export').click()"},
            {'op':'web_wait','script':"document.querySelectorAll('.queue-row').length===2&&Array.from(document.querySelectorAll('.queue-row')).every(n=>n.textContent.includes('已完成')&&n.textContent.includes('.mp4'))"},
            {'op':'detach'},{'op':'attach'},
            {'op':'web_wait','script':"document.querySelectorAll('.queue-row').length===2&&window.videoEditor.clips().clips.length===2"},
            {'op':'web_eval','script':"document.querySelector('#gif-preset').click();document.querySelector('#export').click()"},
            {'op':'web_wait','script':"document.querySelectorAll('.queue-row').length===2&&Array.from(document.querySelectorAll('.queue-row')).every(n=>n.textContent.includes('已完成')&&n.textContent.includes('.gif'))"},
            {'op':'web_eval','script':"document.querySelector('.queue-row button').click()"},
            {'op':'web_wait','script':"document.querySelector('#artifact-dialog').open&&document.querySelector('#artifact-image').naturalWidth>0"},
            {'op':'web_eval','script':"document.querySelector('#artifact-close').click()"},
            {'op':'web_capture','path':str(base/'video.png'),'full':True}]
    if 'translate' in available:
        steps += [{'op':'open','tool':'translate'},{'op':'web_wait','script':'Boolean(window.translationTool)'},
            {'op':'web_eval','script':"document.querySelector('#provider').value='local';document.querySelector('#source').value='en';document.querySelector('#target').value='zh';document.querySelector('#input').value='Please save the file.';document.querySelector('#input').dispatchEvent(new Event('input',{bubbles:true}));window.translationTool.run()"},
            {'op':'web_wait','script':"document.querySelector('#output').value.includes('保存')&&!document.querySelector('#translate').disabled"},
            {'op':'web_eval','script':"document.querySelector('#source').value='zh';document.querySelector('#target').value='en';document.querySelector('#input').value='请保存文件。';document.querySelector('#input').dispatchEvent(new Event('input',{bubbles:true}));window.translationTool.run()"},
            {'op':'web_wait','script':"document.querySelector('#output').value.toLowerCase().includes('save')&&!document.querySelector('#translate').disabled"},
            {'op':'web_eval','script':"document.querySelector('#input').value='Portable draft';document.querySelector('#input').dispatchEvent(new Event('input',{bubbles:true}))"},
            {'op':'draft','enabled':True},{'op':'web_flush'},{'op':'detach'},{'op':'attach'},
            {'op':'web_wait','script':"document.querySelector('#input').value==='Portable draft'"},
            {'op':'web_capture','path':str(base/'translate.png'),'full':True},{'op':'save'}]
    started=time.perf_counter();count,report=execute('business',steps)
    if 'json' in available:
        restore=[{'op':'open','tool':'json'},{'op':'web_wait','script':"Boolean(window.toolApp)&&window.toolApp.text.includes('"+expected+"')"},{'op':'web_flush'}]
    else:
        restore=[{'op':'open','tool':'translate'},{'op':'web_wait','script':"Boolean(window.translationTool)&&document.querySelector('#input').value==='Portable draft'"},{'op':'web_flush'}]
    count2,report2=execute('restore',restore)
    assert runtime_paths,'No bundled browser process observed';expected_root=str((portable/'runtime/webview2').resolve()).lower()
    assert all(str(Path(p).resolve()).lower().startswith(expected_root+os.sep) for p in runtime_paths),runtime_paths
    receipt={'success':True,'steps':count+count2,'business_steps':count,'restore_steps':count2,'seconds':round(time.perf_counter()-started,3),'path':str(portable),'report':str(report),'restore_report':str(report2),'isolated_path':env['PATH'],'system_python_used_by_app':False,'bundled_fixed_runtime_observed':True,'runtime_version':json.loads((portable/'runtime/webview2-runtime.json').read_text(encoding='utf-8'))['version'],'tools_tested':sorted(available),'screenshots':[str(p) for p in (base/'json.png',base/'video.png',base/'translate.png') if p.is_file()]}
    (ROOT/'.local/logs/portable-verification.json').write_text(json.dumps(receipt,indent=2),encoding='utf-8');(ROOT/'.local/logs/IF1-web-portable.json').write_text(json.dumps(receipt,indent=2),encoding='utf-8');print(json.dumps(receipt))
    return receipt
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args();verify(a.source)
