#!/usr/bin/env bash
# 重建 hook 变体，避免在 /tmp 下临时编译（会被清理、不进 git、不可复现）。
#
# 用法: ./build.sh [baseline-64k|norecycle|main] [--deploy]
#
#   baseline-64k  修改前原版（全 64KB 对齐，无地址回收）—— 对照基准
#   norecycle     4KB 小对齐 + 无回收
#   main          ../osrelease_redirect.c（项目当前主源码，默认）
#   --deploy      构建后直接部署到 ../lib/osrelease_redirect.so
set -euo pipefail
cd "$(dirname "$0")"

V="${1:-main}"
DEPLOY="${2:-}"

case "$V" in
  baseline-64k) SRC="osrelease_redirect.baseline-64k.c" ;;
  norecycle)    SRC="osrelease_redirect.norecycle.c" ;;
  main)         SRC="../osrelease_redirect.c" ;;
  *) echo "未知变体: $V（可选 baseline-64k|norecycle|main）" >&2; exit 1 ;;
esac

[ -f "$SRC" ] || { echo "源码缺失: $SRC" >&2; exit 1; }

OUT="osrelease_redirect.$V.so"
gcc -shared -fPIC -O2 -o "$OUT" "$SRC" -ldl -lpthread
echo "已构建: $OUT  (回收: $(nm -D "$OUT" | grep -c ' T pool_take' || true))"

if [ "$DEPLOY" = "--deploy" ]; then
  cp "$OUT" ../lib/osrelease_redirect.so
  echo "已部署: ../lib/osrelease_redirect.so"
fi
