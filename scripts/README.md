# 开发与交付入口

T01 将创建兼容 Windows PowerShell 5.1 的 dev.ps1，作为 doctor、configure、build、run、test、package 的统一入口。T10/T11 增加数据迁移和交接检查。

VS Code 任务调用同一入口。未来脚本从自身位置解析项目根，显式传递源码、包、运行时和数据根，避免依赖终端当前目录。开发日志和生成物进入 .local/，用户数据进入 data/。当前没有可执行脚本。
