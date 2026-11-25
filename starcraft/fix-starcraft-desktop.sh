#!/usr/bin/env bash
# 至砺算：星际争霸桌面入口集成。图标来自本机 Spark 包自带的官方 208×208
# PNG（/opt/apps/com.starcraft.spark/...），缩放到 hicolor 后写入桌面项，
# Exec 指向隔离启动器 run-starcraft.sh（单次启动 + 游戏内 Alt+Enter 全屏）。
#
# 图标产物写入 ~/.local/share/icons，不入库；游戏本体与图标原文件属于
# 第三方专有资产，也不入库（见 .codebuddy/rules/仓库组织约定.md）。
# 只改 Icon/Exec 两行并保留用户其它定制；改文件前生成 .bak。
# 不做崩溃后自动重启/守护。
#
# 用法：
#   ./starcraft/fix-starcraft-desktop.sh             # 安装（幂等）
#   ./starcraft/fix-starcraft-desktop.sh --status
#   ./starcraft/fix-starcraft-desktop.sh --uninstall
#   ./starcraft/fix-starcraft-desktop.sh --help
# License: Apache-2.0
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RUNNER="$SCRIPT_DIR/run-starcraft.sh"
ICON_SRC="${STARCRAFT_ICON_SRC:-/opt/apps/com.starcraft.spark/entries/icons/hicolor/scalable/apps/com.starcraft.spark.png}"
GAME_DIR="${STARCRAFT_DIR:-$HOME/.deepinwine/Deepin-starcraft/drive_c/Program Files/starcraft}"
MAIN_EXE="$GAME_DIR/starcraft.exe"
ICON_NAME=starcraft
ICON_ROOT="${STARCRAFT_ICON_ROOT:-$HOME/.local/share/icons}"
APP_ENTRY="${STARCRAFT_APP_ENTRY:-$HOME/.local/share/applications/starcraft.desktop}"
DESKTOP_DIR="$(xdg-user-dir DESKTOP 2>/dev/null || true)"
[[ -n "$DESKTOP_DIR" && "$DESKTOP_DIR" != "$HOME" ]] || DESKTOP_DIR="$HOME/Desktop"
DESKTOP_ENTRY="$DESKTOP_DIR/starcraft.desktop"
BACKUP_DIR="$HOME/.local/share/starcraft/desktop-backups"
ENTRIES=("$APP_ENTRY" "$DESKTOP_ENTRY")
if [[ "$HOME/Desktop" != "$DESKTOP_DIR" && -d "$HOME/Desktop" ]]; then
    ENTRIES+=("$HOME/Desktop/starcraft.desktop")
fi
EXPECTED_EXEC="\"$RUNNER\""

usage() {
    cat <<EOF
用法：$(basename "$0") [--status|--uninstall|--help]

  （无参数）   生成图标并创建/修正桌面入口，标记 trusted
  --status     只读体检：图标、入口 Icon/Exec/trusted、启动器与主程序
  --uninstall  恢复 .desktop 备份（新建的删除）、移除生成的图标
  --help       显示本说明

环境变量：STARCRAFT_DIR / STARCRAFT_ICON_SRC / STARCRAFT_ICON_ROOT /
          STARCRAFT_APP_ENTRY
备份目录：$BACKUP_DIR
EOF
}

is_trusted() {
    command -v gio >/dev/null 2>&1 || return 1
    gio info -a metadata::trusted "$1" 2>/dev/null | grep -q 'metadata::trusted: true'
}

backup_of() { printf '%s/%s.bak' "$BACKUP_DIR" "$(basename "$1")"; }

entry_ok() {
    local f="$1"
    [[ -f "$f" ]] || return 1
    grep -qF "Icon=$ICON_NAME" "$f" || return 1
    grep -qF "Exec=$EXPECTED_EXEC" "$f" || return 1
    return 0
}

status() {
    local rc=0 s p f
    [[ -x "$RUNNER" ]] && echo "  [OK] 启动器：$RUNNER" || \
        { echo "  [--] 启动器缺失或不可执行：$RUNNER"; rc=1; }
    [[ -s "$MAIN_EXE" && "$(head -c2 "$MAIN_EXE")" == "MZ" ]] && \
        echo "  [OK] 主程序：$MAIN_EXE" || \
        { echo "  [--] 主程序缺失：$MAIN_EXE"; rc=1; }
    [[ -s "$ICON_SRC" ]] && echo "  [OK] 图标源：$ICON_SRC" || \
        { echo "  [--] 图标源缺失：$ICON_SRC"; rc=1; }
    for s in 48x48 128x128 256x256; do
        p="$ICON_ROOT/hicolor/$s/apps/$ICON_NAME.png"
        [[ -s "$p" ]] && echo "  [OK] 图标：$s ($(stat -c%s "$p") 字节)" || \
            { echo "  [--] 缺图标：$p"; rc=1; }
    done
    for f in "${ENTRIES[@]}"; do
        if entry_ok "$f"; then
            if is_trusted "$f"; then
                echo "  [OK] 桌面项：$f（trusted）"
            else
                echo "  [--] 桌面项未标记 trusted：$f"; rc=1
            fi
        else
            echo "  [--] 桌面项缺失或 Icon/Exec 不对：$f"; rc=1
        fi
    done
    return "$rc"
}

install_icons() {
    python3 - "$ICON_SRC" "$ICON_ROOT/hicolor" "$ICON_NAME" <<'PY'
import sys
from pathlib import Path
from PIL import Image

src, root, name = map(Path, sys.argv[1:4])
im = Image.open(src).convert("RGBA")
for size in (16, 24, 32, 48, 64, 96, 128, 256):
    d = root / f"{size}x{size}" / "apps"
    d.mkdir(parents=True, exist_ok=True)
    im.resize((size, size), Image.LANCZOS).save(d / f"{name}.png", "PNG")
PY
}

write_entry() {
    local f="$1" tmp
    [[ -f "$f" && ! -w "$f" ]] && { echo "  警告：无写权限，跳过：$f" >&2; return 0; }
    mkdir -p "$(dirname "$f")"
    if [[ ! -f "$f" ]]; then
        cat > "$f" <<EOF
[Desktop Entry]
Type=Application
Name=星际争霸
Name[en_US]=StarCraft
Comment=星际争霸 1.08（deepin-wine5，启动时按 Alt+Enter 全屏）
Exec=$EXPECTED_EXEC
Icon=$ICON_NAME
Terminal=false
Categories=Game;
StartupNotify=false
EOF
        echo "  已创建：$f"
    else
        local bak; bak="$(backup_of "$f")"
        if [[ ! -f "$bak" ]]; then
            mkdir -p "$BACKUP_DIR"
            cp -p "$f" "$bak"
            echo "  已备份：$f -> $bak"
        fi
        tmp="$(mktemp)"
        python3 - "$f" "$ICON_NAME" "$EXPECTED_EXEC" > "$tmp" <<'PY'
import sys
path, icon, exe = sys.argv[1:]
lines = open(path, encoding="utf-8", errors="replace").read().splitlines()
out, has_icon, has_exec = [], False, False
for line in lines:
    if line.startswith("Icon="):
        out.append("Icon=" + icon); has_icon = True
    elif line.startswith("Exec="):
        out.append("Exec=" + exe); has_exec = True
    else:
        out.append(line)
if not has_icon:
    out.append("Icon=" + icon)
if not has_exec:
    out.append("Exec=" + exe)
print("\n".join(out) + "\n", end="")
PY
        if ! cmp -s "$tmp" "$f"; then
            cat "$tmp" > "$f"
            echo "  已更新：$f（只改 Icon/Exec，其余保留）"
        fi
        rm -f "$tmp"
    fi
    chmod +x "$f"
    if command -v gio >/dev/null 2>&1; then
        gio set "$f" metadata::trusted true 2>/dev/null || \
            echo "  警告：无法标记 trusted：$f" >&2
    else
        echo "  警告：未找到 gio，桌面图标可能需手动信任：$f" >&2
    fi
}

install_desktop() {
    [[ -x "$RUNNER" ]] || { echo "错误：启动器缺失或不可执行：$RUNNER" >&2; exit 1; }
    [[ -s "$MAIN_EXE" ]] || { echo "错误：主程序不存在：$MAIN_EXE（请先安装星际争霸）" >&2; exit 1; }
    [[ -s "$ICON_SRC" ]] || { echo "错误：图标源不存在：$ICON_SRC" >&2; exit 1; }
    echo "[1/3] 缩放官方图标 -> $ICON_ROOT/hicolor"
    install_icons
    command -v gtk-update-icon-cache >/dev/null 2>&1 && \
        gtk-update-icon-cache -q -f -t "$ICON_ROOT/hicolor" 2>/dev/null || true
    echo "[2/3] 写入桌面入口"
    for f in "${ENTRIES[@]}"; do write_entry "$f"; done
    echo "[3/3] 完成。体检：$(basename "$0") --status"
}

uninstall() {
    local f bak
    for f in "${ENTRIES[@]}"; do
        bak="$(backup_of "$f")"
        if [[ -f "$bak" ]]; then
            mv -f "$bak" "$f"
            echo "  已恢复：$f"
        elif [[ -f "$f" ]] && grep -qF "Icon=$ICON_NAME" "$f" && \
             grep -qF "Exec=$EXPECTED_EXEC" "$f"; then
            rm -f "$f"
            echo "  已删除：$f"
        fi
    done
    find "$ICON_ROOT/hicolor" -type f -name "$ICON_NAME.png" -delete 2>/dev/null || true
    echo "  已移除生成的图标；游戏本体与前缀未改动。"
}

case "${1:-}" in
    --help|-h) usage; exit 0 ;;
    --status) status; exit $? ;;
    --uninstall) uninstall; exit 0 ;;
    --*) echo "错误：未知参数：$1（用 --help 查看用法）" >&2; exit 1 ;;
    *) install_desktop ;;
esac
