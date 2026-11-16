# ModynToolbox 1.5.0

Windows x64 · 2026-11-17 · 清爽蓝色与本地翻译版。完整解压到可写目录，双击ModynToolbox.exe，无需系统Python、Qt或另装浏览器运行时。

## 下载

- [完整便携包](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.5.0/ModynToolbox-1.5.0-windows-x64.zip)
- [完整源码快照](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.5.0/ModynToolbox-1.5.0-source.zip)、[独立工具接入SDK](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.5.0/ModynToolbox-1.5.0-tool-sdk.zip)
- 独立工具：[图片压缩1.3.1](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.5.0/ModynToolbox-tool-image-1.3.1-windows-x64.zip)、[视频裁剪1.3.6](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.5.0/ModynToolbox-tool-video-clip-1.3.6-windows-x64.zip)、[文本翻译1.1.3](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.5.0/ModynToolbox-tool-translate-1.1.3-windows-x64.zip)
- [独立媒体引擎](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.5.0/ModynToolbox-media-engine-9.0.2-modyn1-windows-x64.zip)与[精确对应源码/构建记录](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.5.0/ModynToolbox-1.5.0-third-party-source.zip)
- [SHA256校验](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.5.0/SHA256SUMS.txt)、[发布清单](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.5.0/release-manifest.json)

## 改动

公共壳、品牌图标和内置工具采用清爽蓝白。动作按钮的鼠标操作不再画持续的键盘焦点边框；Tab/空格等键盘操作仍可使用并保留可见焦点。会话菜单、记录和设置均有回归。第三方工具仍拥有自己的完整前端，宿主不强制覆盖工具CSS。

视频剪辑重做为单条时间轴：两端拖柄控制截取区间，独立竖线控制播放位置；当前位置设起点/终点、行内精确秒数、显著“添加视频”。预览在小窗收缩，导出按钮始终可见；窄窗导出设置按需展开，输出结果按需查看。精确模式直接导出，快速截取仍先准备确认真实区间；无效区间明确提示并禁止导出，参数变更需重新准备，原文件/取消/恢复保护保留。

文本翻译默认纯本地中英互译，完整包自带OPUS/Argos双向模型、CTranslate2 INT8 CPU、SentencePiece及私有Python。无需配置网址、API Key或首次下载模型；不会在本地失败时暗中回退联网。可在“翻译方式”中显式选择原在线/自建服务，旧会话明确选择的provider保持。CPU最多4线程，模型仅在工具后端执行时加载；模型使磁盘/执行内存增加，机器翻译尤其专业术语仍需复核。

完整离线工具约294MiB，宿主通用工具安装预算从256MiB提高至512MiB，保留文件数、路径和完整性验证；仅工具安装传新预算，其他复制调用默认不变。本地翻译包要求宿主>=1.5.0。模型来源/哈希/作者/CC-BY-4.0及轮子许可随包保留。

## 升级与独立工具

建议备份旧目录/数据，将新版完整包解压到新目录验证。旧会话固定旧包，点击左侧“旧版会话 · 打开新版”新建；不强制迁移未知草稿或覆盖同版本。删除工具代码保留data/tools/<id>。新工具ZIP解压到tools得到<id>/<version>/manifest.json即可自动发现，也可F5刷新；使用既有通信桥的新工具不需要改宿主代码。

## SmartScreen与签名

当前无可信代码签名身份，发行仍如实标记**未签名**。随包signing-report.json记录根启动器、App和WorkerHost的实际Authenticode状态。Windows可能继续提示“Windows已保护你的电脑／发布者未知”；该提示来自系统的发布者和文件信誉检查，不是app能够自行隐藏的普通弹窗。

已提供可选正式签名入口：使用本机已有的有效代码签名证书，在打包生成哈希前签名并验证，失败阻止发布。没有证书不会自动生成自签名、关闭SmartScreen或移除下载标记。正式签名也不保证新文件立即免提示，EV不再自动绕过信誉检查；Microsoft Store分发需另行账号/身份和上架流程。依据：[微软官方说明](https://learn.microsoft.com/windows/apps/package-and-deploy/smartscreen-reputation)。请从本项目官方Releases下载并核对SHA256。

## 验证边界

发布前执行Release/Debug完整回归、两种独立工具冻结宿主SDK接入、真实禁网中英模型推理、原生后台小窗/拖柄/播放头和按钮状态、正式ZIP及公开下载版的真实业务/视频/本地翻译。最终结果与固定源提交在发布回执和源码验证记录归档。物理多屏DPI/Snap、真实登录、IME、另一台干净电脑、复杂媒体与有效证书签名路径仍需相应设备/身份验收；后台测试不等于所有设备通过。

[发布规程](../../source/docs/release-policy.md)规定只有用户明确要求才发布，并同步源码/发行资产、匿名下载复验。

## 公开发布回执

固定源提交 `a7300c42ca6038633827578eabb4be3a113c6aca`，标签 `v1.5.0`。公开时间 `2026-11-16T19:45:19Z`。全部10个附件上传与匿名下载SHA256核对通过；下载版便携34步及视频、目录接入、真实编码取消再次通过。完整包 551.10MiB。

[机器回执](publication.json)、[固定资产清单](release-manifest.json)、[SHA256](SHA256SUMS.txt)、[详细实现/验证](../../source/docs/verification/UIR10-blue-offline.md)。正式签名未配置，SmartScreen仍可能提示；其他物理设备边界保留。
