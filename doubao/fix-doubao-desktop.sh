#!/usr/bin/env bash
# 至砺算：豆包官方 2.32.6 依赖 Wine 11 才有的 RtlGetDeviceFamilyInfoEnum。
# Deepin Wine 8.16 的旧 deb 入口与安装器生成的 Windows .lnk 快捷方式都会
# 把桌面点击带进旧前缀，直接 FATAL（win_util.cc(544) 找不到过程 0x7F）。
# 本脚本只修桌面入口，不碰客户端版本、不回退旧版、不做崩溃后自动拉起。
#
# 做三件事：
#   1) ~/Desktop/doubao-wine.desktop 与用户级 applications 入口的 Exec
#      统一指向 run-doubao-wine11.sh，去掉裸 --uri；
#   2) 给桌面 .desktop 打上 metadata::trusted=true（DING 双击所需）；
#   3) 把 ~/Desktop/豆包.lnk 移出桌面（保留可恢复），避免 DING 交给
#      spark-dwine-helper / deepin-wine8-stable。
#
# 用法：
#   ./fix-doubao-desktop.sh              # 应用（幂等）
#   ./fix-doubao-desktop.sh --status     # 只读检查
#   ./fix-doubao-desktop.sh --uninstall  # 恢复备份与 .lnk
#   ./fix-doubao-desktop.sh --help
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LAUNCHER="$SCRIPT_DIR/run-doubao-wine11.sh"
DESKTOP="${DOUBAO_DESKTOP_FILE:-$HOME/Desktop/doubao-wine.desktop}"
APP_ENTRY="${DOUBAO_APP_ENTRY:-$HOME/.local/share/applications/doubao-wine.desktop}"
LNK="${DOUBAO_LNK_FILE:-$HOME/Desktop/豆包.lnk}"
DISABLED_DIR="${DOUBAO_DISABLED_DIR:-$HOME/.local/share/doubao-wine11/disabled-desktop-shortcuts}"
BACKUP_DIR="${DOUBAO_BACKUP_DIR:-$HOME/.local/share/doubao-wine11/desktop-backups}"
EXPECTED_EXEC="Exec=\"$LAUNCHER\" %F"
LNK_NAME="$(basename "$LNK")"

# 备份不放桌面，否则 DING 会多显示一个 .bak 图标。
backup_path() {
    if [[ "$1" == "$DESKTOP" ]]; then
        printf '%s/desktop-entry.bak' "$BACKUP_DIR"
    else
        printf '%s/app-entry.bak' "$BACKUP_DIR"
    fi
}

usage() {
    cat <<EOF
用法：$(basename "$0") [--status|--uninstall|--help]

  （无参数）  修正桌面入口并打上 trusted 标记（幂等）
  --status    只读检查实际生效状态，未修好时返回 1
  --uninstall 恢复 .desktop 备份、移除 trusted 标记、放回 .lnk
  --help      显示本说明

可用环境变量覆盖路径：DOUBAO_DESKTOP_FILE / DOUBAO_APP_ENTRY /
DOUBAO_LNK_FILE / DOUBAO_DISABLED_DIR / DOUBAO_BACKUP_DIR。
桌面入口原文件备份在 ~/.local/share/doubao-wine11/desktop-backups/，
不会在桌面留下 .bak 图标。
EOF
}

is_trusted() {
    command -v gio >/dev/null 2>&1 || return 1
    gio info -a metadata::trusted "$1" 2>/dev/null | grep -q 'metadata::trusted: true'
}

write_entry() {
    local f="$1"
    mkdir -p "$(dirname "$f")"
    if [[ ! -f "$f" ]]; then
        cat > "$f" <<EOF
[Desktop Entry]
Type=Application
Name=豆包
Comment=豆包官方 2.32.6（Wine 11 / Proton 11）
Exec="$LAUNCHER" %F
Terminal=false
Categories=Utility;
StartupNotify=false
EOF
        echo "已创建：$f"
    else
        local bak
        bak="$(backup_path "$f")"
        if [[ ! -f "$bak" ]]; then
            mkdir -p "$BACKUP_DIR"
            cp -p "$f" "$bak"
            echo "已备份：$f -> $bak"
        fi
        python3 - "$f" "$LAUNCHER" <<'PY'
import sys
path, launcher = sys.argv[1], sys.argv[2]
with open(path, encoding='utf-8') as fh:
    lines = fh.read().splitlines()
out, inserted = [], False
for line in lines:
    if line.startswith('Exec='):
        if not inserted:
            out.append('Exec="%s" %%F' % launcher)
            inserted = True
        continue
    out.append(line)
if not inserted:
    for i, line in enumerate(out):
        if line.strip() == '[Desktop Entry]':
            out.insert(i + 1, 'Exec="%s" %%F' % launcher)
            inserted = True
            break
    if not inserted:
        out[:0] = ['[Desktop Entry]', 'Exec="%s" %%F' % launcher]
with open(path, 'w', encoding='utf-8') as fh:
    fh.write('\n'.join(out) + '\n')
PY
        echo "已统一 Exec：$f"
    fi
    chmod 755 "$f"
    if command -v gio >/dev/null 2>&1; then
        gio set "$f" metadata::trusted true
    else
        echo "警告：gio 不可用，未能设置 trusted 标记；请在桌面图标右键选择「允许启动」。" >&2
    fi
}

disable_lnk() {
    [[ -e "$LNK" ]] || return 0
    mkdir -p "$DISABLED_DIR"
    local dest="$DISABLED_DIR/$LNK_NAME"
    if [[ -e "$dest" ]]; then
        dest="$DISABLED_DIR/$LNK_NAME.$(date +%Y%m%d-%H%M%S)"
    fi
    mv "$LNK" "$dest"
    echo "已移出桌面（可恢复）：$LNK -> $dest"
}

status_report() {
    local rc=0 f
    for f in "$DESKTOP" "$APP_ENTRY"; do
        if [[ -f "$f" ]] && grep -qxF "$EXPECTED_EXEC" "$f"; then
            echo "[OK] Exec 指向 Wine 11：$f"
        else
            echo "[!!] Exec 未指向 Wine 11：$f"
            rc=1
        fi
    done
    for f in "$DESKTOP" "$APP_ENTRY"; do
        if [[ -x "$f" ]]; then
            echo "[OK] 可执行：$f"
        else
            echo "[!!] 缺少可执行位：$f"
            rc=1
        fi
        if is_trusted "$f"; then
            echo "[OK] trusted=true：$f"
        else
            echo "[!!] 未被标记为 trusted：$f"
            rc=1
        fi
    done
    if [[ -e "$LNK" ]]; then
        echo "[!!] 旧 Windows 快捷方式仍在桌面：$LNK"
        rc=1
    else
        echo "[OK] 桌面已无 $LNK_NAME"
    fi
    if [[ -x "$LAUNCHER" ]]; then
        echo "[OK] 启动器存在：$LAUNCHER"
    else
        echo "[!!] 启动器不存在或不可执行：$LAUNCHER"
        rc=1
    fi
    return "$rc"
}

uninstall() {
    local f bak
    for f in "$DESKTOP" "$APP_ENTRY"; do
        bak="$(backup_path "$f")"
        if [[ -f "$bak" ]]; then
            mv -f "$bak" "$f"
            echo "已恢复备份：$f"
        fi
        if command -v gio >/dev/null 2>&1; then
            gio set -t unset "$f" metadata::trusted 2>/dev/null || true
        fi
    done
    if [[ -f "$DISABLED_DIR/$LNK_NAME" && ! -e "$LNK" ]]; then
        mkdir -p "$(dirname "$LNK")"
        mv "$DISABLED_DIR/$LNK_NAME" "$LNK"
        echo "已放回：$LNK"
    fi
    rmdir "$DISABLED_DIR" 2>/dev/null || true
    echo "已卸载桌面入口修复（客户端版本与前缀未改动）。"
}

case "${1:-}" in
    --help|-h) usage ;;
    --status) status_report ;;
    --uninstall) uninstall ;;
    '')
        [[ -x "$LAUNCHER" ]] || { echo "错误：启动器不存在：$LAUNCHER" >&2; exit 1; }
        write_entry "$DESKTOP"
        write_entry "$APP_ENTRY"
        disable_lnk
        if command -v update-desktop-database >/dev/null 2>&1; then
            update-desktop-database "$HOME/.local/share/applications" 2>/dev/null || true
        fi
        echo
        echo "桌面点击现在统一走 Wine 11 / 官方 2.32.6，不再落到 Deepin Wine 8.16。"
        echo "注意：2.32.6 在 Wine 11 下仍可能不出窗口（IsUnMapped / 纯黑），"
        echo "      这是另一个未解问题，见 doubao/README.md；脚本不会回退旧版或自动重启。"
        echo
        status_report
        ;;
    *) echo "未知参数：$1" >&2; usage >&2; exit 2 ;;
esac
