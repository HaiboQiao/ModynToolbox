# ModynToolbox 1.0.2

## 下载

- [完整 Windows x64 便携包](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.0.2/ModynToolbox-1.0.2-windows-x64.zip)
- [源码包](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.0.2/ModynToolbox-1.0.2-source.zip)
- [SHA256SUMS.txt](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.0.2/SHA256SUMS.txt)
- [release-manifest.json](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.0.2/release-manifest.json)
- [GitHub 版本页面](https://github.com/HaiboQiao/ModynToolbox/releases/tag/v1.0.2)

完整解压后双击 ModynToolbox/ModynToolbox.exe。app、tools 和 licenses 随文件夹保留。首次启动在同目录 data 保存数据。当前平台为 Windows x64。

## 本版内容

- 左侧加号创建工具会话，多会话显示折线分支。
- 按住工具名或会话名拖出主窗口，可独立打开，Esc/窗口内释放取消。
- 顶部两行品牌与版本，工具工作区集中显示业务界面。
- 独立后端、草稿、历史、设置、取消、备份恢复、版本化工具安装与刷新。
- 仓库整理为 source 和 releases，加入可复用发布规程与分发验证。
- 主程序版本统一为 1.0.2，原生文件校验包重建为 1.0.4。

## 随包工具与独立下载

| 工具 | 工具版本 | 独立包 |
|---|---|---|
| JSON | 1.0.2 | [下载](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.0.2/ModynToolbox-tool-json-1.0.2-windows-x64.zip) |
| 图片压缩 | 1.0.1 | [下载](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.0.2/ModynToolbox-tool-image-1.0.1-windows-x64.zip) |
| 批量重命名 | 1.0.1 | [下载](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.0.2/ModynToolbox-tool-rename-1.0.1-windows-x64.zip) |
| 文件校验 | 1.0.4 | [下载](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.0.2/ModynToolbox-tool-file-hash-1.0.4-windows-x64.zip) |

独立工具 ZIP 解压到应用 tools 中，得到 <id>/<version>/manifest.json，再在“管理 → 工具包”刷新目录。完整便携版已经包含表中四个工具。

## 升级与数据

正常退出旧版，先备份 data。将新版解压到新文件夹，通过工作区备份/恢复迁移数据，保留需要的旧 tools 版本以恢复固定版本会话。外部源文件与自选输出目录单独迁移。

本机自动回归、离屏手势、ZIP 解压运行和目录刷新有记录。物理跨屏拖动、多 DPI、Snap、另一台干净电脑仍需目标设备检查。本版尚未配置 Authenticode 签名。

发布规则与验证记录在 [源码文档](../../source/docs/release-policy.md)中。GitHub 版本正文以此文件为基础生成。
