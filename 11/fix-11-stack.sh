#!/usr/bin/env bash
# 至砺算：11 对战平台启动栈溢出修复，与显卡无关。
#
# 根因（2026-11-18 取证）：平台真正的客户端是 yybins/YYClient.exe（由
# 11loader.exe 拉起，命令行带 -lnchwnd=...）。其频道序列化函数递归帧约 4KB
# （sub $0xf80,%esp），而 PE 默认线程栈保留仅 1MB（SizeOfStackReserve=0x100000），
# 约 250 层递归即触守护页 → err:virtual "stack overflow ... addr 0x48de11"，
# 主线程异常退出（platform.ini error_code=2），表现为黑屏 / 开启卡死。
#
# 修复：把 YYClient.exe 与 11loader.exe 的 PE 栈保留改 16MB、提交改 64KB，
# 让递归有足够栈空间完成（已实测栈溢出消失、客户端越过原崩溃点）。
# 只改 PE 可选头两个 DWORD，不动代码段；改前备份原始文件，--uninstall 可还原。
# 厂商二进制不入库，补丁作用于本机前缀内的安装产物。
#
# 用法：
#   ./11/fix-11-stack.sh              # 备份并打栈补丁（幂等）
#   ./11/fix-11-stack.sh --status     # 只读体检：各 exe 当前栈设置
#   ./11/fix-11-stack.sh --uninstall  # 从备份还原原始 exe
#   ./11/fix-11-stack.sh --help
#
# 环境变量：E11_WINE11_PREFIX / E11_INSTALL_DIR / E11_STACK_RESERVE（默认 16MB）/
#   E11_STACK_COMMIT（默认 64KB）/ E11_STACK_BACKUP
# License: Apache-2.0
set -euo pipefail

PREFIX="${E11_WINE11_PREFIX:-$HOME/.deepinwine/11duizhan-wine11}"
PREFIX="${PREFIX/#\~/$HOME}"
INSTALL_DIR_REL="${E11_INSTALL_DIR:-11duizhan}"
INSTALL_DIR_REL="${INSTALL_DIR_REL#/}"
DRIVE_C="$PREFIX/drive_c/$INSTALL_DIR_REL"
RESERVE="${E11_STACK_RESERVE:-16777216}"   # 16MB
COMMIT="${E11_STACK_COMMIT:-65536}"        # 64KB
BACKUP_DIR="${E11_STACK_BACKUP:-$HOME/.local/share/11duizhan-wine11/stack-backups}"
# 相对安装目录的目标 PE（真正的客户端 + 拉起器）。
TARGETS=("yybins/YYClient.exe" "11loader.exe")

usage() {
    cat <<EOF
用法：$(basename "$0") [--status|--uninstall|--help]

  （无参数）   备份并把 YYClient.exe / 11loader.exe 的 PE 栈保留改 16MB（幂等）
  --status     只读体检：各目标 exe 的 SizeOfStackReserve / Commit 与备份状态
  --uninstall  从备份还原原始 exe
  --help       显示本说明

环境变量：E11_WINE11_PREFIX / E11_INSTALL_DIR / E11_STACK_RESERVE /
          E11_STACK_COMMIT / E11_STACK_BACKUP
备份目录：$BACKUP_DIR
说明：本修复只解决启动栈溢出；GDI 句柄泄漏导致的后续退出见 README.md 第 7 节。
EOF
}

# PE 栈操作统一由内嵌 python 完成：status / patch / restore。
pe_tool() {
    python3 - "$@" <<'PY'
import struct, sys, os, shutil
mode, path, reserve, commit, backup = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4]), sys.argv[5]

def read_pe(data):
    if data[:2] != b'MZ':
        raise SystemExit(f"不是 MZ/PE：{path}")
    e = struct.unpack_from('<I', data, 0x3c)[0]
    if data[e:e+4] != b'PE\0\0':
        raise SystemExit(f"不是有效 PE：{path}")
    magic = struct.unpack_from('<H', data, e+24)[0]
    if magic != 0x10b:
        raise SystemExit(f"仅支持 PE32（0x10b），实际 {magic:#x}：{path}")
    return e

if mode == 'status':
    if not os.path.isfile(path):
        print(f"  [--] 缺失：{path}"); sys.exit(1)
    data = open(path, 'rb').read()
    e = read_pe(data)
    res, com = struct.unpack_from('<II', data, e+24+72)
    bak = os.path.join(backup, os.path.basename(path) + '.orig')
    tag = "已补丁" if res >= reserve else "默认栈"
    hasbak = "有备份" if os.path.isfile(bak) else "无备份"
    print(f"  [OK] {os.path.basename(path)}：栈保留 {res//1024}KB 提交 {com//1024}KB（{tag}，{hasbak}）")
    sys.exit(0)

if mode == 'patch':
    data = bytearray(open(path, 'rb').read())
    e = read_pe(data)
    res, com = struct.unpack_from('<II', data, e+24+72)
    if res >= reserve and com >= commit:
        print(f"  [=] {os.path.basename(path)}：已是 {res//1024}KB 栈，跳过"); sys.exit(0)
    os.makedirs(backup, exist_ok=True)
    bak = os.path.join(backup, os.path.basename(path) + '.orig')
    if not os.path.isfile(bak):
        shutil.copy2(path, bak)
        print(f"  已备份：{path} -> {bak}")
    struct.pack_into('<I', data, e+24+72, reserve)
    struct.pack_into('<I', data, e+24+76, commit)
    with open(path, 'wb') as f:
        f.write(data)
    print(f"  已补丁：{os.path.basename(path)} 栈保留 {res//1024}KB -> {reserve//1024}KB，提交 {com//1024}KB -> {commit//1024}KB")
    sys.exit(0)

if mode == 'restore':
    bak = os.path.join(backup, os.path.basename(path) + '.orig')
    if not os.path.isfile(bak):
        print(f"  [--] 无备份，跳过：{path}"); sys.exit(0)
    shutil.copy2(bak, path)
    print(f"  已还原：{bak} -> {path}")
    sys.exit(0)

raise SystemExit(f"未知模式：{mode}")
PY
}

status() {
    local rc=0 t
    [[ -d "$DRIVE_C" ]] || { echo "  [--] 安装目录不存在：$DRIVE_C（请先安装平台）"; return 1; }
    for t in "${TARGETS[@]}"; do
        pe_tool status "$DRIVE_C/$t" "$RESERVE" "$COMMIT" "$BACKUP_DIR" || rc=1
    done
    return "$rc"
}

apply() {
    local t
    [[ -d "$DRIVE_C" ]] || { echo "错误：安装目录不存在：$DRIVE_C（请先运行 install-11duizhan.sh）" >&2; exit 1; }
    echo "[1/1] 备份并增大 PE 栈保留（解决启动栈溢出）"
    for t in "${TARGETS[@]}"; do
        [[ -f "$DRIVE_C/$t" ]] || { echo "错误：目标缺失：$DRIVE_C/$t" >&2; exit 1; }
        pe_tool patch "$DRIVE_C/$t" "$RESERVE" "$COMMIT" "$BACKUP_DIR"
    done
    echo "  完成。体检：$(basename "$0") --status"
    echo "  注意：本修复只解决启动栈溢出；GDI 句柄泄漏的后续退出尚未修复，见 README.md 第 7 节。"
}

uninstall() {
    local t
    for t in "${TARGETS[@]}"; do
        pe_tool restore "$DRIVE_C/$t" "$RESERVE" "$COMMIT" "$BACKUP_DIR"
    done
    echo "  已从备份还原；备份文件保留在 $BACKUP_DIR。"
}

case "${1:-}" in
    --help|-h) usage; exit 0 ;;
    --status) status; exit $? ;;
    --uninstall) uninstall; exit 0 ;;
    --*) echo "错误：未知参数：$1（用 --help 查看用法）" >&2; exit 1 ;;
    *) apply ;;
esac