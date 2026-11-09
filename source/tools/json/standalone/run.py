from pathlib import Path
import argparse
import json
import sys
sys.stdout.reconfigure(encoding="utf-8")
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "core"))
from json_tool import process

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="独立 JSON 校验与格式化")
    parser.add_argument("file", type=Path)
    parser.add_argument("--validate-only", action="store_true")
    options = parser.parse_args()
    result = process(options.file.read_text(encoding="utf-8-sig"), not options.validate_only)
    print(result.get("text") or json.dumps(result, ensure_ascii=False))
    raise SystemExit(0 if result["status"] == "succeeded" else 1)
