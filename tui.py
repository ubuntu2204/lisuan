#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Textual TUI 界面（可选增强）。

仅当已安装 textual 且 stdin 是终端时被 lisuan_fix.py 加载。
未安装时主程序自动降级为纯文本菜单，不影响使用。

布局：
    左侧  环境检测结果（按 ok/bad/warn 着色）
    右侧  菜单项（按分组，回车执行）
    底部  最近一次命令的输出
"""

# Textual 的 write/append/clear/run_worker 返回 awaitable，惯例是直接忽略；
# CSS/BINDINGS/title 等属性按 Textual 约定直接赋值。这两类写法会触发
# basedpyright 的严格规则，故仅对本文件放宽。
# pyright: reportUnusedCallResult=false, reportUnannotatedClassAttribute=false

from __future__ import annotations

import os
import subprocess
from typing import TYPE_CHECKING, TextIO, cast, override

from textual.app import App, ComposeResult
from textual.containers import Horizontal
from textual.widgets import Footer, Header, Label, ListItem, ListView, RichLog, Static

from lisuan_fix import GROUP_ORDER, ITEMS, ITEMS_BY_ID, detect, item_label

if TYPE_CHECKING:  # Item 只用于类型注解，运行时不需要
    from lisuan_fix import Item

LEVEL_STYLE = {"ok": "green", "bad": "red", "warn": "yellow", "info": "blue"}
LEVEL_MARK = {"ok": "✓", "bad": "✗", "warn": "!", "info": "·"}


class EnvPanel(Static):
    """左侧环境检测面板。"""

    def refresh_env(self) -> None:
        env = detect()
        lines = [f"[b]内核[/b] {env.kernel}", ""]
        last_cat = None
        for c in env.checks:
            if c.category != last_cat:
                lines.append(f"[b]{c.category}[/b]")
                last_cat = c.category
            style = LEVEL_STYLE.get(c.level, "")
            mark = LEVEL_MARK.get(c.level, "?")
            lines.append(f"  [{style}]{mark}[/{style}] {c.text}")
        self.update("\n".join(lines))


class MenuItemWidget(Static):
    def __init__(self, item: "Item") -> None:
        super().__init__(item_label(item))
        self.item: Item = item


class LisuanFixApp(App[None]):
    """砺算 7G100 修复工具 TUI。"""

    CSS = """
    Screen { layout: vertical; }
    #body { height: 1fr; }
    #env  { width: 45%; border: round $accent; padding: 0 1; overflow-y: auto; }
    #menu { width: 55%; border: round $accent; }
    #log  { height: 12; border: round $accent; }
    """

    BINDINGS = [
        ("q", "quit", "退出"),
        ("r", "refresh", "重新检测"),
    ]

    @override
    def compose(self) -> ComposeResult:
        yield Header()
        with Horizontal(id="body"):
            yield EnvPanel(id="env")
            yield ListView(id="menu")
        yield RichLog(id="log", markup=True, wrap=True)
        yield Footer()

    def on_mount(self) -> None:
        self.title = "砺算 7G100 修复工具"
        self.sub_title = "上下键选择 · 回车执行 · r 重新检测 · q 退出"
        self.query_one("#env", EnvPanel).refresh_env()
        self._load_menu()
        self.query_one("#log", RichLog).write("[dim]就绪。选择菜单项后回车执行。[/dim]")

    def _load_menu(self) -> None:
        menu = self.query_one("#menu", ListView)
        menu.clear()
        for g in GROUP_ORDER:
            items = [it for it in ITEMS if it["group"] == g]
            if not items:
                continue
            menu.append(ListItem(Label(f"[b]{g}[/b]"), disabled=True))
            for it in items:
                menu.append(ListItem(MenuItemWidget(it)))

    def action_refresh(self) -> None:
        self.query_one("#env", EnvPanel).refresh_env()
        self.query_one("#log", RichLog).write("[dim]环境已重新检测。[/dim]")

    def on_list_view_selected(self, event: ListView.Selected) -> None:
        if not event.item.children:
            return
        widget = event.item.children[0]
        if not isinstance(widget, MenuItemWidget):
            return
        log = self.query_one("#log", RichLog)
        log.clear()
        log.write(f"[b]执行 {widget.item['id']}) {widget.item['title']}[/b]")
        self.run_worker(
            lambda: self._exec(widget.item["id"]), thread=True, exclusive=True
        )

    def _exec(self, item_id: int) -> None:
        """在后台线程执行，逐行把输出转发到日志区。"""
        log = self.query_one("#log", RichLog)
        it = ITEMS_BY_ID[item_id]

        if it["cmd"] is None:
            self.call_from_thread(
                log.write,
                (
                    f"[yellow]菜单项 {item_id} 需要交互或内部实现，请在命令行执行：\n"
                    + f"  python3 lisuan_fix.py {item_id}[/yellow]"
                ),
            )
            return

        cmd = it["cmd"]([])
        if it["root"] and os.geteuid() != 0:
            cmd = ["sudo", *cmd]

        self.call_from_thread(log.write, f"[dim]$ {' '.join(cmd)}[/dim]")
        try:
            proc = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                errors="replace",
                bufsize=1,
            )
            assert proc.stdout is not None
            stdout = cast(TextIO, proc.stdout)
            for line in stdout:
                self.call_from_thread(log.write, line.rstrip())
            code = proc.wait()
        except FileNotFoundError as e:
            self.call_from_thread(log.write, f"[red]命令不存在：{e}[/red]")
            return
        except Exception as e:  # noqa: BLE001
            self.call_from_thread(log.write, f"[red]执行失败：{e}[/red]")
            return

        style = "green" if code == 0 else "red"
        self.call_from_thread(
            log.write, f"[{style}]=== 结束，返回码 {code} ===[/{style}]"
        )
        self.call_from_thread(self.query_one("#env", EnvPanel).refresh_env)


def run_tui() -> None:
    LisuanFixApp().run()
