from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
from modyn_sdk import serve
from translation import translate

def execute(action,data,context):
    if action != 'translate': raise ValueError('未知操作')
    return translate(data,context)

if __name__ == '__main__':
    raise SystemExit(serve(execute))
