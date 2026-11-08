#!/usr/bin/env bash
# 豆包官方新版 2.32.6 在 Proton 11 (Wine 11) 下的启动器。
# 环境变量与 Proton 自带 proton 脚本保持一致；前缀独立于 Deepin 前缀。
# 启动输出同时写 ~/.local/share/doubao-wine11/launch.log（可用
# DOUBAO_WINE11_LOG 覆盖），便于排查桌面双击这类没有终端的启动。
#
# 稳健默认：经 Steam Runtime 容器隔离 Mesa/GL 栈。旧图形会话下裸跑宿主 wine 时，
# Chromium 147 主窗口一直 IsUnMapped（style 不含 WS_VISIBLE、从未收到 SW_SHOW），
# 强制 XMapWindow 也只有纯黑；容器内正常映射并渲染真实界面。07:50 重启后的
# 新会话里裸 wine 也能跑，故容器不是唯一必要条件，但能绕开宿主图形栈污染。
# 与《随从大师》「图形环境只注入目标进程树、绝不污染全局」的经验一致。
set -euo pipefail

SELF="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/$(basename "${BASH_SOURCE[0]}")"

# 用 DOUBAO_STEAM_RUNTIME_CHILD 防止容器里二次包装；DOUBAO_NO_STEAM_RUNTIME=1
# 走轻量裸 wine 路径（干净图形会话下可用，旧污染会话下会黑屏），用于对照诊断。
if [[ -z "${DOUBAO_STEAM_RUNTIME_CHILD:-}" && "${DOUBAO_NO_STEAM_RUNTIME:-0}" != 1 ]]; then
    STEAM_COMMON="${STEAM_COMMON:-$HOME/.local/share/Steam/steamapps/common}"
    RUNTIME=""
    for candidate in "$STEAM_COMMON/SteamLinuxRuntime_sniper/run-in-sniper" \
                     "$STEAM_COMMON/SteamLinuxRuntime_soldier/run-in-soldier"; do
        if [[ -x "$candidate" ]]; then
            RUNTIME="$candidate"
            break
        fi
    done
    if [[ -z "$RUNTIME" ]]; then
        echo "错误：找不到 Steam Runtime 容器启动器（run-in-sniper）。" >&2
        echo "请安装 SteamLinuxRuntime_sniper（容器可隔离宿主图形栈），" >&2
        echo "或显式设置 DOUBAO_NO_STEAM_RUNTIME=1 走裸 wine 路径。" >&2
        exit 1
    fi
    exec env DOUBAO_STEAM_RUNTIME_CHILD=1 "$RUNTIME" -- bash "$SELF" "$@"
fi

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

LOG_FILE="${DOUBAO_WINE11_LOG:-$HOME/.local/share/doubao-wine11/launch.log}"
mkdir -p "$(dirname "$LOG_FILE")" 2>/dev/null || true
if ! : >> "$LOG_FILE" 2>/dev/null; then
    echo "错误：无法写入启动日志：$LOG_FILE" >&2
    exit 1
fi
printf '=== %s | %s %s ===\n' "$(date '+%F %T')" "$APP" "$*" >> "$LOG_FILE"
exec > >(tee -a "$LOG_FILE") 2>&1
exec "$PROTON/files/bin/wine" "$APP" "$@"
