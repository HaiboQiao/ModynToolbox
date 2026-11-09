# 开发命令

- dev.ps1 doctor/configure/build/run/test/tools/package 是统一入口。
- tools 显式构建并安装示例工具。run 与 F5 不补回被删除的工具。
- bootstrap.ps1 恢复项目内 Qt SDK。runtime.py 恢复独立后端运行时。
- build-tool.py 构建单个工具，prepare-tools.py 通过已编译 app 安装本地工具。
- package.py 组装新便携目录。使用 dev.ps1 package -Preset windows-release。
- 生成物集中 .local。完整说明见 docs/development.md 与 docs/tool-development.md。
