# ModynToolbox 源码

本目录是完整工程根。仓库根的 releases 维护下载入口，发布前先阅读 docs/release-policy.md。

Windows x64 本地工具箱。C++20 / Qt Widgets 宿主管理统一界面、会话、独立窗口、工具安装和后台作业。每个工具集中在自己的目录，通过独立进程接入。

## 直接使用

双击项目根的 Open-ModynToolbox.cmd，打开最近构建的完整便携版。便携目录也可直接双击 ModynToolbox.exe。发布目录索引在 .local/releases/latest.json。

普通用户也可从 https://github.com/HaiboQiao/ModynToolbox/releases 下载完整便携版。使用方法见 docs/user-guide.md。开发和验证结果见 docs/STATUS.md、docs/verification/UI-R3-navigation.md。完整业务交付记录见 docs/verification/R2-complete.md。

## 目录

```text
source/
├── app/                       宿主、公共 UI、调度、存储及宿主测试
├── tools/
│   ├── json/                  JSON 校验与格式化
│   ├── image/                 图片压缩与格式转换
│   ├── rename/                批量重命名、撤销与恢复
│   ├── file-hash/             独立 C++ 文件校验工具
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

已有公共控件可表达的界面，只需新增工具目录、业务核心、薄适配器、manifest 和 ui.json。使用现有宿主安装完整工具包即可。无需修改宿主 CMake 或 app 前端。新增公共组件或协议能力时更新宿主。接入说明见 docs/tool-development.md，实际验证见 R2 报告。

## 当前范围

包含四个可实际执行的工具、独立窗口与并排会话、版本安装与移除、取消与进程树管理、可选草稿、历史、设置、数据备份恢复和便携发布。

默认浅色雾紫主题。大文本超过 32768 个 UTF-16 码元时显示只读预览，完整数据仍参与处理、复制和导出。首版面向可信本地工具，工具拥有当前用户权限。

自动化验证记录与人工验收项分开保存。目标电脑的多屏 DPI、Snap、系统对话框和干净机器实测仍需最终检查。
