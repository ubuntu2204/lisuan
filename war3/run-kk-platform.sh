#!/usr/bin/env bash
# 至砺算：KK 官方对战平台（Qt5 + QtWebEngine 32 位客户端）在 Wine 下的运行问题
# 与显卡硬件无关：默认 wined3d/Vulkan 后端初始化失败会让 QtWebEngine 在
# Qt5WebEngineCore.dll+0x82FA45 空指针读（c0000005）并退出（rc=5）。显式指定
# Mesa llvmpipe Vulkan ICD 与 wined3d 的 GL 渲染器后，窗口与主进程稳定。
#
# 只在这一个应用的进程树上注入图形环境，绝不污染全局 shell / Steam 客户端；
# 不做崩溃后自动重启、重试或守护。图标与桌面入口见 fix-kk-desktop.sh。
#
# 界面现象澄清（2026-11-14 用户纠正）：启动时盖住主界面的半透明深灰层是
# 活动弹窗自带的模态遮罩，点弹窗的 X 关闭即消失，是正常 UI，不是渲染 bug。
# 曾据此误判为「32 位 ARGB 窗口重影」并注入 XLIB_SKIP_ARGB_VISUALS=1，
# 实测虽让窗口不透明，却让 QtWebEngine 主内容（游戏列表 / 商城导航）整片
# 空白，已撤销；不要再引入 ARGB / 合成层干预。
#
# 真正的缺陷是 UI 错位：Qt 把界面画在 override-redirect 内容层上，Wine 把它
# 固定放在 (0,0)，窗口管理器管理的主框架却在 (293,32)，错开 293/32 px，
# 于是界面画到窗口外、窗口内空白、点击位置与视觉不符。启动后把内容层对齐到
# 主框架即可。应用初始化期间会反复把内容层重置回 (0,0)，所以等结构稳定后
# 对齐并复核；全程有限次数、单次启动，不监控、不守护、不自动重启客户端。
#
# 用法：./war3/run-kk-platform.sh [传给 Platform.exe 的参数]
# 环境变量：KK_PROTON / KK_WINE11_PREFIX / KK_LAUNCH_LOG / KK_WINEDEBUG /
#   KK_ALIGN_WINDOWS（=0 关闭一次性窗口对齐）/
#   KK_WEBENGINE_SOFTWARE（=1 仅供 A/B 诊断，对遮罩与错位均无收益）
# License: Apache-2.0
set -euo pipefail

PROTON="${KK_PROTON:-$HOME/.local/share/Steam/steamapps/common/Proton 11.0}"
PROTON="${PROTON/#\~/$HOME}"
PREFIX="${KK_WINE11_PREFIX:-$HOME/.deepinwine/kkduizhan-wine11}"
PREFIX="${PREFIX/#\~/$HOME}"
INSTALL_DIR="$PREFIX/drive_c/Program Files (x86)/kkduizhan"
APP="$INSTALL_DIR/Platform.exe"
WINE="$PROTON/files/bin/wine"
LOG="${KK_LAUNCH_LOG:-$HOME/.local/share/kkduizhan-wine11/launch.log}"

if [[ ! -x "$WINE" ]]; then
    echo "错误：找不到 Proton 11 的 wine：$WINE" >&2
    exit 1
fi
if [[ ! -f "$APP" ]]; then
    echo "错误：未安装 KK 对战平台：$APP（先跑 install-kk-platform.sh）" >&2
    exit 1
fi
mkdir -p "$(dirname "$LOG")" 2>/dev/null || true
if ! : >> "$LOG" 2>/dev/null; then
    echo "错误：无法写入启动日志：$LOG" >&2
    exit 1
fi

export WINEPREFIX="$PREFIX"
export WINEDLLPATH="$PROTON/files/lib/vkd3d:$PROTON/files/lib/wine"
export LD_LIBRARY_PATH="$PROTON/files/lib/x86_64-linux-gnu:$PROTON/files/lib/i386-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export WINEDLLOVERRIDES='mscoree,mshtml='
# 与项目其它 Wine 应用一致的安全软件渲染基线；两个关键项二选一即可越过崩溃，
# 同时设置是为了在 Vulkan/GL 任一路径异常时都保持可启动（2026-11-14 消融）。
export GBM_BACKEND=dri
export LIBGL_ALWAYS_SOFTWARE=1
export __GLX_VENDOR_LIBRARY_NAME=mesa
export VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json
export WINE_D3D_CONFIG='renderer=gl'

# --- 布局：只清宿主泄漏，不做 ARGB / 虚拟桌面干预 ---
# 宿主会话的 QT_ACCESSIBILITY / QT_IM_MODULE 会泄漏进 Wine 进程，先清掉。
unset QT_ACCESSIBILITY QT_IM_MODULE
# 可选诊断开关（默认关闭）：保留供将来排查 Web 内容刷新问题时 A/B，
# 已证明对弹窗遮罩与 UI 错位都没有收益。
if [[ "${KK_WEBENGINE_SOFTWARE:-0}" == 1 ]]; then
    export QT_SCALE_FACTOR=1
    export QT_AUTO_SCREEN_SCALE_FACTOR=0
    export QT_FONT_DPI=96
    export QTWEBENGINE_CHROMIUM_FLAGS='--disable-gpu --disable-gpu-compositing --disable-accelerated-2d-canvas --force-device-scale-factor=1 --disable-features=CalculateNativeWinOcclusion --disable-backgrounding-occluded-windows'
fi
export WINEDEBUG="${KK_WINEDEBUG:--all}"

printf '=== %s | %s %s ===\n' "$(date '+%F %T')" "$APP" "$*" >> "$LOG"
exec > >(tee -a "$LOG") 2>&1
cd "$INSTALL_DIR"
# 一次性窗口对齐：修 UI 错位。两个同名窗口中 override-redirect 的是内容层，
# 其余是 WM 主框架；按 13xx×9xx 主窗口尺寸区分，排除活动弹窗。
# 应用会把内容层重置回 (0,0)，故最多移动 3 次、每次复核，之后不再干预。
if [[ "${KK_ALIGN_WINDOWS:-1}" == 1 ]]; then
    "$WINE" "$APP" "$@" &
    app_pid=$!
    # 等窗口结构稳定（应用初始化期间内容层位置会反复变）。
    sleep 25
    moves=0
    for i in $(seq 1 12); do
        sleep 2
        main=""; layer=""
        for id in $(xwininfo -root -tree 2>/dev/null | awk '/"KK官方对战平台"/ && /1[23][0-9][0-9]x9[0-9][0-9]/ {print $1}'); do
            if xwininfo -id "$id" -all 2>/dev/null | grep -q 'Override Redirect State: yes'; then
                layer=$id
            else
                main=$id
            fi
        done
        [[ -n "$main" && -n "$layer" ]] || continue
        x=$(xwininfo -id "$main" | awk '/Absolute upper-left X/{print $NF}')
        y=$(xwininfo -id "$main" | awk '/Absolute upper-left Y/{print $NF}')
        lx=$(xwininfo -id "$layer" | awk '/Absolute upper-left X/{print $NF}')
        ly=$(xwininfo -id "$layer" | awk '/Absolute upper-left Y/{print $NF}')
        if [[ "$lx" == "$((x + 1))" && "$ly" == "$((y + 1))" ]]; then
            break
        fi
        xdotool windowmove "$layer" "$((x + 1))" "$((y + 1))"
        command -v xrefresh >/dev/null 2>&1 && xrefresh -root >/dev/null 2>&1 || true
        echo "窗口对齐：内容层 $layer -> 主框架 $main ($x,$y)"
        moves=$((moves + 1))
        [[ $moves -ge 3 ]] && break
    done
    wait "$app_pid"
    exit $?
fi
exec "$WINE" "$APP" "$@"
