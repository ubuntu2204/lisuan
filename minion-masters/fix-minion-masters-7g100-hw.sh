#!/bin/bash
# fix-minion-masters-7g100-hw.sh — 让 Minion Masters 在砺算 7G100 上用【本机显卡硬件渲染】
# （区别于早期 llvmpipe 软件渲染路线脚本，该脚本已随方案废弃删除）
#
# 原理（游戏为 32 位 Windows Unity，Proton 需 32 位 Vulkan ICD，但砺算只发布 64 位）：
#   走 Proton Experimental 的 new WoW64（PROTON_USE_WOW64=1），让 32 位游戏经 wine 的
#   wow64 thunk 调用【64 位】gf Vulkan ICD，从而用上 7G100。要解决四层障碍：
#
#   1) gf 驱动的【发行版白名单】：libvulkan_gf.so 启动时读 /etc/os-release，只认
#      Kylin/uos/openEuler/TencentOS/Deepin/Ubuntu，Steam Runtime 容器不在名单 ->
#      枚举 0 个物理设备。用 LD_PRELOAD 把 /etc/os-release 重定向到伪造的 Ubuntu 文件。
#   2) gf 的 vkCreateDevice 依赖伴生库 libSC.so（着色器编译器），它不在容器库搜索路径 ->
#      CreateDevice 返回 -1。把 libSC.so 软链进 Proton 自带库目录（该目录在 proton 设置的
#      LD_LIBRARY_PATH 首位）。
#   3) Experimental 自带 DXVK v3.0.2 硬要求 VK_KHR_load_store_op_none / VK_KHR_maintenance5，
#      gf（驱动 22.0.4）没有 -> DXVK 跳过 7G100。降级为 Proton 8.0 的 DXVK v2.3（不要求这些）。
#   4) BepInEx/Il2CppInterop 的 Chainloader 在 wine11 wow64 下不触发（插件不加载 -> Vivox 崩）。
#      改用 doorstop 直载的纯 P/Invoke 补丁 MMPatch.dll（不依赖 BepInEx Chainloader）。
#
#   另：winex11 在 wow64 下初始化 OpenGL 失败会导致 wine 崩 -> __GLX_VENDOR_LIBRARY_NAME=mesa
#      + LIBGL_ALWAYS_SOFTWARE=1 让其走 llvmpipe GL（仅影响 winex11 探测，不影响 DXVK 用 7G100）。
#
# 2026-08-29 容器运行时（steamrt4）更新后新增的四层适配：
#   5) 容器内 /run/host/usr/lib 不再可见 -> gf ICD 库与 libSC.so 从该路径消失，
#      loader 静默丢弃驱动 / vkCreateDevice 段错误。改为把两库【复制到 $DEPLOY/lib】
#      （home 在容器内一定可见），libSC 软链改指 home 副本。
#   6) 新 Vulkan loader（1.4.x）对 gf 这类"协商版本 0、只导出 3 个 vk_icd* 符号"的
#      老式 ICD 走直接导出符号表路径 -> 静默丢弃；且严格校验实例扩展（厂商裁掉了
#      VK_KHR_external_semaphore_capabilities）-> vkCreateInstance 被拒。
#      用【协商垫片 ICD】libvulkan_gf_shim.so：对外报协商版本 5、全程走
#      vk_icdGetInstanceProcAddr 转发，枚举时补报被裁扩展、vkCreateInstance/
#      vkCreateDevice 转发前剔除真 ICD 不支持的扩展。经自定义 gf_icd.json +
#      VK_ICD_FILENAMES 喂给 loader。
#   7) Steam overlay Vulkan 层经 vk_icdGetPhysicalDeviceProcAddr 直连真 ICD，
#      绕过垫片 -> 崩在 libc。用 VK_LOADER_LAYERS_DISABLE 禁用（容器内 overlay 本就不可用）。
#   8) 32 位地址宽度：游戏进程低 2GB 被 wine 占用 ~1.7GB（MAP_32BIT 常 ENOMEM），
#      而 2~4GB 全空 -> LD_PRELOAD hook 内置【低 4GB 重映射池】：MAP_32BIT 失败或
#      映射落 >4GB 时，重定向到 2.25~4GB 空闲区（WoW64 thunk 可接 <4GB 任意指针）。
#      覆盖所有 mmap 路径（含虚函数分发的未补丁路径），无需再逐条改驱动。
#
# 用法：
#   ./fix-minion-masters-7g100-hw.sh            # 安装/修复（幂等）
#   ./fix-minion-masters-7g100-hw.sh --status   # 检查各环节状态
#   ./fix-minion-masters-7g100-hw.sh --uninstall # 卸载：还原 Steam 配置/Proton/游戏目录
#   ./fix-minion-masters-7g100-hw.sh --inject-winhttp   # pfx 重建后重新注入 winhttp=n,b
set -u
APPID=489520
ASSETS="$(cd "$(dirname "$0")/assets-mm-gf-vulkan" && pwd)"
# 运行时目录：2026-08-31 由 ~/gf-compat 并入仓库，与源码同处 minion-masters/gf-compat/。
# 注意 osrelease_redirect.c / libvulkan_gf_shim.c 内的伪造路径写死为
# /home/ubuntu/gf-compat，因此这里会对源码 sed 改写后重新编译（见 deploy_hook）。
DEPLOY="$(cd "$(dirname "$0")/gf-compat" && pwd)"
# 目标 Proton 目录（加固对象）。解析优先级：
#   1) MM_PROTON_DIR 显式指定（且不改写 CompatToolMapping，保留 Steam 内选择）；
#   2) 自动跟随 Steam 中游戏当前选择的 Proton（读 CompatToolMapping 489520）；
#   3) 默认 Proton - Experimental。
# 复发背景（2026-09-06）：游戏实际用 Proton 11.0 而脚本默认加固 Experimental，
# 且把 CompatToolMapping 打回 experimental——加固与运行对象脱节致游戏不可玩。
# 例（显式指定）：
#   MM_PROTON_DIR="$HOME/.local/share/Steam/steamapps/common/Proton 11.0" bash fix-...sh
PROTON_EX="${MM_PROTON_DIR:-$HOME/.local/share/Steam/steamapps/common/Proton - Experimental}"
PROTON8="$HOME/.local/share/Steam/steamapps/common/Proton 8.0"
GAME="$HOME/.steam/root/steamapps/common/Minion Masters"
PFX="$HOME/.local/share/Steam/steamapps/compatdata/$APPID/pfx"
CONFIG_VDF="$HOME/.local/share/Steam/config/config.vdf"
LOCALCONFIG_VDF=$(ls ~/.local/share/Steam/userdata/*/config/localconfig.vdf 2>/dev/null | head -1)

LAUNCHOPTS="PROTON_USE_WOW64=1 LD_PRELOAD=$DEPLOY/lib/osrelease_redirect.so __GLX_VENDOR_LIBRARY_NAME=mesa LIBGL_ALWAYS_SOFTWARE=1 VK_ICD_FILENAMES=$DEPLOY/gf_icd.json VK_LOADER_LAYERS_DISABLE=VK_LAYER_VALVE_steam_overlay_64,VK_LAYER_VALVE_steam_overlay_32 %command%"

say() { echo "[mm-hw] $*"; }
err() { echo "[mm-hw] !! $*" >&2; }

# ---- 加固目标解析（2026-09-06 固化）----
# 优先级：MM_PROTON_DIR 显式指定 > Steam 中游戏当前选择的 Proton > 默认 Experimental。
# 复发背景：游戏实际用 Proton 11.0，而裸跑脚本默认加固 Experimental 并把
# CompatToolMapping 打回 experimental——导致加固与运行对象脱节、游戏玩不了。
# 现在自动跟随 Steam 里的实际选择，裸跑也能加固正确对象。
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

if [ -z "${MM_PROTON_DIR:-}" ]; then
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
mm = re.search(r'"489520"\s*\{[^}]*?"name"\s*"([^"]+)"',
               data[seg_start:i-1], flags=re.S)
print(mm.group(1) if mm else "")
PY
)"
  if [ -n "$SEL_TOOL" ]; then
    if SEL_DIR="$(proton_name_to_dir "$SEL_TOOL")" && [ -d "$SEL_DIR" ]; then
      PROTON_EX="$SEL_DIR"
      say "检测到游戏当前 Proton: $SEL_TOOL -> $SEL_DIR（自动加固它）"
    else
      say "游戏选择 $SEL_TOOL 但本地未找到对应目录，回退默认 Experimental"
    fi
  else
    say "Steam 中未记录游戏 Proton 选择，按默认 Experimental 加固"
  fi
fi

# 存在性检查仅约束"安装"模式；--uninstall 允许在环境残缺时做部分还原。
case "${1:-}" in
  --uninstall) ;;
  *)
    [ -d "$PROTON_EX" ] || { err "未安装 Proton - Experimental"; exit 1; }
    [ -d "$PROTON8" ] || { err "未安装 Proton 8.0（提供 DXVK v2.3）"; exit 1; }
    [ -d "$GAME" ] || { err "未安装 Minion Masters"; exit 1; }
    ;;
esac

# ---------- 1) 部署 hook / 垫片 / 库副本 ----------
deploy_hook() {
  mkdir -p "$DEPLOY/lib"
  cp "$ASSETS/os-release" "$DEPLOY/os-release"
  # hook 内写死的伪造文件路径是 /home/ubuntu/gf-compat/os-release。
  # DEPLOY 现为仓库内路径，故恒定走 else 重编分支（需要 gcc）；
  # 保留该判断只为 DEPLOY 被显式指回 ~/gf-compat 时可直接用现成产物。
  if [ "$DEPLOY" = "$HOME/gf-compat" ]; then
    cp "$ASSETS/osrelease_redirect.so" "$DEPLOY/lib/osrelease_redirect.so"
  else
    command -v gcc >/dev/null || { err "非默认 DEPLOY 需 gcc 重编 hook"; return 1; }
    sed "s|/home/ubuntu/gf-compat/os-release|$DEPLOY/os-release|g" \
      "$ASSETS/osrelease_redirect.c" > "$DEPLOY/.osr.c"
    gcc -shared -fPIC -o "$DEPLOY/lib/osrelease_redirect.so" "$DEPLOY/.osr.c" -ldl && rm -f "$DEPLOY/.osr.c"
  fi
  say "hook 已部署: $DEPLOY/lib/osrelease_redirect.so"

  # 协商垫片 ICD + 清单（新容器里 /run/host/usr/lib 不可见，ICD 必须放 home）
  if [ "$DEPLOY" = "$HOME/gf-compat" ]; then
    cp "$ASSETS/libvulkan_gf_shim.so" "$DEPLOY/lib/libvulkan_gf_shim.so"
  else
    command -v gcc >/dev/null || { err "非默认 DEPLOY 需 gcc 重编 shim"; return 1; }
    sed "s|/home/ubuntu/gf-compat|$DEPLOY|g" "$ASSETS/libvulkan_gf_shim.c" > "$DEPLOY/.shim.c"
    gcc -shared -fPIC -o "$DEPLOY/lib/libvulkan_gf_shim.so" "$DEPLOY/.shim.c" && rm -f "$DEPLOY/.shim.c"
  fi
  cp "$ASSETS/gf_icd.json" "$DEPLOY/gf_icd.json"
  say "垫片 ICD 已部署: $DEPLOY/lib/libvulkan_gf_shim.so (+ $DEPLOY/gf_icd.json)"

  # 驱动库副本（容器内可见性以 home 为准；宿主机库含用户的低地址映射补丁则一并带入）
  cp /usr/lib/x86_64-linux-gnu/libvulkan_gf.so "$DEPLOY/lib/libvulkan_gf.so"
  cp /usr/lib/x86_64-linux-gnu/libSC.so "$DEPLOY/lib/libSC.so"
  say "驱动库副本已更新: $DEPLOY/lib/{libvulkan_gf.so,libSC.so}"
}

# ---------- 2) libSC.so 软链进 Proton 库目录 ----------
link_libsc() {
  local dst="$PROTON_EX/files/lib/x86_64-linux-gnu/libSC.so"
  local want="$DEPLOY/lib/libSC.so"
  local cur
  cur=$(readlink "$dst" 2>/dev/null || true)
  if [ "$cur" = "$want" ]; then
    say "libSC.so 链接已存在（-> home 副本）"
  else
    # 旧版曾指向 /run/host/...（新容器里已悬空），一律重指 home 副本
    ln -sf "$want" "$dst"
    say "libSC.so 已链接 -> $dst"
  fi
}

# ---------- 3) DXVK 降级为 v2.3（来自 Proton 8.0）----------
downgrade_dxvk() {
  local de="$PROTON_EX/files/lib/wine/dxvk"
  local d8="$PROTON8/dist"
  # 备份原 v3
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
  # 强制 proton 重同步到前缀
  rm -f "$PFX/drive_c/windows/system32/"{d3d9,d3d10core,d3d11,dxgi}.dll \
        "$PFX/drive_c/windows/syswow64/"{d3d9,d3d10core,d3d11,dxgi}.dll 2>/dev/null
  say "DXVK 已降级为 $(head -1 "$de/version" 2>/dev/null)"
}

# ---------- 4) MMPatch + doorstop 直载 ----------
setup_mmpatch() {
  # bin/ 是 dotnet build 产物，是唯一权威来源。
  #
  # 旧逻辑是在assets 根目录副本与 bin/ 之间用 -nt 比时间戳二选一，这不可靠：
  # git 不保留 mtime，换机器 clone 后两者时间戳相同，-nt 判定失败，会误选
  # 根目录那份 2026-08-26 的旧版（启用 Vivox patch，游戏必卡在登录）。
  # 该旧版已归档到 _work/mm-patch-legacy-20260826/，此处固定取 bin/。
  local src="$ASSETS/bin/MMPatch.dll"
  if [ ! -f "$src" ]; then
    err "找不到 $src —— 请先在 assets-mm-gf-vulkan/ 下执行 dotnet build"
    return 1
  fi
  cp "$src" "$GAME/MMPatch.dll"
  [ -f "$GAME/doorstop_config.ini.bak-bepinex" ] || cp "$GAME/doorstop_config.ini" "$GAME/doorstop_config.ini.bak-bepinex"
  sed -i 's|^target_assembly = .*|target_assembly = MMPatch.dll|' "$GAME/doorstop_config.ini"
  say "MMPatch.dll 已部署 ($(basename "$(dirname "$src")")/$(basename "$src"), $(stat -c %s "$src") B), doorstop target 已指向它"
}

# ---------- 5) Steam 配置：CompatToolMapping + LaunchOptions ----------
config_steam() {
  # 5a. config.vdf: CompatToolMapping 489520 -> proton_experimental
  python3 - "$CONFIG_VDF" <<'PY'
import re, sys
p = sys.argv[1]
data = open(p).read()
m = re.search(r'"CompatToolMapping"\s*\n\s*\{', data)
if not m: sys.exit("no CompatToolMapping")
seg_start = m.end(); depth = 1; i = seg_start
while i < len(data) and depth > 0:
    if data[i] == '{': depth += 1
    elif data[i] == '}': depth -= 1
    i += 1
seg = data[seg_start:i-1]
if '"489520"' in seg:
    # 已存在则确保是 proton_experimental
    seg2 = re.sub(r'("489520"\s*\{[^}]*?"name"\s*")[^"]*(")', r'\1proton_experimental\2', seg, flags=re.S)
    if seg2 != seg:
        open(p,'w').write(data[:seg_start] + seg2 + data[i-1:]); print("[mm-hw] CompatToolMapping 更新为 proton_experimental")
    else:
        print("[mm-hw] CompatToolMapping 已是 proton_experimental")
else:
    entry = '\t\t\t\t\t"489520"\n\t\t\t\t\t{\n\t\t\t\t\t\t"name"\t\t"proton_experimental"\n\t\t\t\t\t\t"config"\t\t""\n\t\t\t\t\t\t"priority"\t\t"250"\n\t\t\t\t\t}\n\t\t\t\t'
    open(p,'w').write(data[:i-1] + entry + data[i-1:]); print("[mm-hw] CompatToolMapping 489520 -> proton_experimental 已写入")
PY
  # 5b. localconfig.vdf: 489520 LaunchOptions
  [ -n "$LOCALCONFIG_VDF" ] || { err "无 localconfig.vdf"; return; }
  python3 - "$LOCALCONFIG_VDF" "$LAUNCHOPTS" <<'PY'
import re, sys
p, lo = sys.argv[1], sys.argv[2]
data = open(p).read()
i = data.find('"apps"'); j = data.find('"489520"', i)
if j < 0: sys.exit("[mm-hw] localconfig 无 489520 段（先运行一次游戏）")
brace = data.find('{', j); depth = 1; k = brace + 1
while depth > 0:
    if data[k] == '{': depth += 1
    elif data[k] == '}': depth -= 1
    k += 1
seg = data[brace:k]
seg2 = re.sub(r'\n\s*"LaunchOptions"\s*"[^"]*"', '', seg)
seg2 = seg2[:1] + '\n\t\t\t\t\t\t"LaunchOptions"\t\t"%s"' % lo.replace('\\','\\\\').replace('"','\\"') + seg2[1:]
open(p,'w').write(data[:brace] + seg2 + data[k:])
print("[mm-hw] LaunchOptions 已写入")
PY
}

# ---------- 6) winhttp=n,b 注入（doorstop 依赖）----------
inject_winhttp() {
  local u="$PFX/user.reg"
  [ -f "$u" ] || { err "pfx 未就绪（先启动一次游戏）"; return 1; }
  python3 - "$u" <<'PY'
import re, sys
p = sys.argv[1]
data = open(p).read()
if '"winhttp"="n,b"' in data:
    print("[mm-hw] winhttp override 已存在"); sys.exit(0)
m = re.search(r'\[Software\\\\Wine\\\\DllOverrides\][^\n]*\n', data)
if m:
    new = data[:m.end()] + '"winhttp"="n,b"\n' + data[m.end():]
else:
    new = data + '\n[Software\\\\Wine\\\\DllOverrides] 1787700000\n"winhttp"="n,b"\n'
open(p,'w').write(new); print("[mm-hw] winhttp=n,b 已注入 user.reg")
PY
}

# ---------- 状态 ----------
status() {
  echo "== 加固目标 ==";    echo "$PROTON_EX"
  echo "== hook ==";        ls -la "$DEPLOY/lib/osrelease_redirect.so" 2>/dev/null || echo "  缺失"
  echo "== shim ICD ==";    ls -la "$DEPLOY/lib/libvulkan_gf_shim.so" 2>/dev/null || echo "  缺失"
  echo "== gf_icd.json =="; cat "$DEPLOY/gf_icd.json" 2>/dev/null || echo "  缺失"
  echo "== 驱动库副本 ==";  ls -la "$DEPLOY/lib/libvulkan_gf.so" "$DEPLOY/lib/libSC.so" 2>/dev/null || echo "  缺失"
  echo "== libSC 链接 ==";  readlink "$PROTON_EX/files/lib/x86_64-linux-gnu/libSC.so" 2>/dev/null || echo "  缺失"
  echo "== DXVK ==";        head -1 "$PROTON_EX/files/lib/wine/dxvk/version" 2>/dev/null
  echo "== MMPatch ==";     ls -la "$GAME/MMPatch.dll" 2>/dev/null || echo "  缺失"
  echo "== doorstop ==";    grep target_assembly "$GAME/doorstop_config.ini" 2>/dev/null
  echo "== CompatTool ==";  grep -A3 '"489520"' "$CONFIG_VDF" 2>/dev/null | grep -m1 name
  echo "== LaunchOpts ==";  grep -m1 LaunchOptions "$LOCALCONFIG_VDF" 2>/dev/null
  echo "== winhttp ==";     grep -m1 '"winhttp"' "$PFX/user.reg" 2>/dev/null || echo "  未注入"
}

# ---------- 卸载：还原本脚本所做的一切 ----------
uninstall() {
  say "== 开始卸载 Minion Masters 7G100 硬件渲染加固 =="
  if pgrep -f "ubuntu12_32/steam" >/dev/null 2>&1; then
    warn "Steam 正在运行——vdf 改动可能在 Steam 退出时被覆盖。建议先退出 Steam 再卸载。"
  fi

  # 1a. config.vdf: 移除 CompatToolMapping 的 489520 条目
  python3 - "$CONFIG_VDF" <<'PY'
import re, sys
try:
    p = sys.argv[1]; data = open(p).read()
except (OSError, IndexError):
    sys.exit(0)
m = re.search(r'"CompatToolMapping"\s*\n\s*\{', data)
if not m:
    print("[mm-hw] config.vdf 无 CompatToolMapping"); sys.exit(0)
seg_start = m.end(); depth = 1; i = seg_start
while i < len(data) and depth > 0:
    if data[i] == '{': depth += 1
    elif data[i] == '}': depth -= 1
    i += 1
seg = data[seg_start:i-1]
seg2 = re.sub(r'\n\s*"489520"\s*\n\s*\{[^}]*\}', '', seg)
if seg2 != seg:
    open(p, 'w').write(data[:seg_start] + seg2 + data[i-1:])
    print("[mm-hw] CompatToolMapping 489520 条目已移除")
else:
    print("[mm-hw] CompatToolMapping 无 489520 条目")
PY

  # 1b. localconfig.vdf: 移除 489520 的 LaunchOptions
  if [ -n "${LOCALCONFIG_VDF:-}" ] && [ -f "$LOCALCONFIG_VDF" ]; then
    python3 - "$LOCALCONFIG_VDF" <<'PY'
import re, sys
p = sys.argv[1]; data = open(p).read()
i = data.find('"apps"'); j = data.find('"489520"', i)
if j < 0:
    print("[mm-hw] localconfig 无 489520 段"); sys.exit(0)
brace = data.find('{', j); depth = 1; k = brace + 1
while depth > 0:
    if data[k] == '{': depth += 1
    elif data[k] == '}': depth -= 1
    k += 1
seg = data[brace:k]
seg2 = re.sub(r'\n\s*"LaunchOptions"\s*"[^"]*"', '', seg)
if seg2 != seg:
    open(p, 'w').write(data[:brace] + seg2 + data[k:])
    print("[mm-hw] LaunchOptions 已移除")
else:
    print("[mm-hw] LaunchOptions 无本修复写入项")
PY
  else
    say "localconfig.vdf 不存在，跳过"
  fi

  # 2. user.reg: 移除 winhttp=n,b
  if [ -f "$PFX/user.reg" ]; then
    python3 - "$PFX/user.reg" <<'PY'
import sys
p = sys.argv[1]; data = open(p).read()
if '"winhttp"="n,b"' in data:
    open(p, 'w').write(data.replace('"winhttp"="n,b"\n', '', 1))
    print("[mm-hw] winhttp=n,b 已移除")
else:
    print("[mm-hw] user.reg 无 winhttp override")
PY
  else
    say "pfx/user.reg 不存在，跳过 winhttp 还原"
  fi

  # 3. DXVK 还原（从 .bak-v3 备份恢复原版 v3）
  local de="$PROTON_EX/files/lib/wine/dxvk"
  if [ -d "$de.bak-v3" ]; then
    chmod -R u+w "$de" 2>/dev/null
    rm -rf "$de"
    cp -r "$de.bak-v3" "$de"
    rm -rf "$de.bak-v3"
    rm -f "$PFX/drive_c/windows/system32/"{d3d9,d3d10core,d3d11,dxgi}.dll \
          "$PFX/drive_c/windows/syswow64/"{d3d9,d3d10core,d3d11,dxgi}.dll 2>/dev/null
    say "DXVK 已还原为原版（.bak-v3 备份已消费），前缀内 dll 将由 Proton 重同步"
  else
    say "无 dxvk.bak-v3 备份，跳过 DXVK 还原（可能从未降级或已还原）"
  fi

  # 4. MMPatch / doorstop 还原
  if [ -f "$GAME/MMPatch.dll" ] || [ -f "$GAME/doorstop_config.ini.bak-bepinex" ]; then
    rm -f "$GAME/MMPatch.dll"
    if [ -f "$GAME/doorstop_config.ini.bak-bepinex" ]; then
      cp "$GAME/doorstop_config.ini.bak-bepinex" "$GAME/doorstop_config.ini"
      rm -f "$GAME/doorstop_config.ini.bak-bepinex"
      say "doorstop_config.ini 已还原"
    fi
    say "MMPatch.dll 已移除"
  else
    say "游戏目录无 MMPatch/doorstop 改动，跳过"
  fi

  # 5. libSC 软链移除
  if [ -L "$PROTON_EX/files/lib/x86_64-linux-gnu/libSC.so" ]; then
    rm -f "$PROTON_EX/files/lib/x86_64-linux-gnu/libSC.so"
    say "libSC.so 软链已移除"
  else
    say "libSC.so 软链不存在，跳过"
  fi

  # 6. DEPLOY 运行时产物清理（仅产物，不动 gf-compat 内的源码/构建脚本）
  rm -rf "$DEPLOY/lib" "$DEPLOY/os-release" "$DEPLOY/gf_icd.json" 2>/dev/null
  say "DEPLOY 运行时产物已清理: $DEPLOY/{lib,os-release,gf_icd.json}"

  say "== 卸载完成。重启 Steam 使配置生效，游戏回到 Steam 默认 Proton 路线 =="
}

case "${1:-}" in
  --status) status; exit 0 ;;
  --uninstall) uninstall; exit 0 ;;
  --inject-winhttp) inject_winhttp; exit 0 ;;
esac

# Steam 运行中改 vdf 会被覆盖，提示
if pgrep -f "ubuntu12_32/steam" >/dev/null 2>&1; then
  say "警告: Steam 正在运行，vdf 改动可能被覆盖。建议先退出 Steam 再运行本脚本，或运行后重启 Steam。"
fi

deploy_hook
link_libsc
downgrade_dxvk
setup_mmpatch
if [ -n "${MM_PROTON_DIR:-}" ]; then
  # 目标 Proton 由调用方指定（非 Experimental）时，CompatToolMapping 由用户在
  # Steam 界面里手动选择，脚本不得改写，否则会把选择打回 proton_experimental。
  say "MM_PROTON_DIR 已指定 -> 跳过 CompatToolMapping 写入，保留你在 Steam 里选择的 Proton"
else
  config_steam
fi
inject_winhttp || true

say "完成。重启 Steam 后启动 Minion Masters 即走 7G100 硬件渲染。"
say "注意: (1) Proton Experimental / Steam 容器运行时更新会覆盖 libSC 链接与 DXVK 降级，"
say "          更新后请重跑本脚本（幂等）。"
say "      (2) 32 位低地址映射由 hook 的【低 4GB 重映射池】兜底；宿主机驱动若另打了"
say "          MAP_32BIT 单字节补丁（~/project/lisuan/gf-lowmem-shim/patch_gf_map32.py）属锦上添花。"
say "      (3) 游戏后端 api.prod.mm21.../v1/* 目前 404（厂商服务器问题，与显卡无关），"
say "          游戏会推进到登录队列后卡住/自行退出；7G100 渲染链路已全程验证"
say "          （Renderer: LISUAN 7G100 + 场景加载 + 登录流程）。"
