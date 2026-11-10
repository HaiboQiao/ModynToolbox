"""Verify CMD routing without opening windows or building user data."""
from pathlib import Path
import json
import os
import shutil
import subprocess
import tempfile
import unittest

SOURCE = Path(__file__).resolve().parents[3]
REPO = SOURCE.parent


class LauncherTests(unittest.TestCase):
    def test_batch_routes_and_exit_codes(self):
        parent = SOURCE / ".local/launcher-tests"
        parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="空格 路径 (launch) ", dir=parent) as folder:
            root = Path(folder)
            (root / "source/scripts").mkdir(parents=True)
            for name in ("Open-ModynToolbox.cmd", "Run-Dev.cmd"):
                shutil.copy2(REPO / name, root / name)
                shutil.copy2(SOURCE / name, root / "source" / name)
            stub = """param([Parameter(ValueFromRemainingArguments=$true)][string[]]$Rest = @())
[IO.File]::WriteAllText($env:MODYN_LAUNCH_TEST_OUTPUT, (@{script=$PSCommandPath;arguments=@($Rest)} | ConvertTo-Json))
exit [int]$env:MODYN_LAUNCH_TEST_CODE
"""
            for name in ("open-release.ps1", "dev.ps1"):
                (root / "source/scripts" / name).write_text(stub, encoding="utf-8-sig")
            cases = [("Open-ModynToolbox.cmd", "open-release.ps1", []),
                     ("source/Open-ModynToolbox.cmd", "open-release.ps1", []),
                     ("Run-Dev.cmd", "dev.ps1", ["run"]),
                     ("source/Run-Dev.cmd", "dev.ps1", ["run"])]
            for relative, expected, arguments in cases:
                for code in (0, 37):
                    with self.subTest(entry=relative, exit_code=code):
                        output = root / "capture.json"
                        env = os.environ.copy()
                        env.update(MODYN_LAUNCH_TEST_OUTPUT=str(output), MODYN_LAUNCH_TEST_CODE=str(code))
                        result = subprocess.run(
                            [os.environ["COMSPEC"], "/D", "/C", "call", str(root / relative)],
                            cwd=root.parent, env=env, input=b"\r\n", capture_output=True, timeout=20,
                            creationflags=subprocess.CREATE_NO_WINDOW)
                        self.assertEqual(result.returncode, code, result.stderr.decode(errors="replace"))
                        capture = json.loads(output.read_text(encoding="utf-8-sig"))
                        self.assertEqual(Path(capture["script"]).name, expected)
                        self.assertEqual(capture["arguments"], arguments)

    def test_portable_script_resolves_package(self):
        parent = SOURCE / ".local/launcher-tests"
        parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=parent) as folder:
            root = Path(folder)
            scripts = root / "scripts"
            scripts.mkdir()
            shutil.copy2(SOURCE / "scripts/open-release.ps1", scripts / "open-release.ps1")
            release = root / ".local/releases/package"
            release.mkdir(parents=True)
            (release / "ModynToolbox.exe").touch()
            (release.parent / "latest.json").write_text(
                json.dumps({"relative_root": ".local/releases/package"}), encoding="utf-8")
            output = root / "portable.json"
            env = os.environ.copy()
            env["MODYN_LAUNCH_TEST_OUTPUT"] = str(output)
            script = str(scripts / "open-release.ps1").replace("'", "''")
            command = """function global:Start-Process {
param([string]$FilePath,[string]$WorkingDirectory)
[IO.File]::WriteAllText($env:MODYN_LAUNCH_TEST_OUTPUT,(@{file=$FilePath;cwd=$WorkingDirectory}|ConvertTo-Json))
}
& '""" + script + "'"
            subprocess.run(["powershell.exe", "-NoProfile", "-Command", command],
                           cwd=parent, env=env, capture_output=True, check=True, timeout=20,
                           creationflags=subprocess.CREATE_NO_WINDOW)
            capture = json.loads(output.read_text(encoding="utf-8-sig"))
            expected = release
            self.assertEqual(Path(capture["file"]), expected / "ModynToolbox.exe")
            self.assertEqual(Path(capture["cwd"]), expected)


    def test_missing_portable_build_reports_recovery(self):
        parent = SOURCE / ".local/launcher-tests"
        parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=parent) as folder:
            root = Path(folder)
            scripts = root / "scripts"
            scripts.mkdir()
            script = scripts / "open-release.ps1"
            shutil.copy2(SOURCE / "scripts/open-release.ps1", script)
            for metadata_exists in (False, True):
                with self.subTest(metadata_exists=metadata_exists):
                    if metadata_exists:
                        releases = root / ".local/releases"
                        releases.mkdir(parents=True)
                        (releases / "latest.json").write_text(
                            json.dumps({"relative_root": ".local/releases/missing"}), encoding="utf-8")
                    result = subprocess.run(
                        ["powershell.exe", "-NoProfile", "-File", str(script)],
                        cwd=parent, capture_output=True, timeout=20,
                        creationflags=subprocess.CREATE_NO_WINDOW)
                    self.assertNotEqual(result.returncode, 0)
                    expected = b"Release folder was moved" if metadata_exists else b"Run-Dev.cmd"
                    self.assertIn(expected, result.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
