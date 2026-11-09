# 当前状态

更新于 2026-11-10。仓库根为 ModynToolbox，完整工程集中 source，发布入口与版本说明集中 releases。

## 当前版本 1.0.2

包含 UI-R3 的左侧会话树、加号、拖出独立窗口、顶部两行品牌和纯工具工作区，以及此前完整的独立后端、调度、草稿、历史、设置、包管理与备份恢复。

产品版本统一从 CMakeLists 读取。随包工具为 JSON 1.0.2、图片 1.0.1、重命名 1.0.1、文件校验 1.0.4。工具保持独立版本，完整便携包与单独工具 ZIP 同时分发。

## 发布与接手入口

- 普通用户使用仓库 releases 或 GitHub Releases 下载完整 ZIP，解压后双击 ModynToolbox.exe。
- 开发者将本目录作为工程根，VS Code 打开 ModynToolbox.code-workspace。
- 任何发布先读 docs/release-policy.md，脚本为 scripts/release.py。
- 版本标签 v1.0.2 对应固定源提交，公开发布回执位于仓库 releases/1.0.2。
- 当前 GitHub 状态和下载复核结果以公开版本页面与该目录回执为准。

## 已公开并完成下载复核

GitHub Release 为 https://github.com/HaiboQiao/ModynToolbox/releases/tag/v1.0.2，ID 439406767。8 个资产上传摘要与本地一致，公开后匿名下载 8 个文件全部匹配，下载版便携业务 33 步通过，3.187 秒。

发布源提交为 8a45e58e2f395e3e9864d54e07fe30b17915acf3，标签 v1.0.2 固定该提交。main 的后续文档回执提交保留发布来源，应用代码一致。

新源码路径 Release 11/11 CTest，21.40 秒。Debug 11/11，40.71 秒。正式 ZIP 校验、源码 Agent 入口、编译版本与运行中工具目录刷新均通过。完整包 64806780 字节，源码包 4201229 字节。

最终回执、SHA256SUMS 和机器可读分发清单位于仓库 releases/1.0.2。报告见 verification/REL-1-publication.md。

## 数据、历史与设备检查

开发数据在 source/data，工具数据在 data/tools/<id>。SDK、缓存、资产和上传进度在 source/.local，排除 Git 与源码 ZIP。旧构建缓存保留隔离。

历史 R2/UI-R3 与首轮 1.0.1 记录保留当时背景。1.0.1 草稿未公开并已清理，原源标签保留。正式 1.0.2 使用已验证的流式上传与草稿恢复。

根 exe 未做 Authenticode 签名，物理多 DPI/Snap、另一台干净电脑仍待目标设备检查。
