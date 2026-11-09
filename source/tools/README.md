# 独立工具

每个工具放在 tools/<tool_id>/ 一个目录，保持独立构建和测试能力。

```text
<tool_id>/
  README.md
  core/                       # 业务核心
  standalone/                 # 工具箱之外的调用入口
  integration/
    adapter/                  # 工具箱协议适配
    manifest.json
    ui.json
  tests/
  assets/
  dependency-lock.*           # 对应语言的锁定清单
  LICENSES/
```

core 不导入宿主或 UI。适配层与核心可运行在同一工具进程。已有 CLI 通过包装器接入，业务与 UI 混合的程序先整理调用入口。工具接入使用现有 UI 组件时可独立发包，新型组件涉及宿主能力升级。

本项目内的工具开发环境、构建和测试包集中 .local/tools/<tool_id>/，运行数据集中 data/tools/<tool_id>/。工具独立带走时可使用自己的本地环境目录，并更新本机路径配置。当前首批工具及测试样本由对应任务创建。
