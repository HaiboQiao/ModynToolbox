# UI v2 媒体组件契约

2026-11-12。已有JSONL协议1消息封套保持原样。manifest_version为1，ui_schema_version为2，view_version为2。UI v1包保持既有行为及3600秒动作上限；UI v2动作上限86400秒，新宿主明确校验两种版本。清单和视图版本必须相等。

## 声明与布局

ui.json使用kind=files，组件video_editor与fields等组成布局。media绑定probe_action、prepare_action、export_action，三个不同的动作必须在清单声明。prepare_when为{field,equals}，引用现有字段决定是否显示准备动作和要求有效计划。公共渲染器不判断tool_id或某个中文模式名称。视频组件替代files/text，不能在同一个媒体视图重复这些输入；start_ms/end_ms/audio_index为公共保留参数名。

v2行子节点可声明stretch为1–4，默认1；窄窗口仍纵向排列。video_editor输出files中的单一源路径及params中的64位毫秒start_ms/end_ms、audio_index。音轨索引来自媒体元信息。业务字段通过普通fields声明，不由宿主构造编码命令。

## 输入与结果

选源触发probe；起止、音轨和导出参数变化只清除plan并保留media。源变化清除整个上下文。异步结果沿用宿主revision/job/session标识检查，过期结果拒绝。恢复草稿先重新probe，再比较保存的源identity，身份一致才恢复原区间，变化则保持新视频整段。

context.media包含source绝对源路径、identity、duration_ms、size_bytes、modified_ms、width/height、rotation、codec、frame_rate、nominal_frame_rate、video_index、audio_tracks[{index,codec}]、source_start_seconds、precise_supported、fast_supported、ignored_streams和warning。所有数据属于现有64KiB context总预算，后端探测原始JSON最多2MiB、轨道最多64条；前端不接收帧数组和完整媒体字节。

context.plan包含fingerprint、start_ms、requested_start_ms、end_ms、fast、output_dir、filename、audio_index和message。fingerprint绑定源身份与本次全部参数，后端export重新验证，不将其作为绕过校验的凭据。prepare_when匹配时，UI要求plan存在，导出前明确确认实际区间。精确操作可以直接export，在后端重新准备。

结果沿用status/message/artifacts/columns/rows/context，输出duration_ms与size_bytes，size_change_percent为相对完整原文件的变化，负数表示变小。最终成功必须发生在完整校验与禁止覆盖提交后。已有完成文件不会因晚到取消被误报成删除或回滚。

## 实现与生命周期

公共控件位于app/ui/components/video_editor.*，Qt Multimedia延迟到有效媒体加载时创建播放器；切换隐藏暂停，浮窗搬移复用同一控件，停止/收回不重复提交。预览是宿主内能力，与独立业务进程隔离范围分别说明。

旧公开1.1.0宿主没有这些能力，将拒绝UI v2。正式宿主1.2.0支持该能力，视频工具min_host_version为1.2.0。不能声称新工具可直接加入旧公开宿主。
