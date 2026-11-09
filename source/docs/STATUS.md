# 当前状态

更新于 2026-11-10。仓库根为 ModynToolbox，完整工程集中 source，发布入口与版本说明集中 releases。

## 当前版本 1.0.1

包含 UI-R3 的左侧会话树、加号、拖出独立窗口、顶部两行品牌和纯工具工作区，以及此前完整的独立后端、调度、草稿、历史、设置、包管理与备份恢复。

产品版本统一从 CMakeLists 读取。随包工具为 JSON 1.0.2、图片 1.0.1、重命名 1.0.1、文件校验 1.0.4。工具保持独立版本，完整便携包与单独工具 ZIP 同时分发。

## 发布与接手入口

- 普通用户使用仓库 releases 或 GitHub Releases 下载完整 ZIP，解压后双击 ModynToolbox.exe。
- 开发者将本目录作为工程根，VS Code 打开 ModynToolbox.code-workspace。
- 任何发布先读 docs/release-policy.md，脚本为 scripts/release.py。
- 版本标签 v1.0.1 对应固定源提交，公开发布回执位于仓库 releases/1.0.1。
- 当前 GitHub 状态和下载复核结果以公开版本页面与该目录回执为准。

## 本地验证

source 新路径已全新构建。Release 11/11 CTest，20.37 秒。Debug 11/11，39.60 秒。最低宿主版本与 GUI/CLI 版本一致性回归通过。

候选完整包经 ZIP 解压与摘要核对，编译程序显示 1.0.1。已启动空工具宿主，运行中将独立文件校验 ZIP 放入 tools 后刷新发现并执行，结果正确。路径越界 ZIP 被拒绝。

验证记录见 verification/REL-1-preflight.md。正式分发清单与公开下载回执在发布流程中生成，绑定具体源提交。

## 数据与生成物

开发数据位于 source/data，工具数据在 data/tools/<id>，工具源码仍集中 tools/<id>。SDK、缓存、包和发布资产集中 source/.local，排除 Git 与源码 ZIP。旧路径构建缓存已保留隔离并重新生成。

历史 R2/UI-R3 记录保留当时路径与测量背景。当前物理多 DPI/Snap、另一台干净电脑仍待目标设备检查。本版根 exe 未做 Authenticode 签名。
