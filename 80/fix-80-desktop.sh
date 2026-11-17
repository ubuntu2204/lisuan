#!/usr/bin/env bash
# 至砺算：图标与桌面集成和显卡无关。80 平台安装器只在 Wine 前缀里留了
# Windows .lnk，Linux 桌面没有图标；本脚本从本机 80launch.exe 的 PE 资源
# 提取官方图标写入 hicolor，并把桌面项指向隔离启动器 run-80duizhan.sh。
#
# 厂商图标产物不入库（写入 ~/.local/share/icons），复用已有的
# dreamina/extract-icon.py，只保留脚本与文档。
# 默认只改 Exec / Icon 两行并保留用户其它定制；改文件前生成 .bak；
# 不做崩溃后自动重启/守护。
#
# 用法：
#   ./80/fix-80-desktop.sh              # 提取图标 + 生成桌面入口（幂等）
#   ./80/fix-80-desktop.sh --status     # 只读体检
#   ./80/fix-80-desktop.sh --uninstall  # 恢复备份、删除生成的图标与入口
#   ./80/fix-80-desktop.sh --help
# License: Apache-2.0
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RUNNER="$SCRIPT_DIR/run-80duizhan.sh"
EXTRACTOR="$SCRIPT_DIR/../dreamina/extract-icon.py"
PREFIX="${E80_WINE11_PREFIX:-$HOME/.deepinwine/80duizhan-wine11}"
PREFIX="${PREFIX/#\~/$HOME}"
CUSTOM_DIR="${E80_INSTALL_DIR:-}"
ICON_NAME=80duizhan
ICON_ROOT="${E80_ICON_ROOT:-$HOME/.local/share/icons}"
APP_ENTRY="${E80_APP_ENTRY:-$HOME/.local/share/applications/80duizhan.desktop}"
DESKTOP_DIR="$(xdg-user-dir DESKTOP 2>/dev/null || true)"
[[ -n "$DESKTOP_DIR" && "$DESKTOP_DIR" != "$HOME" ]] || DESKTOP_DIR="$HOME/Desktop"
DESKTOP_ENTRY="$DESKTOP_DIR/80duizhan.desktop"
BACKUP_DIR="$HOME/.local/share/80duizhan-wine11/desktop-backups"
ENTRIES=("$APP_ENTRY" "$DESKTOP_ENTRY")
if [[ "$HOME/Desktop" != "$DESKTOP_DIR" && -d "$HOME/Desktop" ]]; then
    ENTRIES+=("$HOME/Desktop/80duizhan.desktop")
fi
EXPECTED_EXEC="\"$RUNNER\""

find_main() {
    local d found roots=()
    if [[ -n "$CUSTOM_DIR" ]]; then
        d="$PREFIX/drive_c/${CUSTOM_DIR#/}"
        [[ -s "$d/80launch.exe" ]] || return 1
        printf '%s\n' "$d/80launch.exe"
        return 0
    fi
    for d in "$PREFIX/drive_c/Program Files (x86)" \
             "$PREFIX/drive_c/Program Files" \
             "$PREFIX/drive_c/80duizhan"; do
        [[ -d "$d" ]] && roots+=("$d")
    done
    [[ ${#roots[@]} -gt 0 ]] || return 1
    found="$(find "${roots[@]}" -maxdepth 3 -type f -name 80launch.exe \
        -print -quit 2>/dev/null)"
    [[ -n "$found" ]] || return 1
    printf '%s\n' "$found"
}

MAIN_EXE="$(find_main || true)"

usage() {
    cat <<EOF
用法：$(basename "$0") [--status|--uninstall|--help]

  （无参数）   从 80launch.exe 提取官方图标到 hicolor，创建/修正桌面入口并标记 trusted
  --status     只读体检：图标、入口 Icon/Exec/trusted、启动器与主程序
  --uninstall  恢复 .desktop 备份（新建的删除）、移除生成的图标
  --help       显示本说明

环境变量：E80_WINE11_PREFIX / E80_INSTALL_DIR / E80_ICON_ROOT / E80_APP_ENTRY
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
    if [[ -x "$RUNNER" ]]; then
        echo "  [OK] 启动器：$RUNNER"
    else
        echo "  [--] 启动器缺失或不可执行：$RUNNER"; rc=1
    fi
    if [[ -n "$MAIN_EXE" && -s "$MAIN_EXE" && "$(head -c2 "$MAIN_EXE")" == "MZ" ]]; then
        echo "  [OK] 主程序：$MAIN_EXE"
    else
        echo "  [--] 主程序缺失或不是有效 PE：${MAIN_EXE:-未找到 80launch.exe}"; rc=1
    fi
    for s in 16x16 48x48 128x128 256x256; do
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
Name=80对战平台
Comment=80 对战平台（魔兽争霸 III，Proton 11 / Wine 11）
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
    [[ -n "$MAIN_EXE" && -s "$MAIN_EXE" ]] || { echo "错误：主程序不存在：$PREFIX/drive_c/${CUSTOM_DIR:-Program Files (x86)/80duizhan}/80launch.exe（请先安装平台）" >&2; exit 1; }
    [[ -f "$EXTRACTOR" ]] || { echo "错误：图标提取器缺失：$EXTRACTOR" >&2; exit 1; }
    echo "[1/2] 从 80launch.exe 提取官方图标 -> $ICON_ROOT/hicolor"
    python3 "$EXTRACTOR" --exe "$MAIN_EXE" --name "$ICON_NAME" \
        --icons-dir "$ICON_ROOT/hicolor"
    command -v gtk-update-icon-cache >/dev/null 2>&1 && \
        gtk-update-icon-cache -q -f -t "$ICON_ROOT/hicolor" 2>/dev/null || true
    echo "[2/2] 写入桌面入口"
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
    echo "  已移除生成的图标；启动器与安装目录未改动。"
}

case "${1:-}" in
    --help|-h) usage; exit 0 ;;
    --status) status; exit $? ;;
    --uninstall) uninstall; exit 0 ;;
    --*) echo "错误：未知参数：$1（用 --help 查看用法）" >&2; exit 1 ;;
    *) install_desktop ;;
esac
