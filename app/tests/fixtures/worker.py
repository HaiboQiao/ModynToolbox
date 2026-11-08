"""Language-independent transport fixture, no UI dependencies."""
import json
import sys

request = json.loads(sys.stdin.readline())
request.update(kind="result", engine="python")
print(json.dumps(request, ensure_ascii=False), flush=True)
