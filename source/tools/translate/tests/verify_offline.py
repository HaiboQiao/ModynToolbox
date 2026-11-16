"""Run actual packaged CPU inference with Python network calls disabled."""
from pathlib import Path
import sys,json,subprocess,uuid,os
ROOT=Path(__file__).resolve().parents[3]
if '--child' not in sys.argv:
    package=Path(json.loads((ROOT/'.local/tools/translate/latest.json').read_text(encoding='utf-8'))['package'])
    base=ROOT/'.local/offline-tests'/str(uuid.uuid4())[:8];base.mkdir(parents=True)
    env=os.environ.copy();env['PATH']=os.environ.get('SystemRoot','C:/Windows')+'/System32';env['PYTHONIOENCODING']='utf-8'
    with (base/'result.json').open('wb') as out:
        result=subprocess.run([str(package/'runtime/python.exe'),'-B',__file__,str(package/'backend'),'--child'],cwd=base,env=env,stdout=out,stderr=out,timeout=120,creationflags=subprocess.CREATE_NO_WINDOW)
    print(json.dumps(json.loads((base/'result.json').read_text(encoding='utf-8')),ensure_ascii=True))
    if result.returncode:raise SystemExit(result.returncode)
    value=json.loads((base/'result.json').read_text(encoding='utf-8'));assert value['success'] and value['network_disabled']
    log=ROOT/'.local/logs/offline-verification.json';log.write_text(json.dumps(value,ensure_ascii=False,indent=2),encoding='utf-8')
    raise SystemExit(0)
import sys,socket,json,time,re
sys.stdout.reconfigure(encoding='utf-8')
sys.path.insert(0,sys.argv[1])
def blocked(*args,**kwargs):raise AssertionError('Offline translation attempted network access')
original_socket=socket.socket
class NoNetworkSocket(original_socket):
 def connect(self,*args,**kwargs):return blocked(*args,**kwargs)
 def connect_ex(self,*args,**kwargs):return blocked(*args,**kwargs)
socket.socket=NoNetworkSocket;socket.create_connection=blocked;socket.getaddrinfo=blocked
from translation import translate
results=[]
class Cancelled(Exception):pass
class CancelContext:
 def check(self):raise Cancelled()
try:
 translate({'text':'Please translate this.'},CancelContext())
 raise AssertionError('Cancellation was ignored')
except Cancelled:pass
for text in ('Hello world. This is a local translation tool.','你好，世界。这是一个本地翻译工具。','Please save the file.\n\nThank you.'):
 started=time.perf_counter();v=translate({'text':text});assert v['provider']=='local' and v['translated_text'].strip() and v['translated_text']!=text
 if v['target']=='zh':assert re.search('[\u3400-\u9fff]',v['translated_text'])
 if '\n\n' in text:assert '\n\n' in v['translated_text']
 results.append({'input':text,'output':v['translated_text'],'seconds':round(time.perf_counter()-started,3)})
print(json.dumps({'success':True,'network_disabled':True,'results':results},ensure_ascii=False))
