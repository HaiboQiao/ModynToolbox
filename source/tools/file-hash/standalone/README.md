# 独立使用

构建后运行 ModynHashCli.exe --file <文件路径>。标准输出返回 SHA-256 JSON。core 和 standalone 都不包含宿主或 SDK 头文件。

单独复制这个工具目录后，使用 CMake -DMODYN_WITH_ADAPTER=OFF 构建 CLI，只需要 QtCore 和自己的 C++ 工具链。工具箱适配器位于 integration，与 CLI 分开编译。
