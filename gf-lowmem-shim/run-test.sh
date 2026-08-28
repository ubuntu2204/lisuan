#!/usr/bin/env bash
#
# run-test.sh — 用不同 shim 模式启动《随从大师》做对照测试
#
#   ./run-test.sh base       # 基线：不挂 shim，直连 gf（预期 11 行 + buffer 错误）
#   ./run-test.sh pass       # shim 纯转发，不拦截任何函数
#   ./run-test.sh noexport   # shim 介入但不注入导出扩展
#   ./run-test.sh full       # 完整模式（注入扩展 + 低位映射）
#
set -u

MODE="${1:-full}"
D=/home/ubuntu/project/lisuan/gf-lowmem-shim
GAME="/home/ubuntu/.local/share/Steam/steamapps/common/Minion Masters"
# 可用 PROTON_DIR 覆盖，例如：
#   PROTON_DIR="$HOME/.local/share/Steam/steamapps/common/Proton - MM GF" ./run-test.sh full
PE="${PROTON_DIR:-$HOME/.local/share/Steam/steamapps/common/Proton - Experimental}"
SRT="$HOME/.local/share/Steam/steamapps/common/SteamLinuxRuntime_4"
PFX="$HOME/.local/share/Steam/steamapps/compatdata/489520"
LOGD="$PFX/pfx/drive_c/users/steamuser/AppData/LocalLow/BetaDwarf ApS/Minion Masters"
LOG="/tmp/mm-test-$MODE.log"

USE_ICD=1
case "$MODE" in
  base)     USE_ICD=0; EXTRA="" ;;
  pass)     EXTRA="GF_SHIM_PASS=1" ;;
  noexport) EXTRA="GF_SHIM_NO_EXPORT=1" ;;
  full)     EXTRA="" ;;
  *) echo "用法: $0 {base|pass|noexport|full}"; exit 1 ;;
esac

# 追加任意环境变量，例：EXTRA_ENV="PROTON_ENABLE_NATIVE_VULKAN_LOADER=1" ./run-test.sh full
EXTRA="$EXTRA ${EXTRA_ENV:-}"

# 清掉可能残留的进程，避免互相干扰
pkill -f "proton run MinionMasters" 2>/dev/null
sleep 1
rm -f "$LOGD/Player.log"

cd "$GAME" || exit 1

# 说明：官方 Proton 的 DXVK v3.0.2 要求 VK_KHR_maintenance5，gf 不支持会直接初始化失败，
# 因此测试应配合自定义 Proton（内含 DXVK v2.3），见 PROTON_DIR。
# 这里不再用 WINEDLLPATH——wine 优先加载 pfx 里的 DLL，而 Proton 每次启动都会重新同步覆盖。

if [ -n "${ICD_FILE:-}" ]; then
  export VK_ICD_FILENAMES="$ICD_FILE"
elif [ "$USE_ICD" = "1" ]; then
  export VK_ICD_FILENAMES="$D/gf_lowmem_icd.json"
else
  unset VK_ICD_FILENAMES || true
fi

timeout 45 env $EXTRA \
  WINEDEBUG="${WDBG:-}" \
  PROTON_LOG="${PLOG:-}" \
  LD_PRELOAD=/home/ubuntu/gf-compat/lib/osrelease_redirect.so \
  PROTON_USE_WOW64=1 \
  STEAM_COMPAT_DATA_PATH="$PFX" \
  STEAM_COMPAT_CLIENT_INSTALL_PATH="$HOME/.local/share/Steam" \
  "$SRT/_v2-entry-point" --verb=run -- "$PE/proton" run MinionMasters.exe \
  >"$LOG" 2>&1

echo "===== 模式: $MODE ====="
echo "Player.log 行数: $(wc -l < "$LOGD/Player.log" 2>/dev/null)"
echo -n "渲染器: "; grep -m1 "Renderer:" "$LOGD/Player.log" 2>/dev/null || echo "(无)"
echo "buffer 错误数: $(grep -c 'failed to create buffer' "$LOGD/Player.log" 2>/dev/null)"
echo -n "shim 低位映射次数: "; grep -c "\[gf-shim\] map:" "$LOG" 2>/dev/null
echo -n "instance 创建失败: "; grep -c "Failed to create vulkan instance" "$LOG" 2>/dev/null
echo "详细日志: $LOG"
