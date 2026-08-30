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

# osrelease_redirect 需要特殊处理：它内部把伪造的 os-release 路径写死成哨兵值
# /home/ubuntu/gf-compat/os-release。必须按实际部署位置改写后再编译，否则
# 驱动读不到白名单文件会拒绝初始化，表现为：
#   err:vulkan:vulkan_init_once Failed to initialize instance extensions, res -1
#   Required Vulkan extension VK_KHR_surface not supported
#   DxvkInstance: Required instance extensions not supported
build_osr() {
    local src="$1"
    local out="$2"
    local tmp="$DEST/.osr.build.c"

    if [ ! -f "$src" ]; then
        warn "源文件缺失，跳过：$src"
        return 0
    fi
    if [ "$DRY_RUN" -eq 1 ]; then
        printf '    sed "s|/home/ubuntu/gf-compat/os-release|%s/os-release|g" %s\n' \
            "$DEST" "$src"
        printf '    gcc -shared -fPIC -O2 -o %s <改写后源码> -ldl -lpthread\n' "$out"
        return 0
    fi
    printf '  编译 %s（按部署位置改写 os-release 路径）\n' "$(basename "$src")"
    sed "s|/home/ubuntu/gf-compat/os-release|$DEST/os-release|g" "$src" > "$tmp"
    gcc -shared -fPIC -O2 -o "$out" "$tmp" -ldl -lpthread
    rm -f "$tmp"
    ok "$out"
}

# 协商垫片 ICD：内部 dlopen 厂商库的路径同样是哨兵值
# /home/ubuntu/gf-compat/lib/libvulkan_gf.so，失效会导致垫片加载不了真驱动。
# 源码随 C# 补丁工程放在 ../assets-mm-gf-vulkan/。
build_shim() {
    local src="$SRC_DIR/../assets-mm-gf-vulkan/libvulkan_gf_shim.c"
    local out="$DEST/lib/libvulkan_gf_shim.so"
    local tmp="$DEST/.shim.build.c"

    if [ ! -f "$src" ]; then
        warn "找不到垫片源码：$src"
        return 0
    fi
    if [ "$DRY_RUN" -eq 1 ]; then
        printf '    sed "s|/home/ubuntu/gf-compat|%s|g" %s\n' "$DEST" "$src"
        printf '    gcc -shared -fPIC -O2 -o %s <改写后源码> -ldl\n' "$out"
        return 0
    fi
    printf '  编译 %s（按部署位置改写 dlopen 路径）\n' "$(basename "$src")"
    sed "s|/home/ubuntu/gf-compat|$DEST|g" "$src" > "$tmp"
    gcc -shared -fPIC -O2 -o "$out" "$tmp" -ldl
    rm -f "$tmp"
    ok "$out"
}

build_osr "$SRC_DIR/osrelease_redirect.c" "$DEST/lib/osrelease_redirect.so"
build_shim
build_so "$SRC_DIR/hook.c" "$DEST/hook.so"
build_so "$SRC_DIR/vkfix.c" "$DEST/vkfix.so"
build_so "$SRC_DIR/vkspy.c" "$DEST/vkspy.so"

# ---- 3. 配置与 Python 探针 ----
# 默认 DEST 即 SRC_DIR（就地部署），此时复制会指向同一文件而报错
if [ "$SRC_DIR" = "$DEST" ]; then
  say "源与部署目录相同，跳过复制"
else
  say "复制配置与 Python 探针"
  run cp -f "$SRC_DIR/gf_icd.json" "$DEST/gf_icd.json"
  run cp -f "$SRC_DIR/os-release" "$DEST/os-release"
  run cp -f "$SRC_DIR/vkprobe.py" "$DEST/vkprobe.py"
  run cp -f "$SRC_DIR/vkdev.py" "$DEST/vkdev.py"
  run cp -f "$SRC_DIR/vkdev2.py" "$DEST/vkdev2.py"
  run cp -f "$SRC_DIR/drmname.py" "$DEST/drmname.py"
fi

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
