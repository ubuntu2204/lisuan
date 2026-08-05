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
REPO_TOP="$(cd "${REPO_ROOT}/.." && pwd)"
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
for s in "${REPO_ROOT}"/scripts/*.sh "${REPO_TOP}"/*.sh; do
  [[ -f "$s" ]] || continue
  [[ -x "$s" ]]
  t_assert_eq 0 $? "script is executable: $(basename "$s")"
done

# --- 4. Patch file is non-empty and mentions the root cause -----------------
[[ -s "$PATCH_FILE" ]]
t_assert_eq 0 $? "patch file non-empty"
t_assert_grep "$PATCH_FILE" 'KERNELVER' "patch mentions KERNELVER"
t_assert_grep "$PATCH_FILE" 'uname -r' "patch mentions uname -r"

# --- 5. README references that exist in this repo ---------------------------
for f in patches/0001-dkms-makefile-honor-KERNELVER.patch \
         scripts/fix-lisuan-gpu.sh \
         scripts/verify-lisuan-gpu.sh; do
  grep -qF -- "$f" "${REPO_TOP}/README.md"
  t_assert_eq 0 $? "README references $f"
done

# --- 6. Required top-level docs present --------------------------------------
for f in CHANGELOG.md NOTICE LICENSE; do
  [[ -f "${REPO_TOP}/$f" ]]
  t_assert_eq 0 $? "top-level doc present: $f"
done

t_finish "test-repo-health.sh"
