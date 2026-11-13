# EXT-1 实施设计

保持C++/Qt Widgets架构及主题；新增UI v3和宿主1.3.0本地开发版本，v1/v2不变。动态数据来自result.context，以点分对象路径取数组；可选options_depends_on按父choice稳定值取子列表，无脚本表达式。选项最多2000，静态最多200；字符串兼容，新增{label,value}。动态参数修改保留分析context，文本/文件更改清空；后端仍须校验输入和参数。

目录轮询仅比较目录和manifest/checksums元信息，连续两次稳定才刷新，不读取大媒体和data；默认2秒，工作会话固定原版本。避免复制中瞬态触发缺包。

缺包页新建空白会话，不隐式恢复业务参数/计划，用户可恢复原版本继续。补齐固定常用环境变量，不无差别继承凭据、PYTHONPATH及全局PATH。进程上限按tool.limits.max_processes，继续Job Object管理，不提供逃逸子进程。
