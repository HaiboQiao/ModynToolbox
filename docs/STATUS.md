# 当前状态

更新于 2026-11-09。有效项目为 D:/Workspace/01_Projects/ModynToolbox。

## 最新交付 UI-R3

已将用户提出的会话分支、顶部两行品牌和拖出独立窗口落实到程序。左侧每工具的 ＋ 创建空白会话，多会话显示细折线分支。右键管理会话，Ctrl+N 在导航中创建。展开状态保存。主工作区只保留工具内容，原公共标题、会话选择器和按钮已迁出。

顶部显示 ModynToolbox 和“本地工具工作台 · v1.0.0”。浮窗第二行显示工具与会话名称。拖父工具移出最近会话，拖分支移出指定会话，在主窗口外松开完成，Esc/窗口内松开取消，关闭浮窗收回。

默认 run 对应 windows-complete Debug 已更新，最新便携版也已更新。退出旧窗口后运行原命令即可查看。

## 最近验证

- Release 11/11 CTest，22.52 秒。Debug 11/11，41.14 秒。
- 新增加号/分支/工作区边界与拖出/取消/收回回归，展开恢复和右键重命名通过。
- 实际主窗 1200×780、最小窗 760×480、浮窗 840×700 离屏截图已检查。
- 最新便携包 33 步通过，工具接入 28 步通过，冻结后 97 个宿主文件不变。
- 发行目录 .local/releases/ModynToolbox-1.0.0-f2b221c8，309 项文件摘要一致，初始 data 为空。

最新报告见 verification/UI-R3-navigation.md。此前完整后端、数据、性能与业务交付见 verification/R2-complete.md，其冻结和测量记录属于当时版本。

## 打开与接手

根 Open-ModynToolbox.cmd 打开最近便携版。开发命令为 powershell -NoProfile -File scripts/dev.ps1 run。默认构建路径为 .local/build/windows-complete/bin/Debug/ModynToolbox.App.exe，VS Code F5 使用同一路径。

四工具、独立进程、版本包、取消、草稿、历史、设置和备份恢复继续保留。新增兼容工具仍通过自己的目录和适配接入。项目入口与接手方法见 README、development、handoff 和 tool-development。

## 设备检查

本轮采用离屏渲染、合成输入事件和隐藏原生句柄测试。真实桌面跨屏拖出、多 DPI、Snap、系统对话框、实际 F5 与干净电脑仍由目标设备完成检查。
