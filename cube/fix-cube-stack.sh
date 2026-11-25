#!/usr/bin/env bash
# 至砺算：方块游戏平台（Cube Game）CEF 栈溢出修复，与显卡无关。
#
# 当前状态（2026-11-25）：已不再是现行方案，默认不要执行。
# 实测先做 fix-cube-fonts.sh（系统字体改 Arial）并在启动器里用
# WINEDLLOVERRIDES=windows.ui=n 后，栈溢出与 UISettings 递归均为 0，
# 两个 PE 已用 --uninstall 还原为原厂栈设置，界面正常渲染。
# 本脚本保留仅作历史取证/A-B 对照；若以后平台自更新再次出现
# "stack overflow ... uisettings2_get_TextScaleFactor"，可临时重跑。
#
# 根因（2026-11-25 复现，方块平台 4.1.2.1）：
# ui/cubelauncher.exe（CEF 浏览器，PE32+）在启动阶段经 Wine 的
# IUISettings2::GetTextScaleFactor（Wine 侧为 stub）触发约 5230 层递归，
# 每层约 1600 字节，合计约 8.4MB；其 PE 栈保留仅 8MB（提交 4KB），
# 恰好触守护页：
#   err:virtual "stack overflow 1920 bytes addr 0x6ffffff93b62
#                  stack 0x120880 (0x120000-0x121000-0x920000)"
# 浏览器主线程退出 → 无渲染进程、窗口始终未映射 → 平台无界面。
# 与 11 平台 login 回调递归同类：递归深度有限，栈不够才崩。
#
# 修复：把目标 PE 的栈保留改 64MB、提交改 64KB，让递归跑完。
# 只改 PE 可选头两个 DWORD，不动代码段；备份按忽略栈字段的文件哈希识别版本，
# 版本变化时旧备份转存为 .orig.<hash8>，--uninstall 可还原。
# 厂商二进制不入库，补丁作用于本机前缀内的安装产物。不做看门狗/自动重启。
#
# 用法：
#   ./cube/fix-cube-stack.sh              # 备份并打栈补丁（幂等）
#   ./cube/fix-cube-stack.sh --status     # 只读体检：目标 exe 当前栈设置
#   ./cube/fix-cube-stack.sh --uninstall  # 从备份还原原始 exe
#   ./cube/fix-cube-stack.sh --help
#
# 环境变量：CUBE_PREFIX / CUBE_INSTALL_REL / CUBE_STACK_RESERVE（默认 64MB）/
#   CUBE_STACK_COMMIT（默认 64KB）/ CUBE_STACK_BACKUP
# License: Apache-2.0
set -euo pipefail

PREFIX="${CUBE_PREFIX:-$HOME/.deepinwine/cube-4121}"
PREFIX="${PREFIX/#\~/$HOME}"
INSTALL_REL="${CUBE_INSTALL_REL:-Program Files (x86)/Cube/Cube Platform}"
DRIVE_C="$PREFIX/drive_c/$INSTALL_REL"
RESERVE="${CUBE_STACK_RESERVE:-67108864}"   # 64MB
COMMIT="${CUBE_STACK_COMMIT:-65536}"        # 64KB
BACKUP_DIR="${CUBE_STACK_BACKUP:-$HOME/.local/share/$(basename "$PREFIX")/stack-backups}"

# 溢出发生在 CEF 浏览器进程；主程序栈同样偏小，一并放宽。
TARGETS=("ui/cubelauncher.exe" "Cube.exe")

usage() {
    cat <<EOF
用法：$(basename "$0") [--status|--uninstall|--help]

  （无参数）   备份并把目标 PE 的栈保留改 64MB（幂等；版本变化时
               旧备份转存后再备份当前版本）
  --status     只读体检：目标 exe 的 SizeOfStackReserve / Commit 与备份状态
  --uninstall  从备份还原原始 exe
  --help       显示本说明

安装布局：$INSTALL_REL
目标：    ${TARGETS[*]}
环境变量：CUBE_PREFIX / CUBE_INSTALL_REL / CUBE_STACK_RESERVE /
          CUBE_STACK_COMMIT / CUBE_STACK_BACKUP
备份目录：$BACKUP_DIR
说明：本修复只解决栈溢出；其它异常按 .codebuddy/rules/崩溃处理约定.md 取证。
EOF
}

# PE 栈操作统一由内嵌 python 完成：status / patch / restore。
# 同时支持 PE32(0x10b) 与 PE32+(0x20b)：后者 ImageBase 为 8 字节，
# 栈保留/提交偏移分别为 +72 / +80。
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
    if magic not in (0x10b, 0x20b):
        raise SystemExit(f"仅支持 PE32/PE32+，实际 {magic:#x}：{path}")
    # 栈保留都在可选头 +72（PE32/PE32+ 同址，PE32+ 为 8 字节字段）；
    # 栈提交 PE32 在 +76、PE32+ 在 +80（PE32+ 同样是 8 字节字段）。
    opt = e + 24
    commit_off = opt + (76 if magic == 0x10b else 80)
    return opt + 72, commit_off

def version_hash(data, res_off, com_off):
    """忽略两个栈 DWORD 的文件哈希：同一版本重打补丁不变，自更新后变化。"""
    d = bytearray(data)
    struct.pack_into('<I', d, res_off, 0)
    struct.pack_into('<I', d, com_off, 0)
    return hashlib.sha256(d).hexdigest()

if mode == 'status':
    if not os.path.isfile(path):
        print(f"  [--] 缺失：{path}"); sys.exit(1)
    data = open(path, 'rb').read()
    res_off, com_off = read_pe(data)
    res = struct.unpack_from('<I', data, res_off)[0]
    com = struct.unpack_from('<I', data, com_off)[0]
    bak = os.path.join(backup, os.path.basename(path) + '.orig')
    tag = "已补丁" if res >= reserve else "默认栈"
    hasbak = "有备份" if os.path.isfile(bak) else "无备份"
    print(f"  [OK] {os.path.basename(path)}：栈保留 {res//1024}KB 提交 {com//1024}KB（{tag}，{hasbak}）")
    sys.exit(0)

if mode == 'patch':
    data = bytearray(open(path, 'rb').read())
    res_off, com_off = read_pe(data)
    res = struct.unpack_from('<I', data, res_off)[0]
    com = struct.unpack_from('<I', data, com_off)[0]
    if res >= reserve and com >= commit:
        print(f"  [=] {os.path.basename(path)}：已是 {res//1024}KB 栈，跳过"); sys.exit(0)
    os.makedirs(backup, exist_ok=True)
    bak = os.path.join(backup, os.path.basename(path) + '.orig')
    cur_ver = version_hash(data, res_off, com_off)
    if not os.path.isfile(bak):
        shutil.copy2(path, bak)
        print(f"  已备份：{path} -> {bak}")
    else:
        old = open(bak, 'rb').read()
        try:
            old_res, old_com = read_pe(old)
            old_ver = version_hash(old, old_res, old_com)
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
    struct.pack_into('<I', data, res_off, reserve)
    struct.pack_into('<I', data, com_off, commit)
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
    [[ -d "$DRIVE_C" ]] || { echo "错误：安装目录不存在：$DRIVE_C（请先运行 install-cube.sh）" >&2; exit 1; }
    echo "[1/1] 备份并增大 PE 栈保留（解决 CEF 启动栈溢出）"
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
    --status) status ;;
    --uninstall) uninstall ;;
    --help|-h) usage ;;
    "") apply ;;
    *) echo "未知参数：$1"; usage; exit 1 ;;
esac
