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
for f in README.md LICENSE NOTICE .gitignore; do
  [[ -f "${REPO_ROOT}/$f" ]]
  t_assert_eq 0 $? "required file present: $f"
done

# --- 2. Vendor artifacts must NOT be in the repo (open-source hygiene) ------
# These names are also listed in .gitignore; the test guards accidental commits.
for name in '*.deb' '*.ko' '*.o.hex' 'lisuan_proprietary' 'gf-*.ko'; do
  if find "${REPO_ROOT}" -path '*/tests/*' -prune -o -name "$name" -print 2>/dev/null | grep -q .; then
    t_fail "vendor artifact must not be tracked: $name"
  else
    t_ok "no vendor artifact present: $name"
  fi
done

# --- 3. Scripts are executable ---------------------------------------------
for s in scripts/fix-lisuan-gpu.sh scripts/verify-lisuan-gpu.sh; do
  [[ -x "${REPO_ROOT}/$s" ]]
  t_assert_eq 0 $? "script is executable: $s"
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
  grep -qF -- "$f" "${REPO_ROOT}/README.md"
  t_assert_eq 0 $? "README references $f"
done

t_finish "test-repo-health.sh"
