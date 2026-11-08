"""Windows no-replace, handle-locked rename transactions with recoverable journals."""
import ctypes
from ctypes import wintypes
from contextlib import ExitStack
import hashlib
import json
import os
from pathlib import Path
import re
import uuid
import msvcrt

MAX_FILES = 2000
COLUMNS = [{"key":"source","label":"原文件"},{"key":"target","label":"新位置"},{"key":"status","label":"状态"}]
RESERVED = re.compile(r"^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\.|$)", re.I)


def atomic(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    payload = json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")
    envelope = {"sha256":hashlib.sha256(payload).hexdigest(),"payload":value}
    temporary = path.with_suffix(".writing")
    with temporary.open("w", encoding="utf-8") as stream:
        json.dump(envelope, stream, ensure_ascii=False)
        stream.flush()
        os.fsync(stream.fileno())
    os.replace(temporary, path)


def load(path):
    envelope = json.loads(Path(path).read_text(encoding="utf-8"))
    payload = json.dumps(envelope["payload"], ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")
    if hashlib.sha256(payload).hexdigest() != envelope["sha256"]:
        raise ValueError("记录校验失败，保留原文件与记录")
    if envelope["payload"].get("schema",1) != 1:
        raise ValueError("工具数据版本过新，请使用兼容版本")
    return envelope["payload"]


def identity(path):
    stat = Path(path).stat()
    return {"device":stat.st_dev,"inode":stat.st_ino,"size":stat.st_size,"mtime":stat.st_mtime_ns}


def digest(path):
    value = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(chunk)
    return value.hexdigest()


def valid_name(name):
    return (bool(name) and name not in (".","..") and not name.endswith((" ","."))
            and not re.search(r'[<>:"/\\|?*\x00-\x1f]',name)
            and not RESERVED.match(name) and len(name.encode("utf-16-le")) <= 510)


def token(value):
    if not re.fullmatch(r"[0-9a-f-]{36}", str(value)):
        raise ValueError("记录标识无效")
    return value


def preview(files, params, data_root, progress=None, cancelled=None):
    if not files or len(files) > MAX_FILES:
        raise ValueError("请选择 1–2000 个普通文件")
    prefix, suffix = str(params.get("prefix","")), str(params.get("suffix",""))
    numbering = bool(params.get("numbering", False))
    start, digits = int(params.get("start",1)), int(params.get("digits",3))
    if not 0 <= start <= 1_000_000 or not 1 <= digits <= 12:
        raise ValueError("序号参数无效")
    entries, source_keys, identities = [], set(), set()
    for index, source in enumerate(files):
        if cancelled and cancelled():
            return {"status":"cancelled","message":"预览已取消"}
        path = Path(os.path.abspath(source))
        if path.is_symlink() or not path.is_file():
            raise ValueError("只接受普通文件，源文件缺失或为链接")
        source_key = os.path.normcase(str(path.resolve()))
        info = identity(path)
        unique = (info["device"],info["inode"])
        if source_key in source_keys or unique in identities:
            raise ValueError("输入包含重复文件或文件别名")
        identities.add(unique)
        source_keys.add(source_key)
        number = ("_" + str(start + index).zfill(digits)) if numbering else ""
        name = prefix + path.stem + suffix + number + path.suffix
        if not valid_name(name):
            raise ValueError("生成的文件名包含 Windows 非法名称或超过长度上限")
        target = path.with_name(name)
        info["sha256"] = digest(path)
        entries.append({"source":str(path),"target":str(target),"identity":info})
        if progress:
            progress(index+1,len(files),"生成并核验预览")
    targets = [os.path.normcase(item["target"]) for item in entries]
    if len(set(targets)) != len(targets):
        raise ValueError("多个文件的目标名称重复")
    for item, target in zip(entries, targets):
        if Path(item["target"]).exists() and target not in source_keys:
            raise ValueError("目标文件已存在，禁止覆盖 · " + item["target"])
    plan_id = str(uuid.uuid4())
    plan = {"schema":1,"plan_id":plan_id,"entries":entries,"consumed":False}
    path = Path(data_root)/"plans"/(plan_id+".json")
    atomic(path,plan)
    return {"completed":len(entries),"total":len(entries),"failed":0,"message":f"预览 {len(entries)} 个文件，尚未改名","columns":COLUMNS,
            "rows":[dict(source=e["source"],target=e["target"],status="可执行") for e in entries],
            "context":{"plan_id":plan_id,"plan_sha256":hashlib.sha256(path.read_bytes()).hexdigest(),"affected_count":len(entries)}}


class LockedFile:
    def __init__(self, path, expected):
        self.path = str(path)
        self.handle = None
        if os.name != "nt":
            raise OSError("事务重命名的首发平台为 Windows")
        self.kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        self.kernel.CreateFileW.argtypes = [wintypes.LPCWSTR,wintypes.DWORD,wintypes.DWORD,ctypes.c_void_p,
                                            wintypes.DWORD,wintypes.DWORD,wintypes.HANDLE]
        self.kernel.CreateFileW.restype = wintypes.HANDLE
        self.kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        self.kernel.SetFileInformationByHandle.argtypes = [wintypes.HANDLE,ctypes.c_int,ctypes.c_void_p,wintypes.DWORD]
        self.handle = self.kernel.CreateFileW(self.path,0x80000000|0x10000,1,None,3,0x80,None)
        if self.handle == ctypes.c_void_p(-1).value:
            self.handle = None
            raise ctypes.WinError(ctypes.get_last_error())
        try:
            self.kernel.GetCurrentProcess.restype = wintypes.HANDLE
            self.kernel.DuplicateHandle.argtypes = [wintypes.HANDLE,wintypes.HANDLE,wintypes.HANDLE,ctypes.POINTER(wintypes.HANDLE),wintypes.DWORD,wintypes.BOOL,wintypes.DWORD]
            process = self.kernel.GetCurrentProcess()
            duplicate = wintypes.HANDLE()
            if not self.kernel.DuplicateHandle(process,self.handle,process,ctypes.byref(duplicate),0,False,2):
                raise ctypes.WinError(ctypes.get_last_error())
            descriptor = msvcrt.open_osfhandle(duplicate.value,os.O_RDONLY|os.O_BINARY)
            with os.fdopen(descriptor,"rb") as stream:
                stat = os.fstat(stream.fileno())
                current = {"device":stat.st_dev,"inode":stat.st_ino,"size":stat.st_size,"mtime":stat.st_mtime_ns}
                value = hashlib.sha256()
                for chunk in iter(lambda: stream.read(1024*1024),b""):
                    value.update(chunk)
            if any(current[key] != expected[key] for key in ("device","inode","size","mtime")) or value.hexdigest() != expected["sha256"]:
                raise ValueError("源文件已变化，请重新预览")
        except BaseException:
            self.close()
            raise

    def close(self):
        if self.handle is not None:
            self.kernel.CloseHandle(self.handle)
            self.handle = None

    def __enter__(self):
        return self

    def __exit__(self, *arguments):
        self.close()

    def move(self, target):
        if self.path == os.path.abspath(target):
            return
        class RenameInfo(ctypes.Structure):
            _fields_ = [("Flags",wintypes.DWORD),("RootDirectory",wintypes.HANDLE),
                        ("FileNameLength",wintypes.DWORD),("FileName",wintypes.WCHAR*1)]
        name = os.path.abspath(target).encode("utf-16-le")
        buffer = ctypes.create_string_buffer(max(ctypes.sizeof(RenameInfo),RenameInfo.FileName.offset+len(name)+2))
        info = RenameInfo.from_buffer(buffer)
        info.Flags = 0
        info.RootDirectory = None
        info.FileNameLength = len(name)
        ctypes.memmove(ctypes.addressof(buffer)+RenameInfo.FileName.offset,name,len(name))
        if not self.kernel.SetFileInformationByHandle(self.handle,3,buffer,len(buffer)):
            raise ctypes.WinError(ctypes.get_last_error())
        self.path = os.path.abspath(target)


def transaction(entries, data_root, critical=None, cancelled=None, kind="rename", parent_id=None):
    transaction_id = str(uuid.uuid4())
    journal_path = Path(data_root)/"journals"/(transaction_id+".json")
    journal = {"schema":1,"id":transaction_id,"kind":kind,"status":"prepared","entries":[],"parent_id":parent_id}
    with ExitStack() as stack:
        locks = []
        for entry in entries:
            if cancelled and cancelled():
                return {"status":"cancelled","message":"提交前已取消"}
            locks.append(stack.enter_context(LockedFile(entry["source"],entry["identity"])))
            item = dict(entry, current=entry["source"],
                        temporary=str(Path(entry["source"]).with_name(".modyn-"+transaction_id+"-"+str(len(locks)))))
            journal["entries"].append(item)
        if critical:
            critical()
        journal["status"] = "committing"
        atomic(journal_path,journal)
        try:
            for phase in ("temporary","target"):
                for lock, item in zip(locks,journal["entries"]):
                    journal["intent"] = {"from":item["current"],"to":item[phase]}
                    lock.move(item[phase])
                    item["current"] = item[phase]
            journal["status"] = "completed"
            journal.pop("intent",None)
            atomic(journal_path,journal)
            if kind == "rename":
                atomic(Path(data_root)/"latest.json",{"journal":transaction_id})
        except BaseException as error:
            failures = []
            # Stage every moved file before restoring originals, including swap/case-only batches.
            for lock, item in zip(locks,journal["entries"]):
                try:
                    if os.path.normcase(lock.path) != os.path.normcase(item["temporary"]):
                        lock.move(item["temporary"])
                    item["current"] = lock.path
                except OSError as rollback_error:
                    failures.append(str(rollback_error))
            for lock, item in zip(locks,journal["entries"]):
                try:
                    lock.move(item["source"])
                    item["current"] = lock.path
                except OSError as rollback_error:
                    failures.append(str(rollback_error))
            unrestored = any(lock.path != item["source"] for lock,item in zip(locks,journal["entries"]))
            journal["status"] = "recovery_required" if unrestored else "rolled_back"
            journal["error"] = str(error)[:1000]
            atomic(journal_path,journal)
            return {"status":"failed","completed":len(entries),"total":len(entries),"failed":len(entries),"message":"改名失败 · "+journal["status"],"columns":COLUMNS,
                    "rows":[{"source":i["source"],"target":i["current"],"status":journal["status"]} for i in journal["entries"]],
                    "artifacts":[str(journal_path)],"context":{}}
    return {"completed":len(entries),"total":len(entries),"failed":0,"message":f"完成 {len(entries)} 个文件 · {kind}","columns":COLUMNS,
            "rows":[dict(source=e["source"],target=e["target"],status="成功") for e in entries],
            "context":{},"artifacts":[str(journal_path)]}


def apply(plan_id, plan_sha256, data_root, critical=None, cancelled=None):
    path = Path(data_root)/"plans"/(token(plan_id)+".json")
    if hashlib.sha256(path.read_bytes()).hexdigest() != plan_sha256:
        raise ValueError("计划内容已变化，请重新预览")
    plan = load(path)
    if plan["consumed"]:
        raise ValueError("计划已执行，请重新预览")
    plan["consumed"] = True
    atomic(path,plan)
    return transaction(plan["entries"],data_root,critical,cancelled)


def undo(data_root, critical=None, cancelled=None):
    latest = load(Path(data_root)/"latest.json")
    path = Path(data_root)/"journals"/(token(latest["journal"])+".json")
    journal = load(path)
    if journal["status"] != "completed":
        raise ValueError("最近批次已撤销或需要恢复")
    entries = [dict(source=e["target"],target=e["source"],identity=e["identity"]) for e in journal["entries"]]
    journal["status"] = "undoing"
    atomic(path,journal)
    try:
        result = transaction(entries,data_root,critical,cancelled,"undo",journal["id"])
    except BaseException:
        journal["status"] = "completed"
        atomic(path,journal)
        raise
    if result.get("status","succeeded") == "succeeded":
        journal["status"] = "undone"
        atomic(path,journal)
        # A second click must not silently reapply the batch.
        atomic(Path(data_root)/"latest.json",{"journal":journal["id"]})
    else:
        journal["status"] = "completed"
        atomic(path,journal)
    return result


def recover(data_root, critical=None, cancelled=None):
    pending = []
    for path in list((Path(data_root)/"journals").glob("*.json")):
        journal = load(path)
        if journal["status"] not in ("committing","recovery_required","prepared"):
            continue
        entries = []
        for item in journal["entries"]:
            candidates = {os.path.normcase(p):p for p in [item.get("current"),item["temporary"],item["target"],item["source"]] if p}.values()
            matches = [p for p in candidates if p and Path(p).is_file()
                       and identity(p)["inode"] == item["identity"]["inode"]
                       and identity(p)["device"] == item["identity"]["device"]]
            if len(matches) != 1:
                raise ValueError("无法唯一定位待恢复文件，请检查 journal 中的位置")
            entries.append(dict(source=matches[0],target=item["source"],identity=item["identity"]))
        result = transaction(entries,data_root,critical,cancelled,"recover")
        if result.get("status","succeeded") != "succeeded":
            return result
        journal["status"] = "recovered"
        atomic(path,journal)
        pending.extend(result["rows"])
    journals = [load(p) for p in (Path(data_root)/"journals").glob("*.json")]
    for journal in journals:
        if journal["status"] == "undoing":
            completed = any(j.get("parent_id") == journal["id"] and j["status"] == "completed" for j in journals)
            journal["status"] = "undone" if completed else "completed"
            atomic(Path(data_root)/"journals"/(journal["id"]+".json"),journal)
    return {"completed":len(pending),"total":len(pending),"failed":0,"message":f"恢复 {len(pending)} 个文件","columns":COLUMNS,"rows":pending,"context":{}}


def claims(action, context, data_root):
    journals = []
    if action == "apply":
        journals = [load(Path(data_root)/"plans"/(token(context.get("plan_id"))+".json"))]
    elif action == "undo":
        latest = load(Path(data_root)/"latest.json")
        journals = [load(Path(data_root)/"journals"/(token(latest["journal"])+".json"))]
    elif action == "recover":
        journals = [load(path) for path in (Path(data_root)/"journals").glob("*.json")]
        journals = [j for j in journals if j["status"] in ("committing","prepared","recovery_required")]
    paths = []
    for journal in journals:
        for entry in journal["entries"]:
            paths.extend([entry["source"],entry["target"]])
            paths.extend([entry[key] for key in ("temporary","current") if key in entry])
    return list(dict.fromkeys(paths))
