#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""砺算 7G100 修复工具（Python 版，替代 lisuan-fix.sh）。

用法（本文件位于 packaging/，从仓库根运行）：
    python3 packaging/install.py                 桌面窗口（Tkinter）
    python3 packaging/install.py --detect        只检测并打印报告
    python3 packaging/install.py --detect --json 检测并输出 JSON（便于脚本化）
    python3 packaging/install.py 17 [参数]       非交互直执行第 17 项（修复 Steam）
    python3 packaging/install.py -h              显示本帮助

菜单结构：环境检测单独一节（含两项体检），其余按类别分组（DKMS / Mesa /
Electron / Steam / 随从大师 / 刀塔霸业），类内统一"安装/修复 → 卸载 → 说明"。

界面：Tkinter 桌面窗口，需要显示环境（X11/Wayland）与 python3-tk。
无法启动图形界面时直接报错退出（不再降级纯文本菜单）；
非交互场景请用 --detect / --detect --json / <菜单编号>。

依赖：仅标准库。
"""

from __future__ import annotations

import json
import os
import queue
import shlex
import shutil
import subprocess
import sys
import threading
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, TypedDict


def _find_base() -> Path:
    """定位资源根目录（dkms/ steam/ 等子目录所在的目录）。

    - 开发模式：本文件位于 packaging/，仓库根是其父目录；
    - deb 安装模式：本文件位于 /usr/bin/，资源在 /usr/share/lisuan-fix/；
    - 兼容：本文件被直接放在仓库根时，here 即为资源根。
    """
    here = Path(__file__).resolve().parent
    candidates = [
        here.parent,                     # packaging/install.py -> 仓库根
        here,                            # 直接放在仓库根
        Path("/usr/share/lisuan-fix"),   # deb 标准资源路径
    ]
    for c in candidates:
        if (c / "dkms").is_dir() and (c / "steam").is_dir():
            return c
    return here


REPO_ROOT = _find_base()
S = REPO_ROOT

STEAM_DESKTOP = Path.home() / ".local/share/applications/steam.desktop"
STEAM_DESKTOP_EXEC_FIXED = "Exec=env GBM_BACKEND=dri /usr/bin/steam %U"
STEAM_WRAP = Path.home() / ".local/share/Steam/ubuntu12_64/steamwebhelper_sniper_wrap.sh"

_STEAM_DESKTOP_COMMON = """[Desktop Entry]
Name=Steam
Comment=Application for managing and playing games on Steam
{exec_line}
Icon=steam
Terminal=false
Type=Application
Categories=Game;Application;
StartupNotify=false
StartupWMClass=Steam
"""

STEAM_DESKTOP_BODY_FIXED = _STEAM_DESKTOP_COMMON.format(exec_line=(
    "# GBM_BACKEND=dri 是砺算 7G100 上的必需项：厂商 gf_gbm.so 与系统 libgbm 25.x\n"
    "# ABI 不兼容，会让 CEF GPU 进程 SIGSEGV (exit_code=139)。设为 dri 可让 libgbm\n"
    "# 加载 Mesa 的 dri_gbm.so，绕开崩溃。详见 steam/steam-on-lisuan.md §3\n"
    "Exec=env GBM_BACKEND=dri /usr/bin/steam %U"
))

STEAM_DESKTOP_BODY_PLAIN = _STEAM_DESKTOP_COMMON.format(exec_line="Exec=/usr/bin/steam %U")


# ---------------------------------------------------------------- 基础工具


def run(cmd: list[str], timeout: int = 30) -> tuple[int, str]:
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
    category: str
    level: str          # ok / bad / warn / info
    text: str
    hint: str = ""


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
    env = Env()
    env.kernel = os.uname().release

    lspci = out(["lspci", "-nn"])
    if "4c54:" in lspci:
        model = next(
            (ln.split(": ", 1)[1] for ln in lspci.splitlines() if "4c54:5006" in ln),
            "砺算 GPU",
        )
        env.add("GPU", "ok", f"检测到砺算 GPU：{model}")
    else:
        env.add("GPU", "bad", "未检测到砺算 GPU（PCI 厂商号 4c54）——本工具不适用")

    env.add("内核", "info", f"运行内核：{env.kernel}")

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

    if ok_cmd(["lsmod"]) and "gf " in out(["lsmod"]):
        env.add("模块", "ok", "模块 gf 已加载")
    else:
        env.add("模块", "bad", "模块 gf 未加载 —— 优先尝试菜单 [4] 修复 DKMS", "4")

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
                    f"vermagic 错配：模块={verm or '?'}，运行内核={env.kernel} —— 菜单 [4]", "4")
    else:
        env.add("模块", "warn", "当前内核模块树下没有 gf.ko —— 菜单 [4]", "4")

    dri = Path("/dev/dri")
    cards = sorted(p.name for p in dri.glob("card*")) if dri.is_dir() else []
    if cards:
        env.add("DRM", "ok", "DRM 节点：" + " ".join(cards))
    else:
        env.add("DRM", "bad", "无 /dev/dri/cardN")

    # 全局 lisuan_dri.so 软链会让 Steam 客户端 double free，属有害残留
    dri_dir = Path("/usr/lib/x86_64-linux-gnu/dri")
    if (dri_dir / "lisuan_dri.so").exists():
        env.add("DRI", "warn",
                "检测到全局 lisuan_dri.so 软链 —— 它会使 Steam 客户端 double free，"
                "建议移除（菜单 [8]）", "8")
    elif (dri_dir / "gf_dri.so").exists():
        env.add("DRI", "ok", "gf_dri.so 存在（新方案下无需全局软链）")
    else:
        env.add("DRI", "warn", "未见 gf_dri.so（lisuangpu-lib 未安装？）")

    if shutil.which("steam"):
        env.add("Steam", "ok", "Steam 已安装")

        if STEAM_DESKTOP.is_file():
            try:
                dcontent = STEAM_DESKTOP.read_text(errors="replace")
            except OSError:
                dcontent = ""
            if STEAM_DESKTOP_EXEC_FIXED in dcontent:
                env.add("Steam", "ok", "桌面启动器已配置 GBM_BACKEND=dri（CEF 崩溃已修复）")
            else:
                env.add("Steam", "bad",
                        "桌面启动器缺 GBM_BACKEND=dri —— Steam 界面会崩溃，菜单 [17]", "17")

        if STEAM_WRAP.is_file():
            try:
                wcontent = STEAM_WRAP.read_text(errors="replace")
            except OSError:
                wcontent = ""
            if "libglapi_gf" in wcontent or "GBM_BACKEND=dri" in wcontent:
                env.add("Steam", "bad",
                        "wrap.sh 仍含旧方案残留注入 —— 会让 Steam 客户端 double free，"
                        "请清理（菜单 [19]）", "19")

            attr = out(["lsattr", str(STEAM_WRAP)])
            flags = attr.split()[0] if attr else ""
            if "i" in flags:
                env.add("Steam", "bad",
                        "wrap.sh 被 chattr +i 锁定 —— 会触发 Steam 更新回滚循环，"
                        "导致游戏反复重新下载，请立即解锁（菜单 [18]）", "18")
    else:
        env.add("Steam", "info", "未安装 Steam（跳过 Steam 相关项）")

    if ok_cmd(["systemctl", "is-enabled", "lisuan-steam-autofix.service"]):
        env.add("自愈", "warn",
                "开机自愈服务仍在启用 —— 新方案（环境变量）不需要它，建议卸载（菜单 [20]）", "20")

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
                "Dota Underlords 修复已安装（Steam 内直接点开始游戏；菜单 [25]/[26]）")
    elif ul.is_dir():
        env.add("游戏", "warn", "Dota Underlords 修复未安装 —— 菜单 [24]", "24")

    return env


# ---------------------------------------------------------------- 菜单项

CmdFn = Callable[[list[str]], list[str]]


class Item(TypedDict):
    """一个菜单项。cmd 为 None 表示内部实现；doc 非空表示"说明"项。"""

    id: int
    title: str
    group: str
    root: bool
    cmd: CmdFn | None
    doc: str


def _sh(rel: str, *args: str) -> list[str]:
    return ["bash", str(S / rel), *args]


def _doc(rel: str) -> str:
    return rel


# 分组与排列规则（2026-09-05 修订）：
#   1) 环境检测单独一节，两项"体检"也归入此节（本质就是检测）；
#   2) 其余按类别（目录）分组，顺序：DKMS -> Mesa -> Electron -> Steam
#      -> 随从大师 -> 刀塔霸业；
#   3) 每个类别内严格按"安装/修复 -> 卸载 -> 说明"排列。
ITEMS: list[Item] = [
    # ---- 环境检测（含两项体检）----
    {"id": 1,  "title": "重新检测系统环境", "group": "环境检测", "root": False,
     "cmd": None, "doc": ""},
    {"id": 2,  "title": "体检：GPU 驱动（lspci/lsmod/modinfo/DRM）", "group": "环境检测",
     "root": False, "cmd": lambda a: _sh("dkms/verify-lisuan-gpu.sh"), "doc": ""},
    {"id": 3,  "title": "体检：系统 Mesa 兼容性（zink/GBM dri 后端）", "group": "环境检测",
     "root": False, "cmd": lambda a: _sh("mesa/verify-system-mesa.sh"), "doc": ""},

    # ---- DKMS ----
    {"id": 4,  "title": "安装/修复：DKMS 驱动构建与加载（vermagic 错配、黑屏）",
     "group": "DKMS", "root": True,
     "cmd": lambda a: _sh("dkms/fix-lisuan-gpu.sh", *a), "doc": ""},
    {"id": 5,  "title": "卸载：DKMS Makefile 补丁（还原原厂，不影响已装驱动）",
     "group": "DKMS", "root": True,
     "cmd": lambda a: _sh("dkms/uninstall-lisuan-gpu.sh"), "doc": ""},
    {"id": 6,  "title": "说明：厂商 gf/gbm 崩溃 Bug 报告", "group": "DKMS",
     "root": False, "cmd": None, "doc": _doc("dkms/vendor-bug-report-gf-gbm-crash.md")},

    # ---- Mesa ----
    {"id": 7,  "title": "安装/修复：DRI 驱动名（仅原生 GL 游戏需要，含 --native-gl）",
     "group": "Mesa", "root": True,
     "cmd": lambda a: _sh("mesa/fix-lisuan-mesa-driver-name.sh", *a), "doc": ""},
    {"id": 8,  "title": "卸载：DRI 驱动名软链（只删我们建的软链）", "group": "Mesa",
     "root": True, "cmd": lambda a: _sh("mesa/uninstall-lisuan-mesa-driver-name.sh"),
     "doc": ""},
    {"id": 9,  "title": "说明：系统 Mesa 兼容性分析", "group": "Mesa",
     "root": False, "cmd": None, "doc": _doc("mesa/system-mesa-analysis.md")},

    # ---- Electron ----
    {"id": 10, "title": "启动 ZCode（--no-sandbox + XWayland + 禁用 GPU 合成，修显示异常）",
     "group": "Electron", "root": False, "cmd": lambda a: _sh("electron/use-zcode.sh"),
     "doc": ""},
    {"id": 11, "title": "安装桌面项：ZCode Exec 切到修复 wrapper", "group": "Electron",
     "root": False, "cmd": lambda a: _sh("electron/use-zcode.sh", "--install-desktop"),
     "doc": ""},
    {"id": 12, "title": "启动 CodeBuddy 国内版（gf 硬件加速）", "group": "Electron",
     "root": False, "cmd": lambda a: _sh("electron/use-codebuddy.sh"), "doc": ""},
    {"id": 13, "title": "安装桌面项：CodeBuddy Exec 切到加速 wrapper", "group": "Electron",
     "root": False, "cmd": lambda a: _sh("electron/use-codebuddy.sh", "--install-desktop"),
     "doc": ""},
    {"id": 14, "title": "启动 Qoder 国内版（gf 硬件加速）", "group": "Electron",
     "root": False, "cmd": lambda a: _sh("electron/use-qoder.sh"), "doc": ""},
    {"id": 15, "title": "安装桌面项：Qoder Exec 切到加速 wrapper", "group": "Electron",
     "root": False, "cmd": lambda a: _sh("electron/use-qoder.sh", "--install-desktop"),
     "doc": ""},
    {"id": 16, "title": "说明：Electron 类应用适配（显示异常 / 硬件加速）", "group": "Electron",
     "root": False, "cmd": None, "doc": _doc("electron/README.md")},

    # ---- Steam ----
    {"id": 17, "title": "安装/修复：Steam 界面崩溃（桌面启动器写入 GBM_BACKEND=dri）",
     "group": "Steam", "root": False, "cmd": None, "doc": ""},  # install_steam_fix
    {"id": 18, "title": "卸载：移除 GBM_BACKEND 并解锁 Steam 文件", "group": "Steam",
     "root": True, "cmd": None, "doc": ""},  # remove_steam_fix
    {"id": 19, "title": "卸载：清理旧方案残留（还原 wrap.sh 注入）", "group": "Steam",
     "root": True, "cmd": lambda a: _sh("steam/fix-steam-cef-lock.sh", "--uninstall"),
     "doc": ""},
    {"id": 20, "title": "卸载：开机自愈服务（新方案下已不需要）", "group": "Steam",
     "root": True, "cmd": None, "doc": ""},  # remove_autofix
    {"id": 21, "title": "说明：Steam 在砺算上的适配细节（容器/CEF/Proton）", "group": "Steam",
     "root": False, "cmd": None, "doc": _doc("steam/steam-on-lisuan.md")},

    # ---- 随从大师 ----
    {"id": 22, "title": "安装/修复：硬件渲染加固（libSC.so 链接 + DXVK 降级 + hook）",
     "group": "随从大师", "root": True,
     "cmd": lambda a: _sh("minion-masters/fix-minion-masters-7g100-hw.sh", *a), "doc": ""},
    {"id": 23, "title": "说明：随从大师（Minion Masters）修复文档", "group": "随从大师",
     "root": False, "cmd": None, "doc": _doc("minion-masters/fix-minion-masters.md")},

    # ---- 刀塔霸业 ----
    {"id": 24, "title": "安装/修复：Dota Underlords 补丁集（容器委托+重试+libpng12+桌面项）",
     "group": "刀塔霸业", "root": True,
     "cmd": lambda a: _sh("underlords/fix-underlords-native.sh"), "doc": ""},
    {"id": 25, "title": "启动游戏（宿主直跑，需 Steam 在运行）", "group": "刀塔霸业",
     "root": False, "cmd": None, "doc": ""},  # launch_underlords
    {"id": 26, "title": "查看修复状态", "group": "刀塔霸业", "root": False,
     "cmd": lambda a: _sh("underlords/fix-underlords-native.sh", "--status"), "doc": ""},
    {"id": 27, "title": "说明：Dota Underlords 修复文档", "group": "刀塔霸业",
     "root": False, "cmd": None, "doc": _doc("underlords/fix-underlords-native.md")},
]

ITEMS_BY_ID = {it["id"]: it for it in ITEMS}
GROUP_ORDER = ["环境检测", "DKMS", "Mesa", "Electron", "Steam", "随从大师", "刀塔霸业"]


def item_label(it: Item) -> str:
    tag = " [需要 sudo]" if it["root"] else ""
    return f"{it['id']:>2}) {it['title']}{tag}"


# ---------------------------------------------------------------- Steam 修复


def install_steam_fix() -> int:
    """把 GBM_BACKEND=dri 写入 Steam 桌面启动器（新方案，不修改 Steam 文件）。"""
    STEAM_DESKTOP.parent.mkdir(parents=True, exist_ok=True)
    try:
        if STEAM_DESKTOP.is_file():
            shutil.copy2(STEAM_DESKTOP, str(STEAM_DESKTOP) + ".bak")
            print(f"  已备份原启动器 -> {STEAM_DESKTOP}.bak")
        STEAM_DESKTOP.write_text(STEAM_DESKTOP_BODY_FIXED, encoding="utf-8")
    except OSError as e:
        print(f"  写入失败：{e}")
        return 1
    print(f"  已写入 {STEAM_DESKTOP}")
    print("  Steam 界面崩溃修复完成（下次从菜单启动 Steam 生效）。")
    return 0


def remove_steam_fix() -> int:
    """移除 Steam 修复：还原桌面启动器，并解锁可能残留的 chattr +i。"""
    if STEAM_WRAP.is_file():
        pre = [] if os.geteuid() == 0 else ["sudo"]
        r = run([*pre, "chattr", "-i", str(STEAM_WRAP)])
        print(f"  解锁：{'完成' if r[0] == 0 else '跳过（未锁定/文件系统不支持）'}")

    if STEAM_DESKTOP.is_file():
        try:
            content = STEAM_DESKTOP.read_text(errors="replace")
        except OSError:
            content = ""
        if "GBM_BACKEND=dri" in content:
            bak = Path(str(STEAM_DESKTOP) + ".bak")
            restored = False
            if bak.is_file():
                try:
                    bcontent = bak.read_text(errors="replace")
                except OSError:
                    bcontent = ""
                # 仅当备份"干净"（不含修复）时才还原，否则重写纯净模板
                if bcontent and "GBM_BACKEND=dri" not in bcontent:
                    shutil.copy2(bak, STEAM_DESKTOP)
                    print(f"  已从干净备份还原启动器：{STEAM_DESKTOP}")
                    restored = True
            if not restored:
                STEAM_DESKTOP.write_text(STEAM_DESKTOP_BODY_PLAIN, encoding="utf-8")
                print(f"  已重写为纯净启动器：{STEAM_DESKTOP}")
        else:
            print("  启动器未含 GBM_BACKEND=dri，无需还原。")
    return 0


# ---------------------------------------------------------------- 执行

AUTOFIX_UNIT = "/etc/systemd/system/lisuan-steam-autofix.service"
AUTOFIX_SRC = S / "steam/steam-cef-autofix.sh"


def remove_autofix() -> int:
    seq = [
        ["systemctl", "disable", "--now", "lisuan-steam-autofix.service"],
        ["rm", "-f", AUTOFIX_UNIT],
        ["systemctl", "daemon-reload"],
    ]
    for cmd in seq:
        if os.geteuid() != 0:
            cmd = ["sudo", *cmd]
        p = subprocess.run(cmd)
        if p.returncode != 0:
            return p.returncode
    return 0


def launch_underlords() -> int:
    ul = Path.home() / ".local/share/Steam/steamapps/common/Underlords/game/play-native.sh"
    if not os.access(ul, os.X_OK):
        print(f"  未找到 {ul}")
        return 1
    if not out(["pgrep", "-f", "ubuntu12_32/steam"]):
        print("  [!!] Steam 客户端未在运行，游戏可能无法完成授权")
    return subprocess.run([str(ul)]).returncode


def show_doc(rel: str) -> int:
    """命令行模式下展示说明文档（交互终端用 less，非交互直接打印）。"""
    path = S / rel
    if not path.is_file():
        print(f"  文档不存在：{path}")
        return 1
    if sys.stdin.isatty() and shutil.which("less"):
        return subprocess.run(["less", "-R", str(path)]).returncode
    print(path.read_text(errors="replace"))
    return 0


def run_item(item_id: int, extra: list[str] | None = None) -> int:
    """执行菜单项（命令行模式）。返回进程返回码。"""
    extra = extra or []
    it = ITEMS_BY_ID.get(item_id)
    if it is None:
        print(f"  无效选项：{item_id}")
        return 1
    if item_id == 1:
        print(render_report(detect()))
        return 0
    if it.get("doc"):
        return show_doc(it["doc"])
    if item_id == 17:
        return install_steam_fix()
    if item_id == 18:
        return remove_steam_fix()
    if item_id == 20:
        return remove_autofix()
    if item_id == 25:
        return launch_underlords()

    cmd = it["cmd"](extra) if it["cmd"] else None
    if cmd is None:
        print(f"  菜单项 {item_id} 未定义命令")
        return 1

    # 脚本存在性检查：deb 安装版不含 minion-masters/（体积大）
    if cmd[0] == "bash" and len(cmd) > 1 and not Path(cmd[1]).is_file():
        print(f"  脚本缺失：{cmd[1]}")
        print("  说明：deb 安装版未打包该目录。请在源码仓库中运行本工具，"
              "或复制该目录到 /usr/share/lisuan-fix/。")
        return 127

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
                {"category": c.category, "level": c.level, "text": c.text, "hint": c.hint}
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


# ---------------------------------------------------------------- GUI（Tkinter）


def _load_gui() -> Callable[[], None] | None:
    """加载 Tkinter 桌面窗口；无法启动（无显示环境/未装 tkinter）时返回 None，
    调用方 main() 会报错退出。探测方式：真实创建一次 tk.Tk()。"""
    try:
        import tkinter as tk
        from tkinter import scrolledtext, ttk

        probe = tk.Tk()
        probe.withdraw()
        probe.destroy()
    except Exception:
        return None

    ACCENT = "#0aa2c0"
    ACCENT_DARK = "#087f9c"
    CLR = {
        "ok": "#2e7d32", "bad": "#c62828", "warn": "#ef6c00", "info": "#1565c0",
        "bg": "#eef2f5", "card": "#ffffff", "fg": "#1f2933", "line": "#d7dee5",
        "sel": "#d4f1f6",
    }

    def self_file() -> Path:
        return Path(__file__).resolve()

    def open_in_terminal(item_id: int) -> int:
        """需要 root 的项：打开终端窗口，在终端里以 sudo 重跑本工具的非交互模式。

        密码输入与脚本内的交互提示都发生在真实终端里，结束后驻留显示结果。
        """
        term = (shutil.which("x-terminal-emulator") or shutil.which("gnome-terminal")
                or shutil.which("konsole") or shutil.which("xterm"))
        if not term:
            print("  未找到终端模拟器，无法获取 root 授权。请改在命令行执行：")
            print(f"    sudo python3 {self_file()} {item_id}")
            return 127
        inner = (
            f"cd {shlex.quote(str(REPO_ROOT))} && "
            f"sudo python3 {shlex.quote(str(self_file()))} {item_id}; "
            f'echo; read -n1 -p "完成，按任意键关闭..."'
        )
        if "gnome-terminal" in term and "x-terminal-emulator" not in term:
            subprocess.run([term, "--", "bash", "-c", inner])
        else:
            subprocess.run([term, "-e", f'bash -c "{inner}"'])
        return 0

    class App:
        def __init__(self, root: "tk.Tk") -> None:
            self.root = root
            self.q: "queue.Queue[tuple]" = queue.Queue()
            self.busy = False
            root.title("砺算 7G100 修复工具")
            root.geometry("1150x700")
            root.minsize(960, 600)
            root.configure(bg=CLR["bg"])
            self._build()
            self.root.after(80, self._poll)
            self.refresh()

        # ---- 布局 ----
        def _build(self) -> None:
            top = tk.Frame(self.root, bg=ACCENT_DARK, height=46)
            top.pack(fill="x")
            top.pack_propagate(False)
            tk.Label(top, text="砺算 7G100 修复工具", bg=ACCENT_DARK, fg="white",
                     font=("Noto Sans CJK SC", 14, "bold")).pack(side="left", padx=14)
            mode = "root" if os.geteuid() == 0 else "普通用户"
            tk.Label(top, text=f"当前权限：{mode}", bg=ACCENT_DARK,
                     fg="#e0f7fa").pack(side="right", padx=14)

            body = tk.PanedWindow(self.root, orient="horizontal", sashwidth=4,
                                  bg=CLR["line"])
            body.pack(fill="both", expand=True, padx=6, pady=6)

            # 左：环境检测
            lf = tk.LabelFrame(body, text=" 环境检测 ", bg=CLR["card"], fg=ACCENT_DARK,
                               font=("Noto Sans CJK SC", 10, "bold"))
            body.add(lf, width=470, minsize=360)
            self.env = tk.Text(lf, bd=0, bg=CLR["card"], fg=CLR["fg"],
                               font=("Noto Sans Mono CJK SC", 10), wrap="word",
                               state="disabled", cursor="arrow")
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
                               fg=ACCENT_DARK, font=("Noto Sans CJK SC", 10, "bold"))
            body.add(rf, minsize=480)
            style = ttk.Style()
            try:
                style.theme_use("clam")
            except Exception:
                pass
            style.configure("Fix.Treeview", rowheight=26,
                            font=("Noto Sans CJK SC", 10),
                            background=CLR["card"], fieldbackground=CLR["card"])
            style.map("Fix.Treeview",
                      background=[("selected", CLR["sel"])],
                      foreground=[("selected", CLR["fg"])])
            self.tree = ttk.Treeview(rf, show="tree", selectmode="browse",
                                     style="Fix.Treeview")
            tsb = ttk.Scrollbar(rf, command=self.tree.yview)
            self.tree.configure(yscrollcommand=tsb.set)
            tsb.pack(side="right", fill="y")
            self.tree.pack(fill="both", expand=True, padx=4, pady=4)
            self.tree.column("#0", width=460)
            self.tree.bind("<Double-1>", self._on_double)
            self.tree.bind("<Return>", self._on_double)

            # 底部：日志 + 状态栏
            logf = tk.LabelFrame(self.root, text=" 执行日志 ", bg=CLR["card"],
                                 fg=ACCENT_DARK, font=("Noto Sans CJK SC", 10, "bold"))
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
            self.q.put(("env", detect()))

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
                    self.tree.insert(f"g:{g}", "end", iid=f"i:{it['id']}",
                                     text=f"{it['id']:>2}.  {label}")
            self.tree.tag_configure("group", font=("Noto Sans CJK SC", 10, "bold"),
                                    foreground=ACCENT_DARK)

        # ---- 交互 ----
        def _sel_item(self) -> Item | None:
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
                txt = scrolledtext.ScrolledText(
                    win, wrap="word", font=("Noto Sans Mono CJK SC", 10))
                txt.pack(fill="both", expand=True)
                try:
                    txt.insert("1.0", path.read_text(errors="replace"))
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
                # 内部实现（17 install_steam_fix / 25 launch_underlords）
                import contextlib
                import io as _io

                buf = _io.StringIO()
                try:
                    with contextlib.redirect_stdout(buf):
                        if item_id == 17:
                            code = install_steam_fix()
                        elif item_id == 25:
                            code = launch_underlords()
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

    return run_gui


# ---------------------------------------------------------------- 入口分发


def main(argv: list[str]) -> int:
    args = [a for a in argv[1:]]

    if args and args[0] == "--detect":
        env = detect()
        print(report_json(env) if "--json" in args else render_report(env))
        return 0

    if args and args[0].isdigit():
        item_id = int(args[0])
        print(render_report(detect()))
        return run_item(item_id, args[1:])

    if args and args[0] in ("-h", "--help"):
        print(__doc__)
        return 0

    # 交互模式：Tkinter 桌面窗口（无法启动时直接报错退出，不降级）
    run_gui = _load_gui()
    if run_gui is None:
        print("错误：无法启动图形界面（Tkinter 桌面窗口）。", file=sys.stderr)
        print("  可能原因：", file=sys.stderr)
        print("    - 无显示环境（SSH 远程请用 X11 转发，或在本机桌面运行）",
              file=sys.stderr)
        print("    - 未安装 tkinter（Ubuntu：sudo apt-get install python3-tk）",
              file=sys.stderr)
        print("  非交互替代：--detect / --detect --json / <菜单编号>", file=sys.stderr)
        return 1
    run_gui()
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv))
    except KeyboardInterrupt:
        print("\n再见。")
        sys.exit(130)
