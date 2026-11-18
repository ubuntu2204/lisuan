#!/usr/bin/env bash
#
# test-kk-shim-exit.sh — 垫片退出残框清理（C 项）确定性验收。
#
# LD_PRELOAD 加载 kk-geometry-shim 到独立 X11 客户端（tests/fixtures/
# kk-shim-exit-test.c），按特征尺寸建主框架 / 内容层 / 挽留弹窗，验证：
#   1) 主框架 unmap 时 OR 内容层同步隐藏、remap 时同步恢复（残框修复）；
#   2) 非退出路径（未销毁挽留弹窗）watchdog 不杀进程（最小化不误杀）；
#   3) 退出路径（销毁挽留弹窗 + 主框架隐藏）watchdog ~5s 后从 /proc/self/exe
#      同目录推导 wineserver 跑 -k（假 wineserver 记录调用）并终止进程。
# 缺 gcc / X11 开发头 / DISPLAY 时 SKIP（不拖垮套件）。
# License: Apache-2.0
set -uo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh"

if ! command -v gcc >/dev/null 2>&1 || ! pkg-config --exists x11 2>/dev/null \
   || [[ -z "${DISPLAY:-}" ]]; then
  t_skip "垫片退出机制验收（缺 gcc / X11 dev / DISPLAY）"
  exit 0
fi

gcc -shared -fPIC -O2 -o "$TEST_TMP/shim.so" \
  "$REPO_ROOT/kk/kk-geometry-shim.c" -ldl || { t_fail "垫片编译"; t_finish; }
mkdir -p "$TEST_TMP/bin"
gcc -O2 -o "$TEST_TMP/bin/kk-shim-exit-test" \
  "$FIXTURES_DIR/kk-shim-exit-test.c" -lX11 || { t_fail "机制测试客户端编译"; t_finish; }
# 假 wineserver：记录调用参数，验证 watchdog 的路径推导与 -k 意图
cat > "$TEST_TMP/bin/wineserver" <<EOF
#!/bin/sh
echo "\$*" > "$TEST_TMP/wineserver-args"
EOF
chmod +x "$TEST_TMP/bin/wineserver"

run_client() { # $1=mode
  LD_PRELOAD="$TEST_TMP/shim.so" WINEPREFIX="$TEST_TMP/prefix" \
    "$TEST_TMP/bin/kk-shim-exit-test" $1 2>&1
}

# --- 非退出路径：随隐藏 / 随恢复 / 不误杀 ---
out="$(run_client "")"; rc=$?
if [[ $rc -eq 77 ]]; then
  t_skip "垫片退出机制验收（X 连接 / d32 visual 不具备）"
  exit 0
fi
t_assert_eq 0 "$rc" "非退出路径客户端正常结束"
t_assert_grep <(printf '%s\n' "$out") 'layer-hidden' "主框架隐藏时内容层同步隐藏"
t_assert_grep <(printf '%s\n' "$out") 'layer-restored' "主框架恢复时内容层同步恢复"
t_assert_grep <(printf '%s\n' "$out") 'no-watchdog' "非退出路径 watchdog 不触发"

# --- 退出路径：watchdog 兜底 ---
t0=$(date +%s)
out="$(run_client arm)"; rc=$?
t1=$(date +%s)
elapsed=$((t1 - t0))
t_assert_eq 0 "$rc" "退出路径进程被 watchdog 终止（rc=0）"
if [[ $elapsed -ge 4 && $elapsed -le 7 ]]; then
  t_ok "watchdog 约 5s 触发（实测 ${elapsed}s）"
else
  t_fail "watchdog 触发时间异常（${elapsed}s，期望 4..7s）"
fi
t_assert_grep <(printf '%s\n' "$out") 'layer-hidden' "退出路径内容层同步隐藏（无残框）"
if [[ -f "$TEST_TMP/wineserver-args" ]] && grep -q -- '-k' "$TEST_TMP/wineserver-args"; then
  t_ok "watchdog 推导同目录 wineserver 并执行 -k"
else
  t_fail "watchdog 未调用 wineserver -k"
fi

t_finish