#!/usr/bin/env bash
# 至砺算：KK 官方对战平台（Qt5 + QtWebEngine 32 位客户端）在 Wine 下的运行问题
# 与显卡硬件无关：默认 wined3d/Vulkan 后端初始化失败会让 QtWebEngine 在
# Qt5WebEngineCore.dll+0x82FA45 空指针读（c0000005）并退出（rc=5）。显式指定
# Mesa llvmpipe Vulkan ICD 与 wined3d 的 GL 渲染器后，窗口与主进程稳定。
#
# 只在这一个应用的进程树上注入图形环境，绝不污染全局 shell / Steam 客户端；
# 不做崩溃后自动重启、重试或守护。图标与桌面入口见 fix-kk-desktop.sh。
#
# 界面半透明重影修复（2026-11-14，已用 Wayland 合成画面 A/B 验收）：
# 根因是 Wine 为 KK 主窗口 client 区与活动弹窗创建 32 位 ARGB visual
# （Depth 32），Qt 透明背景与 XWayland 合成叠加后整窗半透明、后面的窗口
# 文字透出，看起来就是重影/错位。XLIB_SKIP_ARGB_VISUALS=1 强制 24 位
# 不透明窗口：实测两个窗口 Depth 32→24，抓图颜色数 32829→166278，
# 界面清晰无重影。另保留 WebEngine 软件绘制与 DPR=1 作为布局保险。
# KK_LAYOUT_LEGACY=1 可一键回退到修复前配置。
#
# 用法：./war3/run-kk-platform.sh [传给 Platform.exe 的参数]
# 环境变量：KK_PROTON / KK_WINE11_PREFIX / KK_LAUNCH_LOG / KK_WINEDEBUG /
#   KK_LAYOUT_LEGACY（=1 关闭下列布局修复）
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

# --- 界面错位 / Web 内容输入修复（下次启动生效，不动当前已登录会话）---
# 宿主会话的 QT_ACCESSIBILITY / QT_IM_MODULE 会泄漏进 Wine 进程，先清掉。
unset QT_ACCESSIBILITY QT_IM_MODULE
if [[ "${KK_LAYOUT_LEGACY:-0}" != 1 ]]; then
    # 根因修复：禁用 32 位 ARGB visual，窗口改回 24 位不透明（2026-11-14
    # Wayland 合成画面 A/B 验证：Depth 32→24，颜色数 32829→166278，
    # 半透明重影消失；消融显示单独这一项即可，无需 WebEngine 开关）。
    export XLIB_SKIP_ARGB_VISUALS=1
fi
# 可选诊断开关（默认关闭）：消融证明 WebEngine 软件绘制 / DPR=1 对半透明重影
# 没有额外收益，仅保留供将来排查 Web 内容刷新问题时 A/B。
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
exec "$WINE" "$APP" "$@"
