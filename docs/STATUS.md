# 当前状态

更新日期为 2026-11-08。用户已授权最终核对后按开发阶段实施。

## 当前阶段

T01 进行中，原生 C++/Qt 试点已可构建运行。已经提供 VS Code F5 配置和统一脚本，CLI 构建/运行/测试已执行。工具箱业务、动态界面规范、完整调度与工具安装系统仍待后续阶段。

## 已验证

Qt 6.8.3 msvc2022_64，Visual Studio 18，CMake 4.3.1-msvc1。独立 Qt SDK 与 aqt 引导环境在 .local 下。两组 CTest 通过，包含三项窗口行为测试和三项 Python/C++ 管道测试。真实 Windows 已观察主窗口、分离后的可见内容/主题和关闭收回，发现并修复浮窗隐藏问题。

已初始化 Git 并建立本地基线，data 和 .local 的实际忽略行为已验证。报告见 [M0](verification/M0-architecture.md)。

## 现在如何运行

在项目根运行 powershell -NoProfile -File scripts/dev.ps1 run。VS Code 打开 GraphiteToolbox.code-workspace，选择 GraphiteToolbox 调试配置，F5 会调用同一构建入口。VS Code 按键本身尚待人工验证，当前运行中的原型可直接试用。

新电脑先安装 VS 2026 C++ 工具与 SDK、Python，再运行 scripts/bootstrap.ps1 恢复项目 Qt SDK。也可用 GRAPHITE_QT_ROOT 指定兼容 SDK。

## 明确下一步

继续 T01。实现并验证 Windows Job Object 启动期归属、受控后台任务、界面描述试点、最小 Release 启动器/包、资源基线和 VS Code 实际 F5。T01 尚未完成，T02–T12 待开始。完整验收清单保持未勾选，已测子集以 M0 报告说明。
