# REL-3 ModynToolbox 1.2.0 发布记录

2026-11-12。用户明确要求发布并上传GitHub，按release-policy连续执行。版本1.2.0，视频工具1.0.0且最低宿主1.2.0；其他四工具保持既有公开版本。

## 固定源提交前证据

- Release CTest14/14，51.11秒，.local/logs/REL-3-ctest-release.log。
- Debug CTest14/14，83.37秒，.local/logs/REL-3-ctest-debug.log。
- 视频核心20项使用正式精简引擎通过，覆盖精确/快速、时长/首帧、压缩、无覆盖、旋转/VFR/音轨、原文件保护与大文件。
- 源码启动器3项，2.767秒，.local/logs/REL-3-launchers.log。
- 原开发引擎静态链接较多外部库，未将其直接用于正式分发。正式引擎从锁定FFmpeg9.0.2和x264源码构建，外部媒体库仅x264，禁用网络/设备捕获/多余编码器，保留lavfi用于合成回归。最终两程序58.23MiB，导入表仅系统DLL。
- 对应源码含实际FFmpeg/x264归档、配置脚本/版本/工具链记录，以及Qt6.8.3 base/multimedia和其播放FFmpeg7.1源码，哈希清单在licenses/media-sources.lock.json。

## 保留的失败与处理

精简初次配置禁用avdevice后，回归生成器缺少lavfi输入。启用内部lavfi而继续禁用其他输入/输出设备后，重新构建、封存引擎并通过20项真实媒体回归。源码和配方一致，未用旧引擎冒充正式验证。

Debug首轮13/14，视频界面测试在QtTest输出前启动异常，单独重试复现。私有调试记录执行空地址，旧结果文件时间未更新，未将旧8项通过记录当成本次结果。只移除该测试生成exe和ilk，关闭增量链接重建后，单项连续3次通过，最终全量14/14。类似的本地增量链接问题此前已记录，但本次具体根因未确定；不宣称已证明编译器缺陷。原始日志REL-3-ui-debugger、ui-relink、ui-relinked-tests、ctest-debug-first均在.local/logs。

## 正式资产与复验

准备生成完整便携、源码、五个独立工具、独立媒体引擎、第三方对应源码、分发清单和SHA256SUMS共11项。源码提交/标签绑定、ZIP解压验证、视频专项、运行中目录接入、公开下载/服务器摘要和最终回执在完成后追加，实时事实以releases/1.2.0/publication.json为准。

## 设备与分发边界

Windows x64，本机自动化、Qt offscreen和受限PATH分别留证，实际下载版需再跑业务。原生播放观感/听感、多屏DPI/Snap、另一台干净电脑、真实空间/ACL故障及复杂4K矩阵仍待设备验收。精确模式暂不支持HDR/高位深/交错。程序无Authenticode签名。第三方组件分别保留相应许可，原始应用代码许可未由项目所有者另行选择。
