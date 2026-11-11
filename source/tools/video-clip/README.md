# 视频裁剪

ModynToolbox 独立工具，开发包 0.1.0。视频分析、裁剪、编码和输出事务在本目录的原生进程运行，前端通过 integration/ui.json 使用公共 UI v2。当前 GitHub 公开宿主 1.1.0 不支持 UI v2，需要本轮源码构建；min_host_version 仍引用当前工程版本，实际兼容还必须满足 ui_schema_version=2。

## 使用

选择或拖入一个视频，等待分析。用时间轴手柄、起终点输入或“设为起点/终点”选段。默认精确裁剪、较高画质、原尺寸、保留选定音轨，另存为 MP4。需要减小文件时选择均衡/较小文件，或限制最长边1920/1280。输入文件始终只读，输出同名自动编号，不覆盖已有文件。

快速截取只复制受支持的 H.264/AAC 流，需要先预览关键帧调整区间，再确认导出。精确模式重新编码，时间输入到毫秒不代表帧精度达到毫秒。压缩不能保证每种源文件都变小，结果“较完整原视频”包含裁剪减少内容的影响。

空格播放/暂停，I/O标记起终点，仅视频区域拥有相应焦点时生效。时间输入格式为HH:MM:SS.mmm，编辑时间时暂停预览。一个会话一个视频，可以加号创建其他会话，拖出独立窗口；离开时暂停，后台导出继续。会话草稿遵循宿主设置，恢复后重新分析并核对源身份。

## 输出和目录

- 默认源目录下 ModynToolbox Exports，可自行选择输出位置和基础文件名。
- core 包含媒体探测、处理计划、进程及文件事务；standalone 是 CLI，integration 是薄协议适配和UI声明。
- 数据归属 data/tools/video-clip。暂存登记在 temporaries，作业在 jobs，最终视频属于用户选择的输出位置。
- .local/tools/video-clip 保存依赖、原生构建和完整工具包；不把系统FFmpeg或Python作为运行依赖。

## 构建与验证

从源码工程根执行，先按 docs/development.md 恢复 Qt 6.8.3 SDK，包含 Multimedia、MultimediaWidgets 所需模块及 ShaderTools。

```powershell
python tools/video-clip/build.py
python tools/video-clip/tests/verify_video.py
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/dev.ps1 tools
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/dev.ps1 test
```

build.py 根据 dependencies.lock.json 下载/校验 FFmpeg 9.0.2，构建 ModynVideo.exe 与 ModynVideoCli.exe，调用通用打包器生成完整工具包。CLI接受动作和input.json路径，输入结构与工具协议相同。验证文件、报告与合成视频都位于 .local/video-tests，不触碰用户视频。

```powershell
.local/tools/video-clip/native/Release/ModynVideoCli.exe probe input.json
python tools/video-clip/tests/verify_host.py --binary .local/build/windows-complete/bin/Debug/ModynToolbox.App.exe
```

## 当前边界

首版优先SDR 8-bit本地视频；已加入无音轨、多音轨选择、旋转、可变帧率与非零时间起点测试。精确模式拒绝HDR/高位深/交错，避免静默改变颜色。仅输出一个视频与一个选择的音轨，字幕、附件和章节不继承。素材时长最多7天，单次编码绝对时限24小时，内存预算2GiB，复杂4K素材需单独评估。预览使用宿主Qt Multimedia，共享宿主UI故障域；导出引擎属于独立受控进程树。

许可证及构建来源见 licenses/README.md 和依赖锁。对外发布需要用户明确指令及发布规程要求。本轮开发实际证据集中 docs/verification/VID-1-video-tool.md，未执行项按报告保留。


补充专项可运行tests/verify_directory.py（传--binary）验证热放目录/F5和移除，tests/verify_cancel.py验证实际编码写出后取消；资源采样tests/measure_video.py传--package。物理/容量未验范围见总验证报告。
