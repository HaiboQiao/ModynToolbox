# ModynToolbox

轻量的 Windows 本地工具箱，支持独立工具、多会话、独立窗口、工具包刷新和离线使用。

## 两个入口

| 你的目标 | 入口 |
|---|---|
| 下载后直接使用 | [发布版本 releases](releases/README.md) |
| 修改程序、开发工具，或交给 AI 接手 | [完整源码 source](source/README.md) |

## 直接使用

下载 [ModynToolbox 1.1.0 Windows x64 便携包](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.1.0/ModynToolbox-1.1.0-windows-x64.zip)，完整解压后双击文件夹中的 **ModynToolbox.exe**。

保留同级 app、tools、data 和 licenses。包内已有图片压缩、JSON、批量重命名和文件校验四个工具，携带所需运行时。目标用户无需安装 Python、Qt 或编译器。

全部版本和独立工具包见 [GitHub Releases](https://github.com/HaiboQiao/ModynToolbox/releases)。发布版本目录维护下载入口、版本说明和校验记录。

## 开发与 AI 接手

完整工程集中在 source。请先读取 [source/AGENTS.md](source/AGENTS.md)、[当前状态](source/docs/STATUS.md)和[接手指南](source/docs/handoff.md)。VS Code 打开 source/ModynToolbox.code-workspace。

恢复开发工具链后，双击根目录或 source 中的 `Run-Dev.cmd` 编译并运行当前源码。`Open-ModynToolbox.cmd` 打开本机最近构建的便携版。PowerShell 中使用 `.\Run-Dev.cmd` 或 `.\Open-ModynToolbox.cmd` 调用文件。

发布前必须读取 [发布规程](source/docs/release-policy.md)。

## 仓库结构

```text
ModynToolbox/
├── README.md
├── AGENTS.md
├── source/                    完整源码、文档、测试、开发脚本
└── releases/                  版本入口、发布说明、校验和分发说明
```

每个工具自身的核心、独立入口、界面描述、适配器和依赖集中在 source/tools/<id>。工具包可独立发布，解压到程序 tools/<id>/<version> 后通过“管理 → 工具包 → 刷新目录”发现。具体接入要求见 [工具开发指南](source/docs/tool-development.md)。
