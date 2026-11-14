# REL-4 1.3.0发布验证

2026-11-14，用户明确授权。实现源提交a637f32，功能验证Release14/14（52.40秒）、Debug14/14（87.53秒），原始证据见EXT-1-host-extensibility.md。本轮准备文件仅更新发行说明和入口，不改程序逻辑。正式固定来源、ZIP/源码启动器/公开下载和远端引用验证完成后追加真实结果。

物理DPI/多屏、另一台电脑、真实Chrome/ADS和特定Profile仍待设备验收，保留既有媒体限制与未签名状态。


## 发布及公开下载完成

固定发行源提交`0f99f3d23e39a09da8fe7fa878332771d9a978ee`，标签`v1.3.0`，Release ID `444852329`，公开时间`2026-11-13T23:57:54Z`。地址：https://github.com/HaiboQiao/ModynToolbox/releases/tag/v1.3.0。正式包source_dirty=false，data初始为空，编译版本1.3.0。

全部11项附件已核对服务器大小及SHA256，并匿名重新下载核对全部11项摘要。完整便携包103674384字节。正式ZIP与公开下载版均执行原有33步、视频专项34步、目录接入11步、真实编码写入后取消和受控暂存恢复。独立文件校验ZIP放目录刷新/执行通过；源码ZIP独立启动器3项通过。

额外对正式ZIP及公开下载宿主执行UI v3真实接入12步：使用测试专属完整JSON派生包，动态/级联字段、中文标签与稳定值、草稿、分离/收回窗口及执行。独立Python后端实际收到sheet=s1、column=VIN、mode=local，宿主EXE摘要不变。测试包仅在私有验证目录，不作为正式工具发布或覆盖用户工具。

首轮私有UI v3验证脚本假设SystemRoot键存在，启动前KeyError；修正为兼容键名与系统目录回退后通过。首次GitHub资产上传curl35/TLS握手失败，保留同一草稿和源提交重试成功，未覆盖公开资产。失败和重试日志保存在.local/logs/REL-4-*以及.local/upload-progress。上述不是应用业务失败。

发布回执、分发清单、SHA256SUMS位于仓库releases/1.3.0。公开下载及解压包在.local/public-downloads/v1.3.0，UI v3截图/报告在.local/rel4-ui3-tests。后续main文档提交归档公开结果，发行标签固定不移动。真实ADS/Chrome、特定Profile、异机、物理DPI/Snap和复杂4K等继续按原设备边界验收。
