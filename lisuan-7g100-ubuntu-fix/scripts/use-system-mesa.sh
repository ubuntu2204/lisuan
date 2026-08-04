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

FORCE_DRI_GBM=0
PRINT_ENV=0
while [[ $# -gt 0 ]]; do
  case "$1" in
    --force-dri-gbm) FORCE_DRI_GBM=1; shift ;;
    --print-env) PRINT_ENV=1; shift ;;
    -h|--help) grep '^#' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    --) shift; break ;;
    *) break ;;
  esac
done

# Force the Mesa implementations of the glvnd-dispatched GLX/EGL APIs.
# (Default glvnd ordering prefers the Lisuan vendor: 10_gf.json < 50_mesa.json.)
export __GLX_VENDOR_LIBRARY_NAME=mesa
export __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json

if [[ "$FORCE_DRI_GBM" == "1" ]]; then
  # System libgbm picks backends by DRM driver name first ("gf" for this
  # card); the vendor gf_gbm.so segfaults on primary nodes under
  # libgbm 25.2.x. The distro "dri" backend is the safe path.
  export GBM_BACKEND=dri
fi

if [[ "$PRINT_ENV" == "1" ]]; then
  echo "__GLX_VENDOR_LIBRARY_NAME=$__GLX_VENDOR_LIBRARY_NAME"
  echo "__EGL_VENDOR_LIBRARY_FILENAMES=$__EGL_VENDOR_LIBRARY_FILENAMES"
  if [[ "$FORCE_DRI_GBM" == "1" ]]; then
    echo "GBM_BACKEND=$GBM_BACKEND"
  fi
  exit 0
fi

if [[ $# -eq 0 ]]; then
  echo "usage: $0 [options] [--] COMMAND [ARGS...]" >&2
  echo "       $0 --print-env" >&2
  exit 2
fi

exec "$@"
