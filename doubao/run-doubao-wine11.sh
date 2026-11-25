#!/usr/bin/env bash
# 豆包官方新版 2.32.6 在 Proton 11 (Wine 11) 下的启动器。
# 环境变量与 Proton 自带 proton 脚本保持一致；前缀独立于 Deepin 前缀。
# 启动输出同时写 ~/.local/share/doubao-wine11/launch.log（可用
# DOUBAO_WINE11_LOG 覆盖），便于排查桌面双击这类没有终端的启动。
#
# 稳健默认：经 Steam Runtime 容器隔离 Mesa/GL 栈。旧图形会话下裸跑宿主 wine 时，
# Chromium 147 主窗口一直 IsUnMapped（style 不含 WS_VISIBLE、从未收到 SW_SHOW），
# 强制 XMapWindow 也只有纯黑；容器内正常映射并渲染真实界面。07:50 重启后的
# 新会话里裸 wine 也能跑，故容器不是唯一必要条件，但能绕开宿主图形栈污染。
# 与《随从大师》「图形环境只注入目标进程树、绝不污染全局」的经验一致。
set -euo pipefail

SELF="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/$(basename "${BASH_SOURCE[0]}")"

# 豆包 CSS 首选 PingFang SC（本机缺失）并常用 font-weight:300，默认回退到
# Noto Sans CJK SC Light，笔画过细。只给本进程树注入专用 fontconfig 替换，
# 把 Light 族名/字重抬到微软雅黑 Regular；DOUBAO_FONT_DISABLE=1 可临时关闭，
# DOUBAO_FONT_CONF 可指向自定义配置。
BASE_DIR="$(dirname "$SELF")"
FONT_CONF="${DOUBAO_FONT_CONF:-$BASE_DIR/fonts.conf}"
if [ "${DOUBAO_FONT_DISABLE:-0}" != "1" ] && [ -f "$FONT_CONF" ]; then
    export FONTCONFIG_FILE="$FONT_CONF"
fi

# gf 硬件加速垫片部署（DOUBAO_GPU=hw 时才需要）。必须在宿主侧、进容器前完成：
# 容器里没有 gcc，/usr/lib 也是运行时的而非宿主的。部署产物在 home 下，
# 容器内可见。源码取自 minion-masters/assets-mm-gf-vulkan（不依赖 MM 运行时部署）。
# 默认 auto：检测到 gf 驱动（libvulkan_gf.so）就启用 hw，否则 sw；
# DOUBAO_GPU=sw/hw 可显式覆盖（hw 异常时回退 sw）。
if [[ -z "${DOUBAO_GPU:-}" || "${DOUBAO_GPU}" == "auto" ]]; then
    if [[ -f /usr/lib/x86_64-linux-gnu/libvulkan_gf.so ]]; then DOUBAO_GPU=hw; else DOUBAO_GPU=sw; fi
fi
export DOUBAO_GPU
GF_ACCEL="${DOUBAO_GF_ACCEL:-$HOME/.local/share/doubao-wine11/gf-accel}"
if [[ "$DOUBAO_GPU" == "hw" && -z "${DOUBAO_STEAM_RUNTIME_CHILD:-}" ]]; then
    GF_ASSETS="$BASE_DIR/../minion-masters/assets-mm-gf-vulkan"
    gf_deploy() {
        mkdir -p "$GF_ACCEL/lib" || return 1
        # 厂商源库为只读，cp 保留权限位会导致二次覆盖 EACCES —— 先删再拷
        rm -f "$GF_ACCEL/lib/libvulkan_gf.so" "$GF_ACCEL/lib/libSC.so"
        cp /usr/lib/x86_64-linux-gnu/libvulkan_gf.so "$GF_ACCEL/lib/" || return 1
        # libSC.so（厂商着色器编译器）不在 pressure-vessel 的驱动合并列表里，
        # 容器内找不到会让 gf ICD 建备时崩（0xC0000005）；一并拷入并经
        # LD_LIBRARY_PATH 暴露（与随从大师 hw 的 libSC 软链同作用）。
        cp /usr/lib/x86_64-linux-gnu/libSC.so "$GF_ACCEL/lib/" || return 1
        cp "$GF_ASSETS/os-release" "$GF_ACCEL/os-release" || return 1
        # 垫片源码里的路径写死为 MM 部署目录，编译前重写为本部署目录
        sed "s|/home/ubuntu/gf-compat|$GF_ACCEL|g" "$GF_ASSETS/libvulkan_gf_shim.c" > "$GF_ACCEL/.shim.c" || return 1
        gcc -shared -fPIC -O2 -o "$GF_ACCEL/lib/libvulkan_gf_shim.so" "$GF_ACCEL/.shim.c" -ldl || return 1
        rm -f "$GF_ACCEL/.shim.c"
        sed "s|/home/ubuntu/gf-compat|$GF_ACCEL|g" "$GF_ASSETS/osrelease_redirect.c" > "$GF_ACCEL/.osr.c" || return 1
        gcc -shared -fPIC -O2 -o "$GF_ACCEL/lib/osrelease_redirect.so" "$GF_ACCEL/.osr.c" -ldl || return 1
        rm -f "$GF_ACCEL/.osr.c"
        printf '{\n    "ICD": {\n        "api_version": "1.3.224",\n        "library_path": "lib/libvulkan_gf_shim.so"\n    },\n    "file_format_version": "1.0.0"\n}\n' > "$GF_ACCEL/gf_icd.json" || return 1
    }
    if [[ ! -f "$GF_ACCEL/lib/libvulkan_gf_shim.so" || ! -f "$GF_ACCEL/lib/osrelease_redirect.so" \
          || ! -f "$GF_ACCEL/gf_icd.json" || ! -f "$GF_ACCEL/lib/libvulkan_gf.so" \
          || ! -f "$GF_ACCEL/lib/libSC.so" ]]; then
        echo ">> 首次硬件加速：部署 gf 垫片到 $GF_ACCEL" >&2
        if ! gf_deploy; then
            echo "错误：gf 垫片部署失败（需要 gcc 与 minion-masters/assets-mm-gf-vulkan 源码）。" >&2
            echo "可改用 DOUBAO_GPU=sw 软件渲染。" >&2
            exit 1
        fi
    fi
fi

# 用 DOUBAO_STEAM_RUNTIME_CHILD 防止容器里二次包装；DOUBAO_NO_STEAM_RUNTIME=1
# 走轻量裸 wine 路径（干净图形会话下可用，旧污染会话下会黑屏），用于对照诊断。
# DOUBAO_FAST=1 是它的别名：省掉约 1.2 秒容器启动开销，稳健性略降，属可选项。
[[ "${DOUBAO_FAST:-0}" == "1" && -z "${DOUBAO_NO_STEAM_RUNTIME:-}" ]] && export DOUBAO_NO_STEAM_RUNTIME=1
if [[ -z "${DOUBAO_STEAM_RUNTIME_CHILD:-}" && "${DOUBAO_NO_STEAM_RUNTIME:-0}" != 1 ]]; then
    STEAM_COMMON="${STEAM_COMMON:-$HOME/.local/share/Steam/steamapps/common}"
    RUNTIME=""
    for candidate in "$STEAM_COMMON/SteamLinuxRuntime_sniper/run-in-sniper" \
                     "$STEAM_COMMON/SteamLinuxRuntime_soldier/run-in-soldier"; do
        if [[ -x "$candidate" ]]; then
            RUNTIME="$candidate"
            break
        fi
    done
    if [[ -z "$RUNTIME" ]]; then
        echo "错误：找不到 Steam Runtime 容器启动器（run-in-sniper）。" >&2
        echo "请安装 SteamLinuxRuntime_sniper（容器可隔离宿主图形栈），" >&2
        echo "或显式设置 DOUBAO_NO_STEAM_RUNTIME=1 走裸 wine 路径。" >&2
        exit 1
    fi
    exec env DOUBAO_STEAM_RUNTIME_CHILD=1 "$RUNTIME" -- bash "$SELF" "$@"
fi

PROTON="${DOUBAO_PROTON:-$HOME/.local/share/Steam/steamapps/common/Proton 11.0}"
PREFIX="${DOUBAO_WINE11_PREFIX:-$HOME/.deepinwine/doubao-wine11}"

export WINEPREFIX="$PREFIX"
export WINEDLLPATH="$PROTON/files/lib/vkd3d:$PROTON/files/lib/wine"
export LD_LIBRARY_PATH="$PROTON/files/lib/x86_64-linux-gnu:$PROTON/files/lib/i386-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export WINEDLLOVERRIDES='mscoree,mshtml='

# 渲染后端：默认 auto（有 gf 驱动→hw 硬件加速，无→sw 软件渲染）；
# DOUBAO_GPU=sw/hw 显式覆盖。hw 路线：Chromium ANGLE/D3D11 → DXVK → gf Vulkan。
# 容器内 gf GLX/EGL 无法认领屏幕（厂商栈设备发现问题，见
# dkms/vendor-bug-report-gf-gbm-crash.md 问题三），
# 所以不走 WineD3D/GL；winex11 自身的 GL 探测仍留在 mesa llvmpipe（与随从大师
# hw 方案一致，避免 wow64 下 winex11 初始化 OpenGL 失败）。gf Vulkan 在容器内
# 需要四件套（2026-11-25 实测，探针 _work/vk-loader-probe.py）：
#   1) os-release 重定向垫片：gf ICD 有发行版白名单，容器内 /etc/os-release 是
#      SteamOS，读到后直接拒绝初始化（枚举返回 -3）；
#   2) VK_DRIVER_FILES 覆盖：pressure-vessel 会注入该变量，新版 loader 里它
#      优先于 VK_ICD_FILENAMES，只设后者会被无视；
#   3) 协商垫片 ICD：gf 裁掉 VK_KHR_external_semaphore_capabilities 等扩展且
#      协商接口版本过旧，新 loader 会拒载/拒建实例（复用随从大师的垫片源码）；
#   4) libSC.so 拷贝 + LD_LIBRARY_PATH：厂商着色器编译器不在容器驱动合并列表。
# 垫片部署在进容器前已完成（见上方）；这里只注入环境变量。
# 硬件路径异常时用 DOUBAO_GPU=sw 回退软件渲染。
if [[ "$DOUBAO_GPU" == "hw" ]]; then
    # winex11 自身 GL 探测留在 mesa 软件路径；DXVK 经 gf Vulkan 用 7G100
    export __GLX_VENDOR_LIBRARY_NAME=mesa
    export LIBGL_ALWAYS_SOFTWARE=1
    export GBM_BACKEND=dri
    export LD_PRELOAD="$GF_ACCEL/lib/osrelease_redirect.so${LD_PRELOAD:+:$LD_PRELOAD}"
    export LD_LIBRARY_PATH="$GF_ACCEL/lib:$LD_LIBRARY_PATH"
    export VK_ICD_FILENAMES="$GF_ACCEL/gf_icd.json"
    export VK_DRIVER_FILES="$GF_ACCEL/gf_icd.json"
    export VK_LOADER_LAYERS_DISABLE='VK_LAYER_VALVE_steam_overlay_64,VK_LAYER_VALVE_steam_overlay_32'
    # DXVK 接管 d3d11/dxgi。优先用仓库 minion-masters/dxvk23（v2.3-21，随从大师
    # hw 已验证）：Proton 11 自带 v2.7 要求 VK_KHR_maintenance5，gf 22.0.4 没有，
    # 会报 No adapters found（2026-11-25 实测）。WINEDLLPATH 只对 builtin 生效，
    # native dll 走标准搜索序（应用目录→system32），所以照 Proton 脚本的做法
    # 把 DXVK dll 拷进前缀 system32/syswow64；原 WineD3D dll 首次覆盖时备份为
    # *.wined3d-bak。sw 模式用 d3d11,dxgi=b 强制 builtin，不受已拷入的 DXVK 影响。
    DXVK_SRC="${DOUBAO_DXVK_DIR:-$BASE_DIR/../minion-masters/dxvk23}"
    [[ -d "$DXVK_SRC/x86_64-windows" ]] || DXVK_SRC="$PROTON/files/lib/wine/dxvk"
    for arch in x86_64:system32 i386:syswow64; do
        src="$DXVK_SRC/${arch%%:*}-windows"
        dst="$PREFIX/drive_c/windows/${arch##*:}"
        mkdir -p "$dst" || { echo "错误：无法创建 $dst" >&2; exit 1; }
        for dll in d3d11 dxgi; do
            if [[ -f "$src/$dll.dll" ]] && ! cmp -s "$src/$dll.dll" "$dst/$dll.dll"; then
                [[ -f "$dst/$dll.dll" && ! -f "$dst/$dll.dll.wined3d-bak" ]] \
                    && cp -f "$dst/$dll.dll" "$dst/$dll.dll.wined3d-bak"
                cp -f "$src/$dll.dll" "$dst/$dll.dll" || { echo "错误：无法写入 $dst/$dll.dll" >&2; exit 1; }
            fi
        done
    done
    export WINEDLLOVERRIDES='d3d11,dxgi=n,b;mscoree,mshtml='
    unset WINE_D3D_CONFIG
    # gf 的 DXVK GPU 合成路径有确定性缺块/破线 bug（设置弹窗等叠加层，
    # 2026-11-25 多轮 A/B 定案：--disable-gpu-compositing 才能修；换 DXVK 2.5、
    # 关 Graphite、关 GPU 光栅、cached 动态缓冲、scissor 钳制均无效）。
    # 折中：合成走 CPU（Skia 多线程，比 llvmpipe GL 合成快），光栅强制留给 gf。
    # 实测滚动负载 CPU：sw 89% / 本组合 78% / 完整 hw 72%，收益大部分保留。
    EXTRA_APP_ARGS=(--disable-gpu-compositing --enable-gpu-rasterization)
else
    export __GLX_VENDOR_LIBRARY_NAME=mesa
    export LIBGL_ALWAYS_SOFTWARE=1
    export GBM_BACKEND=dri
    export VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json
    export WINE_D3D_CONFIG='renderer=gl'
    # hw 模式可能已把 DXVK dll 拷进 system32；强制 builtin 保证 sw 仍是 WineD3D/GL
    export WINEDLLOVERRIDES='d3d11,dxgi=b;mscoree,mshtml='
    EXTRA_APP_ARGS=()
fi

APP=$(find "$PREFIX/drive_c/users" -path '*/Doubao/Application/app/Doubao.exe' -print -quit)
if [[ -z "$APP" ]]; then
    echo "错误：未在 $PREFIX 中找到豆包客户端。" >&2
    exit 1
fi

LOG_FILE="${DOUBAO_WINE11_LOG:-$HOME/.local/share/doubao-wine11/launch.log}"
mkdir -p "$(dirname "$LOG_FILE")" 2>/dev/null || true
# 启动日志只在排障时才看，却会无限增长（实测 89 MB）。超过阈值先轮转为 .1，
# 只保留一份历史，不删用户数据；阈值可用 DOUBAO_LOG_MAX_BYTES 调整。
LOG_MAX_BYTES="${DOUBAO_LOG_MAX_BYTES:-20971520}"
if [[ -f "$LOG_FILE" ]] && [[ "$(stat -c %s "$LOG_FILE" 2>/dev/null || echo 0)" -gt "$LOG_MAX_BYTES" ]]; then
    mv -f "$LOG_FILE" "$LOG_FILE.1" 2>/dev/null || true
fi
if ! : >> "$LOG_FILE" 2>/dev/null; then
    echo "错误：无法写入启动日志：$LOG_FILE" >&2
    exit 1
fi
printf '=== %s | %s %s ===\n' "$(date '+%F %T')" "$APP" "$*" >> "$LOG_FILE"
exec > >(tee -a "$LOG_FILE") 2>&1

# Wine 的 fixme 噪声每次启动刷数千行（经 tee 落盘），默认关掉；err/warn 保留。
# 需要完整诊断时显式传 WINEDEBUG（如 fixme+all、+relay），外部值优先。
export WINEDEBUG="${WINEDEBUG:-fixme-all}"

# Proton 的 files/bin/wine 壳会把 64 位程序交给 32 位 loader，32 位侧加载不了 64 位 PE，
# 只好经 start.exe /exec 再转一次（实测多花约 1.06 秒）。Proton 脚本自己的做法是
# 直接用 64 位 loader 并设 WINELOADERNOEXEC=1（注释：avoid restart through start.exe）。
# 这里按 PE 头判断架构：64 位走直连，32 位或无 64 位 loader 时回退旧路径。
# DOUBAO_WINE_LOADER=legacy 可强制旧行为，用于对照诊断。
pe_machine() {
    local off
    off=$(od -An -tu4 -j 60 -N 4 -- "$1" 2>/dev/null | tr -d ' \n')
    [[ "$off" =~ ^[0-9]+$ ]] || return 1
    od -An -tx2 -j $((off + 4)) -N 2 -- "$1" 2>/dev/null | tr -d ' \n'
}
WINE_LOADER=("$PROTON/files/bin/wine")
WIN_APP="$APP"
if [[ "${DOUBAO_WINE_LOADER:-auto}" != "legacy" && "$(pe_machine "$APP")" == "8664" ]] \
   && [[ -x "$PROTON/files/lib/wine/x86_64-unix/wine" ]]; then
    export WINELOADERNOEXEC=1
    WINE_LOADER=("$PROTON/files/lib/wine/x86_64-unix/wine")
    if [[ -x "$PROTON/files/lib/wine/x86_64-unix/wine-preloader" ]]; then
        WINE_LOADER=("$PROTON/files/lib/wine/x86_64-unix/wine-preloader" "${WINE_LOADER[@]}")
    fi
    # 直连 loader 会把传入的参数原样作为程序自己的命令行/模块路径。以前经 start.exe
    # 时豆包看到的是 C:\users\<user>\...，这里保持一致，避免自检/更新逻辑因路径
    # 变成 Z:\home\... 而误判；前缀不在 drive_c 下时退回原样。
    rel="${APP#"$PREFIX/drive_c"}"
    if [[ "$rel" != "$APP" ]]; then
        WIN_APP="C:${rel//\//\\}"
    fi
fi
exec "${WINE_LOADER[@]}" "$WIN_APP" "${EXTRA_APP_ARGS[@]}" "$@"
