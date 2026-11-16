# IF-1 独立工具完整前端：整体优化方案

2026-11-15。用户已授权实施；下文保留设计基线，当前实现差异和实测见docs/independent-frontend.md及验证报告。以spec.md中的原始目标澄清为准。本文件取代“仅继续扩展声明式布局即可满足用户需求”的建议，旧UI规范继续作为现有实现事实和兼容路径。

## 1. 改造前事实与差距（设计基线）

C++20/Qt6.8.3 Widgets工作台、导航、独立窗口、工具包目录、调度、WorkerHost、SQLite和跨语言工具核心已有实现。UI-R6已局部修复参数置顶。

当前Workbench::ensureView直接构造ToolPanel，Session直接保存ToolPanel指针。runAction、任务回调、草稿、parkUnused、保存/关闭检查依赖text/files/params/result/context等固定结构。ToolPanel::node忽略actions/status的声明位置，构造函数固定页脚。Theme将QSS作用到Qt业务控件。仅改Schema或ui.json无法拆开这些耦合。

目标：工具UI由宿主外部的工具前端资源自行绘制；宿主理解身份、能力、消息和生命周期，不理解某工具的按钮、表格、Sheet、裁剪时间轴或业务布局。

## 2. 路线比较与建议

| 路线 | 符合程度与成本 | 决定 |
|---|---|---|
| 扩展现有JSON控件树 | 仍由宿主提供组件，上限不断出现 | 仅兼容旧工具，不作为主方向 |
| 加载工具原生UI DLL/QML到宿主 | 可以自绘，但ABI/Qt版本、依赖与崩溃域耦合 | 不作为通用工具接入 |
| 将任意外部EXE窗口SetParent进宿主 | 跨进程DPI、焦点、弹窗、生命周期复杂，不能普适支持任意GUI | 不承诺零适配内嵌 |
| 全部改成Electron/Tauri | 可承载Web，但现有壳/窗口/调度需迁移，收益不必然抵消成本 | 当前不重写宿主 |
| Qt WebEngine容器 | Qt集成自然且可承载完整Web页面，但带入另一套Chromium部署与维护 | WebView2原型失败时再做有界对照，不同时维护两种引擎 |
| 原生Qt壳 + WebView2 | 保留已验证壳；完整Web页面自主绘制；增加Win32/COM适配及浏览器运行时 | 建议主路线，先验证原生窗口生命周期 |

工具前端可使用原生HTML/JS、React、Vue、Svelte等，只要求输出符合浏览器运行约束的本地静态资源。宿主不要求统一框架、组件库或构建工具。需服务器渲染/长驻Python服务的工具不能假装静态部署：首轮要求导出静态前端并通过业务桥调用；长驻服务如确有需求，另设明确的service契约和受控生命周期。

这是渲染承载方式的契约，不是继续限制工具的业务控件。任何图表、编辑器、画布和复杂布局无需宿主知道其类型。前端仍受浏览器支持、视口尺寸和声明能力约束；特殊编解码/GPU等需实测。

## 3. 职责和运行结构

工作台进程：C++/Qt壳 + SessionController + ToolViewFactory + 通用HostBridge + 调度/状态/工件服务。

ToolViewFactory按manifest的frontend类型创建LegacyDeclarativeView或WebToolView。抽象接口负责挂载、可见性、焦点、宿主窗口变化、主题通知、状态检查点、恢复和释放。原ToolPanel放入Legacy适配，不再是会话唯一类型。异步接口不阻塞Qt线程；浏览器COM和回调留在有消息循环的STA线程，重工作离开UI线程，回调中不直接打开阻塞模态循环。

每份会话一份WebView控制器/页面。WebView2负责浏览器渲染进程，工具JS不作为宿主原生插件执行；可信宿主桥仍在工作台进程，其错误仍可能影响宿主。

工具业务：原有作业调度 -> 受控WorkerHost/bridge -> 工具自己的Python/C++等后端。后端不需要知道页面是React还是Vue。纯前端计算工具可以不声明后端；重计算使用Web Worker或业务后端。

## 4. 前端接入契约

新增frontend-v1及独立manifest规范版本；不要将前端桥版本等同ui_schema_version或后端protocol_version。现有ui_schema_version=1/2/3保持原语义，精确新增版本号在实现冻结时确定。

拟定frontend字段：type=web、entry=frontend/index.html、bridge_version=1、state_version、min_host_version、min_runtime_version、capabilities及viewport建议。入口与资源限定工具包内；资源摘要进入包完整性清单。该格式是提案，当前宿主不识别。

工具收到bootstrap：toolId、packageVersion、sessionId、frontendInstanceId、协议/能力协商结果、视口/缩放、主题建议、已允许恢复的状态、当前作业摘要。工具前端回ready后才交互。身份以宿主绑定值为准，不信任页面自行传来的其他sessionId。

SDK仅封装桥，不生成页面。拟定能力：jobs.submit/cancel/query/subscribe、files.pick、artifacts.read/open/save、session.checkpoint、theme.subscribe、window.requestDetach/requestAttach。无任意shell执行代理、数据库对象或宿主控件指针。工具画自己的提交/进度/取消按钮，宿主记录同一作业供全局历史与异常恢复。

请求含bridgeVersion、requestId、method、payload，响应有requestId/result或结构化error；事件有session归属、jobId和递增序号。提交使用幂等键，超时后可查询原请求，避免UI重载重复写文件。序号缺口走最新状态和结果引用重建，不无界存储事件历史。

控制消息采用有界JSON，初始建议单消息256KiB、UI状态1MiB（实现前结合真实工具调整并记录），大表格/媒体/二进制走工件引用与分块/范围读取，不能将视频编码成Base64塞进主线程。异步回调可取消，有超时、速率和队列上限；前端每帧坐标不反复请求后端。

后端初期保留现有一次作业JSONL协议，去掉前端对params/text/files字段的强制依赖。提供兼容转换由各工具适配层承担。业务input/result为工具自定有界对象；调度、来源文件预约、写入确认仍需结构化声明，不允许前端自绘确认就绕过宿主写入规则。预检/提交/取消和作业幂等属于服务契约，不规定按钮摆放。

## 5. 隔离与资源策略

不将所有工具共享同一WebView用户数据目录作为默认。建议首轮按toolId+packageVersion创建独立Environment/UDF，工具间形成不同浏览器进程组；同工具多会话共享该组以控制开销。每会话独立页面、状态根与本地资源origin，不用共享localStorage承载会话权威状态。

代价：每个活跃工具版本可能启动浏览器/渲染/GPU等多个进程，绝非“一个工具一个进程”。同工具浏览器进程故障可能影响该工具全部会话；系统GPU/内存压力仍可跨工具影响。不同UDF是进程分组与数据边界，不是对恶意本地后端的权限沙箱。每会话独立Environment更强但更重，首轮不默认。

按需创建：扫描包只读清单，不初始化WebView，不启动后端。隐藏页先隐藏/暂停适用活动；可检查点且无不可恢复编辑状态的页面按LRU回收。浮窗、可见并排和未保存且未获checkpoint确认的页面不回收。页面卸载不取消后台作业。

浏览器进程预算与现有worker JobObject预算分开，不将浏览器控制器或共享进程组挂到某个可取消作业的16进程/内存限制。作业取消不杀UI；卸载一个会话不得杀同组其他会话。浏览器退出与UDF清理通过引擎生命周期处理，不能按进程名全局杀进程。

性能验证记录空壳、首个Web工具、同工具2/5会话、不同工具2/5个、隐藏/释放后、安装100个未打开工具。统计启动/切换延迟、CPU、private bytes/working set、共享页口径、进程树、包体与运行时体积，禁止简单进程working set相加当精确物理占用。预先声明参考机器和目标阈值，原型实测后冻结；不能现在承诺几十MB或绝对不卡。

## 6. 本地资源、文件和能力

使用工具包frontend目录映射本地虚拟HTTPS origin，不开启本地开发服务器供正式用户使用，也不将整个工具包/backend/runtime或用户盘根暴露给页面。每会话资源身份由宿主分配，入口导航与消息来源都校验；不默认暴露桥给iframe/外链/新窗口。

默认离线，生产资产不走CDN；外链显式走系统浏览器，不继承桥。网络、摄像头、下载等能力按工具需要声明。文件选择由宿主返回作用域引用，工具后端仍可在受信本地模型下操作允许文件。拖拽导入、剪贴板、IME、辅助功能和超大列表为必须实测项目，不以浏览器能显示就算完成。

媒体预览通过受限工件映射/范围读取支持大文件，不把源视频复制进frontend。浏览器codec支持与当前Qt播放器不同；视频迁移前用实际编码素材验证，不承诺所有FFmpeg可处理格式都能在WebView直接播放。必要时生成工具自己的代理预览，不因此让宿主重新绘制业务播放器。

## 7. 会话、草稿和窗口生命周期

会话元信息与作业权威状态由宿主管理；前端布局、表单、选区、滚动、未保存编辑及业务视图状态由工具定义。state_version由工具独立维护；宿主仅验证JSON大小和存储归属，不解析业务字段。

运行中内存状态可保持，跨启动持久化继续遵守草稿开关；页面不得通过localStorage绕过“关闭草稿”保存正文。UDF是可清理缓存，不作为工作区迁移和业务恢复的唯一来源。宿主保存有界检查点，前端恢复失败保留原快照并提示，不静默清空或跨版本猜测迁移。

窗口分离/收回优先保留同一控制器和页面，以WebView2支持的ParentWindow变更及Bounds/焦点通知对接新的原生容器。必须处理Qt重设parent导致的HWND变化、跨屏缩放、原生子窗遮挡、菜单/输入法与焦点。不能仅依赖原有QWidget::setParent就声称完成。

若原型不能可靠做到不重载迁移，不用“重建页面+恢复部分JSON”冒充原有无损语义；应在阶段一停止并评估稳定会话容器/组合承载或QtWebEngine对照，再定方案。页面崩溃恢复可以重建，但不能重复作业。

关闭浮窗仍默认收回，关闭会话和退出沿用运行中作业规则。前端beforeClose只是有超时的状态协商，不能无限阻止退出。页面失联时由宿主在容器内提供重载/诊断/关闭恢复界面，该异常界面不成为平时固定业务页脚。

## 8. 故障语义

JS错误：工具内错误状态或容器恢复入口。渲染挂起：独立超时诊断，不依赖挂起页响应；重载前保留最后有效检查点。浏览器进程组失败：标记该组页面待恢复，其他工具不跟着重载。后端失败：页面收到作业失败，继续编辑。宿主退出/崩溃：按既有后端树清理规则结束进程，重启将非终态标为中断，不自动重新执行。

桥输入错误局部拒绝，不崩宿主；大消息/高频消息限流并记录工具身份。前端恢复后先查询作业与结果，不用旧进度事件覆盖新终态。结果artifact和持久化状态有归属、保留及引用生命周期。

## 9. 风格和开发独立性

宿主拥有外框、导航和公共管理界面。提供可选CSS变量（背景、文字、强调、间距、字号、密度）与建议样例，不注入重置所有元素的全局CSS，不跨边界修改工具DOM。工具可以不用样例组件，决定所有内部结构。深色编辑/视频画布可以局部不同，主题变化采用通知，不强行覆盖。

独立开发方式：工具自己的frontend项目可用本地开发服务器+桥模拟器调试，也可用通用开发runner连真实宿主服务。模拟器不证明真实接入。生产使用静态构建产物；使用者不安装npm、不执行npm run dev。工具前端包不自带Electron/Tauri壳；如需独立EXE，用可选通用runner另行打包并标明附加运行时。

开发验收必须包含脱离宿主源码目录构建工具，将完整产物复制到已冻结宿主，记录宿主文件哈希不变。不能先为工具加宿主组件，再宣称该工具零宿主修改接入。

## 10. 目录与分发

保留source/app、source/tools、source/shared、source/data、source/.local，不为本轮再次移动项目根。

源码新增app/ui/tool_runtime/{tool_view,legacy_view,web_view,view_factory}、app/platform/windows/webview2/、app/services/host_bridge/，shared/frontend-sdk/、shared/protocol/frontend-v1/、shared/theme/（可选token），scripts工具构建/验证入口。精确文件名可按现有结构落地，按能力分层而不按工具ID分支。

单工具源码：tools/<id>/{core,frontend,standalone,integration,tests}。frontend内保留自己的package/lock/src；构建缓存与node_modules放项目忽略范围，不进入发布包。integration负责清单、后端协议适配、构建描述；核心不导入宿主前端。

发行：tools/<id>/<version>/{manifest.json,frontend/index.html,frontend/assets,backend,runtime,checksums.json,licenses}。新Web工具不必提供旧ui.json；旧包原结构不变。工具所需后端运行时仍在该工具内，WebView2浏览器引擎由宿主级runtime集中分发一次。

数据：data/tools/<id>/shared、sessions/<uuid>、ui-profiles/<version>和jobs/artifacts；旧数据路径按显式迁移表处理，不直接移动未知工具数据。host.db只存宿主身份/索引/归属与检查点，业务持久化放工具目录。工作区备份纳入业务与会话状态，不备份浏览器缓存、锁和开发依赖。

完整便携包建议携带锁定的Fixed Version WebView2，明确选择该目录，不依赖目标机已安装、网络或管理员安装，SDK/loader许可与运行时许可随包核对。浏览器引擎体积会明显增加，安全更新由宿主发行负责。可另提供复用系统Evergreen的轻包，但必须标明前置条件，不能替代完整离线包。精确SDK/Runtime版本和支持Windows范围由原型锁定，不把官网最新版本直接写成已兼容。

## 11. 升级、删除与现有工具迁移

保留旧UI1/2/3的Legacy适配，现有会话锁定原版本。新的工具包声明web与最低宿主，老宿主清楚拒绝不兼容包。宿主理解frontend_version不等于理解工具state_version；不存在通用自动状态迁移。

运行中工具不允许管理器直接移除；手工删除无法被完全阻止，发现后对新请求报缺包，不假设已加载JS和DLL可以继续安全用。关闭对应会话/作业后可删除整个版本目录，自动发现消失，data保留；UI缓存待引擎释放后再清理。

迁移顺序：新增真正不同的新工具验证 -> 重命名/文件校验 -> 图片 -> 视频 -> JSON按实际需要安排。每工具升新包版本，保留旧包；复用既有core/adapter、许可和测试。旧快照不强转新页面，新会话切新版；明确迁移程序另行定义。

现有五工具的迁移应验证结构确有任务差异：重命名规则+前后对照、图片预览与输出、文件校验结果、JSON双编辑区、视频时间轴；不把原ToolPanel逐像素复制成HTML作为前端独立目标的唯一证明。

## 12. 实施阶段与停止条件

P0 契约：审阅本方案，确认完整前端目标/职责/运行时取舍；规范现有文档与未来能力的状态，保留UI-R6修改。

P1 技术验证：隔离原型实现一个无业务Web页、Qt native容器、桥握手、两工具环境；验证STA、中文IME、拖拽、浮窗迁移100次、窗口大小/DPI、焦点、挂起/进程失败和清理，测资源与固定运行时便携性。原生可见窗口的DPI/IME检查需安排用户不受打扰的验收时段，不能用Qt offscreen替代WebView2真实渲染。关键生命周期失败则停止扩大迁移。

P2 宿主解耦：引入ToolView抽象，Legacy保兼容，会话/持久化/作业消息不再依赖ToolPanel特有控件，全部旧回归通过。

P3 Web接入：落实桥、清单/完整性、资源映射、checkpoint、事件恢复、隔离和懒加载，SDK/模拟器/通用runner文档配套。

P4 冻结接入验证：冻结宿主源码/EXE；至少两个不同框架/明显不同布局工具（含Python、C++后端）由各自目录构建并接入。测接入步骤和耗时，审计宿主哈希及Git差异，无宿主修改。

P5 现有工具逐个迁移与风格验收，测试业务结果、文件确认、取消/部分完成/恢复、并排、旧版本会话；不在一个提交删除全部旧UI。

P6 完整包及文档交接：更新AGENTS/README/开发/独立工具/设计/发布/备份规范，离线干净机验证；只有用户再次明确要求发布才定正式版本、标签、推送与Release。建议兼容并存走下一minor；破坏性协议变化须major或明确迁移，当前不预改版本。

每阶段是内部技术验收点，不要求用户逐段反复批准。后续获得整体实施授权后可连续完成，但遇到P1关键验证失败必须如实处理，不能假设成功。

## 13. 官方依据（2026-11-15核对，Context7工具不可用）

- WebView2进程与UDF：https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/process-model
- Windows宿主线程/STA：https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/threading-model
- 控制器父窗口：https://learn.microsoft.com/en-us/microsoft-edge/webview2/reference/win32/icorewebview2controller
- 本地资源：https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/working-with-local-content
- 桥与来源校验：https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/security
- 运行时分发：https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/evergreen-vs-fixed-version
- 原生跨进程重设父窗DPI限制：https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setparent
- QtWebEngine对照：https://doc.qt.io/qt-6.8/qtwebengine-features.html

上述资料为设计依据；本文件保留实施前基线。已完成实现及实际测量统一见../../verification/IF-1-independent-frontend.md，不以设计承诺代替验收。
