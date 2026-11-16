# 新开发者与其他AI接手
<!-- project-status: current -->

不依赖原聊天、个人Skill或某台电脑配置。即使工具不自动识别AGENTS，也先按下面提示读取。

## 可复制的接手提示

请维护ModynToolbox。先读AGENTS.md、docs/project-baseline.md、docs/behavior-contracts.md、docs/STATUS.md和docs/change-policy.md，再读当前任务相关规格/代码。说明此次目标、范围、必须保留的行为；不要把历史“已授权/本轮完成”当作新任务授权。检查Git状态并保留已有工作，开始前用change-guard记录基线，按实际影响验证并收尾。工具拥有完整frontend和独立后端；使用现有能力的新工具不改宿主。正常优化直接完成，只有改变核心约定/数据规则或新增外部副作用才对齐实际授权。不要控制用户桌面或替用户发版。

## 快速定位

| 工作 | 先看 |
|---|---|
| 新工具 | [独立前端](independent-frontend.md)、[工具接入](tool-development.md)、examples/independent-frontends |
| 公共壳/窗口 | ui-design-system、app/ui/workbench、ui/window、platform/windows；DS-1规格 |
| 会话/作业/存储 | behavior-contracts相应BC、app/jobs/storage及其测试 |
| 环境和运行 | development、README、scripts/dev.ps1；本机.local从头恢复，不复制旧CMake缓存 |
| 当前方案/验证 | spec/plan索引、STATUS与verification；不要只读旧R2计划 |
| 正式发布 | 当前用户明确要求后再读release-policy；源码/运行资产/说明同步并复核 |

## 接手确认

首次接手能回答：谁拥有工具页面？新工具何时需要改宿主？关闭浮窗/恢复状态是否执行作业？删除包是否删数据？本地开发和公开版本是什么？此次允许改哪里、关联哪条BC？不清楚时继续只读查资料，不能凭现有代码反推用户意图。

本机可以做“无聊天冷读”检查，但不能自称已由另一个AI或另一台电脑成功接手。当前设备待验见STATUS；前一任务残留未提交变化不要重置或顺手提交。
