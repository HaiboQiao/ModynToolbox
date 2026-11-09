# 独立原生文件校验工具

这是宿主首次冻结后新增的第四工具，用于验证无需修改宿主即可接入新工具。core/hash_core 为 C++ 分块哈希核心，standalone/main.cpp 为独立 CLI，integration/adapter.cpp 使用公共 C++ SDK，ui.json 描述文件列表、算法选项和结果表格。

使用 SHA-256 或 SHA-512。文件按 1 MiB 分块读取，支持取消。Windows 读取句柄限制并发写入与删除，失败逐项报告。

```powershell
python tools/file-hash/build.py
python tools/file-hash/tests/verify_integration.py --record .local/integration-proof/UI-R3-host.json
python tools/file-hash/tests/verify_manual_removal.py
```

build.py 仅构建本工具，生成私有 exe、QtCore/MSVC 运行库，再调用通用包构建器。产物索引在 .local/tools/file-hash/latest.json。复制生成包，通过现有宿主管理菜单安装。

只需业务核心和独立 CLI 时，整个工具目录复制到其他位置后，以 -DMODYN_WITH_ADAPTER=OFF 配置自己的 CMake 工程。该模式不读取宿主头文件或 SDK。具体命令见 standalone/README.md。

tests/verify_integration.py 使用现有主程序执行真实安装、双会话、两种算法、浮窗、收回、引用保护、移除保留数据、重装与再次运行，最后检查冻结宿主 SHA256。验收脚本依赖仓库验证设施，业务核心和独立 CLI 可单独使用。

依赖与许可见 dependencies.lock、LICENSE.txt。
