# 当前状态

更新于2026-11-12。本快照为1.2.0发行源码，新增视频裁剪1.0.0和UI v2，最低宿主1.2.0；四个原工具版本保持不变。公开上传和下载复验结果以releases/1.2.0回执及main最新记录为准。

正式依赖下Release14/14，51.11秒；Debug14/14，83.37秒；视频核心20项、启动器3项通过。Debug初轮测试程序在启动阶段异常，清理该测试exe/ilk并非增量重链后单项连续3次及全量通过，具体底层原因未确定。记录见verification/REL-3-publication.md。

视频处理引擎为项目从锁定FFmpeg9.0.2/x264源码构建的9.0.2-modyn1，仅系统DLL依赖，两程序58.23MiB；源码、配方和构建记录随正式资产提供。旧开发Gyan构建不再用于本版。Qt播放器仍为独立的Qt6.8.3/FFmpeg7.1动态依赖。

物理播放/听感、多DPI/Snap、异机、实机空间/ACL故障与复杂4K等仍按设备范围验收。正式发布规程见release-policy，视频原开发记录见verification/VID-1-video-tool.md。

## 以下保留开发与历史版本记录


更新于2026-11-12。本地开发版新增视频裁剪与通用UI v2，已安装到日常工具目录并生成完整本地包。Run-Dev.cmd和Open-ModynToolbox.cmd均可使用本轮功能。当前公开版仍1.1.0，尚未发布视频增量。

最终Release14/14（62.34秒）、Debug14/14（104.02秒）；真实视频核心20项、视频控件8项、便携原业务33步、便携视频34步及崩溃恢复、运行中目录接入11步通过。编码写入后取消0.063秒为本次观测。报告verification/VID-1-video-tool.md，功能文档features/video-clip。

宿主内QGraphicsVideoItem负责预览，独立C++进程和FFmpeg/ffprobe负责业务。旧UI v1保持兼容。物理播放/音画观感、多DPI/Snap、异机、空间/ACL故障和复杂4K等保留待验。首轮Release退出异常及修正经过均在报告中保留。

## 以下为已公开版本与此前阶段背景


更新于2026-11-11。当前公开版1.1.0，GitHub发布与下载复核完成。本版包含B Logo、统一控件、数字箭头、工具库刷新、侧栏快捷入口、启动器说明和设计规范。运行包及源码入口在仓库releases/1.1.0，发布报告见verification/REL-2-publication.md。

固定源提交41fe271450446171c5a90530d771840362cfbf8a，Release ID441095721。Release11/11，20.22秒；Debug11/11，41.10秒。8项资产已公开且匿名下载SHA256一致，下载版33步，3.563秒，运行中目录刷新执行和源码包启动器测试通过。

工具版本为JSON1.0.2、图片1.0.1、重命名1.0.1、文件校验1.0.4。当前未签名，物理多DPI/Snap和另一台干净电脑保留设备验收。

以下保留此前版本与开发验证背景。

## 已公开版本 1.0.2 的历史记录

包含 UI-R3 的左侧会话树、加号、拖出独立窗口、顶部两行品牌和纯工具工作区，以及此前完整的独立后端、调度、草稿、历史、设置、包管理与备份恢复。

产品版本统一从 CMakeLists 读取。随包工具为 JSON 1.0.2、图片 1.0.1、重命名 1.0.1、文件校验 1.0.4。工具保持独立版本，完整便携包与单独工具 ZIP 同时分发。

## 发布与接手入口

- 普通用户使用仓库 releases 或 GitHub Releases 下载完整 ZIP，解压后双击 ModynToolbox.exe。
- 开发者将本目录作为工程根，VS Code 打开 ModynToolbox.code-workspace。
- 任何发布先读 docs/release-policy.md，脚本为 scripts/release.py。
- 版本标签 v1.0.2 对应固定源提交，公开发布回执位于仓库 releases/1.0.2。
- 当前 GitHub 状态和下载复核结果以公开版本页面与该目录回执为准。

## 已公开并完成下载复核

GitHub Release 为 https://github.com/HaiboQiao/ModynToolbox/releases/tag/v1.0.2，ID 439406767。8 个资产上传摘要与本地一致，公开后匿名下载 8 个文件全部匹配，下载版便携业务 33 步通过，3.187 秒。

发布源提交为 8a45e58e2f395e3e9864d54e07fe30b17915acf3，标签 v1.0.2 固定该提交。main 的后续文档回执提交保留发布来源，应用代码一致。

新源码路径 Release 11/11 CTest，21.40 秒。Debug 11/11，40.71 秒。正式 ZIP 校验、源码 Agent 入口、编译版本与运行中工具目录刷新均通过。完整包 64806780 字节，源码包 4201229 字节。

最终回执、SHA256SUMS 和机器可读分发清单位于仓库 releases/1.0.2。报告见 verification/REL-1-publication.md。

## 数据、历史与设备检查

开发数据在 source/data，工具数据在 data/tools/<id>。SDK、缓存、资产和上传进度在 source/.local，排除 Git 与源码 ZIP。旧构建缓存保留隔离。

历史 R2/UI-R3 与首轮 1.0.1 记录保留当时背景。1.0.1 草稿未公开并已清理，原源标签保留。正式 1.0.2 使用已验证的流式上传与草稿恢复。

根 exe 未做 Authenticode 签名，物理多 DPI/Snap、另一台干净电脑仍待目标设备检查。

## 本地启动与发布边界优化

2026-11-10 完成源码启动入口和规则说明的本地更新。根与 source 新增 Run-Dev.cmd，编译运行当前源码；Open-ModynToolbox.cmd 继续定位最近构建的便携版。失败保留提示与退出码，README 和开发指南说明 PowerShell 的正确调用方式。

只有用户明确要求发布才触发正式流程，发布必须同步最新源码、说明、运行资产和回执到 GitHub。规则位于 release-policy，根与 source AGENTS 均已接入。本轮为本地优化，应用版本维持 1.0.2，GitHub 当前公开版与本轮源码增量分开记录。

专项验证 `python app/tests/integration/test_launchers.py` 通过 3/3，2.415 秒。覆盖四个 CMD 入口各自成功/失败分支、中文空格括号路径、便携包定位和缺失提示。通过替身调用核对启动链路，真实 GUI 和源码构建沿用原有验证记录。

## UI-R4 品牌间距与 Logo 预览

2026-11-10 本地源码已调整标题栏品牌区，两行文字按 3 个逻辑像素间距整体居中。默认 windows-complete 已构建，workbench 16/16，布局与截图专项 4/4。已观察默认 1200×780、紧凑 920×600 和独立窗离屏截图。

原 ICO 最大 256px，生成 A 工具箱模块、B 工具箱与 M、C 模块托盘三个 1254×1254 RGBA 候选，保存在 .local/design/logo-concepts。用户选择后继续制作正式资源。报告见 verification/UI-R4-brand.md。

全量 Debug 首轮 10/11，末轮 9/11。full_system 出现不同用例的工具包启用失败，末轮 host_crash_tree 未观察到派生进程标记。旧 Release 对照 full_system 15/15 通过。原因尚未确认，后续需独立诊断工具包目录启用及启动时序。品牌专项已通过，整体全量回归保持未通过状态。

## UI-R5 当前本地增量

2026-11-11 完成 B Logo 接入、统一控件与数字箭头、工具库数量/刷新和侧栏快捷入口，默认开发构建已更新。设计规范见 ui-design-system.md，新工具通过现有 UI 描述继承共用样式。

最终 Debug CTest 11/11，39.88 秒，workbench 17 项通过。真实工具包 13 步通过，Windows exe 图标资源确认。规范测试启动访问冲突经清理生成链接产物后通过，具体根因未确定；历史 UI-R4 间歇失败本轮未复现，记录保留。报告 verification/UI-R5-unified-interface.md。物理 DPI/跨屏与异机继续待设备验收。


## VID-1 视频裁剪规划

2026-11-11 按用户本轮要求完成需求与初版技术方案草案，位于 features/video-clip/spec.md、plan.md、task.md、checklist.md。当前未实现；公开版仍为1.1.0。建议独立视频工具包与通用UI v2视频组件，先验证Qt Multimedia/FFmpeg部署、预览、取消和资源约束，再实施。


## REL-3 发布准备（2026-11-12）

用户明确要求发布并上传GitHub。版本1.2.0，视频工具1.0.0且最低宿主1.2.0。补齐对应源码/构建材料，将通用静态引擎替换为锁定FFmpeg9.0.2与x264的项目构建。正式验证、固定提交、上传和公开下载复核进行中，尚未报告发布完成。
