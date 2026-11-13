# EXT-1：独立工具开发反馈与宿主改进

2026-11-14。用户授权分析并实现，本地开发版1.3.0，公开版仍1.2.0。本轮未发布、未推送。当前技术为C++20/Qt6.8.3 Widgets，沿用雾紫Theme、UiComboBox、UiSpinBox、ToolPanel和WorkerHost；不增加工具ID业务分支。

## 建议逐项判断

| 原建议 | 判断及落地 | 原因及边界 |
|---|---|---|
| 1 升级后缺包 | 高优先级。缺包页增加“用可用版本新建会话”，旧会话、旧版本身份及原草稿保留 | 不采用原地强切：参数名、含义、分析context及写入计划可能已经变化。新会话使用新版本默认值；重装旧版本仍能恢复。持久化输入继续遵守用户草稿开关 |
| 2 分析后动态选择 | 高优先级。UI v3 choice通过options_source读取result.context对象路径；options_depends_on提供Sheet→列的级联 | 宿主不理解Excel/VIN。字段值变化保留分析context，更换正文/文件清空分析；后端仍需验证输入身份和参数，不能把宿主context当可信执行许可 |
| 3 参数一行一个 | 接受声明式compact。宽参数区短字段并排，宽度小于460逻辑像素回单列 | 不按字段文字长度自动猜；目录/长文本默认整行，不强行三四列 |
| 4 字段说明 | 接受help及placeholder，说明就近换行，同时提供辅助描述 | placeholder输入后会消失，不能代替持续说明；单位和格式写在label/help |
| 5 放包需手动刷新 | 自动检测目录与manifest/checksums元信息，两次稳定后刷新，约2–4秒；保留F5 | 不递归监视data/大媒体，不每次事件重算全部大文件哈希。稳定元信息不等于复制完成证明，推荐点号暂存目录完整复制后更名；工具运行前仍校验完整性 |
| 6 版本不明显 | 工具条目两行展示名称及可新建版本，多会话分支展示锁定版本，不同版本提示“可用新版”；单会话也显示锁定差异 | 新建版本和现有会话版本是两个概念。不能为了展示新版本悄悄切换正在运行的后端；长名称可用tooltip查看 |
| 7 显示名与值 | v3字符串数组兼容，新增{label,value}选项。参数传稳定value，界面显示中文label | value限定字符串、同字段唯一，支持空字符串表示自动，避免数字/字符串隐式转换 |
| 7补充 长列表 | searchable开启可输入搜索，忽略大小写、按包含匹配补全；仅选择现有项目，不插入自造值 | 搜索文本不是后端参数；未确认的非匹配文本会阻止动作。动态列表最多2000项，静态最多200，超限应由后端缩小范围 |
| 8 高级设置 | advanced字段进入默认收起区，可键盘/鼠标展开 | 折叠不删除值、不重置默认值。正常流程所需的必填项不应藏进高级区 |
| 9 环境缺失 | 优先继承常用Windows用户/系统目录变量；缺少用户目录时由Windows Known Folders回退，用户名由GetUserName回退 | 不采用无条件复制全部环境：全局PYTHONPATH、PATH、虚拟环境等会污染独立依赖。宿主仍设置工具私有PATH、任务TEMP、Python隔离标志；不记录用户路径/环境值 |
| 10 浏览器进程数 | limits.max_processes可为4–64整数，默认16；32预算下真实启动20个子进程回归 | 24并非Chrome通用保证，bridge/后端也计入总量，内存预算仍独立生效。保留Job Object取消/崩溃清理，不提供逃逸进程。错误39本身不能确诊进程数限制；真实ADS/Profile/登录仍需实机复验 |

## 接入示例（交给另一台电脑的AI）

工具使用新字段时，将manifest.ui_schema_version和ui.json.view_version同时设为3，min_host_version设为1.3.0。旧UI v1/v2工具无需改造；新能力不会自动出现在旧工具中，需要工具自己的ui.json声明。宿主1.2.0会拒绝UI v3，而不是误解释新字段。

```json
{
  "view_version": 3,
  "kind": "files",
  "fields": [
    {"id":"sheet","label":"工作表","type":"choice","default":"","compact":true,"searchable":true,"options_source":"analysis.sheets","help":"先分析文件，再选择工作表"},
    {"id":"vin","label":"VIN列","type":"choice","default":"","compact":true,"searchable":true,"options_source":"analysis.columns","options_depends_on":"sheet"},
    {"id":"mode","label":"处理范围","type":"choice","default":"local","options":[{"label":"本机数据","value":"local"},{"label":"当前用户","value":"me"}]},
    {"id":"header","label":"表头行覆盖","type":"text","default":"","advanced":true,"placeholder":"空=自动","help":"自动识别失败时填写，从1开始"}
  ]
}
```

分析动作返回的业务结果包含：

```json
{"context":{"analysis":{"sheets":[{"label":"数据表一","value":"s1"},{"label":"数据表二","value":"s2"}],"columns":{"s1":["VIN","时间"],"s2":["车架号","采集时间"]}}}}
```

options_source从context根按点分对象路径读取，不支持任意脚本、数组索引或模板表达式。options_depends_on必须引用声明在前面的choice；按父字段的稳定value从对象中取数组。先填父字段再填子字段；父字段变化时保留仍合法的子值，否则回到声明默认值或“请选择”。若同名列在不同Sheet语义不同，后端应给每列分配含Sheet身份的稳定value。

默认值和已选值找不到时不静默选择第一项。动态数据无效/超过2000项时显示空列表；工具可通过message解释。动态field数据保存在context及原有草稿中，不保存搜索中间文本。v3参数修改保留context，新的文件/正文使其失效；工具须在后端校验最终参数，执行计划应绑定输入身份及相关参数。可在结果中显式返回context:{}清除分析；不含context的结果保留现有分析。

浏览器类工具的清单示例：

```json
{"min_host_version":"1.3.0","limits":{"max_files":20,"max_input_bytes":5242880,"memory_mb":2048,"max_processes":32}}
```

这是工具级预算，不是每个动作各32。跨工具仍按调度器并发设置控制总负荷，不承诺多个Chrome同时运行时的全机内存。每个会话使用独立的Chrome用户数据目录；避免抢占用户日常Profile，不要把已存在的外部浏览器当作可由宿主取消的受控进程。实际Chrome登录和ADS网络不在合成回归中冒充通过。

## 维护与文档

实现文件：view_spec、ToolPanel、Workbench、package_catalog、job_runner。规范为shared/protocol/view-v3.schema.json及manifest-v3.schema.json。新增工具继续在自己的目录开发，不改宿主公共代码。只有表达不了的新公共能力才扩展宿主。

Context7本轮没有可用工具，Qt及Windows API以官方文档核对：
- https://doc.qt.io/qt-6/qcompleter.html
- https://doc.qt.io/qt-6/qprocessenvironment.html
- https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shgetknownfolderpath
- https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects

验收结果见docs/verification/EXT-1-host-extensibility.md。离屏截图只证明当前Qt离屏布局，物理DPI、另一台电脑和真实浏览器登录另行验收。
