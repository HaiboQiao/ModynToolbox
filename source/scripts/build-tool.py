"""Build a self-contained tool folder. The host source is never edited by this command."""
import argparse
from pathlib import Path
import hashlib
import importlib.util
import json
import shutil
import uuid

ROOT = Path(__file__).resolve().parents[1]


def write_json(path, value):
    path.write_text(json.dumps(value,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")


def build(source, output=None):
    source = Path(source).resolve()
    metadata = json.loads((source/"integration/manifest.json").read_text(encoding="utf-8"))
    recipe = json.loads((source/"tool.build.json").read_text(encoding="utf-8"))
    target = Path(output).resolve() if output else ROOT/".local/tools"/metadata["tool_id"]/"builds"/str(uuid.uuid4())/"package"
    if target.exists():
        raise FileExistsError("Use a new output directory")
    target.mkdir(parents=True)
    backend = target/"backend"
    backend.mkdir()
    if recipe["language"] == "python":
        spec = importlib.util.spec_from_file_location("modyn_runtime",ROOT/"scripts/runtime.py")
        runtime = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(runtime)
        base, pillow = runtime.prepare()
        shutil.copytree(pillow if recipe["runtime"]=="pillow" else base,target/"runtime")
        shutil.copytree(source/"core",backend,dirs_exist_ok=True,ignore=shutil.ignore_patterns("__pycache__","*.pyc"))
        shutil.copy2(source/"integration/adapter.py",backend/"adapter.py")
        shutil.copy2(ROOT/"shared/sdk/python/modyn_sdk.py",backend/"modyn_sdk.py")
        metadata["entry_program"]="runtime/python.exe"
        metadata["entry_args"]=["-B","{package}/backend/adapter.py"]
    else:
        built = Path(recipe["built_binary"])
        if not built.is_absolute():
            built = source/built
        shutil.copy2(built,backend/built.name)
        for name in recipe.get("runtime_files",[]):
            shutil.copy2(Path(name),backend/Path(name).name)
        metadata["entry_program"]="backend/"+built.name
        metadata["entry_args"]=[]
    shutil.copy2(source/"integration/ui.json",target/"ui.json")
    (target/"LICENSES").mkdir()
    shutil.copy2(source/"LICENSE.txt",target/"LICENSES"/"TOOL.txt")
    write_json(target/"build-info.json",{"tool":metadata["tool_id"],"version":metadata["package_version"],
                                         "language":recipe["language"],"sdk_protocol":1})
    checksums={}
    for path in sorted(target.rglob("*")):
        if path.is_symlink():
            raise ValueError("Links are not supported in tool packages")
        if path.is_file():
            checksums[path.relative_to(target).as_posix()]=hashlib.sha256(path.read_bytes()).hexdigest()
    write_json(target/"checksums.json",checksums)
    metadata["integrity_file"]="checksums.json"
    metadata["integrity_sha256"]=hashlib.sha256((target/"checksums.json").read_bytes()).hexdigest()
    write_json(target/"manifest.json",metadata)
    record=ROOT/".local/tools"/metadata["tool_id"]/"latest.json"
    record.parent.mkdir(parents=True,exist_ok=True)
    write_json(record,{"package":str(target),"version":metadata["package_version"]})
    print("TOOL_PACKAGE="+str(target))
    return target


if __name__ == "__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("source",type=Path)
    parser.add_argument("--output",type=Path)
    args=parser.parse_args()
    build(args.source,args.output)
