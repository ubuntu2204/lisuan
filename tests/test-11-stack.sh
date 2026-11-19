#!/usr/bin/env bash
#
# test-11-stack.sh — verify 11/fix-11-stack.sh PE stack patch logic using a
# minimal PE32 fixture (no real platform / wine needed): patch grows the stack
# reserve, status reports it, apply is idempotent, uninstall restores the
# original bytes exactly.
# License: Apache-2.0
set -uo pipefail
# shellcheck source=lib.sh
. "$(dirname "${BASH_SOURCE[0]}")/lib.sh"

FIX="${REPO_ROOT}/11/fix-11-stack.sh"
WORK="${TEST_TMP}/11stack"
PREFIX="${WORK}/prefix"
INST="${PREFIX}/drive_c/11duizhan"
BACKUP="${WORK}/backups"
mkdir -p "${INST}/yybins" "${BACKUP}"

# 生成最小 PE32：MZ + e_lfanew=0x80 + PE 签名 + 可选头 magic 0x10b，
# StackReserve=1MB / StackCommit=4KB（脚本只读这些字段）。
make_pe() {
  python3 - "$1" <<'PY'
import struct, sys
p = sys.argv[1]
data = bytearray(0x100)
data[0:2] = b'MZ'
struct.pack_into('<I', data, 0x3c, 0x80)        # e_lfanew
data[0x80:0x84] = b'PE\0\0'
struct.pack_into('<H', data, 0x80+24, 0x10b)    # PE32 magic
struct.pack_into('<I', data, 0x80+24+72, 0x100000)  # StackReserve 1MB
struct.pack_into('<I', data, 0x80+24+76, 0x1000)    # StackCommit 4KB
open(p, 'wb').write(data)
PY
}
reserve_of() {
  python3 - "$1" <<'PY'
import struct, sys
d = open(sys.argv[1], 'rb').read()
e = struct.unpack_from('<I', d, 0x3c)[0]
print(struct.unpack_from('<I', d, e+24+72)[0])
PY
}

make_pe "${INST}/yybins/YYClient.exe"
make_pe "${INST}/11loader.exe"
cp "${INST}/yybins/YYClient.exe" "${WORK}/YYClient.orig"
cp "${INST}/11loader.exe" "${WORK}/11loader.orig"

export E11_WINE11_PREFIX="$PREFIX" E11_INSTALL_DIR="11duizhan" E11_STACK_BACKUP="$BACKUP"

# --- 1. 初始 status：默认 1MB 栈 ------------------------------------------
OUT="$(bash "$FIX" --status 2>&1)"
t_assert_grep <(printf '%s' "$OUT") "默认栈" "status reports default 1MB stack"
t_assert_eq 1048576 "$(reserve_of "${INST}/yybins/YYClient.exe")" "fixture starts at 1MB reserve"

# --- 2. apply：备份 + 补丁到 64MB ----------------------------------------
OUT="$(bash "$FIX" 2>&1)"
t_assert_grep <(printf '%s' "$OUT") "已补丁" "apply reports patched"
t_assert_eq 67108864 "$(reserve_of "${INST}/yybins/YYClient.exe")" "YYClient reserve grown to 64MB"
t_assert_eq 67108864 "$(reserve_of "${INST}/11loader.exe")" "11loader reserve grown to 64MB"
t_assert_eq 65536 "$(python3 -c "import struct,sys;d=open('${INST}/yybins/YYClient.exe','rb').read();e=struct.unpack_from('<I',d,0x3c)[0];print(struct.unpack_from('<I',d,e+24+76)[0])")" "commit grown to 64KB"
[[ -f "${BACKUP}/YYClient.exe.orig" ]]
t_assert_eq 0 $? "backup created for YYClient.exe"

# --- 3. status：已补丁 ----------------------------------------------------
OUT="$(bash "$FIX" --status 2>&1)"
t_assert_grep <(printf '%s' "$OUT") "已补丁" "status reports patched after apply"

# --- 4. 幂等：再次 apply 不改动 ------------------------------------------
cp "${INST}/yybins/YYClient.exe" "${WORK}/before2"
OUT="$(bash "$FIX" 2>&1)"
t_assert_grep <(printf '%s' "$OUT") "跳过" "second apply is idempotent (skip)"
cmp -s "${INST}/yybins/YYClient.exe" "${WORK}/before2"
t_assert_eq 0 $? "second apply leaves bytes unchanged"

# --- 5. uninstall：精确还原原始字节 --------------------------------------
OUT="$(bash "$FIX" --uninstall 2>&1)"
t_assert_grep <(printf '%s' "$OUT") "已还原" "uninstall reports restored"
cmp -s "${INST}/yybins/YYClient.exe" "${WORK}/YYClient.orig"
t_assert_eq 0 $? "YYClient restored byte-for-byte"
cmp -s "${INST}/11loader.exe" "${WORK}/11loader.orig"
t_assert_eq 0 $? "11loader restored byte-for-byte"
t_assert_eq 1048576 "$(reserve_of "${INST}/yybins/YYClient.exe")" "reserve back to 1MB after uninstall"

# --- 6. 自更新替换 exe：旧备份转存，当前版本重新备份 ----------------------
python3 - "${INST}/yybins/YYClient.exe" "${INST}/11loader.exe" <<'PY'
import sys
for p in sys.argv[1:]:
    d = bytearray(open(p, 'rb').read())
    d[0x40] ^= 0xff  # 模拟厂商自更新改了代码/资源，栈字段仍为默认
    open(p, 'wb').write(d)
PY
cp "${INST}/yybins/YYClient.exe" "${WORK}/YYClient.v2"
cp "${INST}/11loader.exe" "${WORK}/11loader.v2"
OUT="$(bash "$FIX" 2>&1)"
t_assert_grep <(printf '%s' "$OUT") "版本已变化" "self-update triggers backup rotation"
t_assert_eq 67108864 "$(reserve_of "${INST}/yybins/YYClient.exe")" "updated YYClient gets stack patch"
cmp -s "${BACKUP}/YYClient.exe.orig" "${WORK}/YYClient.v2"
t_assert_eq 0 $? "backup now matches updated (unpatched) YYClient"
OUT="$(bash "$FIX" --uninstall 2>&1)"
cmp -s "${INST}/yybins/YYClient.exe" "${WORK}/YYClient.v2"
t_assert_eq 0 $? "uninstall restores updated version after rotation"
cmp -s "${INST}/11loader.exe" "${WORK}/11loader.v2"
t_assert_eq 0 $? "uninstall restores updated loader after rotation"

t_finish "test-11-stack.sh"