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

j = module("json_tool",ROOT/"core/json_tool.py")

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

def json_precision():
    text='{"n":123456789012345678901234567890,"v":-0.12345678901234567890e+22,"s":"\\u4e2d"}'
    assert j.tokenize(text)==j.tokenize(j.process(text)["text"])
    assert j.process('{"a":1,"a":2}')["warnings"]

def json_invalid():
    for text in ('NaN','[1,]','{"x":01}','{"x":}', '['*129+'0'+']'*129,'true false'):
        assert j.process(text)["status"]=="failed",text
    rejects(lambda:j.process("a"*(5*1024*1024+1)))

(ROOT/".local/testdata").mkdir(parents=True,exist_ok=True)

check("json_precision",json_precision)

check("json_invalid",json_invalid)

print(json.dumps({"tool":"json","passed":len(records),"tests":records}))
