#!/usr/bin/env bash
#
# build-deb.sh — 把 install.py 及其依赖的 shell 修复脚本打包成 deb 包。
#
# 产物布局：
#   /usr/bin/lisuan-fix                     可执行入口（install.py 副本，无扩展名）
#   /usr/share/lisuan-fix/{install.py,lisuan_fix_core.py,lisuan_fix_gui.py}  模块
#   /usr/share/lisuan-fix/{dkms,mesa,steam,underlords,electron,kingdom-rush}/   修复脚本
#   /usr/share/applications/lisuan-fix.desktop                     终端菜单项
#
# 用法：
#   ./packaging/build-deb.sh [版本号]     # 默认版本号 = 当天日期（YYYY.MM.DD）
#
# 产物：packaging/dist/lisuan-fix_<版本>_all.deb
#
# License: Apache-2.0
set -euo pipefail

# 本脚本位于 packaging/，仓库根是其父目录；入口壳 install.py 在仓库根
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
VER="${1:-$(date +%Y.%m.%d)}"
PKG="lisuan-fix"
WORK="$(mktemp -d /tmp/${PKG}-build.XXXXXX)"
OUT_DIR="$HERE/dist"

trap 'rm -rf "$WORK"' EXIT

# ---- 组装安装树 -----------------------------------------------------------
T="$WORK/$PKG"
mkdir -p "$T/DEBIAN" \
         "$T/usr/bin" \
         "$T/usr/share/$PKG" \
         "$T/usr/share/applications"

# 主程序：/usr/bin 入口 + 模块原件（入口壳在仓库根，其余模块在 packaging/）
install -m 755 "$ROOT/install.py" "$T/usr/bin/$PKG"
install -m 644 "$ROOT/install.py" "$T/usr/share/$PKG/install.py"
for f in lisuan_fix_core.py lisuan_fix_gui.py lisuan_fix_md.py; do
  install -m 644 "$HERE/$f" "$T/usr/share/$PKG/$f"
done

# 修复脚本目录（保持相对结构；排除测试专用厂商源码与脏文件）
for d in dkms mesa steam underlords electron kingdom-rush; do
  mkdir -p "$T/usr/share/$PKG/$d"
  (cd "$ROOT" && find "$d" -type f \
      ! -path '*/vendor-src/*' \
      ! -name '*.bak' \
      ! -name '__pycache__/*' \
      ! -name '*.pyc' \
      -print0) |
  while IFS= read -r -d '' f; do
    rel="${f#./}"
    mkdir -p "$T/usr/share/$PKG/$(dirname "$rel")"
    install -m 644 "$ROOT/$rel" "$T/usr/share/$PKG/$rel"
  done
  # shell 脚本保留可执行位
  find "$T/usr/share/$PKG/$d" -name '*.sh' -exec chmod 755 {} +
done

# minion-masters（按需精简打包）：
#   主脚本 + 说明 + assets-mm-gf-vulkan（部署源：MMPatch/hook 源码/ICD）。
#   不打包：gf-compat/lib（34M 驱动库副本——部署时由脚本从宿主重建）、
#   dxvk23/、gf-lowmem-shim/（本地实验树）、MinionMasters-*.md（历史记录）。
MM="$ROOT/minion-masters"
mkdir -p "$T/usr/share/$PKG/minion-masters/assets-mm-gf-vulkan"
install -m 755 "$MM/fix-minion-masters-7g100-hw.sh" \
               "$T/usr/share/$PKG/minion-masters/"
install -m 644 "$MM/fix-minion-masters.md" "$T/usr/share/$PKG/minion-masters/"
(cd "$MM" && find assets-mm-gf-vulkan -type f \
    ! -name '*.bak' ! -name '__pycache__/*' ! -name '*.pyc' -print0) |
while IFS= read -r -d '' f; do
  mkdir -p "$T/usr/share/$PKG/minion-masters/$(dirname "$f")"
  install -m 644 "$MM/$f" "$T/usr/share/$PKG/minion-masters/$f"
done
find "$T/usr/share/$PKG/minion-masters" -name '*.sh' -exec chmod 755 {} +

# 菜单项（终端运行 TUI/文本菜单）
cat > "$T/usr/share/applications/$PKG.desktop" <<EOF
[Desktop Entry]
Name=Lisuan Fix (砺算 7G100 修复)
Comment=砺算 7G100 修复工具：DKMS / Mesa / Steam / 游戏 / Electron 适配
Exec=x-terminal-emulator -e /usr/bin/lisuan-fix
Terminal=false
Type=Application
Categories=System;Utility;
Keywords=lisuan;7G100;gpu;steam;
EOF

# ---- 控制文件 -------------------------------------------------------------
cat > "$T/DEBIAN/control" <<EOF
Package: $PKG
Version: $VER
Section: utils
Priority: optional
Architecture: all
Maintainer: Lisuan Fix Contributors <lisuan-fix@localhost>
Depends: python3 (>= 3.10), bash
Recommends: pciutils, kmod, dkms, e2fsprogs, python3-tk
Suggests: steam
Description: 砺算 7G100 GPU 修复工具（DKMS/Mesa/Steam/游戏/Electron 适配）
 一体化修复与体检工具，覆盖：
  - DKMS 驱动构建/加载（vermagic 错配、黑屏）
  - Mesa DRI 驱动名适配（lisuan vs gf）
  - Steam 客户端 CEF 崩溃（GBM_BACKEND=dri 环境变量方案）
  - Dota Underlords 原生直跑补丁
  - Electron 应用（ZCode/CodeBuddy/Qoder）gf 硬件加速
 支持桌面窗口（Tkinter，需 python3-tk）、--detect 体检、
 --json 机器可读输出、按菜单编号非交互直执行。
 。
 Steam CEF 修复不修改任何 Steam 自身文件（环境变量 + 桌面启动器），
 不受 Steam 自我校验影响。
EOF

cat > "$T/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
# 刷新桌面数据库（新增 lisuan-fix.desktop）
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database /usr/share/applications >/dev/null 2>&1 || true
fi
exit 0
EOF
chmod 755 "$T/DEBIAN/postinst"

# md5sums（相对安装树）
( cd "$T" && find usr -type f -exec md5sum {} + | sort -k2 ) > "$T/DEBIAN/md5sums"

# ---- 构建 -----------------------------------------------------------------
mkdir -p "$OUT_DIR"
DEB="$OUT_DIR/${PKG}_${VER}_all.deb"
dpkg-deb --root-owner-group --build "$T" "$DEB"

echo
echo "==> 构建完成: $DEB"
# 先取全量输出再截断显示，避免 head 截断触发 SIGPIPE（pipefail 下导致假失败）
INFO="$(dpkg-deb --info "$DEB")"
echo "$INFO" | head -12
echo "..."
echo "内容 $(dpkg-deb -c "$DEB" | wc -l) 个条目, 大小 $(du -h "$DEB" | cut -f1)"
