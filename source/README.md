# ModynToolbox 源码
<!-- project-status: current -->

开发先读[当前背景](docs/project-baseline.md)、[行为契约](docs/behavior-contracts.md)、[变更规则](docs/change-policy.md)与[交接提示](docs/handoff.md)。不依赖聊天或个人Skill。

当前发行线1.4.0已进入完整工具前端架构：工具作者自行绘制全部业务页面，宿主保留导航/会话/窗口/作业服务。默认图片压缩、视频裁剪、文本翻译三工具；发行附件和公开复验状态见仓库 [1.4.0发布记录](../releases/1.4.0/README.md)。开发入口先读[独立前端指南](docs/independent-frontend.md)，架构实测见[IF-1报告](docs/verification/IF-1-independent-frontend.md)，当前界面/翻译见[UIR-9](docs/verification/UIR9-resize-toolset-translation.md)。

上一版1.3.0的动态字段和接入兼容继续保留，见[EXT-1](docs/features/host-extensibility/design.md)。

本目录是完整工程根。仓库根的 releases 维护下载入口，发布前先阅读 docs/release-policy.md。

Windows x64 本地工具箱。C++20 / Qt Widgets 宿主管理公共壳、会话、独立窗口、工具安装和后台作业；WebView2承载工具自行设计的完整页面。每个工具集中在自己的目录，通过独立进程接入。

## 本地启动入口

- 双击 `Run-Dev.cmd`，编译并运行当前源码，需要下文列出的开发工具链。
- 双击 `Open-ModynToolbox.cmd`，打开本机最近构建的完整便携版。该入口使用 `.local/releases/latest.json` 中的目录。
- 仓库根和本目录均提供这两个 CMD 文件，从其他当前目录调用也会定位到对应脚本。

在本目录的 PowerShell 中运行文件即可。

```powershell
.\Run-Dev.cmd
.\Open-ModynToolbox.cmd
```

也可以直接调用 PowerShell 脚本。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\dev.ps1 run
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\open-release.ps1
```

`%~dp0` 属于 CMD 批处理的脚本目录展开语法。复制 CMD 内部命令到 PowerShell 时，需要改成对应的真实路径或上述相对路径。

## 直接使用

便携目录可直接双击 ModynToolbox.exe。源码目录的本地启动方式见上文。

普通用户也可从 https://github.com/HaiboQiao/ModynToolbox/releases 下载完整便携版。使用方法见 docs/user-guide.md。开发和验证结果见 docs/STATUS.md、docs/verification/UI-R3-navigation.md。完整业务交付记录见 docs/verification/R2-complete.md。

## 目录

```text
source/
├── app/                       宿主、公共 UI、调度、存储及宿主测试
├── tools/
│   ├── json/                  旧工具源码，默认停用
│   ├── image/                 图片压缩与格式转换
│   ├── rename/                旧工具源码，默认停用
│   ├── file-hash/             旧工具源码，默认停用
│   ├── video-clip/            独立视频裁剪、压缩与媒体引擎
│   ├── translate/             中英翻译，自有界面与独立后端
│   ├── toolset.json            默认工具集与停用列表
│   └── tests/                 工具核心测试汇总入口
├── shared/                    版本化协议与 Python/C++ SDK
├── scripts/                   构建、工具打包、便携发布及验证
├── docs/                      需求、决策、记录、证据和交接
├── licenses/                  宿主与分发依赖许可
├── .vscode/                   VS Code 编译与调试入口
├── data/                      开发数据，业务内容不进 Git
└── .local/                    本机 SDK、构建、工具包、发布、测试和日志
```

每个工具包含 core、standalone、integration、tests、依赖声明和构建配置。工具数据统一在 data/tools/<tool_id>，宿主状态在 data/host。安装的工具副本在 .local/installed/tools，源码仍在 tools。

## VS Code 开发

使用 VS Code 打开整个项目根目录。依赖为 Windows x64、Visual Studio C++ 桌面工具与 Windows SDK、CMake、Qt 6.8.3 MSVC x64、开发用 Python 3.10+。安装 VS Code C/C++ 扩展。

首次恢复环境后，在项目终端执行。

```powershell
powershell -NoProfile -File scripts/dev.ps1 doctor
powershell -NoProfile -File scripts/dev.ps1 tools
powershell -NoProfile -File scripts/dev.ps1 test
```

随后 F5 使用 windows-complete / Debug 构建并启动。F5 只编译宿主，主动删除的工具保持删除状态。显式执行 tools 才构建并安装工具。需要 Release 时使用 -Preset windows-release。完整迁移步骤见 docs/development.md。

## 新增工具

新工具自己实现完整frontend与独立业务核心，按前端桥/后端协议构建完整版本包，放入工具目录后自动发现。使用既有能力不改宿主；新增宿主系统能力才扩展公共契约。旧ui.json仅兼容。接入见[工具指南](docs/tool-development.md)与[完整前端](docs/independent-frontend.md)。

## 当前范围

默认包含三个可实际执行的工具、独立窗口与并排会话、版本安装与移除、取消与进程树管理、可选草稿、历史、设置、数据备份恢复和便携发布。

默认浅色雾紫公共壳，工具页面布局自定。旧Qt文本视图使用有界只读预览；完整工具页面按自身容量与编辑体验实现，完整数据处理/导出不因显示预览被截断。面向可信本地工具，不是权限沙箱。

自动化验证记录与人工验收项分开保存。目标电脑的多屏 DPI、Snap、系统对话框和干净机器实测仍需最终检查。

界面风格与公共功能设计见 [UI 设计规范](docs/ui-design-system.md)，新工具开发应一并阅读。


视频裁剪位于tools/video-clip，当前完整frontend和独立后端；旧UI v2继续兼容。旧媒体实现记录见VID-1，当前结果见IF-1/DS-1报告。


## 修改保护与验证

`python scripts/change-guard.py validate`检查当前指引/验收索引；开发修改前start保存内容基线，改后review/verify/finish。`dev.ps1 test -GuardTask <任务>`可按本轮影响选择回归，普通run无需创建任务。允许范围、旧验证失效、保护测试复核与限制见变更规则。GP-1只改开发保护，不会替用户启用系统项或发布新版本。
