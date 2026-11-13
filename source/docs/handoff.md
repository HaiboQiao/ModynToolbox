# 新开发者与 Agent 接手

## 可直接提供的接手提示

请继续维护 ModynToolbox。先读取根 AGENTS.md、README.md、docs/STATUS.md 和 docs/verification/R2-complete.md，再读取与任务有关的 spec、plan、tool-development、decisions、worklog。检查 Git 状态后工作，保留其他修改。先运行 doctor，恢复依赖后执行需要的测试。工具业务放 tools/<id>，公共宿主保持通用，数据统一放指定 data 根。不要依赖之前聊天或个人全局 Skill。

## 当前代码定位

- 入口 app/main.cpp，便携启动器 app/launcher.cpp。
- 工具发现与完整包 app/catalog，运行调度 app/jobs，子进程启动门闩 app/worker_host。
- 通用 UI 在 app/ui/components/tool_panel、view_spec、workbench，主题与窗口各自集中。
- SQLite 与一致性数据迁移在 app/storage。
- 四个工具各有 core、standalone、integration 和 tests。
- shared/protocol 和 shared/sdk 是接入边界，scripts/build-tool.py 为通用打包器。
- docs/task.md 为当前任务映射，checklist 区分自动覆盖与手工验收。

## 接手验证

核对 git log、git diff、忽略规则，确认 data/.local 没有进入提交。从 development 中选择实际入口，执行后记录结果。新增工具优先独立核心测试，再走现有宿主导入、执行、多会话、浮窗和移除。

用户授权本轮完成全部剩余开发。R2 为本轮交付，保留原始失败和修正记录。跨 Agent、另一台电脑和物理多 DPI 的实际接手体验由接收方最终验证。项目文件已经自包含，不能将本地冷读记成跨 Agent 已通过。

## 维护注意

数据库拒绝读取后禁止保存覆盖。草稿恢复失败保留原快照，用户主动修改后才采用新输入。仅显式选择保存正文和文件列表。

工具升级使用新版本，不覆盖旧包。现有会话固定旧版本，关闭引用后才能移除。文件写事务使用计划、文件身份和 journal。进程资源隔离与系统权限分开理解。

较大文本只画前 32768 个 UTF-16 码元，完整内容由会话保留。更复杂的工具页面应先判断是否可用公共组件表达，需要扩展公共组件时同步规范版本、校验器、渲染器和回归。

继续开发时先读当前代码和证据，历史规划中的拟议类名和测试文件名可能已合并到实际模块。

## 仓库整理与发布接手

当前 Git 根含 source 和 releases，本工程位于 source。源码包解压后可直接将所在目录作为工程根。发布前读取 release-policy.md，GitHub 公开状态与固定标签的来源见仓库 releases 对应版本回执。历史验证中的旧根路径保留审计背景，当前构建和数据路径从工程根重新解析。


## 1.2.0新增接手入口

视频工具位于tools/video-clip；通用媒体契约在shared/protocol/media-v2.md。工具1.0.0需要宿主1.2.0。处理引擎来自engine输入锁和配方，普通工具构建恢复同版公开引擎归档，只有维护者重建媒体引擎需要私有MSYS2。发布必须同时携带第三方对应源码并对下载包运行视频专项。最新发布证据见verification/REL-3-publication.md与仓库releases/1.2.0。


## EXT-1 通用接入增量（本地1.3.0）

UI v3支持动态/级联选项、显示名与固定值、搜索、compact、help/placeholder、advanced。独立工具采用新字段前读取docs/features/host-extensibility/design.md（从源码根定位），同步声明最低宿主1.3.0。旧v1/v2继续兼容。工具进程可声明max_processes（4–64，默认16），用户目录环境由宿主补齐，工具包目录变化自动检查，会话仍固定版本。实际证据见verification/EXT-1-host-extensibility.md。
