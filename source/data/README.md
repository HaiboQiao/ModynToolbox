# 本地运行数据

开发运行默认使用本目录，实际数据库和用户内容由应用开发后生成。本目录实际数据不纳入 Git。

```text
host/
  state.sqlite3
  logs/
  backups/
tools/<tool_id>/
  settings/
  storage/
  drafts/<session_id>/
  jobs/<job_id>/{input,result,temp,journal,logs}/
  exports/
```

每个工具的数据集中在自己的目录。工具包移除默认保留此处数据，清理临时文件只按作业归属处理。exports 是建议的默认导出位置，用户明确选择的其他目录也可作为输出。

复制迁移数据前先正常退出应用并确认没有所属 worker 写入。正式备份/导入由后续迁移入口处理。源码记录与用户数据分别迁移，详情见 [开发指南](../docs/development.md)。
