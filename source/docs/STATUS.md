# 当前状态
<!-- project-status: current -->

先读[当前背景](project-baseline.md)、[行为契约](behavior-contracts.md)及[变更规则](change-policy.md)。本文件只记当前摘要，详细过程在worklog和verification。

- 当前源码1.4.0；默认图片压缩/视频裁剪1.2.3、文本翻译1.0.3。文件校验/JSON/重命名默认停用，原数据/源码保留。已按用户本轮明确授权发布v1.4.0并上传GitHub，10项公开附件匿名下载校验与实际运行复验通过；固定来源/完整结果见[REL-5](verification/REL-5-publication.md)及仓库releases/1.4.0回执。
- 工具拥有完整前端和独立后端，宿主只保留公共壳/服务；独立SDK及两后端/两前端冻结宿主接入20步通过，新增工具使用既有桥不改宿主。架构边界见[IF-1](verification/IF-1-independent-frontend.md)。
- 公共窗口关闭颜色、设置自启动、单实例见[DS-1](verification/DS-1-desktop-startup.md)；背景/行为/变更保护见[GP-1](verification/GP-1-project-guardrails.md)。
- 实际开发入口、旧会话版本提示、缩放命中、焦点/滚轮与缓存修复见[UIR-8](verification/UIR8-actual-entry.md)。缓存默认2页/5分钟，可在设置调整。
- 缩放重复更新去重、Qt/Web DPI同步、单细分隔及轻留白/贴边、翻译与三工具集见[UIR-9](verification/UIR9-resize-toolset-translation.md)。当前发布前Release19/19、Debug19/19，SDK20步及目录接入8步通过。
- 并行Python启动系统弹窗改动保留且纳入同树整体验证，不替其他会话改任务状态。此前“本地未发布”是历史任务当时事实，本轮发布状态单独记录。
- 物理多屏DPI/Snap、真实登录自启动、IME、另一台干净电脑/跨AI接手、复杂媒体/空间ACL仍需目标设备证据。当前包未签名，文本翻译按所选在线/自建服务联网，不自带大型离线模型。
