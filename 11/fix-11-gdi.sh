#!/usr/bin/env bash
# 至砺算：11 对战平台 GDI 句柄泄漏取证/回收包装层，与显卡无关。
#
# 根因（2026-11-19）：客户端每轮窗口消息取一枚显示 DC（GetDC/GetDCEx）后
# 不释放，Wine 11 的 GDI 句柄表 65536 用尽 → libcef.dll 0x80000003。
# 本脚本把 32 位前缀里的 user32.dll 换成包装层，其余导出转发给
# user32_orig.dll；诊断模式打印 GetDC 调用来源，E11_GDI_RECLAIM=1 才做回收。
# 只改本机前缀，厂商 DLL 不入库；不做监控/守护/自动重启。
#
# ⚠ 当前状态（2026-11-19）：这只是诊断件，不是修复。Wine 的 known DLL 让
# YYClient.exe 仍加载内置 user32，WINEDLLOVERRIDES 只拦到 11loader.exe；
# 客户端 GetDC 调用点尚未拿到。下一步见 11/README.md §9 与
# .codebuddy/rules/11平台启动排障约定.md 的 IAT 重定向方案。
#
# 用法：
#   ./11/fix-11-gdi.sh               # 编译并部署包装层（幂等）
#   ./11/fix-11-gdi.sh --status      # 只读体检
#   ./11/fix-11-gdi.sh --uninstall   # 恢复原 user32.dll
#   ./11/fix-11-gdi.sh --help
# License: Apache-2.0
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PREFIX="${E11_WINE11_PREFIX:-$HOME/.deepinwine/11duizhan-wine11}"
PREFIX="${PREFIX/#\~/$HOME}"
PROTON="${E11_PROTON:-$HOME/.local/share/Steam/steamapps/common/Proton 11.0}"
PROTON="${PROTON/#\~/$HOME}"
BUILTIN_U32="$PROTON/files/lib/wine/i386-windows/user32.dll"
SYSWOW="$PREFIX/drive_c/windows/syswow64"
DATA="${E11_GDI_DATA:-$HOME/.local/share/11duizhan-wine11/gdi-wrapper}"
DEF="$DATA/user32.def"
DLL="$DATA/user32.dll"
EXP="$DATA/user32-exp.o"
SRC="$REPO_DIR/user32-gdi-wrapper.c"
GEN="$REPO_DIR/gen-11-user32-def.py"

usage() {
    cat <<EOF
用法：$(basename "$0") [--status|--uninstall|--help]

  （无参数） 编译并部署 user32 包装层（包含原文件备份）
  --status   只读体检：包装层、转发 DLL、前缀内 user32.dll
  --uninstall 恢复原 user32.dll，移除包装层
  --help     显示本说明

运行时：需 E11_GDI_WRAP=1 让 run-11duizhan.sh 注入 user32=n,b；
        E11_GDI_DEBUG=1 打印每 500 次 GetDC/ReleaseDC 的调用来源；
        E11_GDI_RECLAIM=1 启用「回收上一枚未释放 DC」（默认关闭，仅取证）。
EOF
}

build() {
    command -v i686-w64-mingw32-gcc >/dev/null || { echo "错误：缺 i686-w64-mingw32-gcc" >&2; exit 1; }
    command -v i686-w64-mingw32-dlltool >/dev/null || { echo "错误：缺 i686-w64-mingw32-dlltool" >&2; exit 1; }
    [[ -f "$BUILTIN_U32" ]] || { echo "错误：找不到内置 user32：$BUILTIN_U32" >&2; exit 1; }
    [[ -f "$SRC" && -f "$GEN" ]] || { echo "错误：包装层源码缺失" >&2; exit 1; }
    mkdir -p "$DATA"
    python3 "$GEN" "$BUILTIN_U32" "$DEF"
    i686-w64-mingw32-dlltool --def "$DEF" --dllname user32.dll --output-exp "$EXP"
    i686-w64-mingw32-gcc -shared -O2 -o "$DLL" "$SRC" "$EXP" \
        -static-libgcc -luser32 -lkernel32
    echo "  已编译：$DLL"
}

status() {
    local rc=0
    [[ -f "$DLL" ]] && echo "  [OK] 包装层：$DLL" || { echo "  [--] 缺包装层：$DLL"; rc=1; }
    if [[ -d "$SYSWOW" ]]; then
        if cmp -s "$SYSWOW/user32_orig.dll" "$BUILTIN_U32"; then
            echo "  [OK] 转发 DLL：$SYSWOW/user32_orig.dll"
        else
            echo "  [--] 转发 DLL 不匹配：$SYSWOW/user32_orig.dll"; rc=1
        fi
        if cmp -s "$SYSWOW/user32.dll" "$DLL"; then
            echo "  [OK] 已部署：$SYSWOW/user32.dll"
        else
            echo "  [--] 前缀 user32.dll 不是包装层（未部署或已还原）"; rc=1
        fi
    else
        echo "  [--] 前缀缺 syswow64：$SYSWOW"; rc=1
    fi
    return "$rc"
}

apply() {
    [[ -d "$SYSWOW" ]] || { echo "错误：前缀缺 syswow64：$SYSWOW" >&2; exit 1; }
    build
    if [[ ! -f "$DATA/user32.dll.orig" ]]; then
        cp -p "$SYSWOW/user32.dll" "$DATA/user32.dll.orig"
        echo "  已备份原 user32.dll：$DATA/user32.dll.orig"
    fi
    rm -f "$SYSWOW/user32_orig.dll" "$SYSWOW/user32.dll"
    cp "$BUILTIN_U32" "$SYSWOW/user32_orig.dll"
    cp "$DLL" "$SYSWOW/user32.dll"
    echo "  已部署包装层。启动：E11_GDI_WRAP=1 ./11/run-11duizhan.sh"
    status
}

uninstall() {
    if [[ -f "$DATA/user32.dll.orig" ]]; then
        rm -f "$SYSWOW/user32.dll"
        cp -p "$DATA/user32.dll.orig" "$SYSWOW/user32.dll"
        echo "  已恢复：$SYSWOW/user32.dll"
    else
        echo "  [--] 无原文件备份，跳过恢复 user32.dll"
    fi
    rm -f "$SYSWOW/user32_orig.dll"
    echo "  已移除转发 DLL（包装层文件保留在 $DATA）。"
}

case "${1:-}" in
    --help|-h) usage; exit 0 ;;
    --status) status; exit $? ;;
    --uninstall) uninstall; exit 0 ;;
    --*) echo "错误：未知参数：$1" >&2; exit 1 ;;
    *) apply ;;
esac
