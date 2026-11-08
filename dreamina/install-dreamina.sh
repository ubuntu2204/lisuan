#!/usr/bin/env bash
# 至砺算：即梦 AI 官方 Windows 客户端在 Wine 11 下的渲染需绕开 ANGLE 的
# WGL_NV_DX_interop2 要求，安装链路本身与显卡无关；本脚本只负责安装与桌面集成，
# 不改驱动、不注入全局 gf 环境、不做崩溃后自动拉起。
#
# 用官方 NSIS 安装器在 Proton 11.0（wine-11.0）独立前缀里安装/升级「即梦 AI」，
# 并从官方 exe 的 RT_ICON 资源提取图标、写好可信的桌面入口。
#
# ★ 关键坑（2026-11-08 实测）：Wine 自带的 powershell 是空壳 stub，wmain 一律
#   exit 0。electron-builder 安装器用它探测「旧版本进程是否还在跑」，第 3 步
#   `if ((Get-CimInstance ...).Count -gt 0) { exit 0 } else { exit 1 }` 被误判成
#   「有进程在跑」，/S 直接 Abort（返回码 2、什么都没装）。本脚本把前缀里的
#   powershell.exe 换成 powershell-stub.c 编译的替身：凡命令行出现
#   Get-CimInstance 就返回 1（= 没有进程）。
#
# ★ 图标坑（2026-11-09）：旧 desktop 模板把 Icon 指向豆包图标，且没有把官方
#   图标装进 hicolor 主题、桌面项也没有 metadata::trusted，桌面/应用列表因此
#   显示不出即梦图标。现在由 extract-icon.py 从本机官方 exe 提取 7 个 PNG 尺寸，
#   写入 ~/.local/share/icons/hicolor/*/apps/dreamina.png，桌面项统一用
#   Icon=dreamina 与动态生成的 Exec。
#
# 用法：
#   ./dreamina/install-dreamina.sh [安装器.exe]        # 完整安装/升级 + 桌面集成
#   ./dreamina/install-dreamina.sh --desktop-only      # 只重建图标/入口（不跑安装器）
#   ./dreamina/install-dreamina.sh --status            # 只读体检
#   ./dreamina/install-dreamina.sh --uninstall         # 撤销桌面集成与替身（保留前缀）
#   ./dreamina/install-dreamina.sh --help
#
# 环境变量：DREAMINA_PROTON / DREAMINA_WINE11_PREFIX / DREAMINA_INSTALL_TIMEOUT /
#   DREAMINA_INSTALL_LOG / DREAMINA_WINEDEBUG / DREAMINA_NO_STOP /
#   XDG_DATA_HOME / DREAMINA_DESKTOP_FILE / DREAMINA_APP_ENTRY /
#   DREAMINA_ICON_ROOT / DREAMINA_ICON_NAME / DREAMINA_BACKUP_DIR
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROTON="${DREAMINA_PROTON:-$HOME/.local/share/Steam/steamapps/common/Proton 11.0}"
PROTON="${PROTON/#\~/$HOME}"
PREFIX="${DREAMINA_WINE11_PREFIX:-$HOME/.deepinwine/dreamina-wine11}"
PREFIX="${PREFIX/#\~/$HOME}"
TIMEOUT="${DREAMINA_INSTALL_TIMEOUT:-900}"
LOG="${DREAMINA_INSTALL_LOG:-/tmp/dreamina-install-$(date +%Y%m%d-%H%M%S).log}"

DATA_HOME="${XDG_DATA_HOME:-}"
[[ "$DATA_HOME" == /* ]] || DATA_HOME="$HOME/.local/share"
DESKTOP_DIR="$HOME/Desktop"
if command -v xdg-user-dir >/dev/null 2>&1; then
    d="$(xdg-user-dir DESKTOP 2>/dev/null || true)"
    [[ -n "$d" && "$d" != "$HOME" ]] && DESKTOP_DIR="$d"
fi
DESKTOP="${DREAMINA_DESKTOP_FILE:-$DESKTOP_DIR/dreamina-wine.desktop}"
APP_ENTRY="${DREAMINA_APP_ENTRY:-$DATA_HOME/applications/dreamina-wine.desktop}"
ICON_ROOT="${DREAMINA_ICON_ROOT:-$DATA_HOME/icons}"
ICON_NAME="${DREAMINA_ICON_NAME:-dreamina}"
ICON_FILE="$ICON_ROOT/hicolor/256x256/apps/$ICON_NAME.png"
BACKUP_DIR="${DREAMINA_BACKUP_DIR:-$DATA_HOME/dreamina-wine11/desktop-backups}"
# DING 用 XDG 桌面目录（本机是 ~/桌面），但历史手工入口放在 ~/Desktop。
# 两个目录都存在时都修一遍：真正被监听的那个立刻出图标，遗留那份也不再指向豆包图标。
DESKTOP_ENTRIES=("$DESKTOP" "$APP_ENTRY")
if [[ -z "${DREAMINA_DESKTOP_FILE:-}" && "$HOME/Desktop" != "$DESKTOP_DIR" \
      && -d "$HOME/Desktop" ]]; then
    DESKTOP_ENTRIES+=("$HOME/Desktop/dreamina-wine.desktop")
fi

RUNNER="$SCRIPT_DIR/run-dreamina.sh"
TEMPLATE="$SCRIPT_DIR/dreamina-wine.desktop"
EXTRACTOR="$SCRIPT_DIR/extract-icon.py"
STUB_SRC="$SCRIPT_DIR/powershell-stub.c"
STUB_MARKER='DREAMINA_PS_STUB_V1'
WINE="$PROTON/files/bin/wine"
WINESERVER="$PROTON/files/bin/wineserver"
PS_TARGETS=(
    "$PREFIX/drive_c/windows/system32/WindowsPowerShell/v1.0/powershell.exe"
    "$PREFIX/drive_c/windows/syswow64/WindowsPowerShell/v1.0/powershell.exe"
)
WORK_DIR=""

cleanup() {
    if [[ -n "$WORK_DIR" && -d "$WORK_DIR" ]]; then
        rm -rf "$WORK_DIR"
    fi
}
trap cleanup EXIT

die() { echo "错误：$*" >&2; exit 1; }

usage() {
    cat <<EOF
用法：$(basename "$0") [选项] [安装器.exe]

  （无参数）   完整安装/升级：自动找安装器 → 初始化前缀 → 修 powershell stub
               → NSIS /S 静默安装 → 校验主程序 → 提取官方图标 → 写桌面入口
  --desktop-only 只重建图标与桌面入口，不跑安装器（修「没有图标」用这个）
  --status      只读体检：前缀、替身、主程序、图标、桌面入口与实际生效标志
  --uninstall   撤销桌面入口/图标/powershell 替身；Wine 前缀与账号数据保留
  --help        显示本说明

安装器查找顺序：命令行参数 > dreamina/Dreamina-Desktop-*-x64-setup.exe >
~/下载 > ~/Downloads；多个版本按文件名里的版本号取最高（不依赖 mtime）。

桌面入口写到 XDG 桌面目录（xdg-user-dir DESKTOP，本机是 ~/桌面）、
~/.local/share/applications；若 ~/Desktop 存在且不是 XDG 桌面目录，历史手工入口
也一并修正。图标由 extract-icon.py 从本机官方 exe 提取，不入库。

环境变量：
  DREAMINA_PROTON / DREAMINA_WINE11_PREFIX    Proton 与 Wine 前缀路径
  DREAMINA_INSTALL_TIMEOUT / _LOG / _WINEDEBUG 安装超时、日志、Wine 调试级别
  DREAMINA_NO_STOP=1                          检测到本前缀进程时直接报错，不关闭
  DREAMINA_DESKTOP_FILE / DREAMINA_APP_ENTRY / DREAMINA_ICON_ROOT
  DREAMINA_ICON_NAME / DREAMINA_BACKUP_DIR    桌面入口、图标与备份位置
EOF
}

# 只在 wine 进程树上设置 Proton 运行库环境，不污染当前 shell / Steam 客户端。
wine_env() {
    local libs="$PROTON/files/lib/x86_64-linux-gnu:$PROTON/files/lib/i386-linux-gnu"
    [[ -n "${LD_LIBRARY_PATH:-}" ]] && libs="$libs:$LD_LIBRARY_PATH"
    env WINEPREFIX="$PREFIX" \
        WINEDLLPATH="$PROTON/files/lib/vkd3d:$PROTON/files/lib/wine" \
        LD_LIBRARY_PATH="$libs" \
        WINEDLLOVERRIDES='mscoree,mshtml=' \
        WINEDEBUG="${DREAMINA_WINEDEBUG:--all}" \
        "$@"
}

# 新版替身带唯一标记；2026-11-08 手工部署的旧替身没有标记，但含
# Get-CimInstance 字符串，而 Wine 原 stub 不含，故仍视作可用（兼容旧前缀）。
stub_ready() {
    local t="$1"
    [[ -f "$t" ]] || return 1
    LC_ALL=C grep -qaF "$STUB_MARKER" "$t" && return 0
    LC_ALL=C grep -qaF 'Get-CimInstance' "$t" && return 0
    return 1
}

backup_path() {
    case "$1" in
        "$DESKTOP")   printf '%s/desktop-entry.bak' "$BACKUP_DIR" ;;
        "$APP_ENTRY") printf '%s/app-entry.bak' "$BACKUP_DIR" ;;
        *)            printf '%s/legacy-desktop-entry.bak' "$BACKUP_DIR" ;;
    esac
}

is_trusted() {
    command -v gio >/dev/null 2>&1 || return 1
    local out
    out="$(gio info -a metadata::trusted "$1" 2>/dev/null || true)"
    grep -q 'metadata::trusted: true' <<< "$out"
}

set_trusted() {
    if command -v gio >/dev/null 2>&1; then
        gio set "$1" metadata::trusted true >/dev/null 2>&1 || \
            echo "警告：无法设置 trusted 标记：$1（请在桌面图标右键选「允许启动」）" >&2
    else
        echo "警告：gio 不可用，未能设置 trusted 标记：$1" >&2
    fi
}

install_dir() {
    find "$PREFIX/drive_c/users" -maxdepth 8 -type d \
         -path '*/AppData/Local/Programs/dreamina-desktop' \
         -print -quit 2>/dev/null || true
}

installed_main_exe() {
    local dir exe
    dir="$(install_dir)"
    [[ -n "$dir" ]] || return 0
    exe="$dir/即梦 AI.exe"
    if [[ -f "$exe" ]]; then
        printf '%s\n' "$exe"
        return 0
    fi
    exe="$(find "$dir" -maxdepth 1 -type f -name '*.exe' \
           ! -name 'Uninstall*' -print -quit 2>/dev/null || true)"
    [[ -n "$exe" ]] && printf '%s\n' "$exe"
    return 0
}

# 多个安装器按文件名里的版本号选最高；仓库不保留 mtime，禁止用 ls -t。
find_installer() {
    local -a found=()
    local p
    if [[ -n "${1:-}" ]]; then
        [[ -f "$1" ]] || return 1
        printf '%s\n' "$1"
        return 0
    fi
    for p in "$SCRIPT_DIR"/Dreamina-Desktop-*-x64-setup.exe \
             "$HOME"/下载/Dreamina-Desktop-*-x64-setup.exe \
             "$HOME"/Downloads/Dreamina-Desktop-*-x64-setup.exe; do
        [[ -f "$p" ]] && found+=("$p")
    done
    (( ${#found[@]} )) || return 1
    python3 - "${found[@]}" <<'PY'
import os, re, sys

def key(path):
    m = re.search(r'Dreamina-Desktop-([0-9][0-9.]*)-x64-setup\.exe$',
                  os.path.basename(path), re.I)
    if not m:
        return (0,)
    return tuple(int(x) if x.isdigit() else 0 for x in m.group(1).split('.'))

print(max(sys.argv[1:], key=key))
PY
}

validate_installer() {
    local f="$1" size
    [[ -f "$f" && -s "$f" ]] || die "安装器不存在或为空：$f"
    [[ "$(head -c2 "$f")" == "MZ" ]] || die "不是 Windows PE 安装器（缺少 MZ 头）：$f"
    size="$(stat -c%s "$f")"
    (( size > 1024 * 1024 )) || die "安装器文件异常小（$size 字节），可能是下载未完成：$f"
    echo "    安装器：$f（$size 字节）"
}

write_desktop_entry() {
    python3 - "$1" "$RUNNER" "$ICON_NAME" "$TEMPLATE" <<'PY'
import os, sys

path, runner, icon, template = sys.argv[1:5]
try:
    with open(path, encoding='utf-8') as fh:
        lines = fh.read().splitlines()
except FileNotFoundError:
    with open(template, encoding='utf-8') as fh:
        lines = fh.read().splitlines()

exec_line = 'Exec="%s" %%F' % runner
icon_line = 'Icon=%s' % icon
out, have_exec, have_icon = [], False, False
for line in lines:
    # 非标准 shebang 会让部分文件管理器把 .desktop 当脚本执行。
    if line.startswith('#!') or line.startswith('Encoding='):
        continue
    if line.startswith('MimeType=') and line.split('=', 1)[1].strip() == '':
        continue
    if line.startswith('Exec='):
        if not have_exec:
            out.append(exec_line)
            have_exec = True
        continue
    if line.startswith('Icon='):
        if not have_icon:
            out.append(icon_line)
            have_icon = True
        continue
    out.append(line)


def insert_after_header(value):
    for i, line in enumerate(out):
        if line.strip() == '[Desktop Entry]':
            out.insert(i + 1, value)
            return
    out[:0] = ['[Desktop Entry]', value]


for key, value in (('Type', 'Application'), ('Name', '即梦 AI'),
                   ('Terminal', 'false')):
    if not any(line.startswith(key + '=') for line in out):
        insert_after_header('%s=%s' % (key, value))
if not have_icon:
    insert_after_header(icon_line)
if not have_exec:
    insert_after_header(exec_line)

tmp = path + '.tmp'
with open(tmp, 'w', encoding='utf-8') as fh:
    fh.write('\n'.join(out) + '\n')
os.replace(tmp, path)
PY
}

install_desktop() {
    local src="$1" step="${2:-[4/5]}" f bak
    [[ -f "$EXTRACTOR" ]] || die "缺少图标提取工具：$EXTRACTOR"
    [[ -f "$TEMPLATE" ]] || die "缺少桌面入口模板：$TEMPLATE"
    [[ -x "$RUNNER" ]] || die "启动器不存在或不可执行：$RUNNER"
    echo "$step 提取官方图标并写入桌面入口"
    python3 "$EXTRACTOR" --exe "$src" \
            --icons-dir "$ICON_ROOT/hicolor" --name "$ICON_NAME" || \
        die "官方图标提取失败（图标源：$src）"
    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
        gtk-update-icon-cache -q -f -t "$ICON_ROOT/hicolor" >/dev/null 2>&1 || true
    fi
    mkdir -p "$BACKUP_DIR" || die "无法创建备份目录：$BACKUP_DIR"
    for f in "${DESKTOP_ENTRIES[@]}"; do
        mkdir -p "$(dirname "$f")" || die "无法创建目录：$(dirname "$f")"
        if [[ -f "$f" ]]; then
            bak="$(backup_path "$f")"
            if [[ ! -f "$bak" ]]; then
                cp -p "$f" "$bak" || die "备份失败：$f"
                echo "    已备份：$f -> $bak"
            fi
        fi
        write_desktop_entry "$f" || die "写入桌面入口失败：$f"
        chmod 755 "$f"
        set_trusted "$f"
        echo "    已写入：$f"
    done
    if command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database "$DATA_HOME/applications" >/dev/null 2>&1 || true
    fi
}

# 只找属于本前缀的 Wine 进程；不按 pgrep -f "$PREFIX"（WINEPREFIX 不在命令行，
# 只在 /proc/<pid>/environ 里），也不碰其它前缀。
prefix_pids() {
    local pid comm cmd environ
    for pid in $(pgrep -f 'dreamina|即梦|wineserver|winedevice\.exe|explorer\.exe' \
                 2>/dev/null || true); do
        [[ "$pid" == "$$" || "$pid" == "$PPID" ]] && continue
        # pgrep 返回的 PID 可能在读取前就退出；先查可读，避免 < 重定向报错。
        [[ -r "/proc/$pid/comm" && -r "/proc/$pid/cmdline" \
           && -r "/proc/$pid/environ" ]] || continue
        comm="$(cat "/proc/$pid/comm" 2>/dev/null || true)"
        case "$comm" in bash|sh|dash|cat|tr|grep|pgrep|python3*) continue ;; esac
        cmd="$(tr '\0' ' ' < "/proc/$pid/cmdline" 2>/dev/null || true)"
        [[ "$comm" == wine* || "$comm" == wineserver* || "$cmd" == *'.exe'* ]] || continue
        environ="$(tr '\0' '\n' < "/proc/$pid/environ" 2>/dev/null || true)"
        grep -qxF "WINEPREFIX=$PREFIX" <<< "$environ" || continue
        printf '%s\n' "$pid"
    done
}

stop_running() {
    local pids
    pids="$(prefix_pids)"
    [[ -z "$pids" ]] && return 0
    if [[ "${DREAMINA_NO_STOP:-0}" == "1" ]]; then
        die "本前缀仍有进程在运行（$(echo "$pids" | tr '\n' ' ')）；请退出即梦 AI 后重试，或去掉 DREAMINA_NO_STOP=1"
    fi
    echo "    检测到本前缀有进程（$(echo "$pids" | tr '\n' ' ')），安装前一次性关闭"
    echo "    说明：只执行 wineserver -k，不做自动拉起/看门狗"
    wine_env "$WINESERVER" -k >/dev/null 2>&1 || true
    for _ in $(seq 1 30); do
        [[ -z "$(prefix_pids)" ]] && return 0
        sleep 1
    done
    die "仍有进程占用前缀 $PREFIX，请手动结束后重试"
}

ensure_prefix() {
    if [[ -d "$PREFIX/drive_c" ]]; then
        echo "[1/5] 复用已有前缀：$PREFIX"
        return 0
    fi
    echo "[1/5] 初始化 Wine 前缀：$PREFIX"
    mkdir -p "$PREFIX" || die "无法创建前缀目录：$PREFIX"
    if ! wine_env timeout 300 "$WINE" wineboot --init >>"$LOG" 2>&1; then
        die "前缀初始化失败，详见：$LOG"
    fi
    [[ -d "$PREFIX/drive_c" ]] || die "前缀初始化后仍没有 drive_c：$PREFIX"
}

ensure_powershell_stub() {
    local t need=0
    for t in "${PS_TARGETS[@]}"; do
        if ! stub_ready "$t"; then
            need=1
        fi
    done
    if [[ $need -eq 0 ]]; then
        echo "[2/5] powershell 替身已就位，跳过"
        return 0
    fi
    echo "[2/5] 替换 powershell stub（修 electron-builder 安装器 Abort）"
    [[ -f "$STUB_SRC" ]] || die "缺少替身源码：$STUB_SRC"
    command -v i686-w64-mingw32-gcc >/dev/null || \
        die "缺少 i686-w64-mingw32-gcc（sudo apt install gcc-mingw-w64-i686）"
    WORK_DIR="$(mktemp -d /tmp/dreamina-install.XXXXXX)" || die "无法创建临时目录"
    if ! i686-w64-mingw32-gcc -O2 -m32 -o "$WORK_DIR/powershell.exe" "$STUB_SRC"; then
        die "编译 powershell 替身失败"
    fi
    LC_ALL=C grep -qaF "$STUB_MARKER" "$WORK_DIR/powershell.exe" || \
        die "编译产物缺少识别标记 $STUB_MARKER"
    for t in "${PS_TARGETS[@]}"; do
        mkdir -p "$(dirname "$t")" || die "无法创建目录：$(dirname "$t")"
        # 只在第一次、且目标确实是 Wine 原件时留备份，便于还原。
        if [[ -f "$t" && ! -f "$t.wine-stub" ]] && ! stub_ready "$t"; then
            cp -p "$t" "$t.wine-stub" || die "备份失败：$t"
        fi
        cp "$WORK_DIR/powershell.exe" "$t" || die "写入替身失败：$t"
        LC_ALL=C grep -qaF "$STUB_MARKER" "$t" || die "替身校验失败：$t"
    done
    rm -rf "$WORK_DIR"
    WORK_DIR=""
}

restore_powershell_stub() {
    local t
    for t in "${PS_TARGETS[@]}"; do
        if [[ -f "$t.wine-stub" ]]; then
            mv -f "$t.wine-stub" "$t"
            echo "    已还原 Wine 原 powershell：$t"
        fi
    done
}

status_report() {
    local rc=0 f t exe dir sig
    echo "=== 即梦 AI 安装/桌面状态 ==="
    if [[ -x "$WINE" ]]; then
        echo "[OK] Proton wine：$WINE"
    else
        echo "[!!] Proton wine 不可执行：$WINE"
        rc=1
    fi
    if [[ -d "$PREFIX/drive_c" ]]; then
        echo "[OK] Wine 前缀：$PREFIX"
    else
        echo "[!!] Wine 前缀不存在：$PREFIX"
        rc=1
    fi
    for t in "${PS_TARGETS[@]}"; do
        if stub_ready "$t"; then
            echo "[OK] powershell 替身：$t"
        else
            echo "[!!] powershell 替身缺失（Wine 原 stub 会让安装 Abort）：$t"
            rc=1
        fi
    done
    exe="$(installed_main_exe)"
    if [[ -n "$exe" && -s "$exe" && "$(head -c2 "$exe")" == "MZ" ]]; then
        echo "[OK] 主程序：$exe（$(stat -c%s "$exe") 字节）"
        dir="$(dirname "$exe")"
        if [[ -s "$dir/resources/app.asar" ]]; then
            echo "[OK] app.asar：$(stat -c%s "$dir/resources/app.asar") 字节"
        else
            echo "[!!] 缺少 resources/app.asar：$dir"
            rc=1
        fi
    else
        echo "[!!] 未找到有效主程序（先执行完整安装）"
        rc=1
    fi
    sig=""
    [[ -f "$ICON_FILE" ]] && \
        sig="$(head -c8 "$ICON_FILE" | od -An -tx1 | tr -d ' \n')"
    if [[ "$sig" == "89504e470d0a1a0a" ]]; then
        echo "[OK] 图标：$ICON_FILE"
    else
        echo "[!!] 图标缺失或不是 PNG：$ICON_FILE"
        rc=1
    fi
    for f in "${DESKTOP_ENTRIES[@]}"; do
        if [[ ! -f "$f" ]]; then
            echo "[!!] 桌面入口不存在：$f"
            rc=1
            continue
        fi
        grep -qxF "Exec=\"$RUNNER\" %F" "$f" && \
            echo "[OK] Exec：$f" || { echo "[!!] Exec 未指向启动器：$f"; rc=1; }
        grep -qxF "Icon=$ICON_NAME" "$f" && \
            echo "[OK] Icon=$ICON_NAME：$f" || { echo "[!!] Icon 不正确：$f"; rc=1; }
        if grep -qi 'doubao' "$f"; then
            echo "[!!] 仍残留豆包图标路径：$f"
            rc=1
        fi
        if is_trusted "$f"; then
            echo "[OK] trusted=true：$f"
        else
            echo "[!!] 未被标记为 trusted：$f"
            rc=1
        fi
    done
    if [[ -x "$RUNNER" ]]; then
        echo "[OK] 启动器：$RUNNER"
    else
        echo "[!!] 启动器不存在或不可执行：$RUNNER"
        rc=1
    fi
    f="$(find_installer "" || true)"
    if [[ -n "$f" ]]; then
        echo "[信息] 可用安装器：$f"
    else
        echo "[信息] 未找到安装器（客户端已安装时可忽略）"
    fi
    return "$rc"
}

uninstall_desktop() {
    local f bak
    for f in "${DESKTOP_ENTRIES[@]}"; do
        if command -v gio >/dev/null 2>&1; then
            gio set -t unset "$f" metadata::trusted >/dev/null 2>&1 || true
        fi
        bak="$(backup_path "$f")"
        if [[ -f "$bak" ]]; then
            mkdir -p "$(dirname "$f")" || true
            mv -f "$bak" "$f"
            echo "    已恢复备份：$f"
        elif [[ -f "$f" ]] && grep -qF "$RUNNER" "$f"; then
            rm -f "$f"
            echo "    已移除：$f"
        fi
    done
    if [[ -d "$ICON_ROOT/hicolor" ]]; then
        find "$ICON_ROOT/hicolor" -type f \
             -path "*/apps/$ICON_NAME.png" -delete 2>/dev/null || true
        echo "    已移除图标主题里的 $ICON_NAME.png"
    fi
    restore_powershell_stub
    echo "已撤销桌面入口/图标/powershell 替身；前缀 $PREFIX 与账号数据保留。"
    echo "卸载客户端本体请用前缀里的「Uninstall 即梦 AI.exe」，本脚本不删除它。"
}

run_install() {
    local rc=0 dir exe
    [[ "$TIMEOUT" =~ ^[0-9]+$ ]] || die "DREAMINA_INSTALL_TIMEOUT 必须是秒数：$TIMEOUT"
    [[ -x "$WINE" ]] || die "找不到 Proton 的 wine：$WINE"
    [[ -x "$WINESERVER" ]] || die "找不到 Proton 的 wineserver：$WINESERVER"
    INSTALLER="$(find_installer "${INSTALLER_ARG:-}")" || {
        echo "错误：未找到官方安装器 Dreamina-Desktop-*-x64-setup.exe。" >&2
        echo "请从官网下载后放进 $SCRIPT_DIR、~/下载 或 ~/Downloads，" >&2
        echo "或用：$(basename "$0") /路径/安装器.exe" >&2
        exit 1
    }
    validate_installer "$INSTALLER"
    mkdir -p "$(dirname "$LOG")" || die "无法创建日志目录：$(dirname "$LOG")"
    : >> "$LOG" || die "无法写入安装日志：$LOG"
    echo "    前缀：$PREFIX"
    echo "    日志：$LOG"

    ensure_prefix
    stop_running
    ensure_powershell_stub

    echo "[3/5] 运行 NSIS 静默安装器 /S（超时 ${TIMEOUT}s）"
    if wine_env timeout "$TIMEOUT" "$WINE" "$INSTALLER" /S >"$LOG" 2>&1; then
        rc=0
    else
        rc=$?
    fi
    echo "    安装器返回码：$rc"
    case "$rc" in
        0) ;;
        2)   die "安装器 Abort（通常是旧进程探测/文件占用）；详见 $LOG" ;;
        124) die "安装超时（${TIMEOUT}s）；可提高 DREAMINA_INSTALL_TIMEOUT，详见 $LOG" ;;
        *)   die "安装器失败（返回码 $rc）；详见 $LOG" ;;
    esac
    wine_env timeout 30 "$WINESERVER" -w >/dev/null 2>&1 || true

    dir="$(install_dir)"
    [[ -n "$dir" ]] || die "安装目录不存在（返回码 0 但没装上）：$PREFIX/drive_c/users/*/AppData/Local/Programs/dreamina-desktop"
    exe="$(installed_main_exe)"
    [[ -n "$exe" && -s "$exe" && "$(head -c2 "$exe")" == "MZ" ]] || \
        die "主程序缺失或不是有效 PE：$dir"
    [[ -s "$dir/resources/app.asar" ]] || die "缺少 resources/app.asar，安装不完整：$dir"
    echo "    安装目录：$dir"
    echo "    主程序  ：$exe"
    echo "    app.asar：$(stat -c%s "$dir/resources/app.asar") 字节"

    install_desktop "$exe"
    echo "[5/5] 安装完成。启动：$RUNNER"
    for f in "${DESKTOP_ENTRIES[@]}"; do
        echo "      桌面项：$f"
    done
    echo "      体检  ：$(basename "$0") --status"
}

MODE="install"
INSTALLER_ARG=""
INSTALLER=""
case "${1:-}" in
    --help|-h) usage; exit 0 ;;
    --status) MODE=status ;;
    --uninstall) MODE=uninstall ;;
    --desktop-only) MODE=desktop ;;
    --*) die "未知参数：$1（用 --help 查看用法）" ;;
    '') ;;
    *) INSTALLER_ARG="$1" ;;
esac
if (( $# > 1 )); then
    die "多余参数：$*（用 --help 查看用法）"
fi

case "$MODE" in
    status) status_report ;;
    uninstall) uninstall_desktop ;;
    desktop)
        exe="$(installed_main_exe)"
        [[ -n "$exe" && -s "$exe" && "$(head -c2 "$exe")" == "MZ" ]] || \
            die "客户端未安装（找不到有效主程序），请先执行完整安装"
        echo "[1/2] 只重建图标与桌面入口（不跑安装器）"
        install_desktop "$exe" "[2/2]"
        echo "[2/2] 完成"
        status_report || true
        ;;
    install) run_install ;;
esac
