# 宿主模块

本目录承载 C++20 / Qt 6 Widgets 宿主。当前仅有说明，源码由 T01 起建立。

catalog/ 管理工具包，ui/ 管理石墨组件和界面描述，core/ 管理会话与状态，jobs/ 管理调度和协议，platform/windows/ 管理启动与进程树，storage/ 管理产物归属。各模块的头文件和实现放同目录，宿主测试集中 tests/，Windows 轻量发布入口放 launcher/。

宿主业务边界、未来文件与验证入口见 [架构](../docs/plan.md) 和 [任务](../docs/task.md)。模块目录按实际实施建立。
