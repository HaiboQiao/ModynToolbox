# 视频依赖与对应源码

正式工具1.0.0使用ModynToolbox自行构建的FFmpeg9.0.2-modyn1与ffprobe，外部媒体库仅x264，使用GPL-3.0-or-later。精确版本/源码哈希在engine/inputs.lock.json，二进制归档及文件哈希在dependencies.lock.json。构建过程未修改FFmpeg/x264源码。

该版本GitHub Release同时提供第三方对应源码ZIP，包含输入归档、build-msys2.sh、prepare.py和实际构建配置/工具链记录。独立工具包包含许可、对应源码入口和构建说明，不能将旧开发阶段Gyan通用构建的说明当成本版来源。

QtCore运行库保持独立可替换，Qt/FFmpeg播放组件许可与源归档说明见仓库licenses/MEDIA-SOURCES.md和NOTICE.md。项目原始代码的许可仍由项目所有者决定，第三方许可分别适用于相应组件。
