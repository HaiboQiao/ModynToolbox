# UI-R4 品牌区与 Logo 候选

2026-11-10。本轮按用户小需求直接修改授权落实品牌间距，Logo 先展示供选择。

## 实现与依据

ChromeWindow 的品牌 QVBoxLayout 使用 AlignVCenter，将两行文字作为自然高度的整体居中，Theme::titleBrandSpacing 为 3 逻辑像素。主窗与独立窗共用实现。实际框架为 C++20 / Qt 6.8.3 Widgets，Theme/QSS 管理样式。当前无可调用 Context7 工具，布局语义核对 Qt 官方文档。[Qt QLayout](https://doc.qt.io/qt-6.8/qlayout.html)

## 实际验证

- scripts/dev.ps1 build 默认 windows-complete 成功，用户 Run-Dev.cmd 的入口构建已更新。
- workbench 16/16 通过。新增 compactBrandLayout 在 12/14/18 字号下检查主窗与独立窗的精确行间距、自然高度和整体居中。
- savePreview、compactBrandLayout 专项含初始化/清理 4/4，1.422 秒。
- 已查看 Qt offscreen 生成的 1200×780 主窗、920×600 紧凑窗、840×620 浮窗及标题栏局部截图。输出 .local/design/UI-R4-main.png 及其 .compact.png、.floating.png、.brand.png 后缀文件。
- 原生鼠标、系统缩放和跨屏沿用项目目标设备待验项，本轮证据为离屏渲染。

## 全量回归异常

Debug 首轮 10/11，40.19 秒，full_system 有两个安装用例报“工具包启用失败，已有版本保留”。独立复跑仍失败，但失败用例变化。末轮 9/11，40.90 秒，full_system 为一个工具包启用失败，host_crash_tree 为 Owned host never launched descendant。工具包错误来自 PackageManager 暂存目录启用分支，其中一轮在创建窗口前的 packageInstallVerifyRemove 已出现，错误原因尚未确定。

本轮未改动的旧 Release 对照 full_system 15/15，7.424 秒，CTest 总计 7.53 秒。保留差异，后续独立诊断目录启用与进程启动时序。全量回归状态保持未通过，不能用专项结果替代。

日志位于 .local/logs/UI-R4-tests.log、UI-R4-system-retry.log、UI-R4-tests-final.log、UI-R4-baseline-system.log、UI-R4-layout-final.txt。

## Logo 预览

用户 modu_app.ico 包含 16/24/32/48/64/128/256px 图层。其工具箱和模块含义清晰，多色与加号在小尺寸下信息密集，蓝底与当前紫色品牌存在差异。

使用内置 imagegen 基于最大原始图层生成三个 1254×1254 RGBA 预览。A 保留工具箱加三个模块，B 融合工具箱与 M，C 使用托盘和模块。统一紫白主色、减少细节。候选与完整提示词在 .local/design/logo-concepts，均为等待选择的概念稿。后续选定后需精修边缘、逐尺寸简化并制作正式 ICO/应用资源。
