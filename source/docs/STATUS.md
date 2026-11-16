# 当前状态
<!-- project-status: current -->

先读[当前背景](project-baseline.md)、[行为契约](behavior-contracts.md)及[变更规则](change-policy.md)。本文件只记当前摘要，详细过程在worklog和verification。

- 当前源码1.5.0：蓝色公共壳/图标，图片1.3.1、视频1.3.6、本地翻译1.1.3。当前用户明确要求优化后发布上传，正在执行；公开完成以releases/1.5.0回执及本轮验证记录为准。上一已验证公开版1.4.0记录见[REL-5](verification/REL-5-publication.md)。
- UIR-10源码验证：最终Release20/20（135.85秒）、Debug20/20（216.42秒）、冻结SDK20步通过；实际禁网中英模型推理、视频三尺寸/真实拖动及鼠标/键盘公共动作状态通过，详见[证据](verification/UIR10-blue-offline.md)。正式便携/公开下载复验继续执行。任务见[UIR-10](features/uir10-blue-offline/task.md)。
- SmartScreen已确认原EXE未签名。提供可选正式签名/实际状态检查，无证书仍未签名，不会关闭系统防护；不能宣称已消除系统信誉提示。
- 工具拥有完整前端和独立后端，宿主只保留公共壳/服务；独立SDK及两后端/两前端冻结宿主接入20步通过，新增工具使用既有桥不改宿主。架构边界见[IF-1](verification/IF-1-independent-frontend.md)。
- 公共窗口关闭颜色、设置自启动、单实例见[DS-1](verification/DS-1-desktop-startup.md)；背景/行为/变更保护见[GP-1](verification/GP-1-project-guardrails.md)。
- 实际开发入口、旧会话版本提示、缩放命中、焦点/滚轮与缓存修复见[UIR-8](verification/UIR8-actual-entry.md)。缓存默认2页/5分钟，可在设置调整。
- 缩放重复更新去重、Qt/Web DPI同步、单细分隔及轻留白/贴边、翻译与三工具集见[UIR-9](verification/UIR9-resize-toolset-translation.md)。该历史轮次Release19/19、Debug19/19，SDK20步及目录接入8步通过，本轮结果以UIR-10为准。
- 并行Python启动系统弹窗改动保留且纳入同树整体验证，不替其他会话改任务状态。此前“本地未发布”是历史任务当时事实，本轮发布状态单独记录。
- 物理多屏DPI/Snap、真实登录自启动、IME、另一台干净电脑/跨AI接手、复杂媒体/空间ACL仍需目标设备证据。当前包未签名；本轮完整离线模型与私有CPU依赖随包，默认纯本地翻译，显式选择在线方式才联网。
