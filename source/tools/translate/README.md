# 文本翻译

独立HTML/CSS/JS前端 + Python标准库后端，不改宿主即可接入。1.0.3支持中文/英文自动互译、手动指定/交换、段落保留、复制/导出、取消和部分结果。

默认MyMemory公共在线，无需APIKey即可试用，有服务配额与500-byte单段限制，工具自动分段、最多8000字符/40段。仅点击翻译发送，不能保证所有文本译质或服务持续可用。公开API并非开源引擎。

LibreTranslate是开源服务方案（https://github.com/LibreTranslate/LibreTranslate，AGPL-3.0），在服务设置选择并填写地址，例如本机http://127.0.0.1:5000；需自行部署并安装zh/en模型，远程使用HTTPS。需要Key时设置Windows用户级环境变量MODYN_TRANSLATE_API_KEY（例如系统的“编辑账户的环境变量”界面），后端从环境变量读取，前端/草稿不接收密钥。可自建部署在本机实现模型安装后的离线使用；本工具不自带引擎和模型。

协议参考：https://docs.libretranslate.com/api/operations/translate/；公共服务：https://mymemory.translated.net/doc/spec.php 。本项目自己实现适配和页面，不包含Pot或LibreTranslate应用源码。后续本地引擎只需在core/translation.py提供适配，不改变宿主。

构建：python scripts/build-tool.py tools/translate。测试：python tools/translate/tests/test_translation.py。用户数据位于data/tools/translate；草稿服从工作区开关，不自动建立翻译历史库。

部分宿主工作进程会过滤自定义进程环境；本工具先读进程环境，再仅从HKCU/Environment读取同名用户设置，不读取其他凭据。没有个人Key时用公共试用或无需Key的自建服务。
