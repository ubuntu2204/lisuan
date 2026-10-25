#!/usr/bin/env bash
#
# fix-plants-vs-zombies.sh — 《植物大战僵尸 GOTY》(AppID 3590) 在砺算 7G100 上的
#                            硬件渲染修复（幂等）
#
# 症状：Steam 点"开始游戏"后没有任何画面；或在宿主直跑时弹 PopCap 的
#         "FATAL ERROR: Invalid command line parameter: -changedir"
#
# 根因（三层，2026-10-25 实机取证）：
#   1) **容器内没有可用图形栈**：PvZ 是 32 位 Windows 游戏，Steam 用
#      pressure-vessel 容器跑 Proton。容器里的 Mesa 没有宿主
#      /usr/share/drirc.d/01-gf_drv.conf 的 lisuan→gf 映射，也没有 glvnd 厂商
#      GLX vendor → winex11.drv 加载 GL 失败（libGL error: No driver found），
#      随后 Wine 的 explorer.exe 崩溃，游戏进程根本起不来（日志刷到 1.7 GB）。
#   2) **32 位进程拿不到厂商驱动**：厂商只发布 x86_64（gf_icd.x86_64.json、
#      gf_dri.so），传统 wow64 下 32 位进程只能走 32 位 GL/Vulkan → 不存在 →
#      退化成 llvmpipe **软件渲染**（CPU 450–510%）。本方案不接受软渲染。
#   3) **PopCap 启动器参数不兼容**：PlantsVsZombies.exe 先把本体解到
#      %ProgramData%\PopCap Games\PlantsVsZombies\popcapgame1.exe，再用
#      -changedir="<游戏目录>" 拉起本体；本体对该参数任何形式都判非法 →
#      FATAL ERROR 弹窗退出。
#
# 修复（沿用 minion-masters 的 32 位硬件渲染路线）：
#   * **Proton new WoW64**（PROTON_USE_WOW64=1）：32 位 PE 经 wow64 thunk 调
#     **64 位** gf Vulkan —— 不需要厂商提供 i386 驱动；
#   * 前缀必须是 wow64 布局（旧的经典前缀移走重建，见 ensure_wow64_prefix）；
#   * 渲染用 **wined3d + Vulkan 后端**（WINE_D3D_CONFIG=renderer=vulkan）：
#     DXVK 在此不可用 —— DXVK 2.3 没有 d3d8（PvZ 是 D3D8/DirectDraw 游戏），
#     DXVK ≥2.4 硬要求 VK_KHR_maintenance5，而 gf 的 Vulkan 只到 1.3 且缺该扩展；
#   * 抽本体常驻 + **宿主直跑**（cwd=游戏目录，不带 -changedir）；
#   * Steam 内点"开始游戏"：启动选项把命令交给 plants-vs-zombies-steam.sh，
#     容器内检测到沙箱后经会话总线委托宿主 systemd 拉起。
#
# Usage:
#   ./fix-plants-vs-zombies.sh                  # 安装/修复（抽本体 + 部署 + 前缀迁移）
#   ./fix-plants-vs-zombies.sh --status         # 状态
#   ./fix-plants-vs-zombies.sh --launch         # 立即宿主直跑一次（排查用）
#   ./fix-plants-vs-zombies.sh --force          # 游戏更新后重抽本体
#   ./fix-plants-vs-zombies.sh --reset-prefix    # 强制把前缀移走重建（wow64 布局）
#   ./fix-plants-vs-zombies.sh --set-launch-options    # 写入 Steam 启动选项（需先退出 Steam）
#   ./fix-plants-vs-zombies.sh --clear-launch-options
#   ./fix-plants-vs-zombies.sh --uninstall
#
# 环境变量覆盖：
#   PVZ_GAME_DIR     游戏目录（默认 .../steamapps/common/Plants Vs Zombies）
#   PVZ_PROTON       Proton 目录（默认 .../common/Proton 11.0，须支持 new WoW64）
#   PVZ_COMPAT_DATA  compatdata 前缀目录（默认 .../compatdata/3590）
#
# License: Apache-2.0
set -euo pipefail

REAL_USER="${SUDO_USER:-$USER}"
REAL_HOME="$(getent passwd "$REAL_USER" | cut -d: -f6)"
REAL_UID="$(id -u "$REAL_USER")"
STEAM_ROOT="$REAL_HOME/.local/share/Steam"
GAME_DIR="${PVZ_GAME_DIR:-$STEAM_ROOT/steamapps/common/Plants Vs Zombies}"
PROTON_DIR="${PVZ_PROTON:-$STEAM_ROOT/steamapps/common/Proton 11.0}"
COMPAT_DATA="${PVZ_COMPAT_DATA:-$STEAM_ROOT/steamapps/compatdata/3590}"
APPID=3590

# 运行时常驻目录：放在 Steam 安装目录之外，避免"验证文件完整性"把产物删掉
RUN_DIR="$REAL_HOME/.local/share/lisuan-fix/plants-vs-zombies"
SELF_DIR="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
BODY="$RUN_DIR/pvz-game.exe"
HOSTRUN="$RUN_DIR/plants-vs-zombies-host-run.sh"
STEAMWRAP="$RUN_DIR/plants-vs-zombies-steam.sh"
DESK="$REAL_HOME/.local/share/applications/plants-vs-zombies-lisuan.desktop"
# 前缀是否已迁到 new WoW64 布局的标记（放在 pfx 之外，移动前缀不会丢）
WOW64_MARKER="$COMPAT_DATA/.lisuan-pvz-wow64"
STEAM_LAUNCH="\"$RUN_DIR/plants-vs-zombies-steam.sh\" %command%"

log()  { printf '\033[1;32m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m!!\033[0m %s\n'  "$*" >&2; }
die()  { printf '\033[1;31mXX\033[0m %s\n'  "$*" >&2; exit 1; }

# PopCap 启动器解出来的本体位置（Wine 前缀内）
LAUNCHER="$GAME_DIR/PlantsVsZombies.exe"
EXTRACTED="$COMPAT_DATA/pfx/drive_c/ProgramData/PopCap Games/PlantsVsZombies/popcapgame1.exe"

run_as_user() {  # sudo 场景下不要留 root 文件
  if [[ $EUID -eq 0 && "$REAL_USER" != root ]]; then
    sudo -u "$REAL_USER" -- "$@"
  else
    "$@"
  fi
}

status() {
  echo "== 植物大战僵尸 GOTY（AppID $APPID）修复状态 =="
  [[ -d "$GAME_DIR" ]] || { echo "  游戏未安装于 $GAME_DIR"; exit 0; }
  [[ -f "$BODY" ]] && echo "  [OK] 游戏本体已抽取（$BODY）" \
                   || echo "  [--] 游戏本体未抽取（首次安装会自动抽取）"
  [[ -x "$HOSTRUN" ]]   && echo "  [OK] 宿主启动器就绪（new WoW64 + wined3d Vulkan）" \
                       || echo "  [--] 宿主启动器缺失"
  [[ -x "$STEAMWRAP" ]] && echo "  [OK] Steam 入口就绪" || echo "  [--] Steam 入口缺失"
  [[ -f "$DESK" ]]      && echo "  [OK] 桌面项已安装"   || echo "  [--] 桌面项未安装"
  [[ -f "$WOW64_MARKER" ]] && echo "  [OK] 前缀已标记为 new WoW64 布局" \
                          || echo "  [--] 前缀未迁移（32 位进程会退回软件渲染）"
  if grep -q 'plants-vs-zombies-steam.sh' "$STEAM_ROOT"/userdata/*/config/localconfig.vdf 2>/dev/null; then
    echo "  [OK] Steam 启动选项已指向 plants-vs-zombies-steam.sh"
  else
    echo "  [--] Steam 启动选项未设置（Steam 里点开始游戏仍需先设置）"
  fi
  pgrep -f 'pvz-game\.exe' >/dev/null 2>&1 && echo "  游戏运行中" || echo "  游戏未运行"
}

# ---- 抽取游戏本体 -----------------------------------------------------------
# 启动器每次运行都会把本体解到前缀里、退出时删除，所以要在它存活时拷贝。
extract_body() {
  if [[ -f "$BODY" && "${1:-}" != "--force" ]]; then
    log "本体已存在，跳过抽取（要重抽：--force）"
    return 0
  fi
  [[ -x "$PROTON_DIR/proton" ]] || die "找不到 Proton：$PROTON_DIR（用 PVZ_PROTON=... 指定）"
  [[ -f "$LAUNCHER" ]]         || die "找不到启动器：$LAUNCHER"

  mkdir -p "$RUN_DIR"
  log "启动一次 PopCap 启动器以抽取本体（它会弹 FATAL ERROR，属预期，抽完立即结束）…"
  local pid
  (
    cd "$GAME_DIR"
    exec env \
      STEAM_COMPAT_CLIENT_INSTALL_PATH="$STEAM_ROOT" \
      STEAM_COMPAT_DATA_PATH="$COMPAT_DATA" \
      SteamAppId="$APPID" SteamGameId="$APPID" \
      "$PROTON_DIR/proton" waitforexitandrun "$LAUNCHER"
  ) >/tmp/plants-vs-zombies-extract.log 2>&1 &
  pid=$!

  local i
  for i in $(seq 1 40); do
    if [[ -f "$EXTRACTED" ]]; then
      cp -f "$EXTRACTED" "$BODY"
      break
    fi
    sleep 0.5
  done

  kill "$pid" 2>/dev/null || true
  pkill -f 'PlantsVsZombies\.exe' 2>/dev/null || true
  sleep 1
  pkill -f 'popcapgame1\.exe' 2>/dev/null || true

  [[ -s "$BODY" ]] || die "抽取失败：未拿到 $EXTRACTED（日志见 /tmp/plants-vs-zombies-extract.log）"
  run_as_user chmod 755 "$BODY"
  log "本体已抽取：$BODY（$(stat -c %s "$BODY") 字节）"
}

# ---- 前缀迁移：经典 wow64 → new WoW64 ---------------------------------------
# 只有 new WoW64 才能让 32 位 PE 调到 64 位 gf Vulkan；旧前缀沿用经典布局，
# 必须移走让 Proton 重建（与 minion-masters 游戏坑 8 的处理一致）。
ensure_wow64_prefix() {
  if [[ "${1:-}" == "--reset-prefix" ]]; then
    rm -f "$WOW64_MARKER"
  fi
  if [[ -f "$WOW64_MARKER" ]]; then
    log "前缀已是 new WoW64 布局，跳过"
    return 0
  fi
  [[ -d "$COMPAT_DATA/pfx" ]] || { touch "$WOW64_MARKER"; log "尚无前缀，首启将直接按 new WoW64 创建"; return 0; }

  # 先让旧前缀的 wineserver 退出，避免移走仍在使用的目录
  if [[ -x "$PROTON_DIR/files/bin/wineserver" ]]; then
    WINEPREFIX="$COMPAT_DATA/pfx" "$PROTON_DIR/files/bin/wineserver" -k >/dev/null 2>&1 || true
  fi
  sleep 1

  local bak="$COMPAT_DATA/pfx.pre-wow64-$(date +%Y%m%d-%H%M%S)"
  mv "$COMPAT_DATA/pfx" "$bak"
  rm -f "$COMPAT_DATA/pfx.lock"
  touch "$WOW64_MARKER"
  log "经典布局前缀已移到 $(basename "$bak")，下次启动由 Proton 重建为 new WoW64"
  warn "旧前缀内的本地存档/PopCap userdata 不会自动迁移（Steam 云存档不受影响）"
}

# ---- 安装宿主启动器 ---------------------------------------------------------
install_hostrun() {
  cat > "$HOSTRUN" <<EOF
#!/bin/bash
# Lisuan 7G100: 在【宿主】用硬件渲染运行《植物大战僵尸 GOTY》。
#
# 为什么必须在宿主跑：Steam 的 pressure-vessel 容器里 Mesa 找不到 lisuan→gf
# 映射、也没有厂商 glvnd GLX vendor，winex11.drv 连 GL 都加载不了 → Wine 起不来。
#
# 为什么这样就能硬件渲染（参考 minion-masters 的 32 位路线）：
#   * PROTON_USE_WOW64=1 → 32 位 PE 经 wow64 thunk 调用 **64 位** gf Vulkan，
#     绕开"厂商没有 i386 驱动"这个死结（前缀必须是 wow64 布局，见 fix 脚本）；
#   * PROTON_USE_WINED3D=1 + WINE_D3D_CONFIG=renderer=vulkan → 用 wined3d 的
#     Vulkan 后端。PvZ 是 D3D8 游戏：DXVK 2.3 没有 d3d8，DXVK ≥2.4 又硬要求
#     VK_KHR_maintenance5（gf 没有），所以 DXVK 这条路走不通。
#
# 为什么不用 PopCap 启动器：它会给本体传 -changedir="<游戏目录>"，本体判非法
# （FATAL ERROR: Invalid command line parameter），且该参数任何形式都不被接受。
set -u
GAME_DIR="${GAME_DIR}"
PROTON_DIR="${PROTON_DIR}"
COMPAT_DATA="${COMPAT_DATA}"
BODY="${BODY}"
APPID=${APPID}
LOCK=/tmp/plants-vs-zombies-3590.lock

# 单实例：本体只能开一个，否则会互抢存档
exec 9>"\$LOCK"
if ! flock -n 9; then
  echo "植物大战僵尸已在运行（锁 \$LOCK）" >&2
  exit 1
fi

cd "\$GAME_DIR" || { echo "找不到游戏目录: \$GAME_DIR" >&2; exit 1; }
[ -f "\$BODY" ] || { echo "找不到游戏本体: \$BODY（请重跑 fix-plants-vs-zombies.sh）" >&2; exit 1; }

# 32 位 → 64 位 gf Vulkan（new WoW64）；wined3d 的 Vulkan 后端
export PROTON_USE_WOW64=1
export PROTON_USE_WINED3D=1
export WINE_D3D_CONFIG="\${WINE_D3D_CONFIG:-renderer=vulkan}"
export STEAM_COMPAT_CLIENT_INSTALL_PATH="\${STEAM_COMPAT_CLIENT_INSTALL_PATH:-$STEAM_ROOT}"
export STEAM_COMPAT_DATA_PATH="\$COMPAT_DATA"
export SteamAppId="\${SteamAppId:-\$APPID}"
export SteamGameId="\${SteamGameId:-\$APPID}"

exec "\$PROTON_DIR/proton" waitforexitandrun "\$BODY"
EOF
  chmod 755 "$HOSTRUN"
  bash -n "$HOSTRUN" || die "宿主启动器生成异常"
  log "宿主启动器已安装：$HOSTRUN"
}

# ---- 安装 Steam 入口（容器 → 宿主委托）-------------------------------------
install_steamwrap() {
  cat > "$STEAMWRAP" <<EOF
#!/bin/bash
# Lisuan 7G100: Steam 里点"开始游戏"时由启动选项调用的入口。
#
# Steam 会用 pressure-vessel 容器包住整条命令，而容器内没有可用图形栈，
# 因此这里检测到沙箱后，经共享会话总线请宿主 systemd 拉起宿主启动器，
# 并阻塞到游戏退出（--wait 回传退出码），Steam 的运行时长统计依然准确。
# 不在容器（桌面项 / 终端直接跑）时就直接执行宿主启动器。
set -u
SELF_DIR="\$(cd "\$(dirname "\$(readlink -f "\$0")")" && pwd)"
HOSTRUN="\$SELF_DIR/plants-vs-zombies-host-run.sh"

if [ ! -d /run/host ]; then
  exec "\$HOSTRUN"
fi

LDSO=/lib64/ld-linux-x86-64.so.2
LIBPATH=/run/host/usr/lib/x86_64-linux-gnu:/run/host/lib/x86_64-linux-gnu:/run/host/usr/lib/x86_64-linux-gnu/systemd
SR() { \$LDSO --library-path "\$LIBPATH" /run/host/usr/bin/systemd-run --user "\$@"; }

# 宿主侧 X 认证文件（Xwayland 会话每次登录随机后缀）
XAUTH=\$(ls /run/host/run/user/${REAL_UID}/.mutter-Xwaylandauth.* /run/host/run/user/${REAL_UID}/.Xauthority 2>/dev/null | head -1)
XAUTH=\${XAUTH#/run/host}

UNIT="pvz-plantsvszombies.service"
exec 3>&2 2>>/tmp/plants-vs-zombies-hostrun.log

SR --unit="\$UNIT" --collect --wait \\
  --setenv=DISPLAY=:0 \\
  --setenv=SteamAppId="\${SteamAppId:-$APPID}" \\
  --setenv=SteamGameId="\${SteamGameId:-$APPID}" \\
  \${XAUTH:+--setenv=XAUTHORITY="\$XAUTH"} \\
  /bin/bash "\$HOSTRUN"
rc=\$?
[ \$rc -ne 0 ] && echo "[plants-vs-zombies-steam] 宿主启动器退出码 \$rc" >&2
exit \$rc
EOF
  chmod 755 "$STEAMWRAP"
  bash -n "$STEAMWRAP" || die "Steam 入口生成异常"
  log "Steam 入口已安装：$STEAMWRAP"
}

# ---- 桌面项 ----------------------------------------------------------------
install_desktop() {
  cat > "$DESK" <<EOF
[Desktop Entry]
Name=Plants vs. Zombies (Lisuan 7G100)
Comment=绕开 Steam 容器与 PopCap 启动器，在宿主用 new WoW64 + 7G100 硬件渲染直跑
Exec="$STEAMWRAP"
Path="$GAME_DIR"
Type=Application
Terminal=false
Categories=Game;
EOF
  run_as_user chmod 644 "$DESK"
  log "桌面项已安装：$DESK"
}

# ---- Steam 启动选项 --------------------------------------------------------
# localconfig.vdf 由 Steam 自己维护；Steam 运行时改动会被覆盖，因此要求先退出。
require_steam_closed() {
  if pgrep -x steam >/dev/null 2>&1 || pgrep -f 'ubuntu12_32/steam' >/dev/null 2>&1; then
    die "Steam 正在运行：请先完全退出 Steam 再执行（否则改动会被覆盖）"
  fi
}

find_localconfig() {
  local cfg
  cfg="$(ls -d "$STEAM_ROOT"/userdata/*/config/localconfig.vdf 2>/dev/null | head -1)"
  [[ -n "$cfg" ]] || die "找不到 localconfig.vdf（先启动一次 Steam 让配置落地）"
  printf '%s' "$cfg"
}

set_launch_options() {
  local cfg
  cfg="$(find_localconfig)"
  require_steam_closed
  cp -a "$cfg" "$cfg.plants-vs-zombies.bak"
  python3 "$SELF_DIR/patch_launch_options.py" "$cfg" "$APPID" --set "$STEAM_LAUNCH" \
    || die "写入失败（原文件已备份：$cfg.plants-vs-zombies.bak）"
  log "Steam 启动选项已设置（备份：$cfg.plants-vs-zombies.bak）"
  log "重新打开 Steam 后，在库里点\"开始游戏\"即会走宿主硬件渲染。"
}

clear_launch_options() {
  local cfg
  cfg="$(find_localconfig)"
  require_steam_closed
  cp -a "$cfg" "$cfg.plants-vs-zombies.bak"
  python3 "$SELF_DIR/patch_launch_options.py" "$cfg" "$APPID" --clear \
    || die "清除失败（原文件已备份：$cfg.plants-vs-zombies.bak）"
  log "Steam 启动选项已清除（备份：$cfg.plants-vs-zombies.bak）"
}

# ---- 参数分发 --------------------------------------------------------------
case "${1:-}" in
  --status)   status; exit 0 ;;
  --launch)
    [[ -x "$HOSTRUN" ]] || die "尚未安装，先运行：$0"
    exec "$HOSTRUN" ;;
  --set-launch-options) set_launch_options; exit 0 ;;
  --clear-launch-options) clear_launch_options; exit 0 ;;
  --uninstall)
    rm -f "$BODY" "$HOSTRUN" "$STEAMWRAP" "$DESK" \
          "$RUN_DIR/pvsz-host-run.sh" "$RUN_DIR/pvsz-steam.sh"   # 兼容旧命名
    log "运行时常驻目录与桌面项已移除（Steam 里的游戏本体未改动）"
    warn "若设置过 Steam 启动选项，请先退出 Steam 执行 ./fix-plants-vs-zombies.sh --clear-launch-options"
    exit 0 ;;
  --force)    ;;   # 与安装同时使用：--force 重抽本体
  --reset-prefix) ;;   # 与安装同时使用：强制前缀重建
  "")         ;;
  *)          die "未知参数：$1（--status / --launch / --force / --reset-prefix / --set-launch-options / --clear-launch-options / --uninstall）" ;;
esac

# ---- 安装流程 --------------------------------------------------------------
[[ -d "$GAME_DIR" ]] || die "未找到游戏目录：$GAME_DIR（用 PVZ_GAME_DIR=... 指定）"
[[ -x "$PROTON_DIR/proton" ]] || die "未找到 Proton：$PROTON_DIR（用 PVZ_PROTON=... 指定）"
grep -q 'PROTON_USE_WOW64' "$PROTON_DIR/proton" 2>/dev/null \
  || die "该 Proton 不支持 new WoW64（$PROTON_DIR）：32 位游戏无法用 64 位 gf Vulkan，请换 Proton 11.0/Experimental"

mkdir -p "$RUN_DIR"
run_as_user mkdir -p "$REAL_HOME/.local/share/applications"
# 清理旧命名（v1 用的 pvsz-* 前缀）遗留的脚本，避免两份并存
rm -f "$RUN_DIR/pvsz-host-run.sh" "$RUN_DIR/pvsz-steam.sh"

ensure_wow64_prefix "${1:-}"
extract_body "${1:-}"
install_hostrun
install_steamwrap
install_desktop

log "全部完成（硬件渲染：new WoW64 + wined3d Vulkan → 7G100）。"
log "  直接玩（推荐）：桌面菜单里的 \"Plants vs. Zombies (Lisuan 7G100)\"，或 $STEAMWRAP"
log "  Steam 里点开始游戏：先关 Steam 执行 ./fix-plants-vs-zombies.sh --set-launch-options，"
log "    或在 属性 → 通用 → 启动选项 粘贴： $STEAM_LAUNCH"
log "  首次启动会重建 wow64 前缀（约 30–60 秒），之后正常。"
log "  状态检查：./fix-plants-vs-zombies.sh --status"
