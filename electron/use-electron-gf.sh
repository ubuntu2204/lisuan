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
#   bash use-electron-gf.sh --app qoder-cn                  # Qoder 国内版
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
# 协商垫片: gf 裁掉了 VK_KHR_external_semaphore_capabilities / _fence_capabilities,
# 而新版 Vulkan loader 严格校验"请求的实例扩展必须由某个 ICD 暴露", Electron/Chromium
# 会请求前者 -> vkCreateInstance 直接失败 (-7 VK_ERROR_INCOMPATIBLE_DRIVER)。
# 垫片对外报协商版本 5 并补报被裁扩展, 故优先使用它。
GF_VK_ICD_SHIM="${GF_VK_ICD_SHIM:-$HOME/gf-compat/gf_icd.json}"

# ---- 通用基础开关 (修显示异常) ----
# 注: 仅 --ozone-platform-hint=x11 在 Electron 43 (Qoder) 上被忽略, 应用仍走 wayland
# 原生 (子进程 --ozone-platform=wayland, 拖拽/缩放窗口残缺), 需再加显式
# --ozone-platform=x11 强制 XWayland; 两个并存以兼容新旧 Chromium (未知开关安全忽略)。
BASE_FLAGS=(
  --no-sandbox
  --ozone-platform=x11
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
START_AFTER=0
ACCEL=1

while [ $# -gt 0 ]; do
  case "$1" in
    --app) APP="$2"; shift 2 ;;
    --cmd) CMD="$2"; shift 2 ;;
    --appimage) APPIMAGE="$2"; shift 2 ;;
    --install-desktop) INSTALL_DESKTOP=1; shift ;;
    --start) START_AFTER=1; shift ;;
    --no-hw) ACCEL=0; shift ;;
    --hw) ACCEL=1; shift ;;
    *) break ;;
  esac
done

resolve_app() {
  case "$APP" in
    codebuddy)
      # CodeBuddy（国际版 / 国内版）。国内版在 Ubuntu 上以 "CodeBuddy CN" 的
      # 名字安装，命令名为 buddycn、桌面项为 buddycn.desktop（2026-09-06 实测）。
      # 按顺序探测：codebuddy(国际) → buddycn(国内) → 常见安装路径。
      for c in codebuddy codebuddy-ai buddycn /usr/share/buddycn/bin/buddycn \
               "$HOME/development/CodeBuddy-*.AppImage" \
               "$HOME/.local/share/codebuddy/codebuddy" /opt/codebuddy/codebuddy; do
        if command -v "$c" >/dev/null 2>&1; then CMD="$(command -v "$c")"; break; fi
        if [ -e "$c" ]; then CMD="$c"; break; fi
      done
      DESKTOP_FILES=("$HOME/.local/share/applications/codebuddy.desktop"
                      "/usr/share/applications/codebuddy.desktop"
                      "$HOME/.local/share/applications/buddycn.desktop"
                      "/usr/share/applications/buddycn.desktop")
      ;;
    qoder|qoder-cn)
      # --app qoder 优先国际版, 未安装时回退国内版 (qoder-cn);
      # --app qoder-cn 只认国内版。桌面项按 Exec 实际指向过滤, 不会互相误改。
      if [ "$APP" = "qoder-cn" ]; then
        CMD="$(command -v qoder-cn || echo /usr/bin/qoder-cn)"
      else
        CMD="$(command -v qoder || command -v qoder-cn || echo /usr/bin/qoder)"
      fi
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
    *) echo "!! 未知 --app: $APP (支持 codebuddy|qoder|qoder-cn|zcode, 或用 --cmd/--appimage)" >&2; exit 1 ;;
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
    # 只改第一个 Exec=（主入口）：某些桌面项还有 Desktop Action 的 Exec
    # （如 CodeBuddy CN 的 --new-window），全改会丢动作参数导致功能缺失。
    if sed -i "0,/^Exec=/s|^Exec=.*|Exec=$SELF --app $APP %U|" "$target" 2>/dev/null; then
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
  # --start: 装完桌面项后不退出, 继续走下面的启动逻辑（"点图标即用" 一键生效）
  [ "$START_AFTER" = 1 ] || exit 0
fi

# ---- 组装启动 ----
EXTRA="${GF_EXTRA_FLAGS:-}"

# 若本脚本从另一个 Qoder 变体的内置终端启动, 环境里会带宿主注入的
# QODER_*/QODERCN_* 变量 (身份、鉴权 token、配置目录等), 会导致
# "运行身份与产物身份不一致" 崩溃或 job_token_invalid; 启动新实例前全部清掉
if [ "$APP" = "qoder" ] || [ "$APP" = "qoder-cn" ]; then
  while IFS='=' read -r _qvar _; do
    case "$_qvar" in QODER*|QODERCN*) unset "$_qvar" ;; esac
  done < <(env)
  unset ELECTRON_PA_APP_NAME
fi

if [ "$ACCEL" = 1 ]; then
  # gf 硬件加速: 注入 gf 驱动 env + Chromium GPU 加速开关
  export __GLX_VENDOR_LIBRARY_NAME=gf
  export LIBGL_ALWAYS_SOFTWARE=0
  # 外部已指定 ICD 时不覆盖(便于改用其它替代实现)
  if [ -z "${VK_ICD_FILENAMES:-}" ]; then
    if [ -e "$GF_VK_ICD_SHIM" ]; then
      # 优先协商垫片, 规避 gf 缺 external_semaphore_capabilities 导致的 -7
      export VK_ICD_FILENAMES="$GF_VK_ICD_SHIM"
    elif [ -e "$GF_VK_ICD" ]; then
      export VK_ICD_FILENAMES="$GF_VK_ICD"
    fi
  fi
  echo ">> 以 gf 硬件加速模式启动: $CMD (ICD=${VK_ICD_FILENAMES:-默认})"
  exec env "$CMD" "${HW_FLAGS[@]}" $EXTRA "$@"
else
  echo ">> 以软件渲染(稳定)模式启动: $CMD"
  exec "$CMD" "${SW_FLAGS[@]}" $EXTRA "$@"
fi
