#!/usr/bin/env bash
#
# mm-render-switch.sh — 切换《随从大师》(AppID 489520) 的渲染后端
#
# 背景（2026-08-28 排查结论）：
#   游戏是 32 位 exe，砺算 gf 只有【64 位】Vulkan 驱动，因此 32 位游戏必须走
#   Proton 的 WoW64（PROTON_USE_WOW64=1）才能用上 7G100。但在 WoW64 下，
#   DXVK 调用 64 位 gf 驱动做显存映射时失败：
#       DxvkMemoryAllocator: Mapping memory failed with VK_ERROR_OUT_OF_HOST_MEMORY
#       d3d11: failed to create buffer (target 0x1 mode 0 size 20) [0x80070057]
#   驱动本身没问题（原生 64 位下 vkMapMemory / vkCreateBuffer 全部成功，已实测），
#   是 32 位 DXVK 经 wine WoW64 thunk 访问 64 位驱动映射的高位地址所致。
#
#   llvmpipe（32 位 Mesa 软件渲染）无此问题，已实测可进入登录界面/主菜单。
#
# 用法：
#   ./mm-render-switch.sh soft    # 切到 llvmpipe（当前可玩，软件渲染）
#   ./mm-render-switch.sh gf      # 切到 7G100 硬件（驱动/Wine 修复后再试）
#   ./mm-render-switch.sh status  # 查看当前启动选项
#
# 注意：Steam 运行时会把自己内存里的值写回 vdf 覆盖本脚本的改动，
#       请先【完全退出 Steam】再执行，然后重新启动 Steam。
set -u

APPID=489520
LOCALCONFIG=$(ls "$HOME"/.local/share/Steam/userdata/*/config/localconfig.vdf 2>/dev/null | head -1)

# llvmpipe：32 位 Mesa 软件渲染，已验证可进入游戏。
# LP_NUM_THREADS 按 CPU 核心数设置可显著提升软件渲染帧率。
CORES=$(nproc 2>/dev/null || echo 8)
SOFT_OPTS="VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json PROTON_USE_WOW64=1 LP_NUM_THREADS=$CORES __GLX_VENDOR_LIBRARY_NAME=mesa %command%"

# gf：7G100 硬件渲染。LD_PRELOAD 的 hook 把 /etc/os-release 重定向成 Ubuntu，
#     绕过 gf 驱动只认国产发行版的白名单，否则枚举不到 GPU。
GF_OPTS="PROTON_USE_WOW64=1 LD_PRELOAD=$HOME/gf-compat/lib/osrelease_redirect.so __GLX_VENDOR_LIBRARY_NAME=mesa LIBGL_ALWAYS_SOFTWARE=1 %command%"

die() { echo "错误: $*" >&2; exit 1; }

current() {
  [ -n "$LOCALCONFIG" ] || die "未找到 localconfig.vdf"
  grep -A6 "\"$APPID\"" "$LOCALCONFIG" 2>/dev/null | grep -m1 'LaunchOptions' \
    | sed 's/^[[:space:]]*//' | cut -c1-200
}

apply() { # apply <新启动选项>
  local new="$1"
  [ -n "$LOCALCONFIG" ] || die "未找到 localconfig.vdf"
  if pgrep -f "ubuntu12_32/steam" >/dev/null 2>&1; then
    echo "警告: Steam 正在运行，改动很可能被其写回覆盖。"
    echo "      请完全退出 Steam（托盘图标 -> 退出）后再执行本脚本。"
  fi
  cp -f "$LOCALCONFIG" "$LOCALCONFIG.bak-$(date +%m%d%H%M)"
  python3 - "$LOCALCONFIG" "$new" <<'PY'
import re, sys
p, new = sys.argv[1], sys.argv[2]
data = open(p, encoding="utf-8", errors="ignore").read()
i = data.find('"apps"')
j = data.find('"489520"', i)
if j < 0:
    sys.exit("localconfig 中无 489520 段（请先在 Steam 里运行过一次该游戏）")
brace = data.find('{', j); depth = 1; k = brace + 1
while depth > 0:
    if data[k] == '{': depth += 1
    elif data[k] == '}': depth -= 1
    k += 1
seg = data[brace:k]
seg2 = re.sub(r'\n\s*"LaunchOptions"\s*"[^"]*"', '', seg)
seg2 = seg2[:1] + '\n\t\t\t\t\t\t"LaunchOptions"\t\t"%s"' % new + seg2[1:]
open(p, 'w', encoding="utf-8").write(data[:brace] + seg2 + data[k:])
print("已写入 LaunchOptions:\n  " + new)
PY
}

case "${1:-status}" in
  soft)   apply "$SOFT_OPTS"; echo; echo "已切到 llvmpipe 软件渲染。重启 Steam 后生效。" ;;
  gf)     apply "$GF_OPTS";   echo; echo "已切到 7G100 硬件渲染。重启 Steam 后生效。" ;;
  status)
    echo "当前启动选项:"; cur=$(current); echo "  ${cur:-（未设置）}"
    echo
    case "${cur:-}" in
      *lvp_icd*)   echo "模式: llvmpipe 软件渲染（可进游戏，性能较低）" ;;
      *osrelease*) echo "模式: 7G100 硬件渲染（当前会在创建 buffer 时崩溃）" ;;
      *)           echo "模式: 未知/未设置" ;;
    esac
    echo
    echo "当前 DXVK: $(head -1 "$HOME/.local/share/Steam/steamapps/common/Proton - Experimental/files/lib/wine/dxvk/version" 2>/dev/null)"
    ;;
  *) echo "用法: $0 {soft|gf|status}" ; exit 1 ;;
esac
