#!/usr/bin/env bash
# 至砺算：KK 官方对战平台（Qt5 + QtWebEngine 32 位客户端）在 Wine 下的运行问题
# 与显卡硬件无关：默认 wined3d/Vulkan 后端初始化失败会让 QtWebEngine 在
# Qt5WebEngineCore.dll+0x82FA45 空指针读（c0000005）并退出（rc=5）。显式指定
# Mesa llvmpipe Vulkan ICD 与 wined3d 的 GL 渲染器后，窗口与主进程稳定。
#
# 只在这一个应用的进程树上注入图形环境，绝不污染全局 shell / Steam 客户端；
# 不做崩溃后自动重启、重试或守护。图标与桌面入口见 fix-kk-desktop.sh。
#
# 用法：./war3/run-kk-platform.sh [传给 Platform.exe 的参数]
# 环境变量：KK_PROTON / KK_WINE11_PREFIX / KK_LAUNCH_LOG / KK_WINEDEBUG
# License: Apache-2.0
set -euo pipefail

PROTON="${KK_PROTON:-$HOME/.local/share/Steam/steamapps/common/Proton 11.0}"
PROTON="${PROTON/#\~/$HOME}"
PREFIX="${KK_WINE11_PREFIX:-$HOME/.deepinwine/kkduizhan-wine11}"
PREFIX="${PREFIX/#\~/$HOME}"
INSTALL_DIR="$PREFIX/drive_c/Program Files (x86)/kkduizhan"
APP="$INSTALL_DIR/Platform.exe"
WINE="$PROTON/files/bin/wine"
LOG="${KK_LAUNCH_LOG:-$HOME/.local/share/kkduizhan-wine11/launch.log}"

if [[ ! -x "$WINE" ]]; then
    echo "错误：找不到 Proton 11 的 wine：$WINE" >&2
    exit 1
fi
if [[ ! -f "$APP" ]]; then
    echo "错误：未安装 KK 对战平台：$APP（先跑 install-kk-platform.sh）" >&2
    exit 1
fi
mkdir -p "$(dirname "$LOG")" 2>/dev/null || true
if ! : >> "$LOG" 2>/dev/null; then
    echo "错误：无法写入启动日志：$LOG" >&2
    exit 1
fi

export WINEPREFIX="$PREFIX"
export WINEDLLPATH="$PROTON/files/lib/vkd3d:$PROTON/files/lib/wine"
export LD_LIBRARY_PATH="$PROTON/files/lib/x86_64-linux-gnu:$PROTON/files/lib/i386-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export WINEDLLOVERRIDES='mscoree,mshtml='
# 与项目其它 Wine 应用一致的安全软件渲染基线；两个关键项二选一即可越过崩溃，
# 同时设置是为了在 Vulkan/GL 任一路径异常时都保持可启动（2026-11-14 消融）。
export GBM_BACKEND=dri
export LIBGL_ALWAYS_SOFTWARE=1
export __GLX_VENDOR_LIBRARY_NAME=mesa
export VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json
export WINE_D3D_CONFIG='renderer=gl'
export WINEDEBUG="${KK_WINEDEBUG:--all}"

printf '=== %s | %s %s ===\n' "$(date '+%F %T')" "$APP" "$*" >> "$LOG"
exec > >(tee -a "$LOG") 2>&1
cd "$INSTALL_DIR"
exec "$WINE" "$APP" "$@"
