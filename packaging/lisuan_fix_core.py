#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""lisuan_fix_core — 砺算 7G100 修复工具核心逻辑（无 UI）。

包含：基础工具、环境检测、菜单定义、修复执行、命令行入口 main()。
界面在 lisuan_fix_gui.py（Tkinter），入口壳在 install.py。

依赖：仅标准库。
"""

from __future__ import annotations

import json
import os
import shlex
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, TypedDict


def _find_base() -> Path:
    """定位资源根目录（dkms/ steam/ 等子目录所在的目录）。

    - 开发模式：本文件位于 packaging/，仓库根是其父目录；
    - deb 安装模式：资源在 /usr/share/lisuan-fix/；
    - 兼容：本文件被直接放在仓库根时，here 即为资源根。
    """
    here = Path(__file__).resolve().parent
    candidates = [
        here.parent,                     # packaging/ -> 仓库根
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
STEAM_WRAP = Path.home() / ".local/share/Steam/ubuntu12_64/steamwebhelper_sniper_wrap.sh"

# Navicat 18 Premium Lite（AppImage）——与 GPU 无关的应用层修复，见 navicat/
# NAVICAT_APPIMAGE 环境变量可覆盖默认路径（与 navicat/fix-navicat-appimage.sh 一致）
NAVICAT_APPIMAGE = Path(os.environ.get("NAVICAT_APPIMAGE")
                        or (Path.home() / "development/navicat18-premium-lite-cs-x86_64.AppImage"))
# 功能不变量：AppRun 含此行即视为已修复（与 navicat/fix-navicat-appimage.sh --status 同源）
NAVICAT_FIXLINE = "export QT_IM_MODULE=compose"

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


def _steam_exec_value(content: str) -> str:
    """取桌面文件第一个 Exec= 行的值（无 Exec 行返回 ""）。"""
    for line in content.splitlines():
        s = line.strip()
        if s.startswith("Exec="):
            return s[len("Exec="):].strip()
    return ""


def _steam_exec_tokens(content: str) -> list[str]:
    ex = _steam_exec_value(content)
    if not ex:
        return []
    try:
        return shlex.split(ex)
    except ValueError:
        return ex.split()


def _steam_exe_index(toks: list[str]) -> int | None:
    """steam 可执行文件在 token 列表中的下标（找不到返回 None）。"""
    return next((i for i, t in enumerate(toks)
                 if t == "steam" or t.endswith("/steam")), None)


def steam_desktop_fixed(content: str) -> bool:
    """桌面启动器是否已把 GBM_BACKEND=dri 注入 steam 启动命令。

    判定"Exec 行里、steam 可执行文件之前是否存在 GBM_BACKEND=dri 环境
    赋值"，不做整行精确匹配——用户完全可以按自己的需要在 env 段叠加
    其它变量（如 `LC_ALL=en_US.UTF-8`），那是合法定制，检测必须容忍，
    否则会把"能正常打开的 Steam"误报成"需修复"。
    """
    toks = _steam_exec_tokens(content)
    exe_idx = _steam_exe_index(toks)
    if exe_idx is None:
        return False
    return "GBM_BACKEND=dri" in toks[:exe_idx]


def _build_fixed_exec(content: str) -> tuple[str, bool]:
    """在原 Exec 行基础上注入 GBM_BACKEND=dri，保留用户其它 env 定制。

    返回 (新 Exec 行值, 是否发生变化)。无 Exec 行时返回 ("", False)。
    """
    toks = _steam_exec_tokens(content)
    if not toks:
        return "", False
    if "GBM_BACKEND=dri" in toks:
        return _steam_exec_value(content), False
    if toks[0] == "env":
        new = ["env", "GBM_BACKEND=dri", *toks[1:]]
    else:
        new = ["env", "GBM_BACKEND=dri", *toks]
    return " ".join(new), True


def _strip_fixed_exec(content: str) -> str | None:
    """从 Exec 行摘除 GBM_BACKEND=dri（_build_fixed_exec 的逆操作）。

    无 Exec 行返回 None。摘除后若 env 前缀下已无其它赋值，连 env 一起去掉，
    避免残留空的 `env`。
    """
    toks = _steam_exec_tokens(content)
    if not toks:
        return None
    rest = [t for t in toks if t != "GBM_BACKEND=dri"]
    exe_idx = _steam_exe_index(rest)
    if (rest and rest[0] == "env" and exe_idx is not None
            and not any("=" in t for t in rest[1:exe_idx])):
        rest.pop(0)
    return " ".join(rest)


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
                "检测到全局 lisuan_dri.so 软链（历史方案残留）—— 它会使 Steam 客户端 "
                "double free，请删除（仅限指向 gf_* 的软链）：\n"
                "    sudo rm /usr/lib/x86_64-linux-gnu/dri/lisuan_*.so && sudo ldconfig")
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
            if steam_desktop_fixed(dcontent):
                env.add("Steam", "ok", "桌面启动器已配置 GBM_BACKEND=dri（CEF 崩溃已修复）")
            else:
                env.add("Steam", "bad",
                        "桌面启动器缺 GBM_BACKEND=dri —— Steam 界面会崩溃，菜单 [12]", "12")

        if STEAM_WRAP.is_file():
            try:
                wcontent = STEAM_WRAP.read_text(errors="replace")
            except OSError:
                wcontent = ""
            if "libglapi_gf" in wcontent or "GBM_BACKEND=dri" in wcontent:
                env.add("Steam", "bad",
                        "wrap.sh 仍含旧方案残留注入 —— 会让 Steam 客户端 double free，"
                        "请运行 sudo bash steam/fix-steam-cef-lock.sh --uninstall 清理"
                        "（历史方案，已不占菜单）")

            attr = out(["lsattr", str(STEAM_WRAP)])
            flags = attr.split()[0] if attr else ""
            if "i" in flags:
                env.add("Steam", "bad",
                        "wrap.sh 被 chattr +i 锁定 —— 会触发 Steam 更新回滚循环，"
                        "导致游戏反复重新下载，请立即解锁（python3 install.py 12 --undo）", "12")
    else:
        env.add("Steam", "info", "未安装 Steam（跳过 Steam 相关项）")

    if ok_cmd(["systemctl", "is-enabled", "lisuan-steam-autofix.service"]):
        env.add("自愈", "warn",
                "开机自愈服务仍在启用 —— 新方案（环境变量）不需要它，请运行：\n"
                "    sudo systemctl disable --now lisuan-steam-autofix.service && "
                "sudo rm -f /etc/systemd/system/lisuan-steam-autofix.service")

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
                "Dota Underlords 修复已安装（Steam 内直接点开始游戏，或用桌面项）")
    elif ul.is_dir():
        env.add("游戏", "warn", "Dota Underlords 修复未安装 —— 菜单 [16]", "16")

    kr = Path.home() / ".local/share/Steam/steamapps/common/Kingdom Rush"
    krsh = kr / "run.sh"
    kr_injected = False
    if krsh.is_file():
        try:
            kr_injected = "Lisuan 7G100 fix (injected)" in krsh.read_text(errors="replace")
        except OSError:
            kr_injected = False
    if kr_injected:
        env.add("游戏", "ok",
                "Kingdom Rush 修复已安装（容器内自动委托到宿主直跑，修 GLXBadFBConfig）")
    elif kr.is_dir():
        env.add("游戏", "warn",
                "Kingdom Rush 修复未安装（Steam 启动会报 GLXBadFBConfig）—— 菜单 [18]", "18")

    # Navicat（与 GPU 无关的应用层修复，AppImage 不在时跳过）
    if NAVICAT_APPIMAGE.is_file():
        if _navicat_fixed():
            env.add("应用", "ok",
                    "Navicat AppImage 修复已安装（fcitx 会话下启动不再闪退）")
        else:
            env.add("应用", "warn",
                    "Navicat AppImage 未修复（QT_IM_MODULE=fcitx 会话下启动即闪退）"
                    " —— 菜单 [20]", "20")

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
#      -> 随从大师 -> 刀塔霸业 -> 王国保卫战 -> Navicat（新类别追加在末尾）；
#   3) 每个类别内严格按"安装/修复 -> 卸载 -> 说明"排列。
#   4) 历史"建立 lisuan_dri.so 软链"方案整体退出菜单：宿主 Mesa 由 drirc
#      （01-gf_drv.conf 的 kernel_driver/dri_driver 映射）天然找到 gf 驱动，
#      Steam 走 GBM_BACKEND=dri，全局软链反而会使 Steam 客户端 double free。
#      修复/清理脚本保留在 mesa/ 作手动工具，不再占用菜单编号。
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
    {"id": 5,  "title": "说明：厂商 gf/gbm 崩溃 Bug 报告", "group": "DKMS",
     "root": False, "cmd": None, "doc": _doc("dkms/vendor-bug-report-gf-gbm-crash.md")},

    # ---- Mesa ----
    {"id": 6,  "title": "说明：系统 Mesa 兼容性分析", "group": "Mesa",
     "root": False, "cmd": None, "doc": _doc("mesa/system-mesa-analysis.md")},

    # ---- Electron ----
    # 每项 = 把应用菜单图标的 Exec 切到本 wrapper：以后从应用菜单点图标即自动
    # 带修复参数（ZCode）或 gf 硬件加速（CodeBuddy/Qoder），不必再手动跑脚本。
    # 只装桌面项、不启动应用（wrapper 的 --start 可让装完即启动，菜单不使用）。
    {"id": 7,  "title": "ZCode：点图标即用（装桌面项；修显示异常，软件渲染稳定优先）",
     "group": "Electron", "root": False,
     "cmd": lambda a: _sh("electron/use-zcode.sh", "--install-desktop"),
     "doc": ""},
    {"id": 8,  "title": "CodeBuddy（含国内版 buddycn）：点图标即用（装桌面项；gf 硬件加速）",
     "group": "Electron", "root": False,
     "cmd": lambda a: _sh("electron/use-codebuddy.sh", "--install-desktop"),
     "doc": ""},
    {"id": 9,  "title": "Qoder：点图标即用（装桌面项；gf 硬件加速）",
     "group": "Electron", "root": False,
     "cmd": lambda a: _sh("electron/use-qoder.sh", "--install-desktop"),
     "doc": ""},
    {"id": 10, "title": "Trae：点图标即用（装桌面项；软件渲染稳定优先，绕开 gf EGL 显示错误）",
     "group": "Electron", "root": False,
     "cmd": lambda a: _sh("electron/use-trae.sh", "--install-desktop"),
     "doc": ""},
    {"id": 11, "title": "说明：Electron 类应用适配（显示异常 / 硬件加速）", "group": "Electron",
     "root": False, "cmd": None, "doc": _doc("electron/README.md")},

    # ---- Steam ----
    {"id": 12, "title": "安装/修复：Steam 界面崩溃（桌面启动器写入 GBM_BACKEND=dri）",
     "group": "Steam", "root": False, "cmd": None, "doc": ""},  # install_steam_fix
    {"id": 13, "title": "说明：Steam 在砺算上的适配细节（容器/CEF/Proton）", "group": "Steam",
     "root": False, "cmd": None, "doc": _doc("steam/steam-on-lisuan.md")},

    # ---- 随从大师 ----
    {"id": 14, "title": "安装/修复：硬件渲染加固（libSC.so 链接 + DXVK 降级 + hook）",
     "group": "随从大师", "root": True,
     "cmd": lambda a: _sh("minion-masters/fix-minion-masters-7g100-hw.sh", *a), "doc": ""},
    {"id": 15, "title": "说明：随从大师（Minion Masters）修复文档", "group": "随从大师",
     "root": False, "cmd": None, "doc": _doc("minion-masters/fix-minion-masters.md")},

    # ---- 刀塔霸业 ----
    {"id": 16, "title": "安装/修复：Dota Underlords 补丁集（容器委托+重试+libpng12+桌面项）",
     "group": "刀塔霸业", "root": True,
     "cmd": lambda a: _sh("underlords/fix-underlords-native.sh"), "doc": ""},
    {"id": 17, "title": "说明：Dota Underlords 修复文档", "group": "刀塔霸业",
     "root": False, "cmd": None, "doc": _doc("underlords/fix-underlords-native.md")},

    # ---- 王国保卫战 ----
    {"id": 18, "title": "安装/修复：Kingdom Rush（修 GLXBadFBConfig：容器委托宿主直跑 + 补 64 位 libopenal）",
     "group": "王国保卫战", "root": True,
     "cmd": lambda a: _sh("kingdom-rush/fix-kingdom-rush.sh"), "doc": ""},
    {"id": 19, "title": "说明：Kingdom Rush 修复文档", "group": "王国保卫战",
     "root": False, "cmd": None, "doc": _doc("kingdom-rush/README.md")},

    # ---- Navicat（与 GPU 无关：AppImage 打包缺陷，见 navicat/README.md）----
    {"id": 20, "title": "安装/修复：Navicat AppImage 启动闪退（重打包注入 QT_IM_MODULE=compose）",
     "group": "Navicat", "root": False,
     "cmd": lambda a: _sh("navicat/fix-navicat-appimage.sh", *a), "doc": ""},
    {"id": 21, "title": "说明：Navicat AppImage 启动闪退修复文档", "group": "Navicat",
     "root": False, "cmd": None, "doc": _doc("navicat/README.md")},
]

ITEMS_BY_ID = {it["id"]: it for it in ITEMS}
GROUP_ORDER = ["环境检测", "DKMS", "Mesa", "Electron", "Steam", "随从大师", "刀塔霸业",
               "王国保卫战", "Navicat"]


class UndoAction(TypedDict):
    """安装项的卸载（还原）动作。

    cmd 为空表示内部实现函数（见 undo_item）；
    always 为 True 时无视状态始终显示按钮（卸载脚本幂等，如 DKMS）。
    """

    cmd: list[str]
    root: bool
    always: bool


# 安装项 -> 卸载动作。GUI 在项状态为"已配置/已修复/有残留"时显示卸载按钮。
UNDO: dict[int, UndoAction] = {
    4:  {"cmd": _sh("dkms/uninstall-lisuan-gpu.sh"), "root": True, "always": True},
    7:  {"cmd": _sh("electron/use-zcode.sh", "--uninstall-desktop"), "root": False,
         "always": False},
    8:  {"cmd": _sh("electron/use-codebuddy.sh", "--uninstall-desktop"),
         "root": False, "always": False},
    9:  {"cmd": _sh("electron/use-qoder.sh", "--uninstall-desktop"), "root": False,
         "always": False},
    10: {"cmd": _sh("electron/use-trae.sh", "--uninstall-desktop"), "root": False,
         "always": False},
    12: {"cmd": [], "root": True, "always": False},   # remove_steam_fix（内部实现）
    14: {"cmd": _sh("minion-masters/fix-minion-masters-7g100-hw.sh", "--uninstall"),
         "root": True, "always": False},
    16: {"cmd": _sh("underlords/fix-underlords-native.sh", "--uninstall"),
         "root": True, "always": False},
    18: {"cmd": _sh("kingdom-rush/fix-kingdom-rush.sh", "--uninstall"),
         "root": True, "always": False},
    20: {"cmd": _sh("navicat/fix-navicat-appimage.sh", "--restore"),
         "root": False, "always": False},
}

# 显示卸载按钮的状态（"已生效"类的状态），见 collect_statuses 的文本
UNDO_SHOWN_WHEN = ("✓ 已配置", "✓ 已修复", "✓ 已部署", "有残留", "启用中")


def undo_item(item_id: int) -> int:
    """执行安装项的卸载/还原（命令行模式）。返回进程返回码。"""
    act = UNDO.get(item_id)
    if act is None:
        print(f"  菜单项 {item_id} 无卸载动作")
        return 1
    if not act["cmd"]:
        return remove_steam_fix() if item_id == 12 else 1
    cmd = [*act["cmd"]]
    if act["root"] and os.geteuid() != 0:
        cmd = ["sudo", *cmd]
    print(f"  还原：{' '.join(cmd)}")
    return subprocess.run(cmd).returncode


def item_label(it: Item) -> str:
    tag = " [需要 sudo]" if it["root"] else ""
    return f"{it['id']:>2}) {it['title']}{tag}"


# ---------------------------------------------------------------- Steam 修复


def _replace_exec_line(content: str, new_exec: str) -> str:
    """替换桌面文件第一个 Exec= 行的值，其余内容原样保留。"""
    lines = content.splitlines(keepends=True)
    for i, ln in enumerate(lines):
        if ln.strip().startswith("Exec="):
            eol = "\n" if ln.endswith("\n") else ""
            lines[i] = f"Exec={new_exec}{eol}"
            break
    return "".join(lines)


def install_steam_fix() -> int:
    """把 GBM_BACKEND=dri 写入 Steam 桌面启动器（新方案，不修改 Steam 文件）。

    只改 Exec 行并保留用户其它定制（如 LC_ALL=en_US.UTF-8）；文件不存在时
    才写完整模板。重复执行幂等。
    """
    STEAM_DESKTOP.parent.mkdir(parents=True, exist_ok=True)
    if not STEAM_DESKTOP.is_file():
        try:
            STEAM_DESKTOP.write_text(STEAM_DESKTOP_BODY_FIXED, encoding="utf-8")
        except OSError as e:
            print(f"  写入失败：{e}")
            return 1
        print(f"  已创建 {STEAM_DESKTOP}")
        print("  Steam 界面崩溃修复完成（下次从菜单启动 Steam 生效）。")
        return 0

    try:
        content = STEAM_DESKTOP.read_text(errors="replace")
    except OSError as e:
        print(f"  读取失败：{e}")
        return 1
    if steam_desktop_fixed(content):
        print(f"  {STEAM_DESKTOP} 已含 GBM_BACKEND=dri，无需改动。")
        return 0

    new_exec, changed = _build_fixed_exec(content)
    if not changed:
        # 无 Exec 行的异常文件：退回完整模板
        try:
            shutil.copy2(STEAM_DESKTOP, str(STEAM_DESKTOP) + ".bak")
            print(f"  已备份原启动器 -> {STEAM_DESKTOP}.bak")
            STEAM_DESKTOP.write_text(STEAM_DESKTOP_BODY_FIXED, encoding="utf-8")
        except OSError as e:
            print(f"  写入失败：{e}")
            return 1
        print(f"  原文件无 Exec 行，已重写为完整模板：{STEAM_DESKTOP}")
        print("  Steam 界面崩溃修复完成（下次从菜单启动 Steam 生效）。")
        return 0

    try:
        shutil.copy2(STEAM_DESKTOP, str(STEAM_DESKTOP) + ".bak")
        print(f"  已备份原启动器 -> {STEAM_DESKTOP}.bak")
        STEAM_DESKTOP.write_text(_replace_exec_line(content, new_exec),
                                 encoding="utf-8")
    except OSError as e:
        print(f"  写入失败：{e}")
        return 1
    print(f"  已更新 {STEAM_DESKTOP} 的 Exec 行（保留你的其它启动参数）：")
    print(f"    Exec={new_exec}")
    print("  Steam 界面崩溃修复完成（下次从菜单启动 Steam 生效）。")
    return 0


def remove_steam_fix() -> int:
    """移除 Steam 修复：从 Exec 行摘除 GBM_BACKEND=dri，解锁残留 chattr +i。

    只动 Exec 行，保留用户其它 env 定制（如 LC_ALL=en_US.UTF-8）；
    Exec 行缺失/无法解析时才回退到备份还原或纯净模板。
    """
    if STEAM_WRAP.is_file():
        pre = [] if os.geteuid() == 0 else ["sudo"]
        r = run([*pre, "chattr", "-i", str(STEAM_WRAP)])
        print(f"  解锁：{'完成' if r[0] == 0 else '跳过（未锁定/文件系统不支持）'}")

    if not STEAM_DESKTOP.is_file():
        print("  未找到桌面启动器，无需还原。")
        return 0
    try:
        content = STEAM_DESKTOP.read_text(errors="replace")
    except OSError as e:
        print(f"  读取失败：{e}")
        return 1
    if not steam_desktop_fixed(content):
        print("  启动器未含 GBM_BACKEND=dri，无需还原。")
        return 0

    new_exec = _strip_fixed_exec(content)
    if new_exec is not None:
        try:
            STEAM_DESKTOP.write_text(_replace_exec_line(content, new_exec),
                                     encoding="utf-8")
        except OSError as e:
            print(f"  写入失败：{e}")
            return 1
        print(f"  已从 Exec 行摘除 GBM_BACKEND=dri：{STEAM_DESKTOP}")
        print(f"    Exec={new_exec}")
        return 0

    # 无 Exec 行（异常文件）：回退历史行为——干净备份还原，否则纯净模板
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
    return 0


# ---------------------------------------------------------------- 执行


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
    if item_id == 12:
        return install_steam_fix()

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


# ---------------------------------------------------------------- 修复状态


def _navicat_fixed() -> bool:
    """Navicat AppImage 是否已修复（内部 AppRun 含 export QT_IM_MODULE=compose）。

    与 navicat/fix-navicat-appimage.sh --status 同一判定：AppImage type2 =
    [runtime ELF][squashfs]，runtime 内部数据里也可能出现 'hsqs' 魔数（假阳性），
    逐个候选交给 unsquashfs -s 验真，取第一个有效偏移，再提取 AppRun 查功能不变量。
    """
    f = NAVICAT_APPIMAGE
    if not f.is_file():
        return False
    if not shutil.which("unsquashfs"):
        # 没有 unsquashfs 时退回备份启发：.bak 仅在修复时创建、还原时被消费
        return Path(str(f) + ".bak").is_file()

    off = ""
    for line in run(["grep", "-abo", "hsqs", str(f)])[1].splitlines():
        cand = line.split(":", 1)[0]
        if cand.isdigit() and "Found a valid SQUASHFS" in run(
                ["unsquashfs", "-s", "-o", cand, str(f)])[1]:
            off = cand
            break
    if not off:
        return False

    tmp = tempfile.mkdtemp(prefix="navicat-probe.")
    try:
        run(["unsquashfs", "-o", off, "-d", tmp, "-f", str(f), "AppRun"])
        apprun = Path(tmp) / "AppRun"
        return apprun.is_file() and NAVICAT_FIXLINE in apprun.read_text(errors="replace")
    except OSError:
        return False
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def _desktop_configured(paths: list[Path], marker: str) -> bool:
    """任一 desktop 文件存在且含 marker（如 wrapper 脚本名）即视为已配置。"""
    for p in paths:
        try:
            if p.is_file() and marker in p.read_text(errors="replace"):
                return True
        except OSError:
            pass
    return False


def collect_statuses(env: Env | None = None) -> dict[int, tuple[str, str]]:
    """各菜单项的修复状态：{item_id: (状态文本, 级别)}。

    级别：ok=已就绪，bad=需修复，warn=需注意，dim=不适用/可选/按需。
    说明与纯操作项（1/5/6/11/13/15/17/19/21）不返回状态。
    """
    env = env or detect()
    st: dict[int, tuple[str, str]] = {}
    home = Path.home()

    def agg(cats: list[str]) -> str:
        levels = [c.level for c in env.checks if c.category in cats]
        if "bad" in levels:
            return "bad"
        if "warn" in levels:
            return "warn"
        return "ok" if levels else "dim"

    mark = {"ok": "✓ 正常", "bad": "✗ 异常", "warn": "! 警告", "dim": "– 不适用"}
    st[2] = (mark[agg(["GPU", "驱动包", "模块", "DRM"])],
             agg(["GPU", "驱动包", "模块", "DRM"]))
    st[3] = (mark[agg(["DRI"])], agg(["DRI"]))

    # 4 DKMS：已加载 > vermagic > 未构建
    ko = ""
    moddir = Path(f"/lib/modules/{env.kernel}")
    if moddir.is_dir():
        ko = next(iter(sorted(moddir.rglob("gf.ko*"))), "")
    ko_ver = ""
    if ko:
        for ln in out(["modinfo", str(ko)]).splitlines():
            if ln.startswith("vermagic"):
                parts = ln.split()
                ko_ver = parts[1] if len(parts) > 1 else ""
                break
    if "gf " in out(["lsmod"]):
        st[4] = ("✓ 已加载", "ok")
    elif not ko:
        st[4] = ("! 未构建", "warn")
    elif ko_ver != env.kernel:
        st[4] = ("✗ vermagic 错配", "bad")
    else:
        st[4] = ("! 未加载", "warn")

    st[5] = ("按需", "dim")

    # 8/9/10 Electron 桌面项是否切到 wrapper。
    # 注：CodeBuddy/Qoder 的桌面项 Exec 实际指向通用 wrapper（use-electron-gf.sh
    # --app <name>），因为 use-electron-gf.sh 内 SELF 解析为它自己，故检测标记
    # 用 use-electron-gf.sh 而非 use-codebuddy.sh / use-qoder.sh。
    user_apps = home / ".local/share/applications"
    sys_apps = Path("/usr/share/applications")
    st[7] = (("✓ 已配置", "ok")
             if _desktop_configured([user_apps / "zcode.desktop"], "use-zcode.sh")
             else ("未配置", "dim"))
    # CodeBuddy 桌面项：国际版 codebuddy.desktop / 国内版 buddycn.desktop
    st[8] = (("✓ 已配置", "ok")
             if _desktop_configured([user_apps / "codebuddy.desktop",
                                     sys_apps / "codebuddy.desktop",
                                     user_apps / "buddycn.desktop",
                                     sys_apps / "buddycn.desktop"],
                                    "use-electron-gf.sh")
             else ("未配置", "dim"))
    qoder = [user_apps / n for n in ("qoder.desktop", "qoder-cn.desktop")]
    qoder += [sys_apps / n for n in ("qoder.desktop", "qoder-cn.desktop")]
    st[9] = (("✓ 已配置", "ok")
             if _desktop_configured(qoder, "use-electron-gf.sh")
             else ("未配置", "dim"))
    # Trae 桌面项：国际版 trae.desktop / 国内版 trae-cn.desktop
    trae = [user_apps / n for n in ("trae.desktop", "trae-cn.desktop")]
    trae += [sys_apps / n for n in ("trae.desktop", "trae-cn.desktop")]
    st[10] = (("✓ 已配置", "ok") if _desktop_configured(trae, "use-electron-gf.sh")
              else ("未配置", "dim"))

    # 12 Steam（锁定检查与已修复判定同源，合并为一项状态）
    wrap_txt = ""
    if STEAM_WRAP.is_file():
        try:
            wrap_txt = STEAM_WRAP.read_text(errors="replace")
        except OSError:
            wrap_txt = ""
    locked = "i" in ((out(["lsattr", str(STEAM_WRAP)]).split() or [""])[0])
    if not shutil.which("steam"):
        st[12] = ("未安装", "dim")
    elif locked:
        st[12] = ("! 文件被锁定", "bad")
    else:
        try:
            fixed = (STEAM_DESKTOP.is_file()
                     and steam_desktop_fixed(
                         STEAM_DESKTOP.read_text(errors="replace")))
        except OSError:
            fixed = False
        st[12] = ("✓ 已修复", "ok") if fixed else ("✗ 需修复", "bad")

    # 14 Minion Masters：以 Steam 配置层的"实际生效标志"判定
    # （与修复的安装位置无关——deb 安装版/源码仓库均通用）：
    #   a. localconfig.vdf 的 489520 LaunchOptions 含 osrelease_redirect（LD_PRELOAD 生效）
    #   b. 游戏目录 doorstop_config.ini 的 target_assembly = MMPatch.dll（hook 生效）
    ul_mm = home / ".steam/root/steamapps/common/Minion Masters"
    ud = home / ".local/share/Steam/userdata"
    # 遍历所有 Steam 用户的 localconfig（含 anonymous）——任一命中即视为生效
    launched = False
    if ud.is_dir():
        for p in ud.glob("*/config/localconfig.vdf"):
            try:
                if "osrelease_redirect" in p.read_text(errors="replace"):
                    launched = True
                    break
            except OSError:
                pass
    doorstop = False
    try:
        doorstop = ((ul_mm / "doorstop_config.ini").is_file()
                    and "MMPatch.dll" in (ul_mm / "doorstop_config.ini").read_text(
                        errors="replace"))
    except OSError:
        doorstop = False
    if not ul_mm.is_dir():
        st[14] = ("未安装", "dim")
    elif launched and doorstop:
        st[14] = ("✓ 已部署", "ok")
    else:
        st[14] = ("✗ 需修复", "bad")

    # 15 Underlords：注入标记
    ul_root = home / ".local/share/Steam/steamapps/common/Underlords"
    if not ul_root.is_dir():
        st[16] = ("未安装", "dim")
    else:
        try:
            injected = ("Lisuan 7G100 fix (injected)"
                        in (ul_root / "game/underlords.sh").read_text(errors="replace"))
        except OSError:
            injected = False
        st[16] = ("✓ 已修复", "ok") if injected else ("✗ 需修复", "bad")

    # 18 Kingdom Rush：run.sh 注入标记
    kr_root = home / ".local/share/Steam/steamapps/common/Kingdom Rush"
    if not kr_root.is_dir():
        st[18] = ("未安装", "dim")
    else:
        try:
            kr_inj = ("Lisuan 7G100 fix (injected)"
                      in (kr_root / "run.sh").read_text(errors="replace"))
        except OSError:
            kr_inj = False
        st[18] = ("✓ 已修复", "ok") if kr_inj else ("✗ 需修复", "bad")

    # 20 Navicat：AppImage 存在且内部 AppRun 已注入（判定与脚本 --status 同源）
    if not NAVICAT_APPIMAGE.is_file():
        st[20] = ("未找到 AppImage", "dim")
    elif _navicat_fixed():
        st[20] = ("✓ 已修复", "ok")
    else:
        st[20] = ("✗ 需修复", "bad")

    return st


# ---------------------------------------------------------------- 入口分发


def _gui_unavailable(reason: str) -> None:
    print("错误：无法启动图形界面（Tkinter 桌面窗口）。", file=sys.stderr)
    if reason:
        print(f"  原因：{reason}", file=sys.stderr)
    print("  可能原因：", file=sys.stderr)
    print("    - 无显示环境（SSH 远程请用 X11 转发，或在本机桌面运行）",
          file=sys.stderr)
    print("    - 未安装 tkinter（Ubuntu：sudo apt-get install python3-tk）",
          file=sys.stderr)
    print("  非交互替代：--detect / --detect --json / <菜单编号>", file=sys.stderr)


def main(argv: list[str]) -> int:
    args = [a for a in argv[1:]]

    if args and args[0] == "--detect":
        env = detect()
        print(report_json(env) if "--json" in args else render_report(env))
        return 0

    if args and args[0].isdigit():
        item_id = int(args[0])
        print(render_report(detect()))
        if "--undo" in args:
            return undo_item(item_id)
        return run_item(item_id, args[1:])

    if args and args[0] in ("-h", "--help"):
        print(__doc__)
        return 0

    # 交互模式：Tkinter 桌面窗口（无法启动时直接报错退出，不降级）
    try:
        from lisuan_fix_gui import gui_available, run_gui
    except ImportError as e:
        _gui_unavailable(f"tkinter 模块缺失（{e}）")
        return 1
    if not gui_available():
        _gui_unavailable("")
        return 1
    run_gui()
    return 0
