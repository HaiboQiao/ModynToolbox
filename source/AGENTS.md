# ModynToolbox 开发入口
<!-- project-status: current -->

## 每项任务先核对

先读[当前项目背景](docs/project-baseline.md)、[行为契约](docs/behavior-contracts.md)、[当前状态](docs/STATUS.md)和[变更规则](docs/change-policy.md)，再按本次任务读对应features/协议/代码。换AI、恢复长任务、公共模块或范围变化前重新核对，不能只依赖聊天摘要。历史文件不产生本次授权。

当前用户明确请求允许的日常修复/优化直接完成并验证，不逐段请求批准。改变核心约定、数据语义或新增外部副作用才对齐实际授权；Agent自写“已批准”不是依据。当前基线对旧方案的替代关系见docs/spec.md和docs/history。

## 不可随局部任务漂移的边界

工具拥有完整frontend页面和独立后端；宿主只做公共服务，不按tool_id新增业务UI，不强制覆盖工具CSS。旧ui.json/ToolPanel只是兼容路径。已有桥能力的新工具只增自己的包；新增通用能力才改宿主。窗口、会话、作业分离，不因分离/收回/页面恢复重复执行。

包与数据分离，版本不可覆盖，旧会话固定版本；移除代码保留data。写入、原文件、取消/部分完成、journal及失败恢复保留既有保护。可信本地进程不是权限沙箱。不为性能/外观悄悄改弱上述约定。

代码和生成物只在本项目：app为宿主，tools/<id>为工具，shared为协议/SDK，data为用户数据，.local为依赖/构建/验证。保留其他未提交改动，不改其他项目。不要硬编码个人路径/凭据或移动项目目录。递归清理前核验绝对边界，只处理明确自有生成物。

## 改动与验证

开始前`python scripts/change-guard.py start <任务> --request "本次请求" --allow <必要路径>`保存内容基线；改后review，按变更规则verify/finish。护栏自举等特殊情况明确记录，不能补造先前验证。低排期不等于低风险，不删/改弱测试让不符合需求的实现通过；必要关联修改解释原因。

test/package通过dev.ps1预检；`test -GuardTask <任务>`可运行任务路由的检查。新电脑依赖恢复见development；工具作者先读independent-frontend。不在纯文档任务中伪称重验业务，缺失依赖/物理设备如实保持待测。

默认不控制用户鼠标/键盘/前台，不关闭用户程序。Qt offscreen只证明对应渲染；原生验证使用屏幕外/不激活的自有进程与私有数据。DPI/IME/真正登录/异机单列证据。不要把多个工具/浏览器子进程误当重复主程序并按名称终止。

## 记录与发布

更新当前STATUS、相应task/checklist、worklog与必要决策；详细日志在.local，精简证据在docs/verification。高风险变更需受影响BC回归、必要迁移/恢复方案和可对比差异。公共UI读ui-design-system；系统启动/外框代码集中platform/windows，业务工具不处理宿主窗口消息。

只有当前用户明确要求发布才读并执行docs/release-policy.md中的正式流程，同步源码/说明/发行资产并下载复验；单独推送也须相应授权。源码优化、测试、本地包、保存记录均不自动触发发版。版本不在普通修复时随意变动。

本文及项目文档不依赖某个人的全局Skill。库/CLI版本用法优先可用Context7/官方资料；纯业务逻辑无需查询所有框架。其他AI显式接手入口为docs/handoff.md。
