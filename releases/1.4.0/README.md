# ModynToolbox 1.4.0

Windows x64 · 2026-11-16 · 完整便携版。普通用户下载完整包、解压到可写目录，双击 **ModynToolbox.exe**，无需系统Python/Qt/开发工具。保留app、runtime、tools、data、licenses整个目录。

## 下载

- [完整Windows x64便携包](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.4.0/ModynToolbox-1.4.0-windows-x64.zip)
- [完整开发源码快照](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.4.0/ModynToolbox-1.4.0-source.zip)
- [工具作者接入SDK](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.4.0/ModynToolbox-1.4.0-tool-sdk.zip)
- 独立工具：[图片压缩1.2.3](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.4.0/ModynToolbox-tool-image-1.2.3-windows-x64.zip)、[视频裁剪1.2.3](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.4.0/ModynToolbox-tool-video-clip-1.2.3-windows-x64.zip)、[文本翻译1.0.3](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.4.0/ModynToolbox-tool-translate-1.0.3-windows-x64.zip)
- [独立媒体引擎](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.4.0/ModynToolbox-media-engine-9.0.2-modyn1-windows-x64.zip)及[精确对应的第三方源码与构建记录](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.4.0/ModynToolbox-1.4.0-third-party-source.zip)
- [SHA256校验](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.4.0/SHA256SUMS.txt)与[机器可读清单](https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.4.0/release-manifest.json)

## 本版内容

工具拥有完整前端页面和独立后端，宿主只提供工具箱、会话、作业与窗口等公共服务。工具作者决定布局，使用已有桥能力新增工具不需修改/重编译app。随附独立SDK和Python/HTML、C++/Vue示例，旧ui.json工具继续走兼容路径。

默认三工具：图片压缩、视频裁剪、文本翻译。翻译支持中英双向，默认公共MyMemory在线服务，也可配置自建LibreTranslate；公共服务有网络/配额限制，文本会发送至所选服务。未捆绑大型离线模型，保留后续服务扩展。图片和视频处理在本地完成。

公共界面支持工具下分支会话、拖出独立窗口、多会话、页面缓存设置、工作区轻留白/贴边切换。改进原生窗口缩放/绘制同步、边缘命中、工具切换反馈、输入焦点边框及滚轮防误改。关闭按钮使用正常实红悬停；提供设置内开机自启动和单实例唤醒。Python启动失败给出具体诊断并减少系统弹窗干扰。

项目自带背景、行为契约、变更保护、开发记录和交接文档；更换电脑或AI仍可从源码入口接手。源代码使用的编译工具不需要安装到普通用户电脑。

## 工具安装与升级

独立工具ZIP直接解压到应用tools，得到tools/<id>/<version>/manifest.json；目录自动发现，亦可按F5刷新。完整前端包要求宿主>=1.4.0，每个工具需携带静态前端、私有后端依赖和校验清单。不要仅复制业务源码，也不要覆盖同版本。

建议升级时先备份旧目录和数据，再将新版完整包解压到新目录验证。1.4.0改变默认工具集，直接覆盖旧目录不会替用户移除旧工具。文件校验、JSON、批量重命名不随新包分发，源码与回归测试保留。删除工具代码不会删除data/tools/<id>。

旧会话固定原版本，保留对应旧包可继续运行；缺包时通过新版入口新建会话，不把未知状态强制迁移。不要让旧宿主保存新工具状态。若要迁移已有工作区，先备份并保留旧工具版本；新安装包data初始为空。

## 验证与设备边界

发布前执行Release与Debug完整回归、冻结宿主下两种独立工具SDK接入、ZIP完整性、受限PATH便携业务、独立ZIP放目录执行及视频专项。发布流程先草稿上传校验，再公开并匿名下载所有附件复验；精确提交和最终结果见本目录发布回执/清单及源码发布验证记录。

本版未进行代码签名，Windows可能提示未知发布者。自带固定WebView2提高下载和解压体积，换取不依赖用户系统浏览器运行时。物理多屏DPI/Snap、真实登录自启动、IME、另一台干净电脑及复杂媒体/空间ACL仍需各自目标设备验收；后台原生测试不等于这些设备全部通过。

[发布规程](../../source/docs/release-policy.md)规定仅在用户明确要求发布时执行，并同步源码与发行资产到GitHub。
