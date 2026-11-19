# ModynToolbox 1.6.0

Windows x64 · 视频裁剪工作台与默认新版入口。完整解压后双击根ModynToolbox.exe，自带Qt/WebView2、各工具私有运行时与离线中英模型，无需开发环境。

## 下载

- [完整便携版](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.6.0/ModynToolbox-1.6.0-windows-x64.zip)
- [源码快照](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.6.0/ModynToolbox-1.6.0-source.zip)、[独立工具SDK](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.6.0/ModynToolbox-1.6.0-tool-sdk.zip)
- 独立工具：[视频裁剪1.4.2](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.6.0/ModynToolbox-tool-video-clip-1.4.2-windows-x64.zip)、[图片压缩1.3.3](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.6.0/ModynToolbox-tool-image-1.3.3-windows-x64.zip)、[本地翻译1.1.5](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.6.0/ModynToolbox-tool-translate-1.1.5-windows-x64.zip)
- [媒体引擎9.0.2-modyn2](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.6.0/ModynToolbox-media-engine-9.0.2-modyn2-windows-x64.zip)、[精确对应源码及构建记录](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.6.0/ModynToolbox-1.6.0-third-party-source.zip)
- [SHA256](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.6.0/SHA256SUMS.txt)、[发布清单](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.6.0/release-manifest.json)

## 本版改动

视频页按预览、片段编辑、导出配置分区，蓝白卡片、直观模式切换、时间轴缩略图/缩放/平移、独立播放头、HH时间与时长输入。支持选段/循环预览、撤销重做、重置、分割/命名/按时长分段与多片段列表。小窗配置按需展开，主编辑区不靠整页滚动。

多片段由后端一个队列顺序执行，每段独立输出、不自动拼接。提交后继续编辑不改变执行快照，取消保留已完成文件；上次队列可明确重试未完成项，使用原视频/选段/设置并复核身份，不自动恢复执行。实际输出可在独立预览弹窗观看或打开文件。

新增MOV、GIF、PNG截图、帧率/视频码率；GIF无音频，最多30秒/15fps/最长边640。MP4/MOV只保留一条选定音轨，字幕/章节/附件不保留。快速模式仍先准备并确认关键帧区间，不能改变尺寸/帧率/码率；多片段队列使用精确模式。画质编码不保证大小，码率概览是理论值。原文件只读，同名编号、临时校验/原子不覆盖提交及中断恢复保留。

新版随包媒体引擎启用GIF/JPEG/PNG，PNG所需zlib1.3.2从锁定官方源码静态构建，FFmpeg/x264/zlib对应源码/许可/脚本随发布提供，不要求系统FFmpeg。

汇集上一轮默认新版入口与按钮优化：点击工具名称、正常主区重启优先已安装兼容新版，并复用已有新版会话；旧分支及草稿仍固定旧包，运行中扫描不抢页面。按钮采用协调的字号/行高/边距，接受字号设置的内置工具保持缩放。宿主新增通用后端artifacts精确普通文件授权以支持用户输出目录预览，不授权整个目录、不按工具ID写业务分支，所有工具继续拥有完整前端/后端。

## 升级与边界

备份旧工作区，将完整包解压到新目录验证；只复制exe不完整。先正常退出旧程序再开新版，单实例不会热替换旧窗口。新增完整工具ZIP解压为tools/<id>/<version>/manifest.json可自动发现，旧同版本包不覆盖，移除代码保留data。恢复的工具浮窗保留其原会话；旧分支按需继续工作。

本版完成P0/P1，多源批量处理、静音检测、场景检测为后续P2，没有放不可用按钮。SDR8-bit优先，HDR/高位深/交错仍明确拒绝；复杂媒体、异机、多屏DPI/IME/真实登录需设备验收。

## 签名与验证

目前无可信代码签名身份，随包signing-report.json如实记录。Windows仍可能出现SmartScreen发布者未知提示；没有关闭保护或用自签名伪装。源码/Release/Debug、原生小窗/队列/取消/输出预览、正式ZIP与匿名下载实际验证结果在发布回执和源码报告归档，不把自动后台测试当全部设备通过。

只有用户明确要求才按[发布规程](../../source/docs/release-policy.md)发布并同步源码与运行资产。
