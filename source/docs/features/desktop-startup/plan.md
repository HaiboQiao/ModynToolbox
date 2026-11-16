# DS-1 实施设计

改造前：Theme将windowClose:hover写为#f5ccd4，图标固定灰色。main只持有data/host/instance.lock，因此不同data根可并行，且重复启动不唤回已有窗口。设置目前无系统启动项。

F1：公共ChromeWindow中专用关闭按钮绘制语义颜色（hover #e81123 / pressed #c42b1c，白叉），避免QIcon固定颜色和QSS选择器覆盖。点击仍走原closeEvent，浮窗关闭仍收回。

F2：Windows StartupRegistration服务通过当前用户KnownFolder Startup的ModynToolbox.lnk注册，IShellLink存真实App EXE、显式工作区/工具路径和--autostart，工作目录固定App目录。采用快捷方式以避免HKCU Run的260字符命令限制。描述标记归属，暂存后同目录替换；不覆盖/删除同名外来链接。设置从真实启动项读取，不以工作区布尔值冒充系统状态。其他副本注册时显示原路径并允许应用时改用当前程序。路径移动需重新设置；不修改Windows对启动项的系统禁用/策略。

F3：原生Session命名mutex按用户SID哈希分隔，跨App路径统一命名；message-only broker窗口接收有限激活消息，无端口/新第三方依赖。不传文件/工具业务数据。入口在数据写入/扫描/恢复之前竞争；已有实例启动中等待有限时间，响应超时说明，不另开工作区。手动启动授予已有PID前台资格再请求恢复；系统不允许前台时提醒任务栏，不修改系统焦点限制。--autostart重复只退出。进程结束由内核释放mutex，保留QLockFile作为第二层数据保护。

激活只恢复最小化主窗及其当前模态对话框，不重建会话、不重提作业、不改变浮窗。开发自动化/headless/单工具runner排除正常单实例；测试服务使用私有scope，避免与用户实例通信。

实现位置：app/platform/windows/{single_instance,startup_registration}，app/main，app/launcher，ChromeWindow/Theme和Workbench设置。main注入StartupRegistration，单测注入临时Startup目录，不写真实系统启动文件夹。工具与协议不变，不升版本。

官方资料（Context7当前未提供可调用工具）：Microsoft CreateMutexW、SendMessageTimeoutW、IShellLinkW、SHGetKnownFolderPath、SetForegroundWindow及Windows Startup配置文档。具体URL在验证报告。
