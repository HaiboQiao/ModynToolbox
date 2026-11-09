# JSON 校验与格式化

core/json_tool.py 为独立业务核心，standalone/run.py 为命令行入口，integration/adapter.py 为公共协议适配，integration/ui.json 为统一界面定义。核心只依赖 Python 标准库。

```powershell
python tools/json/standalone/run.py sample.json
python tools/json/tests/verify_core.py
python scripts/build-tool.py tools/json
```

脚本从自身位置解析模块。完整包携带私有 CPython，依赖版本与摘要见 dependencies.lock 和 scripts/runtime.lock.json。返回结果遵循 shared/protocol。工具数据集中 data/tools/json。

支持严格校验、行列定位、重复键警告、保留非空白 token 的格式化。UTF-8 输入最多 5 MiB，深度 128。工具嵌入不修改宿主。
