#!/bin/bash
# use-zcode.sh - ZCode (Electron AppImage) 启动 wrapper, 国产平台 Ubuntu 24.04 显示异常修复
#
# 适用: 砾算 7G100 + Ubuntu 24.04 上, ZCode (Electron 应用) 出现界面空白 / 花屏 /
#       黑块 / 经 XWayland 渲染残缺等"显示异常"。
#
# 根因与修复 (与 Steam CEF / Foxit WebEngine 同一类, 本质都是 Chromium 渲染进程):
#   1. 桌面项 Exec 漏了官方内置的 --no-sandbox: Ubuntu 24.04 (内核 6.8+/7.0) 收紧
#      unprivileged user namespace, 渲染进程在沙箱里起不来 -> 界面全白。
#   2. Wayland 会话下 Electron 原生 wayland 后端窗口异常: --ozone-platform-hint=x11
#      让其经 XWayland 渲染 (与 Foxit Qt 的 xcb 同思路)。
#   3. 砾算 G100 + Mesa 驱动 GL 特性不全, Chromium GPU 合成白屏/花屏: 禁用 GPU 与
#      GPU 合成 (与 Foxit WebEngine 修复同源)。
#
# 用法:
#   bash use-zcode.sh                          # 启动 ZCode
#   bash use-zcode.sh --install-desktop        # 把系统 zcode.desktop 的 Exec 切到本 wrapper
#   ZCODE_EXTRA_FLAGS="--enable-logging" \
#       bash use-zcode.sh                      # 追加自定义 Chromium 开关
#
# 注: 本脚本只负责"应用启动环境", 不修改系统其它部分; 卸载只需把桌面项 Exec 改回
#     AppImage 真身 (见 --install-desktop 逆操作) 或直接删除。

APPIMAGE="$HOME/development/ZCode-3.7.7-linux-x64.AppImage"
DESKTOP_SRC="$HOME/.local/share/applications/zcode.desktop"
DESKTOP_USER="$HOME/.local/share/applications/zcode.desktop"

if [ "$1" = "--install-desktop" ]; then
  [ -f "$DESKTOP_USER" ] || { echo "!! 未找到 $DESKTOP_USER" >&2; exit 1; }
  # 仅当 Exec 指向 AppImage 本体时才改写 (避免重复套娃)
  if grep -q "$APPIMAGE" "$DESKTOP_USER" && ! grep -q "$(readlink -f "$0")" "$DESKTOP_USER"; then
    sed -i "s|^Exec=.*|Exec=$(readlink -f "$0") %U|" "$DESKTOP_USER"
    echo "OK: $DESKTOP_USER 的 Exec 已切到本 wrapper"
    update-desktop-database "$HOME/.local/share/applications" 2>/dev/null || true
  else
    echo "    桌面项已指向 wrapper 或不在预期位置, 跳过"
  fi
  exit 0
fi

[ -f "$APPIMAGE" ] || { echo "!! 未找到 $APPIMAGE" >&2; exit 1; }

exec "$APPIMAGE" \
  --no-sandbox \
  --ozone-platform-hint=x11 \
  --disable-gpu \
  --disable-gpu-compositing \
  ${ZCODE_EXTRA_FLAGS:-} \
  "$@"
