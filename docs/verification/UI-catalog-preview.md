# 工具导航与目录发现预览验证

日期为 2026-11-08，Windows 11，本机 Qt 6.8.3 / MSVC 19.51 / CMake 4.3.1。用户要求后台验证，当前未执行桌面控制。

## 实施范围

工具替代任务作为左侧导航单位。搜索只筛选名称与描述，保留当前会话。单份会话隐藏切换器，多份显示切换器。收回浮窗同步导航，独立窗口打开期间主区显示明确位置提示，避免误显示另一会话。

主窗和浮窗调用 DWM 深色标题栏与标题文字颜色。离屏平台跳过 DWM。接口已编译，真实外框视觉本次未观察。

安装根按 <id>/<version>/manifest.json 扫描，不载入后端。上限 64 KiB，校验 schema 版本、工具 ID、目录一致性、纯数字三段版本、名称和描述。每工具取数值最新兼容元数据，忽略无效项并输出诊断。当前不会执行包入口，也不保证此元数据包有可运行后端。

## 命令与结果

执行 powershell -NoProfile -File scripts/prepare-preview.ps1，三个元数据包已准备。Windows PowerShell 5 读取中文清单显式指定 UTF8，避免系统默认编码。

设置 GRAPHITE_PREVIEW_PATH 为项目 .local/preview-tools.png，然后执行 powershell -NoProfile -File scripts/dev.ps1 test -Preset windows-check。
最终 3/3 CTest 通过，耗时 1.38 秒，无跳过。

- workbench 六个测试函数通过，含导航搜索、同工具选择/跨工具收回/搜索无匹配、独立会话输入、分离收回、截图、50 次迁移。
- package_catalog 五个测试函数通过，含发现与删除/新增、100 个元数据包、1.10.0 大于 1.2.0、损坏与超限清单、未知 schema。
- worker_transport 三项 Python/C++ fixture 回归通过。

Qt 每组额外记录 initTestCase/cleanupTestCase。测试明细在 .local/build/windows-check/workbench-results.txt、catalog-results.txt，CTest 日志在该构建的 Testing/Temporary。临时数据位于 tests/tmp，仅操作测试临时根。

已查看 .local/preview-tools.png，1060×700 客户区，中文、左侧搜索、工具列表和工具内操作无可见溢出。截图由同一 Workbench 代码渲染，元数据使用明确的示例夹具，不是业务输出。

## 失败与修复

首次部署缺少 offscreen 插件，测试等待。终止本轮测试进程，补齐 qoffscreend.dll。随后中文断言失败来自编辑测试时使用系统默认编码读取 UTF-8 源码，修复为显式 UTF-8。离屏字体库不自动获取 Windows Fonts，测试显式加载本机字体后再生成预览。新增 CTest 45 秒超时与文本明细输出，最终复验通过。

Qt 部署仍提示翻译元数据、DirectX shader 和 VCINSTALLDIR 缺失，沿用 M0 的发布验证待办。

## 证据边界与后续

此次只支持 C01/C03/C05/C24/C33/C34/C38 的子场景。百工具发现不是性能预算验收，自动测试不证明真实标题栏、键盘焦点、DPI/多屏与系统吸附。三个工具仅有元数据与通用文本预览，专用 UI、业务、进程调度和完整安装规范待开发。T01/T02 均保持进行中，完整验收清单保持未勾选。

接口依据为 [Windows DWM 属性](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwmwindowattribute)、[Qt QWidget](https://doc.qt.io/qt-6/qwidget.html)、[Qt 字体加载](https://doc.qt.io/qt-6/qfontdatabase.html#addApplicationFont) 与 [Qt Test](https://doc.qt.io/qt-6/qtest-overview.html)。当前无可调用 Context7，使用官方文档并通过本机锁定版本编译验证。
