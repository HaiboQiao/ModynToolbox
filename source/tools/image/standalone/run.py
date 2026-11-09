from pathlib import Path
import argparse
import json
import sys
sys.stdout.reconfigure(encoding="utf-8")
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "core"))
from image_tool import process

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="独立图片压缩")
    parser.add_argument("files", nargs="+")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--format", choices=["WebP","JPEG","PNG"], default="WebP")
    parser.add_argument("--quality", type=int, default=80)
    parser.add_argument("--longest-edge", type=int, default=0)
    args = parser.parse_args()
    print(json.dumps(process(args.files, vars(args), args.output), ensure_ascii=False, indent=2))
