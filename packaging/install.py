#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""砺算 7G100 修复工具（Python 版，替代 lisuan-fix.sh）。

用法（本文件位于 packaging/，从仓库根运行）：
    python3 packaging/install.py                 交互界面（有 Textual 用 TUI，否则纯文本菜单）
    python3 packaging/install.py --detect        只检测并打印报告
    python3 packaging/install.py --detect --json 检测并输出 JSON（便于脚本化）
    python3 packaging/install.py 10 [参数]       非交互直执行第 10 项（修复 Steam）
    python3 packaging/install.py -h              显示本帮助

菜单结构：环境检测单独一节，其余按类别分组（DKMS / Mesa / Steam / 随从大师 /
刀塔霸业 / Electron），类内统一按"安装/修复 → 体检 → 卸载 → 说明"排列。
编号已于 2026-09-03 重排，与旧版 lisuan-fix.sh 不再一一对应。

依赖：仅标准库。Textual 为可选增强（无它自动降级为纯文本菜单）。

本文件由 lisuan_fix.py + tui.py 合并而来（单文件分发）。两者原先通过
importlib 动态互导以断开导入环；合并后直接引用模块级符号，Textual 相关
代码下沉到 _load_tui() 内部延迟定义，未安装 textual 时不影响主流程。

2026-09-01 修正：Steam CEF 崩溃的正确解法是 `GBM_BACKEND=dri` 环境变量
（只写桌面启动器，不修改 Steam 自身文件）。旧方案（向
steamwebhelper_sniper_wrap.sh 注入 LD_PRELOAD 并 chattr +i 锁定）已被证明
有害——会让 Steam 客户端 double free，且锁定会触发 Steam 更新回滚循环，
导致游戏被反复重新下载。详见 steam/steam-on-lisuan.md §3、§10。
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

# Textual 的 write/append/clear/run_worker 返回 awaitable，惯例是直接忽略；
# CSS/BINDINGS/title 等属性按 Textual 约定直接赋值。这两类写法会触发
# basedpyright 的严格规则，故对本文件放宽。
# pyright: reportUnusedCallResult=false, reportUnannotatedClassAttribute=false

def _find_base() -> Path:
    """定位资源根目录（dkms/ steam/ 等子目录所在的目录）。

    - 开发模式：本文件位于 packaging/，仓库根是其**父目录**
      （如 ~/project/lisuan/packaging/install.py -> ~/project/lisuan）；
    - deb 安装模式：本文件位于 /usr/bin/，资源在 /usr/share/lisuan-fix/；
    - 兼容：本文件被直接放在仓库根时，here 即为资源根。
    """
    here = Path(__file__).resolve().parent
    candidates = [
        here,
        here.parent,                       # 安装在 /usr/bin 时
        Path("/usr/share/lisuan-fix"),     # deb 标准资源路径
    ]
    for c in candidates:
        if (c / "dkms").is_dir() and (c / "steam").is_dir():
            return c
    return here


REPO_ROOT = _find_base()
S = REPO_ROOT

# Steam 桌面启动器（新方案的落点：只改这里，不碰 Steam 自己的文件）
STEAM_DESKTOP = Path.home() / ".local/share/applications/steam.desktop"
STEAM_DESKTOP_EXEC_FIXED = "Exec=env GBM_BACKEND=dri /usr/bin/steam %U"
STEAM_WRAP = Path.home() / ".local/share/Steam/ubuntu12_64/steamwebhelper_sniper_wrap.sh"

# 两个标准模板：修复版 / 纯净版。install/remove 都直接写模板，
# 不做字符串拼接/替换，保证状态可预测（注释等修饰内容随之整体增删）。
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

    # 2026-09-01 修正：全局 lisuan_dri.so 软链会让 Steam 客户端加载 gf 驱动而
    # double free 崩溃，属"有害残留"，检测到应提示移除（而非像旧版那样判为 ok）。
    dri_dir = Path("/usr/lib/x86_64-linux-gnu/dri")
    if (dri_dir / "lisuan_dri.so").exists():
        env.add("DRI", "warn",
                "检测到全局 lisuan_dri.so 软链 —— 它会使 Steam 客户端 double free，"
                "建议移除（菜单 [8]）", "8")
    elif (dri_dir / "gf_dri.so").exists():
        env.add("DRI", "ok", "gf_dri.so 存在（新方案下无需全局软链）")
    else:
        env.add("DRI", "warn", "未见 gf_dri.so（lisuangpu-lib 未安装？）")

    # ---- Steam ----
    if shutil.which("steam"):
        env.add("Steam", "ok", "Steam 已安装")

        # 新方案：桌面启动器带 GBM_BACKEND=dri（精确匹配 Exec 行，
        # 避免把注释中的字样误判为已配置）
        if STEAM_DESKTOP.is_file():
            try:
                dcontent = STEAM_DESKTOP.read_text(errors="replace")
            except OSError:
                dcontent = ""
            if STEAM_DESKTOP_EXEC_FIXED in dcontent:
                env.add("Steam", "ok", "桌面启动器已配置 GBM_BACKEND=dri（CEF 崩溃已修复）")
            else:
                env.add("Steam", "bad",
                        "桌面启动器缺 GBM_BACKEND=dri —— Steam 界面会崩溃，菜单 [10]", "10")

        # 旧方案残留检查：wrap.sh 被注入 / 被 chattr 锁定都是有害的
        if STEAM_WRAP.is_file():
            try:
                wcontent = STEAM_WRAP.read_text(errors="replace")
            except OSError:
                wcontent = ""
            if "libglapi_gf" in wcontent or "GBM_BACKEND=dri" in wcontent:
                env.add("Steam", "bad",
                        "wrap.sh 仍含旧方案残留注入 —— 会让 Steam 客户端 double free，"
                        "请清理（菜单 [12]）", "12")

            attr = out(["lsattr", str(STEAM_WRAP)])
            flags = attr.split()[0] if attr else ""
            if "i" in flags:
                env.add("Steam", "bad",
                        "wrap.sh 被 chattr +i 锁定 —— 会触发 Steam 更新回滚循环，"
                        "导致游戏反复重新下载，请立即解锁（菜单 [11]）", "11")
    else:
        env.add("Steam", "info", "未安装 Steam（跳过 Steam 相关项）")

    # ---- 开机自愈（新方案下已不需要）----
    if ok_cmd(["systemctl", "is-enabled", "lisuan-steam-autofix.service"]):
        env.add("自愈", "warn",
                "开机自愈服务仍在启用 —— 新方案（环境变量）不需要它，建议卸载（菜单 [13]）", "13")

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
                "Dota Underlords 修复已安装（Steam 内直接点开始游戏；菜单 [18]/[19]）")
    elif ul.is_dir():
        env.add("游戏", "warn", "Dota Underlords 修复未安装 —— 菜单 [17]", "17")

    return env


# ---------------------------------------------------------------- 菜单项

#: 命令构造函数：接收额外命令行参数，返回完整 argv
CmdFn = Callable[[list[str]], list[str]]


class Item(TypedDict):
    """一个菜单项。

    cmd 为 None 表示内部实现（见 run_item 的特例分支）；
    doc 非空表示"说明"项，展示对应目录下的 md 文档。
    """

    id: int
    title: str
    group: str
    root: bool
    cmd: CmdFn | None
    doc: str


def _sh(rel: str, *args: str) -> list[str]:
    return ["bash", str(S / rel), *args]


def _doc(rel: str) -> str:
    """说明项：标记要展示的 md（相对仓库根），由 show_doc 渲染。"""
    return rel


# 分组与排列规则：
#   环境检测单独一节；其余按类别（目录）分组，类内固定为
#   "安装/修复 -> 体检 -> 卸载 -> 说明" 的顺序。
# 说明项一律取该类别目录下的 md 文档。
ITEMS: list[Item] = [
    # ---- 环境检测 ----
    {"id": 1,  "title": "重新检测系统环境", "group": "环境检测", "root": False,
     "cmd": None, "doc": ""},

    # ---- DKMS ----
    {"id": 2,  "title": "安装/修复：DKMS 驱动构建与加载（vermagic 错配、黑屏）",
     "group": "DKMS", "root": True,
     "cmd": lambda a: _sh("dkms/fix-lisuan-gpu.sh", *a), "doc": ""},
    {"id": 3,  "title": "体检：GPU 驱动（lspci/lsmod/modinfo/DRM）", "group": "DKMS",
     "root": False, "cmd": lambda a: _sh("dkms/verify-lisuan-gpu.sh"), "doc": ""},
    {"id": 4,  "title": "卸载：DKMS Makefile 补丁（还原原厂，不影响已装驱动）",
     "group": "DKMS", "root": True,
     "cmd": lambda a: _sh("dkms/uninstall-lisuan-gpu.sh"), "doc": ""},
    {"id": 5,  "title": "说明：厂商 gf/gbm 崩溃 Bug 报告", "group": "DKMS",
     "root": False, "cmd": None, "doc": _doc("dkms/vendor-bug-report-gf-gbm-crash.md")},

    # ---- Mesa ----
    {"id": 6,  "title": "安装/修复：DRI 驱动名（仅原生 GL 游戏需要，含 --native-gl）",
     "group": "Mesa", "root": True,
     "cmd": lambda a: _sh("mesa/fix-lisuan-mesa-driver-name.sh", *a), "doc": ""},
    {"id": 7,  "title": "体检：系统 Mesa 兼容性（zink/GBM dri 后端）", "group": "Mesa",
     "root": False, "cmd": lambda a: _sh("mesa/verify-system-mesa.sh"), "doc": ""},
    {"id": 8,  "title": "卸载：DRI 驱动名软链（只删我们建的软链）", "group": "Mesa",
     "root": True, "cmd": lambda a: _sh("mesa/uninstall-lisuan-mesa-driver-name.sh"),
     "doc": ""},
    {"id": 9,  "title": "说明：系统 Mesa 兼容性分析", "group": "Mesa",
     "root": False, "cmd": None, "doc": _doc("mesa/system-mesa-analysis.md")},

    # ---- Steam ----
    {"id": 10, "title": "安装/修复：Steam 界面崩溃（桌面启动器写入 GBM_BACKEND=dri）",
     "group": "Steam", "root": False, "cmd": None, "doc": ""},  # install_steam_fix
    {"id": 11, "title": "卸载：移除 GBM_BACKEND 并解锁 Steam 文件", "group": "Steam",
     "root": True, "cmd": None, "doc": ""},  # remove_steam_fix
    {"id": 12, "title": "清理：旧方案残留（还原 wrap.sh 注入）", "group": "Steam",
     "root": True, "cmd": lambda a: _sh("steam/fix-steam-cef-lock.sh", "--uninstall"),
     "doc": ""},
    {"id": 13, "title": "卸载：开机自愈服务（新方案下已不需要）", "group": "Steam",
     "root": True, "cmd": None, "doc": ""},  # remove_autofix
    {"id": 14, "title": "说明：Steam 在砺算上的适配细节（容器/CEF/Proton）", "group": "Steam",
     "root": False, "cmd": None, "doc": _doc("steam/steam-on-lisuan.md")},

    # ---- 随从大师 ----
    {"id": 15, "title": "安装/修复：硬件渲染加固（libSC.so 链接 + DXVK 降级 + hook）",
     "group": "随从大师", "root": True,
     "cmd": lambda a: _sh("minion-masters/fix-minion-masters-7g100-hw.sh", *a), "doc": ""},
    {"id": 16, "title": "说明：随从大师（Minion Masters）修复文档", "group": "随从大师",
     "root": False, "cmd": None, "doc": _doc("minion-masters/fix-minion-masters.md")},

    # ---- 刀塔霸业 ----
    {"id": 17, "title": "安装/修复：Dota Underlords 补丁集（容器委托+重试+libpng12+桌面项）",
     "group": "刀塔霸业", "root": True,
     "cmd": lambda a: _sh("underlords/fix-underlords-native.sh"), "doc": ""},
    {"id": 18, "title": "启动游戏（宿主直跑，需 Steam 在运行）", "group": "刀塔霸业",
     "root": False, "cmd": None, "doc": ""},  # launch_underlords
    {"id": 19, "title": "查看修复状态", "group": "刀塔霸业", "root": False,
     "cmd": lambda a: _sh("underlords/fix-underlords-native.sh", "--status"), "doc": ""},
    {"id": 20, "title": "说明：Dota Underlords 修复文档", "group": "刀塔霸业",
     "root": False, "cmd": None, "doc": _doc("underlords/fix-underlords-native.md")},

    # ---- Electron ----
    {"id": 21, "title": "启动 ZCode（--no-sandbox + XWayland + 禁用 GPU 合成，修显示异常）",
     "group": "Electron", "root": False, "cmd": lambda a: _sh("electron/use-zcode.sh"),
     "doc": ""},
    {"id": 22, "title": "安装桌面项：ZCode Exec 切到修复 wrapper", "group": "Electron",
     "root": False, "cmd": lambda a: _sh("electron/use-zcode.sh", "--install-desktop"),
     "doc": ""},
    {"id": 23, "title": "启动 CodeBuddy 国内版（gf 硬件加速）", "group": "Electron",
     "root": False, "cmd": lambda a: _sh("electron/use-codebuddy.sh"), "doc": ""},
    {"id": 24, "title": "安装桌面项：CodeBuddy Exec 切到加速 wrapper", "group": "Electron",
     "root": False, "cmd": lambda a: _sh("electron/use-codebuddy.sh", "--install-desktop"),
     "doc": ""},
    {"id": 25, "title": "启动 Qoder 国内版（gf 硬件加速）", "group": "Electron",
     "root": False, "cmd": lambda a: _sh("electron/use-qoder.sh"), "doc": ""},
    {"id": 26, "title": "安装桌面项：Qoder Exec 切到加速 wrapper", "group": "Electron",
     "root": False, "cmd": lambda a: _sh("electron/use-qoder.sh", "--install-desktop"),
     "doc": ""},
]

ITEMS_BY_ID = {it["id"]: it for it in ITEMS}
GROUP_ORDER = ["环境检测", "DKMS", "Mesa", "Steam", "随从大师", "刀塔霸业", "Electron"]


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
    rc = 0

    # 1) 解锁 wrap.sh（若曾被 chattr +i 锁定，会引发 Steam 更新回滚循环）
    if STEAM_WRAP.is_file():
        if os.geteuid() != 0:
            r = run(["sudo", "chattr", "-i", str(STEAM_WRAP)])
        else:
            r = run(["chattr", "-i", str(STEAM_WRAP)])
        if r[0] == 0:
            print(f"  已解锁（如处于锁定状态）：{STEAM_WRAP}")
        else:
            # 文件系统不支持 chattr（如非 ext4）时不必视为失败
            print(f"  解锁跳过（可能未锁定或文件系统不支持 chattr）：{r[1]}")

    # 2) 还原桌面启动器
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
                # 仅当备份确实"干净"（不含修复）时才用它还原。
                # 若备份本身已含修复（常见：在已修复状态下重跑 install 所产生），
                # 用它还原等于把修复又复制回来 —— 此时改为直接编辑移除。
                if bcontent and "GBM_BACKEND=dri" not in bcontent:
                    shutil.copy2(bak, STEAM_DESKTOP)
                    print(f"  已从干净备份还原启动器：{STEAM_DESKTOP}")
                    restored = True
            if not restored:
                STEAM_DESKTOP.write_text(STEAM_DESKTOP_BODY_PLAIN, encoding="utf-8")
                print(f"  已重写为纯净启动器（移除 GBM_BACKEND=dri）：{STEAM_DESKTOP}")
        else:
            print("  启动器未含 GBM_BACKEND=dri，无需还原。")
    else:
        print("  桌面启动器不存在，跳过。")

    return rc


# ---------------------------------------------------------------- 执行

AUTOFIX_UNIT = "/etc/systemd/system/lisuan-steam-autofix.service"
AUTOFIX_SRC = S / "steam/steam-cef-autofix.sh"


def install_autofix() -> int:
    if not AUTOFIX_SRC.is_file():
        print(f"  错误：找不到 {AUTOFIX_SRC}")
        return 1
    print("  提示：新方案（GBM_BACKEND=dri 环境变量）不需要开机自愈服务，")
    print("        Steam 更新不会覆盖桌面启动器。仅在仍使用旧注入方案时才需要本项。")
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


def show_doc(rel: str) -> int:
    """展示某类别目录下的说明文档（md）。

    交互终端优先用 less 分页；非交互（管道/脚本）直接打印全文。
    """
    path = S / rel
    if not path.is_file():
        print(f"  文档不存在：{path}")
        return 1
    if sys.stdin.isatty() and shutil.which("less"):
        return subprocess.run(["less", "-R", str(path)]).returncode
    print(path.read_text(errors="replace"))
    return 0


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

    # 说明项：展示该类别目录下的 md
    if it.get("doc"):
        return show_doc(it["doc"])

    # 内部实现的特例（按新编号）
    if item_id == 10:
        return install_steam_fix()
    if item_id == 11:
        return remove_steam_fix()
    if item_id == 13:
        return remove_autofix()
    if item_id == 18:
        return launch_underlords()

    cmd = it["cmd"](extra) if it["cmd"] else None
    if cmd is None:
        print(f"  菜单项 {item_id} 未定义命令")
        return 1

    # 脚本存在性检查：deb 安装版不含 minion-masters/（体积 64MB），
    # 直接执行会让 bash 报"No such file"，这里给出可读的提示。
    if cmd[0] == "bash" and len(cmd) > 1:
        script = Path(cmd[1])
        if not script.is_file():
            print(f"  脚本缺失：{script}")
            print("  说明：deb 安装版未打包该目录（体积过大）。")
            print("        请在源码仓库中运行本工具，或复制该目录到 /usr/share/lisuan-fix/。")
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


# ---------------------------------------------------------------- TUI（可选）

# Textual 相关的类在 _load_tui() 内部定义：这样未安装 textual 时，
# `from textual.app import App` 不会在模块导入期抛出 ImportError，
# 主程序可正常降级为纯文本菜单。函数内定义类还能直接闭包捕获本模块的
# ITEMS / ITEMS_BY_ID / GROUP_ORDER / detect / item_label / run_item。


def _load_tui() -> Callable[[], None] | None:
    """加载 Textual 界面；textual 不可用时返回 None（调用方降级为纯文本菜单）。"""
    try:
        from textual.app import App, ComposeResult
        from textual.containers import Horizontal
        from textual.widgets import (
            Footer,
            Header,
            Label,
            ListItem,
            ListView,
            RichLog,
            Static,
        )
    except Exception:
        return None

    LEVEL_STYLE = {"ok": "green", "bad": "red", "warn": "yellow", "info": "blue"}
    LEVEL_MARK_TUI = {"ok": "✓", "bad": "✗", "warn": "!", "info": "·"}

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
                mark = LEVEL_MARK_TUI.get(c.level, "?")
                lines.append(f"  [{style}]{mark}[/{style}] {c.text}")
            self.update("\n".join(lines))

    class MenuItemWidget(Static):
        def __init__(self, item: Item) -> None:
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

            # 说明项的 cmd 也是 None，必须先于下面的"内部实现"分支处理
            if it.get("doc"):
                import contextlib
                import io as _io

                buf = _io.StringIO()
                with contextlib.redirect_stdout(buf):
                    code = show_doc(it["doc"])
                text = buf.getvalue()
                if not text:  # show_doc 走 less 分支时无输出，退回直接读文件
                    try:
                        text = (REPO_ROOT / it["doc"]).read_text(errors="replace")
                    except OSError as e:
                        text = f"读取失败：{e}"
                # 超长文档只显示前 200 行，提示用命令行看全文
                lines = text.splitlines()
                preview = "\n".join(lines[:200])
                self.call_from_thread(log.write, "[b]说明文档预览[/b]")
                for ln in preview.splitlines():
                    self.call_from_thread(log.write, ln)
                if len(lines) > 200:
                    self.call_from_thread(
                        log.write,
                        f"[dim]... 共 {len(lines)} 行，已截断。"
                        f"查看完整文档：[b]python3 packaging/install.py {item_id}[/b][/dim]",
                    )
                self.call_from_thread(
                    log.write, f"[green]=== 文档结束，返回码 {code} ===[/green]"
                )
                return

            if it["cmd"] is None:
                self.call_from_thread(
                    log.write,
                    (
                        f"[yellow]菜单项 {item_id} 为内部实现，请在命令行执行：\n"
                        + f"  python3 packaging/install.py {item_id}[/yellow]"
                    ),
                )
                return

            cmd = it["cmd"]([])
            if it["root"] and os.geteuid() != 0:
                cmd = ["sudo", *cmd]

            self.call_from_thread(log.write, f"[dim]$ {' '.join(cmd)}[/dim]")
            try:
                import subprocess as _sp
                from typing import TextIO, cast

                proc = _sp.Popen(
                    cmd,
                    stdout=_sp.PIPE,
                    stderr=_sp.STDOUT,
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

    return run_tui


# ---------------------------------------------------------------- 入口分发


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
            choice = input("请选择 [0-26]: ").strip()
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
    try:
        sys.exit(main(sys.argv))
    except KeyboardInterrupt:
        print("\n再见。")
        sys.exit(130)
