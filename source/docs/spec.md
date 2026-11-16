# 当前产品规格入口
<!-- project-status: current -->

本文件已整理为有效规格索引；[当前背景](project-baseline.md)给出产品目标/PB边界，[行为契约](behavior-contracts.md)给出BC及真实验收。代码现状、旧方案与Agent自写批准都不能覆盖这些要求。

| 当前范围 | 有效说明 |
|---|---|
| 工具拥有完整前端、独立后端、接入/媒体/生命周期 | [独立前端规格](features/independent-tool-frontend/spec.md)、[开发契约](independent-frontend.md) |
| 会话/浮窗/拖出/主工作区 | PB-03、BC-02；[导航决策与历史](decisions.md) |
| 版本/移除/草稿/写入恢复 | PB-04/05/06、BC-03/04/05及工具自己的core/integration/tests |
| 正常单实例/用户登录自启动/关闭控件 | [DS-1规格](features/desktop-startup/spec.md) |
| 旧UI1/2/3动态字段和兼容 | [EXT-1设计](features/host-extensibility/design.md)、shared/protocol；仅兼容，不是新工具模板 |
| 长期背景、变更范围、验收保护 | [GP-1规格](features/project-guardrails/spec.md)、[变更规则](change-policy.md) |

## 替代关系

2026-11-08初始规格全文及原F编号见[历史原文](history/gp1-before/spec.md)。其中“工具UI必须使用公共控件树、复杂交互扩展宿主、共享渲染器统一工具页”等早期路线已被用户澄清的IF-1替代。已有业务保护和旧包兼容保留；不能把历史整份当作仍有效，也不能因此删旧测试。

公开版本/本地状态和未完成设备验收只在STATUS维护；需求与实现尚缺的部分必须明确说明，不能从历史完成标题推断当前全部通过。
