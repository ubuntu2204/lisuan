#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""patch_launch_options.py — 在 Steam 的 localconfig.vdf 里设置/清除某 AppID 的
"LaunchOptions"（仅文本级插入，不改动其它内容，写入前做括号平衡自检）。

Steam 运行时会把 localconfig.vdf 全量回写，所以调用方必须先确认 Steam 已退出。
本脚本只负责"给定文件、给定值"的最小改写，便于离线测试（tests/test-plants-vs-zombies.sh）。

用法：
    patch_launch_options.py <localconfig.vdf> <appid> --set "<启动选项>"
    patch_launch_options.py <localconfig.vdf> <appid> --clear
    patch_launch_options.py <localconfig.vdf> <appid> --print

退出码：0 成功 / 1 用法或结构错误 / 2 找不到 apps 或 AppID 记录

License: Apache-2.0
"""
from __future__ import annotations

import re
import sys


def block_span(text: str, brace_idx: int) -> tuple[int, int] | tuple[None, None]:
    """从 text[brace_idx] 处的 '{' 开始做括号配对，返回 (open_idx, close_idx)。"""
    depth = 0
    i = brace_idx
    in_str = False
    while i < len(text):
        c = text[i]
        if in_str:
            if c == "\\":
                i += 2
                continue
            if c == '"':
                in_str = False
        elif c == '"':
            in_str = True
        elif c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return brace_idx, i
        i += 1
    return None, None


def find_apps_block(text: str) -> tuple[int, int] | None:
    """定位 UserLocalConfigStore/.../Steam/apps 中记录游玩信息的那一块。

    该块的特征是子项里带 "LastPlayed"（首次启动游戏后才会生成）。
    """
    fallback = None
    for m in re.finditer(r'"apps"\s*\{', text):
        o, c = block_span(text, text.index("{", m.end() - 1))
        if o is None:
            continue
        if '"LastPlayed"' in text[o:c]:
            return o, c
        if fallback is None:
            fallback = (o, c)
    return fallback


def find_app_block(apps_block: str, appid: str) -> tuple[int, int] | None:
    m = re.search(r'"%s"\s*\{' % re.escape(appid), apps_block)
    if not m:
        return None
    return block_span(apps_block, apps_block.index("{", m.end() - 1))


def main(argv: list[str]) -> int:
    if len(argv) < 4:
        print(__doc__, file=sys.stderr)
        return 1
    path, appid, action = argv[1], argv[2], argv[3]
    value = argv[4] if len(argv) > 4 else ""

    with open(path, encoding="utf-8", errors="surrogateescape") as fh:
        text = fh.read()

    apps = find_apps_block(text)
    if not apps:
        print("找不到 localconfig.vdf 里的 apps 块", file=sys.stderr)
        return 2
    ao, ac = apps
    apps_block = text[ao:ac]

    app = find_app_block(apps_block, appid)
    if not app:
        print(f"apps 块里没有 AppID {appid} 的记录（先在 Steam 里启动一次该游戏）", file=sys.stderr)
        return 2
    bo, bc = app
    app_block = apps_block[bo:bc]

    current = re.search(r'"LaunchOptions"[ \t]*"((?:[^"\\]|\\.)*)"', app_block)

    if action == "--print":
        # 写盘时做了 VDF 转义（\\ → \\\\、" → \\"），这里还原成可读文本
        raw = current.group(1) if current else ""
        print(raw.replace('\\"', '"').replace("\\\\", "\\"))
        return 0

    # 整行区间（含行首缩进与行尾换行），替换/删除都以行为单位，避免留下空行
    line_start = app_block.rfind("\n", 0, current.start()) + 1 if current else 0
    line_end = app_block.find("\n", current.end()) + 1 if current else 0

    if action == "--set":
        escaped = value.replace("\\", "\\\\").replace('"', '\\"')
        new_line = '\t\t\t\t\t\t"LaunchOptions"\t\t"%s"\n' % escaped
        if current:
            new_block = app_block[:line_start] + new_line + app_block[line_end:]
        else:
            # 新插入时去掉行尾换行：块原本的 '{' 后自带换行，避免多出一行空行
            new_block = app_block.replace("{", "{\n" + new_line.rstrip("\n"), 1)
    elif action == "--clear":
        if not current:
            print("没有 LaunchOptions，无需清除")
            return 0
        new_block = app_block[:line_start] + app_block[line_end:]
    else:
        print(f"未知动作：{action}", file=sys.stderr)
        return 1

    new_text = text[:ao] + apps_block.replace(app_block, new_block, 1) + text[ac:]
    if new_text.count("{") != new_text.count("}"):
        print("改写后括号不平衡，放弃写入", file=sys.stderr)
        return 1

    with open(path, "w", encoding="utf-8", errors="surrogateescape") as fh:
        fh.write(new_text)

    if action == "--set":
        print(f"已写入 AppID {appid} 的 LaunchOptions")
    else:
        print(f"已清除 AppID {appid} 的 LaunchOptions")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
