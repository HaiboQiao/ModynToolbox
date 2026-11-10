# UI-R5 品牌与统一界面验收

2026-11-11。用户选定 B Logo，并授权从实用性改进公共功能、控件协调和工具页设计规范。

## 交付

B 版 Logo 已保存为 app/resources/logo.png 母版和七尺寸 modyn.ico，ICO 经 Qt Resource 内嵌宿主，Windows RC 内嵌根 exe 与 App exe。高分辨率 PNG 母版单独保存，避免将无运行用途的母版重复嵌入 exe。应用、主窗与浮窗使用同一图标。

Theme 统一按钮、输入框的文字、内容高度、标准/紧凑留白与状态，统一菜单和下拉表面。UiSpinBox 复用原生步进/校验/键盘/可访问能力，在样式子控件实际区域绘制上下箭头。工具参数和设置都已替换为该组件。

侧栏工具库标题增加数量和共用矢量刷新图标，F5 直接刷新；底部提供会话/记录/设置入口，Ctrl+, 打开设置。既有工具包、备份、恢复与并排操作保留在工作区管理。中央继续由工具业务页面使用。

完整公共功能取舍、后续候选能力、视觉尺度与独立工具设计规则见 ui-design-system.md，AGENTS/README/tool-development 已提供入口。收藏、参数预设等明确为后续设计候选。

## 验证

- 默认 windows-complete 构建成功，Run-Dev.cmd 使用此构建。
- 最终 CTest 11/11，39.88 秒。workbench 17 个测试项通过，包含新增数字步进和密度回归。
- 数字框在 12/14/18 字号及标准/紧凑下验证上下点击、键盘、上下限和按钮/输入高度协调。
- 已查看默认、紧凑、大字号、紧凑大字号、浮窗和真实工具包主窗的 Qt offscreen 截图，箭头可见，刷新使用共用绘制，避免字体缺字。大字号工具名称按既有导航省略并提供 tooltip，可拖宽侧栏。
- 完整工具包独立数据根 13 步通过，包含目录刷新、真实 JSON 格式化、分离/收回和新会话。结果在 .local/logs/UI-R5-actual-report.json。
- Windows API FindResource 核对根 ModynToolbox.exe 与 ModynToolbox.App.exe 均包含 GROUP_ICON 资源。

最终截图 .local/design/UI-R5-actual.png；布局矩阵 .local/design/UI-R5-main.png 及其后缀文件。全量日志 .local/logs/UI-R5-regression-complete.log，控件日志 UI-R5-workbench.txt。

## 异常与验证边界

资源精简后一次规范测试启动时发生访问冲突，旧结果文件曾造成退出阶段的初步误判。加入与其他界面测试一致的离屏字体初始化后仍复现。清除该测试生成的 exe/ilk 并以 LinkIncremental=false 重新链接后，单项与最终全量通过。具体访问冲突根因未确定，处理保留在 UI-R5-view-relink.log、UI-R5-font-check.log 与 UI-R5-resource-check.log。

历史 UI-R4 的工具包启用/派生标记间歇失败在本轮最终回归中未复现，原记录保留。当前结果为本机 Debug 与离屏验证，真实系统 DPI、跨屏、另一台电脑和正式发布包继续按项目目标设备/发布验收处理。
