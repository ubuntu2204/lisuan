#!/usr/bin/env python3
"""捕获 X11 根窗口或指定窗口，保存为 PNG，并检查窗口映射状态。"""

import argparse
import json
import struct
import subprocess
import sys
import time
from pathlib import Path

from PIL import Image
from Xlib import X, display


def iter_windows(window):
    yield window
    try:
        children = window.query_tree().children
    except Exception:
        return
    for child in children:
        yield from iter_windows(child)


def window_title(window):
    try:
        name = window.get_wm_name()
        if isinstance(name, bytes):
            name = name.decode(errors="replace")
        if name:
            return name
    except Exception:
        pass
    try:
        atom = window.display.get_atom("_NET_WM_NAME")
        prop = window.get_full_property(atom, X.AnyPropertyType)
        if prop and prop.value:
            if isinstance(prop.value, bytes):
                return prop.value.decode("utf-8", errors="replace")
            return str(prop.value)
    except Exception:
        pass
    return ""


def is_viewable(window):
    try:
        return window.get_attributes().map_state == X.IsViewable
    except Exception:
        return False


def find_window(root, title, timeout):
    deadline = time.monotonic() + timeout
    while True:
        matches = []
        for window in iter_windows(root):
            if title in window_title(window) and is_viewable(window):
                matches.append(window)
        if matches:
            exact = [window for window in matches if window_title(window) == title]
            return (exact or matches)[0]
        if time.monotonic() >= deadline:
            return None
        time.sleep(0.5)


def decode_xwd(raw, output):
    header = struct.unpack(">25I", raw[:100])
    header_size, width, height = header[0], header[4], header[5]
    byte_order, bytes_per_line, color_count = header[7], header[12], header[19]
    pixels = raw[header_size + color_count * 12:]
    decoder = "BGRA" if byte_order == 0 else "ARGB"
    image = Image.frombytes(
        "RGBA", (width, height), pixels, "raw", decoder, bytes_per_line,
    ).convert("RGB")
    output.parent.mkdir(parents=True, exist_ok=True)
    image.save(output)
    return width, height


def capture(drawable, output):
    raw = subprocess.check_output(["xwd", "-silent", "-id", f"{drawable.id:#x}"])
    return decode_xwd(raw, output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    target = parser.add_mutually_exclusive_group(required=True)
    target.add_argument("--title", help="窗口标题，支持子串匹配")
    target.add_argument("--id", type=lambda value: int(value, 0), help="X11 window id")
    target.add_argument("--root", action="store_true", help="捕获根窗口")
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--timeout", type=float, default=10.0)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    try:
        xdisplay = display.Display()
    except Exception as exc:
        print(f"无法连接 X11: {exc}", file=sys.stderr)
        return 2
    root = xdisplay.screen().root

    if args.root:
        window = root
    elif args.id is not None:
        window = xdisplay.create_resource_object("window", args.id)
        if not is_viewable(window):
            print(f"窗口不可见或未映射: {args.id:#x}", file=sys.stderr)
            return 1
    else:
        window = find_window(root, args.title, args.timeout)
        if window is None:
            print(f"超时未找到可见窗口: {args.title}", file=sys.stderr)
            return 1

    try:
        size = capture(window, args.output)
    except Exception as exc:
        print(f"截图失败: {exc}", file=sys.stderr)
        return 1

    payload = {
        "ok": True,
        "window_id": f"{window.id:#x}",
        "title": window_title(window),
        "width": size[0],
        "height": size[1],
        "output": str(args.output.resolve()),
    }
    if args.json:
        print(json.dumps(payload, ensure_ascii=False, indent=2))
    else:
        print(f"已保存 {payload['output']} ({size[0]}x{size[1]})")
        print(f"窗口: {payload['window_id']} {payload['title']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
