"""Restore immutable, project-local backend runtimes from pinned official artifacts."""
from pathlib import Path
import hashlib
import json
import shutil
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
CACHE = ROOT / ".local/downloads"
PYTHON_VERSION = "3.14.10"
PYTHON_SHA256 = "90905e64a840557d8c1bc4f5b74db3d7ee85e3ffb217611cac1c7813acea4159"
PILLOW_VERSION = "13.0.0"
PILLOW_SHA256 = "f7cbb7a64e7ec664775195e4f81923bc8a93d9451b963f844e59118f63147a7a"


def download(url, destination, checksum=None):
    destination.parent.mkdir(parents=True, exist_ok=True)
    if not destination.exists():
        temporary = destination.with_suffix(destination.suffix + ".download")
        with urllib.request.urlopen(url, timeout=90) as source, temporary.open("wb") as output:
            shutil.copyfileobj(source, output)
        temporary.replace(destination)
    actual = hashlib.sha256(destination.read_bytes()).hexdigest()
    if checksum and actual != checksum:
        raise ValueError("Artifact checksum mismatch " + destination.name)
    return actual


def prepare():
    archive = CACHE / f"python-{PYTHON_VERSION}-embed-amd64.zip"
    download(f"https://www.python.org/ftp/python/{PYTHON_VERSION}/{archive.name}", archive, PYTHON_SHA256)
    base = ROOT / ".local/runtimes/python"
    if not (base / "python.exe").exists():
        base.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(archive) as package:
            package.extractall(base)
        (base / "python314._pth").write_text("python314.zip\n.\nLib/site-packages\n", encoding="utf-8")
    pillow = ROOT / ".local/runtimes/pillow"
    metadata_path = CACHE / f"pillow-{PILLOW_VERSION}.json"
    download(f"https://pypi.org/pypi/pillow/{PILLOW_VERSION}/json", metadata_path)
    metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
    wheel = next(item for item in metadata["urls"]
                 if item["filename"].endswith("cp314-cp314-win_amd64.whl"))
    wheel_path = CACHE / wheel["filename"]
    checksum = download(wheel["url"], wheel_path, PILLOW_SHA256)
    if not (pillow / "python.exe").exists():
        shutil.copytree(base, pillow, dirs_exist_ok=True)
        packages = pillow / "Lib/site-packages"
        packages.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(wheel_path) as package:
            package.extractall(packages)
    lock = {"python":{"version":PYTHON_VERSION,"sha256":PYTHON_SHA256},
            "pillow":{"version":PILLOW_VERSION,"filename":wheel["filename"],"sha256":checksum}}
    (ROOT / "scripts/runtime.lock.json").write_text(json.dumps(lock,indent=2)+"\n",encoding="utf-8")
    print(json.dumps(lock))
    return base, pillow


if __name__ == "__main__":
    prepare()
