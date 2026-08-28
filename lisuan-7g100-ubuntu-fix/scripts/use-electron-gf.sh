#!/bin/bash
# use-electron-gf.sh - 通用 Electron / Chromium 系应用 wrapper（砾算 7G100 + Ubuntu 24.04）
#
# 适用: 在国产 GPU + Ubuntu 24.04 上, Electron 类桌面应用出现
#       "界面空白 / 花屏 / 黑块 / 经 XWayland 渲染残缺" 等显示异常, 以及
#       在显示正常的基础上, 通过 gf 驱动启用 GPU 硬件加速 (替代默认软件渲染)。
#
# 已内置适配: codebuddy 国内版、qoder 国内版、ZCode（Foxit AI 编码助手）。
# 其它 Electron 应用可用 --cmd / --appimage 复用本脚本。
#
# 修复根因 (与第三节 Steam CEF / Foxit WebEngine 同源, 本质都是 Chromium 渲染进程):
#   1. Ubuntu 24.04 收紧 unprivileged user namespace -> 渲染进程需 --no-sandbox。
#   2. Wayland 原生后端残缺 -> --ozone-platform-hint=x11 经 XWayland 渲染。
#   3. 国产驱动 GPU 合成白屏 -> 默认 --disable-gpu-compositing；如需流畅度再开 gf 加速。
#
# gf 硬件加速原理 (与方案 A 探明一致):
#   - 宿主 gf GLX 已验证可用 (GLX_VENDOR_LIBRARY_NAME=gf glxinfo 可枚举 LISUAN 7G100)。
#   - 注入 __GLX_VENDOR_LIBRARY_NAME=gf + VK_ICD_FILENAMES 指向 gf ICD, 并让 Chromium
#     走桌面 GL (--use-gl=angle --use-angle=gl), 即由 gf 驱动承担渲染/栅格化。
#   - 注意: 只给"本应用"注入 gf env, 不影响 Steam 等其它进程 (全局注入 gf 会让 Steam
#     客户端 GLX 崩溃, 见第三节)。
#
# 用法:
#   bash use-electron-gf.sh --app codebuddy
#   bash use-electron-gf.sh --app qoder
#   bash use-electron-gf.sh --app codebuddy --no-hw        # 强制软件渲染(不稳定/白屏时回退)
#   bash use-electron-gf.sh --app qoder --install-desktop   # 把应用菜单 Exec 切到本 wrapper
#   bash use-electron-gf.sh --cmd /path/to/other-electron   # 任意 Electron 应用
#   GF_EXTRA_FLAGS="--enable-logging=stderr --v=1" bash use-electron-gf.sh --app qoder
#
# 默认: 启用 gf 硬件加速 (GF_ACCEL=1)。若白屏/花屏, 用 --no-hw 回退软件渲染。

set -u

# ---- gf 驱动路径 (随环境调整) ----
GF_GLX_LIB="/usr/lib/x86_64-linux-gnu/libGLX_gf.so"
GF_VK_ICD="/usr/share/vulkan/icd.d/gf_icd.x86_64.json"

# ---- 通用基础开关 (修显示异常) ----
BASE_FLAGS=(
  --no-sandbox
  --ozone-platform-hint=x11
  --disable-gpu-compositing
)

# ---- 软件渲染回退开关 ----
SW_FLAGS=(
  "${BASE_FLAGS[@]}"
  --disable-gpu
)

# ---- gf 硬件加速开关 ----
HW_FLAGS=(
  "${BASE_FLAGS[@]}"
  --ignore-gpu-blocklist
  --enable-gpu-rasterization
  --use-gl=angle
  --use-angle=gl
  --enable-features=Vulkan
)

# ---- 应用解析 ----
APP=""
CMD=""
APPIMAGE=""
DESKTOP_FILES=()
INSTALL_DESKTOP=0
ACCEL=1

while [ $# -gt 0 ]; do
  case "$1" in
    --app) APP="$2"; shift 2 ;;
    --cmd) CMD="$2"; shift 2 ;;
    --appimage) APPIMAGE="$2"; shift 2 ;;
    --install-desktop) INSTALL_DESKTOP=1; shift ;;
    --no-hw) ACCEL=0; shift ;;
    --hw) ACCEL=1; shift ;;
    *) break ;;
  esac
done

resolve_app() {
  case "$APP" in
    codebuddy)
      # codebuddy 国内版常见命令名/路径 (可按需扩展)
      for c in codebuddy codebuddy-ai "$HOME/development/CodeBuddy-*.AppImage" \
               "$HOME/.local/share/codebuddy/codebuddy" /opt/codebuddy/codebuddy; do
        if command -v "$c" >/dev/null 2>&1; then CMD="$(command -v "$c")"; break; fi
        if [ -e "$c" ]; then CMD="$c"; break; fi
      done
      DESKTOP_FILES=("$HOME/.local/share/applications/codebuddy.desktop"
                      "/usr/share/applications/codebuddy.desktop")
      ;;
    qoder)
      CMD="$(command -v qoder || echo /usr/bin/qoder)"
      DESKTOP_FILES=("/usr/share/applications/qoder.desktop"
                      "/usr/share/applications/qoder-cn.desktop"
                      "$HOME/.local/share/applications/qoder.desktop"
                      "$HOME/.local/share/applications/qoder-cn.desktop")
      ;;
    zcode)
      CMD="${HOME}/development/ZCode-3.7.7-linux-x64.AppImage"
      DESKTOP_FILES=("$HOME/.local/share/applications/zcode.desktop")
      ;;
    "") : ;; # 由 --cmd/--appimage 提供
    *) echo "!! 未知 --app: $APP (支持 codebuddy|qoder|zcode, 或用 --cmd/--appimage)" >&2; exit 1 ;;
  esac
}

resolve_app

if [ -z "$CMD" ] && [ -n "$APPIMAGE" ]; then CMD="$APPIMAGE"; fi
if [ -z "$CMD" ]; then
  echo "!! 未解析到启动命令。请显式指定: --cmd /path/to/electron 或 --appimage /path/to.appimage" >&2
  exit 1
fi
if [ ! -e "$CMD" ] && ! command -v "$CMD" >/dev/null 2>&1; then
  echo "!! 启动命令不存在: $CMD" >&2
  exit 1
fi

# 桌面项改写 (把应用菜单 Exec 切到本 wrapper, 之后从菜单直启即带修复+加速)
# 策略: 系统级桌面项 (/usr/share) 复制到用户级 (~/.local/share) 再改写,
#       用户级优先级更高且无需 sudo。
if [ "$INSTALL_DESKTOP" = 1 ]; then
  SELF="$(readlink -f "$0")"
  UDIR="$HOME/.local/share/applications"
  mkdir -p "$UDIR"
  CMD_REAL="$(readlink -f "$CMD" 2>/dev/null || echo "$CMD")"
  found=0
  for d in "${DESKTOP_FILES[@]}"; do
    [ -f "$d" ] || continue
    target="$d"
    if [[ "$d" == /usr/share/* ]]; then
      target="$UDIR/$(basename "$d")"
    fi
    if [ -f "$target" ] && grep -q "$SELF" "$target"; then
      chmod +x "$target" 2>/dev/null || true
      echo "    $target 已指向 wrapper, 跳过"; found=1; continue
    fi
    # 只改写 Exec 指向本次解析出的启动命令的桌面项,
    # 避免把同一应用的其它变体 (如 qoder-cn) 误改写到本二进制
    exec_bin="$(grep '^Exec=' "$d" | head -1 | sed -E 's/^Exec=//; s/ *%[a-zA-Z]+.*$//; s/^"//; s/"$//')"
    exec_real="$(readlink -f "$exec_bin" 2>/dev/null || echo "$exec_bin")"
    if [ "$exec_real" != "$CMD_REAL" ]; then
      echo "    跳过 $d (Exec 指向 $exec_real, 非 $CMD_REAL)"
      continue
    fi
    if [[ "$d" == /usr/share/* ]]; then
      cp "$d" "$target" 2>/dev/null || { echo "!! 无法复制 $d -> $target (需 sudo?)" >&2; continue; }
    fi
    if sed -i "s|^Exec=.*|Exec=$SELF --app $APP %U|" "$target" 2>/dev/null; then
      # 补齐执行位: 系统桌面项复制过来时通常无执行位, 且 sed -i 会丢失执行位;
      # 不可执行的脚本型桌面项会被 GNOME 从应用菜单隐藏
      chmod +x "$target" 2>/dev/null || true
      echo "OK: $target 的 Exec 已切到本 wrapper (--app $APP)"
      found=1
    else
      echo "!! 改写失败: $target (权限不足?)" >&2
    fi
  done
  [ "$found" = 0 ] && echo "!! 未找到任何桌面项: ${DESKTOP_FILES[*]}" >&2
  update-desktop-database "$UDIR" 2>/dev/null || true
  exit 0
fi

# ---- 组装启动 ----
EXTRA="${GF_EXTRA_FLAGS:-}"

if [ "$ACCEL" = 1 ]; then
  # gf 硬件加速: 注入 gf 驱动 env + Chromium GPU 加速开关
  export __GLX_VENDOR_LIBRARY_NAME=gf
  export LIBGL_ALWAYS_SOFTWARE=0
  [ -e "$GF_VK_ICD" ] && export VK_ICD_FILENAMES="$GF_VK_ICD"
  echo ">> 以 gf 硬件加速模式启动: $CMD"
  exec env "$CMD" "${HW_FLAGS[@]}" $EXTRA "$@"
else
  echo ">> 以软件渲染(稳定)模式启动: $CMD"
  exec "$CMD" "${SW_FLAGS[@]}" $EXTRA "$@"
fi
