# ModynToolbox 1.2.0

Windows x64，2026-11-12。新增视频裁剪与通用UI v2媒体组件，保持原有四个工具和UI v1兼容。完整解压Windows便携包后双击ModynToolbox.exe，无需安装Python、Qt或FFmpeg。

## 下载

- [Windows完整便携包](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.2.0/ModynToolbox-1.2.0-windows-x64.zip)
- [完整源码和AI接手文档](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.2.0/ModynToolbox-1.2.0-source.zip)
- [第三方对应源码与构建材料](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.2.0/ModynToolbox-1.2.0-third-party-source.zip)
- [SHA256校验](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.2.0/SHA256SUMS.txt)

附件同时提供五个独立工具包和锁定的媒体引擎归档，工具ZIP解压到tools形成tools/<id>/<version>后按F5刷新。视频工具1.0.0需要宿主1.2.0，旧1.1.0无法加载UI v2。其他四个工具版本保持JSON1.0.2、图片1.0.1、重命名1.0.1、文件校验1.0.4。

## 视频裁剪

支持本地播放预览、时间轴和起止时间输入、精确裁剪、关键帧快速截取、三档画质/尺寸、音轨选择与移除音频。每工具多会话和独立浮窗沿用统一雾紫界面。导出另存、同名编号、不覆盖源，支持取消及受控暂存恢复。

处理引擎为项目构建的FFmpeg9.0.2/x264，锁定源归档、构建脚本、配置和工具链，关闭网络和多余外部媒体依赖。Qt播放器依赖仍随宿主独立部署，业务引擎随工具部署。第三方源码包包含处理引擎及Qt/FFmpeg播放组件的对应源码、清单和许可材料。

## 升级与工具接入

将新版本解压到新目录，保留旧版作备份。需要迁移用户状态时使用工作区备份/恢复，避免用整个旧app目录覆盖新运行库。独立工具安装的是完整构建包；业务源码目录需要先按source/docs/tool-development.md打包。移除闲置工具版本保留data/tools/<id>。

## 验证与边界

Release14/14（51.11秒）、Debug14/14（83.37秒），启动器3项及正式引擎视频核心20项通过。完整ZIP与公开下载版还将复验原有33步业务、视频导出/会话/草稿/取消/崩溃恢复和目录热接入，最终结果和固定来源记录在本目录发布回执。

精确模式面向常见SDR 8-bit素材，暂不处理HDR/高位深/交错。4K/高帧率、不同硬件和额外编码组合需单独验证。物理播放观感/听感、多屏DPI/Snap、另一台干净电脑和实机空间/ACL故障保留设备验收。程序未做Authenticode签名。处理引擎GPL与播放组件LGPL许可分别见licenses及第三方源码包，原始应用代码的许可由项目所有者另行决定。

发布流程见[发布规程](../../source/docs/release-policy.md)。本版本固定源提交、资产摘要和公开下载复验结果保存在本目录回执中。
