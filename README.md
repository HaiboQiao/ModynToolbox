# Graphite Toolbox 石墨工具箱

Windows 本地工具工作台。前台采用 C++20 / Qt 6 Widgets，工具通过统一界面规范、版本化协议和独立后端进程接入。产品名称暂定。

## 当前阶段

2026-11-08 已建立独立项目、完整规划和交接入口。当前可在 VS Code 打开工程与文档，应用源码、构建入口、F5 启动和发布程序由后续 T01 开始实现。实际进度以 [STATUS](docs/STATUS.md) 为准。

本项目依据用户要求在独立目录从零开发，当前本机路径为 `D:\Workspace\01_Projects\GraphiteToolbox`。旧讨论目录仅作历史来源，后续工作以本项目内文件为依据。

## 打开项目

在 VS Code 中选择“文件 → 打开文件夹”，选择本项目根目录，或打开根目录的 `GraphiteToolbox.code-workspace`。

主程序建议用 VS Code 配合 C++、CMake Tools 扩展。Python 工具可在 PyCharm 单独打开 `tools/<tool_id>/` 开发，其业务核心保持独立。[开发与迁移指南](docs/development.md)

## 文件归属

| 目录 | 内容 |
|---|---|
| app/ | 宿主源码与宿主测试，各模块分别集中 |
| tools/ | 每工具一个目录，核心/独立入口/适配/界面/测试放在一起 |
| shared/ | 协议规范与轻量接入 SDK |
| scripts/ | 开发、测试、打包和数据导入导出的统一入口 |
| docs/ | 需求、设计、任务、验收、状态、决策和交接 |
| data/ | 本机运行数据，每工具分别存放 |
| .local/ | 构建、工具环境、安装副本、发布包和测试临时产物 |
| .vscode/ | 编辑器协作配置 |

上述源码目录当前提供用途说明，具体实现由任务逐项创建。完整目标结构含源码与交付包两棵目录树。[架构设计](docs/plan.md)

## 开发依据与接手顺序

1. 读取 [AGENTS.md](AGENTS.md) 和 [当前状态](docs/STATUS.md)。
2. 阅读 [需求](docs/spec.md)、[架构](docs/plan.md)、[开发指南](docs/development.md)。
3. 按 [任务](docs/task.md) 实施，以 [验收清单](docs/checklist.md) 记录证据。
4. 跨电脑或跨 Agent 接手使用 [交接指南](docs/handoff.md)。设计取舍保存在 [决策记录](docs/decisions.md)，实际过程保存在 [工作日志](docs/worklog.md)。

## 数据与记录

开发默认数据根为本项目 `data/`，便携发布包默认数据根为包内 `data/`。用户可明确指定其他数据根，数据根不可写时必须提示选择，路径规则见开发指南。

源码与文档纳入 Git，机器配置、运行数据、依赖环境和构建结果按忽略规则分别处理。复制源码后需要重建开发环境。迁移运行数据要单独携带或导出。当前应用尚待实现，文档核对与应用验收分开记录。
## T01 最新运行入口

当前原生架构试点已可运行。项目根执行 powershell -NoProfile -File scripts/dev.ps1 run，测试用同一脚本的 test 动作。VS Code F5 配置已建立，CLI 已验证。
当前界面验证会话和窗口，业务工具与完整任务管理待后续接入。以 docs/STATUS.md 和 docs/verification/M0-architecture.md 为最新事实。
