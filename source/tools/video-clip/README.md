# 视频裁剪 1.4.2

完整独立Web前端与C++后端；宿主只提供通用窗口/作业/文件能力，不绘制视频业务布局。支持蓝白响应式分区、16张有限缩略图、时间轴缩放/平移、独立播放头、HH:MM:SS.mmm（也接受秒数）、按时长、撤销重做、选段/循环预览、分割/命名/多片段。

## 导出
默认精确裁剪；单片段快速截取必须先准备并确认实际关键帧区间，不能改尺寸/帧率/码率。多片段队列使用精确模式，每段独立输出，不自动拼接。MP4/MOV支持原尺寸/最长边1920/1280、画质或视频码率、源帧率或指定10/12/15/24/25/30/50/60fps；只选一条音轨，也可静音。GIF无音频，≤30秒/≤15fps/最长边640。PNG保存当前播放位置的画面，源时长末尾可能没有可解码帧时需向前定位。

画质模式不承诺大小，码率概览仅理论值。毫秒输入不等于任意素材毫秒输出；快模式受关键帧限制。字幕/章节/附件不保留，精确模式仍拒绝HDR/高位深/交错素材，避免静默错误颜色。原文件只读、同名自动编号、输出先临时写入校验后原子不覆盖提交。

## 队列与恢复
选择多个片段后导出由后端单个作业顺序执行；切换/分离/页面回收不拥有队列执行权。提交后更改编辑不影响已冻结作业。每段提交后写工具自身batches记录，取消保留已完成输出、清理当前临时文件。上次队列只读取记录，不自动执行；明确重试未完成项使用原文件/选段/设置快照并复核原身份。中断发生于提交与记录之间时需用户核对实际输出，不删除未知文件。

草稿仅用户明确启用后跨启动保存，恢复不提交任务。内部editorVersion=2可读已知旧单选区，未来未知状态只读保留。工具包/数据分离，旧会话固定旧包。

## 结构与依赖
frontend/index.html/base.css/tool.js与editor-state.js为工具自有界面；core/video.cpp负责原有媒体/事务及新格式，core/batch.cpp负责队列；integration/adapter.cpp仅协议适配。缓存位于data/tools/video-clip/cache/thumbs，16张160×90 JPEG，最多32份/约64MiB，不是业务唯一副本。队列执行记录位于batches，源路径属于作业记录，不能当作默认输入草稿。

处理引擎9.0.2-modyn2自带静态x264/zlib，PNG/JPEG/GIF可用且禁用网络，不使用系统FFmpeg。inputs.lock.json/dependencies.lock.json锁定对应源码与二进制。精确源码/构建/许可随发行附带。source/.local为项目私有构建和验证目录。

## 开发验证
从source运行python tools/video-clip/build.py；真实核心回归tests/verify_video.py与新能力tests/verify_editor.py。原生后台集成包括app/tests/integration/test_web_frontend.cpp中的videoEditorV2Workflow和blueVideoCompactTimeline。CLI：ModynVideoCli.exe <probe|prepare|export|snapshot|thumbnails|batch-export|batch-status> input.json [cancel-after-ms] [data-root]。

当前仍一次编辑一个视频；多源批量、静音检测和场景检测属于后续P2。物理DPI/IME/异机/复杂媒体需各自证据，不以自动生成样本替代。发布需当前用户明确授权，按docs/release-policy.md执行。

最低宿主1.6.0：本版输出预览依赖通用后端artifacts精确文件授权。旧宿主不应将新包当成完全兼容工具；旧会话继续固定旧包。
