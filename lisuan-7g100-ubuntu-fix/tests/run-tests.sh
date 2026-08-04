#!/usr/bin/env bash
#
# run-tests.sh — run the whole tests/ suite for lisuan-7g100-ubuntu-fix.
#
# Usage:
#   ./tests/run-tests.sh            # run all tests
#   ./tests/run-tests.sh -v         # verbose (show each test script's output)
#   ./tests/run-tests.sh -l         # list available tests
#   ./tests/run-tests.sh test-patch # run only matching tests (substring)
#
# Exit code: 0 if all passed, 1 if any failed.
# License: Apache-2.0
set -uo pipefail

TESTS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$TESTS_DIR" || exit 1

VERBOSE=0
FILTER=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    -v) VERBOSE=1; shift ;;
    -l|--list) LIST=1; shift ;;
    -h|--help)
      grep '^#' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    -*) echo "unknown option: $1" >&2; exit 2 ;;
    *) FILTER="$1"; shift ;;
  esac
done

TESTS=( "$TESTS_DIR"/test-*.sh )
[[ ${#TESTS[@]} -gt 0 && -e "${TESTS[0]}" ]] || { echo "no tests found under $TESTS_DIR" >&2; exit 1; }

if [[ "${LIST:-0}" == "1" ]]; then
  for t in "${TESTS[@]}"; do printf '  %s\n' "$(basename "$t")"; done
  exit 0
fi

PASS=0; FAIL=0; SKIP=0
RESULTS=()

for t in "${TESTS[@]}"; do
  name="$(basename "$t")"
  [[ -z "$FILTER" || "$name" == *"$FILTER"* ]] || continue

  out="$(bash "$t" 2>&1)"; rc=$?
  if [[ $rc -eq 0 ]]; then
    PASS=$((PASS+1))
  else
    FAIL=$((FAIL+1))
  fi
  RESULTS+=("$name|$rc|$out")
done

echo "==================== lisuan test suite ===================="
for entry in "${RESULTS[@]}"; do
  name="${entry%%|*}"
  rest="${entry#*|}"
  rc="${rest%%|*}"
  out="${rest#*|}"
  if [[ $rc -eq 0 ]]; then
    printf '  \033[1;32mPASS\033[0m  %s\n' "$name"
  else
    printf '  \033[1;31mFAIL\033[0m  %s (exit %d)\n' "$name" "$rc"
  fi
  if [[ $VERBOSE -eq 1 || $rc -ne 0 ]]; then
    printf '%s\n' "$out" | sed 's/^/       /'
  fi
done
echo "==========================================================="
printf '  summary: \033[1;32m%d passed\033[0m, \033[1;31m%d failed\033[0m\n' "$PASS" "$FAIL"
[[ $FAIL -eq 0 ]] || exit 1
exit 0
