"""Exercise the renamed host entry with private data and an offscreen Qt platform."""
import os
import pathlib
import subprocess
import sys
import tempfile
import time
import unittest

binary = pathlib.Path(sys.argv.pop(1)).resolve()
source_root = pathlib.Path(sys.argv.pop(1)).resolve()
temporary_root = pathlib.Path(sys.argv.pop(1)).resolve()


class HostStartTests(unittest.TestCase):
    def launch(self, different_directory):
        with tempfile.TemporaryDirectory(dir=temporary_root) as folder:
            root = pathlib.Path(folder)
            data = root / "data"
            environment = os.environ.copy()
            environment["QT_QPA_PLATFORM"] = "offscreen"
            process = subprocess.Popen(
                [str(binary), "--data-root", str(data), "--packages-root",
                 str(source_root / ".local/installed/tools")],
                cwd=root if different_directory else source_root,
                env=environment,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                creationflags=subprocess.CREATE_NO_WINDOW,
            )
            try:
                deadline = time.monotonic() + 5
                lock = data / "host/instance.lock"
                while not lock.exists() and process.poll() is None and time.monotonic() < deadline:
                    time.sleep(0.02)
                self.assertTrue(lock.exists(), "Host did not initialize its explicit data root")
                time.sleep(0.3)
                self.assertIsNone(process.poll(), "Host exited during startup")
                self.assertFalse((root / "host").exists(), "Host wrote data into the working directory")
            finally:
                if process.poll() is None:
                    process.terminate()
                try:
                    process.communicate(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.communicate(timeout=5)

    def test_normal_entry(self):
        self.launch(False)

    def test_independent_working_directory(self):
        self.launch(True)


if __name__ == "__main__":
    unittest.main()
