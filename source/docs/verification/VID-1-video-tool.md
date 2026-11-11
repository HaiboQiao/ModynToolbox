# VID-1 视频裁剪本地交付验证

2026-11-12。用户在需求和方案交付后明确要求按开发方案开发，授权本功能的本地实现与验证。当前为本地开发版，GitHub公开版本仍为1.1.0，未触发正式发布。原规划日期2026-11-11。

## 实际实现

独立工具tools/video-clip，原生C++核心、独立CLI和JSONL薄适配器，包内FFmpeg/ffprobe 9.0.2。通用UI v2提供播放器、双端时间轴、64位毫秒输入、音轨选择、声明式探测/准备/导出动作及模式条件。保留v1工具语义，v2动作上限24小时。保持原视频只读、精确裁剪、关键帧快速截取、三档画质/尺寸、移除音频、安全同名另存、过程取消和受控残留恢复。

画面最终采用QGraphicsVideoItem与QGraphicsView组成VideoSurface，Qt Widgets/Multimedia架构保持一致。已观察默认和紧凑、大字号离屏界面，以及真实解码帧；主/浮窗共用会话与样式。中央区域仍为工具工作区。

## 实际验证

| 项目 | 结果 | 原始记录 |
|---|---|---|
| Release全量 | 14/14，62.34秒 | .local/logs/VID-1-ctest-release.log |
| 默认Debug全量 | 14/14，104.02秒 | .local/logs/VID-1-ctest-complete.log |
| 真实媒体核心 | 20项通过 | tools/video-clip/tests/verify_video.py，.local/logs/VID-1-video-core.log |
| UI组件 | 8项通过，含实际解码/定位/暂停/整段循环、键盘/鼠标时间轴、恢复与六种布局 | app/tests/unit/test_video_editor.cpp，两个构建的video-editor-results.txt |
| 完整便携原有业务 | 33步，3.374秒，中文空格路径、受限PATH | .local/logs/VID-1-portable.log |
| 便携包视频专项 | 34步，加等待真实编码写入后的崩溃恢复，正常退出 | .local/video-host-tests/ae42f7fb-34c6-465d-85dc-78b650e2ec1a/summary.json |
| 编码阶段确定性取消 | 等待暂存视频超过20KB后发送真实JSONL cancel，0.063秒停止并清理 | .local/video-cancel-tests/a898404f-b45b-4191-a03c-feabdbc49daa/summary.json |
| 运行中目录接入/移除 | 11步，复制完整包、刷新、实际导出、关闭后删除包、刷新移除，数据保留 | .local/video-directory-tests/cb169fde-04bb-4a51-bd96-a6219abd4256/summary.json |
| 旧公开宿主兼容 | 1.1.0明确拒绝UI v2 | .local/video-host-tests/old-public-f047c500-22dd-4149-8140-d073b8dbd441/report.json |

真实媒体覆盖非关键帧起点首帧与源画面比较、时长/音轨、压缩、无覆盖、无音轨、音轨选择、旋转、VFR、非零时间起点、高位深拒绝、大于4GiB稀疏MP4、源身份复验、非法区间/名称/输出目录、取消和源哈希不变。大文件测试证明64位文件分析，不代表已编码完整4GiB视频。取消0.063秒为本次样片/机器的一次观测，不是所有素材的延迟保证。

目录接入验证宿主SHA256不变，工具包缓存复制0.072秒，该值只代表本机缓存复制，分析和完整性校验另计。实际接入通过同一通用宿主，未添加tool_id业务绘制分支。

## 资源观测

Release、Qt offscreen、暖文件缓存、合成640×360/25fps/6秒素材。以下为MiB。私有提交是Private bytes，工作集另列，不能称为完整RAM或显存预算。总进程树峰值包含工作进程及FFmpeg，工作集求和可能重复共享页。

| 场景 | 宿主空闲/观测期私有提交 | 宿主工作集 | 总进程树私有提交峰值 |
|---|---:|---:|---:|
| previous-public-host-idle | 11.72 | 33.08 | 11.72 |
| new-host-without-video-package | 12.00 | 34.30 | 12.00 |
| installed-idle | 12.04 | 34.36 | 12.04 |
| loaded-paused | 40.02 | 49.70 | 56.35 |
| playing | 107.59 | 109.88 | 107.59 |
| paused-after-play | 106.79 | 109.59 | 106.79 |
| exporting | 39.90 | 49.86 | 150.23 |

实际播放占用高于仅加载，暂停保留解码缓冲，离开会话由宿主park/视图生命周期回收。复杂4K、不同硬件及原生窗口环境需要单独测量，不从此样片推断通用性能。报告与逐点样本在.local/performance/a6259bfb，采样入口tools/video-clip/tests/measure_video.py。

完整本地验证目录约370.29MiB，其中独立静态FFmpeg/ffprobe约201MiB。工具可按需安装，放入目录并不启动编码。初版优先保证完整离线依赖与接入独立性，后续如需缩减磁盘体积，应独立评估可复现的精简/共享FFmpeg构建，不能删除必要DLL或宣称包体没有增加。

## 当前可用入口

仓库根及source的Run-Dev.cmd编译运行默认Debug源码；Open-ModynToolbox.cmd定位最近完整本地包。两入口均已获得本轮新界面和视频包。完整本地目录为.local/releases/ModynToolbox-1.1.0-032af42e，根ModynToolbox.exe可双击。data初始为空，PATH只含System32时视频专项通过。正式对外版本号/标签/上传仍由发布指令和release-policy控制。

当前Release与验证包主程序SHA256相同，为6146a90b435e306f828b2868ca8b05cb7678a1ad50fce803e435d45c3b12d6a3。本轮工作区存在开发修改，验证包source_dirty为true，不能作为已绑定正式标签的发布资产。

最终完整主界面截图.local/design/VID-1-final-main.png，标准/紧凑12/14/18字号矩阵.local/design/VID-1-*.png。生成样片、日志、运行数据与测试残留全部在.local，未使用用户私有视频。

## 失败与修正记录

最初旋转测试使用新版FFmpeg不再写入的rotate元数据写法，实际源素材没有旋转矩阵，改用display_rotation生成真实旋转样片后通过。修正恢复会话时输入控件禁用、程序化恢复被busy状态阻止、媒体参数更改清除全部上下文、真实输出目标预约、文件名及模式约束。

Release首轮13/14，视频业务全部完成后退出0xc0000005。私有Windows调试助手只调试本任务实例，异常指令位于qoffscreen.dll，记录.local/logs/VID-1-debugger*.log。显式析构播放器及先销毁会话视图的单独修改仍复现。最终用Qt场景视频组件替换原生子窗口承载后，专项和两种配置全量通过。底层插件内部机理未完全证明，保留失败记录，不把猜测写成已证实Qt缺陷。

## 保留的设备/容量与分发检查

原生Windows物理播放观感与听感音画同步、多屏DPI/Snap、另一台干净机器待设备验收。当前已测真实解码、时间边界与输出音轨，不将离屏截图等同于全部系统交互验收。磁盘空间不足及真实ACL拒绝尚未构造实机故障环境，已实现错误处理与暂存保护，非法输出目录和编码失败路径通过。

4K/高帧率、HEVC及其他编解码组合依实际构建/硬件验证，初版精确模式拒绝HDR/高位深/交错。多轨/字幕/章节/附件保留、指定目标MB、硬件编码、批量多片段与剪接不属于初版范围。详细帧率等已分析到媒体结果，独立可折叠媒体详情入口留作呈现优化。

依赖版本、下载/可执行文件SHA256和实际构建LICENSE已随包，正式公开分发前按release-policy补齐Qt Multimedia/FFmpeg实际构建组件的许可与对应源码材料检查。本轮没有创建标签、GitHub Release或推送。

## 官方核查

Context7本轮未发现可调用工具，使用Qt6.8官方文档及6.8.3源码核对QMediaPlayer/QGraphicsVideoItem/QVideoWidget和窗口容器行为。参考https://doc.qt.io/qt-6.8/qgraphicsvideoitem.html、https://raw.githubusercontent.com/qt/qtmultimedia/v6.8.3/src/multimediawidgets/qvideowidget.cpp、https://raw.githubusercontent.com/qt/qtbase/v6.8.3/src/widgets/kernel/qwindowcontainer.cpp。FFmpeg选项与许可来源见工具依赖锁及feature plan。
