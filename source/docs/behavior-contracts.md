# 当前行为契约与保护验证
<!-- project-status: current -->

遵循[当前背景](project-baseline.md)。这是既有行为的验证索引，不是重新收窄工具业务自由。保护级别S，关联PB为H；排期高低不改变保护要求。自动测试名称仅表示所覆盖范围，未测设备条件如实列明。

| ID / 关联 | 必须保留的行为 | 自动检查与位置 | 验证边界 |
|---|---|---|---|
| **BC-01** / PB-01 工具可独立设计/接入 | 新工具完整包放入tools即可发现；使用既有能力无需改宿主，页面不被ToolPanel模板限制。 | `web_frontend`, `package_catalog`；[app/tests/integration/test_web_frontend.cpp](../app/tests/integration/test_web_frontend.cpp) | 冻结宿主后独立SDK接入另执行verify-frozen-frontends.py --freeze；不把模拟器当真接入。 |
| **BC-02** / PB-03 会话和窗口迁移 | 多个会话输入独立；分离/收回保持同一会话，不重复提交，浮窗关闭收回。 | `workbench`, `web_frontend`, `jobs`；[app/tests/unit/test_workbench.cpp](../app/tests/unit/test_workbench.cpp) | 100次迁移有原生自动测试；真实IME/多屏焦点另验。 |
| **BC-03** / PB-04 版本与移除 | 旧会话固定包版本；新版本用新目录；移除代码保留工具数据/历史。 | `full_system`, `package_catalog`；[app/tests/integration/test_system.cpp](../app/tests/integration/test_system.cpp) | 运行中手工破坏包的扩大故障矩阵仍待验。 |
| **BC-04** / PB-05 写入/取消/恢复 | 预览失效需重做；原文件/输出按既定工具策略处理；部分完成如实说明。 | `business_cores`, `video_core`, `full_system`；[tools/tests/verify_cores.py](../tools/tests/verify_cores.py) | 真实空间/ACL、复杂媒体和听感不由合成样例替代。 |
| **BC-05** / PB-06 草稿与重启 | 仅明确启用的草稿跨启动保存正文；恢复不自动执行，坏状态保留不覆盖。 | `full_system`, `web_frontend`；[app/tests/integration/test_system.cpp](../app/tests/integration/test_system.cpp) | 真正跨电脑备份恢复单独验收。 |
| **BC-06** / PB-02 隔离与生命周期 | 前端隐藏/重载不重复作业；后端失败归属明确，取消/退出只处理拥有的进程树。 | `worker_faults`, `host_crash_tree`, `jobs`, `web_frontend`；[app/tests/integration/test_faults.cpp](../app/tests/integration/test_faults.cpp) | 不是可信工具权限沙箱；不同工具与同工具组故障边界要区分。 |
| **BC-07** / PB-03 正常启动与系统项 | 正常主程序单实例；重复手动启动唤回，自启动重复静默；不限制工具浮窗/进程。自启动默认不注册，应用才生效，取消不写，保护外来链接。 | `desktop_startup`, `host_start`, `window_native`；[app/tests/integration/test_desktop_startup.cpp](../app/tests/integration/test_desktop_startup.cpp) | 测试用私有scope/Startup目录；真实登录、前台限制、系统策略待设备验收。 |
| **BC-08** / PB-01 公共壳与兼容UI | 公共控件一致，工具页面不被统一布局覆盖；旧UI1/2/3继续可用。 | `view_spec`, `window_native`, `web_frontend`；[app/tests/unit/test_view_spec.cpp](../app/tests/unit/test_view_spec.cpp) | 背景/按钮像素与真实交互、DPI证据分开。 |
| **BC-09** / PB-08 开发/授权范围 | 本轮必要改动可定位，旧工作不覆盖；测试不打扰桌面；只在明确授权时发布/推送。 | `change_guard`；[scripts/change-guard.py](../scripts/change-guard.py) | 自动检查范围/证据，不能核验聊天授权真伪或保证其他AI遵守。 |

## 维护要求

新增或修改功能先判断关联BC，未覆盖的新行为补充可观测验收；测试不能只检查实现细节。不得擅自删除/改弱既有保护条件。测试文件被修改不一定违规，但必须说明保留/替代了什么；规则和测试均由当前任务授权约束。

verify记录当前代码摘要和真实命令结果；后续改代码/测试/SDK/核心规则旧结果失效。测试失败/依赖缺失保持失败/待测，不能以“之前通过”结束。说明与STATUS更新不重跑业务测试，finish仍重新检查文档一致性。纯文档任务不冒充重新验证所有业务。
