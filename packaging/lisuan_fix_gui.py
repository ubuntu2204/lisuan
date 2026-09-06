#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""lisuan_fix_gui — 砺算 7G100 修复工具的 Tkinter 桌面界面。

布局：左侧环境检测（按级别着色），右侧修复菜单树（按类别分组、双击执行），
底部实时日志与状态栏。需要 root 的菜单项会打开终端窗口，在真实终端里
以 sudo 重跑本工具的非交互模式（密码输入与脚本交互都在真实终端中）。

公共接口：
    gui_available() -> bool   真实探测（创建一次 tk.Tk()）
    run_gui() -> None         启动主窗口并进入主循环

依赖：tkinter（Ubuntu：python3-tk）+ lisuan_fix_core。
"""

from __future__ import annotations

import os
import queue
import shlex
import shutil
import subprocess
import sys
import threading
from pathlib import Path

import tkinter as tk
from tkinter import scrolledtext, ttk

from lisuan_fix_core import (
    ITEMS,
    ITEMS_BY_ID,
    GROUP_ORDER,
    REPO_ROOT,
    S,
    UNDO,
    UNDO_SHOWN_WHEN,
    Env,
    collect_statuses,
    detect,
    install_steam_fix,
    remove_steam_fix,
)
from lisuan_fix_md import render as render_md

ACCENT = "#0aa2c0"
ACCENT_DARK = "#087f9c"
CLR = {
    "ok": "#2e7d32", "bad": "#c62828", "warn": "#ef6c00", "info": "#1565c0",
    "bg": "#eef2f5", "card": "#ffffff", "fg": "#1f2933", "line": "#d7dee5",
    "sel": "#d4f1f6",
}

MONO = ("Noto Sans Mono CJK SC", 10)
UI = ("Noto Sans CJK SC", 10)


def gui_available() -> bool:
    """真实探测：能否创建 Tk 主窗口（无显示环境 / 缺 Tcl-Tk 库时为 False）。"""
    try:
        probe = tk.Tk()
        probe.withdraw()
        probe.destroy()
        return True
    except Exception:
        return False


def _install_entry() -> str:
    """install.py 入口的实际路径（GUI 内 sudo 重跑用）。

    - deb 安装模式：/usr/bin/lisuan-fix；
    - 源码仓库模式：仓库根的 install.py。
    """
    if Path("/usr/share/lisuan-fix/lisuan_fix_core.py").is_file():
        return "/usr/bin/lisuan-fix"
    return str(REPO_ROOT / "install.py")


def open_in_terminal(item_id: int, extra: str = "") -> int:
    """需要 root 的项：打开终端窗口，在终端里以 sudo 重跑本工具的非交互模式。

    extra 为附加参数（如 "--undo" 表示执行该项的卸载/还原）。
    密码输入与脚本内的交互提示都发生在真实终端里，结束后驻留显示结果。
    """
    term = (shutil.which("x-terminal-emulator") or shutil.which("gnome-terminal")
            or shutil.which("konsole") or shutil.which("xterm"))
    suffix = f" {extra}" if extra else ""
    if not term:
        print("  未找到终端模拟器，无法获取 root 授权。请改在命令行执行：")
        print(f"    sudo python3 {_install_entry()} {item_id}{suffix}")
        return 127
    inner = (
        f"cd {shlex.quote(str(REPO_ROOT))} && "
        f"sudo python3 {shlex.quote(_install_entry())} {item_id}{suffix}; "
        f'echo; read -n1 -p "完成，按任意键关闭..."'
    )
    if "gnome-terminal" in term and "x-terminal-emulator" not in term:
        subprocess.run([term, "--", "bash", "-c", inner])
    else:
        subprocess.run([term, "-e", f'bash -c "{inner}"'])
    return 0


class App:
    """主窗口。"""

    def __init__(self, root: "tk.Tk") -> None:
        self.root = root
        self.q: "queue.Queue[tuple]" = queue.Queue()
        self.busy = False
        self._titles: dict[int, str] = {}   # 菜单项原始标题（状态后缀前的文本）
        root.title("砺算 7G100 修复工具")
        root.geometry("1150x700")
        root.minsize(960, 600)
        root.configure(bg=CLR["bg"])
        self._build()
        self._fill_menu()          # 修复：菜单树必须在启动时填充
        self.root.after(80, self._poll)
        self.refresh()

    # ---- 布局 ----
    def _build(self) -> None:
        top = tk.Frame(self.root, bg=ACCENT_DARK, height=46)
        top.pack(fill="x")
        top.pack_propagate(False)
        tk.Label(top, text="砺算 7G100 修复工具", bg=ACCENT_DARK, fg="white",
                 font=(UI[0], 14, "bold")).pack(side="left", padx=14)
        mode = "root" if os.geteuid() == 0 else "普通用户"
        tk.Label(top, text=f"当前权限：{mode}", bg=ACCENT_DARK,
                 fg="#e0f7fa").pack(side="right", padx=14)

        body = tk.PanedWindow(self.root, orient="horizontal", sashwidth=4,
                              bg=CLR["line"])
        body.pack(fill="both", expand=True, padx=6, pady=6)

        # 左：环境检测
        lf = tk.LabelFrame(body, text=" 环境检测 ", bg=CLR["card"], fg=ACCENT_DARK,
                           font=(UI[0], 10, "bold"))
        body.add(lf, width=470, minsize=360)
        self.env = tk.Text(lf, bd=0, bg=CLR["card"], fg=CLR["fg"],
                           font=MONO, wrap="word", state="disabled", cursor="arrow")
        evsb = ttk.Scrollbar(lf, command=self.env.yview)
        self.env.configure(yscrollcommand=evsb.set)
        evsb.pack(side="right", fill="y")
        self.env.pack(fill="both", expand=True, padx=4, pady=4)
        for tag, color in (("ok", CLR["ok"]), ("bad", CLR["bad"]),
                           ("warn", CLR["warn"]), ("info", CLR["info"]),
                           ("cat", ACCENT_DARK)):
            self.env.tag_configure(tag, foreground=color)

        # 右：菜单树
        rf = tk.LabelFrame(body, text=" 修复菜单（双击执行）", bg=CLR["card"],
                           fg=ACCENT_DARK, font=(UI[0], 10, "bold"))
        body.add(rf, minsize=480)
        style = ttk.Style()
        try:
            style.theme_use("clam")
        except Exception:
            pass
        style.configure("Fix.Treeview", rowheight=26, font=UI,
                        background=CLR["card"], fieldbackground=CLR["card"])
        style.map("Fix.Treeview",
                  background=[("selected", CLR["sel"])],
                  foreground=[("selected", CLR["fg"])])
        self.tree = ttk.Treeview(rf, columns=("act",), show="tree headings",
                                 selectmode="browse", style="Fix.Treeview")
        tsb = ttk.Scrollbar(rf, command=self.tree.yview)
        self.tree.configure(yscrollcommand=tsb.set)
        tsb.pack(side="right", fill="y")
        self.tree.pack(fill="both", expand=True, padx=4, pady=4)
        self.tree.column("#0", width=440)
        # "操作"列：已配置的安装项在此显示"卸载"按钮
        self.tree.heading("act", text="操作")
        self.tree.column("act", width=70, anchor="center", stretch=False)
        self.tree.bind("<Double-1>", self._on_double)
        self.tree.bind("<Return>", self._on_double)
        self.tree.bind("<Button-1>", self._on_click)
        # 修复状态行染色（标题后缀符号 + 整行着色）
        self.tree.tag_configure("st_ok", foreground=CLR["ok"])
        self.tree.tag_configure("st_bad", foreground=CLR["bad"])
        self.tree.tag_configure("st_warn", foreground=CLR["warn"])
        self.tree.tag_configure("st_dim", foreground="#78909c")

        # 底部：日志 + 状态栏
        logf = tk.LabelFrame(self.root, text=" 执行日志 ", bg=CLR["card"],
                             fg=ACCENT_DARK, font=(UI[0], 10, "bold"))
        logf.pack(fill="both", padx=6, pady=(0, 4))
        self.log = scrolledtext.ScrolledText(
            logf, height=11, bd=0, bg="#263238", fg="#eceff1",
            font=("Noto Sans Mono CJK SC", 9), insertbackground="white")
        self.log.pack(fill="both", expand=True, padx=4, pady=4)
        self.log.tag_configure("ok", foreground="#8bc34a")
        self.log.tag_configure("bad", foreground="#ff8a80")
        self.log.tag_configure("warn", foreground="#ffd54f")
        self.log.tag_configure("dim", foreground="#90a4ae")

        self.status = tk.Label(self.root, text="就绪", anchor="w", bg="#eceff1",
                               fg=CLR["fg"])
        self.status.pack(fill="x", side="bottom")

    # ---- 日志 ----
    def _log(self, text: str, tag: str = "") -> None:
        self.log.configure(state="normal")
        self.log.insert("end", text + "\n", (tag,) if tag else ())
        self.log.see("end")
        self.log.configure(state="disabled")

    # ---- 环境检测 ----
    def refresh(self) -> None:
        self._set_status("正在检测环境…")
        threading.Thread(target=self._detect_worker, daemon=True).start()

    def _detect_worker(self) -> None:
        env = detect()
        self.q.put(("env", env))
        try:
            self.q.put(("status", collect_statuses(env)))
        except Exception as e:  # noqa: BLE001
            self.q.put(("log", f"状态收集失败：{e}", "warn"))

    def _fill_env(self, env: Env) -> None:
        self.env.configure(state="normal")
        self.env.delete("1.0", "end")
        self.env.insert("end", f"内核  {env.kernel}\n\n", "cat")
        last = None
        for c in env.checks:
            if c.category != last:
                self.env.insert("end", f"◆ {c.category}\n", "cat")
                last = c.category
            mark = {"ok": "✓", "bad": "✗", "warn": "!", "info": "·"}[c.level]
            self.env.insert("end", f"  [{mark}] ", c.level)
            self.env.insert("end", c.text + "\n")
        self.env.configure(state="disabled")
        n_bad, n_warn = len(env.by_level("bad")), len(env.by_level("warn"))
        tip = f"异常 {n_bad} · 警告 {n_warn}" if (n_bad or n_warn) else "一切正常"
        self._set_status(f"检测完成：{tip}")

    # ---- 菜单树 ----
    def _fill_menu(self) -> None:
        self.tree.delete(*self.tree.get_children())
        for g in GROUP_ORDER:
            items = [i for i in ITEMS if i["group"] == g]
            if not items:
                continue
            self.tree.insert("", "end", iid=f"g:{g}", text=g, open=True,
                             tags=("group",))
            for it in items:
                label = it["title"] + ("　🔑" if it["root"] else "")
                text = f"{it['id']:>2}.  {label}"
                self._titles[it["id"]] = text
                self.tree.insert(f"g:{g}", "end", iid=f"i:{it['id']}", text=text)
        self.tree.tag_configure("group", font=(UI[0], 10, "bold"),
                                foreground=ACCENT_DARK)

    def _apply_status(self, st: dict[int, tuple[str, str]]) -> None:
        """把各项修复状态追加到菜单标题后，并整行染色；已生效的安装项显示卸载按钮。"""
        row_tag = {"ok": "st_ok", "bad": "st_bad", "warn": "st_warn",
                   "dim": "st_dim"}
        for g in self.tree.get_children():
            for child in self.tree.get_children(g):
                it = ITEMS_BY_ID[int(child[2:])]
                base = self._titles.get(it["id"], "")
                info = st.get(it["id"])
                if not info:
                    self.tree.item(child, text=base, values=("",), tags=())
                    continue
                label, level = info
                # 仅在"已生效"状态且有卸载动作时显示卸载按钮
                # （always=True 的项：卸载脚本幂等，无视状态始终显示）
                act = UNDO.get(it["id"])
                show_undo = bool(act) and (
                    act.get("always")
                    or any(label.startswith(w) for w in UNDO_SHOWN_WHEN))
                self.tree.item(child, text=f"{base}　{label}",
                               values=("卸载" if show_undo else "",),
                               tags=(row_tag.get(level, "st_dim"),))

    # ---- 卸载按钮 ----
    def _on_click(self, event) -> None:
        """点击"操作"列（卸载按钮）时触发还原；其余区域交给原有双击逻辑。"""
        row = self.tree.identify_row(event.y)
        col = self.tree.identify_column(event.x)
        if not row or not row.startswith("i:") or col != "#1":
            return
        if (self.tree.item(row, "values") or ("",))[0] != "卸载":
            return
        self.undo(int(row[2:]))

    def undo(self, item_id: int) -> None:
        """执行安装项的卸载/还原。"""
        if self.busy:
            self._log("已有任务在执行，请稍候…", "warn")
            return
        act = UNDO.get(item_id)
        if act is None:
            return
        title = ITEMS_BY_ID[item_id]["title"]
        if act["root"] and os.geteuid() != 0:
            self._log(f"[{item_id}] 还原需要 root —— 已在新终端窗口打开，"
                      "请在其中输入密码，完成后回到本窗口点「重新检测」。", "warn")
            open_in_terminal(item_id, extra="--undo")
            return
        self.busy = True
        self._set_status(f"还原 [{item_id}] {title} …")
        self._log(f"$ 还原 [{item_id}] {title}", "dim")
        threading.Thread(target=self._undo_worker, args=(item_id,),
                         daemon=True).start()

    def _undo_worker(self, item_id: int) -> None:
        act = UNDO[item_id]
        title = ITEMS_BY_ID[item_id]["title"]
        if not act["cmd"]:
            # 内部实现（12 remove_steam_fix）
            import contextlib
            import io as _io

            buf = _io.StringIO()
            try:
                with contextlib.redirect_stdout(buf):
                    code = remove_steam_fix() if item_id == 11 else 1
                self.q.put(("log", buf.getvalue(), ""))
            except Exception as e:  # noqa: BLE001
                self.q.put(("log", f"还原失败：{e}", "bad"))
                code = 1
            self.q.put(("done", code, f"还原：{title}"))
            return

        try:
            proc = subprocess.Popen(
                [*act["cmd"]], stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                text=True, errors="replace", bufsize=1)
            assert proc.stdout is not None
            for line in proc.stdout:
                self.q.put(("log", line.rstrip(), ""))
            code = proc.wait()
        except FileNotFoundError as e:
            self.q.put(("log", f"命令不存在：{e}", "bad"))
            code = 127
        except Exception as e:  # noqa: BLE001
            self.q.put(("log", f"还原失败：{e}", "bad"))
            code = 1
        self.q.put(("done", code, f"还原：{title}"))

    # ---- 交互 ----
    def _sel_item(self):
        sel = self.tree.selection()
        if not sel or not sel[0].startswith("i:"):
            return None
        return ITEMS_BY_ID[int(sel[0][2:])]

    def _on_double(self, _e=None) -> None:
        it = self._sel_item()
        if it is not None:
            self.execute(it["id"])

    def execute(self, item_id: int) -> None:
        if self.busy:
            self._log("已有任务在执行，请稍候…", "warn")
            return
        it = ITEMS_BY_ID[item_id]

        # 说明项：弹只读窗口显示 md
        if it.get("doc"):
            path = S / it["doc"]
            if not path.is_file():
                self._log(f"文档不存在：{path}", "bad")
                return
            win = tk.Toplevel(self.root)
            win.title(it["title"])
            win.geometry("860x620")
            txt = scrolledtext.ScrolledText(win, wrap="word", font=MONO,
                                            bg=CLR["card"], fg=CLR["fg"], bd=0)
            txt.pack(fill="both", expand=True)
            try:
                render_md(txt, path.read_text(errors="replace"),
                          ui_font=UI, mono_font=MONO, accent=ACCENT_DARK)
            except OSError as e:
                txt.insert("1.0", f"读取失败：{e}")
            txt.configure(state="disabled")
            self._log(f"已打开说明：{it['title']}")
            return

        # 需要root：打开终端窗口，在终端里以 sudo 重跑本工具非交互模式
        if it["root"] and os.geteuid() != 0:
            self._log(f"[{item_id}] 需要 root —— 已在新终端窗口打开，"
                      "请在其中输入密码，完成后回到本窗口点「重新检测」。", "warn")
            open_in_terminal(item_id)
            return

        self.busy = True
        self._set_status(f"执行 [{item_id}] {it['title']} …")
        self._log(f"$ [{item_id}] {it['title']}", "dim")
        threading.Thread(target=self._exec_worker, args=(item_id,),
                         daemon=True).start()

    def _exec_worker(self, item_id: int) -> None:
        it = ITEMS_BY_ID[item_id]
        if it["cmd"] is None:
            # 内部实现（12 install_steam_fix / 13 remove_steam_fix）
            import contextlib
            import io as _io

            buf = _io.StringIO()
            try:
                with contextlib.redirect_stdout(buf):
                    if item_id == 11:
                        code = install_steam_fix()
                    elif item_id == 13:
                        code = remove_steam_fix()
                    else:
                        code = 1
                self.q.put(("log", buf.getvalue() or f"（返回码 {code}）", ""))
                self.q.put(("done", code, it["title"]))
            except Exception as e:  # noqa: BLE001
                self.q.put(("log", f"执行失败：{e}", "bad"))
                self.q.put(("done", 1, it["title"]))
            return

        cmd = it["cmd"]([])
        if cmd[0] == "bash" and len(cmd) > 1 and not Path(cmd[1]).is_file():
            self.q.put(("log", f"脚本缺失：{cmd[1]}", "bad"))
            self.q.put(("log",
                        "说明：deb 安装版未打包该目录，请在源码仓库中运行。", "dim"))
            self.q.put(("done", 127, it["title"]))
            return

        try:
            proc = subprocess.Popen(
                cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                text=True, errors="replace", bufsize=1)
            assert proc.stdout is not None
            for line in proc.stdout:
                self.q.put(("log", line.rstrip(), ""))
            code = proc.wait()
        except FileNotFoundError as e:
            self.q.put(("log", f"命令不存在：{e}", "bad"))
            code = 127
        except Exception as e:  # noqa: BLE001
            self.q.put(("log", f"执行失败：{e}", "bad"))
            code = 1
        self.q.put(("done", code, it["title"]))

    # ---- 队列轮询 ----
    def _poll(self) -> None:
        try:
            while True:
                msg = self.q.get_nowait()
                kind = msg[0]
                if kind == "env":
                    self._fill_env(msg[1])
                elif kind == "status":
                    self._apply_status(msg[1])
                elif kind == "log":
                    text, tag = msg[1], msg[2]
                    for ln in text.splitlines() or [""]:
                        self._log(ln, tag)
                elif kind == "done":
                    code, title = msg[1], msg[2]
                    self._log(f"=== {title} 结束，返回码 {code} ===",
                              "ok" if code == 0 else "bad")
                    self.busy = False
                    self._set_status("就绪")
                    self.refresh()
        except queue.Empty:
            pass
        self.root.after(80, self._poll)

    def _set_status(self, text: str) -> None:
        self.status.configure(text=text)


def run_gui() -> None:
    root = tk.Tk()
    App(root)
    root.mainloop()
