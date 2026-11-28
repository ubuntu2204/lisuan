#!/usr/bin/env bash
# 至砺算：部署《战舰世界》7G100 硬件加速运行时不复制游戏本体。
# 仅编译垫片、复制厂商运行库、部署 DXVK 2.3，并可离线写入 Steam LaunchOptions。
# 不做存活监控、自动重启、看门狗或失败自动回退。
#
# 用法：
#   ./setup-wows-gf.sh [--configure-steam] [--no-verify]
#   ./setup-wows-gf.sh --status
# 环境变量：WOWS_STEAM_ROOT / WOWS_GAME_DIR / WOWS_GAME_BIN / WOWS_PROTON /
#           WOWS_PROTON8 / WOWS_GF_RUNTIME / WOWS_DXVK_SRC
# License: Apache-2.0
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$SCRIPT_DIR/.." && pwd)"
APPID=552990
STEAM_ROOT="${WOWS_STEAM_ROOT:-$HOME/.local/share/Steam}"
STEAM_ROOT="${STEAM_ROOT/#\~/$HOME}"
GAME_DIR="${WOWS_GAME_DIR:-$STEAM_ROOT/steamapps/common/World of Warships}"
GAME_DIR="${GAME_DIR/#\~/$HOME}"
PROTON="${WOWS_PROTON:-$STEAM_ROOT/steamapps/common/Proton 11.0}"
PROTON="${PROTON/#\~/$HOME}"
PROTON8="${WOWS_PROTON8:-$STEAM_ROOT/steamapps/common/Proton 8.0}"
PROTON8="${PROTON8/#\~/$HOME}"
RUNTIME="${WOWS_GF_RUNTIME:-$HOME/.local/share/worldofwarships-gf-accel}"
RUNTIME="${RUNTIME/#\~/$HOME}"
DXVK_SRC="${WOWS_DXVK_SRC:-$PROTON8/dist/lib64/wine/dxvk}"
DXVK_SRC="${DXVK_SRC/#\~/$HOME}"
PROBE="$REPO/.qoder/skills/lisuan-gpu-workflow/scripts/vk_probe.py"

CONFIGURE_STEAM=0
VERIFY=1
STATUS_ONLY=0

die() { echo "错误：$*" >&2; exit 1; }
say() { echo "[wows-gf] $*"; }

usage() {
    cat <<EOF
用法：$(basename "$0") [选项]
  --configure-steam  在 Steam 客户端完全停止后写入 AppID $APPID LaunchOptions
  --no-verify        跳过容器内 Vulkan 探测
  --status           只读体检
  --help             显示本说明

默认只部署本机运行时和游戏目录内 DXVK，不修改 Steam 配置。
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --configure-steam) CONFIGURE_STEAM=1 ;;
        --no-verify) VERIFY=0 ;;
        --status) STATUS_ONLY=1 ;;
        --help|-h) usage; exit 0 ;;
        *) die "未知参数：$1" ;;
    esac
    shift
done

find_game_bin() {
    if [[ -n "${WOWS_GAME_BIN:-}" ]]; then
        echo "${WOWS_GAME_BIN/#\~/$HOME}"
        return
    fi
    while IFS= read -r exe; do dirname "$exe"; done < <(
        find "$GAME_DIR/bin" -type f -path '*/bin64/WorldOfWarships64.exe' -print 2>/dev/null | sort -V
    ) | tail -1
}

GAME_BIN="$(find_game_bin)"
GAME_MAIN="$GAME_BIN/WorldOfWarships64.exe"

status() {
    local rc=0 item
    for item in "$GAME_MAIN" "$PROTON/proton" "$DXVK_SRC/d3d11.dll" "$DXVK_SRC/dxgi.dll" \
                /usr/lib/x86_64-linux-gnu/libvulkan_gf.so /usr/lib/x86_64-linux-gnu/libSC.so; do
        if [[ -s "$item" ]]; then echo "  [OK] $item"; else echo "  [--] 缺失：$item"; rc=1; fi
    done
    for item in "$RUNTIME/gf_icd.json" "$RUNTIME/os-release" \
                "$RUNTIME/lib/libvulkan_gf_shim.so" "$RUNTIME/lib/osrelease_redirect.so" \
                "$RUNTIME/lib/libvulkan_gf.so" "$RUNTIME/lib/libSC.so"; do
        if [[ -s "$item" ]]; then echo "  [OK] $item"; else echo "  [--] 未部署：$item"; rc=1; fi
    done
    for item in "$GAME_BIN/d3d11.dll" "$GAME_BIN/dxgi.dll"; do
        if cmp -s "$item" "$DXVK_SRC/$(basename "$item")"; then
            echo "  [OK] DXVK 2.3：$item"
        else
            echo "  [--] DXVK 2.3 未部署：$item"; rc=1
        fi
    done
    exit "$rc"
}

deploy_runtime() {
    [[ -x "$(command -v gcc)" ]] || die "找不到 gcc，shim 必须在宿主侧编译"
    [[ -s /usr/lib/x86_64-linux-gnu/libvulkan_gf.so ]] || die "缺少厂商 libvulkan_gf.so"
    [[ -s /usr/lib/x86_64-linux-gnu/libSC.so ]] || die "缺少厂商 libSC.so"

    mkdir -p "$RUNTIME/lib"
    install -m 0644 "$SCRIPT_DIR/assets/gf_icd.json" "$RUNTIME/gf_icd.json"
    install -m 0644 "$SCRIPT_DIR/assets/os-release" "$RUNTIME/os-release"

    if [[ -f "$RUNTIME/lib/libvulkan_gf_shim.so" && ! -f "$RUNTIME/lib/libvulkan_gf_shim.so.bak-wows-setup" ]]; then
        cp -p "$RUNTIME/lib/libvulkan_gf_shim.so" "$RUNTIME/lib/libvulkan_gf_shim.so.bak-wows-setup"
    fi

    install -m 0555 /usr/lib/x86_64-linux-gnu/libvulkan_gf.so "$RUNTIME/lib/libvulkan_gf.so"
    install -m 0555 /usr/lib/x86_64-linux-gnu/libSC.so "$RUNTIME/lib/libSC.so"

    gcc -shared -fPIC -O2 -DGF_SHIM_TRACE=0 \
        -DGF_RUNTIME_DIR="\"$RUNTIME\"" \
        -o "$RUNTIME/lib/.libvulkan_gf_shim.so.new" \
        "$SCRIPT_DIR/assets/libvulkan_gf_shim.c" -ldl \
        -Wl,--disable-new-dtags,-rpath,"$RUNTIME/lib"
    mv -f "$RUNTIME/lib/.libvulkan_gf_shim.so.new" "$RUNTIME/lib/libvulkan_gf_shim.so"

    gcc -shared -fPIC -O2 -DGF_RUNTIME_DIR="\"$RUNTIME\"" \
        -o "$RUNTIME/lib/.osrelease_redirect.so.new" \
        "$SCRIPT_DIR/assets/osrelease_redirect.c" -ldl
    mv -f "$RUNTIME/lib/.osrelease_redirect.so.new" "$RUNTIME/lib/osrelease_redirect.so"
    say "运行时已部署：$RUNTIME"
}

deploy_dxvk() {
    [[ -s "$GAME_MAIN" ]] || die "找不到游戏主程序：$GAME_MAIN"
    [[ -s "$DXVK_SRC/d3d11.dll" && -s "$DXVK_SRC/dxgi.dll" ]] || die "找不到 DXVK 2.3：$DXVK_SRC"
    local dll dst
    for dll in d3d11 dxgi; do
        dst="$GAME_BIN/$dll.dll"
        if [[ -f "$dst" ]] && ! cmp -s "$dst" "$DXVK_SRC/$dll.dll"; then
            mkdir -p "$GAME_BIN/.dxvk23-backup"
            cp -n "$dst" "$GAME_BIN/.dxvk23-backup/$dll.dll"
        fi
        install -m 0555 "$DXVK_SRC/$dll.dll" "$dst"
    done
    say "DXVK 2.3 已部署到游戏主程序目录：$GAME_BIN"
}

launch_options() {
    printf '%s\n' "DXVK_HUD=fps LD_PRELOAD=$RUNTIME/lib/osrelease_redirect.so LD_LIBRARY_PATH=$RUNTIME/lib __GLX_VENDOR_LIBRARY_NAME=mesa LIBGL_ALWAYS_SOFTWARE=1 VK_ICD_FILENAMES=$RUNTIME/gf_icd.json VK_DRIVER_FILES=$RUNTIME/gf_icd.json VK_LOADER_LAYERS_DISABLE=VK_LAYER_VALVE_steam_overlay_64,VK_LAYER_VALVE_steam_overlay_32 %command%"
}

write_options_file() {
    launch_options > "$SCRIPT_DIR/steam-launch-options.txt"
    say "已生成 Steam 启动选项：$SCRIPT_DIR/steam-launch-options.txt"
}

configure_steam() {
    if pgrep -f 'steamrt64/steam .*srt-logger-opened' >/dev/null; then
        die "Steam 客户端正在运行；请先完全退出，再使用 --configure-steam"
    fi
    local lc
    lc="$(find "$STEAM_ROOT/userdata" -maxdepth 3 -path '*/config/localconfig.vdf' -print | head -1)"
    [[ -s "$lc" ]] || die "找不到 localconfig.vdf"
    cp -n "$lc" "$lc.bak-wows-gf"
    WOWS_LAUNCH_OPTIONS="$(launch_options)" python3 - "$lc" <<'PY'
import os,re,sys
p=sys.argv[1]; lo=os.environ['WOWS_LAUNCH_OPTIONS']
s=open(p).read(); i=s.find('"apps"'); j=s.find('"552990"',i)
if j<0: raise SystemExit('localconfig 尚无 552990 段，请先运行一次游戏')
b=s.find('{',j); depth=1; k=b+1
while depth:
    depth += (s[k]=='{')-(s[k]=='}'); k+=1
seg=re.sub(r'\n\s*"LaunchOptions"\s*"[^"]*"','',s[b:k])
seg=seg[:1]+'\n\t\t\t\t\t\t"LaunchOptions"\t\t"'+lo.replace('\\','\\\\').replace('"','\\"')+'"'+seg[1:]
open(p,'w').write(s[:b]+seg+s[k:])
print('[wows-gf] Steam LaunchOptions 已写入')
PY
}

verify_runtime() {
    [[ -s "$PROBE" ]] || die "找不到项目 Vulkan probe：$PROBE"
    local rt="$STEAM_ROOT/steamapps/common/SteamLinuxRuntime_4"
    [[ -x "$rt/_v2-entry-point" ]] || die "找不到 Steam Linux Runtime：$rt"
    say "在 Steam Runtime 容器内验证 gf ICD"
    env LD_PRELOAD="$RUNTIME/lib/osrelease_redirect.so" \
        VK_ICD_FILENAMES="$RUNTIME/gf_icd.json" \
        VK_DRIVER_FILES="$RUNTIME/gf_icd.json" \
        "$rt/_v2-entry-point" --verb=waitforexitandrun -- \
        /run/host/usr/bin/python3 "$PROBE"
}

if [[ "$STATUS_ONLY" == 1 ]]; then status; fi

[[ -s "$GAME_MAIN" ]] || die "找不到游戏主程序：$GAME_MAIN"
[[ -x "$PROTON/proton" ]] || die "找不到 Proton 11：$PROTON"
[[ -s "$DXVK_SRC/d3d11.dll" ]] || die "找不到 DXVK 2.3，请先安装 Proton 8.0"

say "游戏主程序：$GAME_MAIN"
deploy_runtime
deploy_dxvk
write_options_file
if [[ "$CONFIGURE_STEAM" == 1 ]]; then configure_steam; fi
if [[ "$VERIFY" == 1 ]]; then verify_runtime; fi
say "完成。启动：$SCRIPT_DIR/run-wows-gf.sh"
