# ModynToolbox 发布规程

本文件定义发布触发、打包、上传与验证规则。处理相关任务时先读取本文件，再按本次明确授权执行对应范围。正式发布由下列触发规则决定，结果必须有实际证据。

## 发布触发与同步规则

只有用户在当前任务中明确要求“发布”“发版”或“发布新版”才启动正式发布流程。讨论发布规则、修改发布文档、优化源码、修复问题、编译测试和本地验证属于各自任务范围。

普通优化按授权完成本地修改、验证与记录。GitHub 源码同步可按用户明确的单独同步指令执行。版本号、正式标签和 GitHub Release 的创建由明确的发布指令触发。

一旦明确要求发布，本次交付必须同时完成最新源码与说明推送，以及运行包、源码快照、独立工具包、校验清单和发布记录上传 GitHub，并核对远端引用与公开下载。完整流程完成后再报告发布完成。

用户仅授权本地打包时交付本地包。提及“发布”一词的规则讨论本身不产生本次发布授权。

## 1. 仓库与责任

仓库为 https://github.com/HaiboQiao/ModynToolbox.git，默认分支 main。

- 根 README/AGENTS 负责用户与 AI 的路由。
- source 为完整可开发工程，保留 core、适配、UI、SDK、测试、依赖锁和开发记录。
- releases/<version> 保存版本说明、下载入口和最终校验记录。
- ZIP、源码快照、校验清单和机器可读发布清单放到 GitHub Releases assets。
- source/.local 保存本机 SDK、构建、打包、上传和下载复核记录。
- source/data 和发布包 data 为各自运行数据根，业务数据排除 Git 与发布资产。

GitHub Release 的标签对应明确提交。每份公开资产绑定版本、平台、源提交、大小和 SHA256。

## 2. 版本与兼容

宿主唯一版本来源为 source/CMakeLists.txt 的 project VERSION。界面、CLI、宿主最低版本比较、便携包目录名及 release-metadata 必须从该版本生成。

宿主标签使用 v<major>.<minor>.<patch>。兼容修复提升 patch，兼容新能力提升 minor，破坏性协议/数据变化提升 major 并提供迁移说明。

每个工具有自己的 package_version、protocol_version、ui_schema_version、min_host_version 和依赖锁。工具版本独立于宿主版本。工具内容、适配、依赖或原生重建产物变化时提高工具包版本，同版本内容保持固定。

只更新兼容工具时，可单独发布工具 ZIP，并使用 tools/<id>/v<version> 标签记录来源。宿主支持的既有公共 UI 和协议满足需求时，用户更新工具包即可。新增公共组件、协议或窗口能力需要宿主新版本。

## 3. 发行形态

正式 Windows x64 便携 ZIP 顶层为 ModynToolbox，包含以下结构。

```text
ModynToolbox/
├── ModynToolbox.exe
├── app/                       主程序、可信桥、Qt/MSVC 运行库与插件
├── tools/<id>/<version>/      工具清单、UI、完整性索引、后端和私有运行时
├── data/                      初始为空
├── licenses/
├── 使用说明.md
├── release-metadata.json
└── release-checksums.json
```

用户完整解压后双击根 ModynToolbox.exe。验证时不能依赖开发机的 Python、Qt、site-packages、当前工作目录或 PATH。发布目录保留所有运行依赖，Qt DLL 保持动态链接和可替换。

完整便携包携带明确版本的基础工具。本版同时提供每个工具的独立 ZIP。工具 ZIP 顶层为 <id>/<version>，便于直接解压到应用 tools 中。

源码 ZIP 从固定 Git 提交导出 source 子树，附 SOURCE-REVISION.json。内容含 AGENTS、README、源码、规范、测试、锁定清单、发布规程与记录，排除用户数据、SDK、构建缓存和凭据。

## 4. 发布前核对

1. 确认用户授权包含目标仓库和上传范围。仅只读审查或本地打包授权时保持对应范围。
2. 检查 git status、远程地址、远端 main 和现有标签。保留其他改动，解决真实分歧，使用正常快进/合并流程。
3. 确定产品版本、工具版本与发布说明，检查最低宿主、协议和 UI 版本兼容。
4. 阅读本版 STATUS、已知限制和实际证据，区分自动测试、离屏渲染和目标设备验证。
5. 检查源码与历史的凭据、机器配置和用户资料边界。认证通过已有 Git 凭据管理器或环境变量，令牌只在进程内使用。
6. 新路径迁移时重建含绝对路径的缓存。项目依赖保留在 source/.local 或显式配置的工具链。

## 5. 构建和业务验证

从 source 目录运行。

```powershell
powershell -NoProfile -File scripts/dev.ps1 doctor
powershell -NoProfile -File scripts/dev.ps1 tools -Preset windows-release
powershell -NoProfile -File scripts/dev.ps1 test -Preset windows-release
powershell -NoProfile -File scripts/dev.ps1 test -Preset windows-complete
powershell -NoProfile -File scripts/dev.ps1 package -Preset windows-release
```

测试应实际收集并运行全部组。失败先修复再复验，不把跳过计作通过。重命名、图片、JSON、文件校验及调度/取消/进程树/状态迁移都有对应回归。

Qt 部署和各工具自己的构建共同组成发行内容。工具发布方负责把自己的解释器、原生库、资源和许可放进包内。

## 6. 便携与目录刷新验证

使用 scripts/verify-portable.py 在中文空格路径、不同 cwd 和受限 PATH 下通过根启动器执行实际业务。确认发行 data 为空，完整性清单全部匹配。

创建分发 ZIP 后必须重新解压 ZIP 验证。再启动只含宿主的私有测试实例，将独立工具 ZIP 解压到 tools 后刷新，检查工具可发现、可运行、输出正确。发布准备脚本记录此项结果。

明确区分“业务源码文件夹”和“完整工具包”。开发者先实现并打包 manifest、ui.json、协议入口、依赖和校验索引，用户放入对应版本目录。直接复制完整工具包不要求修改 app 源码或重新编译宿主。

复制时可先使用点号开头的暂存目录，完整后移到 tools/<id>/<version>。同版本内容不覆盖。已有会话固定旧版本，移除代码前关闭引用会话并等待作业结束，data/tools/<id> 保留。

## 7. 记录与提交

同步 STATUS、task、checklist、worklog、decisions、使用/开发/交接指南和 releases/<version>/README.md。记录测试命令、实际结果、手工待验、依赖/许可和迁移方法。

源提交准备完成后正常提交，确认工作区干净，再重新生成正式 package。正式 release-metadata 中 source_commit 等于将要打标签的提交，source_dirty 为 false。

正式标签绑定已经验证的源提交。后续记录公开发布结果可单独文档提交，主程序资产仍对应原标签提交。

## 8. 分发准备

在干净源提交上执行。

```powershell
python scripts/release.py prepare
python scripts/release.py validate
```

prepare 读取最近 package、版本与 Git 来源，生成完整便携 ZIP、四个工具 ZIP、固定提交的源码 ZIP、SHA256SUMS.txt 和 release-manifest.json。输出集中 source/.local/distribution/v<version>。相同版本已有分发输出时核对内容，不静默覆写。

validate 对 ZIP 做 CRC/路径检查、解压、包内 SHA256 核验、便携业务与工具放目录刷新验证。发布清单记录结果和绑定的源提交。

单独工具发布时，先提高该工具版本并通过独立核心/适配回归，用工具自己的构建入口生成包。以相同目录结构和 SHA256 清单形成独立 ZIP，按第 9 节草稿/校验/公开流程上传到独立工具标签。当前整包脚本覆盖宿主与随包工具资产。

## 9. 上传 GitHub

```powershell
python scripts/release.py publish
```

上传步骤按顺序执行。

1. 确认 GitHub owner/repo 与授权一致，校验当前提交与准备清单，确认验证已通过。
2. 确认远端 main 无分歧，正常推送 main 与带说明的版本标签，禁止强推已有标签。
3. 先从认证列表查找同标签草稿以支持恢复，再创建 draft Release，发布正文来自 releases/<version>/README.md。
4. 上传完整包、源码、独立工具、校验与清单。Windows 内置 curl 采用流式传输和充分的单文件时限，进度放在 .local/upload-progress，认证配置仅经标准输入传入。重试时复用匹配的草稿资产，不覆盖已发布版本。
5. 核对服务器返回的名称、大小、状态和 digest，必须与本地 SHA256 一致。
6. 资产完整后将草稿设为公开，设置 latest，并记录 URL/ID/提交和时间。
7. 匿名重新下载公开资产，验证所有压缩包与清单的 SHA256。下载到私有验证目录，再运行下载版便携业务检查。
8. 用 Git 远程引用核对 main/标签，以 API 和公开下载核对发布状态。保存 source/.local 中原始结果和源码中的精简记录。

网络使用系统现有代理时按单次命令配置，不把个人代理写入共享 Git 配置。HTTP 重定向跨主机时移除认证头。诊断和日志不打印令牌、凭据管理器输出或完整环境变量。

上传中断时保留草稿、资产 ID 和本地清单，依据 SHA256 安全重试。已公开版本需要修正内容时使用新版本和新标签。

## 10. 对外完成条件

以下结果全部满足才报告“发布完成”。

- 最新源码与说明已上传，远端 main 对应实际本地提交。
- 标签指向记录中的源提交。
- Release 公开且资产完整，下载 URL 可用。
- 本地与下载资产的 SHA256 一致，下载版经过解压运行验证。
- 发布资产中没有用户草稿、日志、私密配置或开发缓存。
- 根 README、source、releases 三个入口一致，其他 Agent 可读到用途、命令、发布规则和已知限制。
- 工具独立发布方式及目录刷新规则清晰可复现。

## 11. 已知设备边界

当前自动验证环境为本机 Windows x64、离屏 UI 和受控私有进程。多屏 DPI、Snap、系统对话框、另一台干净机器和真实跨 Agent 接手各记录自己的结果。缺少的设备检查在发布说明中保留，不与自动通过混写。

签名状态、依赖许可证和对应源码来源进入发布记录。没有签名证书时如实标记未签名，不使用签名完成的表述。

## 12. 发布后

在 releases/<version> 保存精简发布回执和校验索引，更新 STATUS/worklog/checklist。历史证据保留原时间和路径背景。下次发布先从本文件重新核对当前状态、代码版本和用户授权。

## 首次分发网络恢复记录

1.0.1 标签保留最初的固定源提交，未公开为正式 Release。首次大文件传输达到原 HTTP 写入时限后，改用已验证的流式客户端，并补上从认证列表恢复草稿的逻辑。采用新的 1.0.2 标签发布修正后的完整流程，原标签保持其既有来源。
