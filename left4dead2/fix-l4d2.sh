#!/bin/bash
#
# fix-l4d2.sh — 《求生之路 2》(Left 4 Dead 2, AppID 550) 在砺算 7G100 上的
#               "启动后无响应 / 进游戏黑屏" 修复脚本（幂等）
#
# 症状 1：从 Steam 启动后窗口卡死 / 无响应，数分钟后被 Steam 移除
#        （console_log: "Game process removed: AppID 550"），内核无 traps/segfault。
# 症状 2：进得去游戏但整屏黑（有声音无画面）——见下方根因 4。
#
# 根因（2026-09-15 实测）：
#   1) L4D2 是 **原生 Linux 32 位** 游戏（hl2_linux / engine.so 均为 ELF 32），
#      而 Steam 默认把它放进 **pressure-vessel 容器**（SteamLinuxRuntime_soldier
#      + scout）运行——容器内 32 位图形/音频栈不完整，游戏挂起。
#   2) 宿主直跑时暴露真正的加载错误：`Unable to load module engine.so`，
#      `ldd engine.so` 显示缺 **32 位 libopenal.so.1**（OpenAL 音频库）。
#   3) 厂商 **只提供 64 位** gf 驱动（无 32 位 gf_dri.so），故 32 位游戏
#      只能走 Mesa 32 位软渲染（宿主有 /usr/lib/i386-linux-gnu/dri/swrast_dri.so）。
#   4) **黑屏真因**：`bin/vguimatsurface.so`（VGUI 界面渲染模块）缺
#      **32 位 libpng12.so.0**。该模块加载失败时游戏窗口能创建、但界面渲染
#      不出来 —— 表现为"进游戏后全黑"。Ubuntu 24.04 仓库已移除 libpng12,
#      本脚本自动把它部署到**游戏自己的 bin/ 目录**（不改系统、可随游戏走）。
#
# 修复：宿主直跑（STEAM_RUNTIME=0）+ 32 位 Mesa 软渲染 + 补齐 32 位依赖。
#
# Usage:
#   bash fix-l4d2.sh                 # 补依赖 + 部署 libpng12 + 写入宿主直跑启动选项（幂等）
#   bash fix-l4d2.sh --status        # 查看依赖/启动选项/进程状态
#   bash fix-l4d2.sh --launch        # 宿主直跑一次（用于排查，不写配置）
#   bash fix-l4d2.sh --uninstall     # 还原：清空启动选项并移除部署的 libpng12
#
# License: Apache-2.0
set -uo pipefail

GAME="$HOME/.local/share/Steam/steamapps/common/Left 4 Dead 2"
APPID=550
# 32 位库 -> 提供它的 deb 包（i386 架构）
declare -A PKG=(
  [libopenal.so.1]=libopenal1:i386
  [libSDL2-2.0.so.0]=libsdl2-2.0-0:i386
  [libtcmalloc_minimal.so.4]=libgoogle-perftools4:i386
  [libdbus-1.so.3]=libdbus-1-3:i386
  [libfontconfig.so.1]=libfontconfig1:i386
  [libfreetype.so.6]=libfreetype6:i386
  [libpng16.so.16]=libpng16-16:i386
)

# 只用于 Steam 覆盖层内嵌浏览器（libcef/chromehtml），缺失**不影响游戏本体**；
# 其中 libgcrypt.so.11 已绝迹（Ubuntu 14.04 时代），放弃安装，只做提示。
OPTIONAL_MODULES="chromehtml.so libcef.so"
OPTIONAL_LIBS="libgtk-x11-2.0.so.0 libgdk-x11-2.0.so.0 libXi.so.6 libcups.so.2
libnss3.so libnssutil3.so libsmime3.so libplc4.so libnspr4.so libgcrypt.so.11"
OPTIONAL_PKGS="libgtk2.0-0:i386 libxi6:i386 libcups2:i386 libnss3:i386 libnspr4:i386"

say() { echo "[l4d2] $*"; }
err() { echo "[l4d2] !! $*" >&2; }

# 启动选项：宿主直跑 + 32 位软渲染（绕开容器，绕开厂商 32 位驱动缺失）
LAUNCH="STEAM_RUNTIME=0 __GLX_VENDOR_LIBRARY_NAME=mesa LIBGL_ALWAYS_SOFTWARE=1 %command%"

STEAMRT_PNG="$HOME/.local/share/Steam/steamapps/common/SteamLinuxRuntime/steam-runtime/lib/i386-linux-gnu/libpng12.so.0.46.0"
PNG_URL="http://mirrors.aliyun.com/ubuntu/pool/main/libp/libpng/libpng12-0_1.2.54-1ubuntu1.1_i386.deb"

is_optional() {
  local lib="$1" o
  for o in $OPTIONAL_LIBS; do [[ "$lib" == "$o" ]] && return 0; done
  return 1
}

check_deps() {
  [ -d "$GAME/bin" ] || { err "未找到游戏目录: $GAME"; return 1; }
  local f lib required="" optional=""
  # 逐个扫描 bin/ 下所有模块（不能只看 engine.so —— 黑屏真凶是 vguimatsurface.so）
  while IFS= read -r f; do
    while IFS= read -r lib; do
      if is_optional "$lib"; then
        [[ "$optional" == *"$lib"* ]] || optional="$optional $lib"
      else
        [[ "$required" == *"$lib"* ]] || required="$required $lib"
      fi
    done < <(cd "$GAME/bin" && LD_LIBRARY_PATH=".:${LD_LIBRARY_PATH:-}" \
               ldd "$f" 2>/dev/null | grep "not found" | awk '{print $1}')
  done < <(ls "$GAME/bin"/*.so 2>/dev/null)

  if [ -z "$required" ]; then
    say "32 位依赖检查：全部模块依赖齐全 ✅"
  else
    err "存在缺失的 32 位依赖（会导致加载失败 / 黑屏）："
    local pkg_list=()
    for lib in $required; do
      echo "    - $lib${PKG[$lib]:+  (包名 ${PKG[$lib]})}"
      [ -n "${PKG[$lib]:-}" ] && pkg_list+=("${PKG[$lib]}")
    done
    if [ ${#pkg_list[@]} -gt 0 ]; then
      echo
      echo "    安装命令："
      echo "      sudo dpkg --add-architecture i386 && sudo apt update"
      echo "      sudo apt install -y ${pkg_list[*]}"
    fi
  fi

  if [ -n "$optional" ]; then
    echo
    say "（可忽略）Steam 覆盖层内嵌浏览器缺失的库，与游戏本体无关："
    echo "    $(echo $optional)"
    echo "    可选安装：sudo apt install -y $OPTIONAL_PKGS"
    echo "    注：其中 libgcrypt.so.11 已无源可装，不必理会。"
  fi
  [ -z "$required" ]
}

ensure_libpng12() {
  local dst="$GAME/bin/libpng12.so.0"
  if [ -f "$dst" ]; then
    say "libpng12.so.0 已就绪（$dst）"
    return 0
  fi
  say "部署 32 位 libpng12.so.0（VGUI 界面渲染必需，Ubuntu 24.04 已从仓库移除）…"
  if curl -sf --max-time 90 -o /tmp/libpng12-i386.deb "$PNG_URL"; then
    dpkg -x /tmp/libpng12-i386.deb /tmp/libpng12-x 2>/dev/null \
      && cp -aL /tmp/libpng12-x/lib/i386-linux-gnu/libpng12.so.0.54.0 "$dst" \
      && chmod 644 "$dst"
    rm -rf /tmp/libpng12-i386.deb /tmp/libpng12-x
    say "已部署 libpng12.so.0（1.2.54, i386）→ $dst"
  elif [ -f "$STEAMRT_PNG" ]; then
    cp -aL "$STEAMRT_PNG" "$dst" && chmod 644 "$dst"
    say "已由 Steam 自带运行时部署（离线回退, 0.46）→ $dst"
  else
    err "无法获取 32 位 libpng12.so.0（无网络且 Steam 运行时缺失）"
    return 1
  fi
  return 0
}

lc_path() {
  # 取 550 的 localconfig.vdf（遍历所有 Steam 用户）
  local f
  for f in "$HOME"/.local/share/Steam/userdata/*/config/localconfig.vdf; do
    [ -f "$f" ] && echo "$f" && return 0
  done
  return 1
}

status() {
  say "== 游戏目录 =="
  [ -d "$GAME" ] && echo "  $GAME ✅" || { err "  缺失"; return 1; }
  say "== 32 位依赖 =="
  check_deps
  say "== libpng12（VGUI 渲染必需） =="
  [ -f "$GAME/bin/libpng12.so.0" ] \
    && echo "  已部署 ✅" \
    || echo "  缺失（会导致进游戏黑屏）"
  say "== 启动选项（LaunchOptions 550） =="
  if lc_path >/dev/null; then
    local lc; lc="$(lc_path)"
    python3 - "$lc" "$APPID" <<'PY'
import re, sys
p, app = sys.argv[1], sys.argv[2]
data = open(p).read()
i = data.find('"apps"'); j = data.find('"%s"' % app, i)
if j < 0:
    print("  未设置（Steam 里没有该游戏的启动选项；游戏将走默认容器路径）"); sys.exit(0)
brace = data.find('{', j); depth = 1; k = brace + 1
while depth > 0:
    if data[k] == '{': depth += 1
    elif data[k] == '}': depth -= 1
    k += 1
m = re.search(r'"LaunchOptions"\s*"([^"]*)"', data[brace:k])
print("  " + (m.group(1) if m else "未设置"))
PY
  fi
  say "== 进程 =="
  pgrep -f "hl2_linux" >/dev/null && echo "  游戏运行中" || echo "  未运行"
}

write_launch() {
  local lc
  lc="$(lc_path)" || { err "找不到 localconfig.vdf（先启动一次游戏）"; return 1; }
  if pgrep -f "ubuntu12_32/steam" >/dev/null 2>&1; then
    say "提示: Steam 正在运行——建议先退出 Steam 再改，否则可能被内存中的值覆盖。"
  fi
  python3 - "$lc" "$APPID" "$LAUNCH" <<'PY'
import re, sys
p, app, lo = sys.argv[1], sys.argv[2], sys.argv[3]
data = open(p).read()
i = data.find('"apps"'); j = data.find('"%s"' % app, i)
if j < 0:
    sys.exit("[l4d2] localconfig 无 %s 段（先在 Steam 里启动一次游戏）" % app)
brace = data.find('{', j); depth = 1; k = brace + 1
while depth > 0:
    if data[k] == '{': depth += 1
    elif data[k] == '}': depth -= 1
    k += 1
seg = data[brace:k]
m = re.search(r'\n\s*"LaunchOptions"\s*"([^"]*)"', seg)
old = m.group(1) if m else ""
# 保留用户已有参数（如 -language schinese），只去掉本脚本注入的环境变量后合并（幂等）
args = old.replace('%command%', ' ')
for kv in ('STEAM_RUNTIME=0', '__GLX_VENDOR_LIBRARY_NAME=mesa', 'LIBGL_ALWAYS_SOFTWARE=1'):
    args = args.replace(kv, ' ')
args = ' '.join(args.split())
new = (lo + (' ' + args if args else '') + ' %command%')
esc = new.replace('\\', '\\\\').replace('"', '\\"')
if m:
    seg2 = re.sub(r'\n\s*"LaunchOptions"\s*"[^"]*"',
                  '\n\t\t\t\t\t\t"LaunchOptions"\t\t"%s"' % esc, seg)
else:
    seg2 = seg[:1] + '\n\t\t\t\t\t\t"LaunchOptions"\t\t"%s"' % esc + seg[1:]
open(p, 'w').write(data[:brace] + seg2 + data[k:])
print("[l4d2] LaunchOptions 已写入: %s" % new)
PY
}

clear_launch() {
  local lc
  lc="$(lc_path)" || { err "找不到 localconfig.vdf"; return 1; }
  python3 - "$lc" "$APPID" <<'PY'
import re, sys
p, app = sys.argv[1], sys.argv[2]
data = open(p).read()
i = data.find('"apps"'); j = data.find('"%s"' % app, i)
if j < 0:
    print("[l4d2] 无需清理"); sys.exit(0)
brace = data.find('{', j); depth = 1; k = brace + 1
while depth > 0:
    if data[k] == '{': depth += 1
    elif data[k] == '}': depth -= 1
    k += 1
seg = data[brace:k]
m = re.search(r'\n\s*"LaunchOptions"\s*"([^"]*)"', seg)
if not m:
    print("[l4d2] 无启动选项，无需清理"); sys.exit(0)
old = m.group(1)
# 只移除本脚本注入的环境变量，保留用户自己的参数（如 -language schinese）
args = old.replace('%command%', ' ')
for kv in ('STEAM_RUNTIME=0', '__GLX_VENDOR_LIBRARY_NAME=mesa', 'LIBGL_ALWAYS_SOFTWARE=1'):
    args = args.replace(kv, ' ')
args = ' '.join(args.split())
if args:
    esc = args.replace('\\', '\\\\').replace('"', '\\"')
    if '%command%' not in args:
        esc = esc + ' %command%'
    seg2 = re.sub(r'\n\s*"LaunchOptions"\s*"[^"]*"',
                  '\n\t\t\t\t\t\t"LaunchOptions"\t\t"%s"' % esc, seg)
    print("[l4d2] 已移除修复参数，保留: %s" % args)
else:
    seg2 = re.sub(r'\n\s*"LaunchOptions"\s*"[^"]*"', '', seg)
    print("[l4d2] 启动选项已清空（还原为 Steam 默认容器路径）")
if seg2 == seg:
    print("[l4d2] 无本脚本写入的启动选项"); sys.exit(0)
open(p, 'w').write(data[:brace] + seg2 + data[k:])
PY
}

launch_once() {
  cd "$GAME" || return 1
  say "宿主直跑测试（$LAUNCH）…"
  STEAM_RUNTIME=0 __GLX_VENDOR_LIBRARY_NAME=mesa LIBGL_ALWAYS_SOFTWARE=1 \
    ./hl2.sh -game left4dead2 -steam
}

case "${1:-}" in
  --status) status; exit 0 ;;
  --launch) launch_once; exit $? ;;
  --uninstall)
    clear_launch
    rm -f "$GAME/bin/libpng12.so.0" "$GAME/bin/libpng12.so.0.54.0" \
      && say "已移除部署的 libpng12.so.0"
    exit 0 ;;
esac

say "== L4D2 黑屏 / 无响应修复 =="
check_deps
echo
ensure_libpng12
echo
write_launch
say "完成。重启 Steam 后从库里启动《Left 4 Dead 2》即可（宿主直跑 + 32 位软渲染）。"
say "注意: 厂商无 32 位 gf 驱动，本游戏只能软渲染——请在游戏内把分辨率/画质调低。"
