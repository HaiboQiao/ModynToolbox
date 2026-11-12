"""Prepare, validate, publish and verify a versioned ModynToolbox release."""
from pathlib import Path, PurePosixPath
import argparse
import datetime
import hashlib
import json
import os
import shutil
import subprocess
import time
import urllib.error
import urllib.parse
import urllib.request
import uuid
import zipfile
from package import project_version

ROOT = Path(__file__).resolve().parents[1]
REPOSITORY = ROOT.parent
OWNER_REPO = "HaiboQiao/ModynToolbox"
WEB = "https://github.com/" + OWNER_REPO
API = "https://api.github.com/repos/" + OWNER_REPO


def read(path):
    return json.loads(path.read_text(encoding="utf-8"))


def write(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def sha(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def git(*arguments, network=False):
    command = ["git"]
    proxy = urllib.request.getproxies().get("https")
    if network and proxy:
        command += ["-c", "http.proxy=" + proxy]
    env = os.environ.copy()
    env.update(GCM_INTERACTIVE="never", GIT_TERMINAL_PROMPT="0")
    result = subprocess.run(command + list(arguments), cwd=REPOSITORY, env=env,
                            capture_output=True, text=True, timeout=240)
    if result.returncode:
        raise RuntimeError("Git failed: " + result.stderr[-1500:])
    return result.stdout.strip()


def clean():
    if git("status", "--porcelain"):
        raise RuntimeError("Commit the reviewed source and records before preparing a public release")
    return git("rev-parse", "HEAD")


def destination():
    return ROOT / ".local/distribution" / ("v" + project_version())


def inspect_package(package):
    checks = read(package / "release-checksums.json")
    for relative, digest in checks.items():
        target = package / relative
        if not target.is_file() or sha(target) != digest:
            raise RuntimeError("Portable package checksum mismatch: " + relative)
    if any((package / "data").iterdir()):
        raise RuntimeError("Release data directory must be empty")
    return read(package / "release-metadata.json")


def archive_folder(source, output, prefix):
    with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for path in sorted(source.rglob("*")):
            archive.write(path, PurePosixPath(prefix, path.relative_to(source).as_posix()).as_posix())


def asset(path, kind, **extra):
    return {"name": path.name, "kind": kind, "bytes": path.stat().st_size,
            "sha256": sha(path), **extra}


def sums(folder, manifest):
    entries = [(entry["name"], entry["sha256"]) for entry in manifest["assets"]]
    entries.append(("release-manifest.json", sha(folder / "release-manifest.json")))
    (folder / "SHA256SUMS.txt").write_text(
        "".join(digest + "  " + name + "\n" for name, digest in sorted(entries)),
        encoding="utf-8")


def prepare():
    commit = clean()
    version = project_version()
    record = read(ROOT / ".local/releases/latest.json")
    package = ROOT / record["relative_root"]
    metadata = inspect_package(package)
    if metadata["version"] != version or metadata["source_commit"] != commit or metadata["source_dirty"] is not False:
        raise RuntimeError("Rebuild the portable package from the current clean source commit")
    output = destination()
    if output.exists():
        raise RuntimeError("Distribution exists. Validate/publish that immutable output or select a new version")
    output.mkdir(parents=True)
    assets = []
    portable = output / ("ModynToolbox-" + version + "-windows-x64.zip")
    archive_folder(package, portable, "ModynToolbox")
    assets.append(asset(portable, "portable"))
    for manifest_path in sorted((package / "tools").glob("*/*/manifest.json")):
        tool = read(manifest_path)
        archive = output / ("ModynToolbox-tool-" + tool["tool_id"] + "-" + tool["package_version"] + "-windows-x64.zip")
        archive_folder(manifest_path.parent, archive, tool["tool_id"] + "/" + tool["package_version"])
        assets.append(asset(archive, "tool", tool_id=tool["tool_id"], version=tool["package_version"],
                            protocol=tool["protocol_version"], ui_schema=tool["ui_schema_version"],
                            min_host_version=tool.get("min_host_version", "1.0.0")))
    # Media binaries and exact corresponding sources are published with the application.
    media_sources = read(ROOT / "licenses/media-sources.lock.json")
    third_party = output / ("ModynToolbox-" + version + "-third-party-source.zip")
    with zipfile.ZipFile(third_party, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for item in media_sources["inputs"]:
            path = ROOT / ".local/media-source" / item["name"]
            if not path.is_file() or sha(path) != item["sha256"]:
                raise RuntimeError("Corresponding source missing or changed: " + item["name"])
            archive.write(path, "third-party-source/archives/" + item["name"])
        for path in (ROOT / "tools/video-clip/engine").glob("*"):
            if path.is_file(): archive.write(path, "third-party-source/engine/" + path.name)
        for name in ("media-sources.lock.json", "MEDIA-SOURCES.md", "NOTICE.md"):
            archive.write(ROOT / "licenses" / name, "third-party-source/" + name)
        for path in (ROOT / ".local/media-engine/dist").glob("build-*.txt"):
            archive.write(path, "third-party-source/build-records/" + path.name)
    assets.append(asset(third_party, "third_party_source"))
    engine_lock = read(ROOT / "tools/video-clip/dependencies.lock.json")
    engine_zip = ROOT / ".local/downloads" / Path(engine_lock["url"]).name
    if not engine_zip.is_file() or sha(engine_zip) != engine_lock["sha256"]:
        raise RuntimeError("Pinned processing engine archive missing")
    engine_target = output / engine_zip.name
    shutil.copy2(engine_zip, engine_target)
    assets.append(asset(engine_target, "media_engine"))
    source = output / ("ModynToolbox-" + version + "-source.zip")
    git("archive", "--format=zip", "--prefix=ModynToolbox-source-" + version + "/",
        "--output=" + str(source), commit + ":source")
    with zipfile.ZipFile(source, "a", zipfile.ZIP_DEFLATED) as archive:
        archive.writestr("ModynToolbox-source-" + version + "/SOURCE-REVISION.json",
                         json.dumps({"repository": WEB, "commit": commit, "version": version}, indent=2))
    assets.append(asset(source, "source"))
    manifest = {"product": "ModynToolbox", "version": version, "tag": "v" + version,
                "platform": "windows-x64", "source_commit": commit, "repository": WEB,
                "prepared_at": datetime.datetime.now(datetime.timezone.utc).isoformat(),
                "tools": metadata["tools"], "validation": {"complete": False}, "assets": assets}
    write(output / "release-manifest.json", manifest)
    sums(output, manifest)
    print(json.dumps({"prepared": str(output), "assets": len(assets), "commit": commit}))


def extract(archive, target):
    with zipfile.ZipFile(archive) as source:
        total = 0
        for entry in source.infolist():
            path = PurePosixPath(entry.filename)
            if path.is_absolute() or ".." in path.parts or ":" in entry.filename or "\\" in entry.filename:
                raise RuntimeError("Unsafe ZIP entry")
            if (entry.external_attr >> 16) & 0o170000 == 0o120000:
                raise RuntimeError("ZIP contains a symbolic link")
            total += entry.file_size
            if total > 2 * 1024 * 1024 * 1024:
                raise RuntimeError("ZIP exceeds the distribution budget")
        if source.testzip():
            raise RuntimeError("ZIP CRC failure")
        source.extractall(target)


def portable_test(package):
    subprocess.run([os.sys.executable, str(ROOT / "scripts/verify-portable.py"),
                    "--source", str(package)], cwd=ROOT, check=True)


def video_test(package):
    tool = next((package / "tools/video-clip").glob("*/manifest.json")).parent
    subprocess.run([os.sys.executable, str(ROOT / "tools/video-clip/tests/verify_host.py"),
                    "--binary", str(package / "app/ModynToolbox.App.exe"),
                    "--tool-package", str(tool)], cwd=ROOT, check=True)
    subprocess.run([os.sys.executable, str(ROOT / "tools/video-clip/tests/verify_directory.py"),
                    "--binary", str(package / "app/ModynToolbox.App.exe"),
                    "--tool-package", str(tool)], cwd=ROOT, check=True)
    subprocess.run([os.sys.executable, str(ROOT / "tools/video-clip/tests/verify_cancel.py"),
                    "--tool-package", str(tool)], cwd=ROOT, check=True)


def directory_drop_test(package, tool_zip, base):
    host = base / "drop-test"
    host.mkdir()
    shutil.copytree(package / "app", host / "app")
    shutil.copy2(package / "ModynToolbox.exe", host / "ModynToolbox.exe")
    (host / "tools").mkdir()
    sample = host / "sample.txt"
    sample.write_bytes(b"drop-in extension")
    expected = sha(sample)
    marker, report, script = host / "ready.json", host / "report.json", host / "steps.json"
    steps = [{"op": "mark", "path": str(marker), "delay_ms": 5000},
             {"op": "refresh"}, {"op": "open", "tool": "file-hash"},
             {"op": "input", "value": {"files": [str(sample)], "params": {"algorithm": "SHA-256"}}},
             {"op": "run", "action": "hash"}, {"op": "wait", "state": "succeeded"},
             {"op": "expect", "result_contains": expected}]
    write(script, {"report": str(report), "steps": steps})
    temporary = host / "temp"
    temporary.mkdir()
    env = {name: os.environ[name] for name in ("SystemRoot", "WINDIR", "SystemDrive", "COMSPEC") if name in os.environ}
    env.update(QT_QPA_PLATFORM="offscreen", PATH=os.environ["SystemRoot"] + "/System32",
               TEMP=str(temporary), TMP=str(temporary))
    process = subprocess.Popen([str(host / "app/ModynToolbox.App.exe"), "--data-root", str(host / "data"),
                                "--packages-root", str(host / "tools"), "--automation", str(script)],
                               cwd=host, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                               creationflags=subprocess.CREATE_NO_WINDOW)
    try:
        deadline = time.monotonic() + 15
        while not marker.exists() and process.poll() is None and time.monotonic() < deadline:
            time.sleep(0.05)
        if not marker.exists():
            raise RuntimeError("Drop test host failed to start")
        extract(tool_zip, host / "tools")
        if process.wait(timeout=30) or not read(report)["success"]:
            raise RuntimeError("Directory drop/refresh/execution failed")
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=5)


def verify_assets(folder, manifest):
    for entry in manifest["assets"]:
        path = folder / entry["name"]
        if path.stat().st_size != entry["bytes"] or sha(path) != entry["sha256"]:
            raise RuntimeError("Distribution asset mismatch: " + entry["name"])


def validate():
    commit = clean()
    folder = destination()
    manifest = read(folder / "release-manifest.json")
    if manifest["source_commit"] != commit:
        raise RuntimeError("Distribution belongs to another source commit")
    verify_assets(folder, manifest)
    base = ROOT / ".local/distribution-tests" / str(uuid.uuid4())[:8]
    base.mkdir(parents=True)
    for entry in manifest["assets"]:
        extract(folder / entry["name"], base / entry["kind"] / entry["name"])
    source_entry = next(entry for entry in manifest["assets"] if entry["kind"] == "source")
    source_root = base / "source" / source_entry["name"] / ("ModynToolbox-source-" + manifest["version"])
    for name in ("AGENTS.md", "README.md", "CMakeLists.txt", "docs/release-policy.md", "docs/handoff.md"):
        if not (source_root / name).is_file():
            raise RuntimeError("Source archive is incomplete: " + name)
    if read(source_root / "SOURCE-REVISION.json")["commit"] != commit:
        raise RuntimeError("Source archive revision mismatch")
    for path in (source_root / ".local").rglob("*"):
        if path.is_file() and path.name != "README.md":
            raise RuntimeError("Source archive contains generated data")
    for path in (source_root / "data").rglob("*"):
        if path.is_file() and path.name != "README.md":
            raise RuntimeError("Source archive contains user data")
    portable_entry = next(entry for entry in manifest["assets"] if entry["kind"] == "portable")
    package = base / "portable" / portable_entry["name"] / "ModynToolbox"
    inspect_package(package)
    env = os.environ.copy()
    env["QT_QPA_PLATFORM"] = "offscreen"
    actual = subprocess.run([str(package / "app/ModynToolbox.App.exe"), "--version"],
                            capture_output=True, text=True, env=env, check=True,
                            creationflags=subprocess.CREATE_NO_WINDOW).stdout.strip()
    if actual != "ModynToolbox " + manifest["version"]:
        raise RuntimeError("Compiled application version differs from release manifest")
    portable_test(package)
    video_test(package)
    tool = next(entry for entry in manifest["assets"] if entry.get("tool_id") == "file-hash")
    directory_drop_test(package, folder / tool["name"], base)
    manifest["validation"] = {"complete": True, "portable_steps": 33, "directory_drop_refresh_execute": True,
                              "video_host_and_crash_recovery": True, "video_live_directory": True,
                              "video_encoding_cancel": True, "media_corresponding_sources": True,
                              "platform": "Windows x64, isolated PATH and Qt offscreen",
                              "manual_pending": ["physical multi-DPI/Snap", "another clean computer"],
                              "validated_at": datetime.datetime.now(datetime.timezone.utc).isoformat()}
    write(folder / "release-manifest.json", manifest)
    sums(folder, manifest)
    write(folder / "validation.json", {"success": True, "source_commit": commit, "test_root": str(base)})
    print(json.dumps({"validated": True, "directory_drop": True, "assets": len(manifest["assets"])}))


class SafeRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, request, fp, code, message, headers, url):
        result = super().redirect_request(request, fp, code, message, headers, url)
        if result and urllib.parse.urlsplit(request.full_url).hostname != urllib.parse.urlsplit(url).hostname:
            result.remove_header("Authorization")
            result.remove_header("Cookie")
        return result


class GitHub:
    def __init__(self):
        token = os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN")
        if not token:
            env = os.environ.copy()
            env.update(GCM_INTERACTIVE="never", GIT_TERMINAL_PROMPT="0")
            result = subprocess.run(["git", "credential", "fill"], cwd=ROOT,
                                    input="protocol=https\nhost=github.com\n\n",
                                    capture_output=True, text=True, env=env, timeout=30)
            values = dict(line.split("=", 1) for line in result.stdout.splitlines() if "=" in line)
            token = values.get("password")
        if not token:
            raise RuntimeError("GitHub credentials unavailable")
        self.headers = {"Authorization": "Bearer " + token, "User-Agent": "ModynToolbox-release",
                        "Accept": "application/vnd.github+json", "Cache-Control": "no-cache"}
        self.opener = urllib.request.build_opener(SafeRedirect())

    def upload(self, url, file):
        endpoint = urllib.parse.urlsplit(url)
        if endpoint.scheme != "https" or endpoint.hostname != "uploads.github.com":
            raise RuntimeError("Unexpected upload endpoint")
        curl = shutil.which("curl.exe")
        if not curl:
            raise RuntimeError("Windows curl is required for bounded streaming uploads")
        logs = ROOT / ".local/upload-progress"
        logs.mkdir(parents=True, exist_ok=True)
        response = logs / (file.name + ".response.json")
        progress = logs / (file.name + ".log")
        command = [curl, "--config", "-", "--fail-with-body", "--show-error", "--progress-bar",
                   "--connect-timeout", "30", "--max-time", "1800", "--request", "POST",
                   "--header", "Content-Type: application/octet-stream",
                   "--header", "Content-Length: " + str(file.stat().st_size),
                   "--upload-file", str(file), "--output", str(response), url]
        proxy = urllib.request.getproxies().get("https")
        if proxy:
            command[1:1] = ["--proxy", proxy]
        config = "".join("header = " + json.dumps(key + ": " + value) + "\n"
                         for key, value in self.headers.items())
        print("Uploading", file.name, file.stat().st_size, "bytes", flush=True)
        with progress.open("ab") as output:
            result = subprocess.run(command, input=config.encode(), stdout=subprocess.DEVNULL,
                                    stderr=output, timeout=1830)
        if result.returncode:
            raise RuntimeError("Upload interrupted; preserve draft and retry. Progress: " + str(progress))
        return read(response)

    def request(self, method, url, value=None, file=None, missing=False):
        if file:
            return self.upload(url, file)
        headers = self.headers.copy()
        data = None
        if value is not None:
            data = json.dumps(value, ensure_ascii=False).encode()
            headers["Content-Type"] = "application/json"
        for attempt in range(3):
            try:
                request = urllib.request.Request(url, data=data, headers=headers, method=method)
                with self.opener.open(request, timeout=180) as response:
                    body = response.read()
                    return json.loads(body) if body else None
            except urllib.error.HTTPError as error:
                if missing and error.code == 404:
                    return None
                if error.code not in (429, 500, 502, 503, 504) or attempt == 2:
                    raise RuntimeError("GitHub request failed: HTTP " + str(error.code) + " " +
                                       error.read(1000).decode(errors="replace")) from None
            except (urllib.error.URLError, TimeoutError):
                if attempt == 2:
                    raise RuntimeError("GitHub network request failed; preserve draft and retry") from None
            time.sleep(2 ** attempt)
        raise RuntimeError("GitHub retry budget exhausted")


def download(url, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    for attempt in range(3):
        try:
            request = urllib.request.Request(url, headers={"User-Agent": "ModynToolbox-release-verification",
                                                          "Cache-Control": "no-cache"})
            with urllib.request.urlopen(request, timeout=180) as response, target.open("wb") as output:
                shutil.copyfileobj(response, output, 1024 * 1024)
            return
        except (urllib.error.URLError, TimeoutError):
            if attempt == 2:
                raise RuntimeError("Public asset download failed: " + target.name) from None
            time.sleep(2 ** attempt)


def publish():
    commit = clean()
    folder = destination()
    manifest = read(folder / "release-manifest.json")
    if manifest["source_commit"] != commit or not manifest["validation"]["complete"]:
        raise RuntimeError("Validate this clean source commit before publication")
    verify_assets(folder, manifest)
    if git("remote", "get-url", "origin").rstrip("/").removesuffix(".git") != WEB:
        raise RuntimeError("Origin differs from the authorized repository")
    if git("branch", "--show-current") != "main":
        raise RuntimeError("Publish from the reviewed main branch")
    git("fetch", "origin", network=True)
    git("merge-base", "--is-ancestor", "origin/main", commit)
    tag = manifest["tag"]
    local_tags = git("tag", "--list", tag)
    if local_tags:
        if git("rev-parse", tag + "^{commit}") != commit:
            raise RuntimeError("Existing tag points elsewhere")
    else:
        git("tag", "-a", tag, commit, "-m", "ModynToolbox " + manifest["version"])
    git("push", "origin", "HEAD:main", "refs/tags/" + tag, network=True)
    client = GitHub()
    release = client.request("GET", API + "/releases/tags/" + urllib.parse.quote(tag, safe=""), missing=True)
    if not release:
        # The tag endpoint returns published releases. Drafts are found through the authenticated list.
        candidates = client.request("GET", API + "/releases?per_page=100")
        release = next((item for item in candidates if item["tag_name"] == tag), None)
    body = (REPOSITORY / "releases" / manifest["version"] / "README.md").read_text(encoding="utf-8")
    body = body.replace("../../source/docs/release-policy.md", WEB + "/blob/" + tag + "/source/docs/release-policy.md")
    if not release:
        release = client.request("POST", API + "/releases",
                                 {"tag_name": tag, "target_commitish": commit, "name": "ModynToolbox " + manifest["version"],
                                  "body": body, "draft": True, "prerelease": False})
    assets = manifest["assets"] + [asset(folder / "release-manifest.json", "manifest"),
                                    asset(folder / "SHA256SUMS.txt", "checksums")]
    for entry in assets:
        existing = next((item for item in release["assets"] if item["name"] == entry["name"]), None)
        if existing and release["draft"] and existing["state"] != "uploaded":
            client.request("DELETE", API + "/releases/assets/" + str(existing["id"]))
            release["assets"].remove(existing)
            existing = None
        if not existing:
            if not release["draft"]:
                raise RuntimeError("Published release is immutable")
            upload = release["upload_url"].split("{", 1)[0] + "?name=" + urllib.parse.quote(entry["name"])
            existing = client.request("POST", upload, file=folder / entry["name"])
            release["assets"].append(existing)
        digest = existing.get("digest")
        if not digest:
            headers = {**client.headers, "Accept": "application/octet-stream"}
            request = urllib.request.Request(existing["url"], headers=headers)
            check = hashlib.sha256()
            count = 0
            with client.opener.open(request, timeout=180) as response:
                for block in iter(lambda: response.read(1024 * 1024), b""):
                    check.update(block)
                    count += len(block)
            if count != entry["bytes"]:
                raise RuntimeError("Server asset length mismatch")
            digest = "sha256:" + check.hexdigest()
        if existing["state"] != "uploaded" or existing["size"] != entry["bytes"] or digest != "sha256:" + entry["sha256"]:
            raise RuntimeError("Server asset digest mismatch: " + entry["name"])
        print("Verified uploaded asset", entry["name"], flush=True)
    if release["draft"]:
        release = client.request("PATCH", API + "/releases/" + str(release["id"]),
                                 {"draft": False, "make_latest": "true"})
    receipt = {"version": manifest["version"], "source_commit": commit, "tag": tag,
               "release_id": release["id"], "url": release["html_url"], "published_at": release["published_at"],
               "assets": [{"name": item["name"], "url": item["browser_download_url"],
                           "size": item["size"], "digest": item.get("digest")} for item in release["assets"]],
               "public_download_verified": False}
    write(folder / "publication.json", receipt)
    public = ROOT / ".local/public-downloads" / tag
    public.mkdir(parents=True, exist_ok=True)
    for entry in assets:
        remote = next(item for item in release["assets"] if item["name"] == entry["name"])
        target = public / entry["name"]
        download(remote["browser_download_url"], target)
        if target.stat().st_size != entry["bytes"] or sha(target) != entry["sha256"]:
            raise RuntimeError("Public download checksum mismatch: " + entry["name"])
    archive = next(entry for entry in manifest["assets"] if entry["kind"] == "portable")
    unpack = public / ("unpacked-" + str(uuid.uuid4())[:8])
    extract(public / archive["name"], unpack)
    portable_test(unpack / "ModynToolbox")
    video_test(unpack / "ModynToolbox")
    receipt["public_download_verified"] = True
    receipt["downloaded_assets"] = len(assets)
    write(folder / "publication.json", receipt)
    refs = git("ls-remote", "origin", "refs/heads/main", "refs/tags/" + tag + "^{}", network=True)
    references = dict(line.split()[::-1] for line in refs.splitlines())
    if references.get("refs/heads/main") != commit or references.get("refs/tags/" + tag + "^{}") != commit:
        raise RuntimeError("Published Git references do not match source")
    print(json.dumps({"published": release["html_url"], "verified_downloads": len(assets), "source_commit": commit}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("prepare", "validate", "publish"))
    arguments = parser.parse_args()
    {"prepare": prepare, "validate": validate, "publish": publish}[arguments.action]()
