# ModynToolbox

Windows 本地模块化工具箱，C++20 / Qt 6 Widgets 宿主与独立进程后端。默认采用浅色雾紫设计。

## 当前可用范围

已实现工具目录发现、统一主题与一体化窗口标题区域、搜索、独立会话、多个独立窗口和收回。工具从自己的 integration/ui.json 提供文件参数页或文本编辑页。当前支持添加/拖入文件、参数编辑、文本复制/清空，业务处理后端待接入，处理按钮明确禁用。

T01、T02、T03 均有已实现子集，完整阶段尚未完成。实际状态见 [STATUS](docs/STATUS.md)，本轮证据见 [UI-R1](docs/verification/UI-R1-ModynToolbox.md)。

## 打开与运行

当前项目目录为 D:/Workspace/01_Projects/ModynToolbox。

用 VS Code 打开本目录或 ModynToolbox.code-workspace，选择 ModynToolbox 调试配置后按 F5。调试配置与 CLI 使用同一构建入口，实际按键验收待进行。

~~~powershell
powershell -NoProfile -File scripts/dev.ps1 doctor
powershell -NoProfile -File scripts/prepare-preview.ps1
powershell -NoProfile -File scripts/dev.ps1 run
~~~

prepare-preview 显式准备三个工具预览包，已存在的版本目录保持原样。正常 run/F5 只读取安装目录，手动移除的包不会被普通启动补回。

后台验证使用独立输出和离屏测试。

~~~powershell
powershell -NoProfile -File scripts/dev.ps1 test -Preset windows-check
~~~

新电脑需要兼容的 Visual Studio C++/Windows SDK、Python，以及 Qt 6.8.3 MSVC SDK。运行 scripts/bootstrap.ps1 可恢复项目 Qt SDK，或通过 MODYN_QT_ROOT 指定兼容位置。详情见 [开发指南](docs/development.md)。

## 文件归属

| 目录 | 内容 |
|---|---|
| app/ | 宿主源码、主题、公共组件、窗口、目录发现与宿主测试 |
| tools/<id>/ | 单个工具的核心、独立入口、接入描述和测试集中于此，当前先建立 integration 预览 |
| shared/ | 版本化接入规范与后续轻量 SDK |
| scripts/ | 环境恢复、统一开发入口和预览包准备 |
| docs/ | 需求、架构、任务、验收、决策、过程与验证证据 |
| data/ | 运行数据，按宿主与工具归属组织 |
| .local/ | 构建、开发依赖、安装副本、日志与测试临时产物 |
| .vscode/ | 共享编辑器配置 |

源码工具位于 tools，开发安装副本位于 .local/installed/tools/<id>/<version>。退出后删除某工具的安装目录，下次启动撤下其入口。删除源代码目录与移除安装副本分别管理。用户数据位于 data。

## 接手顺序

1. 读取 [AGENTS](AGENTS.md) 与 [STATUS](docs/STATUS.md)。
2. 阅读 [需求](docs/spec.md)、[架构](docs/plan.md)、[开发指南](docs/development.md)。
3. 按 [任务](docs/task.md) 推进，以 [验收](docs/checklist.md) 和 verification 记录真实证据。
4. 跨电脑与跨 Agent 接手参照 [handoff](docs/handoff.md)、[决策](docs/decisions.md) 与 [工作日志](docs/worklog.md)。

源码和记录纳入 Git。data 与 .local 的生成内容受忽略，迁移源码后重新构建，个人数据按需求单独携带。
