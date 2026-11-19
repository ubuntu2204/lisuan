#!/usr/bin/env bash
# 至砺算：11 平台 GDI 泄漏 IAT 重定向取证工具（与显卡无关）。
#
# 目的：把目标 PE 的 USER32.dll 导入名等长替换为 e11gdi.dll，让
# 11/user32-gdi-wrapper.c 真正进入客户端进程，打印 GetDC/GetDCEx/ReleaseDC
# 的 HDC、返回地址和调用模块，抓到泄漏调用点。E11_GDI_RECLAIM=1 时才做
# 「回收上一枚未释放 DC」的定点实验，默认只取证。
#
# 原理：USER32.dll\0 与 e11gdi.dll\0 都是 10 字节，原地替换导入描述符里的
# DLL 名即可；其它导出由包装层转发到 user32_orig.dll（Proton 内置 user32 副本）。
# 只改本机前缀内的厂商二进制，改前备份，--uninstall 可精确还原。
# 不做存活监控、自动重启或守护。
#
# 用法：
#   ./11/redirect-11-gdi-iat.sh              # 构建 wrapper + 备份 + 改 IAT
#   ./11/redirect-11-gdi-iat.sh --status     # 只读体检
#   ./11/redirect-11-gdi-iat.sh --uninstall  # 还原目标 PE 并移除部署的 DLL
#   ./11/redirect-11-gdi-iat.sh --help
#
# 默认目标：正式版 bins/11Client.exe。要诊断 CEF 可设
#   E11_GDI_IAT_TARGET=bins/cef/libcef.dll
# 取证运行：E11_GDI_DEBUG=1 E11_WINEDEBUG='+err+seh-fixme-all' ./11/run-11duizhan.sh
# 回收 A/B：再加 E11_GDI_RECLAIM=1
#
# 环境变量：E11_WINE11_PREFIX / E11_PROTON / E11_INSTALL_DIR /
#   E11_GDI_IAT_TARGET / E11_GDI_IAT_DATA
# License: Apache-2.0
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PREFIX="${E11_WINE11_PREFIX:-$HOME/.deepinwine/11duizhan-263}"
PREFIX="${PREFIX/#\~/$HOME}"
INSTALL_DIR_REL="${E11_INSTALL_DIR:-11duizhan}"
INSTALL_DIR_REL="${INSTALL_DIR_REL#/}"
DRIVE_C="$PREFIX/drive_c/$INSTALL_DIR_REL"
PROTON="${E11_PROTON:-$HOME/.local/share/Steam/steamapps/common/Proton 11.0}"
PROTON="${PROTON/#\~/$HOME}"
BUILTIN_U32="$PROTON/files/lib/wine/i386-windows/user32.dll"
TARGET_REL="${E11_GDI_IAT_TARGET:-bins/11Client.exe}"
TARGET_REL="${TARGET_REL#/}"
TARGET="$DRIVE_C/$TARGET_REL"
TARGET_DIR="$(dirname "$TARGET")"
DATA="${E11_GDI_IAT_DATA:-$HOME/.local/share/$(basename "$PREFIX")/gdi-iat}"
DEF="$DATA/e11gdi.def"
EXP="$DATA/e11gdi-exp.o"
WRAPPER_BUILT="$DATA/e11gdi.dll"
ORIG_BUILT="$DATA/user32_orig.dll"
BACKUP="$DATA/$(basename "$TARGET").orig"
WRAPPER_DEPLOYED="$TARGET_DIR/e11gdi.dll"
ORIG_DEPLOYED="$TARGET_DIR/user32_orig.dll"

die() { echo "错误：$*" >&2; exit 1; }

usage() {
    cat <<EOF
用法：$(basename "$0") [--status|--uninstall|--help]

  （无参数） 构建 e11gdi.dll + user32_orig.dll，备份目标 PE 并重定向导入表
  --status    只读体检：目标 IAT 状态、备份与 wrapper 部署状态
  --uninstall 还原目标 PE，移除部署的 e11gdi.dll / user32_orig.dll
  --help      显示本说明

默认目标：$TARGET
包装层源码：$REPO_DIR/user32-gdi-wrapper.c
构建/备份目录：$DATA

运行时：
  E11_GDI_DEBUG=0    关闭调用来源日志（默认开启，写 stderr 和 e11gdi-calls.log）
  E11_GDI_CACHE=0    关闭屏幕 DC 缓存（默认开启，GetDC(NULL) 复用 HDC）
  E11_GDI_RECLAIM=1  启用「回收上一枚未释放 DC」实验（默认关闭）
EOF
}

# 返回：patched / default / missing / ambiguous
iat_state() {
    python3 - "$1" <<'PY'
import sys
p = sys.argv[1]
try:
    d = open(p, 'rb').read()
except OSError:
    print('missing'); raise SystemExit(0)
old = b'USER32.dll\x00'
new = b'e11gdi.dll\x00'
if d.count(new) == 1 and d.count(old) == 0:
    print('patched')
elif d.count(old) == 1 and d.count(new) == 0:
    print('default')
else:
    print('ambiguous')
PY
}

norm_sha() {
    python3 - "$1" <<'PY'
import hashlib, sys
d = bytearray(open(sys.argv[1], 'rb').read())
d = d.replace(b'e11gdi.dll\x00', b'USER32.dll\x00')
print(hashlib.sha256(d).hexdigest())
PY
}

build() {
    command -v i686-w64-mingw32-gcc >/dev/null || die "缺 i686-w64-mingw32-gcc"
    command -v i686-w64-mingw32-dlltool >/dev/null || die "缺 i686-w64-mingw32-dlltool"
    [[ -f "$BUILTIN_U32" ]] || die "找不到内置 user32：$BUILTIN_U32"
    [[ -f "$REPO_DIR/user32-gdi-wrapper.c" && -f "$REPO_DIR/gen-11-user32-def.py" ]] || \
        die "包装层源码或 def 生成器缺失"
    mkdir -p "$DATA"
    python3 "$REPO_DIR/gen-11-user32-def.py" "$BUILTIN_U32" "$DEF" e11gdi
    i686-w64-mingw32-dlltool --def "$DEF" --dllname e11gdi.dll --output-exp "$EXP"
    i686-w64-mingw32-gcc -shared -O2 -o "$WRAPPER_BUILT" \
        "$REPO_DIR/user32-gdi-wrapper.c" "$EXP" -static-libgcc -luser32 -lkernel32
    rm -f "$ORIG_BUILT"
    cp -p "$BUILTIN_U32" "$ORIG_BUILT"
    echo "  已构建：$WRAPPER_BUILT / $ORIG_BUILT"
}

status() {
    local rc=0 state
    [[ -f "$TARGET" ]] || { echo "  [--] 目标缺失：$TARGET"; return 1; }
    state="$(iat_state "$TARGET")"
    case "$state" in
        patched) echo "  [OK] IAT：已指向 e11gdi.dll（$TARGET_REL）" ;;
        default) echo "  [--] IAT：仍为 USER32.dll（$TARGET_REL）" ;;
        *) echo "  [--] IAT 状态异常：$state（$TARGET_REL）"; rc=1 ;;
    esac
    [[ -f "$BACKUP" ]] && echo "  [OK] 目标备份：$BACKUP" || { echo "  [--] 无备份：$BACKUP"; rc=1; }
    [[ -f "$WRAPPER_BUILT" ]] && echo "  [OK] 包装层源码产物：$WRAPPER_BUILT" || { echo "  [--] 未构建包装层"; rc=1; }
    if [[ -f "$WRAPPER_DEPLOYED" ]] && cmp -s "$WRAPPER_DEPLOYED" "$WRAPPER_BUILT"; then
        echo "  [OK] 已部署 wrapper：$WRAPPER_DEPLOYED"
    else
        echo "  [--] 未部署或不一致：$WRAPPER_DEPLOYED"; rc=1
    fi
    if [[ -f "$ORIG_DEPLOYED" ]] && cmp -s "$ORIG_DEPLOYED" "$ORIG_BUILT"; then
        echo "  [OK] 已部署转发 DLL：$ORIG_DEPLOYED"
    else
        echo "  [--] 未部署或不一致：$ORIG_DEPLOYED"; rc=1
    fi
    return "$rc"
}

apply() {
    local state
    [[ -f "$TARGET" ]] || die "目标不存在：$TARGET"
    state="$(iat_state "$TARGET")"
    [[ "$state" == "default" || "$state" == "patched" ]] || die "目标 IAT 状态异常：$state"
    build

    if [[ "$state" == "default" ]]; then
        if [[ -f "$BACKUP" ]] && [[ "$(norm_sha "$BACKUP")" != "$(norm_sha "$TARGET")" ]]; then
            local rotated="$BACKUP.$(date +%s)"
            mv "$BACKUP" "$rotated"
            echo "  版本变化，旧备份转存：$rotated"
        fi
        [[ -f "$BACKUP" ]] || { cp -p "$TARGET" "$BACKUP"; echo "  已备份：$BACKUP"; }
        python3 - "$TARGET" <<'PY'
import sys
p = sys.argv[1]
d = open(p, 'rb').read()
old, new = b'USER32.dll\x00', b'e11gdi.dll\x00'
if d.count(old) != 1 or d.count(new) != 0:
    raise SystemExit(f'IAT 名称出现次数异常：old={d.count(old)} new={d.count(new)}')
open(p, 'wb').write(d.replace(old, new))
print(f'  已重定向 IAT：{p}')
PY
    else
        echo "  [=] IAT 已是 e11gdi.dll，跳过改文件"
    fi

    rm -f "$WRAPPER_DEPLOYED" "$ORIG_DEPLOYED"
    cp "$WRAPPER_BUILT" "$WRAPPER_DEPLOYED"
    cp "$ORIG_BUILT" "$ORIG_DEPLOYED"
    echo "  已部署：$WRAPPER_DEPLOYED / $ORIG_DEPLOYED"
    status
}

uninstall() {
    [[ -f "$BACKUP" ]] || die "无备份，拒绝还原：$BACKUP"
    if [[ "$(iat_state "$TARGET")" == "patched" ]]; then
        cp -p "$BACKUP" "$TARGET"
        echo "  已还原：$TARGET"
    else
        echo "  [=] 目标 IAT 未重定向，未覆盖文件"
    fi
    # 这两个 DLL 名仅由本工具生成，重建后可能与已部署版本哈希不同；一律移除。
    [[ -f "$WRAPPER_DEPLOYED" ]] && { rm -f "$WRAPPER_DEPLOYED"; echo "  已移除：$WRAPPER_DEPLOYED"; } || true
    [[ -f "$ORIG_DEPLOYED" ]] && { rm -f "$ORIG_DEPLOYED"; echo "  已移除：$ORIG_DEPLOYED"; } || true
    echo "  构建产物与备份保留在 $DATA"
}

case "${1:-}" in
    --help|-h) usage; exit 0 ;;
    --status) status; exit $? ;;
    --uninstall) uninstall; exit 0 ;;
    --*) die "未知参数：$1（用 --help 查看用法）" ;;
    *) apply ;;
esac
