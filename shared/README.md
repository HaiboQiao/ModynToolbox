# 公共接入层

protocol 中的 *-v1.schema.json 定义正式清单、UI 和消息结构。preview 规范仅用于兼容旧预览，不提供业务执行。

sdk/python 和 sdk/cpp 提供可复制到工具适配层的协议帮助。业务核心无需依赖 SDK。各工具的 SDK 副本随版本固定。

接入、容量和调度语义见 docs/tool-development.md。新增兼容工具无需宿主源码变更。
