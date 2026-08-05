#!/usr/bin/env bash
#
# fix-steam-pv-ldconfig.sh — make Steam's pressure-vessel container able to
# load the vendor (gf) GL driver, so CEF's GPU process stops crashing with
# exit_code=139 ("failed to load driver: gf").
#
# STATUS (2026-08-05): superseded as the primary fix. Core-dump analysis
# showed the in-container CEF crash is actually inside the vendor GBM backend
# (gf_gbm.so segfault under newer libgbm), and even with GL libs resolvable
# the container GPU process still crashes in the vendor GL/Vulkan stack.
# The verified working fix is fix-steam-cef-lock.sh (GBM_BACKEND=dri +
# software-rendering flags). Modern pressure-vessel also already passes
# --add-ld.so-path for the overrides dir, weakening this script's premise.
# This script is kept for reference; its injection is dormant while
# --disable-gpu is in effect.
#
# Root cause this fixes:
#   pressure-vessel copies the vendor GL libs (gf_dri.so, libglapi_gf.so.0,
#   libEGL_gf.so.0, libgbm_gf.so.1, libGLX_gf.so.0, ...) into
#   /usr/lib/pressure-vessel/overrides/lib/x86_64-linux-gnu/ INSIDE the
#   container (as symlinks to /run/host/usr/lib/...). But the container's
#   ld.so does NOT search that directory — it is not in any ld.so.conf and
#   pressure-vessel does not add it. So when Mesa dlopens gf_dri.so it
#   cannot resolve libglapi_gf.so.0 -> "failed to load driver: gf" ->
#   CEF GPU process segfaults (exit_code=139, "GPU process isn't usable.
#   Goodbye").
#
#   A PREVIOUS version of this script wrote a snippet into the HOST's
#   /etc/ld.so.conf.d/zz-lisuan-pv-overrides.conf. That does NOT work:
#   pressure-vessel does NOT copy the host's ld.so.conf.d into the container
#   (the container's /etc comes from the sniper runtime image, which only
#   ships i386-linux-gnu.conf / libc.conf / x86_64-linux-gnu.conf). This
#   version injects LD_LIBRARY_PATH into steamwebhelper_sniper_wrap.sh
#   instead — that script runs INSIDE the container, and its environment is
#   inherited by steamwebhelper and every CEF child process (including the
#   GPU process), so ld.so finally searches the overrides directory.
#
# What it does (idempotent):
#   1. Injects an `export LD_LIBRARY_PATH=<overrides64>:<overrides32>:...`
#      line into steamwebhelper_sniper_wrap.sh, right after the existing
#      search-cwd LD_LIBRARY_PATH line.
#   2. Detects and reports the now-useless host /etc/ld.so.conf.d snippet
#      left by the old version (removing it needs root — see message).
#
# IMPORTANT — ordering with fix-steam-cef-lock.sh:
#   Run THIS script FIRST, then fix-steam-cef-lock.sh. The lock script
#   injects --disable-gpu and then chattr +i the file; once locked this
#   script can no longer edit it. To re-edit, unlock first:
#       sudo ./fix-steam-cef-lock.sh --unlock
#
#   Note: while --disable-gpu is in effect, CEF uses software rendering and
#   does not touch GL, so this LD_LIBRARY_PATH is dormant. It becomes
#   relevant the moment you remove --disable-gpu (e.g. to try CEF hardware
#   acceleration): with it, gf_dri.so can finally find libglapi_gf.so.0
#   inside the container.
#
# Usage:
#   ./fix-steam-pv-ldconfig.sh             # inject LD_LIBRARY_PATH
#   ./fix-steam-pv-ldconfig.sh --revert    # remove the injection
#
# License: Apache-2.0
set -euo pipefail

WRAP="$HOME/.local/share/Steam/ubuntu12_64/steamwebhelper_sniper_wrap.sh"
MARKER='lisuan-pv-overrides'
# Container-internal paths where pressure-vessel places vendor GL libs.
PV64=/usr/lib/pressure-vessel/overrides/lib/x86_64-linux-gnu
PV32=/usr/lib/pressure-vessel/overrides/lib/i386-linux-gnu
# The exact search-cwd line Steam ships (we inject right after it).
SEARCH_CWD_LINE='export LD_LIBRARY_PATH=.${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}'

[[ -f "$WRAP" ]] || { echo "Not found: $WRAP (is Steam installed?)" >&2; exit 1; }

# Refuse to edit if the file is locked immutable by fix-steam-cef-lock.sh.
if lsattr "$WRAP" 2>/dev/null | awk '{print $1}' | grep -q 'i'; then
  echo "!! $WRAP 已被锁定 (chattr +i)。" >&2
  echo "!! 请先解锁:  sudo \"$(cd "$(dirname "$0")" && pwd)/fix-steam-cef-lock.sh\" --unlock" >&2
  exit 1
fi

# --- detect & report leftover host snippet from the old (non-working) version ---
OLD_CONF=/etc/ld.so.conf.d/zz-lisuan-pv-overrides.conf
if [[ -f "$OLD_CONF" ]]; then
  echo "==> 注意: 旧版本写入的宿主片段仍存在: $OLD_CONF" >&2
  echo "    pressure-vessel 不会把宿主 /etc/ld.so.conf.d 复制进容器, 该片段已无效。" >&2
  echo "    删除(需 root):  sudo rm $OLD_CONF && sudo ldconfig" >&2
  echo
fi

if [[ "${1:-}" == "--revert" ]]; then
  if grep -qF -- "$MARKER" "$WRAP"; then
    cp -a "$WRAP" "$WRAP.bak"
    # delete the marker comment line together with the line that follows it
    sed -i "/# $MARKER:/{N;d;}" "$WRAP"
    echo "==> 已移除 LD_LIBRARY_PATH 注入 (backup: $WRAP.bak)"
  else
    echo "==> 未发现注入, 无需操作"
  fi
  exit 0
fi

# --- inject LD_LIBRARY_PATH (idempotent) ---
if grep -qF -- "$MARKER" "$WRAP"; then
  echo "==> 已注入 LD_LIBRARY_PATH (幂等, 跳过)"
else
  # sanity: the search-cwd line must be present so we know where to insert
  if ! grep -qF -- "$SEARCH_CWD_LINE" "$WRAP"; then
    echo "!! 未找到预期的 search-cwd 行:" >&2
    echo "!!   $SEARCH_CWD_LINE" >&2
    echo "!! wrap.sh 格式可能已变 (Steam 更新?), 请手动检查 $WRAP" >&2
    echo "!! 需在该行之后插入:" >&2
    echo "!!   export LD_LIBRARY_PATH=$PV64:$PV32\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}" >&2
    exit 1
  fi

  cp -a "$WRAP" "$WRAP.bak"

  # Build the two literal lines to insert (LD_LIBRARY_PATH must stay literal,
  # so the ${...} part is in single quotes; only the paths are interpolated).
  COMMENT_LINE="# $MARKER: container path to vendor GL libs (libglapi_gf.so.0 etc.) so gf_dri.so can load"
  INSERT_LINE='export LD_LIBRARY_PATH='"$PV64"':'"$PV32"'${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}'

  # Insert the two lines right after the search-cwd line, using awk with a
  # plain whole-line string comparison (no regex, no sed r quirks).
  awk -v cmt="$COMMENT_LINE" -v ins="$INSERT_LINE" -v sc="$SEARCH_CWD_LINE" '
    {print}
    $0 == sc { print cmt; print ins }
  ' "$WRAP" > "$WRAP.tmp" && mv "$WRAP.tmp" "$WRAP"

  if ! grep -qF -- "$MARKER" "$WRAP"; then
    echo "!! 注入失败, 请手动检查 $WRAP (backup 已存为 $WRAP.bak)" >&2
    exit 1
  fi
  echo "==> 已注入 LD_LIBRARY_PATH (backup: $WRAP.bak)"
fi

echo
echo "==> wrap.sh 现状:"
grep -nE 'LD_LIBRARY_PATH|exec \./steamwebhelper' "$WRAP"
echo
cat <<EOF
完成。建议顺序:
  1. (保底, 让 CEF 软渲染不碰容器内 GL) 注入 --disable-gpu 并锁定:
       sudo "$(cd "$(dirname "$0")" && pwd)/fix-steam-cef-lock.sh"
  2. 重启 Steam:
       MESA_LOADER_DRIVER_OVERRIDE=gf steam -no-cef-sandbox -cef-force-64bit

验证 CEF 是否还崩:
  grep -c 'exit_code=139' ~/.local/share/Steam/logs/cef_log.txt

撤销本注入:
  "$0" --revert
EOF
