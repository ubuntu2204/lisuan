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

# 豆包 CSS 首选 PingFang SC（本机缺失）并常用 font-weight:300，默认回退到
# Noto Sans CJK SC Light，笔画过细。只给本进程树注入专用 fontconfig 替换，
# 把 Light 族名/字重抬到微软雅黑 Regular；DOUBAO_FONT_DISABLE=1 可临时关闭，
# DOUBAO_FONT_CONF 可指向自定义配置。
BASE_DIR="$(dirname "$SELF")"
FONT_CONF="${DOUBAO_FONT_CONF:-$BASE_DIR/fonts.conf}"
if [ "${DOUBAO_FONT_DISABLE:-0}" != "1" ] && [ -f "$FONT_CONF" ]; then
    export FONTCONFIG_FILE="$FONT_CONF"
fi

# 用 DOUBAO_STEAM_RUNTIME_CHILD 防止容器里二次包装；DOUBAO_NO_STEAM_RUNTIME=1
# 走轻量裸 wine 路径（干净图形会话下可用，旧污染会话下会黑屏），用于对照诊断。
# DOUBAO_FAST=1 是它的别名：省掉约 1.2 秒容器启动开销，稳健性略降，属可选项。
[[ "${DOUBAO_FAST:-0}" == "1" && -z "${DOUBAO_NO_STEAM_RUNTIME:-}" ]] && export DOUBAO_NO_STEAM_RUNTIME=1
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
# 启动日志只在排障时才看，却会无限增长（实测 89 MB）。超过阈值先轮转为 .1，
# 只保留一份历史，不删用户数据；阈值可用 DOUBAO_LOG_MAX_BYTES 调整。
LOG_MAX_BYTES="${DOUBAO_LOG_MAX_BYTES:-20971520}"
if [[ -f "$LOG_FILE" ]] && [[ "$(stat -c %s "$LOG_FILE" 2>/dev/null || echo 0)" -gt "$LOG_MAX_BYTES" ]]; then
    mv -f "$LOG_FILE" "$LOG_FILE.1" 2>/dev/null || true
fi
if ! : >> "$LOG_FILE" 2>/dev/null; then
    echo "错误：无法写入启动日志：$LOG_FILE" >&2
    exit 1
fi
printf '=== %s | %s %s ===\n' "$(date '+%F %T')" "$APP" "$*" >> "$LOG_FILE"
exec > >(tee -a "$LOG_FILE") 2>&1

# Wine 的 fixme 噪声每次启动刷数千行（经 tee 落盘），默认关掉；err/warn 保留。
# 需要完整诊断时显式传 WINEDEBUG（如 fixme+all、+relay），外部值优先。
export WINEDEBUG="${WINEDEBUG:-fixme-all}"

# Proton 的 files/bin/wine 壳会把 64 位程序交给 32 位 loader，32 位侧加载不了 64 位 PE，
# 只好经 start.exe /exec 再转一次（实测多花约 1.06 秒）。Proton 脚本自己的做法是
# 直接用 64 位 loader 并设 WINELOADERNOEXEC=1（注释：avoid restart through start.exe）。
# 这里按 PE 头判断架构：64 位走直连，32 位或无 64 位 loader 时回退旧路径。
# DOUBAO_WINE_LOADER=legacy 可强制旧行为，用于对照诊断。
pe_machine() {
    local off
    off=$(od -An -tu4 -j 60 -N 4 -- "$1" 2>/dev/null | tr -d ' \n')
    [[ "$off" =~ ^[0-9]+$ ]] || return 1
    od -An -tx2 -j $((off + 4)) -N 2 -- "$1" 2>/dev/null | tr -d ' \n'
}
WINE_LOADER=("$PROTON/files/bin/wine")
WIN_APP="$APP"
if [[ "${DOUBAO_WINE_LOADER:-auto}" != "legacy" && "$(pe_machine "$APP")" == "8664" ]] \
   && [[ -x "$PROTON/files/lib/wine/x86_64-unix/wine" ]]; then
    export WINELOADERNOEXEC=1
    WINE_LOADER=("$PROTON/files/lib/wine/x86_64-unix/wine")
    if [[ -x "$PROTON/files/lib/wine/x86_64-unix/wine-preloader" ]]; then
        WINE_LOADER=("$PROTON/files/lib/wine/x86_64-unix/wine-preloader" "${WINE_LOADER[@]}")
    fi
    # 直连 loader 会把传入的参数原样作为程序自己的命令行/模块路径。以前经 start.exe
    # 时豆包看到的是 C:\users\<user>\...，这里保持一致，避免自检/更新逻辑因路径
    # 变成 Z:\home\... 而误判；前缀不在 drive_c 下时退回原样。
    rel="${APP#"$PREFIX/drive_c"}"
    if [[ "$rel" != "$APP" ]]; then
        WIN_APP="C:${rel//\//\\}"
    fi
fi
exec "${WINE_LOADER[@]}" "$WIN_APP" "$@"
