# Offline translation components

Models: Argos Translate packaged OPUS English↔Chinese 1.9. Original authors: Jörg Tiedemann and Santhosh Thottingal; OPUS-MT, EAMT 2020. These derived OPUS models declare CC BY 4.0 in their original READMEs (included unmodified). Model weights are used as provided; no retraining. The application performs bounded sentence/token segmentation and CTranslate2 INT8 CPU inference.

Sources: https://data.argosopentech.com/argospm/v1/ and https://github.com/argosopentech/argospm-index . Exact archives, URLs and SHA256 in offline.lock.json. CTranslate2 (MIT), SentencePiece (Apache-2.0), NumPy, PyYAML and setuptools are obtained as pinned official PyPI wheels. All wheel dist-info/licenses and bundled notices remain in the private runtime. No online translation service is contacted in local mode. Machine translation can contain errors; review important text.
