#!/usr/bin/env bash
# 至砺算：豆包官方 2.32.6 依赖 Wine 11 才有的 RtlGetDeviceFamilyInfoEnum。
# Deepin Wine 8.16 的旧 deb 入口、旧 run-doubao-wine.sh 与安装器生成的
# Windows .lnk 快捷方式都会把点击带进旧前缀，直接 FATAL（win_util.cc(544)
# 找不到过程 0x7F）；旧前缀反复被强杀还会累积 running_exception，
# 弹出「当前版本启动发生异常，是否进行修复？」的豆包修复程序。
# 本脚本只修入口与排查残留状态，不碰客户端版本、不回退旧版、不做自动拉起。
#
# 做五件事：
#   1) 四个 .desktop 入口（桌面 / 用户级 / 系统级 / deb entries）的 Exec
#      统一指向 run-doubao-wine11.sh，去掉裸 --uri 与非标准 shebang；
#   2) 给 .desktop 打上 metadata::trusted=true（DING 双击所需）；
#   3) 把 ~/Desktop/豆包.lnk 移出桌面（保留可恢复），避免 DING 交给
#      spark-dwine-helper / deepin-wine8-stable；
#   4) 备份并清零旧 Deepin 前缀 repair/record.json 的 running_exception
#      （排查期间 wineserver -k / timeout 造成的累积，不是真实崩溃）；
#   5) 系统级与 deb entries 文件属主是当前用户时才改；不可写则明确提示。
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
SYSTEM_ENTRY="${DOUBAO_SYSTEM_ENTRY:-/usr/share/applications/doubao-wine.desktop}"
DEB_ENTRY="${DOUBAO_DEB_ENTRY:-/opt/apps/doubao-wine/entries/applications/doubao-wine.desktop}"
OLD_PREFIX_RECORD="${DOUBAO_OLD_RECORD:-$HOME/.deepinwine/doubao-wine/drive_c/users/ubuntu/AppData/Local/Doubao/User Data/repair/record.json}"
EXPECTED_EXEC="Exec=\"$LAUNCHER\" %F"
LNK_NAME="$(basename "$LNK")"
ENTRIES=("$DESKTOP" "$APP_ENTRY" "$SYSTEM_ENTRY" "$DEB_ENTRY")

# 备份不放桌面，否则 DING 会多显示一个 .bak 图标。
backup_path() {
    case "$1" in
        "$DESKTOP") printf '%s/desktop-entry.bak' "$BACKUP_DIR" ;;
        "$APP_ENTRY") printf '%s/app-entry.bak' "$BACKUP_DIR" ;;
        "$SYSTEM_ENTRY") printf '%s/system-entry.bak' "$BACKUP_DIR" ;;
        *) printf '%s/deb-entry.bak' "$BACKUP_DIR" ;;
    esac
}

usage() {
    cat <<EOF
用法：$(basename "$0") [--status|--uninstall|--help]

  （无参数）  统一四个入口、打 trusted 标记、移走 .lnk、清零旧前缀异常计数（幂等）
  --status    只读检查实际生效状态，未修好时返回 1
  --uninstall 恢复 .desktop / record.json 备份、移除 trusted 标记、放回 .lnk
  --help      显示本说明

可用环境变量覆盖路径：DOUBAO_DESKTOP_FILE / DOUBAO_APP_ENTRY /
DOUBAO_SYSTEM_ENTRY / DOUBAO_DEB_ENTRY / DOUBAO_LNK_FILE /
DOUBAO_OLD_RECORD / DOUBAO_DISABLED_DIR / DOUBAO_BACKUP_DIR。
原文件备份在 ~/.local/share/doubao-wine11/desktop-backups/，
不会在桌面留下 .bak 图标。
EOF
}

is_trusted() {
    command -v gio >/dev/null 2>&1 || return 1
    gio info -a metadata::trusted "$1" 2>/dev/null | grep -q 'metadata::trusted: true'
}

write_entry() {
    local f="$1"
    if [[ -f "$f" && ! -w "$f" ]]; then
        echo "警告：无写权限，跳过：$f（请手动把 Exec 改为 $LAUNCHER）" >&2
        return 0
    fi
    mkdir -p "$(dirname "$f")" 2>/dev/null || true
    if [[ ! -f "$f" ]]; then
        cat > "$f" <<EOF
[Desktop Entry]
Type=Application
Name=豆包
Comment=豆包官方 2.32.6（Steam Runtime / Wine 11）
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
# 非标准 shebang 会让部分文件管理器把 .desktop 当脚本执行，去掉它。
if lines and lines[0].startswith('#!'):
    lines = lines[1:]
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

# 旧 Deepin Wine 前缀跑不了 2.32.6；排查期间反复 wineserver -k / timeout
# 会把 running_exception 累到阈值，弹出「当前版本启动发生异常」修复窗口。
# 只清零这个计数并保留备份，不删账号数据、不禁用修复器、不锁版本。
clear_exception() {
    [[ -f "$OLD_PREFIX_RECORD" ]] || return 0
    python3 - "$OLD_PREFIX_RECORD" "$BACKUP_DIR/old-prefix-record.json.bak" <<'PY'
import json, os, shutil, sys
p, bak = sys.argv[1], sys.argv[2]
with open(p, encoding='utf-8') as fh:
    data = json.load(fh)
ex = data.get('running_exception') or {}
if not ex:
    print('旧前缀 running_exception 已为空')
    sys.exit(0)
if not os.path.exists(bak):
    os.makedirs(os.path.dirname(bak), exist_ok=True)
    shutil.copy2(p, bak)
    print('已备份旧前缀记录：%s' % bak)
data['running_exception'] = {}
with open(p, 'w', encoding='utf-8') as fh:
    json.dump(data, fh, ensure_ascii=False)
print('已清零 running_exception：%s' % ex)
PY
}

status_report() {
    local rc=0 f
    for f in "${ENTRIES[@]}"; do
        if [[ -f "$f" ]] && grep -qxF "$EXPECTED_EXEC" "$f"; then
            echo "[OK] Exec 指向 Wine 11：$f"
        else
            echo "[!!] Exec 未指向 Wine 11：$f"
            rc=1
        fi
        if [[ -f "$f" ]] && head -n 1 "$f" | grep -q '^#!'; then
            echo "[!!] 仍带非标准 shebang：$f"
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
    if [[ -f "$OLD_PREFIX_RECORD" ]] && ! grep -q '"running_exception": *{}' "$OLD_PREFIX_RECORD"; then
        echo "[!!] 旧前缀仍有 running_exception：$OLD_PREFIX_RECORD"
        rc=1
    else
        echo "[OK] 旧前缀 running_exception 已清零"
    fi
    if grep -q 'run-doubao-wine11.sh' "$SCRIPT_DIR/run-doubao-wine.sh" 2>/dev/null; then
        echo "[OK] 旧启动器已转发到 Wine 11"
    else
        echo "[!!] 旧启动器仍会走 Deepin Wine 8.16：$SCRIPT_DIR/run-doubao-wine.sh"
        rc=1
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
    for f in "${ENTRIES[@]}"; do
        bak="$(backup_path "$f")"
        if [[ -f "$bak" ]]; then
            mv -f "$bak" "$f"
            echo "已恢复备份：$f"
        fi
        if command -v gio >/dev/null 2>&1; then
            gio set -t unset "$f" metadata::trusted 2>/dev/null || true
        fi
    done
    if [[ -f "$BACKUP_DIR/old-prefix-record.json.bak" && -f "$OLD_PREFIX_RECORD" ]]; then
        cp -p "$BACKUP_DIR/old-prefix-record.json.bak" "$OLD_PREFIX_RECORD"
        echo "已恢复旧前缀 running_exception 记录"
    fi
    if [[ -f "$DISABLED_DIR/$LNK_NAME" && ! -e "$LNK" ]]; then
        mkdir -p "$(dirname "$LNK")"
        mv "$DISABLED_DIR/$LNK_NAME" "$LNK"
        echo "已放回：$LNK"
    fi
    rmdir "$DISABLED_DIR" 2>/dev/null || true
    echo "已卸载桌面入口修复（客户端版本与前缀账号数据未改动）。"
}

case "${1:-}" in
    --help|-h) usage ;;
    --status) status_report ;;
    --uninstall) uninstall ;;
    '')
        [[ -x "$LAUNCHER" ]] || { echo "错误：启动器不存在：$LAUNCHER" >&2; exit 1; }
        for f in "${ENTRIES[@]}"; do
            write_entry "$f"
        done
        disable_lnk
        clear_exception
        if command -v update-desktop-database >/dev/null 2>&1; then
            update-desktop-database "$HOME/.local/share/applications" 2>/dev/null || true
        fi
        echo
        echo "所有豆包入口现在统一走 Wine 11 / 官方 2.32.6，不再落到 Deepin Wine 8.16；"
        echo "旧前缀的异常计数已清零，「当前版本启动发生异常」修复窗不会再因排查强杀而弹出。"
        echo "不回退旧版、不禁用修复器、不做崩溃后自动重启。"
        echo
        status_report
        ;;
    *) echo "未知参数：$1" >&2; usage >&2; exit 2 ;;
esac
