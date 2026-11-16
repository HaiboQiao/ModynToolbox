# DS-1 公共窗口、自启动与单实例优化

2026-11-15。本轮用户明确要求分析、设计并直接完成三项；本地实现已完成。继续使用本地1.4.0，不改变工具版本，不发布、不推送GitHub。保留此前IF-1/UI-R6成果。设计/范围在features/desktop-startup。

## 原因、取舍和实际行为

| 需求 | 改造前原因 | 本轮处理 |
|---|---|---|
| 关闭悬停太浅 | Theme硬编码#f5ccd4，图标预先画成灰色，并非alpha透明度异常 | 公共ChromeWindow关闭按钮按状态绘制：悬停#e81123，按下#c42b1c，不透明底白叉；普通状态透明/灰叉，保留焦点提示 |
| 开机启动 | 没有系统启动注册服务，普通工作区偏好不能替代系统项 | 设置增加“开机自启动（登录Windows后）”；默认不注册，应用才写当前用户Startup快捷方式，取消不写，关闭移除 |
| 可重复开主程序 | 只有data/host/instance.lock；不同data根可并行，同根失败也不唤醒窗口 | 正常UI增加当前用户/登录Session的命名mutex和message-only broker；重复手动启动唤回已有主窗，重复登录启动静默退出；QLockFile继续保护数据 |

软件并非一律必须单实例；本产品已有工具多会话、并排及独立窗口，因此单主程序更合适。多个独立后端/浏览器进程是现有隔离架构的一部分，未限制其数量，也未通过杀进程“防重开”。窗口分离、收回、写入确认、后台作业和工具自绘前端均沿用。

关闭按钮主窗/浮窗共用。主窗关闭保留退出协商/检查点/作业规则；工具浮窗关闭保留收回会话。没有把关闭按钮改成托盘隐藏，也没有额外引入托盘功能。

自启动由StartupRegistration负责，用KnownFolder定位当前用户Startup文件夹中的ModynToolbox.lnk。IShellLink记录真实App EXE、--autostart、明确data/packages根和工作目录。选择快捷方式是为了支持中文/空格和超过260字符的完整参数，避免Run注册表命令长度限制。保存先暂存，再同目录替换；读取真实项，描述标记归属，不覆盖/删除同名外来文件。

自启动不保存为工作区数据库布尔值，不随备份/迁移启用；测试未启用用户真实自启动。Windows启动应用开关/策略仍优先。移动或换版目录后可在设置检查旧目标并“改用当前程序”；取消该开关可清除本程序管理的旧路径。删除整个程序前应先关闭自启动。

单实例在写工作区、扫描工具及恢复数据前取得，按SID哈希和Session命名，跨目录/副本互通。消息只请求激活，不传业务数据、不打开端口；新启动向已有PID转交前台资格，恢复最小化时保留最大化状态。有模态对话框时优先唤回该对话框，系统不允许前台则请求任务栏提醒。主窗完成初始化后才安装处理器，期间请求保留。等待有界4秒，不响应时给出提示，不另开工作区。

--automation、Qt offscreen、--tool-package是明确的隔离开发通路，不与正常主程序竞争；保留原数据根锁。测试使用随机私有scope，绝不联系用户生产broker。异常终止后内核释放mutex，已验证可重新启动。

## 验证与结果

| 证据 | 实际结果 |
|---|---|
| Release全量 | 16/16，95.72秒；原有15项包含会话/独立工具页面/作业/视频，新增desktop_startup |
| Debug全量 | 16/16，138.62秒；最终留白整理和无响应补测后，重新构建并专项通过11项（含初始化/清理），5.937秒 |
| 最终入口复核 | 主窗就绪后接收激活这一收尾调整后，host_start、window_native、desktop_startup 3/3通过 |
| 桌面专项 | 关闭颜色像素/白叉，Startup链接真实COM保存/读回、中文空格及长参数、路径切换、取消/应用/关闭、失效/外来文件保护及失败保留 |
| 多进程专项 | 原生隐藏broker，启动中回调延迟、重复唤醒、登录启动静默、四进程同时启动唯一主实例、已有实例卡住不另开、异常结束后重开 |
| 完整便携副本 | 根EXE、五工具真实业务/预览、分离/收回、草稿重启48步，14.255秒；实际使用包内Fixed引擎，PATH仅System32 |
| 最终本地包 | 全部SHA256通过、初始data为空；入口latest记录已指向该包 |

桌面专项11项包含Qt初始化与清理，实际功能用例9项。多进程用例验证Windows原生服务，与生产逻辑相同但使用私有scope；未为了测试占据用户正常实例。常规自动化仍走显式隔离入口，不能把这些结果说成已在用户桌面反复双击验过。

关闭状态与设置截图采用实际Qt渲染，已目视检查，见DS-1-evidence/close-states.png和settings.png。以隐藏/离屏方式验证，不移动鼠标键盘、不激活用户前台。设置说明改为顶部自然排布，额外空间放在应用/取消按钮上方。

## 曾失败的检查及修正

初次外来损坏快捷方式保护用例失败：Qt QFileInfo会按Windows快捷方式解析，可能将目标丢失的.lnk判断为不存在。改为GetFileAttributesW检查启动项文件本身，区分不存在、不可访问和同名文件夹；再解析归属。复验包含损坏内容和有效但目标已删除的外来链接，均拒绝覆盖/删除。保留初轮失败日志DS1-targeted.log，不用最终成功覆盖失败事实。

## 改动范围与本地入口

改动集中platform/windows/{single_instance,startup_registration}、main/launcher、公共ChromeWindow/Theme和Workbench设置；新增desktop_startup测试及本增量文档。没有改动tools/业务前后端、接入协议、WorkerHost隔离、调度规则、已有用户数据或自动执行旧任务。新增系统能力全部使用现有Windows API，没有新框架或运行库依赖。

本地完整包：`.local/releases/ModynToolbox-1.4.0-6c876e79`。可双击source/Open-ModynToolbox.cmd，或该目录根ModynToolbox.exe。源代码入口Run-Dev.cmd保留。当前宿主SHA256：`a3352c32c2e7072c83b3efe8cff649980a5dbf8c077381bd65afc498709486bf`；source_dirty=true，属于本地开发包，不是正式固定源发行。

旧版本没有这套broker，第一次体验请正常退出旧窗口后再打开新包。新版之间重复打开会使用已有实例，不自动切到另一目录/工作区。若要切换版本/工作区，先正常退出当前主程序。

## 未以自动化冒充的设备检查

未注销或重启用户电脑验证真实登录，未修改真实Startup目录或系统启动应用开关；不同组织策略/被系统禁用时的登录结果需设备检查。没有抢占前台体验真实最小化/任务栏/多屏DPI及完整键盘辅助功能，逻辑恢复与原生服务已自动验。Windows的前台限制被遵守，不保证操作系统在每种上下文都允许置前。

本次需求已实现并完成上述自动验收，设备范围保留在checklist；未发布和上传，公开版仍1.3.0。

## 官方依据

Context7当前无可调用工具，使用官方资料核对，不引入Qt Network依赖。

- [Windows启动应用与用户启动文件夹](https://support.microsoft.com/en-us/windows/experience/startup-boot/configure-startup-applications-in-windows)
- [IShellLinkW](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nn-shobjidl_core-ishelllinkw)
- [SHGetKnownFolderPath](https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shgetknownfolderpath)
- [Run命令长度限制](https://learn.microsoft.com/en-us/windows/win32/setupapi/run-and-runonce-registry-keys)
- [CreateMutexW及对象Session命名](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createmutexw)
- [SendMessageTimeoutW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendmessagetimeoutw)
- [SetForegroundWindow限制](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setforegroundwindow)
