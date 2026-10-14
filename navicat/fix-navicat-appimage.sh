#!/usr/bin/env bash
#
# fix-navicat-appimage.sh — Navicat 18 Premium Lite（AppImage）启动闪退修复（幂等）
#
# 症状：双击（或终端运行）navicat18-premium-lite-cs-x86_64.AppImage，
#       "注册"对话框闪现不到一秒即段错误退出（核心转储 / SIGSEGV）。
#
# 根因（2026-10-14 实测，完整证据链见同目录 README.md）：
#   AppImage 自带 Qt 6.8.3，但内置输入法插件
#   usr/plugins/platforminputcontexts/libfcitxplatforminputcontextplugin-qt6.so
#   是按 Qt 6.5 编译的（要求 Qt_6.5 符号版本）。本机会话跑的是 fcitx5，
#   登录环境里 QT_IM_MODULE=fcitx + XMODIFIERS=@im=fcitx：
#     1) Qt 按 QT_IM_MODULE=fcitx 加载该插件；Qt 6.8.3 仍提供 Qt_6.5
#        版本节点，动态链接器不报错，插件"加载成功"；
#     2) 插件内部却按 6.5 的 QObjectPrivate 等内部结构构造对象，
#        与 6.8.3 运行时布局不一致 → d 指针错位 → SIGSEGV；
#     3) 崩溃时机正是注册对话框获得焦点的一瞬（QGuiApplication 焦点事件
#        → 输入法插件），表现为"闪退"。
#   gdb 栈：QObject::thread ← QObject 构造 ← fcitx 插件 ← 焦点事件。
#   该插件是包内唯一输入法插件且已损坏，只能切 Qt 内置 compose 输入模块。
#
# 修复：重打包 AppImage —— runtime ELF 原样保留，仅向 squashfs 内的 AppRun
#       注入 export QT_IM_MODULE=compose，再按原压缩器/块大小 mksquashfs
#       重打包。双击、应用菜单、终端三种启动方式全部生效；
#       不删任何文件，原包自动备份为 .bak（仅首次），可 --restore 回退。
#
# Usage:
#   ./fix-navicat-appimage.sh [AppImage路径]  # 修复（默认 ~/development/ 下）
#   ./fix-navicat-appimage.sh --status        # 查看状态
#   ./fix-navicat-appimage.sh --test          # 冒烟测试：启动 10 秒看是否存活
#   ./fix-navicat-appimage.sh --restore       # 从 .bak 还原原包
#   ./fix-navicat-appimage.sh --keep-work     # 保留解包工作目录（排查用）
#   NAVICAT_APPIMAGE=/path/x.AppImage ./fix-navicat-appimage.sh
#
# 依赖：mksquashfs / unsquashfs（sudo apt-get install squashfs-tools）
#
# License: Apache-2.0
set -euo pipefail

DEFAULT_APPIMAGE="$HOME/development/navicat18-premium-lite-cs-x86_64.AppImage"
# 功能不变量：AppRun 里出现这一行（anchored，排除注释提及）即视为已修复。
# 不用整句注释做标记——手工/旧版脚本修过的包注释文字不同，也应被识别为已修复。
FIXLINE='export QT_IM_MODULE=compose'

log()  { printf '\033[1;32m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m!!\033[0m %s\n'  "$*" >&2; }
die()  { printf '\033[1;31mXX\033[0m %s\n'  "$*" >&2; exit 1; }

usage() {
  sed -n '2,36p' "$0" | sed 's/^# \?//'
}

APPIMAGE="${NAVICAT_APPIMAGE:-$DEFAULT_APPIMAGE}"
MODE=fix
KEEP_WORK=0
# 解包工作目录（全局，供 EXIT trap 清理；do_fix 内赋值，不能用 local）
WORK=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --status)     MODE=status ;;
    --restore)    MODE=restore ;;
    --test)       MODE="test" ;;   # 引号必需：test 是内建命令名，shellcheck SC2209
    --keep-work)  KEEP_WORK=1 ;;
    -h|--help)    usage; exit 0 ;;
    --*)          die "未知参数: $1（--help 看用法）" ;;
    *)            APPIMAGE="$1" ;;
  esac
  shift
done

command -v unsquashfs >/dev/null 2>&1 || die "缺少依赖: unsquashfs（sudo apt-get install squashfs-tools）"
command -v mksquashfs  >/dev/null 2>&1 || die "缺少依赖: mksquashfs（sudo apt-get install squashfs-tools）"

[[ -f "$APPIMAGE" ]] || die "未找到 AppImage: $APPIMAGE
  （默认路径是 $DEFAULT_APPIMAGE；可用首个参数或 NAVICAT_APPIMAGE 环境变量指定）"

# ---- squashfs 定位 ------------------------------------------------------------
# AppImage type2 = [runtime ELF][squashfs]。runtime 内部数据里也可能出现
# 'hsqs' 魔数（假阳性），逐个候选交给 unsquashfs -s 验真，取第一个有效的。
find_squashfs_offset() {
  local f="$1" off out
  while read -r off; do
    off="${off%%:*}"
    [[ -n "$off" ]] || continue
    out="$(unsquashfs -s -o "$off" "$f" 2>/dev/null || true)"
    if [[ "$out" == *"Found a valid SQUASHFS"* ]]; then
      printf '%s\n' "$off"
      return 0
    fi
  done < <(grep -abo 'hsqs' "$f" | cut -d: -f1)
  return 1
}

# 读 squashfs 超级块字段（Compression / Block size）
sb_field() { # $1=file $2=offset $3=字段名
  unsquashfs -s -o "$2" "$1" 2>/dev/null \
    | sed -n "s/^$3[[:space:]]\{1,\}\([0-9a-zA-Z]*\).*/\1/p" | head -1
}

# 0 = AppRun 已含 export QT_IM_MODULE=compose（即已修复）
apprun_is_patched() { # $1=文件 $2=偏移
  local tmp
  tmp="$(mktemp -d)"
  if unsquashfs -o "$2" -d "$tmp" -f "$1" AppRun >/dev/null 2>&1 \
     && [[ -f "$tmp/AppRun" ]] \
     && grep -qE "^[[:space:]]*${FIXLINE}([[:space:]]|$)" "$tmp/AppRun" 2>/dev/null; then
    rm -rf "$tmp"
    return 0
  fi
  rm -rf "$tmp"
  return 1
}

navicat_running() {
  pgrep -x navicat >/dev/null 2>&1 && return 0
  pgrep -f 'mount_navica' >/dev/null 2>&1 && return 0
  return 1
}

# ---- 各模式 -------------------------------------------------------------------
status() {
  echo "== Navicat AppImage 修复状态 =="
  echo "  AppImage: $APPIMAGE"
  local off
  off="$(find_squashfs_offset "$APPIMAGE")" || die "不是有效的 AppImage（未找到 squashfs）"
  if apprun_is_patched "$APPIMAGE" "$off"; then
    echo "  [OK] 已修复（AppRun 注入 QT_IM_MODULE=compose）"
  else
    echo "  [--] 未修复（QT_IM_MODULE=fcitx 会话下启动即闪退）"
  fi
  if [[ -f "$APPIMAGE.bak" ]]; then
    echo "  [OK] 原包备份: $APPIMAGE.bak"
  else
    echo "  [--] 无 .bak 备份"
  fi
  if navicat_running; then echo "  Navicat 正在运行"; else echo "  Navicat 未运行"; fi
}

do_fix() {
  local off comp bs
  off="$(find_squashfs_offset "$APPIMAGE")" || die "未找到有效的 squashfs（不是 AppImage？）"
  if apprun_is_patched "$APPIMAGE" "$off"; then
    log "已修复，无需重复操作（--restore 可回退到原包）"
    return 0
  fi

  comp="$(sb_field "$APPIMAGE" "$off" 'Compression')"
  bs="$(sb_field "$APPIMAGE" "$off" 'Block size')"
  case "$comp" in
    zstd|gzip|xz|lzo|lz4) ;;
    *) die "不支持的 squashfs 压缩器: '$comp'（本脚本只认 zstd/gzip/xz/lzo/lz4）" ;;
  esac
  [[ "$bs" =~ ^[0-9]+$ ]] || die "解析块大小失败: '$bs'"

  # 工作目录放在 AppImage 同目录：与目标同文件系统（合成/替换都是同 fs rename，
  # 快且原子），且避开 /tmp 可能是 tmpfs（吃内存）的情况。
  WORK="$(mktemp -d "$(dirname "$APPIMAGE")/.navicat-fix.XXXXXX")"
  if [[ "$KEEP_WORK" -eq 1 ]]; then
    log "工作目录（--keep-work，不会自动清理）: $WORK"
  else
    # shellcheck disable=SC2064  # 故意用单引号，trap 触发时再展开 $WORK
    trap '[ -n "$WORK" ] && rm -rf "$WORK"' EXIT
  fi

  log "提取 runtime（前 $off 字节）…"
  head -c "$off" "$APPIMAGE" > "$WORK/runtime"

  log "解包 squashfs（压缩包约 350 MB，请耐心）…"
  unsquashfs -o "$off" -d "$WORK/root" -f "$APPIMAGE" >/dev/null
  [[ -f "$WORK/root/AppRun" ]] || die "解包结果里没有 AppRun（AppImage 结构异常？）"

  log "给 AppRun 注入 QT_IM_MODULE=compose…"
  cat > "$WORK/inject.txt" <<'EOF'

# --- Lisuan fix: QT_IM_MODULE=compose ---------------------------------------
# 启动即段错误修复（2026-10-14）：内置
# libfcitxplatforminputcontextplugin-qt6.so 按 Qt 6.5 编译，与自带 Qt 6.8.3
# 的 QObject 内部结构不兼容；会话 QT_IM_MODULE=fcitx 时被加载即 SIGSEGV。
# 该插件是本包唯一输入法插件且已损坏，强制切换 compose 输入模块。
export QT_IM_MODULE=compose
EOF
  # 注意：用 [ \t] 而非 [[:space:]]——Ubuntu 默认 awk 是 mawk，不支持 POSIX 字符类
  awk -v inj="$WORK/inject.txt" '
    !done && /^[ \t]*exec[ \t]+-a/ {
      while ((getline line < inj) > 0) print line
      close(inj)
      done = 1
    }
    { print }
  ' "$WORK/root/AppRun" > "$WORK/root/AppRun.patched"
  grep -qE "^[[:space:]]*${FIXLINE}([[:space:]]|$)" "$WORK/root/AppRun.patched" \
    || die "注入失败：AppRun 里找不到 exec -a 行（结构异常，请加 --keep-work 检查）"
  chmod --reference="$WORK/root/AppRun" "$WORK/root/AppRun.patched"
  mv -f "$WORK/root/AppRun.patched" "$WORK/root/AppRun"

  log "重新打包 squashfs（$comp，块大小 $bs）…"
  mksquashfs "$WORK/root" "$WORK/new.squashfs" -noappend -root-owned \
             -comp "$comp" -b "$bs" -processors "$(nproc)" >/dev/null

  log "合成新 AppImage…"
  local out
  out="${APPIMAGE}.new.$$"
  cat "$WORK/runtime" "$WORK/new.squashfs" > "$out"
  chmod --reference="$APPIMAGE" "$out"

  # 仅首次备份，保留 pristine 原件；重复修复不覆盖旧备份
  if [[ ! -f "$APPIMAGE.bak" ]]; then
    cp -p "$APPIMAGE" "$APPIMAGE.bak"
    log "原包已备份: $APPIMAGE.bak"
  else
    log ".bak 已存在，保留原备份（不覆盖）"
  fi

  mv -f "$out" "$APPIMAGE"
  log "替换完成: $APPIMAGE"

  # 收尾自检：新文件必须是"有效 AppImage 且 AppRun 已注入"
  local off2
  off2="$(find_squashfs_offset "$APPIMAGE")" || die "新文件异常：找不到 squashfs"
  apprun_is_patched "$APPIMAGE" "$off2" || die "新文件异常：AppRun 未见注入标记"
  log "[OK] 修复完成（自检通过）"
}

do_restore() {
  [[ -f "$APPIMAGE.bak" ]] || die "没有找到备份: $APPIMAGE.bak（无可回退）"
  navicat_running && warn "Navicat 正在运行，还原不影响已启动的实例（它持有旧 inode）"
  mv -f "$APPIMAGE.bak" "$APPIMAGE"
  chmod +x "$APPIMAGE"
  log "[OK] 已还原原包: $APPIMAGE"
}

do_test() {
  local off rc
  off="$(find_squashfs_offset "$APPIMAGE")" || die "不是有效的 AppImage"
  if ! apprun_is_patched "$APPIMAGE" "$off"; then
    warn "当前是未修复的原包，预期会闪退（先跑一次无参修复再看）"
  fi
  navicat_running && warn "已有 Navicat 实例在运行，单实例机制可能干扰测试判读"
  log "冒烟测试：启动 10 秒（正常应弹出「注册」窗口且进程存活）…"
  rc=0
  timeout 10 "$APPIMAGE" >/dev/null 2>&1 || rc=$?
  case "$rc" in
    124) log "[OK] 10 秒未退出，GUI 正常存活" ;;
    139) die  "段错误（SIGSEGV，rc=139）——仍是坏的" ;;
    134) die  "SIGABRT（rc=134）——异常" ;;
    0)   warn "10 秒内自行退出（rc=0）；若刚才有其它实例在跑属正常，否则可疑" ;;
    *)   warn "退出码 $rc" ;;
  esac
}

case "$MODE" in
  status)  status ;;
  restore) do_restore ;;
  test)    do_test ;;
  fix)     do_fix ;;
esac
