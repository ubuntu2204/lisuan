#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""砺算 7G100 修复工具（入口壳，位于仓库根）。

用法（从仓库根运行）：
    python3 install.py                 桌面窗口（Tkinter）
    python3 install.py --detect        只检测并打印报告
    python3 install.py --detect --json 检测并输出 JSON（便于脚本化）
    python3 install.py 12 [参数]       非交互直执行第 12 项（修复 Steam）
    python3 install.py 22 [参数]       非交互直执行第 22 项（安装/修复 L4D2）
    python3 install.py 23              查看 L4D2 修复与排障说明
    python3 install.py 26 [安装器]     安装/升级豆包 Windows 客户端（Wine 11）
    python3 install.py 28 [安装器]     安装/升级即梦 AI Windows 客户端（Wine 11）
    python3 install.py -h              显示核心模块帮助

实现拆分：
    install.py           入口壳（本文件，仓库根）：定位并导入核心模块
    packaging/lisuan_fix_core.py   核心逻辑：环境检测 / 菜单定义 / 修复执行 / CLI
    packaging/lisuan_fix_gui.py    Tkinter 桌面界面
    packaging/lisuan_fix_md.py     Markdown 渲染器（说明弹窗）

驱动要求：
    本项目要求厂商驱动 `lisuangpu-dkms` / `lisuangpu-lib` **v262.2.0.56**；
    该版本是全部补丁、偏移量和兼容性结论的验证基准。运行 `--detect`
    会检查实际安装版本，其它版本会明确告警，不在本工具保证范围内。

依赖：仅标准库；GUI 需系统包 python3-tk。
"""

from __future__ import annotations

import sys
from pathlib import Path


def _bootstrap() -> None:
    """把核心模块所在目录加入 sys.path。

    - 源码仓库：本文件在仓库根，模块在 packaging/；
    - deb 安装模式：本文件副本在 /usr/bin/lisuan-fix，模块在
      /usr/share/lisuan-fix/。
    """
    here = Path(__file__).resolve().parent
    candidates = [here / "packaging", here, Path("/usr/share/lisuan-fix")]
    for c in candidates:
        if (c / "lisuan_fix_core.py").is_file():
            sys.path.insert(0, str(c))
            return
    # 找不到则交给默认 import 报错（提示更自然）


_bootstrap()

try:
    from lisuan_fix_core import main
except ImportError as e:  # pragma: no cover
    print(f"错误：找不到核心模块 lisuan_fix_core（{e}）", file=sys.stderr)
    print("  请从仓库根运行：python3 install.py --help", file=sys.stderr)
    print("  deb 安装模式下模块应位于 /usr/share/lisuan-fix/", file=sys.stderr)
    sys.exit(1)

if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv))
    except KeyboardInterrupt:
        print("\n再见。")
        sys.exit(130)
