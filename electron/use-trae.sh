#!/bin/bash
# use-trae.sh - Trae（字节 AI IDE, Electron）启动 wrapper, 砺算 7G100 + Ubuntu 24.04
#
# 复用 use-electron-gf.sh 的通用 Electron 修复 + gf 硬件加速。
# 默认启用 gf 加速; 若界面白屏/花屏, 加 --no-hw 回退软件渲染:
#   bash use-trae.sh --no-hw
#
# 国际版 trae / 国内版 trae-cn 均可: --app trae 优先国际版, 未装则回退国内版;
# 若只想用国内版:
#   bash use-trae.sh --app trae-cn
#
# 把应用菜单指到本 wrapper (之后从菜单直启即带修复+加速):
#   bash use-trae.sh --install-desktop
# 还原:
#   bash use-trae.sh --uninstall-desktop
#
# License: Apache-2.0

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$DIR/use-electron-gf.sh" --app trae "$@"
