"""Export integration-only SDK files; the host app source/toolchain is not required by the SDK."""
from pathlib import Path
import argparse,shutil,json
ROOT=Path(__file__).resolve().parents[1]
def export(target):
    target=Path(target).resolve()
    if target.exists():raise FileExistsError('Choose a fresh SDK output directory')
    (target/'scripts').mkdir(parents=True)
    for name in ('build-tool.py','runtime.py','runtime.lock.json'):shutil.copy2(ROOT/'scripts'/name,target/'scripts'/name)
    for name in ('sdk','frontend-sdk','protocol'):shutil.copytree(ROOT/'shared'/name,target/'shared'/name)
    shutil.copytree(ROOT/'licenses',target/'licenses')
    (target/'docs').mkdir()
    guide=(ROOT/'docs/independent-frontend.md').read_text(encoding='utf-8')
    import re
    version=re.search(r'project\(ModynToolbox\s+VERSION\s+([0-9.]+)',(ROOT/'CMakeLists.txt').read_text()).group(1)
    guide=re.sub(r'\]\(([^):]+\.md(?:#[^)]*)?)\)',lambda m:'](https://github.com/HaiboQiao/ModynToolbox/blob/v'+version+'/source/docs/'+m.group(1)+')',guide)
    (target/'docs/independent-frontend.md').write_text(guide,encoding='utf-8')
    shutil.copytree(ROOT/'examples/independent-frontends',target/'examples',ignore=shutil.ignore_patterns('.local','__pycache__','*.pyc'))
    (target/'README.md').write_text(f"""# ModynToolbox Integration SDK {version}

本SDK只含工具接入协议、通信桥、构建脚本和示例，不含app宿主源码/Qt工具链/用户数据。工具作者自行设计完整页面和独立后端。

- 完整接入规范：docs/independent-frontend.md；协议：shared/protocol；纯通信JS：shared/frontend-sdk/modyn.js；Python/C++后端SDK：shared/sdk。
- examples含Python统计（HTML）和C++字节计数（Vue）两种独立示例，框架仅示例，不强制布局。示例依赖/许可由各例自己的文件说明。
- 构建Python工具：python scripts/build-tool.py <工具源目录> --output <新的输出目录>。首次按scripts/runtime.lock.json及runtime.py锁定值恢复私有运行时，需联网或预备下载缓存；生产完整包无需系统Python。
- 原生后端使用各自构建入口，所需编译器/QtCore由作者准备。完成后把完整包复制到宿主tools/<id>/<version>；推荐先点号暂存，完成再更名，自动发现/刷新即可。
- frontend工具要求宿主>=1.4.0，清单/桥版本独立。不要覆盖同版本、操作宿主数据库或强制转移未知旧会话状态。任务、会话和显示窗口分离；提供完整检查点，不在恢复时自动重提作业。
- 新工具使用已有桥能力无需修改app。将所有静态前端资源及私有后端依赖打包，不依赖CDN或开发服务器；依赖许可随工具分发。
""",encoding='utf-8')
    return target
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('output',type=Path);a=p.parse_args();print(export(a.output))
