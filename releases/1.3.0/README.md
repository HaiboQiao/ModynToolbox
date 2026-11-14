# ModynToolbox 1.3.0

Windows x64，2026-11-14。面向独立工具开发改进宿主通用接入能力，完整包内保留原有五个工具。完整解压后双击ModynToolbox.exe，无需另装Python、Qt或FFmpeg。

## 下载

- [Windows完整便携包](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.3.0/ModynToolbox-1.3.0-windows-x64.zip)
- [源码与AI接手文档](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.3.0/ModynToolbox-1.3.0-source.zip)
- [第三方对应源码与构建材料](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.3.0/ModynToolbox-1.3.0-third-party-source.zip)
- [SHA256校验](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.3.0/SHA256SUMS.txt)

附件还提供五个独立工具包及媒体引擎归档，普通用户下载完整便携包即可。

## 新增与优化

- 缺包会话提供“用可用版本新建会话”，保留旧版本身份与草稿，不隐式迁移参数或执行计划。
- UI v3新增分析context驱动的动态与级联下拉、显示名/固定值、搜索、短字段并排、字段说明和默认收起的高级设置。窄窗口回单列。
- 工具目录稳定变化后自动刷新，保留F5。侧栏工具与会话分支采用名称/版本两行，区别新建版本和会话锁定版本。
- 工具进程补齐Windows用户目录环境，缺失时使用原生用户目录接口；保留私有依赖路径及任务TEMP。
- limits.max_processes可声明4–64，默认16，继续由Job Object管理进程树取消和退出。

## 工具开发与升级

旧UI v1/v2继续兼容。新能力需要工具自身声明ui_schema_version=3、view_version=3、min_host_version=1.3.0。请阅读[接入设计与可复制例子](https://github.com/HaiboQiao/ModynToolbox/blob/v1.3.0/source/docs/features/host-extensibility/design.md)。使用已有公共控件时，只修改工具目录内的界面/适配与业务，不需要修改宿主。

工具ZIP完整解压到tools形成tools/<id>/<version>。推荐先复制到点号暂存目录，完成后更名，避免复制中被发现；自动检测的元信息稳定不代替执行前完整性校验。移除前关闭引用会话，保留data/tools/<id>。应用升级解压到新目录，保留旧版备份，按工作区备份/恢复迁移状态。

随包版本保持JSON1.0.2、图片1.0.1、重命名1.0.1、文件校验1.0.4、视频裁剪1.0.0。媒体引擎9.0.2-modyn1不变，视频工具最低宿主1.2.0。工具源码与完整构建包用途不同。

## 验证与边界

本轮功能最终Release14/14（52.40秒）、Debug14/14（87.53秒），覆盖动态字段/草稿、自动发现与缺包恢复、真实用户环境及32预算下20子进程。正式ZIP与公开下载均通过原有33步、视频专项34步、目录接入11步、实际编码取消/恢复；UI v3派生工具接入12步通过。11项公开附件已匿名下载核对SHA256，源码包启动器3项通过。

真实Chrome/ADS登录、特定Profile、另一台电脑、物理多DPI/Snap及复杂4K等仍待设备验收。错误39不单独确诊进程数限制；浏览器还受内存预算约束。视频精确模式暂不支持HDR/高位深/交错。程序无Authenticode签名。第三方GPL/LGPL源码与许可随附件提供，原始应用代码许可由所有者另行决定。

发布流程见[发布规程](../../source/docs/release-policy.md)，最终来源及公开下载验证结果归档在本目录。


## 发布回执

固定源提交`0f99f3d23e39a09da8fe7fa878332771d9a978ee`，Release ID `444852329`，完整ZIP约98.9 MiB。

[发布回执](publication.json) · [分发清单](release-manifest.json) · [SHA256SUMS](SHA256SUMS.txt) · [验证报告](../../source/docs/verification/REL-4-publication.md)
