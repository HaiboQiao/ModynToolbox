from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from modyn_sdk import serve
from json_tool import process


def execute(action, data, context):
    if action not in ("validate", "format"):
        raise ValueError("未知操作")
    return process(data.get("text", ""), formatting=action == "format")


if __name__ == "__main__":
    raise SystemExit(serve(execute))
