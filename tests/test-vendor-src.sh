#!/usr/bin/env bash
#
# test-vendor-src.sh — checks on the verbatim vendor DKMS source transcription
# (lisuan-7g100-ubuntu-fix/vendor-src/):
#   * provenance docs present (README.md + vendor copyright)
#   * NO closed-source artifacts leaked in (*.o, *.ko, *.o.hex,
#     lisuan_proprietary*, build logs)
#   * Makefile is the ORIGINAL (unpatched) vendor version
#   * patches/0001 applies cleanly to it (patch -p1 --dry-run)
#
# License: Apache-2.0
set -uo pipefail
# shellcheck source=lib.sh
. "$(dirname "${BASH_SOURCE[0]}")/lib.sh"

VS="${REPO_ROOT}/vendor-src"

# --- 1. transcription present with provenance --------------------------------
[[ -d "$VS" ]]
t_assert_eq 0 $? "vendor-src directory present"
[[ -f "$VS/README.md" ]]
t_assert_eq 0 $? "vendor-src/README.md (provenance) present"
[[ -f "$VS/copyright" ]]
t_assert_eq 0 $? "vendor-src/copyright (vendor license) present"

# --- 2. source file counts ---------------------------------------------------
NC=$(find "$VS" -name '*.c' | wc -l)
NH=$(find "$VS" -name '*.h' | wc -l)
t_assert_eq 69 "$NC" "vendor .c file count (69)"
t_assert_eq 99 "$NH" "vendor .h file count (99)"

# --- 3. no closed-source / binary artifacts ----------------------------------
LEAK=$(find "$VS" \( -name '*.o' -o -name '*.ko' -o -name '*.ko.zst' \
        -o -name '*.o.hex' -o -name 'lisuan_proprietary*' \
        -o -name '*.obj.log' -o -name '*.bin' -o -name '*.a' \) 2>/dev/null | wc -l)
t_assert_eq 0 "$LEAK" "no closed-source/binary artifacts in vendor-src"

# --- 4. Makefile is the unpatched original -----------------------------------
grep -q '^KERNEL_VERSION := \$(shell uname -r)$' "$VS/Makefile"
t_assert_eq 0 $? "vendor Makefile is ORIGINAL (unpatched)"

# --- 5. the root-cause patch applies cleanly ---------------------------------
( cd "$VS" && patch -p1 --dry-run -s < "$PATCH_FILE" )
t_assert_eq 0 $? "patches/0001 applies cleanly to vendor-src (dry-run)"

t_finish "test-vendor-src.sh"
