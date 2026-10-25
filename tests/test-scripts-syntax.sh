#!/usr/bin/env bash
#
# test-scripts-syntax.sh — bash -n syntax check on every shell script shipped
# in the per-theme dirs (and the test suite itself). No execution, purely static.
# License: Apache-2.0
set -uo pipefail
# shellcheck source=lib.sh
. "$(dirname "${BASH_SOURCE[0]}")/lib.sh"

SCRIPT_DIR="${REPO_ROOT}"
for f in "${SCRIPT_DIR}"/*.sh; do
  [[ -f "$f" ]] || continue
  bash -n "$f" 2>/dev/null
  t_assert_eq 0 $? "bash -n $(basename "$f")"
done
# 覆盖主题子目录及三级归档/构建目录，避免移动后静默失去检查。
while IFS= read -r -d '' f; do
  bash -n "$f" 2>/dev/null
  t_assert_eq 0 $? "bash -n ${f#"$SCRIPT_DIR"/}"
done < <(find "$SCRIPT_DIR" -mindepth 2 -maxdepth 3 -type f -name '*.sh' \
           ! -path '*/_work/*' ! -path '*/.git/*' ! -path '*/packaging/dist/*' -print0)

# The test suite files themselves must also be valid bash.
for f in "${TESTS_DIR}"/test-*.sh "${TESTS_DIR}"/lib.sh "${TESTS_DIR}"/run-tests.sh; do
  [[ -f "$f" ]] || continue
  bash -n "$f" 2>/dev/null
  t_assert_eq 0 $? "bash -n tests/$(basename "$f")"
done

# Repo-root entry scripts (e.g. the interactive fix index).
for f in "${TESTS_DIR}"/../*.sh; do
  [[ -f "$f" ]] || continue
  bash -n "$f" 2>/dev/null
  t_assert_eq 0 $? "bash -n $(basename "$f")"
done

t_finish "test-scripts-syntax.sh"
