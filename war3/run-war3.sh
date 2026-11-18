#!/usr/bin/env bash
# 至砺算：魔兽争霸 III（1.24 冰封王座）单应用启动器，供桌面入口调用。
#
# 根因（2026-11-18 实测）：本机砺算 7G100 只提供 64 位 Vulkan 驱动（gf ICD）。
# War3 是 32 位 PE，其 D3D8 -> vkd3d -> Vulkan 链若继承宿主的 64 位
# gf_icd.x86_64.json，设备初始化失败后进程跳到野地址 0x35800000
# （EXCEPTION_ACCESS_VIOLATION），窗口一闪即灭；改用同架构的 32 位
# llvmpipe ICD（lvp_icd.json）后正常出窗并稳定运行。
#
# 只在 wine 进程树注入环境变量，不污染 Steam 客户端与全局 shell。
# 单次前台启动、退出码原样透传；不做守护、看门狗或崩溃后自动重启
# （见 .codebuddy/rules/崩溃处理约定.md）。
#
# 用法：
#   ./war3/run-war3.sh [参数...]   # 启动 War3（参数透传给 War3.exe）
#   ./war3/run-war3.sh --status    # 只读体检
#   ./war3/run-war3.sh --help
#
# 环境变量：WAR3_DIR / WAR3_PROTON / WAR3_PREFIX / WAR3_ICD /
#   WAR3_WINEDEBUG / WAR3_RESET_CONFIG=1（清掉厂商写入的视频配置后启动）
# License: Apache-2.0
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GAME_DIR="${WAR3_DIR:-/opt/apps/war3/files}"
GAME_DIR="${GAME_DIR%/}"
PROTON="${WAR3_PROTON:-$HOME/.local/share/Steam/steamapps/common/Proton 11.0}"
PROTON="${PROTON/#\~/$HOME}"
PREFIX="${WAR3_PREFIX:-$HOME/.deepinwine/war3-wine11}"
PREFIX="${PREFIX/#\~/$HOME}"
# 32 位 llvmpipe ICD：本机唯一的 32 位可用 Vulkan 实现（gf 只有 64 位）
ICD="${WAR3_ICD:-/usr/share/vulkan/icd.d/lvp_icd.json}"
WINE="$PROTON/files/bin/wine"
MAIN="$GAME_DIR/War3.exe"

die() { echo "错误：$*" >&2; exit 1; }

usage() {
    cat <<EOF
用法：$(basename "$0") [选项] [参数...]

  （无参数） 前台启动魔兽争霸 III（参数透传给 War3.exe）
  --status   只读体检：wine、游戏目录、主程序 PE、Vulkan ICD、前缀、目录可写性
  --help     显示本说明

环境变量：WAR3_DIR、WAR3_PROTON、WAR3_PREFIX、WAR3_ICD、WAR3_WINEDEBUG、
          WAR3_RESET_CONFIG=1（启动前删除 Blizzard 写入的视频配置）
崩溃后不会自动重启；请按 .codebuddy/rules/崩溃处理约定.md 取证定位。
EOF
}

status() {
    local rc=0
    [[ -x "$WINE" ]] && echo "  [OK] wine：$WINE" || { echo "  [--] wine 缺失：$WINE"; rc=1; }
    [[ -d "$GAME_DIR" ]] && echo "  [OK] 游戏目录：$GAME_DIR" || { echo "  [--] 游戏目录缺失：$GAME_DIR"; rc=1; }
    if [[ -s "$MAIN" && "$(head -c2 "$MAIN")" == "MZ" ]]; then
        echo "  [OK] 主程序：$MAIN"
    else
        echo "  [--] 主程序缺失或不是有效 PE：$MAIN"; rc=1
    fi
    [[ -f "$ICD" ]] && echo "  [OK] Vulkan ICD：$ICD" || { echo "  [--] ICD 缺失：$ICD"; rc=1; }
    if [[ -f "$PREFIX/system.reg" ]]; then
        echo "  [OK] Wine 前缀：$PREFIX"
    else
        echo "  [--] Wine 前缀尚未初始化：$PREFIX（首次启动会自动创建）"
    fi
    local d
    for d in Save Replay Maps; do
        if [[ -d "$GAME_DIR/$d" && ! -w "$GAME_DIR/$d" ]]; then
            echo "  [--] $d/ 不可写（存档/录像/自定义地图无法保存）：$GAME_DIR/$d"
            echo "       修复：./war3/fix-war3-writable.sh"
        fi
    done
    return "$rc"
}

case "${1:-}" in
    --help|-h) usage; exit 0 ;;
    --status) status; exit $? ;;
esac

[[ -x "$WINE" ]] || die "找不到 Proton 11 的 wine：$WINE"
[[ -s "$MAIN" && "$(head -c2 "$MAIN")" == "MZ" ]] || die "主程序缺失或不是有效 PE：$MAIN"
[[ -f "$ICD" ]] || die "Vulkan ICD 缺失：$ICD"

# 前缀占用检查：同一 wine 前缀上并发跑两个实例时，后启动的那一个渲染设备
# 初始化会失败，War3 弹出 "Warcraft III was unable to initialize" 并挂住
# （2026-11-18 实测）。这里显式报错退出，不用看门狗/自动重启掩盖。
prefix_busy() {
    local pid
    for pid in $(pgrep -x wineserver 2>/dev/null || true); do
        [[ -r "/proc/$pid/environ" ]] || continue
        if tr '\0' '\n' < "/proc/$pid/environ" 2>/dev/null | grep -qxF "WINEPREFIX=$PREFIX"; then
            printf '%s' "$pid"; return 0
        fi
    done
    return 1
}
if busy_pid="$(prefix_busy)"; then
    die "该 Wine 前缀已有实例在运行（wineserver pid=$busy_pid）：$PREFIX
请先关闭已打开的 War3 窗口再启动；重复点击桌面图标会触发此提示。"
fi

libs="$PROTON/files/lib/x86_64-linux-gnu:$PROTON/files/lib/i386-linux-gnu"
[[ -n "${LD_LIBRARY_PATH:-}" ]] && libs="$libs:$LD_LIBRARY_PATH"

run_wine() {
    exec env \
        VK_ICD_FILENAMES="$ICD" \
        PROTON_USE_WOW64=1 \
        WINEPREFIX="$PREFIX" \
        WINEDLLPATH="$PROTON/files/lib/vkd3d:$PROTON/files/lib/wine" \
        LD_LIBRARY_PATH="$libs" \
        WINEDLLOVERRIDES='mscoree,mshtml,winemenubuilder=' \
        LIBGL_ALWAYS_SOFTWARE=1 \
        __GLX_VENDOR_LIBRARY_NAME=mesa \
        WINEDEBUG="${WAR3_WINEDEBUG:--all}" \
        "$WINE" "$@"
}

# 首次运行：静默初始化前缀（跳过 mono/gecko 下载提示）
if [[ ! -f "$PREFIX/system.reg" ]]; then
    echo "[war3] 首次运行，正在初始化 Wine 前缀：$PREFIX" >&2
    run_wine wineboot -u >/dev/null 2>&1 || true
fi

# 可选：清掉厂商写入的视频配置（只在配置异常时用，默认不动）
if [[ "${WAR3_RESET_CONFIG:-0}" == "1" ]]; then
    echo "[war3] 正在清除 Blizzard 视频配置" >&2
    run_wine reg delete 'HKCU\Software\Blizzard Entertainment' /f >/dev/null 2>&1 || true
fi

cd "$GAME_DIR"
run_wine "$MAIN" "$@"
