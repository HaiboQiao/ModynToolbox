"""Exercise the actual portable launcher from a Chinese/space path with isolated PATH."""
from pathlib import Path
import argparse
import base64
import json
import os
import shutil
import struct
import subprocess
import time
import uuid
import zlib
ROOT=Path(__file__).resolve().parents[1]
def png(path):
    def chunk(kind,data):
        return struct.pack(">I",len(data))+kind+data+struct.pack(">I",zlib.crc32(kind+data)&0xffffffff)
    rows=b"".join(b"\0"+bytes((130,100,220))*40 for _ in range(20))
    path.write_bytes(b"\x89PNG\r\n\x1a\n"+chunk(b"IHDR",struct.pack(">2I5B",40,20,8,2,0,0,0))+chunk(b"IDAT",zlib.compress(rows))+chunk(b"IEND",b""))
def verify(source):
    base=ROOT/".local/portable-tests"/("中文 空格 "+str(uuid.uuid4())[:8]);base.mkdir(parents=True)
    portable=base/"ModynToolbox";shutil.copytree(source,portable)
    input_file=base/"sample.txt";input_file.write_text("original",encoding="utf-8")
    picture=base/"sample.png";png(picture)
    report=base/"acceptance.json";script=base/"script.json"
    screenshot=ROOT/".local/logs/final-main.png";image_screen=ROOT/".local/logs/final-image.png"
    initial=sorted(p.name for p in (portable/"tools").iterdir() if p.is_dir())[0]
    expected_sessions=4+int(initial not in ("image","json","rename"))
    steps=[
        {"op":"open","tool":"json"},
        {"op":"input","value":{"text":'{"项目":"ModynToolbox","n":123456789012345678901234567890}'}},
        {"op":"run","action":"format"},{"op":"wait","state":"succeeded"},{"op":"expect","text":"123456789012345678901234567890","delay_ms":300},
        {"op":"draft","enabled":True},{"op":"detach"},{"op":"expect","detached":True},{"op":"attach"},{"op":"screenshot","path":str(screenshot)},
        {"op":"open","tool":"rename"},{"op":"input","value":{"files":[str(input_file)],"params":{"prefix":"new_"}}},
        {"op":"run","action":"preview"},{"op":"wait","state":"succeeded"},{"op":"run","action":"apply","confirmed":True},{"op":"wait","state":"succeeded"},
        {"op":"expect","exists":str(base/"new_sample.txt")},{"op":"run","action":"undo","confirmed":True},{"op":"wait","state":"succeeded"},{"op":"expect","exists":str(input_file)},
        {"op":"open","tool":"image"},{"op":"input","value":{"files":[str(picture)],"params":{"format":"WebP","output_dir":str(base/"outputs")}}},{"op":"run","action":"compress"},
        {"op":"wait","state":"succeeded"},{"op":"expect","exists":str(base/"outputs/sample.webp"),"delay_ms":100},{"op":"screenshot","path":str(image_screen)},
        {"op":"open","tool":"json","fresh":True},{"op":"input","value":{"text":"[]"}},{"op":"detach"},{"op":"expect","detached":True},{"op":"attach"},
        {"op":"expect","sessions":expected_sessions},{"op":"save"}]
    script.write_text(json.dumps({"report":str(report),"steps":steps},ensure_ascii=False),encoding="utf-8")
    env={k:os.environ[k] for k in ("SystemRoot","WINDIR","SystemDrive","COMSPEC") if k in os.environ}
    (base/"temp").mkdir();env.update(PATH=os.environ["SystemRoot"]+"/System32",TEMP=str(base/"temp"),TMP=str(base/"temp"),QT_QPA_PLATFORM="offscreen")
    started=time.perf_counter()
    subprocess.run([str(portable/"ModynToolbox.exe"),"--automation",str(script)],cwd=base,env=env,check=True,timeout=10,creationflags=subprocess.CREATE_NO_WINDOW)
    lock=portable/"data/host/instance.lock"
    deadline=time.monotonic()+60
    while not report.exists() and time.monotonic()<deadline:time.sleep(0.03)
    if not report.exists():
        if lock.exists():
            pid=int(lock.read_text(encoding="utf-8").splitlines()[0])
            import signal;os.kill(pid,signal.SIGTERM)
        raise RuntimeError("Portable host did not complete; test data preserved at "+str(base))
    result=json.loads(report.read_text(encoding="utf-8"))
    assert result["success"],result
    assert input_file.read_text(encoding="utf-8")=="original"
    deadline=time.monotonic()+10
    while lock.exists() and time.monotonic()<deadline:time.sleep(0.02)
    assert not lock.exists(),"Portable host did not close"
    output={"success":True,"steps":len(steps),"seconds":round(time.perf_counter()-started,3),"path":str(portable),"report":str(report),"isolated_path":env["PATH"],"system_python_used_by_app":False,"screenshots":[str(screenshot),str(image_screen)]}
    (ROOT/".local/logs/portable-verification.json").write_text(json.dumps(output,ensure_ascii=False,indent=2),encoding="utf-8")
    print(json.dumps(output,ensure_ascii=True))
if __name__=="__main__":
    p=argparse.ArgumentParser();p.add_argument("--source",type=Path);a=p.parse_args()
    verify(a.source or Path(json.loads((ROOT/".local/releases/latest.json").read_text(encoding="utf-8"))["root"]))
