import json
import pathlib
import subprocess
import sys
import unittest

compiled_worker = sys.argv.pop(1)
source_root = pathlib.Path(sys.argv.pop(1))

class WorkerTransportTests(unittest.TestCase):
    def call_worker(self, command):
        request = {"job_id": "transport-1", "session_id": "session-2", "text": "独立工具"}
        completed = subprocess.run(command, input=(json.dumps(request) + "\n").encode(),
                                   capture_output=True, timeout=10, check=True)
        result = json.loads(completed.stdout)
        self.assertEqual(result["job_id"], request["job_id"])
        self.assertEqual(result["session_id"], request["session_id"])
        self.assertEqual(result["text"], request["text"])
        self.assertEqual(result["kind"], "result")
        return result

    def test_python_transport(self):
        result = self.call_worker([sys.executable, str(source_root / "app/tests/fixtures/worker.py")])
        self.assertEqual(result["engine"], "python")

    def test_cpp_transport(self):
        self.assertEqual(self.call_worker([compiled_worker])["engine"], "cpp")

    def test_invalid_cpp_request(self):
        result = subprocess.run([compiled_worker], input=b"invalid\n", capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 3)

if __name__ == "__main__":
    unittest.main()
