"""Verify descendants of this test-owned host die after host termination."""
from pathlib import Path
import ctypes
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
binary=Path(sys.argv[1]).resolve()
root=Path(sys.argv[2]).resolve()
temporary=root/".local/testdata"
temporary.mkdir(parents=True,exist_ok=True)
kernel=ctypes.WinDLL("kernel32",use_last_error=True)
kernel.OpenProcess.argtypes=[ctypes.c_ulong,ctypes.c_int,ctypes.c_ulong]
kernel.OpenProcess.restype=ctypes.c_void_p
kernel.WaitForSingleObject.argtypes=[ctypes.c_void_p,ctypes.c_ulong]
kernel.CloseHandle.argtypes=[ctypes.c_void_p]
with tempfile.TemporaryDirectory(dir=temporary) as folder:
    base=Path(folder)
    package=base/"package"
    original=Path(json.loads((root/".local/tools/json/latest.json").read_text(encoding="utf-8"))["package"])
    shutil.copytree(original,package)
    shutil.copy2(root/"app/tests/fixtures/backend.py",package/"backend/adapter.py")
    manifest=json.loads((package/"manifest.json").read_text(encoding="utf-8"))
    manifest.update(tool_id="fixture",actions=[{"id":"run","label":"run","timeout_seconds":60}])
    hashes={p.relative_to(package).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in package.rglob("*") if p.is_file() and p.name not in ("manifest.json","checksums.json")}
    (package/"checksums.json").write_text(json.dumps(hashes),encoding="utf-8")
    manifest["integrity_sha256"]=hashlib.sha256((package/"checksums.json").read_bytes()).hexdigest()
    (package/"manifest.json").write_text(json.dumps(manifest),encoding="utf-8")
    script=base/"script.json"
    script.write_text(json.dumps({"report":str(base/"report.json"),"steps":[{"op":"install","path":str(package)},{"op":"open","tool":"fixture"},{"op":"run","action":"run","input":{"params":{"mode":"tree"}}},{"op":"wait","timeout_ms":60000}]}),encoding="utf-8")
    environment=os.environ.copy();environment["QT_QPA_PLATFORM"]="offscreen"
    process=subprocess.Popen([str(binary),"--data-root",str(base/"data"),"--packages-root",str(base/"tools"),"--automation",str(script)],env=environment,cwd=base,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,creationflags=subprocess.CREATE_NO_WINDOW)
    handle=None
    try:
        marker=base/"data/tools/fixture/child.pid"
        deadline=time.monotonic()+15
        while not marker.exists() and process.poll() is None and time.monotonic()<deadline:time.sleep(0.02)
        assert marker.exists(),"Owned host never launched descendant"
        handle=kernel.OpenProcess(0x100000,False,int(marker.read_text()))
        assert handle,"Cannot observe owned descendant"
        process.terminate()
        process.wait(timeout=5)
        assert kernel.WaitForSingleObject(handle,5000)==0,"Descendant survived host crash"
        # Restart the same data root. Interrupted history remains, no old worker is rerun.
        script.write_text(json.dumps({"report":str(base/"restart.json"),"steps":[{"op":"expect","sessions":1},{"op":"mark","path":str(base/"ready")}]}),encoding="utf-8")
        subprocess.run([str(binary),"--data-root",str(base/"data"),"--packages-root",str(base/"tools"),"--automation",str(script)],env=environment,cwd=base,creationflags=subprocess.CREATE_NO_WINDOW,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=15)
        import sqlite3
        db=sqlite3.connect(base/"data/host/state.sqlite")
        jobs=[json.loads(row[0]) for row in db.execute("SELECT body FROM jobs")]
        db.close()
        assert jobs and all(j["state"]=="interrupted" for j in jobs)
        print("Owned host crash, descendant cleanup and interrupted restart passed")
    finally:
        if process.poll() is None:process.terminate();process.wait(timeout=5)
        if handle:kernel.CloseHandle(handle)
