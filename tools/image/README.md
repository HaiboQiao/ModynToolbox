# 图片压缩与转换

core/image_tool.py 为业务核心，standalone/run.py 为独立入口，integration/adapter.py 为协议适配，integration/ui.json 为工具界面。依赖 Pillow，完整包携带私有 Python/Pillow。

```powershell
.local/runtimes/pillow/python.exe -B tools/image/standalone/run.py sample.png --output outputs --format WebP
.local/runtimes/pillow/python.exe -B tools/image/tests/verify_core.py
python scripts/build-tool.py tools/image
```

支持静态 JPEG/PNG/WebP，EXIF 方向、去元数据、透明转 JPEG 白底、避免放大、编号避免覆盖及逐文件取消。默认输出到自己的 exports。容量最多 500 张、50 MiB/图、4000 万像素/图。依赖锁定与许可随包提供。
