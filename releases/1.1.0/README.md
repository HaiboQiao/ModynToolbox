# ModynToolbox 1.1.0

2026-11-11，Windows x64。

## 下载与打开

- [完整便携包](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.1.0/ModynToolbox-1.1.0-windows-x64.zip)
- [完整源码包](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.1.0/ModynToolbox-1.1.0-source.zip)
- [SHA256 校验](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.1.0/SHA256SUMS.txt)
- [机器可读分发清单](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.1.0/release-manifest.json)

完整解压，双击 ModynToolbox/ModynToolbox.exe。保留 app、tools、licenses 等同级内容。程序与工具携带各自运行依赖，普通使用无需安装 Python、Qt 或开发工具。

## 本版变化

- 新工具箱与 M 品牌标识，用于主窗、独立窗和 EXE，提供七种图标尺寸。
- 左上角两行品牌文字紧凑排列并整体居中。
- 统一按钮、输入框、菜单的字号、内边距、边框和交互状态，支持标准/紧凑密度及 12–18px 字号。
- 修复数字输入框上下箭头空白，鼠标和键盘步进、上下限反馈一致。
- 侧栏工具库显示可用工具数量，刷新按钮和 F5 支持发现新增工具包。
- 会话、运行记录、设置提供直接入口，Ctrl+, 打开设置，Ctrl+K 搜索工具。
- 新增 Run-Dev.cmd 编译运行当前源码，Open-ModynToolbox.cmd 打开本地最近构建的便携版。
- 统一界面规范随源码提供，后续工具使用现有 UI 描述即可继承公共样式。
- 发布触发和源码/发布资产同步规则已写入两级 AGENTS 与发布规程。

## 独立工具包

| 工具 | 版本 | 下载 |
|---|---|---|
| JSON | 1.0.2 | [独立包](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.1.0/ModynToolbox-tool-json-1.0.2-windows-x64.zip) |
| 图片压缩 | 1.0.1 | [独立包](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.1.0/ModynToolbox-tool-image-1.0.1-windows-x64.zip) |
| 批量重命名 | 1.0.1 | [独立包](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.1.0/ModynToolbox-tool-rename-1.0.1-windows-x64.zip) |
| 文件校验 | 1.0.4 | [独立包](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.1.0/ModynToolbox-tool-file-hash-1.0.4-windows-x64.zip) |

完整便携包包含上述四工具。增加工具时将完整工具包解压至 tools/<id>/<version>，点击工具库刷新或按 F5。新增工具遵循接入规范，工具自己的后端语言和依赖独立维护。

## 升级与开发

退出旧版并备份 data。新版解压至新目录，通过工作区备份/恢复迁移数据。保留被旧会话引用的工具版本，外部源文件与自选输出目录另外迁移。

开发者使用源码包中的 AGENTS、README、docs/STATUS.md、docs/ui-design-system.md 和工具开发指南。正式发布流程见 [发布规程](../../source/docs/release-policy.md)。

## 验证范围与设备检查

发布按规程执行 Debug/Release 回归、解压运行、独立工具放目录刷新、公开下载与摘要复核，实际结果记录在仓库本版回执。

本版未配置 Authenticode 签名。物理多 DPI、跨屏拖动、Snap 和另一台干净电脑继续保留目标设备验收。自动验证采用本机 Windows x64 与 Qt offscreen。

## 发布回执

Release 与 Debug 均 11/11 通过，ZIP 解压业务33步、运行中工具目录刷新、导出源码启动器测试通过。8项公开资产已匿名下载并核对SHA256，下载版33步复验通过，3.563秒。

固定源提交 41fe271450446171c5a90530d771840362cfbf8a，标签 v1.1.0。完整包65068970字节，源码包5390999字节。[公开回执](publication.json)、[分发清单](release-manifest.json)、[校验记录](SHA256SUMS.txt)。
