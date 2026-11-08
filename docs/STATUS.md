# 当前状态

更新日期为 2026-11-08。用户授权按阶段连续实施，并要求后台验证避免占用桌面。

## 当前阶段

T01 原生试点进行中。T02 已先行实现独立的元数据目录发现子集，完整安装/执行规范待开发。宿主为 C++20 / Qt 6 Widgets。

## 最新成果与证据

- 导航按工具组织，搜索为“搜索工具”。同工具多会话独立，多于一份才显示切换入口。浮窗收回同步左侧选中工具。
- Windows 原生深色标题栏接口已加入并编译，主窗与独立窗共用。此次没有前台原生视觉验收。
- 从显式包根的 <id>/<version>/manifest.json 发现工具，校验元数据与数字版本排序。退出后删除安装工具目录，下次启动不再发现。开发运行不会自动恢复手动删除的工具。
- windows-check 独立后台构建、离屏测试与截图可用。最终 CTest 为 3/3 通过，1.38 秒。界面六项、目录五项与跨语言三项测试通过，另有 Qt 初始化/清理记录。100 个元数据包被发现，尚未测资源预算。
- 接入预览含 JSON、图片压缩、批量重命名，业务处理尚待开发。

证据见 verification/UI-catalog-preview.md。此前真实 GUI 证据见 verification/M0-architecture.md，涉及本轮改动的视觉行为需复验。

## 运行与验证

项目根运行 powershell -NoProfile -File scripts/prepare-preview.ps1，明确准备三个元数据预览包，仅在需要时执行。
随后运行 powershell -NoProfile -File scripts/dev.ps1 run。VS Code 的 F5 使用同一包根与数据根，按键本身待人工验收。
后台验证运行 powershell -NoProfile -File scripts/dev.ps1 test -Preset windows-check，不改写正在运行的 windows-dev 二进制。

源码在 tools，开发安装副本在 .local/installed/tools，数据在 data。勿把源码目录删除等同于卸载安装副本。prepare-preview 是显式预览准备动作，再次执行会补回已删除的示例包。

## 后续工作

继续 T01 的 Windows Job Object、取消/任务通路、界面描述试点、Release 启动器、性能与真实 VS Code/DPI 验收。T02 后续补完整 manifest/view/messages、入口与运行时校验、暂存安装、版本引用和忙碌卸载。T03–T12 待开始。完整 44 项验收均未完成，保持未勾选。
