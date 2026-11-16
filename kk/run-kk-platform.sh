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
# 真正的缺陷是 Wine/Proton 11 对 KK「分层子窗口」的坐标语义误处理：内容层被
# SetParent 挂到主框架下后用 UpdateLayeredWindow(pptDst=(0,0)) 定位，Windows
# 按父客户区坐标解释、Wine 按屏幕坐标解释，于是内容层停在屏幕 (0,0)：视觉
# 错位、win32k 子窗口命中裁剪丢弃侧栏输入；另有 OR+TOPMOST 内容层盖住弹窗
# 按键、web 客户窗口被 detach 到 1x1 dummy 下导致游戏列表不可见两个并发缺陷。
# 修复分两层，均仅注入本应用进程树：
#   kk/kk-user32-wrapper.c：native user32 包装层，把子窗口 ULW 的 pptDst
#     换算成屏幕坐标（根因修复），其余导出 forwarder 转回内置 user32_orig；
#   kk/kk-geometry-shim.c：LD_PRELOAD X11 垫片，剥夺内容层抬升 + 弹窗抬到
#     内容层之上 + web 客户窗口重挂主框架。
# 不监控、不守护、不自动重启客户端。取证与消融见 kk/README.md §5。
#
# 用法：./kk/run-kk-platform.sh [传给 Platform.exe 的参数]
# 环境变量：KK_PROTON / KK_WINE11_PREFIX / KK_LAUNCH_LOG / KK_WINEDEBUG /
#   KK_GEOMETRY_SHIM（=0 关闭几何垫片，回到未修复的原始行为）/
#   KK_SHIM_DEBUG（=1 垫片日志进启动日志）/
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

# --- 几何垫片：修 UI 错位 / 按键不可点 / 游戏列表不可见（根因见文件头）---
# 按需编译到状态目录（不入库），仅经 LD_PRELOAD 注入本 wine 进程树。
# Platform.exe 走 i386-unix wine-preloader（32 位 unix 进程），X11 调用在
# 32 位侧，故主垫片编 32 位；同时编 64 位以兼容新 WoW64（X 调用在 64 位侧）。
# LD_PRELOAD 列两个路径，加载器各取兼容者，不兼容者仅一行 ignored 警告。
# 垫片只认 KK 主框架 / 内容层的精确创建尺寸，厂商改尺寸时自动退化为透传。
if [[ "${KK_GEOMETRY_SHIM:-1}" == 1 ]]; then
    REPO_DIR="$(cd "$(dirname "$0")" && pwd)"
    SHIM_SRC="$REPO_DIR/kk-geometry-shim.c"
    SHIM_DIR="$(dirname "$LOG")"
    SHIM32="$SHIM_DIR/kk-geometry-shim32.so"
    SHIM64="$SHIM_DIR/kk-geometry-shim.so"
    if [[ ! -f "$SHIM32" || ! -f "$SHIM64" || "$SHIM_SRC" -nt "$SHIM32" || "$SHIM_SRC" -nt "$SHIM64" ]]; then
        command -v gcc >/dev/null 2>&1 || { echo "错误：编译几何垫片需要 gcc：$SHIM_SRC" >&2; exit 1; }
        gcc -shared -fPIC -O2 -o "$SHIM64" "$SHIM_SRC" -ldl || {
            echo "错误：几何垫片 64 位编译失败：$SHIM_SRC" >&2; exit 1; }
        gcc -m32 -shared -fPIC -O2 -nostartfiles -nostdlib -fno-stack-protector \
            -fno-asynchronous-unwind-tables \
            -o "$SHIM32" "$SHIM_SRC" /usr/lib/i386-linux-gnu/libc.so.6 || {
            echo "错误：几何垫片 32 位编译失败（需 i386 libc.so.6）：$SHIM_SRC" >&2; exit 1; }
    fi
    export LD_PRELOAD="$SHIM32:$SHIM64${LD_PRELOAD:+:$LD_PRELOAD}"

    # user32 包装层：根因修复。部署到前缀 syswow64（32 位 native 搜索路径），
    # user32_orig.dll 是内置 user32 的重命名副本（unixlib 名字嵌在 PE 里，
    # 改名加载已实测）。64 位进程经 n,b 回退内置，不受影响。
    SYSWOW="$PREFIX/drive_c/windows/syswow64"
    BUILTIN_U32="$PROTON/files/lib/wine/i386-windows/user32.dll"
    WRAP_SRC="$REPO_DIR/kk-user32-wrapper.c"
    WRAP_DEF="$SHIM_DIR/kk-user32.def"
    WRAP_DLL="$SHIM_DIR/kk-user32.dll"
    [[ -d "$SYSWOW" ]] || { echo "错误：前缀缺 syswow64（非 WoW64 前缀？）：$SYSWOW" >&2; exit 1; }
    [[ -f "$BUILTIN_U32" ]] || { echo "错误：找不到内置 user32：$BUILTIN_U32" >&2; exit 1; }
    if [[ ! -f "$SYSWOW/user32_orig.dll" || "$BUILTIN_U32" -nt "$SYSWOW/user32_orig.dll" ]]; then
        cp "$BUILTIN_U32" "$SYSWOW/user32_orig.dll" || exit 1
        rm -f "$WRAP_DLL"  # 内置换了就重编 forwarder 表
    fi
    # def 每次启动重新生成（纯标准库解析，毫秒级），保证 forwarder 表与
    # 当前内置 user32 一致；def 比 dll 新即重编（数秒）。
    command -v i686-w64-mingw32-gcc >/dev/null 2>&1 || {
        echo "错误：编译 user32 包装层需要 i686-w64-mingw32-gcc" >&2; exit 1; }
    python3 "$REPO_DIR/gen-kk-user32-def.py" "$BUILTIN_U32" "$WRAP_DEF" || exit 1
    if [[ ! -f "$WRAP_DLL" || "$WRAP_SRC" -nt "$WRAP_DLL" || "$WRAP_DEF" -nt "$WRAP_DLL" ]]; then
        i686-w64-mingw32-dlltool --def "$WRAP_DEF" --dllname user32.dll \
            --output-exp "$SHIM_DIR/kk-user32-exp.o" || {
            echo "错误：user32 包装层 def 处理失败：$WRAP_DEF" >&2; exit 1; }
        i686-w64-mingw32-gcc -shared -O2 -o "$WRAP_DLL" "$WRAP_SRC" \
            "$SHIM_DIR/kk-user32-exp.o" -luser32 -lkernel32 || {
            echo "错误：user32 包装层编译失败：$WRAP_SRC" >&2; exit 1; }
    fi
    cp "$WRAP_DLL" "$SYSWOW/user32.dll" || exit 1
    export WINEDLLOVERRIDES="$WINEDLLOVERRIDES;user32=n,b"
fi

printf '=== %s | %s %s ===\n' "$(date '+%F %T')" "$APP" "$*" >> "$LOG"
exec > >(tee -a "$LOG") 2>&1
cd "$INSTALL_DIR"
exec "$WINE" "$APP" "$@"
