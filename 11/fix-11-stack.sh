#!/usr/bin/env bash
# 至砺算：11 对战平台登录栈溢出修复，与显卡无关。
#
# 根因（2026-11-19 复现，官网正式版 2.6.3.48）：
# 主客户端 bins/11Client.exe 在 onLoginSucceed 回调里递归序列化，函数 0x4836b0
# 每帧 sub $0xfc4,%esp（约 4KB），PE 栈保留仅 0x3e8480（约 4MB），约千层递归
# 即触守护页：err:virtual "stack overflow ... addr 0x4836d2"，主线程退出、
# 应用日志停在 DoWebPageLogin:1，窗口残留为僵尸 CEF 进程 → 卡死。
#
# 历史 3.0 公测版（E11_WINE11_PREFIX=~/.deepinwine/11duizhan-wine11）：
# 真正的客户端是 yybins/YYClient.exe，溢出地址 0x48de11/0x48e441，
# PE 栈保留 1MB；本脚本按安装布局自动选择目标。
#
# 修复：把目标 PE 的栈保留改 64MB、提交改 64KB，让递归完成。
# 16MB 在 Wine 11 下不再溢出，但 Wine 9 A/B 仍在 16MB 溢出，故默认取 64MB。
# 只改 PE 可选头两个 DWORD，不动代码段；备份按忽略栈字段的文件哈希识别版本，
# 版本变化时旧备份转存为 .orig.<hash8>，--uninstall 可还原。
# 厂商二进制不入库，补丁作用于本机前缀内的安装产物。不做看门狗/自动重启。
#
# 用法：
#   ./11/fix-11-stack.sh              # 备份并打栈补丁（幂等）
#   ./11/fix-11-stack.sh --status     # 只读体检：目标 exe 当前栈设置
#   ./11/fix-11-stack.sh --uninstall  # 从备份还原原始 exe
#   ./11/fix-11-stack.sh --help
#
# 环境变量：E11_WINE11_PREFIX / E11_INSTALL_DIR / E11_STACK_RESERVE（默认 64MB）/
#   E11_STACK_COMMIT（默认 64KB）/ E11_STACK_BACKUP
# License: Apache-2.0
set -euo pipefail

PREFIX="${E11_WINE11_PREFIX:-$HOME/.deepinwine/11duizhan-263}"
PREFIX="${PREFIX/#\~/$HOME}"
INSTALL_DIR_REL="${E11_INSTALL_DIR:-11duizhan}"
INSTALL_DIR_REL="${INSTALL_DIR_REL#/}"
DRIVE_C="$PREFIX/drive_c/$INSTALL_DIR_REL"
RESERVE="${E11_STACK_RESERVE:-67108864}"   # 64MB
COMMIT="${E11_STACK_COMMIT:-65536}"        # 64KB
BACKUP_DIR="${E11_STACK_BACKUP:-$HOME/.local/share/$(basename "$PREFIX")/stack-backups}"

# 按安装布局选择目标：正式版只有 bins/11Client.exe 在登录回调溢出；
# 旧 3.0 公测版仍是 YYClient.exe + 11loader.exe。
targets() {
    if [[ -f "$DRIVE_C/bins/11Client.exe" ]]; then
        TARGETS=("bins/11Client.exe")
    else
        TARGETS=("yybins/YYClient.exe" "11loader.exe")
    fi
}
targets

usage() {
    cat <<EOF
用法：$(basename "$0") [--status|--uninstall|--help]

  （无参数）   备份并把目标客户端的 PE 栈保留改 64MB（幂等；版本变化时
               旧备份转存后再备份当前版本）
  --status     只读体检：目标 exe 的 SizeOfStackReserve / Commit 与备份状态
  --uninstall  从备份还原原始 exe
  --help       显示本说明

安装布局：
  正式版 2.6.3.48（默认 ~/.deepinwine/11duizhan-263）：bins/11Client.exe
  3.0 公测版（E11_WINE11_PREFIX=~/.deepinwine/11duizhan-wine11）：
               yybins/YYClient.exe + 11loader.exe

环境变量：E11_WINE11_PREFIX / E11_INSTALL_DIR / E11_STACK_RESERVE /
          E11_STACK_COMMIT / E11_STACK_BACKUP
备份目录：$BACKUP_DIR
说明：本修复只解决栈溢出；其它异常按 .codebuddy/rules/崩溃处理约定.md 取证。
EOF
}

# PE 栈操作统一由内嵌 python 完成：status / patch / restore。
pe_tool() {
    python3 - "$@" <<'PY'
import struct, sys, os, shutil, hashlib, time
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

def version_hash(data, e):
    """忽略两个栈 DWORD 的文件哈希：同一版本重打补丁不变，自更新后变化。"""
    d = bytearray(data)
    struct.pack_into('<II', d, e+24+72, 0, 0)
    return hashlib.sha256(d).hexdigest()

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
    cur_ver = version_hash(data, e)
    if not os.path.isfile(bak):
        shutil.copy2(path, bak)
        print(f"  已备份：{path} -> {bak}")
    else:
        old = open(bak, 'rb').read()
        try:
            old_ver = version_hash(old, read_pe(old))
        except SystemExit:
            old_ver = ''
        if old_ver != cur_ver:
            rotated = f"{bak}.{old_ver[:8] or 'unknown'}"
            if os.path.exists(rotated):
                rotated = f"{rotated}-{int(time.time())}"
            os.replace(bak, rotated)
            print(f"  版本已变化，旧备份转存：{rotated}")
            shutil.copy2(path, bak)
            print(f"  已备份当前版本：{path} -> {bak}")
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
    echo "  注意：本修复只解决栈溢出；其它异常按 .codebuddy/rules/崩溃处理约定.md 取证，不做自动重启。"
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