# 当前工具开发与接入
<!-- project-status: current -->

先读[项目背景](project-baseline.md)，新工具从[独立完整前端指南](independent-frontend.md)开始。工具拥有自己的全部页面/样式/交互，后端语言和依赖独立；不是先找宿主有没有该业务控件。

## 新工具的最短路径

1. 在自己的目录实现core和frontend，按选定框架生成自包含静态产物，保留自己的测试/依赖/许可。
2. integration只适配版本化前端桥和后端协议；清单manifest v2/frontend v1、最低宿主声明和自己的不可变版本同步。
3. 用导出的集成SDK构建完整工具包，复制tools/<id>/<version>，由宿主自动发现。使用既有能力不改app/CMake；SDK不含业务组件或强制CSS。
4. 用真实host runner或工具箱验证自己的输入/处理/取消/错误/会话/窗口/草稿。模拟器只证明前端开发，不代替实际业务。
5. 升级创建新版本，旧会话保持固定；关闭引用/作业后移除代码包，保留data。工具源码与可运行包是不同对象，不能只复制裸脚本假装完整安装。

## 验收与回归

相关约定见[BC索引](behavior-contracts.md)，变更前记录范围。影响公共桥/协议/运行时或构建器时，在已构建Release基础上执行`python scripts/verify-frozen-frontends.py --freeze`，保存宿主前后哈希和独立工具构建证据；不依赖某台电脑的临时脚本。自己的新工具仍需实际执行/输出验收，两个示例不能替所有工具保证业务正确。

## 旧UI维护

ui.json和UI1/2/3继续兼容，老工具作者可按实际需要沿用；这不满足“作者自绘整个页面”的新接入路线。旧字段说明见[历史兼容说明](history/gp1-before/tool-development.md)，准确Schema以shared/protocol为准。动作/容量限制因版本不同，不从旧说明复制到新清单。

公共视觉建议见[UI约定](ui-design-system.md)，完整页面可自行选择组件库和任务布局。只新增业务图表/编辑器不扩展宿主控件；真正新增宿主系统能力才讨论桥和版本升级。
