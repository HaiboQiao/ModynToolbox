# 本机生成物

本目录用于开发环境、构建、安装副本、打包及测试临时内容，生成物不纳入 Git。当前只建立用途说明。

```text
build/<preset>/
tools/<tool_id>/{envs,build,package}/
installed/{tools,runtimes}/
releases/
tests/
logs/
machine.json
```

机器配置可记录本机绝对路径。换电脑重建本目录中的环境和缓存，保留需要随身携带的离线依赖介质或工具包。清理生成物不能连带删除 data/，发布成果需要单独保存或复制。
