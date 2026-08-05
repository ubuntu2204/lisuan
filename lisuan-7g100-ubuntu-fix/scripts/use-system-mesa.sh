#!/usr/bin/env bash
#
# use-system-mesa.sh — run an application on the SYSTEM Mesa (25.2.x) GL
# stack instead of the Lisuan vendor's bundled Mesa 22.0.4 fork.
#
# Why this exists (see docs/system-mesa-analysis.md for the full analysis):
#   * The 7G100's vendor GL stack (libGLX_gf / libEGL_gf / gf_dri.so /
#     gf_gbm.so) is a self-contained Mesa 22.0.4 fork. It CANNOT be loaded
#     by the distro Mesa 25.2.x: 25.2's pipe-loader only knows drivers that
#     were compiled in (pipe_loader_drm.c driver_descriptors[]), and there
#     is no external Gallium pipe-driver mechanism anymore.
#   * What DOES work on the system Mesa today: zink (GL-on-Vulkan) on top
#     of the vendor Vulkan ICD (libvulkan_gf.so, Vulkan 1.3).
#       - GLX: mesa GLX auto-falls back to zink (kopper) for this card.
#       - EGL: mesa EGL has NO zink fallback; select the mesa EGL vendor
#         explicitly and you get zink on the Wayland/X11 EGL platforms.
#         NOTE: mesa EGL's GBM platform still crashes (vendor gf_gbm.so
#         segfaults under libgbm 25.2.x); use --force-dri-gbm to sidestep.
#
# Usage:
#   ./use-system-mesa.sh [options] [--] COMMAND [ARGS...]
#
# Options:
#   --force-dri-gbm   Also set GBM_BACKEND=dri so any libgbm user skips the
#                     broken vendor gf_gbm backend (safe fallback, zink/kms).
#   --print-env       Print the environment recipe and exit (no command run).
#   -h, --help        Show this help.
#
# Examples:
#   ./use-system-mesa.sh --print-env
#   ./use-system-mesa.sh glxinfo -B
#   ./use-system-mesa.sh --force-dri-gbm -- weston
#
# License: Apache-2.0
set -euo pipefail

# ---------------- 参数解析：只认选项，其余全部当作要运行的命令 ----------------
FORCE_DRI_GBM=0   # 是否同时强制 GBM 走 dri 后端（避开厂商 gf_gbm 段错误）
PRINT_ENV=0       # 只打印环境变量配方不执行命令（供其它脚本/用户抄作业）
while [[ $# -gt 0 ]]; do
  case "$1" in
    --force-dri-gbm) FORCE_DRI_GBM=1; shift ;;
    --print-env) PRINT_ENV=1; shift ;;
    -h|--help) grep '^#' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;   # 帮助=打印头部注释
    --) shift; break ;;   # -- 之后一律是命令
    *) break ;;           # 第一个非选项参数即命令开头
  esac
done

# 强制 glvnd 把 GLX/EGL 分发给 Mesa 实现（而非厂商 libGLX_gf/libEGL_gf）。
# glvnd 缺省按 JSON 文件名排序，10_gf.json 优先于 50_mesa.json，所以
# 必须显式指定；这两个变量只影响当前进程及其子进程。
export __GLX_VENDOR_LIBRARY_NAME=mesa
export __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json

if [[ "$FORCE_DRI_GBM" == "1" ]]; then
  # 系统 libgbm 默认按 DRM 驱动名选后端（本卡是 "gf" → 厂商 gf_gbm.so，
  # 在新 libgbm 下会段错误）；GBM_BACKEND=dri 强制用发行版 dri 后端
  # （走 zink/kms，安全）。见 docs/system-mesa-analysis.md §5 地雷 1。
  export GBM_BACKEND=dri
fi

# --print-env：输出配方供人工/其它脚本使用，不执行任何命令
if [[ "$PRINT_ENV" == "1" ]]; then
  echo "__GLX_VENDOR_LIBRARY_NAME=$__GLX_VENDOR_LIBRARY_NAME"
  echo "__EGL_VENDOR_LIBRARY_FILENAMES=$__EGL_VENDOR_LIBRARY_FILENAMES"
  if [[ "$FORCE_DRI_GBM" == "1" ]]; then
    echo "GBM_BACKEND=$GBM_BACKEND"
  fi
  exit 0
fi

# 未给命令时报用法（退出码 2 = 用法错误，便于脚本区分）
if [[ $# -eq 0 ]]; then
  echo "usage: $0 [options] [--] COMMAND [ARGS...]" >&2
  echo "       $0 --print-env" >&2
  exit 2
fi

# exec 替换本进程：环境变量自然继承给目标应用，退出码也原样传回
exec "$@"
