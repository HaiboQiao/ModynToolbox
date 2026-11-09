"""Aggregate independent per-tool regressions using the current test interpreter."""
from pathlib import Path
import json,subprocess,sys
ROOT=Path(__file__).resolve().parents[2]
records=[]
for tool in ("json","rename","image"):
    completed=subprocess.run([sys.executable,"-B",str(ROOT/"tools"/tool/"tests/verify_core.py")],capture_output=True,check=True)
    value=json.loads(completed.stdout)
    records.extend(value["tests"])
report=ROOT/".local/logs/core-regression.json"
report.write_text(json.dumps({"passed":len(records),"tests":records},indent=2),encoding="utf-8")
print(json.dumps({"passed":len(records),"report":str(report)}))
