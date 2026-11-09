from pathlib import Path
import argparse
import json
import sys
sys.stdout.reconfigure(encoding="utf-8")
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "core"))
import rename_tool

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="独立事务重命名")
    parser.add_argument("action", choices=["preview","apply","undo","recover"])
    parser.add_argument("files", nargs="*")
    parser.add_argument("--data-root", type=Path, required=True)
    parser.add_argument("--prefix", default="")
    parser.add_argument("--suffix", default="")
    parser.add_argument("--plan-id")
    parser.add_argument("--plan-sha256")
    args = parser.parse_args()
    if args.action == "preview":
        result = rename_tool.preview(args.files,vars(args),args.data_root)
    elif args.action == "apply":
        result = rename_tool.apply(args.plan_id,args.plan_sha256,args.data_root)
    elif args.action == "undo":
        result = rename_tool.undo(args.data_root)
    else:
        result = rename_tool.recover(args.data_root)
    print(json.dumps(result,ensure_ascii=False,indent=2))
