#!/usr/bin/env bash
# 至砺算：图标与桌面集成和显卡无关。War3 安装在 /opt/apps/war3/files，
# 安装器不会往 Linux 桌面放图标；本脚本从 War3.exe 的 PE 资源提取官方图标，
# 并用安装目录自带的 128x128 war3.png 补齐大尺寸，写入 hicolor，
# 再生成指向隔离启动器 run-war3.sh 的桌面项。
#
# 图标产物不入库（写入 ~/.local/share/icons），复用 dreamina/extract-icon.py。
# 默认只改 Exec / Icon 两行并保留用户其它定制；改文件前生成 .bak；
# 不做崩溃后自动重启/守护。
#
# 用法：
#   ./war3/fix-war3-desktop.sh              # 提取图标 + 生成桌面入口（幂等）
#   ./war3/fix-war3-desktop.sh --status     # 只读体检
#   ./war3/fix-war3-desktop.sh --uninstall  # 恢复备份、删除生成的图标与入口
#   ./war3/fix-war3-desktop.sh --help
# License: Apache-2.0
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RUNNER="$SCRIPT_DIR/run-war3.sh"
EXTRACTOR="$SCRIPT_DIR/../dreamina/extract-icon.py"
GAME_DIR="${WAR3_DIR:-/opt/apps/war3/files}"
GAME_DIR="${GAME_DIR%/}"
MAIN_EXE="$GAME_DIR/War3.exe"
OFFICIAL_PNG="$GAME_DIR/war3.png"
ICON_NAME=war3
ICON_ROOT="${WAR3_ICON_ROOT:-$HOME/.local/share/icons}"
APP_ENTRY="${WAR3_APP_ENTRY:-$HOME/.local/share/applications/war3.desktop}"
DESKTOP_DIR="$(xdg-user-dir DESKTOP 2>/dev/null || true)"
[[ -n "$DESKTOP_DIR" && "$DESKTOP_DIR" != "$HOME" ]] || DESKTOP_DIR="$HOME/Desktop"
DESKTOP_ENTRY="$DESKTOP_DIR/war3.desktop"
BACKUP_DIR="$HOME/.local/share/war3-wine11/desktop-backups"
ENTRIES=("$APP_ENTRY" "$DESKTOP_ENTRY")
if [[ "$HOME/Desktop" != "$DESKTOP_DIR" && -d "$HOME/Desktop" ]]; then
    ENTRIES+=("$HOME/Desktop/war3.desktop")
fi
EXPECTED_EXEC="\"$RUNNER\""

usage() {
    cat <<EOF
用法：$(basename "$0") [--status|--uninstall|--help]

  （无参数）   提取官方图标并补齐大尺寸，创建/修正桌面入口并标记 trusted
  --status     只读体检：图标、入口 Icon/Exec/trusted、启动器与主程序
  --uninstall  恢复 .desktop 备份（新建的删除）、移除生成的图标
  --help       显示本说明

环境变量：WAR3_DIR / WAR3_ICON_ROOT / WAR3_APP_ENTRY
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
# ANCHOR-A
status() {
    local rc=0 s p f
    if [[ -x "$RUNNER" ]]; then
        echo "  [OK] 启动器：$RUNNER"
    else
        echo "  [--] 启动器缺失或不可执行：$RUNNER"; rc=1
    fi
    if [[ -s "$MAIN_EXE" && "$(head -c2 "$MAIN_EXE")" == "MZ" ]]; then
        echo "  [OK] 主程序：$MAIN_EXE"
    else
        echo "  [--] 主程序缺失或不是有效 PE：$MAIN_EXE"; rc=1
    fi
    for s in 16x16 48x48 128x128; do
        p="$ICON_ROOT/hicolor/$s/apps/$ICON_NAME.png"
        if [[ -s "$p" ]]; then
            echo "  [OK] 图标：$s ($(stat -c%s "$p") 字节)"
        else
            echo "  [--] 缺图标：$p"; rc=1
        fi
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

# 用安装目录自带的官方 128x128 图标补齐各尺寸（PNG 缩放，不引入外部依赖）
scale_official_icon() {
    [[ -f "$OFFICIAL_PNG" ]] || return 0
    python3 - "$OFFICIAL_PNG" "$ICON_ROOT/hicolor" "$ICON_NAME" <<'PY'
import sys
from pathlib import Path
try:
    from PIL import Image
except Exception:
    sys.exit(0)
src, root, name = Path(sys.argv[1]), Path(sys.argv[2]), sys.argv[3]
try:
    im = Image.open(src).convert("RGBA")
except Exception:
    sys.exit(0)
for size in (16, 24, 32, 48, 64, 96, 128, 256):
    d = root / f"{size}x{size}" / "apps"
    d.mkdir(parents=True, exist_ok=True)
    im.resize((size, size), Image.LANCZOS).save(d / f"{name}.png", "PNG")
PY
}
# ANCHOR-B
write_entry() {
    local f="$1" tmp
    if [[ -f "$f" && ! -w "$f" ]]; then
        echo "  警告：无写权限，跳过：$f" >&2
        return 0
    fi
    mkdir -p "$(dirname "$f")" 2>/dev/null || true
    if [[ ! -f "$f" ]]; then
        cat > "$f" <<EOF
[Desktop Entry]
Type=Application
Name=魔兽争霸 III
Name[en_US]=Warcraft III
Comment=魔兽争霸 III 冰封王座 1.24（Proton 11 / Wine 11，llvmpipe 软件渲染）
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
    [[ -x "$RUNNER" ]] || { echo "错误：启动器缺失：$RUNNER" >&2; exit 1; }
    [[ -s "$MAIN_EXE" ]] || { echo "错误：主程序不存在：$MAIN_EXE（请先安装 War3）" >&2; exit 1; }
    [[ -f "$EXTRACTOR" ]] || { echo "错误：图标提取器缺失：$EXTRACTOR" >&2; exit 1; }
    echo "[1/3] 从 War3.exe 提取官方图标 -> $ICON_ROOT/hicolor"
    python3 "$EXTRACTOR" --exe "$MAIN_EXE" --name "$ICON_NAME" \
        --icons-dir "$ICON_ROOT/hicolor"
    echo "[2/3] 用安装目录自带 war3.png 补齐各尺寸"
    scale_official_icon
    command -v gtk-update-icon-cache >/dev/null 2>&1 && \
        gtk-update-icon-cache -q -f -t "$ICON_ROOT/hicolor" 2>/dev/null || true
    echo "[3/3] 写入桌面入口"
    mkdir -p "$(dirname "$APP_ENTRY")"
    for f in "${ENTRIES[@]}"; do write_entry "$f"; done
    echo "  完成。体检：$(basename "$0") --status"
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
    echo "  已移除生成的图标；启动器与游戏目录未改动。"
}

case "${1:-}" in
    --help|-h) usage; exit 0 ;;
    --status) status; exit $? ;;
    --uninstall) uninstall; exit 0 ;;
    --*) echo "错误：未知参数：$1（用 --help 查看用法）" >&2; exit 1 ;;
    *) install_desktop ;;
esac
