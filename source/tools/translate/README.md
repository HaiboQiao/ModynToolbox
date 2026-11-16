# 文本翻译 1.1.3

默认纯本地中英互译：输入/粘贴文本点击翻译，不需要网址、API Key、外部Python或首次下载。完整包包含Argos封装的OPUS中英1.9双向模型、CTranslate2 INT8 CPU、SentencePiece及私有Python。模型与依赖在工具自己的目录，宿主不加载推理库。需要宿主>=1.5.0（通用包上限512MiB）。构建产物约294MiB，压缩大小以发布清单为准；模型在执行时加载，CPU最多4线程，不要求GPU。

支持自动判断中英/指定方向、交换原译文、复制/导出、8000字符、段落、取消及部分结果。离线失败不会暗中切在线。翻译结果可能有误，专有名词/专业文本请复核。长句按标点及240子词有界分段，采用与Argos接近的beam=4、length_penalty=0.2；分段会影响上下文，不宣称LLM级质量。

可选“翻译方式”中仍保留MyMemory公共在线和LibreTranslate自建服务。只有主动选在线时才联网；旧会话的明确provider保持，服务可能有配额/Key需求。API Key只从用户变量MODYN_TRANSLATE_API_KEY读取，不进入页面、草稿或参数；本地模式不读取该密钥。

开发：python tools/translate/build.py，或scripts/prepare-tools.py统一调用。首次从offline.lock.json锁定的官方PyPI/Argos下载，SHA256不符即停止，缓存与产物在项目.local；用户收到完整包后无此下载步骤。依赖/模型许可见licenses和随包runtime的dist-info，模型是CC-BY-4.0，不误用Argos应用的MIT作为模型授权。

验证：tests/test_translation.py原在线协议回归；tests/verify_offline.py在私有解释器禁用网络、真实模型双向/段落/取消验证；tests/verify_host.py验证实际WebView2/WorkerHost的在线夹具与本地双向。数据在data/tools/translate，草稿按宿主开关，不自动创建翻译历史库。

私有推理进程设置CT2_USE_MKL=0、CT2_PACKED_GEMM=0、MKL_DISABLE_FAST_MM=1，避免MKL缓存提交内存撞上Job预算；不改Windows用户环境。当前样本单方向峰值提交约140MiB（原MKL约926MiB），双方向同进程约231MiB，仅为本机短句样本，不代表宿主/浏览器/所有文本总内存。
