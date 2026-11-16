# 开发修改与功能保护规则
<!-- project-status: current -->

先读[当前背景](project-baseline.md)、[行为契约](behavior-contracts.md)、[STATUS](STATUS.md)。本规则属于项目协作流程，不是权限沙箱，不给Agent增加用户未授权的范围。

## 三种等级分别判断

- 保护强度H：PB核心约束，普通局部优化不得顺手改变；S：BC已承诺行为，改关联模块须回归；U：表现偏好，按请求优化但不能损害H/S。
- 排期P0故障/数据风险、P1核心流程、P2效率增强、P3表现细节，只用于先后顺序。不是“低优先级可坏”。
- 风险low/medium/high由实际修改位置保守推导；公共会话/存储/协议/启动/构建等high，单工具或公共视觉一般medium，文档一般low。规则/测试修改要求复核；未知运行路径按high，不静默略过。

## 每次修改的最低动作

1. 核对当前用户请求、仓库/工作区及已有未提交变化。任务开始、换AI/上下文恢复、准备改共享能力或范围变化前重读基线；同任务不反复扫描全部历史。
2. 简述本次目标、允许改动范围、关联PB/BC及必须保持的行为。小需求直接实施；必要关联修改可纳入范围并说明，不借“同步优化”做无关重构。
3. 开始前运行start保存内容快照及当时规则索引。它保存当前工作树而非仅HEAD，已有未提交成果不会被误算或回滚。禁止用重新start覆盖基线来掩盖改动。
4. 按需求实现，review查看实际变化和所需检查。若超出原范围，先解释必要性；原任务已覆盖的关联调整可以修改记录并说明，改变核心产品约定/数据语义/外部副作用则必须有当前用户的明确依据。不要因普通实现细节重复请求批准。
5. verify执行真实检查。代码/测试/SDK/核心规则再改后旧验证失效。失败先定位，不擅自删除保护测试、放宽原验收或填“通过”；依赖缺失保持待测。
6. 检查差异、补当前状态/证据，再finish。规则/测试发生变化须附复核说明，指出保留/替代了什么及本次请求依据。检查不自动回滚、提交、发布或终止用户程序。

普通修复复用既有规格/记录；新增行为或架构变化在对应features中维护spec/plan/task/checklist。内部步骤连续推进，文档“批准”标题不替代真实授权。

## 实际命令（从source目录执行）

```powershell
python scripts/change-guard.py validate
python scripts/change-guard.py start close-color --priority P3 --request "用户要求只调整关闭颜色" --allow app/ui/window/chrome_window.cpp --allow app/ui/theme/theme.h --allow app/ui/theme/theme.cpp --allow app/tests/integration/test_desktop_startup.cpp
python scripts/change-guard.py review close-color
powershell -NoProfile -File scripts/dev.ps1 test -Preset windows-release -GuardTask close-color
python scripts/change-guard.py finish close-color --review-note "说明现有关闭/浮窗/作业行为如何保留以及保护测试变化"
```

示例只演示用法，不授权立即执行该颜色修改。allow填写源目录相对路径，新增文档按需明确允许；固定STATUS/worklog/task/checklist和verification证据属于可记录范围。Git仓库根在source上一层时，根入口使用`@repo/AGENTS.md`等逻辑名；源码ZIP单目录仍可运行。不要填任意整盘目录或覆盖其他项目。

依赖已构建时可直接`python scripts/change-guard.py verify <任务> --preset windows-release`。verify使用固定Python检查和已登记CTest；涉及CTest时先核对缓存属于本项目并构建当前源码，结果同时绑定编译产物，避免拿旧EXE测新代码。后一次失败覆盖之前同代码的通过记录。它，先核对实际收集到的测试，零测试不是成功；不执行任务记录提供的任意shell命令。原始输出和记录在`.local/change-guard/<任务>/`。release/package不等于发布。

`dev.ps1 test/package`始终先validate文档结构；指定GuardTask的test按本轮实际改动选择回归。普通run/打开程序不需要先创任务，直接cmake/其他AI也可能绕过检查，因此AGENTS要求并不等于不可绕过的系统限制。未建任务的常规全量测试不能自动证明本轮没有越界。

## 更新允许范围与受保护约定

开始时尽量列出必要目录/文件。若确需扩展范围，在record.json的allowed中增加具体路径并在review_note记录理由/请求依据，保留before快照和原请求；不得重建before。范围声明是可审查记录，不是脚本核验过的用户授权。原有保护路径/检查路由与当前配置共同复核，不能通过改bookkeeping把运行代码藏起来；退役检查须说明替代验证。更改H/S约定时同步基线、行为索引、兼容/迁移及检查；未经用户明确改变产品要求不能这样做。

保护测试修改未必违规，但需要解释；删掉被索引的关键用例会使validate失败，必须审查新的等价验证。机器只能检查文件/引用/结果摘要和范围，不能证明测试语义没变、说明真实或所有AI必读。高风险仍须业务审查和既有文件/版本迁移保护。

## 当前事实与历史的维护

本基线、BC和对应功能规格是当前有效要求；STATUS只记当前状态与证据入口。历史详单进history/worklog/verification，不反复追加为“当前”。代码用于核对事实，不把错误现状当需求。变更优先保留Git可对比检查点，提交与推送/发版分开；不得顺手提交别人的工作。

规则是项目自包含文档，不安装个人Skill或修改全局AI/Git钩子。没有自动读AGENTS能力的AI，从[handoff](handoff.md)复制显式接手提示。真正跨AI/电脑验收要单独记录，本机冷读不冒充通过。


检查范围是源码/规则，不全量监控用户data、.local生成物或外部依赖；这些仍由授权、文件权限、目录隔离与业务测试保护。任务记录也不是防篡改签名；脚本自身可被绕过/修改，不能把一个PASS当作需求语义已被自动证明。
