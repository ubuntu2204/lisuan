#!/usr/bin/env bash
# 至砺算：星际争霸 1 重制版 1.23.10.12409 单次前台启动器。
# 默认 Wine 11 + DXVK 2.3 + lavapipe 软件 Vulkan；gf 原生 ICD 暂不兼容。
# 不做存活监控、自动重启、看门狗或守护；退出码原样透传。
#
# 用法：./starcraft-remastered/run.sh [游戏参数...]
# 环境变量：SC_REMASTERED_GAME_DIR / SC_REMASTERED_PREFIX / SC_REMASTERED_PROTON /
#           SC_REMASTERED_DXVK / SC_REMASTERED_VK_ICD / SC_REMASTERED_WINEDEBUG
# License: Apache-2.0
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$SCRIPT_DIR/.." && pwd)"
GAME_DIR="${SC_REMASTERED_GAME_DIR:-$SCRIPT_DIR/game}"
GAME_DIR="${GAME_DIR/#\~/$HOME}"
PREFIX="${SC_REMASTERED_PREFIX:-$HOME/.deepinwine/StarCraft-Remastered-wine11}"
PREFIX="${PREFIX/#\~/$HOME}"
PROTON="${SC_REMASTERED_PROTON:-$HOME/.local/share/Steam/steamapps/common/Proton 11.0}"
PROTON="${PROTON/#\~/$HOME}"
WINE="$PROTON/files/bin/wine"
DXVK="${SC_REMASTERED_DXVK:-$REPO/minion-masters/dxvk23/x86_64-windows}"
VK_ICD="${SC_REMASTERED_VK_ICD:-/usr/share/vulkan/icd.d/lvp_icd.json}"
MAIN="$GAME_DIR/x86_64/StarCraft.exe"

die() { echo "错误：$*" >&2; exit 1; }

case "${1:-}" in
    --status)
        rc=0
        [[ -s "$MAIN" && "$(head -c2 "$MAIN")" == "MZ" ]] && \
            echo "  [OK] 主程序：$MAIN" || { echo "  [--] 主程序缺失：$MAIN"; rc=1; }
        [[ -f "$PREFIX/system.reg" ]] && echo "  [OK] Wine 前缀：$PREFIX" || \
            { echo "  [--] Wine 前缀缺失：$PREFIX"; rc=1; }
        [[ -x "$WINE" ]] && echo "  [OK] Wine：$WINE" || { echo "  [--] Wine 缺失：$WINE"; rc=1; }
        [[ -f "$DXVK/d3d11.dll" && -f "$DXVK/dxgi.dll" ]] && \
            echo "  [OK] DXVK 2.3：$DXVK" || { echo "  [--] DXVK 2.3 缺失：$DXVK"; rc=1; }
        [[ -f "$VK_ICD" ]] && echo "  [OK] Vulkan ICD：$VK_ICD" || \
            { echo "  [--] Vulkan ICD 缺失：$VK_ICD"; rc=1; }
        exit "$rc"
        ;;
    --help|-h)
        cat <<EOF
用法：$(basename "$0") [游戏参数...]
  --status   只读体检
  --help     显示本说明

默认：Wine 11 + DXVK 2.3 + lavapipe 软件 Vulkan
可覆盖：SC_REMASTERED_VK_ICD=/path/to/icd.json $0
EOF
        exit 0
        ;;
esac

[[ -s "$MAIN" && "$(head -c2 "$MAIN")" == "MZ" ]] || die "主程序缺失或不是 PE：$MAIN"
[[ -f "$PREFIX/system.reg" ]] || die "Wine 前缀缺失，请先运行 install.sh：$PREFIX"
[[ -x "$WINE" ]] || die "Wine 缺失：$WINE"
[[ -f "$DXVK/d3d11.dll" && -f "$DXVK/dxgi.dll" ]] || die "DXVK 2.3 缺失：$DXVK"
[[ -f "$VK_ICD" ]] || die "Vulkan ICD 缺失：$VK_ICD"

libs="$PROTON/files/lib/x86_64-linux-gnu:$PROTON/files/lib/i386-linux-gnu"
[[ -n "${LD_LIBRARY_PATH:-}" ]] && libs="$libs:$LD_LIBRARY_PATH"
wine_path="$DXVK:$PROTON/files/lib/vkd3d/x86_64-windows:$PROTON/files/lib/wine/x86_64-windows"

# 与随包 64 位启动器 .bat 一致：工作目录为游戏根目录。
cd "$GAME_DIR"
exec env WINEPREFIX="$PREFIX" \
    WINEDLLPATH="$wine_path" \
    LD_LIBRARY_PATH="$libs" \
    WINEDLLOVERRIDES='mscoree=,mshtml=,winemenubuilder=' \
    VK_ICD_FILENAMES="$VK_ICD" \
    WINEDEBUG="${SC_REMASTERED_WINEDEBUG:--all}" \
    "$WINE" "$MAIN" -launch "$@"
