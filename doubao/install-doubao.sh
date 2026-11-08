#!/usr/bin/env bash
# 用官方在线安装器在 Wine 11（Proton 11.0）前缀内安装/升级豆包 Windows 客户端。
#
# 安装器是字节跳动 Saman 在线安装器：它自身只带一个安装器版本号
# （内嵌 product_version，如 2.25.16），真正的客户端包由服务器下发，
# 所以装出来的版本取决于官方当前最新版，而不是文件名里那串数字。
#
# 本脚本不做任何版本回退、锁版本或禁用更新器（见 .codebuddy/rules/豆包客户端版本约定.md），
# 只负责五件事：
#   1) 先向官方接口查一下服务器当前下发哪个版本，报给用户；
#   2) 关掉该前缀内正在运行的豆包——安装器无法覆盖正在使用的文件；
#   3) 跑起安装器并点掉界面上的「立即安装」，等安装日志出现 _OnInstallSuccess；
#   4) 安装后跑一次与版本无关的前缀层修复（WinSxS 软链接 + LocalLow 目录）；
#   5) 统一桌面入口，避免安装器重建的 .lnk 把双击又带回 Deepin Wine 8.16。
#
# 关键事实（2026-11-07 实测）：`/S` 对这个 Saman 在线安装器**不是静默安装**。
# 日志会记 CmdLine:/S 但紧跟着“saman command line install mode is ”（空），
# 然后弹出 718x478 的安装窗口停在「立即安装」上等人点击，因此本脚本靠
# 定位并点击按钮推进，而不是依赖 /S。
#
# 用法：
#   ./doubao/install-doubao.sh [安装器.exe 路径]
#   DOUBAO_WINE11_PREFIX=~/.deepinwine/doubao-wine11 ./doubao/install-doubao.sh
set -uo pipefail

PROTON="${DOUBAO_PROTON:-$HOME/.local/share/Steam/steamapps/common/Proton 11.0}"
PREFIX="${DOUBAO_WINE11_PREFIX:-$HOME/.deepinwine/doubao-wine11}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOG="${DOUBAO_INSTALL_LOG:-/tmp/doubao-install-$(date +%Y%m%d-%H%M%S).log}"
# 等待下载+安装的上限（秒）。在线包几百 MB，默认给 15 分钟。
TIMEOUT="${DOUBAO_INSTALL_TIMEOUT:-900}"

WINE="$PROTON/files/bin/wine"
WINESERVER="$PROTON/files/bin/wineserver"

# 环境变量必须与 Proton 自带 proton 脚本一致，否则 wine 找不到自带 DLL/宿主库。
export WINEPREFIX="$PREFIX"
export WINEDLLPATH="$PROTON/files/lib/vkd3d:$PROTON/files/lib/wine"
export LD_LIBRARY_PATH="$PROTON/files/lib/x86_64-linux-gnu:$PROTON/files/lib/i386-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export WINEDLLOVERRIDES='mscoree,mshtml='

[[ -x "$WINE" ]] || { echo "错误：找不到 Proton 的 wine：$WINE" >&2; exit 1; }

# 前缀不存在时自动初始化（与 dreamina/install-dreamina.sh 行为一致）：
# 首次从 install.py 菜单安装时本机往往还没有 Wine 11 前缀，
# 直接报错退出的话用户永远装不上。wineboot --init 是幂等的。
if [[ ! -d "$PREFIX/drive_c" ]]; then
    echo "初始化 Wine 11 前缀：$PREFIX"
    mkdir -p "$PREFIX"
    WINEDEBUG=-all timeout 300 "$WINE" wineboot --init >/dev/null 2>&1
fi
[[ -d "$PREFIX/drive_c" ]] || { echo "错误：Wine 11 前缀初始化失败：$PREFIX" >&2; exit 1; }

# 只读探测已安装的客户端版本，仅用于报告前后变化，不参与任何回退判断。
detect_version() {
    local manifest
    for manifest in \
        "$PREFIX"/drive_c/users/*/AppData/Local/Doubao/Application/app/manifest.json \
        "$PREFIX/drive_c/Program Files/Doubao/app/manifest.json"
    do
        [[ -f "$manifest" ]] || continue
        sed -n 's/^[[:space:]]*"version":[[:space:]]*"\([^"]*\)".*/\1/p' "$manifest" | head -n 1
        return 0
    done
    printf '未安装\n'
}

# 直接向官方接口问“现在给 Windows x64 下发哪个版本”。
# 这个接口无需登录、无需 Wine，宿主 curl 就能读，用来在安装前先说清结果。
query_server_version() {
    command -v curl >/dev/null || return 1
    timeout 20 curl -s -A 'Mozilla/5.0' \
        'https://www.doubao.com/service/settings/v3/?device_platform=pc&brand=doubao&aid=582478&os=windows&update_type=downloader&version_code=2.25.16&os_version=10.0.22000' \
        | grep -oE 'doubao_pc/[0-9.]+/Doubao_win_X64_[0-9.]+-signed\.zip' \
        | head -n 1 | sed -E 's#doubao_pc/([0-9.]+)/.*#\1#'
}

# 安装器自己的日志，完成与否以它为准。
installer_log() {
    local f
    for f in "$PREFIX"/drive_c/users/*/AppData/Local/Doubao/"User Data"/sdk_storage/log/Doubao-installer.log; do
        [[ -e "$f" ]] && { printf '%s\n' "$f"; return 0; }
    done
    printf '%s\n'
}

# 从安装器窗口里定位「立即安装」按钮：抓窗口位图，找按钮主题色 #3370FF 的实心块。
# 不靠写死坐标，因为窗口尺寸会随 DPI 缩放。需要 python3-xlib + Pillow + xdotool。
#
# 窗口枚举、等待映射、抓图重试全部在 Python 侧完成，原因有二：
#   1) 豆包客户端自身也会注册名为「豆包」的窗口，抓到它就抓不到安装界面；
#   2) 安装器窗口刚弹出时可能尚未真正映射（map_state != IsViewable），
#      立刻 XGetImage 会抛 Xlib.error.BadMatch，必须等映射并重试。
# 成功时输出「窗口id x y」，失败时非 0 退出并把原因写到 stderr。
find_install_button() {
    python3 - <<'PY'
import subprocess, sys, time
try:
    from Xlib import X, display
    from PIL import Image
except ImportError as e:
    sys.exit(f"缺少抓图依赖：{e}（python3-xlib / Pillow）")

TARGET = (51, 112, 255)   # main_window_*.xml 里 start_install 的 bkcolor #FF3370ff
DEADLINE = time.time() + 60


def near(c, delta=28):
    return all(abs(a - b) <= delta for a, b in zip(c, TARGET))


def installer_wids():
    """枚举属于安装器进程的窗口（豆包客户端自身的窗口要排除）。"""
    try:
        out = subprocess.run(["xdotool", "search", "--name", "豆包"],
                             capture_output=True, text=True).stdout.split()
    except (OSError, subprocess.SubprocessError):
        return []
    wids = []
    for wid in out:
        try:
            prop = subprocess.run(["xprop", "-id", wid, "_NET_WM_PID"],
                                  capture_output=True, text=True).stdout
            pid = prop.split("=")[-1].strip()
            if not pid.isdigit():
                continue
            with open(f"/proc/{pid}/cmdline", "rb") as f:
                cmd = f.read().replace(b"\0", b" ").decode("utf-8", "replace")
        except (OSError, subprocess.SubprocessError):
            continue
        if "doubao_client" in cmd:
            wids.append(int(wid))
    return wids


def grab(d, wid):
    """抓窗口位图；窗口还没真正映射时抛异常，由调用方重试。"""
    w = d.create_resource_object("window", wid)
    attrs = w.get_attributes()
    if attrs.map_state != X.IsViewable:
        raise RuntimeError(f"窗口 {wid} 尚未映射（map_state={attrs.map_state}）")
    g = w.get_geometry()
    img = w.get_image(0, 0, g.width, g.height, X.ZPixmap, 0xffffffff)
    raw = bytes(img.data)
    bpp = len(raw) // (g.width * g.height)
    if bpp == 4:
        return Image.frombytes("RGBA", (g.width, g.height), raw, "raw", "BGRA", 0, 1).convert("RGB")
    if bpp == 3:
        return Image.frombytes("RGB", (g.width, g.height), raw, "raw", "BGR", 0, 1)
    raise RuntimeError(f"未知像素宽度：{bpp}")


def button_center(im):
    px = im.load()
    # 只取“实心行”（单行蓝色像素足够多），排除同样蓝色的「自定义安装地址」文字
    rows = [y for y in range(im.height)
            if sum(1 for x in range(im.width) if near(px[x, y])) > 50]
    if not rows:
        return None
    y0, y1 = rows[0], rows[-1]
    xs = [x for y in range(y0, y1 + 1) for x in range(im.width) if near(px[x, y])]
    return (min(xs) + max(xs)) // 2, (y0 + y1) // 2


d = display.Display()
last = "没找到安装器窗口"
while time.time() < DEADLINE:
    for wid in installer_wids():
        try:
            im = grab(d, wid)
        except Exception as e:
            last = f"{type(e).__name__}: {e}"
            continue
        pos = button_center(im)
        if pos:
            print(wid, pos[0], pos[1])
            sys.exit(0)
        last = "窗口里没找到「立即安装」按钮（可能已点过或界面改版）"
    time.sleep(1)
sys.exit(last)
PY
}

# 等安装器窗口出现。用 _NET_WM_PID 反查 cmdline，确认是安装器而不是豆包主程序。
wait_installer_window() {
    local deadline=$(( $(date +%s) + 90 )) wid pid
    while (( $(date +%s) < deadline )); do
        for wid in $(xdotool search --name '豆包' 2>/dev/null); do
            pid=$(xprop -id "$wid" _NET_WM_PID 2>/dev/null | sed 's/.*= *//')
            [[ -n "$pid" ]] || continue
            if tr '\0' ' ' < "/proc/$pid/cmdline" 2>/dev/null | grep -q 'doubao_client'; then
                printf '%s\n' "$wid"
                return 0
            fi
        done
        sleep 1
    done
    return 1
}

# 挑安装器：命令行参数 > 仓库 doubao/（本机文件，已 gitignore）> 用户下载目录，
# 同目录下多个则取修改时间最新的一个。路径含空格，全部加引号。
find_installer() {
    local -a found=()
    local p
    if [[ -n "${1:-}" ]]; then
        [[ -f "$1" ]] || { echo "错误：指定的安装器不存在：$1" >&2; exit 1; }
        printf '%s\n' "$1"
        return 0
    fi
    for p in "$SCRIPT_DIR"/doubao_client_*.exe \
             "$HOME"/下载/doubao_client_*.exe \
             "$HOME"/Downloads/doubao_client_*.exe
    do
        [[ -f "$p" ]] && found+=("$p")
    done
    (( ${#found[@]} )) || return 1
    printf '%s\n' "${found[@]}" | xargs -d '\n' ls -t | head -n 1
}

INSTALLER="$(find_installer "${1:-}")" || {
    echo "错误：未找到官方安装器 doubao_client_*_ext.exe。" >&2
    echo "请从豆包官网下载 Windows 在线安装器后重试：./doubao/install-doubao.sh /路径/安装器.exe" >&2
    exit 1
}

before="$(detect_version)"
server_ver="$(query_server_version || true)"
echo "安装器：$INSTALLER"
echo "前  缀：$PREFIX"
echo "当前版本：${before}"
echo "安装器渠道下发：${server_ver:-未查到（接口不可达或格式变了）}"
# 「官方版本」有两个口径，别混用：
#   - 安装器渠道：就是上面这个接口返回的版本（2026-11-08 实测 2.32.6）；
#   - 客户端运行版本：客户端自更新后可以更高（2026-11-08 实测运行中 2.35.8）。
# 跑一次安装器 = 把客户端落回安装器渠道版本，之后由客户端自己的更新器决定最终版本。
if [[ -n "$server_ver" && "$before" != "未安装" ]]; then
    if [[ "$before" == "$server_ver" ]]; then
        echo "说明：与安装器渠道版本一致，本次为同版本完整重装。"
    elif [[ "$(printf '%s\n%s\n' "$before" "$server_ver" | sort -V | tail -n 1)" == "$before" ]]; then
        echo "提示：当前客户端版本（$before）高于安装器渠道（$server_ver）；"
        echo "      本次安装会先落回渠道版本，客户端启动后由它自己的更新器决定最终版本。"
    fi
fi
echo "日志：$LOG"

# 安装器无法覆盖正在使用的文件；只结束本前缀的 Wine 进程，不动其他前缀
# （Deepin 的 ~/.deepinwine/doubao-wine 与 Steam 的 Proton 前缀互不影响）。
#
# 注意：Wine 进程的命令行里**不含** WINEPREFIX（只有 C:\... 路径），
# `pgrep -f "$PREFIX"` 匹配不到；前缀只在 /proc/<pid>/environ 里。
# 所以这里改用 environ 精确匹配，否则运行中的豆包关不掉，
# 安装器会因文件被占用而卡住（实测：客户端没关 → 按钮定位 BadMatch → 等到超时）。
prefix_pids() {
    local pid
    for pid in $(pgrep -f 'Doubao\.exe|explorer\.exe|wineserver|winedevice\.exe' 2>/dev/null); do
        if tr '\0' '\n' < "/proc/$pid/environ" 2>/dev/null |
           grep -qxF "WINEPREFIX=$PREFIX"; then
            printf '%s\n' "$pid"
        fi
    done
}

if [[ -n "$(prefix_pids)" ]]; then
    echo "检测到本前缀有豆包进程在运行，先关闭该前缀（wineserver -k）……"
    "$WINESERVER" -k 2>/dev/null || true
    for _ in $(seq 1 30); do
        [[ -z "$(prefix_pids)" ]] && break
        sleep 1
    done
    if [[ -n "$(prefix_pids)" ]]; then
        echo "错误：仍有进程占用该前缀，请手动结束后重试：" >&2
        printf '  pid %s\n' $(prefix_pids) >&2
        exit 1
    fi
    echo "已关闭。"
fi

# 不传 /S：实测它不会静默，反而多一个无人应答的等待阶段。
for dep in xdotool xprop; do
    command -v "$dep" >/dev/null || { echo "错误：缺少 $dep，无法自动点「立即安装」。" >&2; exit 1; }
done

"$WINE" "$INSTALLER" >"$LOG" 2>&1 &
installer_pid=$!

if ! WID="$(wait_installer_window)"; then
    echo "错误：等不到安装器窗口。安装器输出见 $LOG" >&2
    kill "$installer_pid" 2>/dev/null
    exit 1
fi

# 安装器日志在首次运行时才会创建，新前缀下刚启动那一刻可能还不存在。
for _ in $(seq 1 30); do
    ILOG="$(installer_log)"
    [[ -n "$ILOG" ]] && break
    sleep 1
done
[[ -n "$ILOG" ]] || { echo "错误：找不到安装器日志，无法判定安装结果。" >&2; exit 1; }

# 点按钮前先记住安装日志已有多少行，之后只看本轮新增部分，
# 避免把上一次安装的 _OnInstallSuccess 当成本次结果。
base_lines=$(wc -l < "$ILOG" 2>/dev/null || echo 0)

if ! XYZ="$(find_install_button 2>&1)"; then
    echo "警告：无法自动定位「立即安装」按钮：$XYZ" >&2
    echo "请手动在弹出的安装窗口里点「立即安装」，脚本继续等安装完成……" >&2
else
    read -r WID bx by <<< "$XYZ"
    echo "安装器窗口：$WID；「立即安装」按钮中心：$bx,$by"
    # 先激活窗口再用 XTEST 真实点击：xdotool click --window 走 XSendEvent，
    # 合成事件常被 Wine 侧忽略，点下去没反应。
    xdotool windowactivate --sync "$WID" 2>/dev/null \
        || xdotool windowfocus "$WID" 2>/dev/null || true
    xdotool mousemove --window "$WID" "$bx" "$by"
    sleep 1
    xdotool click 1
fi

# 完成判据：安装器日志本轮新增行里出现成功/失败标记。
# 包体接近 1GB，下载+解包在本机约 60 秒，默认上限给 15 分钟。
deadline=$(( $(date +%s) + TIMEOUT ))
status=""
while (( $(date +%s) < deadline )); do
    newlog=$(tail -n +$((base_lines + 1)) "$ILOG" 2>/dev/null || true)
    if grep -q '_OnInstallSuccess' <<< "$newlog"; then
        status=成功
        break
    fi
    if grep -qE 'DownloadPackageFail|InvalidSignature|_OnInstallFailed' <<< "$newlog"; then
        status=失败
        break
    fi
    if ! kill -0 "$installer_pid" 2>/dev/null && [[ -n "$newlog" ]]; then
        # 安装器已退出且日志没再动，拿最后一次结果收尾
        grep -q '_OnInstallSuccess' <<< "$newlog" && status=成功 || status="未标记（安装器已退出）"
        break
    fi
    sleep 5
done

after="$(detect_version)"
echo "安装结果：${status:-超时（仍在下载，可稍后查 $ILOG）}"
echo "安装前版本：$before"
echo "安装后版本：$after"

# 安装成功后安装器会自动拉起豆包（日志 _LaunchAgent）。那个实例目前仍是黑屏窗口，
# 留着只会变成孤儿进程，不如收尾干净，由启动器重新拉起。
if [[ "$status" == 成功 ]]; then
    "$WINESERVER" -k 2>/dev/null || true
    sleep 2
fi

# 新版本装进同一前缀时，安装包可能再次带入指向打包者 home 的悬空 WinSxS 链接，
# 缺 LocalLow 也会触发 cryptnet 崩溃。这一步与版本无关，纯前缀层修复。
if [[ -x "$SCRIPT_DIR/fix-doubao-wine.sh" ]]; then
    echo "运行前缀层修复……"
    DOUBAO_WINE_PREFIX="$PREFIX" "$SCRIPT_DIR/fix-doubao-wine.sh" || \
        echo "警告：前缀层修复未完全通过，详见上方输出。" >&2
fi

# 安装器每次都可能在 ~/Desktop 重建 Windows 快捷方式 豆包.lnk，DING 会把它
# 交给 spark-dwine-helper / deepin-wine8-stable（旧前缀），双击又撞 0x7F FATAL。
# 装完顺手统一桌面入口，避免「刚装好又启动不了」的回归。
if [[ -x "$SCRIPT_DIR/fix-doubao-desktop.sh" ]]; then
    echo "运行桌面入口修复……"
    "$SCRIPT_DIR/fix-doubao-desktop.sh" >/dev/null || \
        echo "警告：桌面入口修复未完全通过，可执行 doubao/fix-doubao-desktop.sh --status 查看。" >&2
fi

echo
echo "完成。启动新版：$SCRIPT_DIR/run-doubao-wine11.sh"
echo "安装日志：$LOG"
