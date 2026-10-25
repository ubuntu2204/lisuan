#!/bin/bash
# vk32-scan.sh — 扫描「32 位 Venus 客户端」创建深度附件的可行参数组合。
# 目的：用正确的 VkImageUsageFlags 独立验证深度资源路径，不再供
#      vkcreateimage-shim.c 改写（2026-10-25 根因是 feedback-loop usage 能力检查）。
#
# 用法：bash vk32-scan.sh [probe 路径，默认 /tmp/vk32]
set -u
PROBE="${1:-/tmp/vk32}"
SOCK="${XDG_RUNTIME_DIR:-/run/user/1000}/l4d2-venus.sock"
export VN_DEBUG=vtest VTEST_SOCKET_NAME="$SOCK"
export VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/virtio_icd.json

# usage 位：TRANSFER_SRC=0x1 TRANSFER_DST=0x2 SAMPLED=0x4 STORAGE=0x8
#           COLOR_ATTACHMENT=0x10 DEPTH_STENCIL=0x20 INPUT_ATTACHMENT=0x80
COMBOS=(
  "D16/OPTIMAL/DEPTH_STENCIL:124:1:0x20"
  "D16/OPTIMAL/DEPTH_STENCIL|SAMPLED:124:1:0x24"
  "D16/OPTIMAL/DEPTH_STENCIL|TRANSFER_SRC:124:1:0x21"
  "D16/OPTIMAL/DEPTH_STENCIL|TRANSFER_DST:124:1:0x22"
  "D16/OPTIMAL/DEPTH_STENCIL|SAMPLED|TX_SRC|TX_DST:124:1:0x27"
  "D16/LINEAR/DEPTH_STENCIL:124:0:0x20"
  "D16/LINEAR/DEPTH_STENCIL|SAMPLED:124:0:0x24"
  "D16/OPTIMAL/SAMPLED:124:1:0x4"
  "D16/OPTIMAL/TRANSFER_DST:124:1:0x2"
  "D24S8/OPTIMAL/DEPTH_STENCIL:130:1:0x20"
  "D24S8/OPTIMAL/DEPTH_STENCIL|SAMPLED:130:1:0x24"
  "RGBA8/OPTIMAL/COLOR_ATTACHMENT:37:1:0x10"
  "RGBA8/OPTIMAL/COLOR_ATTACHMENT|SAMPLED:37:1:0x14"
)

printf '%-46s %s\n' "组合" "结果"
printf '%s\n' "---------------------------------------------------------------------------"
for c in "${COMBOS[@]}"; do
  name="${c%%:*}"; rest="${c#*:}"
  fmt="${rest%%:*}"; rest2="${rest#*:}"
  tiling="${rest2%%:*}"; usage="${rest2##*:}"
  out=$(timeout 25 "$PROBE" "$fmt" "$tiling" "$usage" 2>&1)
  rc=$?
  if [ $rc -ge 128 ] || [ $rc -eq 124 ]; then
    res="*** 崩溃/超时 (rc=$rc) ***"
  elif echo "$out" | grep -q "完整可用"; then
    res="OK 完整可用"
  elif echo "$out" | grep -q "bind 失败"; then
    res="WARN createImage ok, bind 失败"
  elif echo "$out" | grep -q "allocMemory 失败"; then
    res="WARN createImage ok, alloc 失败"
  elif echo "$out" | grep -q "无匹配 memoryType"; then
    res="WARN createImage ok, 无 memoryType"
  elif echo "$out" | grep -q "createImage 失败"; then
    res=$(echo "$out" | grep -o "createImage 失败 rc=[-0-9]*" | tail -1)
  else
    res="? $(echo "$out" | tail -1)"
  fi
  printf '%-46s %s\n' "$name" "$res"
done
