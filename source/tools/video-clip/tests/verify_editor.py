"""Real format/thumbnail/queue regression using only generated private media."""
from pathlib import Path
import subprocess,json,os,uuid,hashlib,time
ROOT=Path(__file__).resolve().parents[3];BIN=ROOT/'.local/tools/video-clip/native/Release';BASE=ROOT/'.local/video-editor-v2'/str(uuid.uuid4())[:8];BASE.mkdir(parents=True);DATA=BASE/'data';out=BASE/'outputs';records=[]
def process(args,timeout=120):return subprocess.run([str(a) for a in args],capture_output=True,timeout=timeout,creationflags=subprocess.CREATE_NO_WINDOW)
def ff(*args):
 r=process([BIN/'ffmpeg.exe','-v','error','-nostdin','-y',*args]);assert r.returncode==0,r.stderr.decode(errors='replace')[-1500:]
source=BASE/'测试 视频.mp4';ff('-f','lavfi','-i','testsrc2=size=320x180:rate=25','-f','lavfi','-i','sine=sample_rate=48000','-t','5','-c:v','libx264','-preset','ultrafast','-threads','2','-c:a','aac',source);original=hashlib.sha256(source.read_bytes()).hexdigest()
base={'files':[str(source)],'params':{'start_ms':1000,'end_ms':3000,'mode':'精确裁剪','quality':'均衡','output_dir':str(out),'filename':'片段'}}
def call(action,params=None,input=None,expected='succeeded'):
 data=json.loads(json.dumps(input or base));data['params'].update(params or {});p=BASE/(str(uuid.uuid4())+'.json');p.write_text(json.dumps(data),encoding='utf-8');r=process([BIN/'ModynVideoCli.exe',action,p,'86400000',DATA]);result=json.loads(r.stdout);assert result['status']==expected,result;return result
def info(file):
 r=process([BIN/'ffprobe.exe','-v','error','-show_streams','-show_format','-of','json',file]);assert r.returncode==0;return json.loads(r.stdout)
def check(name,fn):
 t=time.perf_counter();fn();records.append({'name':name,'passed':True,'seconds':round(time.perf_counter()-t,3)});print(name,'PASS',flush=True)
try:
 def mov():
  r=call('export',{'format':'mov','fps':15,'bitrate_kbps':800});p=Path(r['artifacts'][0]);assert p.suffix=='.mov';v=info(p);assert v['format']['tags']['major_brand'].strip()=='qt';assert v['streams'][0]['avg_frame_rate']=='15/1';assert abs(float(v['format']['duration'])-2)<.2
 check('mov_fps_bitrate',mov)
 def gif():
  r=call('export',{'format':'gif','fps':12});p=Path(r['artifacts'][0]);assert p.suffix=='.gif';v=info(p);assert v['streams'][0]['codec_name']=='gif' and len(v['streams'])==1;assert p.stat().st_size>100;count=process([BIN/'ffprobe.exe','-v','error','-count_frames','-show_entries','stream=nb_read_frames','-of','json',p]);assert int(json.loads(count.stdout)['streams'][0]['nb_read_frames'])>1
 check('gif_real_decode',gif)
 def png():
  r=call('snapshot',{'position_ms':1500});p=Path(r['artifacts'][0]);assert p.suffix=='.png';assert p.read_bytes()[:8]==b'\x89PNG\r\n\x1a\n'.decode('unicode_escape').encode('latin1');assert info(p)['streams'][0]['width']==320
  a=process([BIN/'ffmpeg.exe','-v','error','-ss','1.5','-i',source,'-frames:v','1','-vf','scale=160:90','-pix_fmt','gray','-f','rawvideo','pipe:1']);b=process([BIN/'ffmpeg.exe','-v','error','-i',p,'-frames:v','1','-vf','scale=160:90','-pix_fmt','gray','-f','rawvideo','pipe:1']);assert a.returncode==b.returncode==0 and len(a.stdout)==len(b.stdout)==14400;assert sum(abs(x-y) for x,y in zip(a.stdout,b.stdout))/14400<8
 check('snapshot_png',png)
 def thumbnails():
  a=call('probe',{'with_thumbnails':True});assert len(a['thumbnails'])==16 and all(t['url'].startswith('data:image/jpeg;base64,') for t in a['thumbnails']);b=call('thumbnails');assert b['thumbnails']==a['thumbnails'];assert len(list((DATA/'cache/thumbs').glob('*.json')))==1
 check('thumbnails_cache_16',thumbnails)
 for name,p in [('bad_format',{'format':'exe'}),('bad_fps',{'fps':1000}),('bad_bitrate',{'bitrate_kbps':100001}),('fast_resize',{'mode':'快速截取','size':'最长边1280'}),('gif_fps_limit',{'format':'gif','fps':30})]:check(name,lambda p=p:call('export',p,expected='failed'))
 def batch():
  data=json.loads(json.dumps(base));data['clips']=[{'id':'a','name':'开头','start_ms':0,'end_ms':1000},{'id':'b','name':'结尾','start_ms':3000,'end_ms':4500}];r=call('batch-export',input=data);assert r['completed']==2 and len(r['artifacts'])==2;assert len(set(r['artifacts']))==2;assert all(Path(f).is_file() for f in r['artifacts']);assert all(q['state']=='succeeded' for q in r['queue']);history=call('batch-status',input={'files':[],'params':{}});assert history['batch_id']==r['batch_id'] and len(history['queue'])==2 and history['queue_source_identity'] and history['queue_input']['params']['mode']=='精确裁剪' and 'context' not in history
  r2=call('batch-export',input=data);assert not set(r['artifacts'])&set(r2['artifacts'])
 check('batch_outputs_persistent_no_overwrite',batch)
 def preflight():
  before=set(out.iterdir());data=json.loads(json.dumps(base));data['clips']=[{'start_ms':0,'end_ms':1000},{'start_ms':4000,'end_ms':3000}];call('batch-export',input=data,expected='failed');assert set(out.iterdir())==before
 check('batch_preflight_all_before_write',preflight)
 def stale_source():
  data=json.loads(json.dumps(base));data['clips']=[{'start_ms':0,'end_ms':1000}];data['context']={'media':{'identity':'changed'}};before=set(out.iterdir());call('batch-export',input=data,expected='failed');assert set(out.iterdir())==before
 check('retry_source_identity_guard',stale_source)
 assert hashlib.sha256(source.read_bytes()).hexdigest()==original
 result={'success':True,'tests':records,'source_unchanged':True,'root':str(BASE)}
except Exception as e:
 result={'success':False,'tests':records,'error':str(e),'root':str(BASE)};raise
finally:
 (BASE/'report.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');(ROOT/'.local/logs/VID2-core.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');print('REPORT',BASE/'report.json')
