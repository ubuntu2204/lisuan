#!/usr/bin/env bash
#
# test-repo-health.sh — static checks on the repository layout that must hold
# before publishing to Gitee: required files exist, no vendor artifacts are
# accidentally tracked, scripts are executable, README references resolve.
# License: Apache-2.0
set -uo pipefail
# shellcheck source=lib.sh
. "$(dirname "${BASH_SOURCE[0]}")/lib.sh"

# --- 1. Required top-level files present -----------------------------------
# LICENSE / NOTICE / .gitignore / README.md all live at the repo root
# (REPO_ROOT here is the deliverables subtree, not a standalone repo).
REPO_TOP="${REPO_ROOT}"
for f in README.md; do
  [[ -f "${REPO_TOP}/$f" ]]
  t_assert_eq 0 $? "required file present: $f"
done

# --- 2. Vendor artifacts must NOT be tracked (open-source hygiene) ----------
# 直接用 git ls-files 检查“是否被跟踪”，比扫磁盘更精确：本地留存但未跟踪
# 的厂商文件（.gitignore 已排除）不算违规。
for pat in '*.deb' '*.ko' '*.ko.zst' '*.o' '*.o.hex' 'lisuan_proprietary*'; do
  if git -C "$REPO_TOP" ls-files -z -- "$pat" 2>/dev/null | grep -qz .; then
    t_fail "vendor artifact must not be tracked: $pat"
  else
    t_ok "no vendor artifact tracked: $pat"
  fi
done

# --- 3. Scripts are executable ---------------------------------------------
# 交付子树与仓库根的所有 .sh 都必须带可执行位（否则用户直接 ./xxx.sh 会失败）
for s in "${REPO_TOP}"/*.sh; do
  [[ -f "$s" ]] || continue
  [[ -x "$s" ]]
  t_assert_eq 0 $? "script is executable: $(basename "$s")"
done
while IFS= read -r -d '' s; do
  [[ -x "$s" ]]
  t_assert_eq 0 $? "script is executable: ${s#"$REPO_ROOT"/}"
done < <(find "$REPO_ROOT" -mindepth 2 -maxdepth 3 -type f -name '*.sh' \
           ! -path '*/_work/*' ! -path '*/.git/*' ! -path '*/packaging/dist/*' -print0)

# --- 4. Patch file is non-empty and mentions the root cause -----------------
[[ -s "$PATCH_FILE" ]]
t_assert_eq 0 $? "patch file non-empty"
t_assert_grep "$PATCH_FILE" 'KERNELVER' "patch mentions KERNELVER"
t_assert_grep "$PATCH_FILE" 'uname -r' "patch mentions uname -r"

# --- 5. README references that exist in this repo ---------------------------
for f in dkms/patches/0001-dkms-makefile-honor-KERNELVER.patch \
         dkms/fix-lisuan-gpu.sh \
         dkms/verify-lisuan-gpu.sh \
         audio/fix-lisuan-hdmi-audio.sh; do
  grep -qF -- "$f" "${REPO_TOP}/README.md"
  t_assert_eq 0 $? "README references $f"
done

# --- 6. Required top-level docs present --------------------------------------
for f in CHANGELOG.md NOTICE LICENSE; do
  [[ -f "${REPO_TOP}/$f" ]]
  t_assert_eq 0 $? "top-level doc present: $f"
done

# --- 7. L4D2 current resources, archive paths and menu registration -----------
for f in left4dead2/fix-l4d2.sh \
         left4dead2/setlocale-shim.c \
         left4dead2/README.md \
         left4dead2/探索记录.md \
         left4dead2/archive/README.md \
         left4dead2/archive/vkcreateimage-shim.c \
         left4dead2/vtest-forward/build-venus-bridge.sh \
         left4dead2/vtest-forward/run-l4d2-venus.sh \
         left4dead2/vtest-forward/patches/0001-venus-prefer-ext-calibrated-timestamps.patch \
         left4dead2/vtest-forward/patches/0002-venus-hide-broken-feedback-loop-layout.patch; do
  [[ -f "${REPO_TOP}/$f" ]]
  t_assert_eq 0 $? "L4D2 resource present: $f"
done
t_assert_grep "${REPO_TOP}/left4dead2/fix-l4d2.sh" \
  'archive/vkcreateimage-shim.c' "L4D2 optional shim uses archive path"
t_assert_grep "${REPO_TOP}/packaging/build-deb.sh" \
  'find left4dead2' "deb includes L4D2 resources"
python3 - "$REPO_TOP" <<'PY'
import sys
from pathlib import Path
root = Path(sys.argv[1])
sys.path.insert(0, str(root / "packaging"))
import lisuan_fix_core as core
assert len(core.ITEMS_BY_ID) == 25
assert core.GROUP_ORDER[-1] == "求生之路 2"
assert core.ITEMS_BY_ID[22]["root"] is False
assert core.ITEMS_BY_ID[22]["cmd"]([])[1].endswith("left4dead2/fix-l4d2.sh")
assert core.ITEMS_BY_ID[23]["root"] is False
assert core.ITEMS_BY_ID[23]["doc"] == "left4dead2/README.md"
assert core.UNDO[22]["root"] is False
assert "--uninstall" in core.UNDO[22]["cmd"]
# 植物大战僵尸 GOTY（32 位 Windows 游戏，new WoW64 + 7G100 硬件渲染）：安装/说明 + 卸载
assert core.ITEMS_BY_ID[24]["root"] is False
assert core.ITEMS_BY_ID[24]["cmd"]([])[1].endswith("Plants-vs-Zombies/fix-plants-vs-zombies.sh")
assert core.ITEMS_BY_ID[25]["doc"] == "Plants-vs-Zombies/README.md"
assert core.UNDO[24]["root"] is False
assert "--uninstall" in core.UNDO[24]["cmd"]
PY
t_assert_eq 0 $? "L4D2 menu 22/23 and undo registration"

t_finish "test-repo-health.sh"
