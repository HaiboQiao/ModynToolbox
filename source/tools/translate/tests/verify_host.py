"""Real native host + independent worker; loopback fixture tests protocol, not translation quality."""
from pathlib import Path
import argparse,json,os,subprocess,threading,uuid
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
ROOT=Path(__file__).resolve().parents[3]
def verify(binary):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self,*args):pass
        def do_POST(self):
            data=json.loads(self.rfile.read(int(self.headers['Content-Length'])))
            value='你好，世界！' if data['target']=='zh' else 'Hello, world!'
            self.send_response(200);self.end_headers();self.wfile.write(json.dumps({'translatedText':value},ensure_ascii=False).encode())
    server=ThreadingHTTPServer(('127.0.0.1',0),Handler);thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
    base=ROOT/'.local/translation-host'/str(uuid.uuid4())[:8];base.mkdir(parents=True)
    endpoint=f'http://127.0.0.1:{server.server_port}'
    value={'text':'Hello, world!','source':'en','target':'zh','provider':'libretranslate','endpoint':endpoint}
    package=json.loads((ROOT/'.local/tools/translate/latest.json').read_text(encoding='utf-8'))['package']
    steps=[{'op':'open','tool':'translate'},{'op':'web_wait','script':'Boolean(window.translationTool)'},
      {'op':'web_wait','script':"document.querySelector('footer').getBoundingClientRect().bottom<=innerHeight+1"},
      {'op':'input','value':{'ui_state':value,'input':value}},
      {'op':'web_wait','script':"document.querySelector('#provider').value==='libretranslate'"},
      {'op':'web_eval','script':'window.translationTool.run()'},
      {'op':'web_wait','script':"document.querySelector('#output').value.includes('你好')&&!document.querySelector('#translate').disabled"},
      {'op':'web_eval','script':"document.querySelector('#input').value='你好，世界！';document.querySelector('#source').value='zh';document.querySelector('#target').value='en';document.querySelector('#input').dispatchEvent(new Event('input',{bubbles:true}));"},
      {'op':'web_eval','script':'window.translationTool.run()'},
      {'op':'web_wait','script':"document.querySelector('#output').value==='Hello, world!'&&!document.querySelector('#translate').disabled"},
      {'op':'draft','enabled':True},{'op':'web_flush'},{'op':'detach'},{'op':'attach'},
      {'op':'web_wait','script':"document.querySelector('#input').value.includes('你好')&&document.querySelector('#output').value==='Hello, world!'"},
      {'op':'web_capture','path':str(base/'translation.png'),'full':True},
      {'op':'input','value':{'ui_state':{'text':'Please save the file.','source':'en','target':'zh','provider':'local'},'input':{'text':'Please save the file.','source':'en','target':'zh','provider':'local'}}},
      {'op':'web_wait','script':"document.querySelector('#provider').value==='local'"},
      {'op':'web_eval','script':'window.translationTool.run()'},
      {'op':'web_wait','script':"document.querySelector('#output').value.includes('保存')&&!document.querySelector('#translate').disabled"},
      {'op':'web_eval','script':"document.querySelector('#input').value='请保存文件。';document.querySelector('#source').value='zh';document.querySelector('#target').value='en';document.querySelector('#input').dispatchEvent(new Event('input',{bubbles:true}));window.translationTool.run()"},
      {'op':'web_wait','script':"document.querySelector('#output').value.toLowerCase().includes('save')&&!document.querySelector('#translate').disabled"},
      {'op':'web_capture','path':str(base/'translation-local.png'),'full':True},{'op':'save'}]
    report=base/'report.json';script=base/'steps.json';script.write_text(json.dumps({'report':str(report),'steps':steps},ensure_ascii=False),encoding='utf-8')
    env=os.environ.copy();env.update(QT_QPA_PLATFORM='windows',MODYN_BACKGROUND_VALIDATION='1',MODYN_WEBVIEW2_RUNTIME=json.loads((ROOT/'.local/toolchains/WebView2Runtime/runtime.json').read_text())['root'],TEMP=str(base),TMP=str(base))
    try:
      with (base/'host.log').open('wb') as log:
        run=subprocess.run([str(binary),'--data-root',str(base/'data'),'--tool-package',package,'--automation',str(script)],cwd=ROOT,env=env,stdout=log,stderr=log,timeout=90,creationflags=subprocess.CREATE_NO_WINDOW)
      result=json.loads(report.read_text(encoding='utf-8'));assert run.returncode==0 and result['success'],result
      receipt={'success':True,'steps':len(steps),'fixture':'loopback adapter plus actual packaged CPU local model; not a general quality benchmark','root':str(base),'report':str(report),'screenshot':str(base/'translation-local.png')}
      (ROOT/'.local/logs/UIR9-translation-host.json').write_text(json.dumps(receipt,indent=2),encoding='utf-8');print(json.dumps(receipt));return receipt
    finally:server.shutdown();server.server_close();thread.join()
if __name__=='__main__':
    args=argparse.ArgumentParser();args.add_argument('--binary',type=Path,required=True);a=args.parse_args();verify(a.binary.resolve())
