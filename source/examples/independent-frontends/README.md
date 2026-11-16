# 冻结宿主后的独立工具示例

python-statistics为原生HTML/JS+独立Python核心；native-bytes为Vue+独立C++核心。仅依赖导出的集成SDK，不修改宿主。分别在自己的目录开发和构建，前端全部自绘。其源码不属于app，默认不随日常五工具安装，供工具作者复用与接入验证。

先导出SDK。Python：python SDK/scripts/build-tool.py python-statistics --output <新包目录>。C++：python native-bytes/build.py --sdk <SDK> --qt-root <自己的Qt SDK> --output <新包目录>。完整包放tools/<id>/<version>，或用主程序的--tool-package独立开发入口。
