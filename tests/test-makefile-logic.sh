#!/usr/bin/env bash
#
# test-makefile-logic.sh — verify the FIXED Makefile honors KERNELVER (the root
# cause of the bug), while the buggy one ignores it. Uses real `make` to expand
# the KERNEL_VERSION selection line against the fixtures.
# License: Apache-2.0
set -uo pipefail
# shellcheck source=lib.sh
. "$(dirname "${BASH_SOURCE[0]}")/lib.sh"

command -v make >/dev/null 2>&1 || { t_skip "make not installed — logic tests skipped"; t_finish "test-makefile-logic.sh"; }

RUN_KVER() { # $1=Makefile  $2=extra make args...
  local mk="$1"; shift
  make --no-print-directory -f "$mk" print-kver "$@" 2>/dev/null
}

TARGET_KVER="7.0.0-28-generic"
NATIVE_KVER="$(uname -r)"

# --- 1. Buggy Makefile IGNORES KERNELVER (bug reproduced) -----------------
BUGGY_OUT="$(RUN_KVER "${FIXTURES_DIR}/Makefile.buggy" "KERNELVER=${TARGET_KVER}")"
t_assert_eq "$NATIVE_KVER" "$BUGGY_OUT" \
  "buggy Makefile ignores KERNELVER (resolves to running kernel)"

# --- 2. Fixed Makefile HONORS KERNELVER (the actual fix) -------------------
FIXED_OUT="$(RUN_KVER "${FIXTURES_DIR}/Makefile.fixed" "KERNELVER=${TARGET_KVER}")"
t_assert_eq "$TARGET_KVER" "$FIXED_OUT" \
  "fixed Makefile honors KERNELVER (builds for target kernel)"

# --- 3. Fixed Makefile falls back to uname -r for plain manual `make` ------
FALLBACK_OUT="$(RUN_KVER "${FIXTURES_DIR}/Makefile.fixed")"
t_assert_eq "$NATIVE_KVER" "$FALLBACK_OUT" \
  "fixed Makefile falls back to uname -r without KERNELVER (manual build)"

# --- 4. Patching the buggy fixture flips behavior (end-to-end) -------------
WORKDIR="${TEST_TMP}/logic"
mkdir -p "$WORKDIR"
cp "${FIXTURES_DIR}/Makefile.buggy" "$WORKDIR/Makefile"
(
  cd "$WORKDIR" || exit 1
  patch -p1 -s < "$PATCH_FILE" >/dev/null 2>&1
)
PATCHED_OUT="$(RUN_KVER "$WORKDIR/Makefile" "KERNELVER=${TARGET_KVER}")"
t_assert_eq "$TARGET_KVER" "$PATCHED_OUT" \
  "after patch, previously buggy Makefile now honors KERNELVER"

t_finish "test-makefile-logic.sh"
