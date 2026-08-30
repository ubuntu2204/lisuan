#!/usr/bin/env bash
#
# sample_lowmem.sh — 在游戏运行期间采样其地址空间，重点看低 2GB / 低 4GB 占用。
#
# 用途：判断给 gf 打上 MAP_32BIT 补丁后，mmap 失败是否源于低 2GB 已被占满。
#   MAP_32BIT 的语义是把映射放进进程【前 2GB】，若该区域被 wine 的堆/模块占满，
#   mmap 就会失败，DXVK 表现为 VK_ERROR_MEMORY_MAP_FAILED。
#
# 用法: ./sample_lowmem.sh [采样次数] [间隔秒]
#
set -u
N="${1:-14}"
T="${2:-3}"
OUT=/tmp/lowmem_samples.txt
: > "$OUT"

GAME="/home/ubuntu/.local/share/Steam/steamapps/common/Minion Masters"
PE="${PROTON_DIR:-$HOME/.local/share/Steam/steamapps/common/Proton - MM GF}"
SRT="$HOME/.local/share/Steam/steamapps/common/SteamLinuxRuntime_4"
PFX="$HOME/.local/share/Steam/steamapps/compatdata/489520"

pkill -f "proton run MinionMasters" 2>/dev/null
sleep 1

cd "$GAME" || exit 1
env LD_PRELOAD=/home/ubuntu/gf-compat/lib/osrelease_redirect.so \
    PROTON_USE_WOW64=1 \
    STEAM_COMPAT_DATA_PATH="$PFX" \
    STEAM_COMPAT_CLIENT_INSTALL_PATH="$HOME/.local/share/Steam" \
    "$SRT/_v2-entry-point" --verb=run -- "$PE/proton" run MinionMasters.exe \
    >/tmp/mm-sample.log 2>&1 &
GAMEPID=$!

for ((i=1; i<=N; i++)); do
    sleep "$T"
    # 找到真正的游戏进程（wine 会把进程名设为 exe 名）
    PIDS=$(pgrep -f "MinionMasters.exe" 2>/dev/null)
    if [ -z "$PIDS" ]; then
        echo "[$i] 无游戏进程" >> "$OUT"
        continue
    fi
    for p in $PIDS; do
        [ -r "/proc/$p/maps" ] || continue
        {
            echo "===== [$i] pid=$p ====="
            # 低 2GB 用量
            awk -F'[- ]' '
              { s=strtonum("0x"$2)-strtonum("0x"$1)
                e=strtonum("0x"$2)
                if (e <= 0x80000000) low2 += s
                else if (e <= 0x100000000) b24 += s
                tot += s }
              END { printf "  低2GB已用: %.1f MB\n  2-4GB已用: %.1f MB\n  总计映射: %.1f MB\n  VMA数量: %d\n",
                    low2/1048576, b24/1048576, tot/1048576, NR }
            ' "/proc/$p/maps"
            # 低 2GB 里最大的几块
            echo "  --- 低2GB 前5大映射 ---"
            awk -F'[- ]' '
              { s=strtonum("0x"$2)-strtonum("0x"$1); e=strtonum("0x"$2)
                if (e <= 0x80000000) printf "%12.1f MB  0x%s-0x%s  %s\n", s/1048576, $1, $2, $NF }
            ' "/proc/$p/maps" | sort -rn | head -5
        } >> "$OUT"
    done
done

wait $GAMEPID 2>/dev/null
echo "采样完成 -> $OUT"
