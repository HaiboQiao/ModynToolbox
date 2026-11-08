"""Verify same-name dependencies stay private in two tool packages."""
from pathlib import Path
import importlib.util
import json
import os
import subprocess
import uuid
ROOT=Path(__file__).resolve().parents[1]
base=ROOT/".local/isolation-tests"/str(uuid.uuid4())[:8];base.mkdir(parents=True)
spec=importlib.util.spec_from_file_location("builder",ROOT/"scripts/build-tool.py")
builder=importlib.util.module_from_spec(spec);spec.loader.exec_module(builder)
packages=[]
for letter in ("a","b"):
    source=base/("source-"+letter)
    (source/"core").mkdir(parents=True);(source/"integration").mkdir()
    (source/"core/state_dependency.py").write_text("VALUE='private-"+letter+"'\n",encoding="utf-8")
    (source/"integration/adapter.py").write_text(
        "from pathlib import Path\nimport sys,time\nsys.path.insert(0,str(Path(__file__).resolve().parent))\n"
        "import state_dependency\nfrom modyn_sdk import serve\n"
        "def probe(action,data,context):\n    time.sleep(0.3)\n    (context.data_root/'probe.txt').write_text(state_dependency.VALUE,encoding='utf-8')\n"
        "    return {'text':'dependency='+state_dependency.VALUE+'; path='+state_dependency.__file__,'message':'isolated'}\n"
        "raise SystemExit(serve(probe))\n",encoding="utf-8")
    metadata=json.loads((ROOT/"tools/json/integration/manifest.json").read_text(encoding="utf-8"))
    metadata.update(tool_id="env-"+letter,name="Environment "+letter,package_version="1.0.0",
                    actions=[{"id":"probe","label":"probe","batch":False,"cancellable":True,"timeout_seconds":15}])
    (source/"integration/manifest.json").write_text(json.dumps(metadata),encoding="utf-8")
    (source/"integration/ui.json").write_text(json.dumps({"view_version":1,"kind":"text"}),encoding="utf-8")
    (source/"tool.build.json").write_text(json.dumps({"language":"python","runtime":"python"}),encoding="utf-8")
    (source/"LICENSE.txt").write_text("Private regression fixture. Python runtime license retained.",encoding="utf-8")
    packages.append(builder.build(source))
script=base/"script.json";report=base/"report.json"
steps=[{"op":"install","path":str(p)} for p in packages]+[
    {"op":"open","tool":"env-a"},{"op":"run","action":"probe"},
    {"op":"open","tool":"env-b"},{"op":"run","action":"probe"},{"op":"wait","state":"succeeded"},
    {"op":"expect","text":"dependency=private-b"},{"op":"open","tool":"env-a"},{"op":"expect","text":"dependency=private-a"}]
script.write_text(json.dumps({"report":str(report),"steps":steps}),encoding="utf-8")
fake=base/"global";fake.mkdir();(fake/"state_dependency.py").write_text("VALUE='wrong-global'\n",encoding="utf-8")
env=os.environ.copy();env.update(QT_QPA_PLATFORM="offscreen",PYTHONPATH=str(fake),PYTHONHOME=str(fake))
binary=ROOT/".local/build/windows-release/bin/Release/ModynToolbox.App.exe"
process=subprocess.run([str(binary),"--data-root",str(base/"data"),"--packages-root",str(base/"tools"),"--automation",str(script)],env=env,cwd=base,capture_output=True,timeout=30,creationflags=subprocess.CREATE_NO_WINDOW)
assert process.returncode==0 and json.loads(report.read_text(encoding="utf-8"))["success"],(process.returncode,str(report))
for letter in ("a","b"):assert (base/"data/tools"/("env-"+letter)/"probe.txt").read_text(encoding="utf-8")=="private-"+letter
output={"success":True,"same_dependency_name":True,"different_values":["private-a","private-b"],"global_python_variables_ignored":True,"private_data_independent":True,"report":str(report)}
(ROOT/".local/logs/environment-isolation.json").write_text(json.dumps(output,indent=2),encoding="utf-8")
print(json.dumps(output))
