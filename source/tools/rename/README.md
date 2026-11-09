# 批量重命名

core/rename_tool.py 提供独立的计划、文件身份检查、两阶段 journal、撤销与恢复。standalone/run.py 可独立运行，integration 仅连接工具箱协议和声明式界面。业务核心只依赖 Python 标准库及 Windows 文件 API。

```powershell
python tools/rename/standalone/run.py preview sample.txt --data-root test-data --prefix new_
python tools/rename/tests/verify_core.py
python scripts/build-tool.py tools/rename
```

独立 apply 使用预览返回的 --plan-id 与 --plan-sha256。工具箱会在确认后传入这些信息。data/tools/rename 保存计划与 journal，移除工具代码时保留。文件身份或目标变化时拒绝覆盖，恢复和撤销检查已有文件状态。最多 2000 个文件。
