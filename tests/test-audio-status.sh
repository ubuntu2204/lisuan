#!/usr/bin/env bash
#
# test-audio-status.sh — verify the HDMI/DP audio fixer's CLI contract
# without root and without the hardware: --status always succeeds and prints
# a report, --help prints usage, unknown options fail with 2.
# License: Apache-2.0
set -uo pipefail
# shellcheck source=lib.sh
. "$(dirname "${BASH_SOURCE[0]}")/lib.sh"

FIXER="${REPO_ROOT}/audio/fix-lisuan-hdmi-audio.sh"

# 1) --status：即便本机没有 4c54 音频功能，也必须正常退出并给出结论，
#    不能因 set -e / 读 sysfs 失败而中途崩溃。
status_out="$(bash "$FIXER" --status 2>&1)"
t_assert_eq 0 $? "--status exits 0"
case "$status_out" in
  *"砺算 7G100 HDMI/DP 音频状态"*)
    t_ok "--status prints report header" ;;
  *)
    t_fail "--status prints report header (got: ${status_out:0:80})" ;;
esac

# 2) 有设备时必须报出驱动/中断/卡/开机自绑四项；无设备时给出"未找到"。
case "$status_out" in
  *"开机自绑"*) t_ok "--status reports boot service state" ;;
  *"未找到砺算伴生音频功能"*) t_ok "--status reports missing device" ;;
  *) t_fail "--status reports neither device nor absence" ;;
esac

# 3) --help 必须可用（脚本头部注释即用法）。
bash "$FIXER" --help >/dev/null 2>&1
t_assert_eq 0 $? "--help exits 0"

# 4) 未知参数必须显式失败，避免"打错参数却静默安装"。
bash "$FIXER" --definitely-not-an-option >/dev/null 2>&1
t_assert_eq 2 $? "unknown option exits 2"

# 5) 安装入口在无 root 时必须拒绝，而不是半途修改系统。
if [[ $EUID -ne 0 ]]; then
  bash "$FIXER" >/dev/null 2>&1
  t_assert_eq 1 $? "install without root refuses (exit 1)"
fi

t_finish "test-audio-status.sh"
