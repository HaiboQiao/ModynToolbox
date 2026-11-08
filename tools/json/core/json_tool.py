"""Strict JSON validation and whitespace-only formatting, independent of the host."""
import json
import re

MAX_BYTES = 5 * 1024 * 1024
MAX_DEPTH = 128
MAX_OUTPUT = 20 * 1024 * 1024
TOKEN = re.compile(r'"[^"\\]*(?:\\.[^"\\]*)*"|-?(?:0|[1-9][0-9]*)(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?|true|false|null|[{}\[\],:]')


def tokenize(text):
    tokens = []
    position = 0
    depth = 0
    for match in TOKEN.finditer(text):
        if text[position:match.start()].strip(" \t\r\n"):
            raise json.JSONDecodeError("非法 JSON 标记", text, position)
        token = match.group()
        if token in ("{", "["):
            depth += 1
            if depth > MAX_DEPTH:
                raise json.JSONDecodeError("嵌套超过 128 层", text, match.start())
        elif token in ("}", "]"):
            depth -= 1
        tokens.append(token)
        position = match.end()
    if text[position:].strip(" \t\r\n"):
        raise json.JSONDecodeError("非法 JSON 标记", text, position)
    return tokens


def process(text, formatting=True):
    if not isinstance(text, str) or len(text.encode("utf-8")) > MAX_BYTES:
        raise ValueError("JSON 输入最多 5 MiB（UTF-8）")
    if text.startswith("\ufeff"):
        text = text[1:]
    duplicates = []
    def pairs(values):
        seen = set()
        for key, value in values:
            if key in seen:
                duplicates.append(key)
            seen.add(key)
        return values
    try:
        tokens = tokenize(text)
        json.loads(text, parse_int=str, parse_float=str,
                   parse_constant=lambda value: (_ for _ in ()).throw(ValueError("禁止 NaN/Infinity")),
                   object_pairs_hook=pairs)
    except json.JSONDecodeError as error:
        return {"status": "failed", "message": f"第 {error.lineno} 行，第 {error.colno} 列 · {error.msg}",
                "line": error.lineno, "column": error.colno,"completed":1,"total":1,"failed":1}
    except (ValueError, RecursionError) as error:
        return {"status": "failed", "message": str(error),"completed":1,"total":1,"failed":1}
    output = text
    if formatting:
        fragments = []
        depth = 0
        length = 0
        for index, token in enumerate(tokens):
            previous = tokens[index - 1] if index else ""
            following = tokens[index + 1] if index + 1 < len(tokens) else ""
            if token in ("{", "["):
                depth += 1
                piece = token + ("" if following in ("}", "]") else "\n" + "  " * depth)
            elif token in ("}", "]"):
                depth -= 1
                piece = ("" if previous in ("{", "[") else "\n" + "  " * depth) + token
            elif token == ",":
                piece = ",\n" + "  " * depth
            elif token == ":":
                piece = ": "
            else:
                piece = token
            length += len(piece.encode("utf-8"))
            if length > MAX_OUTPUT:
                raise ValueError("格式化结果超过 20 MiB")
            fragments.append(piece)
        output = "".join(fragments)
    warning = "重复键 " + "、".join(dict.fromkeys(duplicates))[:500] if duplicates else ""
    return {"status": "succeeded", "message": "JSON 有效" + (f" · 检测到 {len(duplicates)} 个重复键，保留原始键值" if warning else ""),
            "text": output, "warnings": [warning] if warning else [],"completed":1,"total":1,"failed":0}
