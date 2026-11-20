#!/usr/bin/env bash
# 至砺算：11 对战平台单应用启动器，供桌面入口调用。
#
# 只在 wine 进程树上设置 Proton 11 的运行库环境与独立前缀，
# 不污染全局 shell、不修改 Steam 自身文件。单次前台启动，
# 不做存活监控、自动重启或守护；退出码原样透传。
#
# 平台主窗口是 Depth 32/ARGB 窗口，在 Wayland/XWayland 下会被合成成完全不可见；
# 默认注入 XLIB_SKIP_ARGB_VISUALS=1 让它使用 Depth 24（已实测窗口变为可见）。
#
# 现行默认：官网正式版 2.6.3.48（独立前缀 ~/.deepinwine/11duizhan-263），
# 实测窗口可见、无栈溢出、GDI 句柄峰值 142，不需要 3.0 公测版的栈/GDI 补丁。
# 3.0 公测版的自更新会在 3.0.0.68 <-> 3.0.0.79 间切换并冲掉栈补丁；无参数启动
# 默认追加 -updatemode=0 冻结版本（E11_ALLOW_UPDATE=1 可恢复 A/B）。这是绕过更新器，
# 不是修好更新器本身。E11_WINE11_PREFIX 可切回旧前缀做对照。
#
# 用法：
#   ./11/run-11duizhan.sh [参数...]   # 启动平台（参数透传给 11loader.exe）
#   ./11/run-11duizhan.sh --status    # 只读体检
#   ./11/run-11duizhan.sh --help
#
# 环境变量：E11_PROTON / E11_WINE11_PREFIX / E11_INSTALL_DIR / E11_WINEDEBUG /
#   E11_SKIP_ARGB_VISUALS（=0 可关闭 ARGB 规避，用于 A/B 对照）/
#   E11_ALLOW_UPDATE（=1 恢复自更新）/
#   E11_GDI_WRAP（=1 注入 GDI 诊断/回收包装层，需先跑 fix-11-gdi.sh）/
#   E11_VK_ICD（覆盖 Vulkan ICD；=skip 不注入，用于 A/B）/
#   E11_RENDER_LAYER（默认 disable：每次启动禁用所有 gp_* 的 DxRender/DxWorker；
#     =keep 可关闭，用于 A/B。11-19 只禁 gp_1290，11-20 平台改用 gp_1292
#     导致拉起挂死，故固化为每次启动覆盖所有 gp_* 目录）
# License: Apache-2.0
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROTON="${E11_PROTON:-$HOME/.local/share/Steam/steamapps/common/Proton 11.0}"
PROTON="${PROTON/#\~/$HOME}"
PREFIX="${E11_WINE11_PREFIX:-$HOME/.deepinwine/11duizhan-263}"
PREFIX="${PREFIX/#\~/$HOME}"
INSTALL_DIR_REL="${E11_INSTALL_DIR:-11duizhan}"
INSTALL_DIR_REL="${INSTALL_DIR_REL#/}"
BASE="$PREFIX/drive_c/$INSTALL_DIR_REL"
MAIN=""
for name in 11Loader.exe 11loader.exe; do
    if [[ -s "$BASE/$name" && "$(head -c2 "$BASE/$name")" == "MZ" ]]; then
        MAIN="$BASE/$name"
        break
    fi
done
WINE="$PROTON/files/bin/wine"

die() { echo "错误：$*" >&2; exit 1; }

usage() {
    cat <<EOF
用法：$(basename "$0") [选项] [参数...]

  （无参数） 前台启动 11 对战平台（参数透传给主程序，兼容大小写两种文件名）
  --status   只读体检：wine、安装前缀、主程序 PE
  --help     显示本说明

环境变量：E11_PROTON、E11_WINE11_PREFIX、E11_INSTALL_DIR、E11_WINEDEBUG、
          E11_SKIP_ARGB_VISUALS（默认 1，规避 Depth 32 窗口不可见）、
          E11_ALLOW_UPDATE（=1 恢复自更新；默认 -updatemode=0 冻结版本）、
          E11_GDI_WRAP（=1 注入 GDI 包装层，需先跑 fix-11-gdi.sh）、
          E11_RENDER_LAYER（默认 disable，禁用所有 gp_* 渲染替换层）。
崩溃后不会自动重启；请按 .codebuddy/rules/崩溃处理约定.md 取证定位。
EOF
}

case "${1:-}" in
    --help|-h) usage; exit 0 ;;
    --status)
        [[ -x "$WINE" ]] && echo "  [OK] wine：$WINE" || { echo "  [--] wine 缺失：$WINE"; exit 1; }
        [[ -s "$MAIN" && "$(head -c2 "$MAIN")" == "MZ" ]] && \
            echo "  [OK] 主程序：$MAIN" || { echo "  [--] 主程序缺失或不是 PE：$MAIN"; exit 1; }
        exit 0
        ;;
esac

[[ -x "$WINE" ]] || die "找不到 Proton 11 的 wine：$WINE"
[[ -s "$MAIN" && "$(head -c2 "$MAIN")" == "MZ" ]] || die "主程序缺失或不是有效 PE：$MAIN"

if [[ $# -eq 0 && "${E11_ALLOW_UPDATE:-0}" != "1" ]]; then
    set -- -updatemode=0
fi

dlloverrides='mscoree,mshtml='
[[ "${E11_GDI_WRAP:-0}" == "1" ]] && dlloverrides="$dlloverrides;user32=n"

# 平台拉起 War3（32 位 D3D8→DXVK→Vulkan）继承本进程树环境；本机 gf 驱动只有
# 64 位 ICD，32 位链必须用 llvmpipe（根因与修法同 war3/run-war3.sh，2026-11-18 实测）。
# 平台自身 UI 走 CEF/GL（GL 已禁用），不受 Vulkan ICD 影响。E11_VK_ICD=skip 可关。
vk_icd="${E11_VK_ICD:-/usr/share/vulkan/icd.d/lvp_icd.json}"
vk_env=()
if [[ "$vk_icd" != "skip" && -f "$vk_icd" ]]; then
    vk_env=(VK_ICD_FILENAMES="$vk_icd")
fi

# 固化 11-19/11-20 的拉起修复：平台按地图/版本切换 gp_1290/gp_1292/... 补丁
# 目录，并在进房间时按缺失文件重新部署 DLL。每次启动前由 fix-11-render.sh
# 把所有 gp_* 的 DxRender/DxWorker 改名、补丁 War3Shell 名称并置目录只读，
# 新下载的目录同样覆盖。
if [[ "${E11_RENDER_LAYER:-disable}" == "disable" ]]; then
    if ! bash "$SCRIPT_DIR/fix-11-render.sh" >/dev/null 2>&1; then
        echo "警告：禁用 gp_* 渲染层未完全成功，请运行 11/fix-11-render.sh --status 查看" >&2
    fi
fi

libs="$PROTON/files/lib/x86_64-linux-gnu:$PROTON/files/lib/i386-linux-gnu"
[[ -n "${LD_LIBRARY_PATH:-}" ]] && libs="$libs:$LD_LIBRARY_PATH"
cd "$(dirname "$MAIN")"
exec env WINEPREFIX="$PREFIX" \
    WINEDLLPATH="$PROTON/files/lib/vkd3d:$PROTON/files/lib/wine" \
    LD_LIBRARY_PATH="$libs" \
    WINEDLLOVERRIDES="$dlloverrides" \
    XLIB_SKIP_ARGB_VISUALS="${E11_SKIP_ARGB_VISUALS:-1}" \
    WINEDEBUG="${E11_WINEDEBUG:--all}" \
    "${vk_env[@]}" \
    "$WINE" "$MAIN" "$@"
