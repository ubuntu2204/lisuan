#!/usr/bin/env bash
# 豆包官方新版 2.32.6 在 Proton 11 (Wine 11) 下的启动器。
# 环境变量与 Proton 自带 proton 脚本保持一致；前缀独立于 Deepin 前缀。
set -euo pipefail

PROTON="${DOUBAO_PROTON:-$HOME/.local/share/Steam/steamapps/common/Proton 11.0}"
PREFIX="${DOUBAO_WINE11_PREFIX:-$HOME/.deepinwine/doubao-wine11}"

export WINEPREFIX="$PREFIX"
export WINEDLLPATH="$PROTON/files/lib/vkd3d:$PROTON/files/lib/wine"
export LD_LIBRARY_PATH="$PROTON/files/lib/x86_64-linux-gnu:$PROTON/files/lib/i386-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export WINEDLLOVERRIDES='mscoree,mshtml='

# 先用 Mesa 软件渲染确认 Wine API 兼容已通；gf 硬件加速单独一轮评估。
export __GLX_VENDOR_LIBRARY_NAME=mesa
export LIBGL_ALWAYS_SOFTWARE=1
export GBM_BACKEND=dri
export VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json
export WINE_D3D_CONFIG='renderer=gl'

APP=$(find "$PREFIX/drive_c/users" -path '*/Doubao/Application/app/Doubao.exe' -print -quit)
if [[ -z "$APP" ]]; then
    echo "错误：未在 $PREFIX 中找到豆包客户端。" >&2
    exit 1
fi

exec "$PROTON/files/bin/wine" "$APP" "$@"
