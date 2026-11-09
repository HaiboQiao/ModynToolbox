# M0 架构试点实施记录

2026-11-08，T01 进行中。

## 实际产物

原生 Qt 主窗口、两份独立文本会话、新建会话、窗口分离与关闭收回，应用级石墨主题。统一 scripts/dev.ps1 提供 doctor/configure/build/run/test，package 当前明确报告待实现。CMake Preset、VS Code 构建任务与调试配置已建立。

Python 和 C++ 测试执行器通过真实子进程管道收发带作业/会话标识的 JSON，验证中文、正常结果和非法 C++ 输入。它们是传输 fixture，尚未接入 UI 调度和正式工具。

## 环境

Qt 6.8.3 的 qtbase/qttools 由 aqtinstall 3.3.0 下载到 .local/toolchains/Qt。引导依赖精确版本在 scripts/bootstrap.lock.txt，恢复入口 scripts/bootstrap.ps1。使用 VS 18 Community 自带 CMake 4.3.1-msvc1，生成器 Visual Studio 18 2026，x64 Debug。当前 Python 测试解释器 3.13.9。

## 执行与结果

- powershell -NoProfile -File scripts/dev.ps1 test，最终 2/2 CTest 通过，耗时 2.75 秒。
- workbench 包含独立输入、分离/收回与可见性、50 次迁移三项行为。
- worker_transport 包含 Python、C++ 的中文 JSON 往返与 C++ 非法输入拒绝三项行为。
- 通过真实桌面 UI 观察主窗、分离窗、主题、内容可见和关闭收回。
- git check-ignore 实测 data/host/instance.lock 和项目 Qt 工具链路径受忽略。

完整构建输出位于 .local/logs/build-test.log，CTest 日志在 .local/build/windows-dev/Testing/Temporary/。

## 实测发现与修复

第一轮对象/状态测试通过，但 GUI 观察发现 QStackedWidget 移出视图仍为隐藏状态，浮窗为空白。修复显式显示与应用级主题，增加可见性断言，复测及 GUI 观察确认内容和主题正常。

跨语言中文测试发现 Python 默认 stdout 编码与 UTF-8 协议不一致。显式设置 stdin/stdout UTF-8 后复测通过。

Qt 部署工具提示翻译元数据、DirectX shader DLL 和 VCINSTALLDIR 信息不足。当前 Widgets Debug 程序与测试可运行，Release 分发依赖和干净机器验证仍须单独完成。

## 尚待验证

T01 的进程树启动期归属、宿主崩溃后的子进程回收、取消/队列、动态界面描述、100 工具目录、性能预算、Release 启动器和离线包仍待实现。真实 VS Code F5、完整最小化/多 DPI/多屏测试待执行。

原型侧栏和编辑器是架构试点布局，后续 M1 按最终工具导航/界面规范完善。当前没有真实重命名/图片/JSON 业务能力。

该证据只支持部分 C03/C05/C09/C30/C38 的子场景，完整 C 项保持待验。
