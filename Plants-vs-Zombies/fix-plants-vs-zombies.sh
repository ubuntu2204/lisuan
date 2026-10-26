#!/usr/bin/env bash
#
# fix-plants-vs-zombies.sh — 《植物大战僵尸 GOTY》(AppID 3590) 在砺算 7G100 上的
#                            启动与渲染修复（幂等；2026-10-26 起默认软件渲染）
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
#      退化成 llvmpipe **软件渲染**（CPU 450–510%）。当时为避开 llvmpipe 而
#      追求硬件渲染；后续实机结论见下。
#   3) **PopCap 启动器参数不兼容**：PlantsVsZombies.exe 先把本体解到
#      %ProgramData%\PopCap Games\PlantsVsZombies\popcapgame1.exe，再用
#      -changedir="<游戏目录>" 拉起本体；本体对该参数任何形式都判非法 →
#      FATAL ERROR 弹窗退出。
#
# 2026-10-26 补充结论：用户感知的“卡顿”来自 wined3d GL 的硬件路径本身；
# 在游戏内关闭 3D Acceleration（Is3D=0，软件渲染）立即流畅。PvZ 是 800x600
# 的 2D 游戏，软件渲染足够，所以本脚本现在默认固化软件渲染；new WoW64 + GL
# 仍保留，用于保证能启动、不黑屏，并供 --hardware 复测。
#
# 修复（启动链路与 minion-masters 的 32 位路线一致）：
#   * **Proton new WoW64**（PROTON_USE_WOW64=1）：32 位 PE 经 wow64 thunk 调
#     **64 位** gf 驱动 —— 不需要厂商提供 i386 驱动；
#   * 前缀必须是 wow64 布局（旧的经典前缀移走重建，见 ensure_wow64_prefix）；
#   * 渲染后端用 **wined3d GL**（WINE_D3D_CONFIG=renderer=gl；软/硬由 Is3D 决定）：
#     DXVK 在此不可用 —— DXVK 2.3 没有 d3d8（PvZ 是 D3D8/DirectDraw 游戏），
#     DXVK ≥2.4 硬要求 VK_KHR_maintenance5，而 gf 的 Vulkan 只到 1.3 且缺该扩展；
#   * 抽本体常驻 + **宿主直跑**（cwd=游戏目录，不带 -changedir）；
#   * Steam 内点"开始游戏"：启动选项把命令交给 plants-vs-zombies-steam.sh，
#     容器内检测到沙箱后经会话总线委托宿主 systemd 拉起。
#
# Usage:
#   ./fix-plants-vs-zombies.sh                  # 安装/修复（抽本体 + 部署 + 前缀迁移）
#   ./fix-plants-vs-zombies.sh --status         # 状态（含当前软件/硬件渲染模式）
#   ./fix-plants-vs-zombies.sh --launch         # 默认关闭 3D 加速（软件渲染）后宿主直跑
#   ./fix-plants-vs-zombies.sh --launch --hardware  # 开启 3D 加速后直跑（仅排查用，本机实测卡）
#   ./fix-plants-vs-zombies.sh --software       # 只固化软件渲染（Is3D=0，推荐）
#   ./fix-plants-vs-zombies.sh --hardware       # 只固化硬件渲染（Is3D=1，供复测）
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
# 低 4GB 重映射池（沿用 minion-masters 的 gf-compat，同一个 wow64 陷阱同一个产物）
SHIM_HINT="$REAL_HOME/project/lisuan/minion-masters/gf-compat/lib/osrelease_redirect.so"

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
  [[ -x "$HOSTRUN" ]]   && echo "  [OK] 宿主启动器就绪（new WoW64 + wined3d GL）" \
                       || echo "  [--] 宿主启动器缺失"
  [[ -x "$STEAMWRAP" ]] && echo "  [OK] Steam 入口就绪" || echo "  [--] Steam 入口缺失"
  if shim="$(find_shim || true)"; then
    echo "  [OK] 低 4GB 重映射池 shim 可用（仅 vulkan 后端需要）：$shim"
  else
    echo "  [--] 无低 4GB 重映射池 shim（只有改用 PVZ_RENDERER=vulkan 时才需要）"
  fi
  case "$(read_render_mode)" in
    software) echo "  [OK] 渲染模式：软件渲染（3D Acceleration 关闭，本机推荐，实测流畅）" ;;
    hardware) echo "  [!!] 渲染模式：硬件渲染（3D Acceleration 开启，本机实测卡顿；--software 切回）" ;;
    *)        echo "  [--] 渲染模式：未知（游戏还没生成配置）" ;;
  esac
  case "$(read_3d_verdict)" in
    PASS)   echo "  [OK] 游戏 3D 能力检测：通过（FailureReason 为空）" ;;
    NOKEY)  echo "  [--] 游戏还没跑过 3D 检测" ;;
    *)      echo "  [!!] 游戏 3D 检测不通过：$(read_3d_verdict) —— 会进 PopCap 安全模式" ;;
  esac
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

# ---- 低 4GB 重映射池 shim -------------------------------------------------
# new WoW64 下 32 位 PE 拿不住 64 位显存映射指针：wined3d/DXVK 会报
#   fixme:vulkan:win32u_vkMapMemory2KHR returned mapping 0x... does not fit 32-bit pointer
#   err:d3d:wined3d_allocator_chunk_vk_map Failed to map chunk memory
# 表现为黑屏 + 主循环空转卡死（GNOME 弹“无响应”）。minion-masters 已经为此写了
# 一个在 mmap 层把 ≥4GB 的 GPU 映射重定位到 2.25–4GB 的 LD_PRELOAD，这里直接复用。
find_shim() {
  local c
  for c in "${PVZ_SHIM:-}" "$SHIM_HINT" \
           "$SELF_DIR/../minion-masters/gf-compat/lib/osrelease_redirect.so" \
           "$REAL_HOME/gf-compat/osrelease_redirect.so"; do
    [[ -n "$c" && -f "$c" ]] || continue
    file -b -L "$c" 2>/dev/null | grep -q 'ELF 64-bit' || continue
    printf '%s' "$c"
    return 0
  done
  return 1
}

# ---- 安装宿主启动器 ---------------------------------------------------------
install_hostrun() {
  local shim
  shim="$(find_shim || true)"
  cat > "$HOSTRUN" <<EOF
#!/bin/bash
# Lisuan 7G100: 在【宿主】运行《植物大战僵尸 GOTY》。
# 默认由 fix-plants-vs-zombies.sh 把 Is3D 写为 0（关闭 3D Acceleration，软件渲染），
# 这是 2026-10-26 实测流畅的配置；wined3d GL 的硬件路径在本机会卡顿。
#
# 为什么必须在宿主跑：Steam 的 pressure-vessel 容器里 Mesa 找不到 lisuan→gf
# 映射、也没有厂商 glvnd GLX vendor，winex11.drv 连 GL 都加载不了 → Wine 起不来。
#
# 为什么是 wined3d 的 GL 后端（最重要）：
#   PvZ 是 DirectDraw/D3D8 游戏，启动时会自己跑一次 3D 能力检测，结论写进注册表
#   HKCU\\Software\\SteamPopCap\\PlantsVsZombies\\Test3D\\FailureReason：
#     * renderer=vulkan → "Alpha blend Not Supported"（wined3d 的 Vulkan 后端不报
#       D3DPBCAPS_SRCALPHA）→ 游戏进 PopCap 安全模式：自动关掉 3D 加速 +
#       把存档隔离到 userdata_backup → 变成软件渲染 → 非常卡；
#     * renderer=gl     → FailureReason=""（检测通过，3D 加速保持开启）。
#   所以这里用 GL；而且**不能加低 4GB 重映射池 shim**——shim 会把 GL 的 DRI
#   映射也搬进低 4GB，gf 驱动陷入 reloc64 死循环（实测 200% CPU 不推进）。
#   DXVK 始终不可用：2.3 没有 d3d8，≥ 2.4 硬要求 gf 缺的 VK_KHR_maintenance5。
#
# 为什么不用 PopCap 启动器：它会给本体传 -changedir="<游戏目录>"，本体判非法
# （FATAL ERROR: Invalid command line parameter），且该参数任何形式都不被接受。
set -u
GAME_DIR="${GAME_DIR}"
PROTON_DIR="${PROTON_DIR}"
COMPAT_DATA="${COMPAT_DATA}"
BODY="${BODY}"
SHIM="${shim:-}"
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

# 32 位 PE 经 wow64 thunk 调 64 位驱动（new WoW64）；渲染走 wined3d 的 GL 后端
export PROTON_USE_WOW64=1
export PROTON_USE_WINED3D=1
# PVZ_RENDERER 默认 gl；改 vulkan 会被游戏的 3D 检测判不合格，除非你清楚自己在做什么
export WINE_D3D_CONFIG="\${WINE_D3D_CONFIG:-renderer=\${PVZ_RENDERER:-gl}}"
# 低 4GB 重映射池只对 Vulkan 路径有意义（vkMapMemory 高地址）；
# 给 GL 路径加它反而会让 gf 驱动在 reloc64 上死循环，所以默认不预加载。
if [ "\${PVZ_RENDERER:-gl}" = "vulkan" ]; then
  if [ -n "\$SHIM" ] && [ -f "\$SHIM" ]; then
    export LD_PRELOAD="\$SHIM\${LD_PRELOAD:+ \$LD_PRELOAD}"
  else
    echo "!! Vulkan 路径缺少低 4GB 重映射池（\${SHIM:-未配置}），预计会黑屏" >&2
  fi
fi
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
# 用带引号的 heredoc：生成物内部自己算路径与 uid，不在安装时做字符串拼接，
# 避免“游戏目录含空格/特殊字符”时转义链出错。
install_steamwrap() {
  cat > "$STEAMWRAP" <<'EOF'
#!/bin/bash
# Lisuan 7G100: Steam 里点"开始游戏"时由启动选项调用的入口。
#
# Steam 会用 pressure-vessel 容器包住整条命令，而容器内没有可用图形栈
#（Mesa 缺 lisuan→gf 映射、无厂商 glvnd vendor），所以检测到容器后必须把
# 游戏交给宿主。经共享会话总线调宿主的 systemd-run --user 拉起宿主启动器，
# --wait 阻塞到游戏退出并回传退出码（含 128+signal），Steam 的时长/退出检测依然有效。
# 机制与 left4dead2/、kingdom-rush/、underlords/ 一致（含它们踩过的坑）。
set -u
SELF_DIR="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
HOST_RUN="$SELF_DIR/plants-vs-zombies-host-run.sh"
UNIT="pvz-plantsvszombies.service"
LOG=/tmp/plants-vs-zombies-hostrun.log
UID_NUM=$(id -u)
APPID_DEFAULT=3590

# 容器判定：不同 runtime（steamrt4 / soldier / sniper）布局不同，单看
# /usr/lib/pressure-vessel 会漏判（漏判 = 在容器内直接跑游戏 = 必然失败）。
HOSTPFX=""
for p in /run/host /host; do
  [ -d "$p/usr/bin" ] && HOSTPFX="$p" && break
done
IN_CT=0
for m in /usr/lib/pressure-vessel /run/pressure-vessel /run/host /host; do
  [ -d "$m" ] && IN_CT=1
done
# pressure-vessel 在容器内注入的独有环境变量（宿主必无），比文件系统标志更可靠
[ -n "${PRESSURE_VESSEL_PREFIX:-}" ] && IN_CT=1
[ -n "${PRESSURE_VESSEL_RUNTIME:-}" ] && IN_CT=1
[ "${container:-}" = pressure-vessel ] && IN_CT=1
[ -n "$HOSTPFX" ] && IN_CT=1

{
  echo "=== $(date -Is) ==="
  echo "argv: $*"
  echo "in_container=$IN_CT hostpfx=${HOSTPFX:-无}"
} >> "$LOG" 2>&1

# 不在容器（桌面项 / 终端直跑）→ 直接执行宿主启动器
if [ "$IN_CT" = 0 ]; then
  echo "branch: host-direct" >> "$LOG"
  exec "$HOST_RUN"
fi

# 委托所需的东西缺一不可；缺了就报错退出，绝不在容器内直接跑游戏
if [ -z "$HOSTPFX" ] || [ ! -e "$HOSTPFX/usr/bin/systemd-run" ]; then
  echo "[plants-vs-zombies] !! 容器内但缺少委托条件（宿主前缀=无），拒绝在容器内启动游戏" >&2
  echo "FAIL: missing delegation tools" >> "$LOG"
  exit 1
fi

# 动态链接器：宿主可能是 /lib64 或 /lib/x86_64-linux-gnu
LDSO=""
for l in "$HOSTPFX/lib64/ld-linux-x86-64.so.2" "$HOSTPFX/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2"; do
  [ -e "$l" ] && LDSO="$l" && break
done
[ -n "$LDSO" ] || { echo "FAIL: no host ld.so under $HOSTPFX" >> "$LOG"; exit 1; }
LIBPATH="$HOSTPFX/usr/lib/x86_64-linux-gnu:$HOSTPFX/lib/x86_64-linux-gnu:$HOSTPFX/usr/lib/x86_64-linux-gnu/systemd"
# 选一个能连上宿主用户 systemd 的总线地址：
#   * steamrt4（SteamLinuxRuntime_4）不挂载宿主 /run，但 pressure-vessel 会把
#     会话总线代理到 /run/pressure-vessel/bus，容器默认 DBUS_SESSION_BUS_ADDRESS 就指它；
#   * 旧布局则把宿主 /run 挂在 $HOSTPFX 下。
# 逐个探测，连不上就报错退出，绝不退回“在容器内直接跑游戏”。
try_bus() {
  DBUS_SESSION_BUS_ADDRESS="$1" \
    $LDSO --library-path "$LIBPATH" $HOSTPFX/usr/bin/systemctl --user show-environment >/dev/null 2>&1
}
BUS=""
for cand in "${DBUS_SESSION_BUS_ADDRESS:-}" "unix:path=$HOSTPFX/run/user/$UID_NUM/bus" "unix:path=/run/user/$UID_NUM/bus"; do
  [ -n "$cand" ] || continue
  if try_bus "$cand"; then BUS="$cand"; break; fi
done
if [ -z "$BUS" ]; then
  echo "FAIL: no host session bus reachable" >> "$LOG"
  echo "[plants-vs-zombies] !! 容器内但连不上宿主会话总线，拒绝在容器内启动游戏" >&2
  exit 1
fi
export DBUS_SESSION_BUS_ADDRESS="$BUS"
# 只有宿主 run 目录真可见时才改 XDG_RUNTIME_DIR（否则保持容器值，与总线代理一致）
[ -d "$HOSTPFX/run/user/$UID_NUM" ] && export XDG_RUNTIME_DIR="$HOSTPFX/run/user/$UID_NUM"
SR() { $LDSO --library-path "$LIBPATH" $HOSTPFX/usr/bin/systemd-run --user "$@"; }
SS() { $LDSO --library-path "$LIBPATH" $HOSTPFX/usr/bin/systemctl --user "$@"; }
echo "bus=$BUS xdg=$XDG_RUNTIME_DIR" >> "$LOG"

# 宿主侧 X 认证文件：先看宿主 run 目录是否可见，再回退到问宿主 systemd
XAUTH=""
[ -d "$HOSTPFX/run/user/$UID_NUM" ] && XAUTH=$(ls $HOSTPFX/run/user/$UID_NUM/.mutter-Xwaylandauth.* $HOSTPFX/run/user/$UID_NUM/.Xauthority 2>/dev/null | head -1)
XAUTH=${XAUTH#$HOSTPFX}
[ -n "$XAUTH" ] || XAUTH=$(SS show-environment 2>/dev/null | sed -n 's/^XAUTHORITY=//p')

if SS is-active "$UNIT" >/dev/null 2>&1; then
  echo "[plants-vs-zombies] !! 宿主游戏已在运行，拒绝重复委托" >&2
  echo "FAIL: host unit already active" >> "$LOG"
  exit 1
fi

# 转发 Steam 注入的 appid 环境：缺了宿主侧 steam_api 认不出游戏 ID，会 ABRT 重拉
SPID_ENV=""
[ -n "${SteamPID:-}" ] && SPID_ENV="--setenv=SteamPID=$SteamPID"
SR --wait --unit="$UNIT" --collect \
  --setenv=HOME="$HOME" \
  --setenv=DISPLAY="${DISPLAY:-:0}" \
  --setenv=XAUTHORITY="$XAUTH" \
  --setenv=SteamAppId="${SteamAppId:-$APPID_DEFAULT}" \
  --setenv=SteamGameId="${SteamGameId:-$APPID_DEFAULT}" \
  $SPID_ENV \
  /bin/bash "$HOST_RUN" >>"$LOG" 2>&1
rc=$?
echo "$(date -Is) host unit exit rc=$rc" >> "$LOG"
if [ "$rc" -ne 0 ]; then
  echo "[plants-vs-zombies] !! 宿主游戏异常退出 rc=$rc；请查 journalctl --user -u $UNIT" >&2
fi
exit "$rc"
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
Comment=绕开 Steam 容器与 PopCap 启动器；默认软件渲染（关闭 3D 加速），本机实测流畅
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

# ---- 游戏自己的 3D 检测结论（无需截图即可判定硬件加速是否真开着）--------------
# PvZ 启动时跑一次能力检测，结果写进 HKCU\Software\SteamPopCap\PlantsVsZombies\Test3D
# 的 FailureReason：空串 = 通过；非空（如 "Alpha blend Not Supported"）= 不通过，
# 游戏会进安全模式（关掉 3D 加速 + 把存档隔到 userdata_backup）→ 软件渲染→卡。
read_3d_verdict() {
  local reg="$COMPAT_DATA/pfx/user.reg"
  [[ -f "$reg" ]] || { echo "NOKEY"; return 0; }
  python3 - "$reg" <<'PY'
import re, sys
txt = open(sys.argv[1], encoding="utf-8", errors="replace").read()
m = re.search(r'\[Software\\\\SteamPopCap\\\\PlantsVsZombies\\\\Test3D\](.*?)(?=\n\[|\Z)', txt, re.S)
if not m:
    print("NOKEY"); raise SystemExit
r = re.search(r'"FailureReason"="([^"]*)"', m.group(1))
if not r:
    print("NOPVAL"); raise SystemExit
print("PASS" if r.group(1) == "" else r.group(1))
PY
}

check_3d() {
  local v; v="$(read_3d_verdict)"
  case "$v" in
    PASS)   log "游戏 3D 能力检测：通过（FailureReason 为空；不代表硬件加速一定开启，模式看 --status）" ;;
    NOKEY)  warn "游戏还没跑过 3D 检测（先启动一次游戏）" ;;
    NOPVAL) warn "Test3D 键里没有 FailureReason 字段" ;;
    *)      warn "游戏 3D 检测不通过：$v —— 会进 PopCap 安全模式；软件渲染模式下不影响流畅度" ;;
  esac
}

# 把游戏从安全模式里拉出来：清掉失效的 Test3D 缓存、把被隔离的存档放回 userdata/。
# 换过渲染后端后尤其需要：DisplayGUID 与能力集一变，旧结论会把游戏锁在软件渲染里。
reset_3d_test() {
  local reg="$COMPAT_DATA/pfx/user.reg" D="$COMPAT_DATA/pfx/drive_c/ProgramData/Steam/PlantsVsZombies"
  pgrep -f 'pvz-game\.exe' >/dev/null 2>&1 && die "游戏正在运行：先退出游戏再复位"
  [[ -f "$reg" ]] || die "没有前缀注册表：$reg"
  cp -a "$reg" "$reg.reset-3d.bak"
  python3 - "$reg" <<'PY'
import re, sys
p = sys.argv[1]
s = open(p, encoding="utf-8", errors="surrogateescape").read()
s2 = re.sub(r'\[Software\\\\SteamPopCap\\\\PlantsVsZombies\\\\Test3D\].*?(?=\n\[|\Z)', '', s, count=1, flags=re.S)
open(p, "w", encoding="utf-8", errors="surrogateescape").write(s2)
print("Test3D 缓存已清除" if "Test3D" not in s2 else "仍有残留 Test3D 键，请手工检查")
PY
  if [[ -f "$D/userdata_backup/user1.dat" && ! -f "$D/userdata/user1.dat" ]]; then
    cp -a "$D/userdata_backup/user1.dat" "$D/userdata/user1.dat"
    log "被隔离的存档已放回 userdata/（backup 原件保留）"
  fi
  log "已复位 3D 检测；下次启动会重测（后端为 gl 时 FailureReason 应为空）"
}

# ---- 渲染模式：软件（默认，流畅）/ 硬件（仅复测） --------------------------
# 2026-10-26 实机结论：本机 gf 上 wined3d GL 的硬件路径卡顿；游戏内关掉
# "3D Acceleration" 立即流畅。PvZ 是 800x600 的 2D 游戏，软件渲染足够，
# 因此默认把 HKCU\Software\SteamPopCap\PlantsVsZombies\Is3D 写为 0。
read_render_mode() {
  local reg="$COMPAT_DATA/pfx/user.reg"
  [[ -f "$reg" ]] || { echo "unknown"; return 0; }
  python3 - "$reg" <<'PY'
import re, sys
s = open(sys.argv[1], encoding="utf-8", errors="replace").read()
m = re.search(r'\[Software\\\\SteamPopCap\\\\PlantsVsZombies\][^\n]*\n(.*?)(?=\n\[|\Z)', s, re.S)
if not m:
    print("unknown"); raise SystemExit
r = re.search(r'^"Is3D"=dword:([0-9a-fA-F]+)', m.group(1), re.M)
if not r:
    print("unknown")
else:
    print("hardware" if int(r.group(1), 16) else "software")
PY
}

set_render_mode() {
  local mode="${1:-software}" reg="$COMPAT_DATA/pfx/user.reg" value
  case "$mode" in
    software) value=0 ;;
    hardware) value=1 ;;
    *) die "未知渲染模式：$mode（software/hardware）" ;;
  esac
  pgrep -f 'pvz-game\.exe' >/dev/null 2>&1 && die "游戏正在运行：先退出游戏再切换渲染模式"
  if [[ ! -f "$reg" ]]; then
    warn "没有前缀注册表：$reg（首次启动游戏后再执行 --software 即可固化）"
    return 0
  fi
  cp -a "$reg" "$reg.render-mode.bak"
  python3 - "$reg" "$value" <<'PY'
import re, sys
p, value = sys.argv[1], sys.argv[2]
s = open(p, encoding="utf-8", errors="surrogateescape").read()
header = "[Software\\\\SteamPopCap\\\\PlantsVsZombies]"
line = '"Is3D"=dword:0000000' + value
m = re.search(r'^\[Software\\\\SteamPopCap\\\\PlantsVsZombies\][^\n]*\n', s, re.M)
if not m:
    if not s.endswith("\n"):
        s += "\n"
    s += "\n" + header + "\n" + line + "\n"
else:
    start = m.end()
    end = s.find("\n[", start)
    if end < 0:
        end = len(s)
    body = s[start:end]
    if re.search(r'^"Is3D"=dword:[0-9a-fA-F]+', body, re.M):
        body = re.sub(r'^"Is3D"=dword:[0-9a-fA-F]+.*$', line, body, count=1, flags=re.M)
    else:
        body = line + "\n" + body
    s = s[:start] + body + s[end:]
open(p, "w", encoding="utf-8", errors="surrogateescape").write(s)
PY
  if [[ "$mode" == software ]]; then
    log "已关闭 3D Acceleration（Is3D=0，软件渲染）——本机实测此模式流畅"
  else
    log "已开启 3D Acceleration（Is3D=1，硬件渲染）——仅用于复测，本机实测会卡顿"
  fi
}

# ---- 参数分发 --------------------------------------------------------------
case "${1:-}" in
  --status)   status; exit 0 ;;
  --check-3d) check_3d; exit 0 ;;
  --reset-3d-test) reset_3d_test; exit 0 ;;
  --software) set_render_mode software; exit 0 ;;
  --hardware) set_render_mode hardware; exit 0 ;;
  --launch)
    [[ -x "$HOSTRUN" ]] || die "尚未安装，先运行：$0"
    if [[ "${2:-}" == "--hardware" ]]; then
      set_render_mode hardware
    else
      set_render_mode software
    fi
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
  *)          die "未知参数：$1（--status / --check-3d / --reset-3d-test / --software / --hardware / --launch [--hardware] / --force / --reset-prefix / --set-launch-options / --clear-launch-options / --uninstall）" ;;
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
set_render_mode software

log "全部完成。默认关闭 3D Acceleration（软件渲染），这是本机实测流畅的配置。"
log "  直接玩（推荐）：桌面菜单里的 \"Plants vs. Zombies (Lisuan 7G100)\"，或 $STEAMWRAP"
log "  终端直跑：./fix-plants-vs-zombies.sh --launch（默认软件渲染）"
log "  Steam 里点开始游戏：先关 Steam 执行 ./fix-plants-vs-zombies.sh --set-launch-options，"
log "    或在 属性 → 通用 → 启动选项 粘贴： $STEAM_LAUNCH"
log "  首次启动会重建 wow64 前缀（约 30–60 秒），之后正常。"
log "  想复测硬件路径：./fix-plants-vs-zombies.sh --launch --hardware（本机实测会卡）"
log "  状态检查（含渲染模式）：./fix-plants-vs-zombies.sh --status"
