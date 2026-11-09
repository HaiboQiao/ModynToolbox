"""Atomic image conversion. No host/UI dependency."""
import os
from pathlib import Path
import tempfile
import warnings
from PIL import Image, ImageOps

MAX_FILES = 500
MAX_BYTES = 50 * 1024 * 1024
MAX_PIXELS = 40_000_000
Image.MAX_IMAGE_PIXELS = MAX_PIXELS
COLUMNS = [{"key":"name","label":"文件"},{"key":"before","label":"原始大小"},
           {"key":"after","label":"输出大小"},{"key":"status","label":"状态"},{"key":"output","label":"输出位置"}]


def publish(temporary, destination):
    destination = Path(destination)
    stem, extension = destination.stem, destination.suffix
    for number in range(10000):
        target = destination if number == 0 else destination.with_name(f"{stem}_{number}{extension}")
        try:
            # Windows rename refuses to replace an existing destination.
            os.rename(temporary, target)
            return target
        except FileExistsError:
            continue
    raise FileExistsError("输出目录中重名文件过多")


def process(files, params, output_dir, progress=None, cancelled=None, checkpoint=None, thumbnail=None):
    if not files or len(files) > MAX_FILES:
        raise ValueError("请选择 1–500 个文件")
    output_dir = Path(output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    format_name = str(params.get("format", "WebP")).upper()
    if format_name not in ("WEBP", "JPEG", "PNG"):
        raise ValueError("仅支持 WebP、JPEG、PNG")
    quality = int(params.get("quality", 80))
    longest = int(params.get("longest_edge", 0))
    if not 1 <= quality <= 100 or not 0 <= longest <= 100000:
        raise ValueError("质量或最长边参数无效")
    rows, artifacts = [], []
    for index, source in enumerate(files):
        if cancelled and cancelled():
            return {"status":"cancelled","message":"已取消，已完成输出保留","columns":COLUMNS,"rows":rows,"artifacts":artifacts,"completed":len(rows),"total":len(files),"failed":len(rows)-len(artifacts)}
        path = Path(source).resolve()
        row = {"name":path.name,"source":str(path),"before":"","after":"","output":"","status":""}
        temporary = None
        try:
            if not path.is_file() or path.stat().st_size > MAX_BYTES:
                raise ValueError("文件缺失或超过 50 MiB")
            row["before"] = path.stat().st_size
            with warnings.catch_warnings():
                warnings.simplefilter("error", Image.DecompressionBombWarning)
                with Image.open(path) as original:
                    if original.format not in ("JPEG", "PNG", "WEBP") or getattr(original, "n_frames", 1) != 1:
                        raise ValueError("仅处理静态 JPEG/PNG/WebP")
                    if original.width * original.height > MAX_PIXELS:
                        raise ValueError("像素超过 4000 万")
                    image = ImageOps.exif_transpose(original)
                    if longest:
                        image.thumbnail((longest, longest), Image.Resampling.LANCZOS)
                    if thumbnail and index == 0:
                        preview = image.copy()
                        preview.thumbnail((256,256), Image.Resampling.LANCZOS)
                        preview.save(thumbnail, "PNG", exif=b"")
                    if format_name == "JPEG":
                        if image.mode in ("RGBA","LA") or "transparency" in image.info:
                            rgba = image.convert("RGBA")
                            background = Image.new("RGB", rgba.size, "white")
                            background.paste(rgba, mask=rgba.getchannel("A"))
                            image = background
                        else:
                            image = image.convert("RGB")
                    elif image.mode not in ("RGB","RGBA","L","LA","P"):
                        image = image.convert("RGBA")
                    suffix = {"WEBP":".webp","JPEG":".jpg","PNG":".png"}[format_name]
                    descriptor, temporary = tempfile.mkstemp(prefix=".modyn-", suffix=suffix, dir=output_dir)
                    os.close(descriptor)
                    options = {"optimize":True} if format_name == "PNG" else {"quality":quality}
                    options["exif"] = b""
                    image.save(temporary, format=format_name, **options)
            target = publish(temporary, output_dir / (path.stem + suffix))
            temporary = None
            row.update(after=target.stat().st_size, output=str(target), status="成功")
            artifacts.append(str(target))
        except Exception as error:
            row["status"] = "跳过 · " + str(error)[:180]
        finally:
            if temporary and Path(temporary).exists():
                Path(temporary).unlink()
        rows.append(row)
        partial = {"columns":COLUMNS,"rows":rows,"artifacts":artifacts,"completed":len(rows),"total":len(files),"failed":len(rows)-len(artifacts)}
        if checkpoint:
            checkpoint(partial)
        if progress:
            progress(index+1, len(files), path.name)
    succeeded = len(artifacts)
    return {"status":"succeeded" if succeeded else "failed",
            "message":f"成功 {succeeded}，跳过/失败 {len(files)-succeeded}",
            "columns":COLUMNS,"rows":rows,"artifacts":artifacts,"completed":len(rows),"total":len(files),"failed":len(rows)-len(artifacts)}
