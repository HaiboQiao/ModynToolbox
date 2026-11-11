# REL-2 ModynToolbox 1.1.0 发布记录

2026-11-11。用户明确要求发布并上传 GitHub，目标 HaiboQiao/ModynToolbox。本版按兼容新能力提升 minor 为 1.1.0，包含 B Logo、统一控件/数字箭头、工具库刷新、侧栏快捷入口、启动器说明和设计系统。

## 固定源码前验证

Release CTest 11/11，20.22 秒。Debug 11/11，41.10 秒。启动器专项3/3，2.502秒。四工具包指纹当前有效，工具版本 JSON1.0.2、图片1.0.1、重命名1.0.1、文件校验1.0.4。

启动器专项原依赖完整仓库外层，修正为在导出源码中验证源码自带入口，在完整检出中另外验证根路由，支持源码包独立接手。最终分发还将直接运行源码 ZIP 内该项测试。

## 发布阶段

按 release-policy 在干净源提交上重新 package，再 prepare/validate。固定提交与实际资产摘要由 releases/1.1.0 回执保存。上传及公开下载结果在完成后追加。

## 设备范围

本版未做 Authenticode 签名，物理跨屏/DPI/Snap 与另一台干净电脑仍按设备验收记录。自动范围为本机 Windows x64、Qt offscreen、私有测试数据和进程。

## 公开发布完成

固定源提交41fe271450446171c5a90530d771840362cfbf8a，标签v1.1.0。GitHub Release ID441095721，公开时间2026-11-11T00:39:41Z。

本地正式ZIP解压33步，3.829秒。运行中的宿主发现新增独立文件校验工具并执行正确。导出的源码ZIP内启动器专项通过，两个发行exe的Windows图标资源核对。

8项资产上传大小/服务器SHA256匹配，公开后匿名下载8项全部一致。下载完整包再解压，33步业务复验通过，3.563秒，运行PATH仅Windows/System32。完整便携65068970字节，源码5390999字节。所有资产来自固定源提交，发布数据为空。

[GitHub版本](https://github.com/HaiboQiao/ModynToolbox/releases/tag/v1.1.0)、[回执与校验](https://github.com/HaiboQiao/ModynToolbox/tree/main/releases/1.1.0)。原始日志在.local/logs/REL-2-*，公开下载保存在.local/public-downloads/v1.1.0。
