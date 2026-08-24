#!/bin/bash
# use-codebuddy.sh - CodeBuddy 国内版 (Electron) 启动 wrapper, 砾算 7G100 + Ubuntu 24.04
#
# 复用 use-electron-gf.sh 的通用 Electron 修复 + gf 硬件加速。
# 默认启用 gf 加速; 若界面白屏/花屏, 加 --no-hw 回退软件渲染:
#   bash use-codebuddy.sh --no-hw
#
# 若 CodeBuddy 安装在非标准路径, 用环境变量覆盖:
#   CODEBUDDY_CMD=/opt/codebuddy/codebuddy bash use-codebuddy.sh
# 或把桌面项指到本 wrapper:
#   bash use-codebuddy.sh --install-desktop

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export CODEBUDDY_CMD="${CODEBUDDY_CMD:-}"
if [ -n "${CODEBUDDY_CMD:-}" ]; then
  exec "$DIR/use-electron-gf.sh" --cmd "$CODEBUDDY_CMD" "$@"
else
  exec "$DIR/use-electron-gf.sh" --app codebuddy "$@"
fi
