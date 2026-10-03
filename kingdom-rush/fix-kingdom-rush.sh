#!/usr/bin/env bash
#
# fix-kingdom-rush.sh — 《Kingdom Rush》(AppID 246420) 在砺算 7G100 上的
#                       "Unable to create OpenGL window / GLXBadFBConfig" 修复（幂等）
#
# 症状：Steam 点"开始游戏"后弹窗
#         Unable to create OpenGL window
#         This program requires a graphics card and video drivers which
#         support OpenGL 2.1 or OpenGL ES 2.
#         OpenGL context creation error: Could not create GL context: GLXBadFBConfig
#
# 根因（2026-10-03 实测）：
#   1) Kingdom Rush 是**原生 64 位 LÖVE 游戏**（liblove.so.0 + SDL2 + OpenGL），
#      通过自带的 run.sh 启动。
#   2) Steam 默认把它放进 pressure-vessel 容器（SteamLinuxRuntime）运行——
#      容器内厂商 GLX/EGL 无法认领屏幕（沙箱隐藏 /sys/bus/pci，设备发现路径缺失），
#      回退的 mesa 又给不出游戏请求的 GLXFBConfig → GLXBadFBConfig。
#   3) 宿主的厂商图形栈完全正常：glxinfo 显示 LISUAN 7G100、OpenGL 4.6；
#      在宿主直接跑 run.sh，游戏窗口正常创建、音频正常（实测无报错）。
#   4) 宿主还缺**64 位 libopenal.so.1**（Ubuntu 24.04 默认只装了 i386 版），
#      LÖVE 需要它——用 `apt install libopenal1` 补齐 amd64 版即可。
#
# 修复：绕开容器、委托到宿主直跑（与 underlords 同一套 systemd-run 委托方案）
#       + 补齐 64 位 libopenal。
#
# Usage:
#   sudo ./fix-kingdom-rush.sh            # 安装/修复（打补丁 + 生成委托脚本 + 检查依赖）
#   ./fix-kingdom-rush.sh --status        # 查看修复状态
#   sudo ./fix-kingdom-rush.sh --uninstall# 还原 run.sh、移除委托脚本与桌面项
#   ./fix-kingdom-rush.sh --launch        # 宿主直跑一次（排查用，不写配置）
#
# License: Apache-2.0
set -euo pipefail

REAL_USER="${SUDO_USER:-$USER}"
REAL_HOME="$(getent passwd "$REAL_USER" | cut -d: -f6)"
REAL_UID="$(id -u "$REAL_USER")"
GAME_DIR="${KR_DIR:-$REAL_HOME/.local/share/Steam/steamapps/common/Kingdom Rush}"
LAUNCHER="$GAME_DIR/run.sh"
HOSTRUN="$GAME_DIR/kr-host-run.sh"
APPID=246420

log()  { printf '\033[1;32m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m!!\033[0m %s\n'  "$*" >&2; }
die()  { printf '\033[1;31mXX\033[0m %s\n'  "$*" >&2; exit 1; }

MARKER='# --- Lisuan 7G100 fix (injected)'

has_openal64() {
  # 不用 `ldconfig -p | grep -q`：grep -q 命中即退会触发上游 SIGPIPE，
  # 在 set -o pipefail 下整条管道被判失败（即使已装也误报“缺失”）。
  # 先捕获到变量再做子串判定，彻底避开该陷阱。
  local out; out="$(ldconfig -p 2>/dev/null || true)"
  [[ "$out" == *'libopenal.so.1 (libc6,x86-64)'* ]]
}

status() {
  echo "== Kingdom Rush 修复状态 =="
  [[ -d "$GAME_DIR" ]] || { echo "  游戏未安装于 $GAME_DIR"; exit 0; }
  if grep -q "$MARKER" "$LAUNCHER" 2>/dev/null; then
    echo "  [OK] run.sh 已打补丁（容器委托到宿主直跑）"
  else
    echo "  [--] run.sh 未打补丁（游戏会在容器内跑 → GLXBadFBConfig）"
  fi
  [[ -x "$HOSTRUN" ]] && echo "  [OK] kr-host-run.sh 就绪" || echo "  [--] kr-host-run.sh 缺失"
  has_openal64 && echo "  [OK] 64 位 libopenal.so.1 就绪" \
               || echo "  [--] 缺 64 位 libopenal.so.1（游戏会报打不开音频设备）"
  [[ -f "$REAL_HOME/.local/share/applications/kingdom-rush-lisuan.desktop" ]] \
    && echo "  [OK] 桌面项已安装" || echo "  [--] 桌面项未安装"
  pgrep -f "$GAME_DIR/Kingdom Rush" >/dev/null && echo "  游戏运行中" || echo "  游戏未运行"
}

if [[ "${1:-}" == "--status" ]]; then status; exit 0; fi

if [[ "${1:-}" == "--launch" ]]; then
  [[ -x "$GAME_DIR/Kingdom Rush" ]] || die "未找到游戏: $GAME_DIR"
  log "宿主直跑测试（绕开容器）…"
  export GBM_BACKEND=dri
  exec "$LAUNCHER"
fi

[[ $EUID -eq 0 ]] || die "需要 root:  sudo $0 $*"
[[ -d "$GAME_DIR" && -f "$LAUNCHER" ]] || die "未找到游戏: $GAME_DIR（如安装路径不同，用 KR_DIR=... 指定）"

# ---- uninstall --------------------------------------------------------------
if [[ "${1:-}" == "--uninstall" ]]; then
  if [[ -f "$LAUNCHER.orig" ]]; then
    cp -a "$LAUNCHER.orig" "$LAUNCHER"
    log "run.sh 已还原自 run.sh.orig"
  else
    warn "无 run.sh.orig 备份，无法还原"
  fi
  rm -f "$HOSTRUN" "$REAL_HOME/.local/share/applications/kingdom-rush-lisuan.desktop"
  log "kr-host-run.sh / 桌面项已移除（libopenal1 保留，无害）"
  exit 0
fi

# ---- 1. 补齐 64 位 libopenal ------------------------------------------------
if has_openal64; then
  log "64 位 libopenal.so.1 已存在，跳过"
else
  log "安装 64 位 libopenal1（Ubuntu 24.04 默认只装 i386，LÖVE 需要 amd64）…"
  if apt-get install -y libopenal1; then
    has_openal64 && log "libopenal1:amd64 已安装" || warn "安装后仍未检测到 64 位 libopenal，请手动确认"
  else
    warn "apt 安装失败（无网络?）。手动执行： sudo apt install -y libopenal1"
  fi
fi

# ---- 2. kr-host-run.sh（容器 → 宿主委托）-----------------------------------
cat > "$HOSTRUN" <<EOF
#!/bin/bash
# Lisuan 7G100: 在 Steam pressure-vessel 容器内被调用时，把 Kingdom Rush
# 委托到**宿主**以原生方式运行。
#
# 原因：容器隐藏 /sys/bus/pci，厂商图形栈在容器内无法认领屏幕，游戏请求的
# GLXFBConfig 拿不到 → GLXBadFBConfig。宿主厂商栈正常，故经共享会话总线调用
# 宿主 systemd，把游戏作为 user service 拉起（不用 scope，避免干扰 Steam 的
# "游戏运行中"判定）；本包装进程轮询服务状态，游戏退出后即返回。
LDSO=/lib64/ld-linux-x86-64.so.2
LIBPATH=/run/host/usr/lib/x86_64-linux-gnu:/run/host/lib/x86_64-linux-gnu:/run/host/usr/lib/x86_64-linux-gnu/systemd
SR() { \$LDSO --library-path "\$LIBPATH" /run/host/usr/bin/systemd-run --user "\$@"; }
SS() { \$LDSO --library-path "\$LIBPATH" /run/host/usr/bin/systemctl --user "\$@"; }
# 注意：GAME_DIR 可能含空格（"Kingdom Rush"），此处必须加引号，否则赋值被
# 拆成“执行命令 Rush”、G 变空，\$G/run.sh 退化成 /run.sh → 服务 status=127 秒退。
G="$GAME_DIR"

# 宿主侧 X 认证文件（Xwayland 会话每次登录随机后缀）
XAUTH=\$(ls /run/host/run/user/$REAL_UID/.mutter-Xwaylandauth.* /run/host/run/user/$REAL_UID/.Xauthority 2>/dev/null | head -1)
XAUTH=\${XAUTH#/run/host}
if [ -z "\$XAUTH" ]; then
  XAUTH=\$(SS show-environment 2>/dev/null | sed -n 's/^XAUTHORITY=//p')
fi

UNIT="kr-kingdomrush.service"
exec 3>&2 2>>/tmp/kr-hostrun.log

if SS is-active "\$UNIT" >/dev/null 2>&1; then
  echo "[kr-host-run] host game already running, waiting for it to exit" >&2
else
  # 转发 Steam 注入游戏的启动环境到宿主：否则宿主上 libsteam_api 认不出
  # appID → SteamAPI_Init() failed; no appID found，且
  # SteamAPI_RestartAppIfNecessary() 返回 true（自己调 steam.sh 重拉→ ABRT）。
  # RestartAppIfNecessary 判“是否由 Steam 启动”就是看有无 SteamAppId 环境变量。
  SPID_ENV=""
  [ -n "\${SteamPID:-}" ] && SPID_ENV="--setenv=SteamPID=\$SteamPID"
  SR --unit="\$UNIT" --collect \\
    --setenv=HOME=$REAL_HOME \\
    --setenv=DISPLAY=:0 \\
    --setenv=XAUTHORITY="\$XAUTH" \\
    --setenv=SteamAppId="\${SteamAppId:-$APPID}" \\
    --setenv=SteamGameId="\${SteamGameId:-$APPID}" \\
    \$SPID_ENV \\
    /bin/bash "\$G/run.sh" || echo "[kr-host-run] systemd-run failed rc=\$?" >&2
fi

sleep 5
while SS is-active "\$UNIT" 2>/dev/null | grep -q '^active\$'; do
  sleep 3
done
SS reset-failed "\$UNIT" 2>/dev/null
echo "[kr-host-run] host game exited" >&2
exit 0
EOF
chmod 755 "$HOSTRUN"
bash -n "$HOSTRUN" || die "kr-host-run.sh 生成异常"
log "kr-host-run.sh 已安装"

# ---- 3. 打补丁 run.sh -------------------------------------------------------
if grep -q "$MARKER" "$LAUNCHER"; then
  log "run.sh 已含补丁标记，跳过（如需重打先 --uninstall 或还原 .orig）"
else
  [ -f "$LAUNCHER.orig" ] || cp -a "$LAUNCHER" "$LAUNCHER.orig"
  cat > "$LAUNCHER" <<EOF
#!/bin/bash
$MARKER: 容器内委托到宿主直跑 ---
# Kingdom Rush 是原生 64 位 LÖVE 游戏；在 Steam pressure-vessel 容器内厂商
# GLX 无法认领屏幕 → GLXBadFBConfig。检测到容器环境时改由宿主直跑。
LAUNCH_LOC="\$(cd "\$(dirname "\$(readlink -f "\$0")")" && pwd)"
if [ -d /usr/lib/pressure-vessel ]; then
  exec "\$LAUNCH_LOC/kr-host-run.sh"
fi
# 宿主：使用厂商 glvnd 栈直接运行
export GBM_BACKEND=dri
export LD_LIBRARY_PATH="\${LAUNCH_LOC}:\${LD_LIBRARY_PATH:-}"
exec "\$LAUNCH_LOC/Kingdom Rush" "\$@"
EOF
  chmod 755 "$LAUNCHER"
  bash -n "$LAUNCHER" || die "补丁后语法检查失败，请检查 $LAUNCHER（备份: $LAUNCHER.orig）"
  log "run.sh 已打补丁（备份: run.sh.orig）"
fi

# ---- 4. 桌面项（可选：不经 Steam 直接玩）-----------------------------------
DESK="$REAL_HOME/.local/share/applications/kingdom-rush-lisuan.desktop"
cat > "$DESK" <<EOF
[Desktop Entry]
Name=Kingdom Rush (Lisuan native)
Comment=绕开 Steam 容器、用砺算 7G100 宿主图形栈直跑
Exec="$LAUNCHER"
Path="$GAME_DIR"
Type=Application
Terminal=false
Categories=Game;
EOF
chown "$REAL_USER:" "$DESK"
log "桌面项已安装: $DESK"

log "全部完成。Steam 内点\"开始游戏\"即可（run.sh 检测到容器会自动委托到宿主直跑）。"
log "状态检查: ./fix-kingdom-rush.sh --status   卸载: sudo ./fix-kingdom-rush.sh --uninstall"
