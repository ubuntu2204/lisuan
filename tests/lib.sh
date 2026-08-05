#!/usr/bin/env bash
#
# lib.sh — tiny shared test helpers for the tests/ suite.
# License: Apache-2.0
#
# Usage: source this file at the top of every tests/test-*.sh.
set -uo pipefail

# Directory layout ---------------------------------------------------------
# tests/ lives at the repo root; the deliverables under test (patches,
# scripts, docs, README) live in the lisuan-7g100-ubuntu-fix/ subtree.
TESTS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${TESTS_DIR}/../lisuan-7g100-ubuntu-fix" && pwd)"
FIXTURES_DIR="${TESTS_DIR}/fixtures"
PATCH_FILE="${REPO_ROOT}/patches/0001-dkms-makefile-honor-KERNELVER.patch"

# Per-test temp dir (cleaned up by run-tests.sh, or on exit).
TEST_TMP="$(mktemp -d "${TMPDIR:-/tmp}/lisuan-test.XXXXXX")"
trap 'rm -rf "$TEST_TMP"' EXIT

# Counters -----------------------------------------------------------------
TESTS_TOTAL=0
TESTS_FAILED=0

# Helpers ------------------------------------------------------------------
t_ok() {
  TESTS_TOTAL=$((TESTS_TOTAL + 1))
  printf '  \033[1;32m[PASS]\033[0m %s\n' "$*"
}

t_fail() {
  TESTS_TOTAL=$((TESTS_TOTAL + 1))
  TESTS_FAILED=$((TESTS_FAILED + 1))
  printf '  \033[1;31m[FAIL]\033[0m %s\n' "$*" >&2
}

t_skip() {
  printf '  \033[1;33m[SKIP]\033[0m %s\n' "$*"
}

# Assert that $1 equals $2 (strings compared literally).
t_assert_eq() {
  local expected="$1" actual="$2" label="${3:-eq}"
  if [[ "$actual" == "$expected" ]]; then
    t_ok "$label"
  else
    t_fail "$label (expected: <$expected>; got: <$actual>)"
  fi
}

# Assert that the file $1 contains the pattern $2.
t_assert_grep() {
  local file="$1" pattern="$2" label="${3:-grep}"
  if grep -qE -- "$pattern" "$file"; then
    t_ok "$label"
  else
    t_fail "$label (pattern /$pattern/ not found in $file)"
  fi
}

# Assert that the file $1 does NOT contain the pattern $2.
t_assert_not_grep() {
  local file="$1" pattern="$2" label="${3:-not-grep}"
  if grep -qE -- "$pattern" "$file"; then
    t_fail "$label (pattern /$pattern/ unexpectedly found in $file)"
  else
    t_ok "$label"
  fi
}

# Report per-file summary; exit non-zero if anything failed.
t_finish() {
  local name="${1:-$(basename "$0")}"
  echo
  if [[ $TESTS_FAILED -eq 0 ]]; then
    printf '\033[1;32m%s: %d/%d passed\033[0m\n' "$name" "$TESTS_TOTAL" "$TESTS_TOTAL"
    exit 0
  else
    printf '\033[1;31m%s: %d/%d passed (%d failed)\033[0m\n' \
      "$name" "$((TESTS_TOTAL - TESTS_FAILED))" "$TESTS_TOTAL" "$TESTS_FAILED"
    exit 1
  fi
}
