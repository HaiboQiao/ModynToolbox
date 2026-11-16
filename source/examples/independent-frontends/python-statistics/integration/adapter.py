from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
from modyn_sdk import serve
from statistics_core import analyze
import json

def run(action,data,context):
    if action!='analyze':raise ValueError('Unknown action')
    result=analyze(data.get('document',''))
    return {'statistics':result,'text':json.dumps(result,ensure_ascii=False)}
if __name__=='__main__':raise SystemExit(serve(run))
