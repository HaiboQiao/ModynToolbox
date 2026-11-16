# 当前架构与开发规划入口
<!-- project-status: current -->

先读[项目背景](project-baseline.md)和[行为契约](behavior-contracts.md)。下表是已实施的职责定位，不是拟议类名清单；未来调整需按变更规则和实际授权执行。

| 模块 | 当前职责 | 保护重点 |
|---|---|---|
| app/main、launcher、platform/windows | 正常入口、用户Session单实例、登录启动项、原生外框/浏览器承载 | 单实例不限制工具进程；系统项只按明确动作改变 |
| app/ui/workbench | 会话身份、导航、布局/浮窗、状态、服务绑定 | 显示位置不重提作业；不同会话独立 |
| app/ui/tool_runtime | ToolSurface接口、完整WebToolView | 页面拥有业务结构，桥不理解工具业务字段 |
| app/ui/components/tool_panel、view_spec | 旧UI1/2/3兼容 | 不作为新工具必须套用的模板 |
| app/jobs、worker_host | 调度、协议、资源/进程树、取消与退出 | 非UI重处理、归属明确、已有写入保护 |
| app/catalog、storage | 包发现/校验/版本、持久化与备份 | 代码/数据分离，坏状态保留 |
| tools/<id>、examples | 工具自己的core/frontend/integration/tests/依赖 | 使用既有能力时独立构建接入 |
| shared | 版本化协议、SDK与接入帮助 | 兼容变更需更新双方与验收 |
| scripts/change-guard.py、docs | 当前背景、变更影响与验证记录 | 不自动授权、回滚或发布 |

实施细节：[IF-1](features/independent-tool-frontend/plan.md)、[DS-1](features/desktop-startup/plan.md)、[GP-1](features/project-guardrails/plan.md)。新增工作在对应features维护，按任务控制范围。旧架构比较与R2原计划见[历史](history/gp1-before/plan.md)，不再要求重做已替代的固定工具UI路线。
