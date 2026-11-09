"""Language-independent transport fixture, no UI dependencies."""
import json
import sys

sys.stdin.reconfigure(encoding='utf-8')
sys.stdout.reconfigure(encoding='utf-8')

request = json.loads(sys.stdin.readline())
request.update(kind="result", engine="python")
print(json.dumps(request, ensure_ascii=False), flush=True)
