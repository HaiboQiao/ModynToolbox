"""Real FFmpeg/CLI regression. All inputs and outputs belong to a private test root."""
from pathlib import Path
import ctypes,hashlib,json,os,subprocess,sys,time,uuid
ROOT=Path(__file__).resolve().parents[3]
BIN=ROOT/".local/tools/video-clip/native/Release"
BASE=ROOT/".local/video-tests"/str(uuid.uuid4());BASE.mkdir(parents=True)
REPORT=BASE/"report.json"
records=[]
def run(args,timeout=90):
    return subprocess.run([str(a) for a in args],capture_output=True,timeout=timeout,creationflags=subprocess.CREATE_NO_WINDOW if os.name=="nt" else 0)
def ff(*args):
    p=run([BIN/"ffmpeg.exe","-hide_banner","-loglevel","error","-nostdin","-y",*args]);assert p.returncode==0,p.stderr.decode(errors="replace")[-2000:]
def call(action,params=None,source=None,context=None,cancel=None,expect="succeeded"):
    p=BASE/(str(uuid.uuid4())+".json")
    p.write_text(json.dumps({"files":[str(source or sample)],"params":params or {},"context":context or {}}),encoding="utf-8")
    args=[BIN/"ModynVideoCli.exe",action,p]
    if cancel is not None:args.append(str(cancel))
    r=run(args);result=json.loads(r.stdout)
    assert result["status"]==expect,result
    return result
def test(name,fn):
    started=time.perf_counter();fn();records.append({"name":name,"seconds":round(time.perf_counter()-started,3),"passed":True});print(name,"PASS",flush=True)
def inspect(p):
    r=run([BIN/"ffprobe.exe","-v","error","-show_streams","-show_format","-of","json",p]);assert r.returncode==0
    return json.loads(r.stdout)
sample=BASE/"中文 视频 sample.mp4"
ff("-f","lavfi","-i","testsrc2=size=640x360:rate=25","-f","lavfi","-i","sine=frequency=880:sample_rate=48000","-t","6","-c:v","libx264","-crf","0","-preset","ultrafast","-g","50","-threads","2","-c:a","aac","-movflags","+faststart",sample)
source_hash=hashlib.sha256(sample.read_bytes()).hexdigest()
params={"start_ms":1240,"end_ms":4240,"mode":"精确裁剪","quality":"均衡","size":"原尺寸","output_dir":str(BASE/"导出 结果"),"filename":"片段"}
try:
    metadata=call("probe")["context"]
    test("probe_metadata",lambda: (None if metadata["media"]["duration_ms"]==6000 and metadata["media"]["width"]==640 else (_ for _ in ()).throw(AssertionError(metadata))))
    result=call("export",params,context=metadata);output=Path(result["artifacts"][0]);info=inspect(output)
    test("precise_duration_audio",lambda: (None if abs(float(info["format"]["duration"])-3)<.08 and len(info["streams"])==2 else (_ for _ in ()).throw(AssertionError(info))))
    def first_frame():
        def pixels(path,seek):
            r=run([BIN/"ffmpeg.exe","-v","error","-ss",str(seek),"-i",path,"-frames:v","1","-vf","scale=160:90","-pix_fmt","gray","-f","rawvideo","pipe:1"]);assert r.returncode==0;return r.stdout
        a,b=pixels(sample,1.24),pixels(output,0)
        assert len(a)==len(b)==14400
        error=sum(abs(x-y) for x,y in zip(a,b))/len(a);assert error<8,error
    test("non_keyframe_first_frame",first_frame)
    def collision():
        again=call("export",params,context=metadata);assert again["artifacts"][0]!=str(output);assert output.exists()
    test("no_overwrite",collision)
    def compression():
        r=call("export",dict(params,start_ms=0,end_ms=6000,quality="较小文件",size="最长边1280",filename="压缩"),context=metadata)
        assert r["size_bytes"]<sample.stat().st_size
    test("compression",compression)
    def no_audio():
        r=call("export",dict(params,remove_audio=True,filename="静音"));assert len(inspect(r["artifacts"][0])["streams"])==1
    test("remove_audio",no_audio)
    def fast():
        p=dict(params,mode="快速截取",filename="快速")
        call("export",p,context=metadata,expect="failed")
        ready=call("prepare",p,context=metadata)
        assert ready["context"]["plan"]["start_ms"]<=1240
        r=call("export",p,context=ready["context"])
        decoded=run([BIN/"ffmpeg.exe","-v","error","-i",r["artifacts"][0],"-f","null","-"])
        assert decoded.returncode==0 and not decoded.stderr,decoded.stderr
    test("fast_prepare_and_decode",fast)
    test("invalid_range",lambda: call("export",dict(params,start_ms=5000,end_ms=4000),expect="failed"))
    test("invalid_filename",lambda: call("export",dict(params,filename="CON"),expect="failed"))
    def changed():
        wrong=json.loads(json.dumps(metadata));wrong["media"]["identity"]="changed"
        call("export",params,context=wrong,expect="failed")
    test("source_identity_recheck",changed)
    def unwritable():
        obstacle=BASE/"not-a-directory";obstacle.write_text("keep")
        call("export",dict(params,output_dir=str(obstacle)),expect="failed");assert obstacle.read_text()=="keep"
    test("invalid_output_directory",unwritable)
    def cancelled():
        call("export",dict(params,filename="取消"),cancel=100,expect="cancelled")
        assert not list((BASE/"导出 结果").glob(".modyn-video-*"))
        assert not list((BASE/"导出 结果").glob("取消*.mp4"))
    test("cancel_cleanup",cancelled)
    def silent_source():
        silent=BASE/"no-audio.mp4";ff("-i",sample,"-an","-c:v","copy",silent)
        r=call("export",params,source=silent);assert len(inspect(r["artifacts"][0])["streams"])==1
    test("source_without_audio",silent_source)
    def rotation():
        rotated=BASE/"rotate.mp4";ff("-display_rotation:v:0","90","-i",sample,"-c","copy",rotated)
        r=call("export",params,source=rotated);v=inspect(r["artifacts"][0])["streams"][0];assert v["width"]==360 and v["height"]==640,v
    test("portrait_rotation",rotation)
    def large_sparse():
        if os.name!="nt":return
        import msvcrt
        big=BASE/"large-sparse.mp4"
        with big.open("wb") as f:
            handle=msvcrt.get_osfhandle(f.fileno());returned=ctypes.c_ulong()
            ctl=ctypes.windll.kernel32.DeviceIoControl;ctl.argtypes=[ctypes.c_void_p,ctypes.c_ulong,ctypes.c_void_p,ctypes.c_ulong,ctypes.c_void_p,ctypes.c_ulong,ctypes.POINTER(ctypes.c_ulong),ctypes.c_void_p]
            assert ctl(handle,0x900c4,None,0,None,0,ctypes.byref(returned),None)
            f.write(sample.read_bytes());f.seek(4*1024**3+1024);f.write(b'\0')
        r=call("probe",source=big);assert r["context"]["media"]["size_bytes"]>4*1024**3
    test("larger_than_4GiB",large_sparse)
    def multiple_audio():
        multi=BASE/"multi-audio.mp4"
        ff("-i",sample,"-map","0:v:0","-map","0:a:0","-map","0:a:0","-c","copy",multi)
        m=call("probe",source=multi)["context"];assert len(m["media"]["audio_tracks"])==2
        r=call("export",dict(params,audio_index=2),source=multi,context=m)
        assert len(inspect(r["artifacts"][0])["streams"])==2
    test("select_audio_track",multiple_audio)
    def variable_rate():
        vfr=BASE/"variable-rate.mkv"
        ff("-i",sample,"-vf",r"select=if(lt(t\,3)\,1\,not(mod(n\,2)))","-fps_mode","vfr","-c:v","libx264","-threads","2","-c:a","aac",vfr)
        r=call("export",params,source=vfr);info=inspect(r["artifacts"][0]);assert abs(float(info["format"]["duration"])-3)<.15
    test("variable_frame_rate",variable_rate)
    def source_time_origin():
        offset=BASE/"nonzero-start.mkv"
        ff("-i",sample,"-c","copy","-output_ts_offset","5",offset)
        m=call("probe",source=offset)["context"];assert m["media"]["source_start_seconds"]>4
        r=call("export",params,source=offset,context=m);assert abs(float(inspect(r["artifacts"][0])["format"]["duration"])-3)<.15
    test("nonzero_timestamp_origin",source_time_origin)
    def hdr_rejected():
        high=BASE/"10-bit.mp4"
        ff("-i",sample,"-an","-vf","format=yuv420p10le","-c:v","libx264","-threads","2",high)
        m=call("probe",source=high)["context"];assert not m["media"]["precise_supported"]
        call("export",params,source=high,context=m,expect="failed")
    test("high_bit_depth_rejected",hdr_rejected)
    def protected_source_write():
        # CLI keeps a read-only sharing handle during encoding. Original bytes are unchanged.
        assert hashlib.sha256(sample.read_bytes()).hexdigest()==source_hash
    test("source_unchanged",protected_source_write)
    assert hashlib.sha256(sample.read_bytes()).hexdigest()==source_hash
    (ROOT/".local/video-tests/latest.json").write_text(json.dumps({"root":str(BASE),"sample":str(sample),"report":str(REPORT)},indent=2),encoding="utf-8")
    result={"success":True,"tests":records,"root":str(BASE),"source_hash_unchanged":True}
except Exception as e:
    result={"success":False,"tests":records,"error":str(e),"root":str(BASE)}
    raise
finally:
    REPORT.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding="utf-8")
    print("REPORT",REPORT,flush=True)
