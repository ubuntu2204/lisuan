#!/usr/bin/env bash
#
# test-shellcheck.sh — run shellcheck (if installed) over every shipped shell
# script. shellcheck catches quoting bugs, useless cats, unquoted expansions
# etc. that `bash -n` cannot see.
#
# 未安装 shellcheck 时整体跳过（t_skip），不阻塞 CI；本地建议安装：
#   sudo apt-get install shellcheck
#
# License: Apache-2.0
set -uo pipefail
# shellcheck source=lib.sh
. "$(dirname "${BASH_SOURCE[0]}")/lib.sh"

if ! command -v shellcheck >/dev/null 2>&1; then
  t_skip "shellcheck not installed — static lint skipped (apt-get install shellcheck)"
  t_finish "test-shellcheck.sh"
fi

# 收集全部脚本：各主题目录(steam/ display/ ...)、测试套件自身、仓库根入口脚本
SCRIPTS=()
while IFS= read -r -d '' f; do
  SCRIPTS+=("$f")
done < <(find "$REPO_ROOT" -mindepth 2 -maxdepth 3 -type f -name '*.sh' \
           ! -path '*/_work/*' ! -path '*/.git/*' ! -path '*/packaging/dist/*' -print0)
for f in "${TESTS_DIR}"/test-*.sh "${TESTS_DIR}"/lib.sh "${TESTS_DIR}"/run-tests.sh; do
  [[ -f "$f" ]] && SCRIPTS+=("$f")
done
for f in "${REPO_ROOT}"/*.sh; do [[ -f "$f" ]] && SCRIPTS+=("$f"); done

# -S warning：只报 warning 及以上（style 级别噪音过多，豁免）；
# SC1091（source 外部文件无法跟踪）对本项目无意义，一并豁免
for f in "${SCRIPTS[@]}"; do
  shellcheck -S warning -e SC1091 "$f" >/dev/null 2>&1
  t_assert_eq 0 $? "shellcheck $(basename "$f")"
done

t_finish "test-shellcheck.sh"
