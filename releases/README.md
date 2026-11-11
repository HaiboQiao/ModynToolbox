# 发布版本

普通用户从这里选择版本。完整 ZIP 托管在 [GitHub Releases](https://github.com/HaiboQiao/ModynToolbox/releases)。

| 版本 | 下载与说明 | 平台 |
|---|---|---|
| 1.1.0 | [新版品牌与统一界面](1.1.0/README.md) | Windows x64 |
| 1.0.2 | [完整便携版、独立工具和校验](1.0.2/README.md) | Windows x64 |

## 下载后打开

1. 下载 Windows x64 完整便携 ZIP。
2. 将整个 ZIP 解压到可写目录。
3. 双击 ModynToolbox 文件夹中的 ModynToolbox.exe。

保留整个应用文件夹。程序自身携带 Qt/MSVC 运行库，各工具携带自己的依赖。首次使用不要求系统 Python 或开发工具。

## 增加工具

从版本附件下载独立工具 ZIP，解压到应用的 tools 文件夹中，形成 tools/<id>/<version>/manifest.json。点击侧栏工具库右侧刷新按钮或按 F5，也可打开“工作区管理 → 工具包 → 刷新目录”，即可发现并运行兼容工具。

开发者先按 [接入规范](../source/docs/tool-development.md)构建完整工具包。工具版本与主程序版本独立，升级使用新的工具版本目录。已有会话固定旧版本，关闭引用后再移除旧版。移除工具程序保留 data/tools/<id> 中的数据。

## 源码

开发和 AI 接手使用 [source](../source/README.md)。[发布规程](../source/docs/release-policy.md)记录版本、打包、验证、GitHub 上传及下载复核流程。
