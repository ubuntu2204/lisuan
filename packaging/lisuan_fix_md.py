#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""lisuan_fix_md — 轻量 Markdown → Tkinter Text 渲染（说明弹窗用）。

支持（覆盖本项目全部说明文档用到的语法）：
    # / ## / ###          标题（分级着色、加粗）
    ``` 代码块 ```        等宽深色块
    | a | b |             表格（等宽原样呈现，跳过 |---| 分隔行）
    - / * / 1.            列表（缩进 + 项目符号）
    > 引用                缩进斜体灰
    **粗体**              加粗
    `行内代码`            等宽浅底
    [文本](url)           链接样式（蓝色下划线）
    ---                   水平线

不依赖第三方库；只操作传入的 tk.Text 实例，便于脱离 GUI 单测。
"""

from __future__ import annotations

import re

_INLINE = re.compile(r"(\*\*[^*\n]+?\*\*|`[^`\n]+?`|\[[^\]\n]+\]\([^)\s]+\))")
_HR = re.compile(r"^-{3,}$")
_TABLE_SEP = re.compile(r"^\|[\s:|-]+\|$")


def render(widget, src: str, *, ui_font, mono_font, accent) -> None:
    """把 markdown 源码 src 渲染进 tk.Text（替换原内容）。

    ui_font / mono_font: ("字体名", 字号) 二元组；accent: 标题色。
    """
    w = widget
    name, size = ui_font
    mname, msize = mono_font

    w.tag_configure("h1", font=(name, size + 5, "bold"), foreground=accent,
                    spacing1=16, spacing3=8)
    w.tag_configure("h2", font=(name, size + 2, "bold"), foreground=accent,
                    spacing1=12, spacing3=6)
    w.tag_configure("h3", font=(name, size + 1, "bold"), spacing1=10, spacing3=4)
    w.tag_configure("bold", font=(name, size, "bold"))
    w.tag_configure("code", font=(mname, msize - 1),
                    background="#eceff1", foreground="#263238")
    w.tag_configure("codeblock", font=(mname, msize - 1),
                    background="#263238", foreground="#eceff1",
                    lmargin1=14, lmargin2=14, spacing1=2, wrap="none")
    w.tag_configure("li", lmargin1=24, lmargin2=16, spacing1=2)
    w.tag_configure("quote", lmargin1=20, lmargin2=20, foreground="#5f6b76",
                    font=(name, size, "italic"))
    w.tag_configure("link", foreground="#1565c0", underline=True)
    w.tag_configure("table", font=(mname, msize - 1), lmargin1=12, lmargin2=12)
    w.tag_configure("hr", foreground="#b0bec5", justify="center",
                    spacing1=10, spacing3=10)

    w.delete("1.0", "end")
    lines = src.splitlines()
    i, n = 0, len(lines)
    while i < n:
        s = lines[i].strip()

        # 代码块（状态机）
        if s.startswith("```"):
            i += 1
            while i < n and not lines[i].strip().startswith("```"):
                w.insert("end", lines[i] + "\n", ("codeblock",))
                i += 1
            i += 1  # 跳过收尾 ```
            continue

        if s.startswith("### "):
            w.insert("end", s[4:] + "\n", ("h3",))
        elif s.startswith("## "):
            w.insert("end", s[3:] + "\n", ("h2",))
        elif s.startswith("# "):
            w.insert("end", s[2:] + "\n", ("h1",))
        elif _HR.match(s):
            w.insert("end", "─" * 56 + "\n", ("hr",))
        elif s.startswith("> "):
            _inline(w, s[2:], ("quote",))
            w.insert("end", "\n", ("quote",))
        elif s.startswith("|"):
            if not _TABLE_SEP.match(s):     # 跳过 |---|---| 分隔行
                w.insert("end", s + "\n", ("table",))
        elif re.match(r"^\d+\. ", s):
            m = re.match(r"^(\d+)\. (.*)$", s)
            w.insert("end", f"{m.group(1)}. ", ("li",))
            _inline(w, m.group(2), ("li",))
            w.insert("end", "\n", ("li",))
        elif s.startswith("- ") or s.startswith("* "):
            w.insert("end", "• ", ("li",))
            _inline(w, s[2:], ("li",))
            w.insert("end", "\n", ("li",))
        elif s == "":
            w.insert("end", "\n")
        else:
            _inline(w, lines[i])
            w.insert("end", "\n")
        i += 1


def _inline(w, text: str, base: tuple = ()) -> None:
    """处理行内标记：**粗体** / `行内代码` / [链接](url)。"""
    for part in _INLINE.split(text):
        if not part:
            continue
        if part.startswith("**") and part.endswith("**") and len(part) > 4:
            w.insert("end", part[2:-2], (*base, "bold"))
        elif part.startswith("`") and part.endswith("`") and len(part) > 2:
            w.insert("end", part[1:-1], (*base, "code"))
        elif part.startswith("[") and "](" in part:
            m = re.match(r"^\[([^\]]+)\]\(([^)\s]+)\)$", part)
            if m:
                w.insert("end", m.group(1), (*base, "link"))
                continue
            w.insert("end", part, base)
        else:
            w.insert("end", part, base)
