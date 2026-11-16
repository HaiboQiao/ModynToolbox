# Python 初始化弹窗专项结果

2026-11-15，侧聊用户提供python.exe应用程序错误0xc0000142截图，确认发生在主聊天开发/优化验证期间。当前侧聊仅修启动/测试生命周期，不接续UI、翻译、工具删除任务，不改运行包latest。

## 发现与置信边界

0xc0000142为运行时/DLL初始化失败，不能仅凭截图认定Python安装损坏或确定具体进程。只读Windows事件未取得可对应截图的PID，因此“每次两次”不能解释成固定重试两次。

代码中发现两条明确竞态：fixtures/backend.py的budget模式创建20个Python后立即返回，只确认Popen创建数量，不确认初始化；JobRunner收到结果后关闭KILL_ON_JOB_CLOSE组，仍在加载DLL的尾部进程可能被提前清理。该测试还把32进程预算与128MiB组内存混测，启动阶段也可能遇到额度压力。tree模式在子进程初始化前发布PID，而host_crash_tree按PID文件立即终止宿主，同样可能落在初始化中。用户提供的发生场景与此吻合，是本次优先修复路径；没有把它冒充截图每个弹窗的逐PID取证。

## 改动

- 测试子进程在Python代码运行后写ready文件；20个同时完成初始化后才确认数量，随后通过release文件协作退出、逐个确认正常退出。tree的PID文件也在ready后发布，仍测试宿主崩溃与进程树清理。
- 只有进程数量用例使用512MiB/15秒，内存故障用例仍128MiB，生产默认内存/进程数量限制不变。防止把两种资源故障混成“20个已正常启动”。
- WorkerHost在创建子进程之前设置仅本进程的SEM_FAILCRITICALERRORS，子进程继承。未修改注册表、全局Python、Windows Error Reporting或用户程序。真实启动错误仍返回失败退出码，不以弹框等待用户点击。
- JobRunner对0xC0000142明确显示“运行时初始化失败、尚未执行工具代码”及内存/进程限额，不自动重试或标为成功。

## 验证

独立构建目录.local/build/py-startup-dialog，未覆盖主线程Debug/Release或latest产物。专项CTest jobs、worker_faults、host_crash_tree共3/3通过（9.28秒）。worker_faults包括20个已初始化进程、错误模式继承、原崩溃/泛洪/错ID/内存失败、取消、临界区等待与后代清理；宿主异常退出后的历史中断/不重跑验证通过。实际日志和源码/WorkerHost哈希见evidence。

本侧聊没有调用系统键鼠、关闭用户进程或复现带弹窗的旧竞态。没有运行主线程正在修改的全量UI套件、没有发布或替换用户正在使用的包。正常开发入口下次编译会包含源码修复；最终运行包仍需主开发任务正常构建。侧聊任务护栏保持专项事实，不伪标19组全量检查完成。

参考：[Microsoft SetErrorMode](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-seterrormode)、[Terminating a Process](https://learn.microsoft.com/en-us/windows/win32/procthread/terminating-a-process)。
