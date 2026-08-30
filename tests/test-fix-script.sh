#!/usr/bin/env bash
#
# test-fix-script.sh — verify the core patch logic embedded in
# display/fix-lisuan-gpu.sh (the perl one-liner + "already patched" detection),
# including idempotency, without needing root, dkms or the vendor driver.
# License: Apache-2.0
set -uo pipefail
# shellcheck source=lib.sh
. "$(dirname "${BASH_SOURCE[0]}")/lib.sh"

# The exact perl one-liner used by display/fix-lisuan-gpu.sh.
PATCH_PERL='s/^KERNEL_VERSION := \$\(shell uname -r\)$/KERNEL_VERSION := \$(if \$(KERNELVER),\$(KERNELVER),\$(shell uname -r))/m'
ALREADY_GREP='KERNEL_VERSION := $(if $(KERNELVER)'

WORKDIR="${TEST_TMP}/fix"
mkdir -p "$WORKDIR"

# --- 1. Perl one-liner transforms buggy Makefile into the fixed one --------
cp "${FIXTURES_DIR}/Makefile.buggy" "$WORKDIR/Makefile"
perl -0pi -e "$PATCH_PERL" "$WORKDIR/Makefile"
diff -u "${FIXTURES_DIR}/Makefile.fixed" "$WORKDIR/Makefile" >/dev/null 2>&1
t_assert_eq 0 $? "perl one-liner produces Makefile.fixed"

# --- 2. "already patched" detection used by the script ----------------------
grep -q "$ALREADY_GREP" "$WORKDIR/Makefile"
t_assert_eq 0 $? "patched Makefile triggers 'already patched' detection"

# --- 3. Idempotency: running the one-liner again changes nothing -----------
cp -a "$WORKDIR/Makefile" "$WORKDIR/Makefile.before"
perl -0pi -e "$PATCH_PERL" "$WORKDIR/Makefile"
diff -u "$WORKDIR/Makefile.before" "$WORKDIR/Makefile" >/dev/null 2>&1
t_assert_eq 0 $? "second run is a no-op (idempotent)"

# --- 4. Unpatched Makefile does NOT match the detection grep ---------------
grep -q "$ALREADY_GREP" "${FIXTURES_DIR}/Makefile.buggy"
t_assert_eq 1 $? "buggy Makefile does not trigger 'already patched' detection"

t_finish "test-fix-script.sh"
