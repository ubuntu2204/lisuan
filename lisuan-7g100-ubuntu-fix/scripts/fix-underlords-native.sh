#!/usr/bin/env bash
#
# fix-underlords-native.sh — install/uninstall the Dota Underlords fix set for
# Lisuan (砺算) 7G100 (makes "click Play in Steam" work).
#
# Background (full evidence in docs/vendor-bug-report-gf-gbm-crash.md §4 and
# README "游戏兼容性"):
#   * The game runs in Steam's pressure-vessel container, where the vendor
#     graphics stack is unusable (sandbox hides /sys/bus/pci -> vendor Vulkan
#     ICD enumerates 0 devices; container GLX/EGL cannot claim the screen).
#   * Fix: delegate out of the sandbox — underlords.sh detects the container
#     and calls the HOST's systemd over the shared session bus to run the game
#     natively (host graphics stack works fine), blocking until the game exits
#     so Steam's run-time tracking stays accurate.
#
# What gets installed (all idempotent):
#   1. underlords.sh patched:
#      - container branch -> exec ul-host-run.sh (host delegation)
#      - smart crash retry: segfault -> clean stale /dev/shm/LS_SHARE_MEMORY_*
#        and restart (max 10; 3 crashes within 180s = crash loop -> stop)
#      - quit detection: exit-time teardown also segfaults (same engine bug);
#        if CSteam3Client::Shutdown is in the session log, do NOT restart
#      - -nobreakpad so crashes produce a kernel core for gdb analysis
#        (the bundled breakpad handler swallows SIGSEGV)
#   2. ul-host-run.sh   — container->host delegation via systemd-run --user
#   3. play-native.sh   — native launcher (also usable from the desktop entry)
#   4. libpng12.so.0    — downloaded into bin/linuxsteamrt64/ if missing
#                         (Ubuntu 24.04 removed it; Source2 pango needs it)
#   5. desktop entry    — "Dota Underlords (Lisuan native)"
#
# Usage:
#   sudo ./fix-underlords-native.sh            # install (or repair)
#   sudo ./fix-underlords-native.sh --uninstall
#   ./fix-underlords-native.sh --status
#
# License: Apache-2.0
set -euo pipefail

# ---- locate user & game dir -------------------------------------------------
REAL_USER="${SUDO_USER:-$USER}"
REAL_HOME="$(getent passwd "$REAL_USER" | cut -d: -f6)"
REAL_UID="$(id -u "$REAL_USER")"
GAME_DIR="${1:-}"
case "$GAME_DIR" in --*) GAME_DIR="";; esac
if [[ -z "$GAME_DIR" ]]; then
  GAME_DIR="$REAL_HOME/.local/share/Steam/steamapps/common/Underlords/game"
fi
LAUNCHER="$GAME_DIR/underlords.sh"

log()  { printf '\033[1;32m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m!!\033[0m %s\n'  "$*" >&2; }
die()  { printf '\033[1;31mXX\033[0m %s\n'  "$*" >&2; exit 1; }

MARKER='# --- Lisuan 7G100 fix (injected)'

status() {
  echo "== Dota Underlords 修复状态 =="
  [[ -d "$GAME_DIR" ]] || { echo "  游戏未安装于 $GAME_DIR"; exit 0; }
  if grep -q "$MARKER" "$LAUNCHER" 2>/dev/null; then
    echo "  [OK] underlords.sh 已打补丁（容器委托 + 智能重试 + 退出检测）"
  else
    echo "  [--] underlords.sh 未打补丁"
  fi
  for f in ul-host-run.sh play-native.sh; do
    [[ -x "$GAME_DIR/$f" ]] && echo "  [OK] $f 就绪" || echo "  [--] $f 缺失"
  done
  [[ -f "$GAME_DIR/bin/linuxsteamrt64/libpng12.so.0" ]] \
    && echo "  [OK] libpng12.so.0 就绪" || echo "  [--] libpng12.so.0 缺失"
  [[ -f "$REAL_HOME/.local/share/applications/dota-underlords-native.desktop" ]] \
    && echo "  [OK] 桌面项已安装" || echo "  [--] 桌面项未安装"
}

if [[ "${1:-}" == "--status" ]]; then status; exit 0; fi
[[ $EUID -eq 0 ]] || die "需要 root:  sudo $0 $*"
[[ -d "$GAME_DIR" && -f "$LAUNCHER" ]] || die "未找到游戏: $GAME_DIR"

# ---- uninstall --------------------------------------------------------------
if [[ "${1:-}" == "--uninstall" ]]; then
  if [[ -f "$LAUNCHER.orig" ]]; then
    cp -a "$LAUNCHER.orig" "$LAUNCHER"
    log "underlords.sh 已还原自 underlords.sh.orig"
  else
    warn "无 underlords.sh.orig 备份，无法还原（游戏文件可能已被 Steam 更新覆盖）"
  fi
  rm -f "$GAME_DIR/ul-host-run.sh" "$GAME_DIR/play-native.sh" \
        "$REAL_HOME/.local/share/applications/dota-underlords-native.desktop"
  log "ul-host-run.sh / play-native.sh / 桌面项已移除"
  log "libpng12.so.0 与 *.orig 备份保留（无害）"
  exit 0
fi

# ---- 1. patch underlords.sh --------------------------------------------------
if grep -q "$MARKER" "$LAUNCHER"; then
  log "underlords.sh 已含补丁标记，跳过补丁（如需强制重打：先还原 .orig）"
else
  cp -a "$LAUNCHER" "$LAUNCHER.orig"
  GAME_DIR_ESC="$GAME_DIR" python3 - "$LAUNCHER" <<'PYEOF'
import os, sys
p = sys.argv[1]
g = os.environ['GAME_DIR_ESC']
s = open(p).read()

# 1) container delegation branch right after the shebang
s = s.replace('#!/bin/bash\n', '''#!/bin/bash
# --- Lisuan 7G100 fix (injected): in-container delegation + crash retry ---
if [ -d /usr/lib/pressure-vessel ]; then
  # inside steam runtime container: sandbox hides /sys/bus/pci so the vendor
  # Vulkan ICD finds no GPU here. Delegate to host via session D-Bus instead.
  export GBM_BACKEND=dri   # keep for any in-container GL touches (e.g. overlays)
  exec "''' + g + '''/ul-host-run.sh"
else
  # on host: use vendor glvnd stack directly
  export GBM_BACKEND=dri
fi
export UL_LOG=/tmp/underlords-stderr.log
''', 1)

# 2) smart crash retry (replaces the plain exit-42 restart loop)
old_loop = 'STATUS=42\nwhile [ $STATUS -eq 42 ]; do'
new_loop = '''STATUS=42
CRASH_RETRY=0
CRASH_TIMES=""
# Lisuan fix: auto-restart the game on segfault (139) — this engine has a
# sporadic crash in the panorama text module (heap corruption, random timing,
# driver-independent). Auto-restart keeps a session playable.
# Guards: max 10 restarts per run; if 3 crashes happen within 180s it is a
# crash loop (same content keeps crashing) — bail out instead of looping.
while [ $STATUS -eq 42 ] || { [ $STATUS -eq 139 ] && [ $CRASH_RETRY -lt 10 ]; }; do
        if [ $STATUS -eq 139 ]; then
                # 用户退出时 panorama 析构也会段错误（同一处 bug）；退出前游戏
                # 会打印 CSteam3Client::Shutdown——有该标记说明是"退出时崩"，
                # 不是游戏中崩，不重启。
                if tail -n 200 /tmp/underlords-stderr.log 2>/dev/null | grep -q 'CSteam3Client::Shutdown'; then
                        echo "[lisuan-fix] crash during quit (teardown); not restarting" >&2
                        exit 0
                fi
                now=$(date +%s)
                CRASH_RETRY=$((CRASH_RETRY+1))
                CRASH_TIMES="$CRASH_TIMES $now"
                recent=0
                for t in $CRASH_TIMES; do
                        [ $((now-t)) -le 180 ] && recent=$((recent+1))
                done
                if [ $recent -ge 3 ]; then
                        echo "[lisuan-fix] 3 crashes within 180s — crash loop, giving up" >&2
                        exit 139
                fi
                echo "[lisuan-fix] segfault; cleaning stale shm, restart $CRASH_RETRY/10" >&2
                for f in /dev/shm/LS_SHARE_MEMORY_*; do
                        [ -e "$f" ] || continue
                        pid=${f##*_}
                        kill -0 "$pid" 2>/dev/null || rm -f "$f"
                done
                sleep 3
        fi'''
assert old_loop in s, 'STATUS=42 loop not found (game script changed?)'
s = s.replace(old_loop, new_loop, 1)

# 3) per-session log truncation before each (re)launch
old_dbg = '\tif [ "${GAME_DEBUGGER}" == "gdb" ] || [ "${GAME_DEBUGGER}" == "cgdb" ]; then'
assert old_dbg in s, 'debugger branch not found (game script changed?)'
s = s.replace(old_dbg, '\t: > /tmp/underlords-stderr.log   # per-session log (quit-marker detection)\n' + old_dbg, 1)

# 4) -nobreakpad (kernel core on crash) + per-session stderr capture
old_exec = '"${GAMEROOT}"/${GAMEEXE} "$@"\n\tfi'
new_exec = '"${GAMEROOT}"/${GAMEEXE} "$@" -nobreakpad 2>>/tmp/underlords-stderr.log\n\tfi'
assert old_exec in s, 'game exec line not found (game script changed?)'
s = s.replace(old_exec, new_exec, 1)

open(p, 'w').write(s)
print('patched')
PYEOF
  chmod +x "$LAUNCHER"
  bash -n "$LAUNCHER" || die "补丁后语法检查失败，请检查 $LAUNCHER（备份: $LAUNCHER.orig）"
  log "underlords.sh 已打补丁（备份: underlords.sh.orig）"
fi

# ---- 2. ul-host-run.sh -------------------------------------------------------
cat > "$GAME_DIR/ul-host-run.sh" <<EOF
#!/bin/bash
# Lisuan 7G100: run Dota Underlords NATIVE on the host from inside the Steam
# runtime container.
#
# Why: the pressure-vessel sandbox hides /sys/bus/pci, so the vendor Vulkan
# ICD cannot enumerate the GPU in-container; container GLX/EGL cannot use the
# vendor stack either. The host graphics stack works fine, so we move the
# game out of the sandbox via the host's systemd (reachable through the
# shared session bus). The game runs as a systemd service (not a scope),
# because a scope would put the game in the systemd-run process's cgroup and
# confuse Steam's "game still running" detection. This wrapper instead polls
# the service state and exits when the game exits, so Steam tracks lifetime
# through this wrapper process.
LDSO=/lib64/ld-linux-x86-64.so.2
LIBPATH=/run/host/usr/lib/x86_64-linux-gnu:/run/host/lib/x86_64-linux-gnu:/run/host/usr/lib/x86_64-linux-gnu/systemd
SR() { \$LDSO --library-path "\$LIBPATH" /run/host/usr/bin/systemd-run --user "\$@"; }
SS() { \$LDSO --library-path "\$LIBPATH" /run/host/usr/bin/systemctl --user "\$@"; }
G=$GAME_DIR

# 宿主侧 X 认证文件（Xwayland 会话每次登录随机后缀）
XAUTH=\$(ls /run/host/run/user/$REAL_UID/.mutter-Xwaylandauth.* /run/host/run/user/$REAL_UID/.Xauthority 2>/dev/null | head -1)
XAUTH=\${XAUTH#/run/host}
# 兜底：直接问宿主 systemd 用户环境
if [ -z "\$XAUTH" ]; then
  XAUTH=\$(SS show-environment 2>/dev/null | sed -n 's/^XAUTHORITY=//p')
fi

UNIT="ul-underlords.service"
exec 3>&2 2>>/tmp/ul-hostrun.log    # 内部错误写日志，不污染 Steam 输出

if SS is-active "\$UNIT" >/dev/null 2>&1; then
  echo "[ul-host-run] host game already running, waiting for it to exit" >&2
else
  SR --unit="\$UNIT" --collect \\
    --setenv=HOME=$REAL_HOME \\
    --setenv=DISPLAY=:0 \\
    --setenv=XAUTHORITY="\$XAUTH" \\
    /bin/bash "\$G/play-native.sh" || echo "[ul-host-run] systemd-run failed rc=\$?" >&2
fi

# 轮询服务状态：游戏退出后立即返回（Steam 据此结束"运行中"状态）
sleep 5
while SS is-active "\$UNIT" 2>/dev/null | grep -q '^active\$'; do
  sleep 3
done
SS reset-failed "\$UNIT" 2>/dev/null
echo "[ul-host-run] host game exited" >&2
exit 0
EOF
chmod +x "$GAME_DIR/ul-host-run.sh"
bash -n "$GAME_DIR/ul-host-run.sh" || die "ul-host-run.sh 生成异常"
log "ul-host-run.sh 已安装"

# ---- 3. play-native.sh --------------------------------------------------------
cat > "$GAME_DIR/play-native.sh" <<EOF
#!/bin/bash
# Dota Underlords — native launch for Lisuan 7G100 (bypasses Steam runtime
# container, whose GLX cannot load the vendor driver). Steam client must be
# running for auth. Only ONE instance is allowed — a second instance fails
# with "InitSteamNetworking(): Failed to initialize SteamNetworkingSockets".
if pgrep -f '^$GAME_DIR/bin/linuxsteamrt64/underlords ' >/dev/null 2>&1; then
  echo "Dota Underlords is already running." >&2
  exit 1
fi
# 清理被强杀游戏遗留的 LS_SHARE_MEMORY_*（新实例附着死 shm 会在 tier0 段错误）
for f in /dev/shm/LS_SHARE_MEMORY_*; do
  [ -e "\$f" ] || continue
  pid=\${f##*_}
  kill -0 "\$pid" 2>/dev/null || rm -f "\$f"
done

G="\$(cd "\$(dirname "\$0")" && pwd)"
export STEAM_RUNTIME=0
export GBM_BACKEND=dri
exec "\$G/underlords.sh" -steam -vulkan
EOF
chmod +x "$GAME_DIR/play-native.sh"
bash -n "$GAME_DIR/play-native.sh" || die "play-native.sh 生成异常"
log "play-native.sh 已安装"

# ---- 4. libpng12 --------------------------------------------------------------
PNG="$GAME_DIR/bin/linuxsteamrt64/libpng12.so.0"
if [[ -f "$PNG" ]]; then
  log "libpng12.so.0 已存在，跳过"
else
  log "下载 libpng12.so.0（Ubuntu 24.04 已移除该库，Source 2 pango 需要）..."
  if curl -sf --max-time 120 -o /tmp/libpng12.deb \
       http://mirrors.aliyun.com/ubuntu/pool/main/libp/libpng/libpng12-0_1.2.54-1ubuntu1.1_amd64.deb; then
    dpkg -x /tmp/libpng12.deb /tmp/libpng12-x
    cp -aL /tmp/libpng12-x/lib/x86_64-linux-gnu/libpng12.so.0.* "$PNG"
    rm -rf /tmp/libpng12.deb /tmp/libpng12-x
    log "libpng12.so.0 已安装到 $PNG"
  else
    warn "下载失败（无网络?）。手动从 Ubuntu 18.04 归档获取 libpng12-0 并把"
    warn "libpng12.so.0 放到 $GAME_DIR/bin/linuxsteamrt64/"
  fi
fi

# ---- 5. desktop entry ---------------------------------------------------------
DESK="$REAL_HOME/.local/share/applications/dota-underlords-native.desktop"
ICON="$(ls "$REAL_HOME/.local/share/Steam/appcache/librarycache/1046930"/*icon* 2>/dev/null | head -1)"
cat > "$DESK" <<EOF
[Desktop Entry]
Name=Dota Underlords (Lisuan native)
Comment=Bypass Steam runtime container for Lisuan 7G100 (host graphics stack)
Exec=$GAME_DIR/play-native.sh
Type=Application
Terminal=false
Categories=Game;
EOF
[[ -n "$ICON" ]] && printf 'Icon=%s\n' "$ICON" >> "$DESK"
chown "$REAL_USER:" "$DESK"
log "桌面项已安装: $DESK"

log "全部完成。Steam 内点\"开始游戏\"即可（委托到宿主直跑）。"
log "状态检查: $0 --status   卸载: sudo $0 --uninstall"
