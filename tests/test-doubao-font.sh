#!/usr/bin/env bash
#
# test-doubao-font.sh — 验证豆包专用字体替换配置存在、被启动器注入，
# 且能把过细的 PingFang SC/Light 回退抬到常规字重。不启动豆包。
# License: Apache-2.0
set -uo pipefail
# shellcheck source=lib.sh
. "$(dirname "${BASH_SOURCE[0]}")/lib.sh"

CONF="${REPO_ROOT}/doubao/fonts.conf"
LAUNCH="${REPO_ROOT}/doubao/run-doubao-wine11.sh"

[[ -f "$CONF" ]]
t_assert_eq 0 $? "doubao fontconfig file present"
t_assert_grep "$CONF" 'PingFang SC' "config overrides PingFang SC"
t_assert_grep "$CONF" '<const>medium</const>' "config raises font weight to medium"
t_assert_grep "$LAUNCH" 'FONTCONFIG_FILE' "launcher injects FONTCONFIG_FILE"

if command -v fc-match >/dev/null 2>&1; then
    out="$(FONTCONFIG_FILE="$CONF" fc-match -f '%{family}|%{style}' 'PingFang SC:weight=light' 2>/dev/null)"
    case "$out" in
        *Microsoft\ YaHei*|*微软雅黑*)
            t_ok "PingFang SC light resolves to Microsoft YaHei"
            ;;
        *)
            t_fail "PingFang SC still resolves to: $out"
            ;;
    esac
    case "$out" in
        *Light*|*Thin*|*DemiLight*)
            t_fail "light face still selected: $out"
            ;;
        *)
            t_ok "no Light/Thin face selected"
            ;;
    esac
else
    t_skip "fc-match unavailable; skip runtime font check"
fi

t_finish "test-doubao-font.sh"
