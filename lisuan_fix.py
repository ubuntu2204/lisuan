#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""砺算 7G100 修复工具（Python 版，替代 lisuan-fix.sh）

三种模式：
    python3 lisuan_fix.py                 交互界面（有 Textual 用 TUI，否则纯文本菜单）
    python3 lisuan_fix.py --detect        只检测并打印报告
    python3 lisuan_fix.py --detect --json 检测并输出 JSON（便于脚本化）
    python3 lisuan_fix.py 2 [参数]        非交互直执行第 2 项

菜单项编号与旧版 lisuan-fix.sh 一一对应，README 中的说明无需修改。

依赖：仅标准库。Textual 为可选增强（无它自动降级为纯文本菜单）。
"""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, TypedDict

REPO_ROOT = Path(__file__).resolve().parent
S = REPO_ROOT

# ---------------------------------------------------------------- 基础工具


def run(cmd: list[str], timeout: int = 30) -> tuple[int, str]:
    """执行命令，返回 (返回码, 合并后的输出)。命令不存在时返回 (127, "")。"""
    try:
        p = subprocess.run(
            cmd, capture_output=True, text=True, timeout=timeout, errors="replace"
        )
        return p.returncode, (p.stdout + p.stderr).strip()
    except FileNotFoundError:
        return 127, ""
    except subprocess.TimeoutExpired:
        return 124, "timeout"


def out(cmd: list[str], timeout: int = 30) -> str:
    return run(cmd, timeout)[1]


def ok_cmd(cmd: list[str], timeout: int = 30) -> bool:
    return run(cmd, timeout)[0] == 0


# ---------------------------------------------------------------- 检测结果


@dataclass
class Check:
    """一条检测结论。level: ok / bad / warn / info"""

    category: str
    level: str
    text: str
    hint: str = ""          # 指向的菜单项，如 "菜单 [2]"


@dataclass
class Env:
    kernel: str = ""
    checks: list[Check] = field(default_factory=list)

    def add(self, category: str, level: str, text: str, hint: str = "") -> None:
        self.checks.append(Check(category, level, text, hint))

    def by_level(self, level: str) -> list[Check]:
        return [c for c in self.checks if c.level == level]


# ---------------------------------------------------------------- 环境检测


def detect() -> Env:
    """只读检测，逐项给出结论并指向对应菜单项。与旧版 detect() 等价。"""
    env = Env()
    env.kernel = os.uname().release

    # ---- GPU ----
    lspci = out(["lspci", "-nn"])
    if "4c54:" in lspci:
        model = next(
            (ln.split(": ", 1)[1] for ln in lspci.splitlines() if "4c54:5006" in ln),
            "砺算 GPU",
        )
        env.add("GPU", "ok", f"检测到砺算 GPU：{model}")
    else:
        env.add("GPU", "bad", "未检测到砺算 GPU（PCI 厂商号 4c54）——本工具不适用")

    # ---- 内核 ----
    env.add("内核", "info", f"运行内核：{env.kernel}")

    # ---- DKMS / 驱动包 ----
    if ok_cmd(["dpkg-query", "-W", "lisuangpu-dkms"]):
        ver = out(["dpkg-query", "-W", "-f=${Version}", "lisuangpu-dkms"])
        env.add("驱动包", "ok", f"lisuangpu-dkms 已安装：{ver}")
    else:
        env.add("驱动包", "bad", "lisuangpu-dkms 未安装（请先向厂商/OEM 获取驱动包）")

    if shutil.which("dkms"):
        dkst = out(["dkms", "status"])
        lisuan_lines = [ln for ln in dkst.splitlines() if "lisuan" in ln.lower()]
        if lisuan_lines:
            for ln in lisuan_lines:
                env.add("DKMS", "info", f"dkms: {ln}")
        else:
            env.add("DKMS", "warn", "dkms 中无 lisuan 模块记录")

    # ---- 内核模块 gf ----
    if ok_cmd(["lsmod"]) and "gf " in out(["lsmod"]):
        env.add("模块", "ok", "模块 gf 已加载")
    else:
        env.add("模块", "bad", "模块 gf 未加载 —— 优先尝试菜单 [2] 修复 DKMS", "2")

    ko = ""
    moddir = Path(f"/lib/modules/{env.kernel}")
    if moddir.is_dir():
        for p in sorted(moddir.rglob("gf.ko*")):
            ko = str(p)
            break
    if ko:
        verm = ""
        for ln in out(["modinfo", ko]).splitlines():
            if ln.startswith("vermagic"):
                verm = ln.split()[1] if len(ln.split()) > 1 else ""
                break
        if verm == env.kernel:
            env.add("模块", "ok", f"vermagic 与运行内核一致（{verm}）")
        else:
            env.add("模块", "bad",
                    f"vermagic 错配：模块={verm or '?'}，运行内核={env.kernel} —— 菜单 [2]", "2")
    else:
        env.add("模块", "warn", "当前内核模块树下没有 gf.ko —— 菜单 [2]", "2")

    # ---- DRM / DRI ----
    dri = Path("/dev/dri")
    cards = sorted(p.name for p in dri.glob("card*")) if dri.is_dir() else []
    if cards:
        env.add("DRM", "ok", "DRM 节点：" + " ".join(cards))
    else:
        env.add("DRM", "bad", "无 /dev/dri/cardN")

    dri_dir = Path("/usr/lib/x86_64-linux-gnu/dri")
    if (dri_dir / "lisuan_dri.so").exists():
        env.add("DRI", "ok", "lisuan_dri.so 软链存在（DRI 驱动名已修复）")
    elif (dri_dir / "gf_dri.so").exists():
        env.add("DRI", "bad", "缺 lisuan_dri.so 软链 —— 菜单 [4]", "4")
    else:
        env.add("DRI", "warn", "未见 gf_dri.so（lisuangpu-lib 未安装？）")

    # ---- Steam ----
    if shutil.which("steam"):
        env.add("Steam", "ok", "Steam 已安装")
        wrap = Path.home() / ".local/share/Steam/ubuntu12_64/steamwebhelper_sniper_wrap.sh"
        if wrap.is_file():
            try:
                content = wrap.read_text(errors="replace")
            except OSError:
                content = ""
            has_gbm = "GBM_BACKEND=dri" in content
            has_glapi = "libglapi_gf" in content
            if has_gbm and has_glapi:
                env.add("Steam", "ok", "Steam CEF 补丁已注入（GBM_BACKEND + LD_PRELOAD）")
            else:
                env.add("Steam", "bad", "Steam CEF 补丁缺失/被客户端还原 —— 菜单 [5]", "5")
            attr = out(["lsattr", str(wrap)])
            if "i" in attr.split()[0] if attr else False:
                env.add("Steam", "ok", "wrap.sh 已锁定（chattr +i）")
            else:
                env.add("Steam", "warn", "wrap.sh 未锁定，可能被 Steam 校验还原 —— 菜单 [5]", "5")
    else:
        env.add("Steam", "info", "未安装 Steam（跳过 Steam 相关项）")

    # ---- 开机自愈 ----
    if ok_cmd(["systemctl", "is-enabled", "lisuan-steam-autofix.service"]):
        env.add("自愈", "ok", "Steam 补丁开机自愈服务已安装（Steam 更新后自动重新注入）")
    else:
        env.add("自愈", "warn",
                "未安装开机自愈服务 —— 菜单 [9]（Steam 更新后需手动跑菜单 [5]）", "9")

    # ---- 游戏 ----
    ul = Path.home() / ".local/share/Steam/steamapps/common/Underlords/game"
    ulsh = ul / "underlords.sh"
    injected = False
    if ulsh.is_file():
        try:
            injected = "Lisuan 7G100 fix (injected)" in ulsh.read_text(errors="replace")
        except OSError:
            injected = False
    if injected and os.access(ul / "play-native.sh", os.X_OK):
        env.add("游戏", "ok",
                "Dota Underlords 修复已安装（Steam 内直接点开始游戏；菜单 [8]/[15]）")
    elif ul.is_dir():
        env.add("游戏", "warn", "Dota Underlords 修复未安装 —— 菜单 [14]", "14")

    return env


# ---------------------------------------------------------------- 菜单项

#: 命令构造函数：接收额外命令行参数，返回完整 argv
CmdFn = Callable[[list[str]], list[str]]


class Item(TypedDict):
    """一个菜单项。cmd 为 None 表示内部实现（见 run_item 的特例分支）。"""

    id: int
    title: str
    group: str
    root: bool
    cmd: CmdFn | None


def _sh(rel: str, *args: str) -> list[str]:
    return ["bash", str(S / rel), *args]


ITEMS: list[Item] = [
    {"id": 1,  "title": "重新检测系统环境", "group": "检测", "root": False,
     "cmd": None},
    {"id": 2,  "title": "修复 DKMS 驱动构建/加载（vermagic 错配、黑屏）", "group": "修复",
     "root": True,  "cmd": lambda a: _sh("display/fix-lisuan-gpu.sh", *a)},
    {"id": 4,  "title": "修复 DRI 驱动名（lisuan_dri.so 软链）", "group": "修复",
     "root": True,  "cmd": lambda a: _sh("mesa/fix-lisuan-mesa-driver-name.sh")},
    {"id": 5,  "title": "修复 Steam 界面崩溃（注入容器补丁并锁定）", "group": "修复",
     "root": True,  "cmd": lambda a: _sh("steam/fix-steam-cef-lock.sh")},
    {"id": 6,  "title": "解锁 Steam 补丁（Steam 客户端更新前执行）", "group": "修复",
     "root": True,  "cmd": lambda a: _sh("steam/fix-steam-cef-lock.sh", "--unlock")},
    {"id": 9,  "title": "安装开机自愈（Steam 更新后自动重新注入补丁）", "group": "修复",
     "root": True,  "cmd": None},  # 内部实现，见 install_autofix
    {"id": 10, "title": "卸载开机自愈服务", "group": "卸载", "root": True,
     "cmd": None},  # 内部实现
    {"id": 11, "title": "卸载 DKMS Makefile 补丁（还原原厂，不影响已装驱动）", "group": "卸载",
     "root": True,  "cmd": lambda a: _sh("display/uninstall-lisuan-gpu.sh")},
    {"id": 12, "title": "卸载 DRI 驱动名软链（只删我们建的软链）", "group": "卸载",
     "root": True,  "cmd": lambda a: _sh("mesa/uninstall-lisuan-mesa-driver-name.sh")},
    {"id": 13, "title": "卸载 Steam 补丁（还原 wrap.sh，界面将回到无法启动状态）", "group": "卸载",
     "root": True,  "cmd": lambda a: _sh("steam/fix-steam-cef-lock.sh", "--uninstall")},
    {"id": 3,  "title": "GPU 驱动体检（lspci/lsmod/modinfo/DRM）", "group": "体检",
     "root": False, "cmd": lambda a: _sh("display/verify-lisuan-gpu.sh")},
    {"id": 7,  "title": "系统 Mesa 兼容性体检（zink/GBM dri 后端）", "group": "体检",
     "root": False, "cmd": lambda a: _sh("mesa/verify-system-mesa.sh")},
    {"id": 8,  "title": "启动 Dota Underlords（宿主直跑，需 Steam 在运行）", "group": "游戏",
     "root": False, "cmd": None},  # 内部实现
    {"id": 14, "title": "安装/修复 Dota Underlords 补丁集（容器委托+重试+libpng12+桌面项）",
     "group": "游戏", "root": True,
     "cmd": lambda a: _sh("underlords/fix-underlords-native.sh")},
    {"id": 15, "title": "查看 Dota Underlords 修复状态", "group": "游戏", "root": False,
     "cmd": lambda a: _sh("underlords/fix-underlords-native.sh", "--status")},
    {"id": 16, "title": "启动 ZCode（--no-sandbox + XWayland + 禁用 GPU 合成，修显示异常）",
     "group": "Electron", "root": False, "cmd": lambda a: _sh("electron/use-zcode.sh")},
    {"id": 17, "title": "把 ZCode 桌面项 Exec 切到修复 wrapper", "group": "Electron",
     "root": False, "cmd": lambda a: _sh("electron/use-zcode.sh", "--install-desktop")},
    {"id": 18, "title": "启动 CodeBuddy 国内版（gf 硬件加速）", "group": "Electron",
     "root": False, "cmd": lambda a: _sh("electron/use-codebuddy.sh")},
    {"id": 19, "title": "把 CodeBuddy 桌面项 Exec 切到加速 wrapper", "group": "Electron",
     "root": False, "cmd": lambda a: _sh("electron/use-codebuddy.sh", "--install-desktop")},
    {"id": 20, "title": "启动 Qoder 国内版（gf 硬件加速）", "group": "Electron",
     "root": False, "cmd": lambda a: _sh("electron/use-qoder.sh")},
    {"id": 21, "title": "把 Qoder 桌面项 Exec 切到加速 wrapper", "group": "Electron",
     "root": False, "cmd": lambda a: _sh("electron/use-qoder.sh", "--install-desktop")},
]

ITEMS_BY_ID = {it["id"]: it for it in ITEMS}
GROUP_ORDER = ["检测", "修复", "卸载", "体检", "游戏", "Electron"]


def item_label(it: Item) -> str:
    tag = " [需要 sudo]" if it["root"] else ""
    return f"{it['id']:>2}) {it['title']}{tag}"


# ---------------------------------------------------------------- 执行

AUTOFIX_UNIT = "/etc/systemd/system/lisuan-steam-autofix.service"
AUTOFIX_SRC = S / "steam/steam-cef-autofix.sh"


def install_autofix() -> int:
    if not AUTOFIX_SRC.is_file():
        print(f"  错误：找不到 {AUTOFIX_SRC}")
        return 1
    unit = """[Unit]
Description=Lisuan Steam CEF patch auto-heal (re-inject after Steam update)
After=network.target

[Service]
Type=oneshot
ExecStart={src}

[Install]
WantedBy=multi-user.target
""".format(src=AUTOFIX_SRC)
    cmds = [
        ["tee", AUTOFIX_UNIT],
        ["systemctl", "daemon-reload"],
        ["systemctl", "enable", "lisuan-steam-autofix.service"],
    ]
    return run_elevated_sequence(unit, cmds)


def remove_autofix() -> int:
    seq = [
        ["systemctl", "disable", "--now", "lisuan-steam-autofix.service"],
        ["rm", "-f", AUTOFIX_UNIT],
        ["systemctl", "daemon-reload"],
    ]
    return run_elevated_sequence(None, seq)


def run_elevated_sequence(stdin_data: str | None, cmds: list[list[str]]) -> int:
    """依次执行命令，非 root 时自动套 sudo。第一条可带 stdin。"""
    for i, cmd in enumerate(cmds):
        if os.geteuid() != 0:
            cmd = ["sudo", *cmd]
        try:
            p = subprocess.run(
                cmd,
                input=(stdin_data if i == 0 else None),
                text=True,
                errors="replace",
            )
            if p.returncode != 0:
                print(f"  命令失败（{p.returncode}）：{' '.join(cmd)}")
                return p.returncode
        except FileNotFoundError:
            print(f"  命令不存在：{cmd[0]}")
            return 127
    return 0


def launch_underlords() -> int:
    ul = Path.home() / ".local/share/Steam/steamapps/common/Underlords/game/play-native.sh"
    if not os.access(ul, os.X_OK):
        print(f"  未找到 {ul}")
        return 1
    if not out(["pgrep", "-f", "ubuntu12_32/steam"]):
        print("  [!!] Steam 客户端未在运行，游戏可能无法完成授权")
    return subprocess.run([str(ul)]).returncode


def run_item(item_id: int, extra: list[str] | None = None) -> int:
    """执行菜单项。返回进程返回码。"""
    extra = extra or []
    it = ITEMS_BY_ID.get(item_id)
    if it is None:
        print(f"  无效选项：{item_id}")
        return 1

    if item_id == 1:
        print(render_report(detect()))
        return 0
    if item_id == 9:
        return install_autofix()
    if item_id == 10:
        return remove_autofix()
    if item_id == 8:
        return launch_underlords()

    cmd = it["cmd"](extra) if it["cmd"] else None
    if cmd is None:
        print(f"  菜单项 {item_id} 未定义命令")
        return 1
    if it["root"] and os.geteuid() != 0:
        cmd = ["sudo", *cmd]
    print(f"  执行：{' '.join(cmd)}")
    return subprocess.run(cmd).returncode


# ---------------------------------------------------------------- 报告渲染

LEVEL_MARK = {"ok": "[OK]", "bad": "[--]", "warn": "[!!]", "info": "[ii]"}


def render_report(env: Env) -> str:
    lines = ["=" * 54, "  环境检测", "=" * 54]
    last_cat = None
    for c in env.checks:
        if c.category != last_cat:
            lines.append(f"== {c.category} ==")
            last_cat = c.category
        lines.append(f"  {LEVEL_MARK.get(c.level, '[??]')} {c.text}")
    lines.append("")
    return "\n".join(lines)


def report_json(env: Env) -> str:
    return json.dumps(
        {
            "kernel": env.kernel,
            "checks": [
                {
                    "category": c.category,
                    "level": c.level,
                    "text": c.text,
                    "hint": c.hint,
                }
                for c in env.checks
            ],
            "summary": {
                "ok": len(env.by_level("ok")),
                "bad": len(env.by_level("bad")),
                "warn": len(env.by_level("warn")),
            },
        },
        ensure_ascii=False,
        indent=2,
    )


# ---------------------------------------------------------------- 入口分发


def _load_tui() -> Callable[[], None] | None:
    """动态加载 Textual 界面，不可用时返回 None（调用方降级为纯文本菜单）。

    用 importlib 而不是静态 import，是为了断开 lisuan_fix <-> tui 的导入环：
    tui 需要本模块的 ITEMS / detect / item_label，本模块需要 tui 的 run_tui。
    """
    try:
        import importlib

        mod = importlib.import_module("tui")
    except Exception:
        return None
    fn: Callable[[], None] | None = getattr(mod, "run_tui", None)
    return fn if callable(fn) else None


def main(argv: list[str]) -> int:
    args = [a for a in argv[1:]]

    if args and args[0] == "--detect":
        env = detect()
        if "--json" in args:
            print(report_json(env))
        else:
            print(render_report(env))
        return 0

    if args and args[0].isdigit():
        item_id = int(args[0])
        extra = args[1:]
        print(render_report(detect()))
        return run_item(item_id, extra)

    if args and args[0] in ("-h", "--help"):
        print(__doc__)
        return 0

    # 交互模式
    run_tui = _load_tui()
    if run_tui is not None and sys.stdin.isatty():
        run_tui()
        return 0
    return text_menu()


def text_menu() -> int:
    """纯文本交互菜单（Textual 不可用时的降级路径，与旧 bash 版等价）。"""
    print(render_report(detect()))
    while True:
        print("=" * 54)
        print("  修复菜单（输入编号回车执行，Ctrl-C 退出）")
        print("=" * 54)
        for g in GROUP_ORDER:
            items = [it for it in ITEMS if it["group"] == g]
            if not items:
                continue
            print(f"  {g}")
            for it in items:
                print("  " + item_label(it))
        print("   0) 退出")
        print("=" * 54)
        try:
            choice = input("请选择 [0-21]: ").strip()
        except (EOFError, KeyboardInterrupt):
            print("\n再见。")
            return 0
        if choice == "0":
            print("再见。")
            return 0
        if not choice.isdigit():
            continue
        try:
            rc = run_item(int(choice))
        except KeyboardInterrupt:
            print("\n  已中断。")
            continue
        if rc != 0:
            print(f"  （返回码 {rc}）")
        print()


if __name__ == "__main__":
    # 把本模块登记为 "lisuan_fix"，让 tui.py 的 `from lisuan_fix import ...`
    # 命中本模块，避免把 lisuan_fix.py 当成独立模块再加载一份副本。
    sys.modules.setdefault("lisuan_fix", sys.modules["__main__"])
    try:
        sys.exit(main(sys.argv))
    except KeyboardInterrupt:
        print("\n再见。")
        sys.exit(130)
