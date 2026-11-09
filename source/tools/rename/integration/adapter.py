from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from modyn_sdk import serve
import rename_tool


def execute(action, data, context):
    if action == "preview":
        return rename_tool.preview(data.get("files", []), data.get("params", {}), context.data_root,
                                   context.progress, context.cancelled)
    if action in ("apply","undo","recover"):
        context.reserve(rename_tool.claims(action,data.get("context",{}),context.data_root))
    if action == "apply":
        values = data.get("context", {})
        return rename_tool.apply(values.get("plan_id"), values.get("plan_sha256"),
                                 context.data_root, context.critical, context.cancelled)
    if action == "undo":
        return rename_tool.undo(context.data_root, context.critical, context.cancelled)
    if action == "recover":
        return rename_tool.recover(context.data_root, context.critical, context.cancelled)
    raise ValueError("未知操作")


if __name__ == "__main__":
    raise SystemExit(serve(execute))
