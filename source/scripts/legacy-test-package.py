"""Private test-only legacy view fixture; never changes installed or published tool packages."""
from pathlib import Path
import json,shutil,hashlib

def legacy_package(package, destination, source_root):
    package=Path(package);destination=Path(destination)
    manifest=json.loads((package/'manifest.json').read_text(encoding='utf-8'))
    if not manifest.get('frontend'):return package
    shutil.copytree(package,destination)
    ui=Path(source_root)/'tools'/manifest['tool_id']/'integration/ui.json'
    shutil.copy2(ui,destination/'ui.json')
    view=json.loads(ui.read_text(encoding='utf-8'))
    manifest.pop('frontend',None);manifest['manifest_version']=1;manifest['ui_schema_version']=view['view_version'];manifest['view_file']='ui.json'
    checks=json.loads((destination/'checksums.json').read_text(encoding='utf-8'));checks['ui.json']=hashlib.sha256((destination/'ui.json').read_bytes()).hexdigest()
    (destination/'checksums.json').write_text(json.dumps(checks,indent=2)+'\n',encoding='utf-8');manifest['integrity_sha256']=hashlib.sha256((destination/'checksums.json').read_bytes()).hexdigest()
    (destination/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    return destination
