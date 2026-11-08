# ModynToolbox实施任务

2026-11-08 更新。用户授权按阶段开发并继续下一阶段。T01 进行中，T02 的元数据目录发现先行实施，其他任务待开始。当前已有原生试点和部分测试，完整完成范围以 STATUS 与验证报告为准。

依据为 [需求](spec.md) 和 [架构设计](plan.md)。文中路径均相对本项目。验证入口由相应任务创建，执行前确认用例实际收集，跳过不能计通过。

## T01 新工程环境、VS Code 启动与架构试点

状态为进行中，已完成可运行原型及部分验证，见 verification/M0-architecture.md。覆盖 F04,F05,F16–F18,F20–F24,N08–N11，先行 AC04,AC18,AC20–AC24。

新建根 CMakeLists.txt/CMakePresets.json、本机配置流程、scripts/dev.ps1、.vscode/tasks.json/launch.json、app/main.cpp、app/launcher/、app/tests/fixtures/workers/ 与 docs/verification/M0-architecture.md。项目已经有规划和目录说明，源码从零建立。依赖无，契约见 plan §1、§4–7、§10、§13–18 及 development §2–6。

1. 建立本地 Git 和忽略校验，确认既有文档与引用。诊断实际 MSVC/SDK/Qt/CMake，锁定兼容组合与恢复说明。机器路径进入 .local/machine.json，公共预设保持可移植。
2. 创建统一 doctor/configure/build/run/test/package 的首批可执行入口。VS Code F5 与 CLI 使用相同预设/参数，运行数据进入项目/data，生成物进入 .local。
3. 用有界 UI 描述展示表格/表单和文本视图，创建主区、两会话、正常独立窗和分离/收回，保持输入、选择和滚动。
4. 在工具箱之外独立运行 Python 和编译型测试核心，再通过薄适配接入。验证初始化之前进程树归属、派生子进程、取消、异常与宿主结束。
5. 实测启动/内存、多窗、100 个登记项与 parked 会话，建立资源预算建议。构建最小便携包，验证根启动器、app 内 DLL、tools/runtimes/data 定位。
6. 更新 STATUS、worklog、环境清单和 M0 报告。真实执行后才把启动命令标为可用。

验证入口为创建后的 scripts/dev.ps1 doctor/build/run/test/package 与 VS Code F5。对同一参考输入比较两入口。测试目录位于 .local/tests，真实 GUI 检查最小化、Alt+Tab、状态和数据路径。记录实际命令、收集用例数、失败与缺口。

完成条件为新项目可从 VS Code 和 CLI 运行原型，跨语言/窗口/进程树与最小发布有证据。若资源目标不满足先定位原因，必要时用同一协议样本比较候选宿主，记录决定后再推进。

## T02 工具包、协议规范与版本维护

状态为进行中，仅完成目录元数据预览子集，见 verification/UI-catalog-preview.md。覆盖 F07,F16,F17,F19。AC07,AC17,AC19。用户本轮要求继续下一阶段，因此先行处理独立于 T01 剩余进程/发布试验的目录扫描工作。完整 T02 仍需满足 T01 相关依赖及下述完成条件。

新建 `shared/protocol/{manifest-v1,view-v1,messages-v1}.schema.json`、`app/catalog/`、`app/core/contracts.h`、三个工具包的 manifest/UI 初稿、`app/tests/unit/test_package_catalog.cpp` 和 `app/tests/integration/test_package_install.cpp`。依赖 T01，契约见 plan §3、§9。

实现包兼容、入口路径、runtime_ref、动作/字段引用和能力检查。导入使用受限暂存与版本目录，激活失败保留旧版。会话和作业固定版本，忙碌包/被引用环境拒绝卸载。测试包仅显示明确的待接入视图。

验证入口为 `ctest --test-dir .local/build/windows-dev -R "package_catalog|package_install" --output-on-failure`。准备有效、未知版本、路径越界、缺入口、不完整和重复版本包，检查启用状态及磁盘。安装第四包后目录无需重新编译。

完成条件为规范有真实校验调用方，本地版本导入/固定/移除行为符合 spec，包摘要校验不被说成来源信任验证。

## T03 雾紫原生组件与界面描述渲染

状态为进行中，本轮先实现已批准的视觉与有界 UI 预览子集，完整规范继续依赖 T02。覆盖 F01,F03,F11,F12,F16,N02,N07,N09。AC01,AC03,AC14,AC17。

新建 `app/ui/{main_window,navigation,workspace,theme,view_spec_validator,native_view_renderer}.h/.cpp`、`app/ui/components/`、本地图标及许可记录、`app/tests/unit/{test_view_spec,test_navigation}.cpp`。依赖 T02，契约见 plan §4、§11。

实现受控组件树、字段/动作绑定、有限条件、表格模型/代理、文本编辑与图片预览角色。按用户选择落实搜索在左、单层标题、条件任务入口和无底部会话列表。未知组件/错误绑定给局部错误，UI 描述与结果均有负载限制。

验证入口为 `ctest --test-dir .local/build/windows-dev -R "view_spec|navigation" --output-on-failure`。注入超深树、未知组件、超限节点、无匹配搜索和结果增量洪泛。真实图形环境观察雾紫主题、键盘、DPI 和两种布局。

完成条件为同一组件体系可渲染三类界面，主程序不按工具 ID 分支，输入状态与数据模型分开。

## T04 多会话、视图归属与独立窗

状态为待开始。覆盖 F02,F03,F04,F06,F20。AC02,AC03,AC04。

新建 `app/core/session_store.h/.cpp`、`app/ui/window_manager.h/.cpp`、`app/tests/unit/{test_sessions,test_window_lifecycle}.cpp`，修改 workspace。依赖 T03，契约见 plan §3、§10。

实现包版本固定、独立输入修订、parked/main/floating 位置、主区两槽和所有会话选择器。迁移捕获/恢复滚动、选择及焦点，关闭独立窗收回，暂时隐藏不触发应用退出。parked 可释放视图但保持作业连接。

验证入口为 `ctest --test-dir .local/build/windows-dev -R "sessions|window_lifecycle" --output-on-failure`。真实 Windows 检查 Alt+Tab、主窗最小化、全会话分离和收回。重复 50 次创建/关闭并记录活跃视图数与内存趋势。

完成条件为唯一可见视图、状态保留和任务归属正确，视图释放不删除会话。

## T05 跨语言作业、进程树与资源管理

状态为待开始。覆盖 F05,F11,F13,F18,F20,N03,N06。AC05,AC11,AC18,AC20。

新建 `app/jobs/{scheduler,worker_supervisor,protocol,resource_lease}.h/.cpp`、`app/platform/windows/{process_launcher,job_object}.h/.cpp`、`shared/sdk/python/`、`app/tests/unit/{test_protocol,test_job_states}.cpp`、`app/tests/integration/{test_workers,test_resource_leases,test_process_tree}.cpp`。依赖 T01,T02,T04，契约见 plan §5–7。

整合 M0 进程方案，实现控制/数据分离、输入快照、整体资源预约、启动握手、异步管道、消息有界、进度合并、取消、候选终态与正常退出确认。短交互优先队列保留名额，默认两执行槽且长批处理最多一份。每作业管理自己的进程树，先检查归属再执行工具。两种不同版本测试依赖放入独立环境，实际导入后得到不同结果。

验证入口为 `ctest --test-dir .local/build/windows-dev -R "protocol|job_states|workers|resource_leases|process_tree" --output-on-failure`。覆盖坏行/半行/超长/乱序/过时事件、日志洪泛、无终态退出、候选成功后异常退出、启动时派生子进程、归属失败及父进程受控终止。三作业排队、图片组和同路径争用正确。

完成条件为异语言、依赖版本与进程树隔离有实际证据，失败不会覆盖其他会话，完整输出和部分效果可定位。

## T06 宿主与工具数据、恢复和退出

状态为待开始。覆盖 F06,F14,F17,F19,N04,N06。AC06,AC12,AC15,AC19。

新建 `app/core/state_repository.h/.cpp`、`app/storage/artifact_store.h/.cpp`、`app/app_lifecycle.h/.cpp`、`app/tests/unit/{test_state_migration,test_artifact_store}.cpp` 和 `app/tests/integration/test_restore_shutdown.cpp`。依赖 T04,T05，契约见 plan §7–10。

实现宿主数据库单写、工具私有目录、草稿策略、快照/结果引用、日志限额与带归属清理。备份迁移与更高版本提示、移除显示器后位置恢复、退出一次汇总、中断 journal 显示及固定工具版本恢复。

验证入口为 `ctest --test-dir .local/build/windows-dev -R "state_migration|artifact_store|restore_shutdown" --output-on-failure`。检查有/无草稿、损坏库、缺包/缺环境、缺文件、越界引用、目录别名、仍存活进程和中断写入。所有破坏性故障仅操作测试目录与进程。

完成条件为数据归属明确，临时输入按策略清理，未解决 journal 和用户导出不误删，恢复不自动重跑。

## T07 JSON 工具纵向交付

状态为待开始。覆盖 F10,F16,F17,N05,N06。AC10,AC16,AC18。

完善 `tools/json/integration/{manifest.json,ui.json}`，新建 `tools/json/{core,standalone,integration/adapter}/`、独立依赖锁/环境构建说明、`tools/json/tests/`、`app/tests/integration/test_json_tool.cpp`。依赖 T03,T05,T06，契约见 plan §4、§5、§12.3。首版建议 Python 处理器，语言选择属于工具自身。

实现粘贴/载入、严格校验、行列定位、重复键警告、保留 token 格式化、结果修订/导出。通过公共协议接入，worker 不导入宿主 Qt UI。

验证在该工具项目测试环境运行 `python -m pytest tools/json/tests -q`，再执行 `ctest --test-dir .local/build/windows-dev -R json_tool --output-on-failure`。覆盖 BOM、NaN/Infinity、深度、5 MiB 边界、精度/转义、旧修订结果、分离与导出。工具环境实际解释器路径记录于其构建说明。

完成条件为原始 token 保留，结果来自真实独立后端，错误保持用户输入。

## T08 图片工具纵向交付

状态为待开始。覆盖 F09,F16,F17,N05,N06。AC09,AC16。

完善 `tools/image/integration/{manifest.json,ui.json}`，新建 `tools/image/{core,standalone,integration/adapter}/`、锁定 Pillow 的独立环境/许可说明、`tools/image/tests/`、`app/tests/integration/test_image_tool.cpp`。依赖 T03,T05,T06，契约见 plan §4、§5、§12.2。

实现扫描、格式/质量/尺寸/输出目录、方向和透明处理、同目录临时发布、防覆盖、逐文件结果及取消。预览采用受限缩略图，后端原图解码内存与宿主分开统计。

验证在该工具测试环境运行 `python -m pytest tools/image/tests -q`，再执行 `ctest --test-dir .local/build/windows-dev -R image_tool --output-on-failure`。覆盖坏图/动图/透明/方向、输入上限、同名竞争、取消、原图哈希和输出解码。

完成条件为处理期间其他会话可用，实际输出与部分结果一致，缓存按预算回收。

## T09 重命名工具、记录与恢复

状态为待开始。覆盖 F08,F13,F16,F17,N05,N06。AC08,AC12,AC16。

完善 `tools/rename/integration/{manifest.json,ui.json}`，新建 `tools/rename/{core,standalone,integration/adapter}/`、独立环境说明、`tools/rename/tests/`、`app/tests/integration/test_rename_tool.cpp`。依赖 T03,T05,T06，契约见 plan §5、§7、§12.1。首版可选 Python，关键文件语义必须实际验证。

实现排序/规则、只读计划及失效、确认 plan_id、源身份复验、两阶段移动、每步 journal、失败恢复和尝试撤销。恢复由该工具明确动作处理，宿主只展示公共恢复状态，不重写工具算法。

验证在该工具测试环境运行 `python -m pytest tools/rename/tests -q`，再执行 `ctest --test-dir .local/build/windows-dev -R rename_tool --output-on-failure`。覆盖交换/大小写、Unicode/空格、占用/权限、外部替换、目标抢占、各阶段中断与撤销冲突，逐项核对文件位置和哈希。

完成条件为磁盘、计划和日志一致，失败恢复不覆盖第三方内容，普通退出等待提交。

## T10 便携数据与开发迁移

状态为待开始。覆盖 F21,F24,N11。AC21,AC24。

新建 scripts/data/ 导出/导入入口、app/storage/ 迁移处理、app/tests/integration/test_portable_migration.cpp 和 docs/verification/M3-migration.md，完善 development/handoff。依赖 T06,T07，契约见 plan §8、§16–18。

实现规范数据根、实例锁、一致性备份/恢复、格式与完整性清单、版本预检、暂存启用及失败保留。便携包和开发数据路径分别测试，外部资料允许重定位并复验。SQLite/WAL 和待恢复 journal 必须覆盖。

验证入口为 ctest --test-dir .local/build/windows-dev -R portable_migration --output-on-failure，加真实不同盘符/中文路径、只读目录、占用实例和新机器。确认旧缓存重建，数据和工具版本关联正确，没有自动重跑写入。

完成条件为程序/数据迁移有可复现证据，损坏或不兼容导入保留原数据，开发迁移说明能实际执行。

## T11 可复现交接与记录验收

状态为待开始，当前已建立文档基础，实际开发接手验收待实施。覆盖 F22,F23,F24,N10,N11。AC22–AC24。

完善 AGENTS.md、README、STATUS、development、handoff、decisions、worklog。建立 app/tests/integration/test_project_layout.cpp 和 docs/verification/M3-handoff.md。依赖 T01,T07,T10，契约见 plan §13、§16–18。

检查 Git 跟踪/忽略边界和实际脚本入口，准备不依赖旧聊天与个人 Skill 的接手包。接手者读取说明后定位当前任务，在新的源码路径检查环境并执行构建/运行/必要测试。若没有不同 Agent 可用，记录本地冷读和新路径结果，跨 Agent 实际测试保持待验。

验证入口为项目布局/路径测试、全新路径开发恢复、handoff 清单和真实记录核对。检查公共文件没有活动的本机硬编码，未提交内容交接完整，用户数据未误纳入源码分享。

完成条件为当前状态、任务、命令、证据和下一步一致，接手者能按文件恢复开发。自动读取与图形环境缺口真实说明。

## T12 完整扩展、发布和资源回归

状态为待开始。覆盖所有 AC，重点 AC07,AC13,AC17–AC20。

新建 `app/tests/integration/{test_workbench_e2e,test_extension_install}.cpp`、测试第四包、`scripts/package/`、依赖与许可清单、正式使用说明，生成 `.local/releases/` 与 `docs/verification/M3-release.md`。依赖 T07,T08,T09,T10,T11，契约见 plan §9、§13–15。

执行完整行为和故障回归，第四包在已构建宿主中新增并执行，无需重编宿主。验证独立版本更新/忙碌卸载、兼容环境去重和冲突环境。构建 Release 文件夹包，干净普通账户断网、无系统 Python/开发环境运行实际三工具。

验证入口为 `ctest --test-dir .local/build/windows-dev --output-on-failure`、三个工具各自测试，以及 checklist 全部真实 Windows 场景。重复 M0 的性能样本，按已确定预算判断，报告宿主/整树/运行时/包体分别数据。Qt 部署器收集宿主 Qt 依赖，各工具包由其自身构建，不以 PyInstaller 打包宿主。

完成条件为全部必需项有实际证据，跳过/环境缺失保留未验证，正式包排除测试 fixture，README 填写实际运行与构建命令。

## 里程碑

| 阶段 | 任务 | 成果 |
|---|---|---|
| M0 架构试点 | T01 | 原生窗、异语言和环境、进程树、发布与资源数据 |
| M1 公共工作台 | T02–T06 | 版本包、组件、会话、后台任务、持久化与恢复 |
| M2 三工具 | T07–T09 | 独立后端与完整文件/文本流程 |
| M3 交付 | T10–T12 | 扩展证据、离线包、多 DPI、资源回归及说明 |

任务进度只在本文件维护，实际验收状态在 checklist，证据在 docs/verification。所有任务目前待开始。

## 本轮执行包 UI-R1

用户明确批准新设计并要求统一更名。先完成目录/产品名/环境变量/构建目标及文档更名，再实现公共主题、图标、窗口外框和元数据驱动的文件/文本预览。覆盖 F01,F02,F04,F12,F16,F21–F23 的局部范围。验证入口为 scripts/dev.ps1 test -Preset windows-check。回归包含搜索、独立输入、分离收回、窗口控制、UI 限额、旧清单回退、重命名后全新构建与新品牌扫描。业务与系统交互验收分别记状态。

UI-R1 已完成更名、主题/外框、独立 UI 预览与阶段回归。旧根已清理，真实系统交互仍待验收，见 verification/UI-R1-ModynToolbox.md。T03 的完整规范与组件树未完成。
