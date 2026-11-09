from pathlib import Path
import importlib.util
import json,os,sys,tempfile,time
ROOT=Path(__file__).resolve().parents[1]
sys.stdout.reconfigure(encoding="utf-8")
records=[]


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value

r = module("rename_tool",ROOT/"core/rename_tool.py")

def check(name, action):
    started = time.perf_counter()
    action()
    records.append({"name":name,"passed":True,"ms":round((time.perf_counter()-started)*1000,2)})

def rejects(action):
    try:
        action()
    except (ValueError, OSError):
        return
    raise AssertionError("Expected rejection")

def rename_roundtrip():
    with tempfile.TemporaryDirectory(dir=ROOT/".local/testdata") as folder:
        base=Path(folder);data=base/"data";a=base/"a.txt";a.write_text("original")
        result=r.preview([str(a)],{"prefix":"new_"},data);assert a.exists()
        c=result["context"];r.apply(c["plan_id"],c["plan_sha256"],data)
        b=base/"new_a.txt";assert b.read_text()=="original" and not a.exists()
        rejects(lambda:r.apply(c["plan_id"],c["plan_sha256"],data))
        r.undo(data);assert a.read_text()=="original" and not b.exists()
        rejects(lambda:r.undo(data))

def rename_conflicts():
    with tempfile.TemporaryDirectory(dir=ROOT/".local/testdata") as folder:
        base=Path(folder);data=base/"data";a=base/"a.txt";a.write_text("A")
        rejects(lambda:r.preview([str(a),str(a)],{},data))
        rejects(lambda:r.preview([str(a)],{"prefix":"bad:"},data))
        b=base/"x_a.txt";b.write_text("B")
        rejects(lambda:r.preview([str(a)],{"prefix":"x_"},data));assert b.read_text()=="B"
        p=r.preview([str(a)],{"prefix":"y_"},data)["context"];a.write_text("changed")
        rejects(lambda:r.apply(p["plan_id"],p["plan_sha256"],data));assert a.read_text()=="changed"
        link=base/"alias.txt";os.link(a,link)
        rejects(lambda:r.preview([str(a),str(link)],{},data))

def rename_rollback():
    with tempfile.TemporaryDirectory(dir=ROOT/".local/testdata") as folder:
        base=Path(folder);data=base/"data";a=base/"a.txt";b=base/"b.txt";a.write_text("A");b.write_text("B")
        p=r.preview([str(a),str(b)],{"prefix":"x_"},data)["context"]
        original=r.LockedFile.move;calls=[0]
        def fail_once(self,target):
            calls[0]+=1
            if calls[0]==3:raise OSError("injected")
            return original(self,target)
        r.LockedFile.move=fail_once
        try:result=r.apply(p["plan_id"],p["plan_sha256"],data)
        finally:r.LockedFile.move=original
        assert result["status"]=="failed"
        assert a.read_text()=="A" and b.read_text()=="B"
        assert all(r.load(p)["status"]=="rolled_back" for p in (data/"journals").glob("*.json"))

def rename_recovery():
    with tempfile.TemporaryDirectory(dir=ROOT/".local/testdata") as folder:
        base=Path(folder);data=base/"data";a=base/"a.txt";a.write_text("A")
        info=r.identity(a);info["sha256"]=r.digest(a);temp=base/".modyn-interrupted";a.rename(temp)
        identifier="12345678-1234-1234-1234-123456789012"
        r.atomic(data/"journals"/(identifier+".json"),{"schema":1,"id":identifier,"kind":"rename","status":"committing",
            "entries":[{"source":str(a),"temporary":str(temp),"target":str(base/"b.txt"),"current":str(a),"identity":info}]})
        r.recover(data);assert a.read_text()=="A" and not temp.exists()

def future_schema_refused():
    with tempfile.TemporaryDirectory(dir=ROOT/".local/testdata") as folder:
        path=Path(folder)/"future.json";r.atomic(path,{"schema":99,"entries":[]})
        rejects(lambda:r.load(path))

def rename_capacity_boundary():
    with tempfile.TemporaryDirectory(dir=ROOT/".local/testdata") as folder:
        base = Path(folder)
        files = []
        for number in range(2001):
            path = base / (str(number) + ".txt")
            path.write_bytes(b"x")
            files.append(str(path))
        result = r.preview(files[:2000], {"prefix": "new_"}, base / "data")
        assert len(result["rows"]) == 2000
        rejects(lambda: r.preview(files, {"prefix": "new_"}, base / "data"))
        assert all(Path(path).read_bytes() == b"x" for path in files)

(ROOT/".local/testdata").mkdir(parents=True,exist_ok=True)

check("rename_roundtrip",rename_roundtrip)

check("rename_conflicts",rename_conflicts)

check("rename_rollback",rename_rollback)

check("rename_recovery",rename_recovery)



check("future_schema_refused",future_schema_refused)
check("rename_capacity_boundary", rename_capacity_boundary)
print(json.dumps({"tool":"rename","passed":len(records),"tests":records}))
