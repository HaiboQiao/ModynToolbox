# 独立完整工具前端：开发与接入

本地开发版宿主1.4.0。宿主承载完整Web页面，不绘制业务组件；旧UI v1/v2/v3继续使用Legacy适配。工具作者自行选择布局、框架、样式、控件与业务数据结构。不得要求宿主按工具ID增加页面代码。

## 最小工具包

发布目录tools/<id>/<version>内含manifest.json、frontend/index.html和assets、checksums.json及licenses；有后端时另含backend/和私有runtime/。前端只加载自己的静态资源，生产不启动npm开发服务器、不依赖CDN。工具核心可以独立开发；integration只适配契约。

清单使用manifest_version=2、min_host_version=1.4.0，frontend={type:web,entry:frontend/index.html,bridge_version:1,state_version:1,capabilities:[jobs,files,state,...]}。frontend与后端protocol_version是独立版本。完整字段见shared/protocol/manifest-v2-web.schema.json；后端沿用JSONL v1。纯前端包可以省略后端协议和执行入口，recipe.language=frontend。新旧工具版本目录各自不可变。

capabilities可声明jobs、files、clipboard、export、window、state。它约束宿主服务桥，不是对可信本地后端的Windows权限沙箱。未声明能力返回具体拒绝。SDK只负责通信，不创建表单或CSS。基础样式和主题仅为工具可选择的建议。

## 独立开发与构建

运行python scripts/export-tool-sdk.py <新目录>可导出集成SDK，无app/宿主源码。工具自己的前端按选定框架构建；使用导出的scripts/build-tool.py <工具源码> --output <新包目录>组装。Python工具recipe沿用language=python/runtime=python或pillow；C++工具可自行编译，再在recipe填写built_binary及runtime_files。

开发可以在自己的前端页面先加载SDK/mock.js，再加载modyn.js；模拟器不做真实文件操作或业务执行，不算业务验证。真实开发运行使用ModynToolbox.App.exe --tool-package <完整包> --data-root <独立数据目录>。开发包/SDK不要求修改宿主，不默认安装到用户工具库。VS Code/命令行都可采用自己的工具语言环境。

正式包的frontend/modyn.js由构建器从SDK复制。工具作者也可按协议自行实现桥，不强制使用这个JS文件。源码frontend/node_modules等不进入包，采用构建后的自包含资源。源码、前端vendor与依赖许可随工具保存。

## SDK与消息

import {modyn} from './modyn.js'。on('bootstrap',handler)接收工具/版本/会话/前端实例身份、能力、当前检查点、输入、结果、作业和主题建议，ready()触发握手。主题变化通过on('theme',handler)通知，工具自行选择应用。CSS媒体查询负责响应视口。

- submit(action,input,key)：提交自己的后端动作。key须唯一且可用于同输入的重试；同key不同输入拒绝。每会话仅允许一个非终态作业，接受回执保留最多32项。浏览器重载不自动再次执行。on('job')显示状态，on('result')自行绘制结果。query()/cancel()处理当前会话作业。
- pick({filter,directory})：选择文件/目录，返回files条目path/name/bytes/url。drop(FileList)通过浏览器额外对象传递真实磁盘文件，不能由JS字符串冒充系统拖入。文件名扩展检查仍由工具前后端负责。
- url(path)：为已授权输入或工具数据中的文件生成受限本地媒体URL，支持范围读取，不暴露整个磁盘目录。artifacts.read按最多64KiB读取；artifacts.open打开已授权文件。artifacts.save使用宿主导出通路；clipboard.write最多5MiB。
- checkpoint(state,input)：仅定义自己的JSON状态与下次作业输入，宿主不解析其业务字段。setCheckpointProvider(()=>({state,input}))必须对有编辑状态的工具注册，供隐藏页回收和退出时请求新检查点。不要只在输入事件里延时保存而不响应检查点请求。
- window.detach/window.attach请求显示位置变化，不重建业务会话，不重提作业。

raw/call为版本化底层请求；完整方法见request.schema.json。小控制消息最多256KiB，SDK会将较大payload分成64KiB传输块，最多16MiB、4个并行传输、2分钟超时；大文件/媒体不走JSON。检查点总对象最多16MiB（为保留现有5MiB JSON输入，将原提案1MiB调整为有界16MiB），数据库工作区总状态仍受64MiB上限。大业务数据优先使用工具私有文件/工件引用。传输暂存不进入工作区备份，正常关闭或下次启动清理宿主自有临时条目。

## 会话、生命周期与恢复

宿主保存身份、作业和版本；页面保存自己的编辑状态。草稿关闭时，仅保持运行中状态，跨启动不保存input/ui_state。不要通过localStorage绕过草稿开关；浏览器UDF为缓存，不是唯一业务状态。工作区备份排除ui-profiles和ui-transfer，保存宿主检查点及工具业务数据。

UIR-7起，UIR-8默认最多保留最近两个闲置Web页面5分钟（设置可调0–5页、1–10分钟），超额按最近使用顺序、超时按闲置时间回收；活动/并排/独立页面不回收，首次仍按需创建。隐藏页在真正回收前申请新检查点，确认后才回收；无响应或有作业的页面保留。退出会请求检查点，超时由用户选择保留上次状态退出或继续。browser/renderer失败由容器提供重载；后端任务独立，不因页面重载再运行。新页面先读当前任务状态和结果。旧版本/旧state_version不自动转成新版字段，恢复失败保留快照。

默认每工具版本一个浏览器进程组，同工具多会话共享该组；不同工具分别建UDF。它增加内存成本，同工具浏览器进程组失败可能影响多个会话，宿主仍是共享管理进程。扫描工具不启动引擎，关闭所有页面通过控制器生命周期释放，无按进程名清理其他应用。

完整包带Fixed Runtime；开发依赖运行python scripts/setup-webview2.py恢复，锁定来源与SHA256见shared/frontend-sdk/dependencies.lock.json。开发前先运行scripts/dev.ps1 doctor，真实Web前端用Windows原生验证；Qt offscreen只证明旧控件和后台通路。

## 验证与发布

新增工具至少验证独立构建、放目录发现、会话隔离、缩放/窄窗、分离/收回、草稿、错误、取消、输入输出和资源释放。记录宿主EXE/源码冻结前后哈希。工作台原生自动化仅在显式MODYN_BACKGROUND_VALIDATION下允许，测试窗口屏幕外、不激活。

正式发布必须按docs/release-policy.md由用户明确触发；日常工具开发不自动发布宿主。当前实际验收结果见docs/verification/IF-1-independent-frontend.md，未完成的物理IME/多屏与异机项目保持待验。


## 长期维护入口（GP-1）

当前目标以[项目背景](project-baseline.md)为准，已有行为见[BC](behavior-contracts.md)，修改遵守[范围与验证](change-policy.md)。已构建Release和准备依赖后，`python scripts/verify-frozen-frontends.py --freeze`生成新的私有冻结快照并运行两工具接入实验；可用--binary指定宿主。旧快照保留，不再依赖.local手工脚本。没有该环境时标待验，不把旧哈希视为当前代码证据。

## 工作区交互建议（UIR-7）

宿主承载区没有公共24px留白，工具拥有整个视口，可自行设计内容内边距。内置工具采用12px、窄屏10px；不是SDK强制布局。聚焦输入采用1px颜色边框，滚轮仅滚动、不改数字/选项，且不blur；可复制某个内置工具的frontend/interactions.js作为工具自身实现。SDK依旧只通信，不代画页面。缓存不是业务持久层，不能依赖某页永远驻留，检查点提供器必须完整且可重复调用。


UIR-8为旧会话在导航提供明确的新版新建入口，固定版本不会静默改变。输入风格/滚轮行为属于工具包，更新后只有新版页面使用对应实现；原版页面不由宿主强制注入修改。工具首次加载有宿主短暂阶段提示；页面就绪后全部视口仍由工具拥有。
