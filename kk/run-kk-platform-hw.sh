#!/usr/bin/env bash
# 至砺算：KK 官方对战平台【硬件渲染】启动器（7G100）。
#
# 与 kk/run-kk-platform.sh（llvmpipe 软件方案，大厅 web 黑块）不同，本脚本
# 让 32 位 KK 的 QtWebEngine/CEF 经 Proton new WoW64 + gf-compat 用 7G100
# 硬件 Vulkan 渲染，绕开 lvp 下 CEF GPU 进程反复崩溃导致的 web 黑块。
#
# 方案复刻 minion-masters/fix-minion-masters-7g100-hw.sh（已实测），四要素：
#   1) 独立 new WoW64 前缀（proton 自己初始化，不能复制传统 WoW64 前缀）；
#   2) gf-compat：os-release 白名单绕过 + 低 4GB 显存映射重定位 + 垫片 ICD；
#   3) DXVK v2.3（gf 缺 VK_KHR_maintenance5，Proton 自带 v2.7.1 会被跳过）；
#   4) libSC.so 软链（gf vkCreateDevice 依赖）+ Steam 容器（提供 VK_KHR_surface）。
# 专用 Proton 副本 "Proton - KK HW" 由 kk/setup-kk-hw-proton.sh 生成，
# 不修改全局 Proton、不触发 Steam 校验。
#
# 独立前缀，单次启动 + 显式收尾，无守护/重试/自动重启（遵循崩溃处理约定）。
# 用法：bash kk/run-kk-platform-hw.sh [--reset] [--keep] [--status]
#   --reset   重建独立前缀并迁移 KK 程序与登录态（首次运行自动执行）
#   --keep    保留进程不收尾（供手动登录/抓图验证）
#   --status  只读检查运行环境
set -uo pipefail

REPO="$(cd "$(dirname "$0")/.." && pwd)"
MM="$REPO/minion-masters"
P11="${KK_HW_PROTON:-$HOME/.local/share/Steam/steamapps/common/Proton - KK HW}"
SRC_PFX="$HOME/.deepinwine/kkduizhan-wine11"
CD="$HOME/.local/share/Steam/steamapps/compatdata/481234"
PFX="$CD/pfx"
LOG="$HOME/.local/share/kkduizhan-wine11/poc-hw.log"
SHOT="/tmp/kk-poc-hw.png"
CAP="$REPO/.codebuddy/skills/lisuan-7g100-compat/scripts/capture-wayland-screen.py"

die() { echo "[kk-hw] !! $*" >&2; exit 1; }
say() { echo "[kk-hw] $*"; }

status() {
    echo "== 专用 Proton =="; echo "  $P11"; [ -x "$P11/proton" ] && echo "  [OK] proton 存在" || echo "  [--] 缺失（先跑 setup-kk-hw-proton.sh）"
    echo "  DXVK: $(tail -1 "$P11/files/lib/wine/dxvk/version" 2>/dev/null || echo 未知)"
    echo "  libSC -> $(readlink "$P11/files/lib/x86_64-linux-gnu/libSC.so" 2>/dev/null || echo 未链接)"
    echo "== gf-compat =="
    for f in lib/osrelease_redirect.so lib/libvulkan_gf.so lib/libSC.so gf_icd.json; do
        [ -f "$MM/gf-compat/$f" ] && echo "  [OK] $f" || echo "  [--] $f 缺失"
    done
    echo "== 独立前缀 =="; echo "  $PFX"; [ -f "$CD/version" ] && echo "  [OK] version=$(cat "$CD/version")" || echo "  [--] 未初始化"
    echo "  KK 程序: $([ -f "$PFX/drive_c/Program Files (x86)/kkduizhan/Platform.exe" ] && echo OK || echo 缺失)"
}

case "${1:-}" in
    --status) status; exit 0;;
    -h|--help) sed -n '2,22p' "$0"; exit 0;;
esac

[ -x "$P11/proton" ] || die "无专用 Proton：$P11（先跑 setup-kk-hw-proton.sh）"
[ -f "$SRC_PFX/drive_c/Program Files (x86)/kkduizhan/Platform.exe" ] || die "源 KK 未安装"
[ -f "$MM/gf-compat/lib/osrelease_redirect.so" ] || die "gf-compat hook 未部署"

# 1) 独立前缀：必须让 proton 自己全新初始化成 new WoW64（不能直接复制传统
#    WoW64 前缀——32 位 CEF 无法经 thunk 调到 64 位 gf）。然后植入 KK 程序与
#    登录态。仅首次/--reset。
if [ "${1:-}" = "--reset" ] || [ ! -f "$PFX/drive_c/Program Files (x86)/kkduizhan/Platform.exe" ]; then
    say "用 proton 全新初始化 new WoW64 前缀：$PFX"
    "$P11/files/bin/wineserver" -k -p "$PFX" 2>/dev/null || true
    sleep 1
    rm -rf "$CD"; mkdir -p "$CD"
    STEAM_COMPAT_CLIENT_INSTALL_PATH="$HOME/.local/share/Steam" \
    STEAM_COMPAT_DATA_PATH="$CD" STEAM_COMPAT_APP_ID=481234 \
    PROTON_USE_WOW64=1 WINEDEBUG=-all \
        "$P11/proton" run cmd /c exit >/dev/null 2>&1 || true
    [ -f "$CD/version" ] || die "proton 未建好前缀"
    # 植入 KK 程序文件（架构无关）
    mkdir -p "$PFX/drive_c/Program Files (x86)"
    cp -a "$SRC_PFX/drive_c/Program Files (x86)/kkduizhan" \
          "$PFX/drive_c/Program Files (x86)/" || die "复制 KK 程序失败"
    # 导入登录态注册表（同 wine 11，键结构兼容）
    cp "$SRC_PFX/user.reg" "$SRC_PFX/system.reg" "$SRC_PFX/userdef.reg" "$PFX/" || true
    say "new WoW64 前缀就绪（version=$(cat "$CD/version")）"
fi

# 2) 环境：复刻 MM 硬件方案 + KK 几何修复层
export STEAM_COMPAT_CLIENT_INSTALL_PATH="$HOME/.local/share/Steam"
export STEAM_COMPAT_DATA_PATH="$CD"
export STEAM_COMPAT_APP_ID=481234
export WINEPREFIX="$PFX"
export PROTON_USE_WOW64=1
export WINEDLLPATH="$MM/dxvk23/i386-windows:$MM/dxvk23/x86_64-windows:$P11/files/lib/vkd3d:$P11/files/lib/wine"
export LD_LIBRARY_PATH="$P11/files/lib/x86_64-linux-gnu:$P11/files/lib/i386-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
# DXVK v2.3 native，让 D3D11(CEF/ANGLE) -> DXVK -> Vulkan(gf)
export WINEDLLOVERRIDES='mscoree,mshtml=;d3d11,dxgi,d3d10core,d3d9=n,b'
export LD_PRELOAD="$MM/gf-compat/lib/osrelease_redirect.so"
export VK_ICD_FILENAMES="$MM/gf-compat/gf_icd.json"
export VK_LOADER_LAYERS_DISABLE='VK_LAYER_VALVE_steam_overlay_64,VK_LAYER_VALVE_steam_overlay_32'
export GBM_BACKEND=dri LIBGL_ALWAYS_SOFTWARE=1 __GLX_VENDOR_LIBRARY_NAME=mesa
export WINEDEBUG=-all
unset QT_ACCESSIBILITY QT_IM_MODULE

# --- 几何修复层：UI 错位 / 按键错位（与软件方案同源，见 README §5）---
# 上半段 offscreen 补丁解决了"web 内容不呈现"，但 Wine win32u 的 SetParent
# 坐标语义缺陷仍让内容层/web 子窗位置错开 → 按键漂移。移植软件方案的
# user32 wrapper（native 坐标换算）+ geometry shim（X11 堆叠）。
# 仅注入本应用进程树；KK_GEOMETRY_SHIM=0 可关，回到未修复行为。
SHIM_DIR="$(dirname "$LOG")"
# 几何层修 UI/按键错位；与 offscreen 补丁一起才是完整方案。
# 早期"wrapper 导致启动崩"的真因是 cp 跟随软链污染了 Proton 内置 user32.dll
# （已改为先 rm 软链再 cp），与 new WoW64 本身无关。
if [ "${KK_GEOMETRY_SHIM:-1}" = 1 ]; then
    REPO_DIR="$(cd "$(dirname "$0")" && pwd)"
    SHIM_SRC="$REPO_DIR/kk-geometry-shim.c"
    SHIM32="$SHIM_DIR/kk-geometry-shim32.so"
    SHIM64="$SHIM_DIR/kk-geometry-shim.so"
    if [ ! -f "$SHIM32" ] || [ ! -f "$SHIM64" ] || \
       [ "$SHIM_SRC" -nt "$SHIM32" ] || [ "$SHIM_SRC" -nt "$SHIM64" ]; then
        command -v gcc >/dev/null 2>&1 || die "编译几何垫片需要 gcc"
        gcc -shared -fPIC -O2 -o "$SHIM64" "$SHIM_SRC" -ldl || die "垫片 64 位编译失败"
        gcc -m32 -shared -fPIC -O2 -nostartfiles -nostdlib -fno-stack-protector \
            -fno-asynchronous-unwind-tables -o "$SHIM32" "$SHIM_SRC" \
            /usr/lib/i386-linux-gnu/libc.so.6 || die "垫片 32 位编译失败"
    fi
    export LD_PRELOAD="$SHIM32:$SHIM64${LD_PRELOAD:+:$LD_PRELOAD}"

    # user32 包装层：native user32 换算子窗口 ULW 的父客户区坐标→屏幕坐标
    SYSWOW="$PFX/drive_c/windows/syswow64"
    BUILTIN_U32="$P11/files/lib/wine/i386-windows/user32.dll"
    WRAP_SRC="$REPO_DIR/kk-user32-wrapper.c"
    WRAP_DEF="$SHIM_DIR/kk-user32.def"
    WRAP_DLL="$SHIM_DIR/kk-user32.dll"
    [ -d "$SYSWOW" ] || die "前缀缺 syswow64：$SYSWOW"
    [ -f "$BUILTIN_U32" ] || die "找不到内置 user32：$BUILTIN_U32"
    if [ ! -f "$SYSWOW/user32_orig.dll" ] || [ "$BUILTIN_U32" -nt "$SYSWOW/user32_orig.dll" ]; then
        rm -f "$SYSWOW/user32_orig.dll" 2>/dev/null || true   # builtin 常是 444 只读
        cp "$BUILTIN_U32" "$SYSWOW/user32_orig.dll" || exit 1
        rm -f "$WRAP_DLL"
    fi
    command -v i686-w64-mingw32-gcc >/dev/null 2>&1 || die "编译 user32 wrapper 需要 i686-w64-mingw32-gcc"
    python3 "$REPO_DIR/gen-kk-user32-def.py" "$BUILTIN_U32" "$WRAP_DEF" || exit 1
    if [ ! -f "$WRAP_DLL" ] || [ "$WRAP_SRC" -nt "$WRAP_DLL" ] || [ "$WRAP_DEF" -nt "$WRAP_DLL" ]; then
        i686-w64-mingw32-dlltool --def "$WRAP_DEF" --dllname user32.dll \
            --output-exp "$SHIM_DIR/kk-user32-exp.o" || exit 1
        i686-w64-mingw32-gcc -shared -O2 -o "$WRAP_DLL" "$WRAP_SRC" \
            "$SHIM_DIR/kk-user32-exp.o" -luser32 -lkernel32 || exit 1
    fi
    # 关键：前缀里 user32.dll 是指向 Proton 内置 DLL 的软链。直接 cp 会
    # [跟随软链] 覆盖 Proton 自带文件（next launch user32 forward 找不到
    # user32_orig → LoadStringW 未实现 → KK 启动即崩）。必须先 rm 软链再 cp。
    rm -f "$SYSWOW/user32.dll" 2>/dev/null || true
    cp "$WRAP_DLL" "$SYSWOW/user32.dll" || exit 1
    export WINEDLLOVERRIDES="$WINEDLLOVERRIDES;user32=n,b"

    # win32u 包装层（实验，默认关闭）：PE forwarder 包装实测不拦截 Wine 的
    # NtUserSetParent（user32 经内部 syscall dispatcher 调用，不走 PE 导出），
    # 且覆盖 win32u.dll 会让主框架掉到 (0,0)、内容层消失。2026-11-20 已验证，
    # 因此默认 0；仅在明确 A/B 时用 KK_WIN32U_WRAPPER=1 开启。
    BUILTIN_W32U="$P11/files/lib/wine/i386-windows/win32u.dll"
    W32U_SRC="$REPO_DIR/kk-win32u-wrapper.c"
    W32U_DEF="$SHIM_DIR/kk-win32u.def"
    W32U_DLL="$SHIM_DIR/kk-win32u.dll"
    [ -f "$BUILTIN_W32U" ] || die "找不到内置 win32u：$BUILTIN_W32U"
    if [ "${KK_WIN32U_WRAPPER:-0}" = 1 ]; then
        if [ ! -f "$SYSWOW/win32u_orig.dll" ] || [ "$BUILTIN_W32U" -nt "$SYSWOW/win32u_orig.dll" ]; then
            rm -f "$SYSWOW/win32u_orig.dll" 2>/dev/null || true
            cp "$BUILTIN_W32U" "$SYSWOW/win32u_orig.dll" || exit 1
            rm -f "$W32U_DLL"
        fi
        python3 "$REPO_DIR/gen-kk-user32-def.py" "$BUILTIN_W32U" "$W32U_DEF" \
            win32u win32u_orig NtUserSetParent || exit 1
        if [ ! -f "$W32U_DLL" ] || [ "$W32U_SRC" -nt "$W32U_DLL" ] || [ "$W32U_DEF" -nt "$W32U_DLL" ]; then
            i686-w64-mingw32-dlltool --def "$W32U_DEF" --dllname win32u.dll \
                --output-exp "$SHIM_DIR/kk-win32u-exp.o" || exit 1
            i686-w64-mingw32-gcc -shared -O2 -o "$W32U_DLL" "$W32U_SRC" \
                "$SHIM_DIR/kk-win32u-exp.o" -luser32 -lkernel32 || exit 1
        fi
        rm -f "$SYSWOW/win32u.dll" 2>/dev/null || true
        cp "$W32U_DLL" "$SYSWOW/win32u.dll" || exit 1
        export WINEDLLOVERRIDES="$WINEDLLOVERRIDES;win32u=n,b"
        say "win32u 包装层已注入（修 web 子窗父客户区原点）"
    else
        rm -f "$SYSWOW/win32u.dll" 2>/dev/null || true
        ln -sf "$BUILTIN_W32U" "$SYSWOW/win32u.dll" 2>/dev/null || true
    fi
    say "几何修复层已注入（wrapper + shim）"
fi

# libSC.so 软链进 Proton 库目录（gf vkCreateDevice 依赖）
LNSC="$P11/files/lib/x86_64-linux-gnu/libSC.so"
[ -L "$LNSC" ] || { [ -e "$LNSC" ] && mv "$LNSC" "$LNSC.bak-poc"; ln -sf "$MM/gf-compat/lib/libSC.so" "$LNSC"; }

# 把 DXVK v2.3 native 复制进前缀（覆盖 v2.7.1）；gf 缺 maintenance5，
# 只有 v2.3 能建 device（v2.7.1 报 Skipping: maintenance5 unsupported）。
cp -f "$MM/dxvk23/x86_64-windows/"{d3d9,d3d10core,d3d11,dxgi}.dll \
      "$PFX/drive_c/windows/system32/" 2>/dev/null || true
cp -f "$MM/dxvk23/i386-windows/"{d3d9,d3d10core,d3d11,dxgi}.dll \
      "$PFX/drive_c/windows/syswow64/" 2>/dev/null || true

: > "$LOG"
say "启动 proton waitforexitandrun（容器 + new WoW64 + gf）；日志 $LOG"
cd "$PFX/drive_c/Program Files (x86)/kkduizhan" || die "无安装目录"
"$P11/proton" waitforexitandrun Platform.exe >"$LOG" 2>&1 &
PID=$!

# 单次等待登录/大厅就绪（最多 70s），不依赖看门狗。
# 注意：proton 的 $! 会很快失效（它 fork/exec），判活必须用真正的 Platform.exe；
# KK 应用日志写在前缀内 platform.log，不在 proton 的 stdout（$LOG）里。
APPLOG_DIR="$PFX/drive_c/Program Files (x86)/kkduizhan/log"
READY=0
for i in $(seq 1 35); do
    sleep 2
    if grep -rlqE 'WndLobby|GameLobby|Boardcast' "$APPLOG_DIR"/platform.log 2>/dev/null; then READY=1; fi
    # wine 进程命令行是 Windows 路径，不含 $PFX；只按 exe 名判活
    if ! pgrep -f 'Platform\.exe' >/dev/null 2>&1; then
        say "KK 进程不存在（第 ${i} 次检查）"; break
    fi
    [ "$READY" = 1 ] && break
done
say "就绪标志=$READY 循环结束（$(date +%T)）"

# 窗口与日志概览
DISPLAY=:0 xwininfo -root -tree 2>/dev/null | grep -iE 'KK官方|对战|登录' | head -10
say "--- 日志关键行 ---"
grep -iE 'vulkan|dxvk|renderer|LISUAN|7G100|ERR|error|fail|offscreen|surface|WndLobby|login server' "$LOG" 2>/dev/null | tail -30

# 合成画面抓帧（唯一合法通道）
python3 "$CAP" -o "$SHOT" >/dev/null 2>&1 || say "抓图失败"
if [ -f "$SHOT" ]; then
    python3 - "$SHOT" <<'PY'
from PIL import Image
from collections import Counter
import sys
im=Image.open(sys.argv[1]).convert('RGB')
print("[kk-hw] shot", im.size, "colors", len(im.getcolors(2000000) or []))
# 大厅内容区大致 x=460..1600 y=120..940（按 1920x1080）
pix=[im.getpixel((x,y)) for y in range(120,940,16) for x in range(460,1600,16)]
print("[kk-hw] content top:", Counter(pix).most_common(6))
PY
fi

# 显式收尾：不监控、不守护、不自动重启
if [ "${1:-}" != "--keep" ]; then
    say "收尾：结束进程"
    kill "$PID" 2>/dev/null || true
    sleep 2
    pkill -f "$PFX.*Platform.exe" 2>/dev/null || true
    pkill -f 'QtWebEngineProcess' 2>/dev/null || true
    "$P11/files/bin/wineserver" -k -p "$PFX" 2>/dev/null || true
fi
say "完成；截图 $SHOT，日志 $LOG"
