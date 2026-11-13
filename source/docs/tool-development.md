# 工具开发与接入 v1

## 独立开发边界

每个 tools/<tool_id> 集中自己的 core、standalone、integration、依赖声明和工具构建配置。core 可独立使用，适配层通过进程协议连接宿主。宿主仅负责通用控件、窗口、调度和数据归属。

Python 工具不需要 Qt，C++ 示例工具只用 QtCore 做 JSON 与哈希。其他语言可以直接实现 JSONL 协议并提交 Windows 可执行入口。工具界面遵循公共 UI 描述，语言和依赖完全独立。

宿主前端保持通用。已有文本、文件、表格、图片预览、选择、范围、整数、布尔、目录和文本参数组件的工具只增加自己的目录与适配文件。新的公共控件类型或协议语义需要更新宿主并提高规范版本。

## 最短接入路径

1. 在独立目录开发并验证业务核心，保留 standalone 独立入口。
2. 编写 integration/manifest.json，声明名称、版本、动作、容量和策略。填写 view_file 为 ui.json，protocol_version 与 ui_schema_version 为 1。
3. 编写 integration/ui.json。用字段声明参数，用 row/column 组合公共组件。动作从 manifest 自动生成，动作调用收到 text、files、params、context。
4. 适配层使用 shared/sdk/python/modyn_sdk.py 或 shared/sdk/cpp/modyn_sdk.h。接收动作，调用自己的核心，返回公共结果。
5. 编写 tool.build.json 与依赖锁定记录。Python 可参考三个基础工具，native 可参考 file-hash。Python 工具执行 python scripts/build-tool.py tools/<id>。自定义原生构建可在 tool.build.json 声明 build_script，通用 prepare-tools 会调用该工具自己的脚本。file-hash 使用 python tools/file-hash/build.py。这些入口只生成工具包。
6. 在现有 app 的“工作区管理 → 工具包”安装生成目录。界面由 ui.json 渲染，后端按需启动，无需修改 app 代码、宿主 CMake 或重新编译宿主。
7. 独立测试新会话、浮窗、取消、缺依赖和移除。升级用新 package_version，禁止同版本覆盖安装。

现有工具目录可以从其他工程复制进来。需要适配的是界面说明和输入输出协议，原始业务核心可以继续维护在自己的工程。

## 清单和 UI

规范在 shared/protocol/*-v1.schema.json。运行时还有跨字段校验和容量约束，JSON Schema 是编辑器辅助。

- 标识为小写字母开头，后续小写字母、数字或连字符，最多 64 字符。避开 Windows 保留目录名。版本为三段无前导零整数。
- min_host_version 默认 1.0.0。未来宿主要求高于本版本会拒绝。
- limits 指定 max_files 0–2000、max_input_bytes 1–5242880、memory_mb 64–2048。
- actions 最多 12 项。batch 表示重批处理，write_sources 表示可能写输入文件，cancellable 控制普通取消，timeout_seconds 为 1–3600。
- confirm 设置执行前明确确认文案。requires_context 指定结果 context 中必须存在的字段。编辑输入自动使 context 失效。
- dynamic_resources 用于先读取计划再整体预约源/目标文件的事务动作。写入前调用 SDK reserve，再进入 critical。
- UI 总节点最多 80，深度最多 8，容器最多 16 个子项，每种叶子组件最多一份，参数最多 12 个。
- 参数类型为 choice/range/number/boolean/text/directory。enabled_when 支持引用另一参数并按 equals 或 not 控制可用状态。
- 支持的叶子为 files/text/fields/actions/output_text/output_table/image_preview/status。动作栏和状态栏固定在滚动内容外。
- ui.json 与 manifest.json 各最多 64 KiB。不要在静态说明放入用户私密内容。

## 执行协议

宿主启动可信 WorkerHost，先将其加入 Windows Job Object，再打开执行门闩。入口程序独立运行在私有工作目录，标准输出只用于协议，标准错误计量后丢弃正文。

宿主先发送 run 消息，含 protocol、tool_id、session_id、job_id、revision、action、job_root、data_root、dynamic_resources。输入保存在 job_root/input.json。后端发送 ready，随后可发送 progress、reserve，最后发送 result 并正常退出。事件必须携带相同标识和单调递增 seq。

取消消息为 kind=cancel，资源授权消息为 kind=resources_granted，均携带 job_id。Python SDK 使用无缓冲控制读取，避免后台线程妨碍解释器正常退出。

- 控制消息每行最多 64 KiB，每作业标准输出 4 MiB、标准错误 2 MiB。
- 输入引用最多 16 MiB，结果最多 32 MiB，文本结果最多 20 MiB。
- 表格最多 2000 行、20 列，单元格最多 4096 字符，缩略 PNG 最大 512×512 且 1 MiB。
- result_ref 固定 result.json。可提供 completed/total/failed 的非负整数计数，artifacts 最多 2000 条，context 最多 64 KiB。结果包含 status、message，可含 text、columns、rows、artifacts、context、preview_ref。columns 是 key/label 对象数组，rows 是按 key 索引的对象数组，artifacts 是绝对输出路径字符串数组。
- 状态为 succeeded/failed/cancelled。部分成功保留逐项结果，取消保留已经发布的输出。
- 结果写入先临时文件，再原子替换，写完才发送 result。大结果从文件加载，避免塞入控制消息。

## 资源与数据

全局最多 2 个执行进程组，默认最多 1 个 batch，设置可调整为 2 个，同工具不同版本也串行执行。只读动作可共享资源，写入动作与相关资源互斥。路径和 Windows 文件身份都参与预约，覆盖硬链接别名。预约只约束本宿主调度，文件事务还必须使用实际文件句柄检查身份并拒绝覆盖。

每个 Job Object 限制总提交内存、进程数 16 和 CPU 比率。正常取消合作进行，超时与协议错误终止对应进程树。critical 文件提交期间等待事务结束，异常终止依赖工具自己的 journal 恢复。工具有当前用户权限，进程隔离不构成权限沙箱。

开发工具数据位于 data/tools/<id>，私有作业位于 jobs/<uuid>。参数与会话存储由宿主管理，工具不要写宿主 SQLite。业务数据版本迁移由各工具处理并保留兼容记录。移除代码保留该工具数据。

## 构建和迁移

build-tool.py 将业务代码、适配器、私有运行时和许可打包，生成 checksums.json 和完整性摘要。包不包含本机源码路径。Python 运行时锁定官方 CPython 和 Pillow wheel，在自己的 .local/runtimes 恢复，不改系统 Python。

无需 Python 的原生工具可以只携带自己的 exe 和运行库。不同工具环境不共享 site-packages、当前工作目录和临时目录。重用代码 SDK 不表示共享运行进程。

源码交给其他人时携带本文件、协议、SDK、完整工具目录和依赖锁。普通用户接收构建后的工具包文件夹。

## 导航与文本显示

manifest 可指定 category（最多 40 字符）和 keywords（最多 20 项、每项 80 字符）。工具超过 8 项且有多个分类时显示分类筛选。搜索读取元数据，不创建工具视图。

大文本超过 32768 个 UTF-16 码元时仅绘制只读预览。完整数据继续处理、复制和导出，UI 会提示此策略。宿主内部导出与外部工具作业共用调度、文件租约和关闭/取消规则，不修改工具原结果。

## 独立发布与目录刷新

工具版本与宿主版本独立。先构建带 manifest、ui.json、checksums、完整后端/运行时/许可的工具包，再以 <id>/<version> 为 ZIP 顶层发布。用户将 ZIP 解压到应用 tools 目录，通过管理菜单中的工具包/刷新目录发现。当前整包与独立工具资产同时发布；后续仅工具变更可使用 tools/<id>/v<version> 标签单独分发。流程见 release-policy.md。

新增工具界面遵循 [UI 设计规范](ui-design-system.md)。通过 ui.json 使用公共组件即可继承品牌、字号、密度和状态样式，number 字段使用宿主 UiSpinBox。业务后端继续独立。


## UI v2 媒体工具

video-clip提供独立原生核心、CLI、薄适配器、锁定的媒体引擎及声明式视频界面。UI v2支持video_editor、角色动作、prepare_when和长作业，详见shared/protocol/media-v2.md。已有v1工具无需迁移。新增同类工具可复用该公共能力；宿主1.2.0起支持，旧1.1.0不兼容。

通用tool.build.json新增可选license_files字符串数组，构建时复制到LICENSES并计入完整性索引，同名拒绝；其他配方行为不变。视频工具实际的FFmpeg静态依赖约201MiB，独立工具包按需安装；安装并不等于启动编码进程。


## EXT-1 通用接入增量（本地1.3.0）

UI v3支持动态/级联选项、显示名与固定值、搜索、compact、help/placeholder、advanced。独立工具采用新字段前读取docs/features/host-extensibility/design.md（从源码根定位），同步声明最低宿主1.3.0。旧v1/v2继续兼容。工具进程可声明max_processes（4–64，默认16），用户目录环境由宿主补齐，工具包目录变化自动检查，会话仍固定版本。实际证据见verification/EXT-1-host-extensibility.md。
