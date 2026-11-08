from pathlib import Path
import importlib.util
import json,os,sys,tempfile,time
ROOT=Path(__file__).resolve().parents[1]
sys.stdout.reconfigure(encoding="utf-8")
records=[]


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value

i = module("image_tool",ROOT/"core/image_tool.py")

from PIL import Image

def check(name, action):
    started = time.perf_counter()
    action()
    records.append({"name":name,"passed":True,"ms":round((time.perf_counter()-started)*1000,2)})

def rejects(action):
    try:
        action()
    except (ValueError, OSError):
        return
    raise AssertionError("Expected rejection")

def image_safety():
    with tempfile.TemporaryDirectory(dir=ROOT/".local/testdata") as folder:
        base=Path(folder);source=base/"transparent.png";out=base/"out"
        Image.new("RGBA",(32,16),(255,0,0,0)).save(source)
        original=source.read_bytes()
        first=i.process([str(source)],{"format":"JPEG","quality":95,"longest_edge":1000},out)
        target=Path(first["artifacts"][0])
        with Image.open(target) as image:
            assert image.size==(32,16) and image.getpixel((0,0))[0]>240 and image.getpixel((0,0))[1]>240
            assert not image.getexif()
        second=i.process([str(source)],{"format":"JPEG"},out);assert second["artifacts"][0]!=str(target)
        assert source.read_bytes()==original
        bad=base/"bad.png";bad.write_text("broken")
        partial=i.process([str(source),str(bad)],{"format":"WebP"},out)
        assert len(partial["artifacts"])==1 and len(partial["rows"])==2
        assert i.process([str(source)],{},out,cancelled=lambda:True)["status"]=="cancelled"
        rejects(lambda:i.process([],{},out))

def image_orientation():
    with tempfile.TemporaryDirectory(dir=ROOT/".local/testdata") as folder:
        base=Path(folder);source=base/"rotate.jpg";exif=Image.Exif();exif[274]=6
        Image.new("RGB",(30,10),"red").save(source,exif=exif)
        result=i.process([str(source)],{"format":"PNG","longest_edge":15},base/"out")
        with Image.open(result["artifacts"][0]) as image:assert image.size==(5,15) and not image.getexif()

def image_exif_removed():
    with tempfile.TemporaryDirectory(dir=ROOT/".local/testdata") as folder:
        base=Path(folder);source=base/"source.jpg";exif=Image.Exif();exif[271]="Test Camera";exif[274]=6
        Image.new("RGB",(30,10),"red").save(source,exif=exif)
        for name in ("PNG","JPEG","WebP"):
            result=i.process([str(source)],{"format":name},base/name)
            with Image.open(result["artifacts"][0]) as output:assert not output.getexif()

def image_capacity_boundaries():
    with tempfile.TemporaryDirectory(dir=ROOT/".local/testdata") as folder:
        base = Path(folder)
        source = base / "small.png"
        Image.new("RGB", (2, 2), "white").save(source)
        sample = source.read_bytes()
        files = []
        for number in range(501):
            path = base / (str(number) + ".png")
            path.write_bytes(sample)
            files.append(str(path))
        result = i.process(files[:500], {"format": "PNG"}, base / "batch")
        assert len(result["artifacts"]) == 500 and len(result["rows"]) == 500
        rejects(lambda: i.process(files, {}, base / "batch"))
        with source.open("ab") as stream:
            stream.truncate(i.MAX_BYTES)
        assert i.process([str(source)], {}, base / "size-exact")["status"] == "succeeded"
        with source.open("ab") as stream:
            stream.write(b"x")
        assert i.process([str(source)], {}, base / "size-over")["status"] == "failed"
        for dimensions, expected in (((8000, 5000), "succeeded"), ((8001, 5000), "failed")):
            path = base / (str(dimensions[0]) + "-pixels.png")
            with Image.new("L", dimensions, 0) as image:
                image.save(path)
            result = i.process([str(path)], {"longest_edge": 64}, base / "pixels")
            assert result["status"] == expected, result

(ROOT/".local/testdata").mkdir(parents=True,exist_ok=True)

check("image_safety",image_safety)

check("image_orientation",image_orientation)



check("image_exif_removed",image_exif_removed)

check("image_capacity_boundaries", image_capacity_boundaries)
print(json.dumps({"tool":"image","passed":len(records),"tests":records}))
