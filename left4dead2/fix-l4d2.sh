#!/bin/bash
# fix-l4d2.sh — 《求生之路 2》(Left 4 Dead 2, AppID 550) 在砺算 7G100 上走
#               【显卡硬件渲染】（Proton new WoW64 + DXVK -> 64 位 gf Vulkan）
#
# ⚠ 方案变更（2026-10-06，按用户硬性要求）：游戏一律禁用软件渲染，必须 GPU 加速。
#   旧方案（本文件历史版本）：原生 Linux 32 位 + STEAM_RUNTIME=0 + Mesa llvmpipe
#   软渲染 —— 厂商只有 64 位 gf 驱动（无 32 位 gf_dri.so / 32 位 Vulkan），
#   原生路线永远到不了 GPU，已废弃。为旧方案装的 32 位库（libopenal1:i386、
#   bin/libpng12.so.0 等）保留无害，仅服务原生版回退。
#
# 原理：游戏目录自带 Windows 版（left4dead2.exe）。32 位 Windows 游戏走
#   Proton new WoW64：PROTON_USE_WOW64=1 让 32 位游戏进程经 wine wow64 thunk
#   调用【64 位】gf Vulkan ICD，DXVK 把 DX9 翻译成 Vulkan -> 7G100 硬件渲染。
#
#   与随从大师（fix-minion-masters-7g100-hw.sh）同源的五层障碍解法（实战验证）：
#   1) gf 驱动发行版白名单：libvulkan_gf.so 读 /etc/os-release，不认 Steam
#      Runtime 容器 -> 枚举 0 设备。LD_PRELOAD hook 重定向到伪造的 Ubuntu 文件。
#   2) 新 Vulkan loader（1.4.x）丢弃"协商版本 0"的老式 ICD、且校验被厂商裁剪的
#      实例扩展 -> 协商垫片 ICD libvulkan_gf_shim.so（VK_ICD_FILENAMES 喂给 loader）。
#   3) Steam overlay Vulkan 层绕过垫片直连真 ICD -> 崩溃，VK_LOADER_LAYERS_DISABLE 禁用。
#   4) vkCreateDevice 依赖伴生库 libSC.so（不在容器库搜索路径）-> 软链进 Proton
#      自带库目录；驱动库副本放 home（容器内 /run/host/usr/lib 不可见）。
#   5) DXVK v3.x 硬要求 VK_KHR_load_store_op_none / VK_KHR_maintenance5，gf（22.0.4）
#      没有 -> CreateDevice 返回 -1。目标 Proton 的 DXVK 为 v3.x 时自动降级为
#      Proton 8.0 的 v2.3（Proton 11.0 当前已是 v2.3，无需降级）。
#
#   另：winex11 在 wow64 下初始化 OpenGL 会崩 -> __GLX_VENDOR_LIBRARY_NAME=mesa
#      + LIBGL_ALWAYS_SOFTWARE=1 让 wine 的 GL 探测走 llvmpipe（仅探测用，
#      游戏 actual 渲染走 DXVK -> gf Vulkan 硬件，不受影响）。
#
# 用法：
#   bash fix-l4d2.sh             # 安装/修复（幂等；建议先完全退出 Steam）
#   bash fix-l4d2.sh --status    # 查看各环节状态
#   bash fix-l4d2.sh --uninstall # 卸载：还原 Steam 配置 / DXVK / libSC 链接
#
# 共享 Proton 说明：目标 Proton 目录可能同时被随从大师加固过（libSC 链接、
#   DXVK 版本）。两边脚本都幂等且解法同源，互相重跑只会"改指自己的副本"，
#   内容一致无冲突；卸载本修复只回收指向本 DEPLOY 的链接。
# L4D2_PROTON_DIR 环境变量可显式指定目标 Proton 目录（此时不改写
#   CompatToolMapping，由你在 Steam 界面里选择）。
set -u

APPID=550
GAME="$HOME/.local/share/Steam/steamapps/common/Left 4 Dead 2"
REPO="$(cd "$(dirname "$0")/.." && pwd)"
ASSETS="$REPO/minion-masters/assets-mm-gf-vulkan"     # hook/shim 源码与模板（与随从大师共享）
DEPLOY="$(cd "$(dirname "$0")" && pwd)/gf-compat"     # 本游戏的运行时部署目录（自持，不依赖随从大师；deploy_hook 会创建）
PROTON8="$HOME/.local/share/Steam/steamapps/common/Proton 8.0"   # DXVK v2.3 来源
PFX="$HOME/.local/share/Steam/steamapps/compatdata/$APPID/pfx"
CONFIG_VDF="$HOME/.local/share/Steam/config/config.vdf"
LOCALCONFIG_VDF=$(ls ~/.local/share/Steam/userdata/*/config/localconfig.vdf 2>/dev/null | head -1)

# 目标 Proton（加固对象）：L4D2_PROTON_DIR > Steam 中 550 当前选择 > 默认 Experimental
PROTON="${L4D2_PROTON_DIR:-$HOME/.local/share/Steam/steamapps/common/Proton - Experimental}"

say() { echo "[l4d2-gpu] $*"; }
err() { echo "[l4d2-gpu] !! $*" >&2; }

# ---- 目标 Proton 解析 ----
proton_name_to_dir() {
  local name="$1" base="$HOME/.local/share/Steam/steamapps/common" ver="${1#proton_}" c
  case "$name" in
    proton_experimental)
      c="Proton - Experimental"
      [ -d "$base/$c" ] && { echo "$base/$c"; return 0; }
      return 1 ;;
    *)
      for c in "Proton ${ver}.0" "Proton ${ver}" "Proton ${ver//_/.}" "Proton ${ver} (Beta)"; do
        [ -d "$base/$c" ] && { echo "$base/$c"; return 0; }
      done
      return 1 ;;
  esac
}

if [ -z "${L4D2_PROTON_DIR:-}" ]; then
  SEL_TOOL="$(python3 - "$CONFIG_VDF" <<'PY'
import re, sys
try:
    data = open(sys.argv[1]).read()
except (OSError, IndexError):
    sys.exit(0)
m = re.search(r'"CompatToolMapping"\s*\n\s*\{', data)
if not m:
    sys.exit(0)
seg_start = m.end(); depth = 1; i = seg_start
while i < len(data) and depth > 0:
    if data[i] == '{': depth += 1
    elif data[i] == '}': depth -= 1
    i += 1
mm = re.search(r'"550"\s*\{[^}]*?"name"\s*"([^"]+)"', data[seg_start:i-1], flags=re.S)
print(mm.group(1) if mm else "")
PY
)"
  if [ -n "$SEL_TOOL" ]; then
    if SEL_DIR="$(proton_name_to_dir "$SEL_TOOL")" && [ -d "$SEL_DIR" ]; then
      PROTON="$SEL_DIR"
      say "检测到 550 当前 Proton: $SEL_TOOL -> $SEL_DIR（自动加固它）"
    else
      say "选择 $SEL_TOOL 但本地无对应目录，回退默认 Experimental"
    fi
  else
    say "Steam 未记录 550 的 Proton 选择，按默认 Experimental 加固"
  fi
fi

# 存在性检查仅约束"安装"模式；--uninstall 允许残缺环境下部分还原
case "${1:-}" in
  --uninstall) ;;
  *)
    [ -d "$PROTON" ] || { err "目标 Proton 不存在: $PROTON"; exit 1; }
    [ -d "$GAME" ] || { err "未找到游戏目录: $GAME"; exit 1; }
    [ -f "$GAME/left4dead2.exe" ] || { err "缺 Windows 版主程序 left4dead2.exe（Steam 校验文件）"; exit 1; }
    [ -d "$ASSETS" ] || { err "缺共享资产目录: $ASSETS"; exit 1; }
    ;;
esac

# ---------- 1) 部署 hook / 垫片 ICD / 驱动库副本（home 内，容器可见） ----------
deploy_hook() {
  mkdir -p "$DEPLOY/lib"
  cp "$ASSETS/os-release" "$DEPLOY/os-release"
  command -v gcc >/dev/null || { err "需要 gcc 重编 hook/shim（源码内路径需改写为 $DEPLOY）"; return 1; }

  # 1a. os-release 重定向 hook（源码内伪造路径写死 ~/gf-compat，sed 改写后编译）
  sed "s|/home/ubuntu/gf-compat/os-release|$DEPLOY/os-release|g" \
    "$ASSETS/osrelease_redirect.c" > "$DEPLOY/.osr.c"
  gcc -shared -fPIC -o "$DEPLOY/lib/osrelease_redirect.so" "$DEPLOY/.osr.c" -ldl \
    && rm -f "$DEPLOY/.osr.c"
  say "hook 已部署: $DEPLOY/lib/osrelease_redirect.so"

  # 1b. 协商垫片 ICD（dlopen 路径同样写死 ~/gf-compat，sed 改写）
  sed "s|/home/ubuntu/gf-compat|$DEPLOY|g" "$ASSETS/libvulkan_gf_shim.c" > "$DEPLOY/.shim.c"
  gcc -shared -fPIC -o "$DEPLOY/lib/libvulkan_gf_shim.so" "$DEPLOY/.shim.c" \
    && rm -f "$DEPLOY/.shim.c"
  cp "$ASSETS/gf_icd.json" "$DEPLOY/gf_icd.json"   # json 内是相对路径 lib/...，直接可用
  say "垫片 ICD 已部署: $DEPLOY/lib/libvulkan_gf_shim.so (+ $DEPLOY/gf_icd.json)"

  # 1c. 驱动库副本（容器内以 home 路径可见）
  # 注意：厂商源库为 444 只读，cp 会保留权限位导致二次覆盖 EACCES —— 先删再拷
  rm -f "$DEPLOY/lib/libvulkan_gf.so" "$DEPLOY/lib/libSC.so"
  cp /usr/lib/x86_64-linux-gnu/libvulkan_gf.so "$DEPLOY/lib/libvulkan_gf.so"
  cp /usr/lib/x86_64-linux-gnu/libSC.so "$DEPLOY/lib/libSC.so"
  say "驱动库副本已更新: $DEPLOY/lib/{libvulkan_gf.so,libSC.so}"
}

# ---------- 2) libSC.so 软链进 Proton 库目录（vkCreateDevice 依赖） ----------
link_libsc() {
  local dst="$PROTON/files/lib/x86_64-linux-gnu/libSC.so"
  local want="$DEPLOY/lib/libSC.so"
  local cur
  cur=$(readlink "$dst" 2>/dev/null || true)
  if [ "$cur" = "$want" ]; then
    say "libSC.so 链接已存在（-> 本 DEPLOY 副本）"
  else
    ln -sf "$want" "$dst"
    say "libSC.so 已链接 -> $dst"
  fi
}

# ---------- 3) DXVK 保障：目标 Proton 为 v3.x 时降级为 v2.3 ----------
ensure_dxvk() {
  local de="$PROTON/files/lib/wine/dxvk"
  local ver
  ver=$(head -1 "$de/version" 2>/dev/null)
  case "$ver" in
    *v3.*)
      [ -d "$PROTON8" ] || { err "目标 Proton 的 DXVK=$ver 需降级，但未安装 Proton 8.0"; return 1; }
      local d8="$PROTON8/dist"
      [ -d "$de.bak-v3" ] || cp -r "$de" "$de.bak-v3"
      chmod -R u+w "$de" 2>/dev/null
      local f
      for f in d3d9.dll d3d10core.dll d3d11.dll dxgi.dll openvr_api_dxvk.dll; do
        [ -f "$d8/lib/wine/dxvk/$f" ] && cp "$d8/lib/wine/dxvk/$f" "$de/i386-windows/$f"
        [ -f "$d8/lib64/wine/dxvk/$f" ] && cp "$d8/lib64/wine/dxvk/$f" "$de/x86_64-windows/$f"
      done
      local v8
      v8=$(find "$d8" -name version -path "*dxvk*" | head -1)
      [ -n "$v8" ] && cp "$v8" "$de/version"
      # 强制 Proton 重同步 dll 到前缀
      rm -f "$PFX/drive_c/windows/system32/"{d3d9,d3d10core,d3d11,dxgi}.dll \
            "$PFX/drive_c/windows/syswow64/"{d3d9,d3d10core,d3d11,dxgi}.dll 2>/dev/null
      say "DXVK 已降级 $ver -> $(head -1 "$de/version")"
      ;;
    *v2.*)
      say "DXVK 版本 $ver（gf 兼容，无需降级）"
      ;;
    "")
      err "未找到 $de/version（Proton 目录结构异常）"; return 1 ;;
    *)
      say "DXVK 版本 $ver（非 v3.x，保持不动）"
      ;;
  esac
  return 0
}

# ---------- 4) Steam 配置：CompatToolMapping + LaunchOptions ----------
config_steam() {
  # 4a. config.vdf：未显式指定 PROTON 目录时，把 550 绑到目标工具
  if [ -n "${L4D2_PROTON_DIR:-}" ]; then
    say "L4D2_PROTON_DIR 已指定 -> 不改写 CompatToolMapping，请在 Steam 里手动选择该 Proton"
  else
    local tool="proton_experimental"
    [ -n "${SEL_TOOL:-}" ] && [ -n "${SEL_DIR:-}" ] && [ "$PROTON" = "$SEL_DIR" ] && tool="$SEL_TOOL"
    python3 - "$CONFIG_VDF" "$tool" <<'PY'
import re, sys
p, tool = sys.argv[1], sys.argv[2]
data = open(p).read()
m = re.search(r'"CompatToolMapping"\s*\n\s*\{', data)
if not m: sys.exit("[l4d2-gpu] config.vdf 无 CompatToolMapping")
seg_start = m.end(); depth = 1; i = seg_start
while i < len(data) and depth > 0:
    if data[i] == '{': depth += 1
    elif data[i] == '}': depth -= 1
    i += 1
seg = data[seg_start:i-1]
if '"550"' in seg:
    seg2 = re.sub(r'("550"\s*\{[^}]*?"name"\s*")[^"]*(")', r'\g<1>' + tool + r'\g<2>', seg, flags=re.S)
    if seg2 != seg:
        open(p, 'w').write(data[:seg_start] + seg2 + data[i-1:])
        print("[l4d2-gpu] CompatToolMapping 550 更新为 %s" % tool)
    else:
        print("[l4d2-gpu] CompatToolMapping 550 已是 %s" % tool)
else:
    entry = ('\t\t\t\t\t"550"\n\t\t\t\t\t{\n\t\t\t\t\t\t"name"\t\t"%s"\n'
             '\t\t\t\t\t\t"config"\t\t""\n\t\t\t\t\t\t"priority"\t\t"250"\n'
             '\t\t\t\t\t}\n\t\t\t\t' % tool)
    open(p, 'w').write(data[:i-1] + entry + data[i-1:])
    print("[l4d2-gpu] CompatToolMapping 550 -> %s 已写入" % tool)
PY
  fi

  # 4b. localconfig.vdf：写入 LaunchOptions（保留用户已有参数，如 -language schinese）
  [ -n "$LOCALCONFIG_VDF" ] || { err "无 localconfig.vdf（先在 Steam 里启动一次游戏）"; return 1; }
  python3 - "$LOCALCONFIG_VDF" "$DEPLOY" <<'PY'
import re, sys
p, deploy = sys.argv[1], sys.argv[2]
data = open(p).read()
i = data.find('"apps"'); j = data.find('"550"', i)
if j < 0: sys.exit("[l4d2-gpu] localconfig 无 550 段（先在 Steam 里启动一次游戏）")
brace = data.find('{', j); depth = 1; k = brace + 1
while depth > 0:
    if data[k] == '{': depth += 1
    elif data[k] == '}': depth -= 1
    k += 1
seg = data[brace:k]
m = re.search(r'\n\s*"LaunchOptions"\s*"([^"]*)"', seg)
old = m.group(1) if m else ""
# 剥掉本修复的注入项与 %command%，保留用户自有参数
user_args = old.replace('%command%', ' ')
for tok in ('PROTON_USE_WOW64=1', '__GLX_VENDOR_LIBRARY_NAME=mesa',
            'LIBGL_ALWAYS_SOFTWARE=1'):
    user_args = user_args.replace(tok, ' ')
user_args = re.sub(r'LD_PRELOAD=\S+', ' ', user_args)
user_args = re.sub(r'VK_ICD_FILENAMES=\S+', ' ', user_args)
user_args = re.sub(r'VK_LOADER_LAYERS_DISABLE=\S+', ' ', user_args)
user_args = ' '.join(user_args.split())
env = ("PROTON_USE_WOW64=1 "
       "LD_PRELOAD=%s/lib/osrelease_redirect.so "
       "__GLX_VENDOR_LIBRARY_NAME=mesa LIBGL_ALWAYS_SOFTWARE=1 "
       "VK_ICD_FILENAMES=%s/gf_icd.json "
       "VK_LOADER_LAYERS_DISABLE=VK_LAYER_VALVE_steam_overlay_64,VK_LAYER_VALVE_steam_overlay_32"
       % (deploy, deploy))
new = env + ' %command%' + ((' ' + user_args) if user_args else '')
esc = new.replace('\\', '\\\\').replace('"', '\\"')
if m:
    seg2 = re.sub(r'\n\s*"LaunchOptions"\s*"[^"]*"',
                  '\n\t\t\t\t\t\t"LaunchOptions"\t\t"%s"' % esc, seg)
else:
    seg2 = seg[:1] + '\n\t\t\t\t\t\t"LaunchOptions"\t\t"%s"' % esc + seg[1:]
open(p, 'w').write(data[:brace] + seg2 + data[k:])
print("[l4d2-gpu] LaunchOptions 已写入%s" %
      ("（保留用户参数: %s）" % user_args if user_args else ""))
PY
}

# ---------- 状态 ----------
status() {
  echo "== 游戏目录 ==";      [ -f "$GAME/left4dead2.exe" ] && echo "  $GAME（含 Windows 版 left4dead2.exe）✅" || err "  缺 left4dead2.exe"
  echo "== 目标 Proton ==";   echo "  $PROTON $([ -d "$PROTON" ] && echo ✅ || echo 缺失)"
  echo "== DXVK ==";          echo "  $(head -1 "$PROTON/files/lib/wine/dxvk/version" 2>/dev/null || echo 未知)（v3.x 会被降级为 v2.3）"
  echo "== hook ==";          ls "$DEPLOY/lib/osrelease_redirect.so" 2>/dev/null || echo "  缺失"
  echo "== 垫片 ICD ==";      ls "$DEPLOY/lib/libvulkan_gf_shim.so" "$DEPLOY/gf_icd.json" 2>/dev/null || echo "  缺失"
  echo "== 驱动库副本 ==";    ls "$DEPLOY/lib/libvulkan_gf.so" "$DEPLOY/lib/libSC.so" 2>/dev/null || echo "  缺失"
  echo "== libSC 链接 ==";    readlink "$PROTON/files/lib/x86_64-linux-gnu/libSC.so" 2>/dev/null || echo "  缺失"
  echo "== CompatTool ==";    python3 -c "
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
print('  ' + (mm.group(1) if mm else '未绑定（将走默认容器/原生路径）'))
"
  echo "== LaunchOpts ==";    python3 -c "
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
  echo "== 进程 ==";          pgrep -f "hl2.exe\|left4dead2.exe" >/dev/null && echo "  游戏运行中" || echo "  未运行"
}

# ---------- 卸载 ----------
uninstall() {
  say "== 卸载 L4D2 GPU 修复 =="
  if pgrep -f "ubuntu12_32/steam" >/dev/null 2>&1; then
    say "警告: Steam 正在运行，vdf 改动可能在退出时被覆盖。建议先退出 Steam。"
  fi
  # 1. CompatToolMapping 移除 550 条目
  python3 - "$CONFIG_VDF" <<'PY'
import re, sys
try:
    p = sys.argv[1]; data = open(p).read()
except (OSError, IndexError):
    sys.exit(0)
m = re.search(r'"CompatToolMapping"\s*\n\s*\{', data)
if not m:
    print("[l4d2-gpu] config.vdf 无 CompatToolMapping"); sys.exit(0)
seg_start = m.end(); depth = 1; i = seg_start
while i < len(data) and depth > 0:
    if data[i] == '{': depth += 1
    elif data[i] == '}': depth -= 1
    i += 1
seg = data[seg_start:i-1]
seg2 = re.sub(r'\n\s*"550"\s*\n\s*\{[^}]*\}', '', seg)
if seg2 != seg:
    open(p, 'w').write(data[:seg_start] + seg2 + data[i-1:])
    print("[l4d2-gpu] CompatToolMapping 550 条目已移除")
else:
    print("[l4d2-gpu] CompatToolMapping 无 550 条目")
PY
  # 2. LaunchOptions 还原为用户自有参数（剥离本修复注入项）
  if [ -n "$LOCALCONFIG_VDF" ] && [ -f "$LOCALCONFIG_VDF" ]; then
    python3 - "$LOCALCONFIG_VDF" <<'PY'
import re, sys
p = sys.argv[1]; data = open(p).read()
i = data.find('"apps"'); j = data.find('"550"', i)
if j < 0:
    print("[l4d2-gpu] localconfig 无 550 段"); sys.exit(0)
brace = data.find('{', j); depth = 1; k = brace + 1
while depth > 0:
    if data[k] == '{': depth += 1
    elif data[k] == '}': depth -= 1
    k += 1
seg = data[brace:k]
m = re.search(r'\n\s*"LaunchOptions"\s*"([^"]*)"', seg)
if not m:
    print("[l4d2-gpu] 无 LaunchOptions"); sys.exit(0)
args = m.group(1).replace('%command%', ' ')
for tok in ('PROTON_USE_WOW64=1', '__GLX_VENDOR_LIBRARY_NAME=mesa',
            'LIBGL_ALWAYS_SOFTWARE=1'):
    args = args.replace(tok, ' ')
args = re.sub(r'LD_PRELOAD=\S+', ' ', args)
args = re.sub(r'VK_ICD_FILENAMES=\S+', ' ', args)
args = re.sub(r'VK_LOADER_LAYERS_DISABLE=\S+', ' ', args)
args = ' '.join(args.split())
if args:
    esc = args.replace('\\', '\\\\').replace('"', '\\"')
    if '%command%' not in esc:
        esc += ' %command%'
    seg2 = re.sub(r'\n\s*"LaunchOptions"\s*"[^"]*"',
                  '\n\t\t\t\t\t\t"LaunchOptions"\t\t"%s"' % esc, seg)
    print("[l4d2-gpu] 已移除注入项，保留用户参数: %s" % args)
else:
    seg2 = re.sub(r'\n\s*"LaunchOptions"\s*"[^"]*"', '', seg)
    print("[l4d2-gpu] LaunchOptions 已清空")
open(p, 'w').write(data[:brace] + seg2 + data[k:])
PY
  fi
  # 3. DXVK 还原（仅当存在 v3 备份；与随从大师共用同一备份）
  local de="$PROTON/files/lib/wine/dxvk"
  if [ -d "$de.bak-v3" ]; then
    chmod -R u+w "$de" 2>/dev/null
    rm -rf "$de"; cp -r "$de.bak-v3" "$de"; rm -rf "$de.bak-v3"
    rm -f "$PFX/drive_c/windows/system32/"{d3d9,d3d10core,d3d11,dxgi}.dll \
          "$PFX/drive_c/windows/syswow64/"{d3d9,d3d10core,d3d11,dxgi}.dll 2>/dev/null
    say "DXVK 已还原原版 v3.x（备份已消费）"
  else
    say "无 .bak-v3 备份，DXVK 保持现状"
  fi
  # 4. libSC 链接：只回收指向本 DEPLOY 的（不动随从大师的）
  local sc="$PROTON/files/lib/x86_64-linux-gnu/libSC.so"
  if [ "$(readlink "$sc" 2>/dev/null)" = "$DEPLOY/lib/libSC.so" ]; then
    rm -f "$sc"; say "libSC.so 链接已移除（指向本 DEPLOY）"
  else
    say "libSC.so 链接非本 DEPLOY（$(readlink "$sc" 2>/dev/null || echo 无)），保留"
  fi
  # 5. 清理本 DEPLOY 运行时产物（源码在 assets-mm-gf-vulkan/，不受影响）
  rm -rf "$DEPLOY/lib" "$DEPLOY/os-release" "$DEPLOY/gf_icd.json" "$DEPLOY/.osr.c" "$DEPLOY/.shim.c"
  say "DEPLOY 产物已清理: $DEPLOY/{lib,os-release,gf_icd.json}"
  say "== 卸载完成。重启 Steam 后 550 回到 Steam 默认路线 =="
}

case "${1:-}" in
  --status) status; exit 0 ;;
  --uninstall) uninstall; exit 0 ;;
esac

# Steam 运行中改 vdf 会被其内存值覆盖
if pgrep -f "ubuntu12_32/steam" >/dev/null 2>&1; then
  say "警告: Steam 正在运行——vdf 改动可能被覆盖。建议先完全退出 Steam 再运行，或运行后重启 Steam。"
fi

say "== L4D2 GPU 硬件渲染修复（Proton WoW64 + DXVK -> gf Vulkan）=="
deploy_hook || exit 1
link_libsc
ensure_dxvk || exit 1
config_steam
say "完成。重启 Steam 后启动《Left 4 Dead 2》即走 7G100 硬件渲染。"
say "提醒: (1) Proton/容器运行时更新会覆盖 libSC 链接与 DXVK 降级，更新后重跑本脚本（幂等）。"
say "      (2) 验证 GPU 生效：游戏内控制台 mat_info 应显示 LISUAN（而非 llvmpipe）。"
