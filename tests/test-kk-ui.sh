#!/usr/bin/env bash
#
# test-kk-ui.sh — KK 对战平台 UI 自动化验收（真实指针注入 + 合成画面断言）。
#
# 环境具备（GNOME Wayland + KK 已安装 + 依赖齐全）时跑 kk/ui-autotest.py：
# 真实点击活动弹窗 X、悬停侧栏行断言 UI 变化、真实点击视觉中心断言选中、
# 再点 DotA 断言切换；命中偏移会被实测并判 FAIL。
# 环境不具备时 ui-autotest.py 退出 77，本测试记 SKIP 并以 0 退出（不拖垮套件）。
# 另含不依赖 GUI 的静态断言（注入通道 / 收尾 / 偏移扫描存在）。
# License: Apache-2.0
set -uo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh"

AUTOTEST="${REPO_ROOT}/kk/ui-autotest.py"

t_assert_grep "$AUTOTEST" 'org.gnome.Mutter.RemoteDesktop' \
  "KK UI autotest injects real pointer via Mutter RemoteDesktop"
t_assert_grep "$AUTOTEST" 'NotifyPointerMotionRelative' \
  "KK UI autotest injects relative pointer motion (real hit path, no screencast prereg)"
t_assert_grep "$AUTOTEST" 'wineserver' \
  "KK UI autotest explicitly teardowns launched KK"
t_assert_not_grep "$AUTOTEST" 'xdotool click' \
  "KK UI autotest does not rely on XTEST clicks (undelivered under XWayland)"

out="$(python3 "$AUTOTEST" 2>&1)"; rc=$?
printf '%s\n' "$out" | sed 's/^/    /'
if [[ $rc -eq 77 ]]; then
  t_skip "KK UI 自动化验收（环境不具备：见上方 SKIP 原因）"
  exit 0
elif [[ $rc -eq 0 ]]; then
  t_ok "KK UI 自动化验收（X 可点 / 悬停反馈 / 命中零偏移 / 选中切换）"
  exit 0
else
  t_fail "KK UI 自动化验收（exit $rc）"
  exit 1
fi