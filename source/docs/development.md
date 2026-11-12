# 开发、构建与迁移

更新于 2026-11-10。本文件记录当前可执行入口。早期规划中的文件名与测试名以实际实现和本文件为准。

## 工具链

宿主为 C++20、Qt 6.8.3 Widgets/Core/Concurrent/Sql/Multimedia/MultimediaWidgets，MSVC x64。Visual Studio 安装 C++ 桌面开发、Windows SDK 和 CMake 组件。dev.ps1 通过 vswhere 定位工具。Qt 默认位于项目 .local/toolchains/Qt/6.8.3/msvc2022_64，也可设置 MODYN_QT_ROOT。

当前机器已用 Visual Studio 2026 / MSVC 19.51 构建。Qt MSVC2022 x64 SDK 与当前编译器的组合通过本机测试。其他机器应重新 doctor 和编译，保留 Qt 许可及运行库。

开发 Python 用来运行构建和验证脚本。普通用户的便携包携带各工具自己的运行时。锁定 CPython 3.14.10 embed x64 和 Pillow 13.0.0，官方来源与 SHA256 在 scripts/runtime.lock.json。runtime.py 下载后校验，失败不启用下载内容。恢复依赖时需要网络或预先缓存的锁定文件。

## 常用命令

在 source 工程根终端运行。VS Code 打开 source/ModynToolbox.code-workspace，或直接将 source 目录作为工作区。

```powershell
powershell -NoProfile -File scripts/dev.ps1 doctor
powershell -NoProfile -File scripts/dev.ps1 build
powershell -NoProfile -File scripts/dev.ps1 tools
powershell -NoProfile -File scripts/dev.ps1 run
powershell -NoProfile -File scripts/dev.ps1 test
powershell -NoProfile -File scripts/dev.ps1 test -Preset windows-release
powershell -NoProfile -File scripts/dev.ps1 package -Preset windows-release
```

默认 windows-complete 为 Debug。windows-release 为 Release。windows-dev/windows-check 保留历史验证用途。VS Code F5 与终端使用同一脚本和预设，启动时传入项目 data 与 .local/installed/tools。只开发 Python 工具时可以使用 PyCharm 打开对应工具目录，宿主开发采用 VS Code。

tools 是显式恢复工具入口，遍历 tools/*/tool.build.json。普通 run/F5 不自动补回删除的工具。工具内容改变要提高 package_version，同版本安装内容不允许被静默替换。

## CMD 启动入口

`Run-Dev.cmd` 调用 `scripts/dev.ps1 run`，构建后启动当前源码。`Open-ModynToolbox.cmd` 调用 `scripts/open-release.ps1`，按本机索引启动已构建的便携版。两个入口在仓库根和 source 均可使用，失败时保留终端提示并返回原始退出码。

PowerShell 中运行 `.\Run-Dev.cmd` 或 `.\Open-ModynToolbox.cmd`。CMD 内的 `%~dp0` 由批处理解释，直接调用 PowerShell 脚本时使用 `-File .\scripts\dev.ps1 run` 等实际路径。

启动入口专项回归使用 `python app/tests/integration/test_launchers.py`。测试暂存文件集中在 .local/launcher-tests，使用替身脚本检查四个入口、成功/失败退出码与中文空格路径，并拦截 Start-Process 核对便携包定位。

## 源码和运行目录

app/catalog 负责发现、校验、版本和安装。app/jobs 负责预检、队列、租约、后台进程和内部结果导出。app/storage 负责 SQLite 状态及一致性迁移。app/ui 为通用组件、主题、工作台与共用窗口。app/platform/windows 集中系统外框和进程平台语义。

每工具 core 可独立使用，standalone 为独立入口，integration 连接公共协议和 UI。shared/sdk 不包含业务逻辑。宿主没有按工具 ID 绘制页面或执行算法的分支。

开发安装包在 .local/installed/tools/<id>/<version>。开发数据在 data/host 和 data/tools/<id>。私有作业在 tools/<id>/jobs/<uuid>，任务完成清除输入和结果暂存，预览最迟在退出或下次启动清除。用户导出、事务 journal 和工具持久数据保留。

便携目录结构如下。

```text
ModynToolbox/
├── ModynToolbox.exe
├── app/                       主程序、WorkerHost、Qt 与 MSVC 运行库
├── tools/<id>/<version>/      完整工具包与各自运行时
├── data/host/                 SQLite、实例锁与迁移记录
├── data/tools/<id>/           独立工具数据、作业和默认导出
├── licenses/
├── 使用说明.md
└── release-checksums.json
```

当前采用自包含工具环境。三个 Python 工具各带自己的解释器和依赖，原生工具带自己的 exe 和 DLL。包体包含重复运行库，后续可按精确环境指纹增加共享缓存，首版保持独立分发与删除语义。

## 测试和证据

test 实际收集 11 组 CTest，含 workbench、worker_transport、package_catalog、view_spec、window_native、host_start、jobs、full_system、business_cores、worker_faults、host_crash_tree。测试只处理自己的临时目录与进程。

独立扩展与发布验证入口如下。

```powershell
python tools/file-hash/tests/verify_integration.py --record .local/integration-proof/UI-R3-host.json
python tools/file-hash/tests/verify_manual_removal.py
python scripts/verify-environments.py
python scripts/verify-portable.py
python scripts/measure-resources.py
```

扩展测试需要已存在的冻结记录。当前 UI-R3 基线见 verification/UI-R3-navigation.md，首次完整实验见 R2-complete.md。新实验使用新的记录名，freeze 后通过 --record 传给验证脚本，保留旧证据。measure-resources 使用 Windows 进程计数与 Qt offscreen，数据用于本机比较。物理窗口交互、不同 DPI 和干净机器另做人工验收。原始日志放 .local/logs，摘要与截图保存在 docs/verification。

## 迁移到另一台电脑

1. 正常退出程序。备份整个源码仓库、需要保留的 data 和发布目录。外部源文件、自选输出目录另外复制。
2. 新电脑安装上述开发工具。源码恢复到新目录，读取 AGENTS、README、STATUS。保留工具和共享协议源码。
3. 重新配置 Qt 或 MODYN_QT_ROOT。执行 doctor、tools、test。生成目录 .local/build 含机器缓存，重新生成。
4. 普通用户只需复制完整便携文件夹。数据备份恢复使用应用管理菜单，导入失败保留原数据及迁移记录。
5. 未来版本数据库会拒绝读写，使用兼容程序或备份恢复。缺失工具包会保留会话和草稿，重装相同版本可继续。外部文件缺失时重新添加正确路径。

仅备份源码不会包含忽略的运行数据、工具链和发布包。交付时明确给接收者哪几类内容。

## 正式发布

发布入口为 docs/release-policy.md，生成与上传脚本为 scripts/release.py。正式版本从 CMakeLists 读取，release-metadata 绑定源提交和工具版本。源码 ZIP 含 SOURCE-REVISION.json，可独立恢复开发环境；正式对外发布使用完整 Git 检出并通过干净提交检查。


## 视频组件 SDK 增量

视频公共组件需要Qt 6.8.3的Multimedia模块与Qt ShaderTools。使用Qt官方维护工具补装相同版本，或使用已有aqt环境在本项目工程根补装。依赖仍集中.local，保持版本6.8.3。

```powershell
.local/bootstrap/venv/Scripts/python.exe -m aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 --noarchives -m qtmultimedia qtshadertools -O .local/toolchains/Qt
```

新电脑先恢复原开发工具链，再执行工具构建。test现有14组包含真实视频核心、解码界面和固定宿主接入，必须先构建视频工具并让私有FFmpeg可用。`scripts/dev.ps1 tools`会恢复并构建所有工具。播放测试仅离屏并静音，不控制桌面。


## 1.2.0视频引擎来源

常规适配器构建从dependencies.lock.json恢复发布中的锁定媒体引擎ZIP，无需安装MSYS2。仅重建FFmpeg本身时使用tools/video-clip/engine/prepare.py和私有UCRT64，具体输入与工具链见该目录README及licenses/MEDIA-SOURCES.md。正式发布同时提供第三方源码归档，不能用旧开发Gyan镜像材料替代本版来源。
