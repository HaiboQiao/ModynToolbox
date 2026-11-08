"""ModynToolbox worker protocol v1. This module has no UI dependencies."""
import json
import os
from pathlib import Path
import sys
import threading

MAX_INPUT = 16 * 1024 * 1024
MAX_RESULT = 32 * 1024 * 1024


def atomic_json(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".writing")
    encoded = json.dumps(value, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    if len(encoded) > MAX_RESULT:
        raise ValueError("结果超过 32 MiB，请缩小处理范围")
    with temporary.open("wb") as stream:
        stream.write(encoded)
        stream.flush()
        os.fsync(stream.fileno())
    os.replace(temporary, path)


def read_line(limit):
    data = bytearray()
    while len(data) <= limit:
        byte = os.read(sys.stdin.fileno(), 1)
        if not byte or byte == b"\n":
            return bytes(data)
        data.extend(byte)
    raise ValueError("Control line exceeds limit")


class Cancelled(Exception):
    pass


class Context:
    def __init__(self, request):
        self.request = request
        self.job_root = Path(request["job_root"]).resolve()
        self.data_root = Path(request["data_root"]).resolve()
        self.stop = threading.Event()
        self.committing = False
        self.grant = threading.Event()
        self.reserved = False
        self.sequence = 0
        self.guard = threading.Lock()
        self.partial = {}

    def emit(self, kind, **values):
        with self.guard:
            self.sequence += 1
            message = {key: self.request[key] for key in
                       ("protocol", "tool_id", "session_id", "job_id", "revision")}
            message.update(kind=kind, seq=self.sequence, **values)
            encoded = json.dumps(message, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
            if len(encoded) > 65536:
                raise ValueError("控制消息超过 64 KiB")
            sys.stdout.buffer.write(encoded + b"\n")
            sys.stdout.buffer.flush()

    def cancelled(self):
        return self.stop.is_set() and not self.committing

    def check(self):
        if self.cancelled():
            raise Cancelled()

    def progress(self, completed=0, total=0, message="", **values):
        self.emit("progress", completed=completed, total=total,
                  message=message[:512], cancellable=not self.committing, **values)

    def reserve(self, paths):
        if self.reserved or self.committing:
            raise ValueError("资源只能在执行前整体预约一次")
        atomic_json(self.job_root / "resources.json", {"paths": list(dict.fromkeys(map(str, paths)))})
        self.emit("reserve", resources_ref="resources.json")
        while not self.grant.wait(0.05):
            self.check()
        self.reserved = True

    def critical(self):
        self.check()
        if self.request.get("dynamic_resources") and not self.reserved:
            raise ValueError("写入前尚未取得资源预约")
        self.committing = True
        self.progress(message="正在提交，等待完成", phase="commit")

    def checkpoint(self, result):
        self.partial = result
        atomic_json(self.job_root / "result.json", dict(result, status="running"))

    def receive_cancel(self):
        while True:
            line = read_line(65536)
            if not line or len(line) > 65536:
                return
            try:
                value = json.loads(line)
                if value.get("kind") == "cancel" and value.get("job_id") == self.request["job_id"]:
                    self.stop.set()
                elif value.get("kind") == "resources_granted" and value.get("job_id") == self.request["job_id"]:
                    self.grant.set()
            except (ValueError, UnicodeError):
                return


def serve(handler):
    line = read_line(MAX_INPUT)
    if not line or len(line) > MAX_INPUT:
        return 2
    request = json.loads(line)
    if request.get("protocol") != 1 or request.get("kind") != "run":
        return 2
    context = Context(request)
    context.job_root.mkdir(parents=True, exist_ok=True)
    context.data_root.mkdir(parents=True, exist_ok=True)
    context.emit("ready")
    threading.Thread(target=context.receive_cancel, daemon=True).start()
    try:
        reference = context.job_root / "input.json"
        if reference.stat().st_size > MAX_INPUT:
            raise ValueError("输入快照超过 16 MiB")
        data = json.loads(reference.read_text(encoding="utf-8"))
        context.check()
        result = handler(request["action"], data, context)
        result.setdefault("status", "succeeded")
    except Cancelled:
        result = dict(context.partial, status="cancelled", message="已取消，已完成输出保留")
    except Exception as error:
        result = dict(context.partial, status="failed", message=(str(error) or type(error).__name__)[:2048],
                      error_type=type(error).__name__)
    atomic_json(context.job_root / "result.json", result)
    context.emit("result", result_ref="result.json")
    return 0
