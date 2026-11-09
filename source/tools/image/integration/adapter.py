from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from modyn_sdk import serve
from image_tool import process


def execute(action, data, context):
    if action != "compress":
        raise ValueError("未知操作")
    params = data.get("params", {})
    output = params.get("output_dir") or str(context.data_root / "exports")
    preview = context.job_root / "preview.png"
    result = process(data.get("files", []), params, output, context.progress,
                     context.cancelled, context.checkpoint, preview)
    if preview.exists():
        result["preview_ref"] = "preview.png"
    return result


if __name__ == "__main__":
    raise SystemExit(serve(execute))
