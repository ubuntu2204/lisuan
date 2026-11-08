#!/usr/bin/env bash
# 即梦 AI（Dreamina）Windows 桌面客户端 1.0.59 在 Proton 11 (Wine 11) 下的启动器。
#
# 与 doubao/run-doubao-wine11.sh 同构：环境变量与 Proton 自带 proton 脚本保持一致，
# 前缀独立于 Deepin 前缀，稳健默认走 Steam Runtime 容器隔离 Mesa/GL 栈。
# 启动输出同时写 ~/.local/share/dreamina-wine11/launch.log（可用
# DREAMINA_WINE11_LOG 覆盖），便于排查桌面双击这类没有终端的启动。
#
# 用法：
#   ./dreamina/run-dreamina.sh            # 启动客户端
#   DREAMINA_NO_STEAM_RUNTIME=1 ./dreamina/run-dreamina.sh   # 走裸 wine（诊断用）
set -euo pipefail

SELF="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/$(basename "${BASH_SOURCE[0]}")"

# 用 DREAMINA_STEAM_RUNTIME_CHILD 防止容器里二次包装。
if [[ -z "${DREAMINA_STEAM_RUNTIME_CHILD:-}" && "${DREAMINA_NO_STEAM_RUNTIME:-0}" != 1 ]]; then
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
        echo "或显式设置 DREAMINA_NO_STEAM_RUNTIME=1 走裸 wine 路径。" >&2
        exit 1
    fi
    exec env DREAMINA_STEAM_RUNTIME_CHILD=1 "$RUNTIME" -- bash "$SELF" "$@"
fi

PROTON="${DREAMINA_PROTON:-$HOME/.local/share/Steam/steamapps/common/Proton 11.0}"
PREFIX="${DREAMINA_WINE11_PREFIX:-$HOME/.deepinwine/dreamina-wine11}"

export WINEPREFIX="$PREFIX"
export WINEDLLPATH="$PROTON/files/lib/vkd3d:$PROTON/files/lib/wine"
export LD_LIBRARY_PATH="$PROTON/files/lib/x86_64-linux-gnu:$PROTON/files/lib/i386-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export WINEDLLOVERRIDES='mscoree,mshtml='

# 与豆包同款：先用 Mesa 软件渲染确认 Wine API 兼容已通；gf 硬件加速单独评估。
export __GLX_VENDOR_LIBRARY_NAME=mesa
export LIBGL_ALWAYS_SOFTWARE=1
export GBM_BACKEND=dri
export VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json
export WINE_D3D_CONFIG='renderer=gl'

[[ -x "$PROTON/files/bin/wine" ]] || {
    echo "错误：找不到 Proton 的 wine：$PROTON/files/bin/wine" >&2
    exit 1
}

# 不把主程序名写死：优先「即梦 AI.exe」，版本改名后回退到安装目录里
# 任意一个非卸载器 exe（排除 resources/ 下的 elevate.exe）。
INSTDIR=$(find "$PREFIX/drive_c/users" -maxdepth 8 -type d \
          -path '*/AppData/Local/Programs/dreamina-desktop' \
          -print -quit 2>/dev/null || true)
APP=""
if [[ -n "$INSTDIR" && -f "$INSTDIR/即梦 AI.exe" ]]; then
    APP="$INSTDIR/即梦 AI.exe"
elif [[ -n "$INSTDIR" ]]; then
    APP=$(find "$INSTDIR" -maxdepth 1 -type f -name '*.exe' \
          ! -name 'Uninstall*' -print -quit 2>/dev/null || true)
fi
if [[ -z "$APP" ]]; then
    echo "错误：未在 $PREFIX 的 AppData/Local/Programs/dreamina-desktop 里找到主程序。" >&2
    echo "先执行 ./dreamina/install-dreamina.sh 安装。" >&2
    exit 1
fi

LOG_FILE="${DREAMINA_WINE11_LOG:-$HOME/.local/share/dreamina-wine11/launch.log}"
mkdir -p "$(dirname "$LOG_FILE")" 2>/dev/null || true
if ! : >> "$LOG_FILE" 2>/dev/null; then
    echo "错误：无法写入启动日志：$LOG_FILE" >&2
    exit 1
fi
printf '=== %s | %s %s ===\n' "$(date '+%F %T')" "$APP" "$*" >> "$LOG_FILE"
exec > >(tee -a "$LOG_FILE") 2>&1

# 默认渲染开关（2026-11-08 实测，理由见 dreamina/README.md）：
#   --use-gl=desktop  : **关键**。Electron 默认走 ANGLE，而 ANGLE 的 GL 后端在 Wine 下
#                        要求 wglGetProcAddress 扩展 WGL_NV_DX_interop2，wine 的 wined3d
#                        不提供，GPU 进程会直接 “Exiting GPU process due to errors during
#                        initialization” 退出，窗口停在 IsUnMapped（屏幕上什么都没有）。
#                        改用 desktop GL（Wine 的 opengl32 → mesa llvmpipe）即可正常渲染。
#   --ignore-gpu-blocklist : 允许在软件渲染环境下继续用 GL。
#   --enable-unsafe-swiftshader : Chromium 137+ 默认禁用 SwiftShader 光栅化。
#   --no-sandbox / --disable-gpu-sandbox : Wine 下 Chromium 沙箱起不来。
# 额外传参会在此之后追加，可覆盖默认。
exec "$PROTON/files/bin/wine" "$APP" \
    --use-gl=desktop \
    --ignore-gpu-blocklist \
    --enable-unsafe-swiftshader \
    --no-sandbox \
    --disable-gpu-sandbox \
    "$@"
