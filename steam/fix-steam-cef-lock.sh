#!/usr/bin/env bash
#
# fix-steam-cef-lock.sh — lock the CEF software-rendering patch in place so
# Steam's checksum verification cannot revert it.
#
# Steam overwrites steamwebhelper_sniper_wrap.sh on every launch ("Verifying
# all executable checksums"), undoing a plain edit. This script re-applies the
# CEF injection AND sets the immutable attribute (chattr +i) so the
# file cannot be overwritten. ext4 only.
#
# What gets injected (verified working config, 2026-08-05):
#   1. `export GBM_BACKEND=dri` — the REAL root cause: inside the
#      pressure-vessel container, system libgbm resolves DRM name "gf" to the
#      vendor backend gbm/gf_gbm.so (22.0.4 fork), which segfaults (jump to
#      NULL) under the newer libgbm ABI. Core dumps of the crashing processes
#      show the crash frames inside gf_gbm.so. Forcing the distro "dri"
#      backend sidesteps it (see docs/system-mesa-analysis.md §5 landmine #1).
#   2. `export LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libglapi_gf.so.0` —
#      Steam bundles its own ANGLE (libEGL.so/libGLESv2.so) in ubuntu12_64/,
#      which LD_LIBRARY_PATH=. puts first. That ANGLE lazily resolves GL API
#      symbols (glVertexAttribIPointer etc.) against the global scope; inside
#      the container no glapi provider is loaded yet, so ld.so aborts the GPU
#      process with "symbol lookup error ... (fatal)" -> SIGSEGV (seen as
#      exit_code=139/11). Preloading the vendor libglapi provides the symbols.
#      (Verified via LD_DEBUG=libs traces of the crashing gpu-process.)
#   3. Plain `exec ./steamwebhelper "$@"` — with 1+2 in place the GPU process
#      no longer crashes; it tries ANGLE EGL (fails cleanly: the container's
#      glvnd EGL dispatch never reaches the vendor EGL, and system mesa EGL
#      has no usable backend for this card — vendor EGL itself works fine on
#      the host), then falls back to software rendering. The UI works.
#      Full hardware compositing for the UI needs vendor-side container/EGL
#      dispatch support, and in-container GLX for native games needs a GLX
#      vendor registration (see docs/vendor-bug-report-gf-gbm-crash.md §3/§4).
#
# Note: sed -i recreates the file, so the executable bit is restored with
# chmod +x afterwards — without it pv-adverb fails with "Permission denied".
#
# Usage:
#   sudo ./fix-steam-cef-lock.sh              # apply + lock
#   sudo ./fix-steam-cef-lock.sh --unlock     # remove the lock (before Steam update)
#   sudo ./fix-steam-cef-lock.sh --uninstall  # restore wrap.sh to original (undo injection)
#
# License: Apache-2.0
set -euo pipefail

# 改文件属性（chattr）需要 root
[[ $EUID -eq 0 ]] || { echo "需要 root:  sudo $0 $*" >&2; exit 1; }

# sudo 下 HOME 会变成 /root，须用 SUDO_USER 找回真实用户的家目录
REAL_USER="${SUDO_USER:-$USER}"
REAL_HOME="$(getent passwd "$REAL_USER" | cut -d: -f6)"
# Steam 容器化启动 CEF 前执行的包装脚本，也是注入环境变量的唯一入口
WRAP="${REAL_HOME}/.local/share/Steam/ubuntu12_64/steamwebhelper_sniper_wrap.sh"

[[ -f "$WRAP" ]] || { echo "未找到: $WRAP — Steam 是否已安装？" >&2; exit 1; }

# 无论注入还是解锁，都先去掉不可变属性，保证后续可写（幂等）
chattr -i "$WRAP" 2>/dev/null || true

# --unlock 模式：只解锁不注入，供 Steam 客户端更新前使用
if [[ "${1:-}" == "--unlock" ]]; then
  echo "==> 已解锁 $WRAP — Steam 更新可正常覆盖。更新后重新运行 sudo $0 重新注入。"
  exit 0
fi

# --uninstall 模式：完整撤销注入，把 wrap.sh 还原到原厂状态（不重新锁定）
if [[ "${1:-}" == "--uninstall" ]]; then
  if [[ -f "${WRAP}.bak" ]]; then
    # 有备份（注入时自动存的）→ 整体还原，最可靠
    cp -a "${WRAP}.bak" "$WRAP"
    rm -f "${WRAP}.bak"
    echo "==> 已从备份还原 $WRAP"
  else
    # 无备份 → 逐行删除注入的两个 export（只删我们加的行，不动其它内容）
    sed -i '/^export GBM_BACKEND=dri$/d; /^export LD_PRELOAD=\/usr\/lib\/x86_64-linux-gnu\/libglapi_gf\.so\.0$/d' "$WRAP"
    echo "==> 已移除注入的环境变量行"
  fi
  chmod +x "$WRAP"    # 还原后仍要保证可执行（备份还原一般会保留，双保险）
  # 校验：注入标记必须全部消失，且原厂 exec 行仍在
  if grep -q -- 'GBM_BACKEND=dri' "$WRAP" || grep -q -- 'libglapi_gf' "$WRAP"; then
    echo "!! 还原不彻底，请手动检查 $WRAP" >&2
    exit 1
  fi
  grep -q -- 'exec ./steamwebhelper' "$WRAP" || { echo "!! wrap.sh 结构异常，请手动检查" >&2; exit 1; }
  echo "==> 卸载完成：wrap.sh 已还原且未锁定。Steam 界面将回到修复前状态（无法启动）。"
  exit 0
fi

# 重要（2026-08-24 修正）: 向 Steam 客户端 wrap 注入 gf PRELOAD/GBM 会让
# **Steam 客户端自身** 走 gf glapi -> 无头/XWayland 下 double free 崩溃
# (见 docs/steam-on-lisuan.md §2/§3)。故默认 **不再注入 Steam 客户端 wrap**。
# 此脚本仅保留清理能力 (--unlock / --uninstall) 以移除历史注入。
# 若确实需要给游戏进程树注入 gf GL, 请改用游戏 Launch Options 或 Proton
# user_settings.py (仅游戏), 不要碰 Steam 客户端 wrap。

# 注入前检查：两个标记都在说明已注入过，跳过（幂等）
MARK_OK=1
grep -q -- 'GBM_BACKEND=dri' "$WRAP" || MARK_OK=0
grep -q -- 'libglapi_gf' "$WRAP" || MARK_OK=0
if [[ "$MARK_OK" == "0" ]]; then
  # 未注入：默认什么都不做（不再向 Steam 客户端 wrap 注入 gf，避免其崩溃）
  echo "==> 未检测到 gf 注入, 且默认不再向 Steam 客户端 wrap 注入 (会导致 Steam 客户端 double free)。"
  echo "==> 如需为游戏进程树注入 gf GL, 请用游戏 Launch Options / Proton user_settings.py, 不要改此 wrap。"
  chmod +x "$WRAP"
  exit 0
fi

# 已注入（历史残留）：提示用户用 --uninstall 清理，不要继续锁定
echo "!! 检测到 wrap 仍含 gf 注入 (GBM_BACKEND=dri / libglapi_gf)，这会让 Steam 客户端崩溃。"
echo "!! 请运行:  sudo $0 --uninstall   来移除（移除后 Steam 客户端恢复稳定）。"
chmod +x "$WRAP"
exit 1

# 以下旧逻辑已废弃（保留注释，勿启用）：
#   sed -i 's|^exec \./steamwebhelper "\$@"$|export GBM_BACKEND=dri\nexport LD_PRELOAD=...|' "$WRAP"
#   chattr +i "$WRAP"
cat <<EOF

注意: 本脚本默认不再向 Steam 客户端 wrap 注入 gf 环境变量 (该注入会导致 Steam 客户端
double free 崩溃, 见 docs/steam-on-lisuan.md §2)。仅用于清理历史注入:
  sudo $0 --uninstall   移除 wrap 里的 gf 注入 (Steam 客户端恢复稳定)
  sudo $0 --unlock      仅解锁 (供 Steam 更新前)

游戏需要 gf GL 时, 请改用游戏 Launch Options / Proton user_settings.py 注入, 不要碰
Steam 客户端 wrap。
EOF
