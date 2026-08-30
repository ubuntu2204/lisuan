#!/bin/bash
# use-qoder.sh - Qoder 国内版 (Electron) 启动 wrapper, 砾算 7G100 + Ubuntu 24.04
#
# 复用 use-electron-gf.sh 的通用 Electron 修复 + gf 硬件加速。
# 默认启用 gf 加速; 若界面白屏/花屏, 加 --no-hw 回退软件渲染:
#   bash use-qoder.sh --no-hw
#
# 把应用菜单指到本 wrapper (之后从菜单直启即带修复+加速):
#   bash use-qoder.sh --install-desktop

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$DIR/../common/use-electron-gf.sh" --app qoder "$@"
