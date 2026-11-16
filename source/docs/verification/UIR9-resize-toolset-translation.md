# UIR-9 缩放、工作区、工具精简与翻译交付

本地宿主仍1.4.0；默认图片压缩/视频裁剪1.2.3、文本翻译1.0.3。未发布、未推送GitHub，公开1.3.0资产不变。规格见[UIR-9](../features/ui-r9-translation/spec.md)。

## 缩放闪烁：发现与修复

代码检查确认原先每个resize都会对8个透明命中层重复设置透明度和Z序，并向浏览器重复设置父HWND、Bounds、移动通知与可见性；这些不必要的原生更新会触发额外重绘/合成。改为仅首次/句柄变化配置命中层，Bounds、父窗口、可见性、屏幕位置分别按变化更新。普通布局消息合并，**WM_ENTERSIZEMOVE原生拖动循环内直接同步Bounds**，不依赖0ms定时器才能实时更新；退出/取消复位。隐藏事件只改可见性，避免把已停放的控制器挂回退休HWND，100次迁移保护保留。

连续尺寸回归又暴露真实DPI不一致：Qt宽817、DPR1.25，浏览器innerWidth681、DPR1.5。不能只放宽尺寸断言；现在关闭WebView2自主监测比例，使用Qt实际比例设置RasterizationScale，显式RawPixels Bounds模式，并响应DPR/移动/父窗口变化。最终尺寸断言、字体比例与原生迁移通过。API口径见[Microsoft Controller3](https://learn.microsoft.com/en-us/microsoft-edge/webview2/reference/win32/icorewebview2controller3)。

两个真实Web工具各30次原生缩放循环后：父窗口、可见性和透明命中层的重复配置增量均为0，页面身份不变、最终视口吻合；保留原有独立窗口、草稿、故障恢复和窄窗测试。另对**当前保留的image/video-clip/translate各40次**发送自有屏幕外HWND尺寸变化及进入/退出尺寸循环消息，21步通过，页面标记和水平溢出检查通过，截图已看。没有发送全局鼠标键盘事件。

这些证据证明去掉多余更新、尺寸/状态稳定；不是物理显示器的高速DWM逐帧录像，不能冒称所有显卡、多屏和真实手拖观感都已完全无闪烁。用户仍可在原开发入口验收实际拖动。

## 分隔线与工作区

原来sidebar右边线叠加3px默认QSplitter把手，显示为多余底色窄条。现在侧栏不画重复右边线，把手1px/单细线，使用Qt自动扩展命中区保持可拖动（[Qt QSplitter](https://doc.qt.io/qt-6.8/qsplitter.html#handleWidth-prop)）。

按用户重新提出的审美需求，默认“轻留白”：四周10逻辑px、极淡直线边框、白色工具表面，不用厚圆卡片或阴影；设置“工作区外观→贴边”恢复0留白。应用、取消及持久化已测。这里只改公共承载层，未注入工具CSS，工具完整页面仍独立。

## 三个工具已停用

file-hash/json/rename从开发安装库移出，三个活动会话从活动列表移出；没有删除data/tools下的数据。SQLite备份API保留整库，代码包转存.local/disabled-tools/uir9-ddac6697，备份data/host/backups/uir9-ddac6697/state.sqlite。真实目录移动前已验证绝对路径位于本项目，并检查开发数据没有实例锁；SQL修改失败会回滚并移回包。

三个源码保留给历史兼容与恢复，不归入默认分发。tools/toolset.json控制默认构建/打包；重新执行tools仍为image/translate/video-clip，未装回三个工具。宿主没有按ID封锁外部工具，别人复制完整包的通用发现能力保留。历史发行目录不强删，新本地包只含三工具。不要拿整库旧备份覆盖后来新数据；恢复应另做合并/新会话。

## 翻译工具的选择与实现

调研[LibreTranslate](https://github.com/LibreTranslate/LibreTranslate)和[Pot](https://github.com/pot-app/pot-desktop)。LibreTranslate提供开源翻译服务接口、支持安装模型后的本机服务；Pot是完整桌面聚合应用，整体嵌入会重复壳与运行环境。本版自己实现轻量适配和完整页面，不复制Pot/GPL界面或打包LibreTranslate/AGPL服务；未来实际分发这些服务时再按其许可整理来源与交付。

- 独立源目录tools/translate：HTML/CSS/JS + Python标准库后端，使用现有桥，无宿主按translate ID写业务UI。完整包约23.35MiB，不随箱子增加大型模型；后端按作业启动。
- 支持中文/英文自动互译、手动语言、交换、段落保留、Ctrl+Enter、复制/导出、取消及已完成部分结果；最多8000字符/40段，有单独错误说明。默认窗的双文本区按剩余空间伸缩，翻译按钮不被挤出视口；窄窗纵向排布。
- 默认公共在线MyMemory，无需提供个人Key即可试用，有用量限制，单段最多500 UTF-8字节，按[官方接口](https://mymemory.translated.net/doc/spec.php)自动分段；这是公共API，不是开源引擎。页头明确当前服务，点击才发送原文，不自动翻译/上传剪贴板。
- LibreTranslate模式填写自建服务地址，远程HTTPS、本机允许HTTP，按[官方接口](https://docs.libretranslate.com/api/operations/translate/)对接。需要用户自行部署并安装中英模型；本工具不自带离线引擎。模型就绪的本机服务可离线使用，未来本地引擎只扩展工具自身适配。
- Key只取进程或同名Windows用户环境变量MODYN_TRANSLATE_API_KEY；宿主可能过滤进程环境，因此后端仅回读HKCU/Environment这一显式设置，不读取其他凭据。Key不进页面、草稿、作业参数或返回值；未写入真实注册表/环境变量，也未用私人Key做线上验证。Key配置分支通过单元模拟验证。
- 仅支持普通文本；公开示例成功不代表长文/术语译质基准。私密内容应选自己的服务，服务配额与模型质量属于外部条件。

## 最终验证口径

| 检查 | 结果 |
|---|---|
| 最终Release全部CTest | **19/19，126.74秒**，包含新增translation_core/translation_host和全部旧工具兼容保护 |
| Debug公共代码阶段 | 19/19，174.86秒；之后最终变更仅原生拖动Bounds通路/翻译页面，已重建Debug并对受影响项复验，不能把先前19写成最终全部19 |
| 最终Debug专项 | Web 9/9（65.182秒），原生窗口5/5，翻译真实宿主17步通过 |
| 翻译独立核心 | 8项通过，覆盖UTF-8分段、双向、Key来源/不回显、错误、取消前检查与本机HTTP/重定向拒绝 |
| 翻译原生宿主 | 本机HTTP夹具17步：双向协议、独立进程、草稿、分离/收回、按钮可达；夹具不冒充译质 |
| 真实在线 | 最终完整应用7步，MyMemory公开样例Hello world↔你好世界实际互译成功；自带Python/工作进程执行，非本地词典假结果 |
| 当前三工具原生尺寸改变 | 每工具40次，合计120次/21步通过；无全局输入 |
| 冻结宿主新工具接入 | 最终Release20步，Python/HTML与C++/Vue，实验内宿主源码与EXE不变 |
| 项目护栏自身 | 21项中20通过/1因Windows符号链接权限1314跳过 |
| 最终精简包 | **30步（26业务+4恢复），10.888秒**；当前三工具、独立PATH、自带Python与Fixed Runtime157.0.4341.60实测。原48步含已停用工具；那些源码能力仍在19组回归验证，未以减少便携步骤掩盖失败 |

源码与编译产物绑定在.local/change-guard/ui-r9-translation/record.json。最终Release EXE SHA256：`7021045ee6c738f736fd2c5b136d102b472e7c4aee621ee3e631b51bdb2e3226`。本地包`.local/releases/ModynToolbox-1.4.0-bdd6e50e`，其中data空，验证在私有副本完成。证据见[UIR9-evidence](UIR9-evidence/)，包括[最终真实翻译页](UIR9-evidence/translation-live.png)。

## 失败纠正和并行工作

初版1.0.0适配器未向嵌入Python添加自身目录，导致找不到modyn_sdk、未ready；修正路径后用新不可变版本，不覆写旧包，并从安装库移出失败预览包。1.0.2补齐过滤环境下的Key配置；1.0.3依据真实截图修复底部按钮被挤出、明确当前服务。旧失败日志保留，没有把mock互译当真实服务测试。

视口初断言把clientWidth与Qt宽度直接比较，随后实际诊断确认DPI不同步；最终修正渲染比例并保留尺寸/溢出约束，未删掉失败测试。新翻译两项已登记docs/guardrails.json长期必检，保留全部旧检查。

侧聊py-startup-dialog在本轮期间修改worker_host、job_runner及故障夹具/说明。主任务没有编辑它们；来源和只读哈希已单独记入任务记录，只为同工作树完整回归承认这些已有差异，原before不变。最终哈希确认这些文件未被本轮修改，整体验证包含其现状；不把侧聊修复归功本轮，不替侧聊修改任务状态。浏览器异步缓存锁和屏幕外几何警告保留，未按进程名清理用户应用。

## 打开

正常退出旧窗口，继续Run-Dev.cmd/dev.ps1 run/VSCode即可；Debug已构建。Open-ModynToolbox.cmd本机latest也更新。翻译1.0.3有公共在线即用入口，自建服务在工具内展开设置。数据与旧工具备份保留，未发布上传。
