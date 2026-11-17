#!/usr/bin/env bash
# 80launch.exe PE stack fix; local prefix only.
set -euo pipefail
PREFIX="${E80_WINE11_PREFIX:-$HOME/.deepinwine/80duizhan-wine11}"
PREFIX="${PREFIX/#\~/$HOME}"
REL="${E80_INSTALL_DIR:-Program Files (x86)/80duizhan}"
EXE="$PREFIX/drive_c/${REL#/}/80launch.exe"
BAK="$EXE.bak-stack"
die() { echo "错误：$*" >&2; exit 1; }
peek() {
python3 - "$EXE" <<PY
import struct,sys
d=open(sys.argv[1],"rb").read(4096)
pe=struct.unpack_from("<I",d,0x3c)[0]; opt=pe+24
if d[pe:pe+4]!=b"PE\0\0" or struct.unpack_from("<H",d,opt)[0]!=0x10b:
 sys.exit(2)
r=struct.unpack_from("<I",d,opt+0x48)[0]; c=struct.unpack_from("<I",d,opt+0x4c)[0]
print("  栈保留/提交：0x%x / 0x%x"%(r,c))
sys.exit(0 if (r,c)==(0x800000,0x10000) else (1 if (r,c)==(0x100000,0x1000) else 2))
PY
}
status() {
 [[ -s "$EXE" ]] || die "主程序不存在：$EXE"
 echo "  主程序：$EXE"
 local rc=0; peek || rc=$?; return "$rc"
}
apply() {
 [[ -s "$EXE" ]] || die "主程序不存在：$EXE"
 local rc=0; peek || rc=$?
 case "$rc" in
  0) echo "  已修复，无需重复修改。"; return 0 ;;
  1) ;;
  *) die "栈头检查未通过（$rc），未改动文件" ;;
 esac
 [[ -f "$BAK" ]] || { cp -p "$EXE" "$BAK"; echo "  已备份：$BAK"; }
 python3 - "$EXE" <<PY
import struct,sys
p=sys.argv[1]; d=bytearray(open(p,"rb").read())
pe=struct.unpack_from("<I",d,0x3c)[0]; opt=pe+24
struct.pack_into("<I",d,opt+0x48,0x800000)
struct.pack_into("<I",d,opt+0x4c,0x10000)
open(p,"wb").write(d)
PY
 echo "  已写入 0x800000/0x10000"; peek
}
uninstall() {
 [[ -f "$BAK" ]] || die "没有备份：$BAK"
 cp -p "$BAK" "$EXE"
 echo "  已恢复：$EXE"; peek || true
}
case "${1:-}" in
  --help|-h) echo "用法：$0 [--status|--uninstall]"; exit 0 ;;
  --status|--check) status; exit $? ;;
  --uninstall|--restore) uninstall; exit 0 ;;
  --*) die "未知参数：$1" ;;
  *) apply ;;
esac
