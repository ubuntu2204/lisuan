#!/usr/bin/env bash
#
# deploy.sh — 编译 gf-compat 的 LD_PRELOAD 探针并部署到运行时目录
#
# 用法：
#   ./deploy.sh                 编译并部署到本目录（默认）
#   ./deploy.sh --dry-run       只打印将执行的操作，不改动任何文件
#   DEST=/path/to/dir ./deploy.sh   部署到指定目录
#
# 运行时目录就是本目录：源码与编译产物同处 minion-masters/gf-compat/。
# 2026-08-31 之前运行时在 ~/gf-compat，现已并入仓库；Steam 启动选项里的
# LD_PRELOAD 指向的是这里的 lib/osrelease_redirect.so。
#
# 关于厂商专有二进制（libSC.so / libvulkan_gf.so）：
#   这两个库属于 lisuangpu-lib 包，是第三方专有软件，按仓库政策不入库
#   （见仓库根目录 .gitignore）。脚本只检查它们是否存在，
#   缺失时给出获取位置，不会尝试下载。
#
set -euo pipefail

SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEST="${DEST:-$SRC_DIR}"
DRY_RUN=0

if [ "${1:-}" = "--dry-run" ]; then
    DRY_RUN=1
fi

say() { printf '==> %s\n' "$*"; }
warn() { printf '  !! %s\n' "$*" >&2; }
ok() { printf '  ok  %s\n' "$*"; }

# dry-run 时只打印，否则真正执行
run() {
    if [ "$DRY_RUN" -eq 1 ]; then
        printf '    %s\n' "$*"
    else
        "$@"
    fi
}

say "gf-compat 部署：$SRC_DIR -> $DEST"

# ---- 1. 目录骨架 ----
run mkdir -p "$DEST/lib"

# ---- 2. 编译 LD_PRELOAD 探针 ----
# osrelease_redirect : 生产路径在用（发行版白名单绕过 + 低 4GB 显存映射重定位）
# 其余三个        : 排查 Vulkan/DXVK 问题时的诊断探针，按需使用
build_so() {
    local src="$1"
    local out="$2"
    local name
    name="$(basename "$src")"

    if [ ! -f "$src" ]; then
        warn "源文件缺失，跳过：$src"
        return 0
    fi
    if [ "$DRY_RUN" -eq 1 ]; then
        printf '    gcc -shared -fPIC -O2 -o %s %s -ldl -lpthread\n' "$out" "$src"
        return 0
    fi
    printf '  编译 %s\n' "$name"
    gcc -shared -fPIC -O2 -o "$out" "$src" -ldl -lpthread
    ok "$out"
}

build_so "$SRC_DIR/osrelease_redirect.c" "$DEST/lib/osrelease_redirect.so"
build_so "$SRC_DIR/hook.c" "$DEST/hook.so"
build_so "$SRC_DIR/vkfix.c" "$DEST/vkfix.so"
build_so "$SRC_DIR/vkspy.c" "$DEST/vkspy.so"

# ---- 3. 配置与 Python 探针 ----
say "复制配置与 Python 探针"
run cp -f "$SRC_DIR/gf_icd.json" "$DEST/gf_icd.json"
run cp -f "$SRC_DIR/os-release" "$DEST/os-release"
run cp -f "$SRC_DIR/vkprobe.py" "$DEST/vkprobe.py"
run cp -f "$SRC_DIR/vkdev.py" "$DEST/vkdev.py"
run cp -f "$SRC_DIR/vkdev2.py" "$DEST/vkdev2.py"
run cp -f "$SRC_DIR/drmname.py" "$DEST/drmname.py"

# ---- 4. 厂商专有二进制检查（不随仓库分发）----
say "检查厂商专有二进制"
missing=0
for lib in libSC.so libvulkan_gf.so; do
    if [ -f "$DEST/lib/$lib" ]; then
        ok "$lib 已就位"
    else
        missing=1
        warn "lib/$lib 缺失"
    fi
done
if [ "$missing" -eq 1 ]; then
    warn ""
    warn "厂商库需自行从 lisuangpu-lib 包获取，不在本仓库分发："
    warn "  libvulkan_gf.so  /usr/lib/x86_64-linux-gnu/libvulkan_gf.so"
    warn "  libSC.so         容器内为 /run/host/usr/lib/x86_64-linux-gnu/libSC.so"
    warn "  复制进 $DEST/lib/ 后重跑本脚本即可通过检查。"
fi

say "完成。Steam 启动选项引用的库：$DEST/lib/osrelease_redirect.so"
