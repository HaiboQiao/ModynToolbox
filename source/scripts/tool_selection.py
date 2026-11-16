"""Default built-in distribution profile. External copied tools remain independently discoverable."""
import json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def recipes():
    config=json.loads((ROOT/'tools/toolset.json').read_text(encoding='utf-8'))
    disabled=set(config.get('disabled_builtin_tools',[]))
    return [p for p in sorted((ROOT/'tools').glob('*/tool.build.json')) if p.parent.name not in disabled]
