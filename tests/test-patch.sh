#!/usr/bin/env bash
#
# test-patch.sh — verify that patches/0001-dkms-makefile-honor-KERNELVER.patch
# applies cleanly to the buggy Makefile and produces exactly the fixed one.
# License: Apache-2.0
set -uo pipefail
# shellcheck source=lib.sh
. "$(dirname "${BASH_SOURCE[0]}")/lib.sh"

WORKDIR="${TEST_TMP}/apply"
mkdir -p "$WORKDIR"

# --- 1. patch(1) application (the documented manual method) ----------------
cp "${FIXTURES_DIR}/Makefile.buggy" "$WORKDIR/Makefile"
(
  cd "$WORKDIR" || exit 1
  patch -p1 -s < "$PATCH_FILE" >/dev/null 2>&1
)
t_assert_eq 0 $? "patch -p1 applies without error"
t_assert_grep "$WORKDIR/Makefile" \
  'KERNEL_VERSION := \$\(if \$\(KERNELVER\),\$\(KERNELVER\),\$\(shell uname -r\)\)' \
  "patched Makefile honors KERNELVER"
diff -u "${FIXTURES_DIR}/Makefile.fixed" "$WORKDIR/Makefile" >/dev/null 2>&1
t_assert_eq 0 $? "patched result is byte-identical to Makefile.fixed"

# --- 2. patch(1) reverse (uninstall path documented in README) ------------
(
  cd "$WORKDIR" || exit 1
  patch -p1 -R -s < "$PATCH_FILE" >/dev/null 2>&1
)
t_assert_eq 0 $? "patch -p1 -R reverses cleanly"
diff -u "${FIXTURES_DIR}/Makefile.buggy" "$WORKDIR/Makefile" >/dev/null 2>&1
t_assert_eq 0 $? "reversed result matches Makefile.buggy"

# --- 3. git apply (if git is available) -----------------------------------
if command -v git >/dev/null 2>&1; then
  mkdir -p "$WORKDIR/git"
  cp "${FIXTURES_DIR}/Makefile.buggy" "$WORKDIR/git/Makefile"
  (
    cd "$WORKDIR/git" || exit 1
    git apply --check "$PATCH_FILE" 2>/dev/null && git apply "$PATCH_FILE" 2>/dev/null
  )
  t_assert_eq 0 $? "git apply works"
  diff -u "${FIXTURES_DIR}/Makefile.fixed" "$WORKDIR/git/Makefile" >/dev/null 2>&1
  t_assert_eq 0 $? "git apply result matches Makefile.fixed"
else
  t_skip "git not installed — git apply test skipped"
fi

t_finish "test-patch.sh"
