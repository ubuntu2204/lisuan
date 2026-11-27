#!/usr/bin/env bash
# 至砺算：安装星际争霸 1 重制版 1.23.10.12409（64 位）。
# 游戏为厂商专有资产，只复制到本机运行目录，不纳入 Git。
#
# 用法：./starcraft-remastered/install.sh [源目录]
# 环境变量：SC_REMASTERED_SOURCE / SC_REMASTERED_GAME_DIR / SC_REMASTERED_PREFIX
# License: Apache-2.0
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE="${1:-${SC_REMASTERED_SOURCE:-/home/ubuntu/development/starcraft1.23.10.12409}}"
SOURCE="${SOURCE/#\~/$HOME}"
GAME_DIR="${SC_REMASTERED_GAME_DIR:-$SCRIPT_DIR/game}"
GAME_DIR="${GAME_DIR/#\~/$HOME}"
PREFIX="${SC_REMASTERED_PREFIX:-$HOME/.deepinwine/StarCraft-Remastered-wine11}"
PREFIX="${PREFIX/#\~/$HOME}"
PROTON="${SC_REMASTERED_PROTON:-$HOME/.local/share/Steam/steamapps/common/Proton 11.0}"
PROTON="${PROTON/#\~/$HOME}"
WINE="$PROTON/files/bin/wine"
DXVK="${SC_REMASTERED_DXVK:-$SCRIPT_DIR/../minion-masters/dxvk23/x86_64-windows}"
MAIN="$GAME_DIR/x86_64/StarCraft.exe"

die() { echo "错误：$*" >&2; exit 1; }

deploy_dxvk() {
    local dll
    [[ -f "$DXVK/d3d11.dll" && -f "$DXVK/dxgi.dll" ]] || die "找不到 DXVK 2.3：$DXVK"
    mkdir -p "$PREFIX/drive_c/windows/system32"
    for dll in d3d9 d3d10core d3d11 dxgi; do
        cp -f "$DXVK/$dll.dll" "$PREFIX/drive_c/windows/system32/$dll.dll"
        # 重制版用 LoadLibrary 加载图形 DLL；应用目录副本可避免 Wine 选内置实现。
        cp -f "$DXVK/$dll.dll" "$GAME_DIR/x86_64/$dll.dll"
    done
    # Wine 11 new WoW64 下环境变量覆盖对 dxgi/d3d11 不生效，写入前缀注册表。
    for dll in d3d9 d3d10core d3d11 dxgi; do
        timeout 15 env WINEPREFIX="$PREFIX" WINEARCH=win64 WINEDEBUG=-all \
            LD_LIBRARY_PATH="$PROTON/files/lib/x86_64-linux-gnu" \
            "$WINE" reg add 'HKCU\Software\Wine\DllOverrides' \
            /v "$dll" /d native /f >/dev/null
    done
    # Wine 11 的 new WoW64 将 vkd3d 拆成独立 DLL；补入前缀，避免内置 D3D 加载失败。
    for dll in "$PROTON"/files/lib/vkd3d/x86_64-windows/libvkd3d*.dll; do
        [[ -f "$dll" ]] && cp -f "$dll" "$PREFIX/drive_c/windows/system32/"
    done
}

if [[ -s "$MAIN" && "$(head -c2 "$MAIN")" == "MZ" ]]; then
    [[ -f "$PREFIX/system.reg" ]] || die "游戏已安装但 Wine 前缀缺失：$PREFIX"
    deploy_dxvk
    echo "[安装] 已安装并更新 DXVK：$MAIN"
    exit 0
fi

[[ -s "$SOURCE/x86_64/StarCraft.exe" ]] || die "找不到 64 位主程序：$SOURCE/x86_64/StarCraft.exe"
[[ -d "$SOURCE/Data/data" ]] || die "游戏数据目录缺失：$SOURCE/Data/data"
[[ -x "$WINE" ]] || die "找不到 Proton 11 Wine：$WINE"
[[ ! -e "$GAME_DIR" || -d "$GAME_DIR" ]] || die "目标不是目录：$GAME_DIR"

mkdir -p "$GAME_DIR"
if [[ -n "$(find "$GAME_DIR" -mindepth 1 -maxdepth 1 -print -quit)" ]]; then
    die "目标目录非空，为避免覆盖已有文件已停止：$GAME_DIR"
fi

tmp="$GAME_DIR.install.$$"
mkdir -p "$tmp"
cleanup() { [[ -d "$tmp" ]] && rm -rf -- "$tmp"; }
trap cleanup EXIT

echo "[1/2] 复制游戏文件（优先使用 reflink，不修改源目录）"
cp -a --reflink=auto "$SOURCE/." "$tmp/"
[[ -s "$tmp/x86_64/StarCraft.exe" && -d "$tmp/Data/data" ]] || die "复制结果校验失败"
rmdir "$GAME_DIR"
mv "$tmp" "$GAME_DIR"
trap - EXIT
echo "      游戏目录：$GAME_DIR"

echo "[2/2] 初始化独立 Wine 11 64 位前缀"
if [[ ! -f "$PREFIX/system.reg" ]]; then
    mkdir -p "$PREFIX"
    libs="$PROTON/files/lib/x86_64-linux-gnu:$PROTON/files/lib/i386-linux-gnu"
    timeout 90 env WINEPREFIX="$PREFIX" WINEARCH=win64 WINEDEBUG=-all \
        LD_LIBRARY_PATH="$libs" WINEDLLOVERRIDES='mscoree,mshtml,winemenubuilder=' \
        "$WINE" wineboot -u
fi
[[ -f "$PREFIX/system.reg" ]] || die "Wine 前缀初始化失败：$PREFIX"
deploy_dxvk
echo "      运行前缀：$PREFIX"
echo "[完成] 启动：$SCRIPT_DIR/run.sh"
