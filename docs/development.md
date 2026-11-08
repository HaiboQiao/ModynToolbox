# 开发、启动、目录与迁移指南

2026-11-08 开发基线。当前可打开工作区查看规划。本文的构建、F5、运行、打包和迁移命令是 T01/T10 将交付的入口，应用入口状态以 STATUS 为准。

## 1. 两种使用方式

开发整个项目建议 VS Code，打开项目根目录或 GraphiteToolbox.code-workspace。安装 C++、CMake Tools 扩展，Python 扩展用于工具开发。VS Code 调用编译器和调试器，C++ 主程序先编译再运行。[VS Code CMake 指南](https://code.visualstudio.com/docs/cpp/cmake-quickstart)

PyCharm 可单独打开 tools/<tool_id>，选择该工具解释器，运行 standalone 或测试。主程序和各工具共用公开契约，各工具核心不依赖具体编辑器。[PyCharm 项目与运行](https://www.jetbrains.com/help/pycharm/creating-and-running-your-first-python-project.html)

交付给普通使用者时，以便携程序启动器打开。源码开发和便携程序是两种交付内容，实际目录见下一节。

## 2. 开发目录的目标结构

```text
GraphiteToolbox/
  README.md
  AGENTS.md
  GraphiteToolbox.code-workspace
  CMakeLists.txt                       # T01 创建
  CMakePresets.json                    # 公共预设，T01 创建
  CMakeUserPresets.json                # 忽略的本机预设，按工具约定生成
  .gitignore
  .vscode/
    extensions.json
    tasks.json                        # T01 创建
    launch.json                       # T01 创建
  app/
    main.cpp
    launcher/                         # 发布根目录的轻量启动器
    catalog/
    ui/                               # 主题、组件、导航和工作区
    core/                             # 会话、状态契约
    jobs/
    platform/windows/
    storage/
    tests/{unit,integration,fixtures}/
  tools/<tool_id>/
    README.md
    core/
    standalone/
    integration/                      # 内含 adapter/、manifest.json、ui.json
    tests/
    assets/
    dependency-lock.*
    build.ps1                         # 本工具构建/打包配方，按语言建立
    LICENSES/
  shared/
    protocol/
    sdk/
  scripts/
    dev.ps1
    package/
    data/
  docs/
    STATUS.md
    spec.md
    plan.md
    task.md
    checklist.md
    development.md
    handoff.md
    decisions.md
    worklog.md
    validation.md
    verification/
    reference/
  data/
    host/{state.sqlite3,logs,backups}/
    tools/<tool_id>/
      settings/
      storage/
      drafts/<session_id>/
      jobs/<job_id>/{input,result,temp,journal,logs}/
      exports/
  .local/
    machine.json
    build/<preset>/
    tools/<tool_id>/{envs,build,package}/
    installed/{tools,runtimes}/
    releases/
    tests/
    logs/
```

以上是完成开发后的目标结构，当前已创建的说明目录与文件见 STATUS。实现目录按任务建立，避免提前添加空模块。源码、数据与生成物各有固定入口，同工具的业务、适配、界面和测试集中在自己的文件夹。

头文件与实现同目录。宿主测试在 app/tests，工具测试跟随工具。测试输入样本可放各 tests/fixtures，用户运行资料只进入 data 或其明确输出位置。

## 3. 便携发布目录

```text
GraphiteToolbox/
  GraphiteToolbox.exe                  # 小型 Windows 启动器
  app/
    GraphiteToolbox.App.exe            # Qt 主程序
    Qt*.dll
    platforms/
    imageformats/
    sqldrivers/
    ...                               # 实际需要的 Qt 部署内容
  tools/<tool_id>/<package_version>/
    manifest.json
    ui.json
    assets/
    backend/
    runtime/                          # 自包含，或引用 runtimes 下精确环境
    LICENSES/
  runtimes/<environment_fingerprint>/
  data/
    host/
    tools/<tool_id>/
  docs/
    quick-start.md
    licenses/
```

轻量启动器放在根目录，主程序和 Qt DLL 在 app 集中。启动器依据自己的可执行文件位置定位 app/tools/runtimes/data，传递正确引用，提供启动失败的可读信息。首次实现需测试路径含中文/空格、从其他当前目录启动和不同盘符。

Qt 依赖与插件由部署工具收集，工具自己的依赖另按工具包组装。[Qt 部署说明](https://doc.qt.io/qt-6/windows-deployment.html)

打包使用明确的源码/构建产物清单，发布包初始 data 为空，不复制开发者运行数据、机器配置和测试日志。Python 等工具运行环境按发布配方重新组装并验证可重定位，开发虚拟环境不作为可跨机发布物。系统依赖与运行前提按实际工具链说明。

当前没有该发布包。M0 验证最小原型部署，M3 验证完整离线交付。Windows 系统运行前提和 VC 运行库按实际工具链记录，包大小以实测报告。

## 4. 开发启动的统一入口

T01 创建 scripts/dev.ps1，兼容 Windows PowerShell 5.1。VS Code 任务和 CLI 调用相同脚本与 CMake Preset。

| 动作 | 计划入口 | 行为 |
|---|---|---|
| 检查环境 | powershell -NoProfile -File scripts/dev.ps1 doctor | 检查编译器、SDK、Qt、CMake 和所需工具运行环境，输出缺口 |
| 配置 | powershell -NoProfile -File scripts/dev.ps1 configure | 使用 windows-dev 预设配置到 .local/build/windows-dev |
| 构建 | powershell -NoProfile -File scripts/dev.ps1 build | 编译，部署调试运行所需 Qt 依赖 |
| 运行 | powershell -NoProfile -File scripts/dev.ps1 run | 构建后启动，传入明确的项目包/运行时/数据根 |
| 测试 | powershell -NoProfile -File scripts/dev.ps1 test | 运行已收集用例并报告数量、失败、跳过 |
| 打包 | powershell -NoProfile -File scripts/dev.ps1 package | Release 构建、工具包组装和发布目录验证 |

脚本从自身位置定位根，所有参数按独立字符串传递，不依赖工作目录，不经字符串拼接执行用户输入。具体参数由 T01 写实后更新本表。

### VS Code 的完成体验

首次按开发指南安装环境，在工作区选择项目预设。按 F5 选择“GraphiteToolbox”，先运行构建任务，再启动 Qt 主程序进入石墨工作台。Ctrl+F5 对应无调试启动。构建失败保持错误与日志，程序启动失败明确说明。

计划调试器为 MSVC 调试器，主目标为 GraphiteToolboxApp。程序路径由 CMake 目标解析，调试参数中的项目根使用 workspaceFolder。核心参数包括 --data-root、--packages-root、--runtimes-root。源码目录中的启动不写死某台机器路径。

计划构建产物位于 .local/build/windows-dev/bin/GraphiteToolbox.App.exe，Qt 调试 DLL 与插件由构建/部署步骤放在该运行位置。F5 和 run 脚本使用一致的目录、参数与环境。release/便携启动器另有独立验证。

## 5. 环境、版本和本机配置

需要 C++20 编译器、对应 Windows SDK、CMake、Qt 6 Widgets 的兼容 MSVC 构建、VS Code 扩展，以及实际后端需要的独立工具环境。Git 用于代码和文档记录。

精确版本在 T01 完成兼容和发布试验后记录为可追溯锁定清单。当前仅发现本机 VS Code CLI、Git 2.55.0.windows.4、Python 3.13.9（Anaconda）和 VS C++ 安装版本 18.9.12120.119。M0 需要定位 CMake/Qt/Windows SDK 与真实编译器，不把安装版本等同于完整可构建环境。

本机路径保存在忽略的 .local/machine.json。按 CMake 标准需要的 CMakeUserPresets.json 可以放根目录并忽略，内容引用本机配置。公共 CMakePresets.json 使用相对生成位置和公共选项，保持可提交。[CMake Presets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)

系统编辑器与编译器可按其安装方式放在系统位置。项目创建的 Python 环境、下载缓存、构建和依赖产物集中 .local。工具环境按工具 ID 和依赖锁指纹管理，业务包运行时不修改全局 Python 或全局依赖。

## 6. 数据根与路径规则

开发默认数据根为项目/data，已安装工具副本为 .local/installed/tools，共享环境为 .local/installed/runtimes。工具源码在 tools，运行时使用固定版本的安装副本。

便携版默认包根/data、包根/tools 与包根/runtimes。显式 --data-root 优先，必要时也可指定工具包/环境根。数据根不可写时说明实际路径、原因并要求选择可写目录，不能静默创建另一套未知位置。

同一数据根使用实例锁。重复启动定位现有实例或报告占用，避免两个宿主同时写状态库。自动测试显式使用 .local/tests 下独立数据根。

数据库保存应用拥有文件的逻辑相对引用，根目录变更后重新解析。外部输入/输出路径保留来源与实际身份检查，换电脑出现缺失时允许重新定位，再验证后执行。脚本、业务、包与数据迁移都需覆盖中文/空格和盘符变化。

默认导出可建议 data/tools/<id>/exports，用户也可明确选择其他目录。日志、缓存、临时文件和恢复 journal 都能定位到所属工具/作业。

## 7. 工具独立开发与接入

1. 在 tools/<id> 或外部独立仓库开发 core 和 standalone，先独立测试功能。
2. 在 integration/adapter 转换协议、参数、进度、取消和错误，核心业务不依赖宿主。
3. 在 integration/ui.json 根据实际任务定义界面，manifest 描述包运行布局中的入口。
4. 本工具 build.ps1 或对应配方在 .local/tools/<id>/package 组装运行包，包括 backend、界面和所需环境。
5. 校验、测试后安装到 .local/installed/tools/<id>/<version>，注册目录索引。
6. 在已构建宿主走完整流程，移除和更新按版本引用处理。

源清单与 UI 是唯一可编辑来源，发布目录由构建复制/组装。协议 SDK 以锁定版本打包到适配环境，工具运行不从宿主源码树临时导入模块。独立项目的环境根可自行配置，在当前仓库内由项目脚本统一放 .local/tools/<id>/envs。

## 8. Git 与持续记录

README 保持入口与现状，AGENTS 保持协作边界，STATUS 保持当前任务与下一步。task 跟踪任务，checklist 跟踪证据，decisions 解释重要决定，worklog 追加有意义阶段的实际过程。

源码、界面描述、协议、依赖锁、构建脚本、编辑器共享配置、文档和许可进入 Git。本机预设、运行数据和生成环境由 .gitignore 排除。T01 初始化本地仓库、检查忽略规则，按实际阶段形成相关提交。外部远程操作独立遵循授权。

每个阶段日志至少记录目的、相关变更、实际验证命令及结果、未执行项、下一步。引用必要摘要和报告，原始大日志放 .local。Git 历史与这些记录一起构成跨 Agent 上下文。

## 9. 迁移与恢复

开发迁移携带源码和记录，在新机器重建环境。Python 虚拟环境和 CMake 缓存可能包含旧路径，不能作为可跨机复用的依赖保证。便携程序迁移携带整个发布目录及所需数据，目标电脑满足平台和运行前提后验证。

T10 实现数据导出/导入。导出时协调应用进入一致状态，等待或取消适用作业，数据库通过关闭/备份接口获得一致快照，涉及 SQLite WAL 时包含或正确合并所需状态。原始复制以程序正常关闭、全部所属 worker 结束为前提。

备份包记录格式、工具包/状态版本和完整性清单。待恢复 journal 必须携带。导入先预检并备份当前数据，暂存验证后启用，失败保留原数据。默认不覆盖用户外部原文件，不重放写入任务。

源码、用户数据、离线依赖和可执行发布包分别标识。为他人提供源码时默认排除 data/.local。用户明确需要迁移自己的内容时另携带数据。详细交接步骤见 [handoff](handoff.md)。

## 10. 文档与自动加载的边界

AGENTS.md 是开放的项目指令格式，适用于多种编码 Agent，具体自动读取规则取决于使用的工具。项目同时提供 README 和显式交接提示。[AGENTS.md 说明](https://agents.md/)

Codex 的项目指令可通过 AGENTS.md 提供，具体发现和覆盖规则按其官方文档。本项目不会把 Codex 特定本机设置作为通用开发依赖。[官方 AGENTS 指南](https://learn.chatgpt.com/docs/agent-configuration/agents-md)

本轮 Context7 无可调用工具，API/编辑器行为按官方文档核对。当前规划中的任务、脚本和 F5 由实施阶段创建并真实验证，资料核对不能替代本项目运行证据。
