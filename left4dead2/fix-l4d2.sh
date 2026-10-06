#!/bin/bash
# fix-l4d2.sh — 《求生之路 2》(Left 4 Dead 2, AppID 550) 在砺算 7G100 上走
#               【原生 Linux 32 位 + Vulkan 直连（Venus 进程外转发）】硬件渲染
#
# 方案定稿（2026-10-06）：不用 left4dead2.exe、不用 Proton、不用 DXVK、不用软件渲染。
#
# 原理：L4D2 原生 Linux 版自带 32 位 shaderapivk.so（Valve 官方 Vulkan 后端）。
#   32 位进程无法 dlopen 64 位 libvulkan_gf.so（同一进程不能混 ABI），所以不硬加载，
#   而是让 32 位进程加载系统 Mesa 的【32 位】Venus ICD（libvulkan_virtio.so），
#   把 Vulkan 命令经 Unix socket 转发给一个【64 位】virglrenderer/Venus 服务端，
#   由服务端加载 64 位 gf Vulkan ICD 真正渲染：
#
#     hl2_linux -vulkan
#       -> bin/shaderapivk.so (ELF32)
#       -> libvulkan_virtio.so (ELF32, Venus 客户端)
#       == Unix socket ==>
#       -> virgl_test_server --venus (ELF64) -> libvulkan_gf.so (ELF64) -> 7G100
#
#   与 Proton WoW64 的区别：不碰 Windows 版、不引入 wine/DXVK，游戏本体全程原生。
#   与 llvmpipe 的区别：像素由 gf 驱动在 GPU 上算，Mesa 只做命令转发。
#
# 组成（全部在 home 内，不装系统目录）：
#   1) 64 位 Venus 服务端：vtest-forward/build-venus-bridge.sh 构建到
#      ~/.local/share/l4d2-venus/（私有前缀，不动系统 virglrenderer）
#   2) systemd --user 单元 l4d2-venus.service：随会话常驻服务端
#   3) 启动包装 l4d2-venus-launch.sh：确认服务端就绪 -> 注入变量 -> exec 游戏
#   4) Steam：STEAM_RUNTIME=0（容器内的 i386 栈没有 Venus ICD）+ 包装器 + -vulkan
#
# 用法：
#   bash fix-l4d2.sh             # 安装/修复（幂等；建议先完全退出 Steam）
#   bash fix-l4d2.sh --status    # 查看各环节状态
#   bash fix-l4d2.sh --uninstall # 卸载：停服务、还原 Steam 配置
#
# 环境变量（可选覆盖）：
#   L4D2_VENUS_HOME   服务端私有前缀（默认 ~/.local/share/l4d2-venus）
#   L4D2_VENUS_SOCKET socket 路径（默认 $XDG_RUNTIME_DIR/l4d2-venus.sock）
#   L4D2_RENDER_NODE  服务端使用的 render 节点（默认 /dev/dri/renderD128）
set -u

APPID=550
GAME="$HOME/.local/share/Steam/steamapps/common/Left 4 Dead 2"
REPO="$(cd "$(dirname "$0")/.." && pwd)"
VTEST="$(cd "$(dirname "$0")" && pwd)/vtest-forward"

BRIDGE_HOME="${L4D2_VENUS_HOME:-${XDG_DATA_HOME:-$HOME/.local/share}/l4d2-venus}"
SERVER="$BRIDGE_HOME/build/vtest/virgl_test_server"
WORKER="$BRIDGE_HOME/build/server/virgl_render_server"
SOCKET="${L4D2_VENUS_SOCKET:-${XDG_RUNTIME_DIR:-/tmp}/l4d2-venus.sock}"
GF_ICD="${L4D2_GF_ICD:-/usr/share/vulkan/icd.d/gf_icd.x86_64.json}"
VIRTIO_ICD="${L4D2_VIRTIO_ICD:-/usr/share/vulkan/icd.d/virtio_icd.json}"
RENDER_NODE="${L4D2_RENDER_NODE:-/dev/dri/renderD128}"

UNIT_DIR="$HOME/.config/systemd/user"
UNIT="$UNIT_DIR/l4d2-venus.service"
LAUNCH="$BRIDGE_HOME/l4d2-venus-launch.sh"   # Steam 启动链入口（容器内执行）
HOST_RUN="$BRIDGE_HOME/l4d2-host-run.sh"     # 宿主侧启动器（委托目标）
GAME_UNIT="l4d2-host-game.service"           # 委托后在宿主上承载游戏的单元

CONFIG_VDF="$HOME/.local/share/Steam/config/config.vdf"
LOCALCONFIG_VDF=$(ls ~/.local/share/Steam/userdata/*/config/localconfig.vdf 2>/dev/null | head -1)

say() { echo "[l4d2-venus] $*"; }
err() { echo "[l4d2-venus] !! $*" >&2; }

# 存在性检查仅约束"安装"；--uninstall / --status 允许残缺环境
case "${1:-}" in
  --uninstall|--status) ;;
  *)
    [ -d "$GAME" ] || { err "未找到游戏目录: $GAME"; exit 1; }
    [ -f "$GAME/hl2.sh" ] || { err "缺原生启动器 hl2.sh"; exit 1; }
    [ -f "$GAME/bin/shaderapivk.so" ] || { err "缺 32 位 Vulkan 后端 bin/shaderapivk.so（Steam 校验文件）"; exit 1; }
    [ -f "$VIRTIO_ICD" ] || { err "缺 32/64 位 Venus ICD 配置: $VIRTIO_ICD"; exit 1; }
    [ -f "$GF_ICD" ] || { err "缺 gf ICD 配置: $GF_ICD"; exit 1; }
    [ -f /usr/lib/i386-linux-gnu/libvulkan_virtio.so ] || { err "缺 32 位 Venus ICD: /usr/lib/i386-linux-gnu/libvulkan_virtio.so"; exit 1; }
    [ -f /usr/lib/i386-linux-gnu/libvulkan.so.1 ] || { err "缺 32 位 Vulkan loader: /usr/lib/i386-linux-gnu/libvulkan.so.1"; exit 1; }
    ;;
esac

# ---------- 0) render 节点 ----------
detect_render_node() {
  [ -c "$RENDER_NODE" ] && return 0
  local n
  n=$(ls /dev/dri/renderD* 2>/dev/null | head -1)
  [ -n "$n" ] || { err "无 /dev/dri/renderD*（gf 内核模块未加载？）"; return 1; }
  RENDER_NODE="$n"
  say "render 节点 auto: $RENDER_NODE"
}

# ---------- 1) 构建 64 位 Venus 服务端 ----------
build_server() {
  if [ -x "$SERVER" ] && [ -x "$WORKER" ]; then
    say "Venus 服务端已构建: $SERVER"
    return 0
  fi
  say "构建 64 位 Venus 服务端（首次约需数分钟）"
  bash "$VTEST/build-venus-bridge.sh" || { err "服务端构建失败"; return 1; }
  [ -x "$SERVER" ] || { err "构建未产出 $SERVER"; return 1; }
}

# ---------- 2) systemd --user 单元：随会话常驻服务端 ----------
install_unit() {
  mkdir -p "$UNIT_DIR"
  cat > "$UNIT" <<EOF
[Unit]
Description=L4D2 Venus bridge (64-bit virglrenderer -> gf Vulkan -> 7G100)
After=graphical-session.target

[Service]
Type=simple
# 服务端用 64 位 gf ICD 真正渲染；客户端是 32 位 Mesa Venus ICD
Environment=RENDER_SERVER_EXEC_PATH=$WORKER
Environment=VK_ICD_FILENAMES=$GF_ICD
# 旧 socket 残留会让服务端 bind 失败，启动前先清
ExecStartPre=/bin/rm -f $SOCKET
# 必须常驻 accept 循环（不加 --no-loop-or-fork，否则服务完一个客户端就退出）
ExecStart=$SERVER --no-virgl --venus --rendernode $RENDER_NODE --socket-path $SOCKET
# 按崩溃处理约定：不做自动重启，崩溃留现场便于定位根因
Restart=no

[Install]
WantedBy=default.target
EOF
  systemctl --user daemon-reload
  systemctl --user enable l4d2-venus.service >/dev/null 2>&1 \
    && say "单元已启用（开机/登录自起）: $UNIT"
  systemctl --user restart l4d2-venus.service 2>/dev/null \
    || systemctl --user start l4d2-venus.service

  local i
  for i in $(seq 1 60); do
    [ -S "$SOCKET" ] && { say "服务端就绪: $SOCKET"; return 0; }
    sleep 0.1
  done
  err "服务端未创建 socket，见: journalctl --user -u l4d2-venus"
  return 1
}

# ---------- 3a) 宿主侧启动器：真正跑游戏的地方 ----------
write_host_runner() {
  cat > "$HOST_RUN" <<EOF
#!/bin/bash
# l4d2-host-run.sh — 宿主侧启动器（由 left4dead2/fix-l4d2.sh 生成，勿手改）
# 在宿主上跑原生 32 位 L4D2：用系统 i386 Mesa 的 Venus ICD 连到 64 位 gf 服务端。
set -u
SOCKET="$SOCKET"
GAME="$GAME"

if [ ! -S "\$SOCKET" ]; then
  systemctl --user start l4d2-venus.service 2>/dev/null || true
  for _ in \$(seq 1 60); do [ -S "\$SOCKET" ] && break; sleep 0.1; done
fi
[ -S "\$SOCKET" ] || { echo "[l4d2-venus] !! Venus 服务端 socket 未就绪: \$SOCKET" >&2; exit 1; }

# 32 位进程 -> Mesa Venus ICD -> socket -> 64 位服务端 -> libvulkan_gf.so
export VK_ICD_FILENAMES="$VIRTIO_ICD"
export VTEST_SOCKET_NAME="\$SOCKET"
# 关键：Mesa Venus ICD 默认走 virtio-gpu 内核路径（本机没有该设备），
# 必须显式指定 vtest 才会经 socket 连到我们的 64 位服务端。
# 缺它时 vkEnumeratePhysicalDevices 直接 ERROR_INITIALIZATION_FAILED。
export VN_DEBUG=vtest
# Steam overlay 的 Vulkan 层会绕过 Venus ICD 直连，禁用
export VK_LOADER_LAYERS_DISABLE=VK_LAYER_VALVE_steam_overlay_32,VK_LAYER_VALVE_STEAM_OVERLAY_64
export STEAM_RUNTIME=0

cd "\$GAME"
exec ./hl2.sh "\$@"
EOF
  chmod +x "$HOST_RUN"
  say "宿主启动器已部署: $HOST_RUN"
}

# ---------- 3b) Steam 启动链入口：容器内则委托宿主 ----------
write_launcher() {
  cat > "$LAUNCH" <<EOF
#!/usr/bin/env bash
# l4d2-venus-launch.sh — Steam 启动链入口（由 left4dead2/fix-l4d2.sh 生成，勿手改）
#
# Steam 会把 Linux 原生游戏也包进 pressure-vessel 容器（SteamLinuxRuntime_soldier
# + scout-on-soldier），STEAM_RUNTIME=0 拦不住。容器内是 steamrt 的 i386 栈：没有
# libvulkan_virtio.so，且 glibc 太旧无法加载宿主编译的 i386 ICD。
# 因此：检测到容器就委托宿主跑游戏（与 underlords/ 同一机制，经共享会话总线调
# 宿主的 systemd-run --user），并阻塞等待游戏退出，让 Steam 的时长/退出检测有效。
set -u
UNIT="$GAME_UNIT"
HOST_RUN="$HOST_RUN"
UID_NUM=\$(id -u)

# 取出命令行里 hl2.sh 之后的游戏参数（-game/-steam/-vulkan/用户参数…）
args=(); found=0
for a in "\$@"; do
  if [ "\$found" = 1 ]; then args+=("\$a"); fi
  case "\$a" in *hl2.sh*) found=1 ;; esac
done
if [ "\${#args[@]}" = 0 ]; then args=(-game left4dead2 -steam -vulkan); fi

if [ -d /usr/lib/pressure-vessel ]; then
  LDSO=/lib64/ld-linux-x86-64.so.2
  LIBPATH=/run/host/usr/lib/x86_64-linux-gnu:/run/host/lib/x86_64-linux-gnu:/run/host/usr/lib/x86_64-linux-gnu/systemd
  SR() { \$LDSO --library-path "\$LIBPATH" /run/host/usr/bin/systemd-run --user "\$@"; }
  SS() { \$LDSO --library-path "\$LIBPATH" /run/host/usr/bin/systemctl --user "\$@"; }

  # 宿主侧 X 认证文件（Xwayland 会话每次登录随机后缀）
  XAUTH=\$(ls /run/host/run/user/\$UID_NUM/.mutter-Xwaylandauth.* /run/host/run/user/\$UID_NUM/.Xauthority 2>/dev/null | head -1)
  XAUTH=\${XAUTH#/run/host}
  [ -n "\$XAUTH" ] || XAUTH=\$(SS show-environment 2>/dev/null | sed -n 's/^XAUTHORITY=//p')

  if SS is-active "\$UNIT" >/dev/null 2>&1; then
    echo "[l4d2-venus] 宿主游戏已在运行，等待其退出" >&2
  else
    SR --unit="\$UNIT" --collect \\
      --setenv=HOME="\$HOME" \\
      --setenv=DISPLAY="\${DISPLAY:-:0}" \\
      --setenv=XAUTHORITY="\$XAUTH" \\
      /bin/bash "\$HOST_RUN" "\${args[@]}" \\
      || echo "[l4d2-venus] !! systemd-run 失败 rc=\$?" >&2
  fi

  sleep 5
  while SS is-active "\$UNIT" 2>/dev/null | grep -q '^active\$'; do sleep 3; done
  SS reset-failed "\$UNIT" 2>/dev/null
  echo "[l4d2-venus] 宿主游戏已退出" >&2
  exit 0
fi

# 不在容器内（宿主）则直接跑
exec "\$HOST_RUN" "\${args[@]}"
EOF
  chmod +x "$LAUNCH"
  say "启动包装已部署: $LAUNCH"
}

# ---------- 4) Steam 配置：清除 Proton 绑定 + 写入启动选项 ----------
# 4a. 移除 CompatToolMapping 550（不再走 Proton/Windows 版）
clear_compat_tool() {
  [ -f "$CONFIG_VDF" ] || { err "无 config.vdf"; return 1; }
  python3 - "$CONFIG_VDF" <<'PY'
import re, sys
p = sys.argv[1]
data = open(p).read()
m = re.search(r'"CompatToolMapping"\s*\n\s*\{', data)
if not m:
    print("[l4d2-venus] config.vdf 无 CompatToolMapping"); sys.exit(0)
seg_start = m.end(); depth = 1; i = seg_start
while i < len(data) and depth > 0:
    if data[i] == '{': depth += 1
    elif data[i] == '}': depth -= 1
    i += 1
seg = data[seg_start:i-1]
seg2 = re.sub(r'\n\s*"550"\s*\n\s*\{[^}]*\}', '', seg)
if seg2 != seg:
    open(p, 'w').write(data[:seg_start] + seg2 + data[i-1:])
    print("[l4d2-venus] CompatToolMapping 550 已移除（改回原生 Linux 版）")
else:
    print("[l4d2-venus] CompatToolMapping 无 550 条目")
PY
}

# 4b. LaunchOptions: <包装器> %command% -vulkan <用户参数>
#     （不再需要 STEAM_RUNTIME=0：它拦不住 pressure-vessel，容器由包装器委托绕过）
config_launch_options() {
  [ -n "$LOCALCONFIG_VDF" ] && [ -f "$LOCALCONFIG_VDF" ] || { err "无 localconfig.vdf（先在 Steam 里启动一次游戏）"; return 1; }
  python3 - "$LOCALCONFIG_VDF" "$LAUNCH" <<'PY'
import re, sys
p, launch = sys.argv[1], sys.argv[2]
data = open(p).read()
i = data.find('"apps"'); j = data.find('"550"', i)
if j < 0: sys.exit("[l4d2-venus] localconfig 无 550 段（先在 Steam 里启动一次游戏）")
brace = data.find('{', j); depth = 1; k = brace + 1
while depth > 0:
    if data[k] == '{': depth += 1
    elif data[k] == '}': depth -= 1
    k += 1
seg = data[brace:k]
m = re.search(r'\n\s*"LaunchOptions"\s*"([^"]*)"', seg)
old = m.group(1) if m else ""

# 剥掉本方案与已废弃的 Proton WoW64 方案注入项，保留用户自有参数
user_args = old.replace('%command%', ' ')
for tok in ('STEAM_RUNTIME=0', 'PROTON_USE_WOW64=1', '__GLX_VENDOR_LIBRARY_NAME=mesa',
            'LIBGL_ALWAYS_SOFTWARE=1', '-vulkan'):
    user_args = user_args.replace(tok, ' ')
user_args = re.sub(r'LD_PRELOAD=\S+', ' ', user_args)
user_args = re.sub(r'VK_ICD_FILENAMES=\S+', ' ', user_args)
user_args = re.sub(r'VK_LOADER_LAYERS_DISABLE=\S+', ' ', user_args)
user_args = re.sub(r'\S*l4d2-venus-launch\.sh', ' ', user_args)
user_args = ' '.join(user_args.split())

new = '%s %%command%% -vulkan' % launch
if user_args:
    new += ' ' + user_args
esc = new.replace('\\', '\\\\').replace('"', '\\"')
if m:
    seg2 = re.sub(r'\n\s*"LaunchOptions"\s*"[^"]*"',
                  '\n\t\t\t\t\t\t"LaunchOptions"\t\t"%s"' % esc, seg)
else:
    seg2 = seg[:1] + '\n\t\t\t\t\t\t"LaunchOptions"\t\t"%s"' % esc + seg[1:]
open(p, 'w').write(data[:brace] + seg2 + data[k:])
print("[l4d2-venus] LaunchOptions 已写入%s" %
      ("（保留用户参数: %s）" % user_args if user_args else ""))
PY
}

# ---------- 状态 ----------
status() {
  echo "== 游戏（原生 32 位）=="
  [ -f "$GAME/hl2.sh" ] && echo "  $GAME ✅" || err "  缺 hl2.sh"
  [ -f "$GAME/bin/shaderapivk.so" ] \
    && echo "  bin/shaderapivk.so ✅ $(file -b "$GAME/bin/shaderapivk.so" | cut -d, -f1)" \
    || err "  缺 bin/shaderapivk.so"
  echo "== 客户端（32 位 Venus ICD）=="
  ls /usr/lib/i386-linux-gnu/libvulkan_virtio.so 2>/dev/null || err "  缺失"
  ls "$VIRTIO_ICD" 2>/dev/null || err "  缺失 $VIRTIO_ICD"
  echo "== 服务端（64 位 virglrenderer）=="
  [ -x "$SERVER" ] && echo "  $SERVER ✅" || echo "  未构建（跑 vtest-forward/build-venus-bridge.sh）"
  systemctl --user is-active l4d2-venus.service 2>/dev/null | sed 's/^/  systemd: /'
  systemctl --user is-enabled l4d2-venus.service 2>/dev/null | sed 's/^/  enabled: /'
  [ -S "$SOCKET" ] && echo "  socket: $SOCKET ✅" || echo "  socket: $SOCKET 未就绪"
  echo "== 渲染节点 =="; echo "  $RENDER_NODE $([ -c "$RENDER_NODE" ] && echo ✅ || echo 缺失)"
  echo "== 启动包装（Steam 链入口）=="; [ -x "$LAUNCH" ] && echo "  $LAUNCH ✅" || echo "  缺失"
  echo "== 宿主启动器 =="; [ -x "$HOST_RUN" ] && echo "  $HOST_RUN ✅" || echo "  缺失"
  echo "== CompatTool（应为空 = 原生）=="
  python3 -c "
import re, sys
try: data = open('$CONFIG_VDF').read()
except OSError: sys.exit('  config.vdf 不可读')
m = re.search(r'\"CompatToolMapping\"\s*\n\s*\{', data)
if not m: sys.exit('  无 CompatToolMapping')
seg = data[m.end():]; depth = 1; i = 0
while depth > 0:
    if seg[i] == '{': depth += 1
    elif seg[i] == '}': depth -= 1
    i += 1
mm = re.search(r'\"550\"\s*\{[^}]*?\"name\"\s*\"([^\"]+)\"', seg[:i], re.S)
print('  ' + (mm.group(1) if mm else '未绑定 ✅'))
"
  echo "== LaunchOpts =="
  python3 -c "
import re, sys
try: data = open('$LOCALCONFIG_VDF').read()
except OSError: sys.exit('  localconfig.vdf 不可读')
i = data.find('\"apps\"'); j = data.find('\"550\"', i)
if j < 0: sys.exit('  localconfig 无 550 段')
brace = data.find('{', j); depth = 1; k = brace + 1
while depth > 0:
    if data[k] == '{': depth += 1
    elif data[k] == '}': depth -= 1
    k += 1
m = re.search(r'\"LaunchOptions\"\s*\"([^\"]*)\"', data[brace:k])
print('  ' + (m.group(1) if m else '未设置'))
"
  echo "== 进程 =="; pgrep -f "hl2_linux" >/dev/null && echo "  游戏运行中" || echo "  未运行"
}

# ---------- 卸载 ----------
uninstall() {
  say "== 卸载 L4D2 Venus 修复 =="
  if pgrep -f "ubuntu12_32/steam" >/dev/null 2>&1; then
    say "警告: Steam 正在运行，vdf 改动可能在退出时被覆盖。建议先退出 Steam。"
  fi
  systemctl --user stop l4d2-venus.service 2>/dev/null && say "服务端已停止"
  systemctl --user stop "$GAME_UNIT" 2>/dev/null && say "宿主游戏单元已停止"
  systemctl --user disable l4d2-venus.service 2>/dev/null && say "单元已禁用"
  rm -f "$UNIT"; systemctl --user daemon-reload 2>/dev/null
  rm -f "$LAUNCH" "$HOST_RUN" "$SOCKET"

  # LaunchOptions 还原为用户自有参数
  if [ -n "$LOCALCONFIG_VDF" ] && [ -f "$LOCALCONFIG_VDF" ]; then
    python3 - "$LOCALCONFIG_VDF" <<'PY'
import re, sys
p = sys.argv[1]; data = open(p).read()
i = data.find('"apps"'); j = data.find('"550"', i)
if j < 0:
    print("[l4d2-venus] localconfig 无 550 段"); sys.exit(0)
brace = data.find('{', j); depth = 1; k = brace + 1
while depth > 0:
    if data[k] == '{': depth += 1
    elif data[k] == '}': depth -= 1
    k += 1
seg = data[brace:k]
m = re.search(r'\n\s*"LaunchOptions"\s*"([^"]*)"', seg)
if not m:
    print("[l4d2-venus] 无 LaunchOptions"); sys.exit(0)
args = m.group(1).replace('%command%', ' ')
for tok in ('STEAM_RUNTIME=0', 'PROTON_USE_WOW64=1', '__GLX_VENDOR_LIBRARY_NAME=mesa',
            'LIBGL_ALWAYS_SOFTWARE=1', '-vulkan'):
    args = args.replace(tok, ' ')
args = re.sub(r'LD_PRELOAD=\S+', ' ', args)
args = re.sub(r'VK_ICD_FILENAMES=\S+', ' ', args)
args = re.sub(r'VK_LOADER_LAYERS_DISABLE=\S+', ' ', args)
args = re.sub(r'\S*l4d2-venus-launch\.sh', ' ', args)
args = ' '.join(args.split())
if args:
    esc = args.replace('\\', '\\\\').replace('"', '\\"')
    if '%command%' not in esc: esc += ' %command%'
    seg2 = re.sub(r'\n\s*"LaunchOptions"\s*"[^"]*"',
                  '\n\t\t\t\t\t\t"LaunchOptions"\t\t"%s"' % esc, seg)
    print("[l4d2-venus] 已移除注入项，保留用户参数: %s" % args)
else:
    seg2 = re.sub(r'\n\s*"LaunchOptions"\s*"[^"]*"', '', seg)
    print("[l4d2-venus] LaunchOptions 已清空")
open(p, 'w').write(data[:brace] + seg2 + data[k:])
PY
  fi
  say "注: 已构建的服务端留存在 $BRIDGE_HOME（重建耗时，需要时手动删除）"
  say "== 卸载完成。重启 Steam 后 550 回到 Steam 默认（原生）路线 =="
}

case "${1:-}" in
  --status) status; exit 0 ;;
  --uninstall) uninstall; exit 0 ;;
esac

if pgrep -f "ubuntu12_32/steam" >/dev/null 2>&1; then
  say "警告: Steam 正在运行——vdf 改动可能被覆盖。建议先完全退出 Steam 再运行，或运行后重启 Steam。"
fi

say "== L4D2 硬件渲染修复（原生 32 位 -vulkan -> Venus -> gf Vulkan）=="
detect_render_node || exit 1
build_server || exit 1
install_unit || exit 1
write_host_runner
write_launcher
clear_compat_tool
config_launch_options
say "完成。重启 Steam 后启动《Left 4 Dead 2》即走 7G100 硬件渲染。"
say "提醒: (1) 验证 GPU 生效：游戏内控制台 mat_info 应显示 LISUAN（而非 llvmpipe）。"
say "      (2) 手动排障: vtest-forward/run-l4d2-venus.sh --test （32 位 vkcube 冒烟）。"
say "      (3) 崩服日志: journalctl --user -u l4d2-venus"
