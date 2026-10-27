#!/usr/bin/env bash
# 修复豆包 Wine 前缀内失效的 WinSxS 软链接，以及 cryptnet 必需的 LocalLow 目录。
set -euo pipefail

PREFIX="${DOUBAO_WINE_PREFIX:-$HOME/.deepinwine/doubao-wine}"
MARKER='/.deepinwine/doubao-wine/dosdevices/c:/'

if [[ ! -d "$PREFIX/drive_c/windows" ]]; then
    echo "错误：未找到豆包 Wine 前缀：$PREFIX" >&2
    exit 1
fi

# Wine 8.16 cryptnet 的缓存函数在 SHGetKnownFolderPath(LocalAppDataLow)
# 失败时返回 -1，但调用方只检查 NULL，随后会把 -1 当 FILE* 使用并崩溃。
PROFILE="$PREFIX/drive_c/users/$USER"
if [[ -d "$PROFILE/AppData" && ! -d "$PROFILE/AppData/LocalLow" ]]; then
    mkdir -p "$PROFILE/AppData/LocalLow"
    echo "已补齐：${PROFILE#"$PREFIX/"}/AppData/LocalLow"
fi

fixed=0
missing=0
while IFS= read -r -d '' link; do
    target=$(readlink "$link")
    [[ "$target" == /home/*"$MARKER"* ]] || continue

    suffix=${target#*dosdevices/c:/}
    destination="$PREFIX/dosdevices/c:/$suffix"
    if [[ ! -e "$destination" ]]; then
        echo "跳过（目标不存在）：$link -> $destination" >&2
        missing=$((missing + 1))
        continue
    fi

    relative=$(realpath -ms --relative-to="$(dirname "$link")" "$destination")
    ln -sfn "$relative" "$link"
    echo "已修复：${link#"$PREFIX/"} -> $relative"
    fixed=$((fixed + 1))
done < <(find "$PREFIX" -type l -print0)

if (( missing > 0 )); then
    echo "错误：仍有 $missing 个链接的目标不存在，未修改这些链接。" >&2
    exit 1
fi

remaining=0
while IFS= read -r -d '' link; do
    target=$(readlink "$link")
    [[ "$target" == /home/*"$MARKER"* ]] && remaining=$((remaining + 1))
done < <(find "$PREFIX" -type l -print0)

if (( remaining > 0 )); then
    echo "错误：仍有 $remaining 个指向其他用户目录的链接。" >&2
    exit 1
fi

APP_DIR="$PREFIX/drive_c/Program Files/Doubao/app"
USER_DATA="$PROFILE/AppData/Local/Doubao/User Data"
MANIFEST="$APP_DIR/manifest.json"
current_version=''
if [[ -f "$MANIFEST" ]]; then
    current_version=$(sed -n 's/^[[:space:]]*"version":[[:space:]]*"\([^"]*\)".*/\1/p' "$MANIFEST" | head -n 1)
fi

# 2.32.6 (Chromium 147) 在本机 Wine 8.16 和 Wine 11 下均无法进入主窗口。
# 只回退这个已验证的问题版本，不影响以后其他版本。
if [[ "$current_version" == '2.32.6' ]]; then
    ARCHIVE='/opt/apps/doubao-wine/files/files.7z'
    UNPACK='/opt/p7zip-legacy/bin/7z'
    if [[ ! -f "$ARCHIVE" || ! -x "$UNPACK" ]]; then
        echo "错误：回退 2.32.6 需要原安装包和 p7zip-legacy。" >&2
        exit 1
    fi

    echo "检测到不兼容的豆包 2.32.6，从原安装包恢复 1.56.6……"
    if [[ -x /opt/deepin-wine8-stable/bin/wineserver ]]; then
        WINEPREFIX="$PREFIX" /opt/deepin-wine8-stable/bin/wineserver -k >/dev/null 2>&1 || true
    fi

    restore_tmp=$(mktemp -d "${TMPDIR:-/tmp}/doubao-restore.XXXXXX")
    "$UNPACK" x "$ARCHIVE" -o"$restore_tmp" 'drive_c/Program Files/Doubao/app/*' -y >/dev/null
    mv "$APP_DIR" "$restore_tmp/app-previous"
    cp -a "$restore_tmp/drive_c/Program Files/Doubao/app" "$APP_DIR"

    # 清除高版本更新包和渲染缓存；账号、配置及聊天数据保留。
    rm -rf "$USER_DATA/update_data" \
        "$USER_DATA/ShaderCache" "$USER_DATA/GrShaderCache" \
        "$USER_DATA/GraphiteDawnCache" "$USER_DATA/Default/GPUCache" \
        "$USER_DATA/Default/DawnWebGPUCache" "$USER_DATA/Default/DawnGraphiteCache" \
        "$USER_DATA/Default/Cache" "$USER_DATA/Default/Code Cache"
    printf '%s\n' '129.0.6668.79' > "$USER_DATA/Last Version"

    if [[ -f "$USER_DATA/repair/record.json" ]]; then
        python3 - "$USER_DATA/repair/record.json" <<'PY'
import json
import sys
from pathlib import Path
path = Path(sys.argv[1])
data = json.loads(path.read_text())
data["running_exception"] = {}
path.write_text(json.dumps(data, separators=(",", ":")) + "\n")
PY
    fi
    rm -rf "$restore_tmp"
    current_version='1.56.6'
fi

# 版本屏蔽配置实测不能阻止安装。保留 update.exe 的文件名和有效 PE 结构，
# 仅把入口改成 xor eax,eax; ret，令更新器立即退出，避免修复器误报文件缺失。
if [[ -f "$APP_DIR/update.exe" ]]; then
    python3 - "$APP_DIR/update.exe" <<'PY'
import struct
import sys
from pathlib import Path

path = Path(sys.argv[1])
data = bytearray(path.read_bytes())
if len(data) < 0x40 or data[:2] != b"MZ":
    raise SystemExit("update.exe 不是有效的 PE 文件")
pe = struct.unpack_from("<I", data, 0x3C)[0]
if data[pe:pe + 4] != b"PE\0\0":
    raise SystemExit("update.exe 缺少 PE 头")
section_count = struct.unpack_from("<H", data, pe + 6)[0]
optional_size = struct.unpack_from("<H", data, pe + 20)[0]
optional = pe + 24
entry_rva = struct.unpack_from("<I", data, optional + 16)[0]
section_table = optional + optional_size
entry_offset = None
for index in range(section_count):
    section = section_table + index * 40
    virtual_size, virtual_address, raw_size, raw_address = struct.unpack_from("<IIII", data, section + 8)
    if virtual_address <= entry_rva < virtual_address + max(virtual_size, raw_size):
        entry_offset = raw_address + entry_rva - virtual_address
        break
if entry_offset is None:
    raise SystemExit("无法定位 update.exe 入口点")
if data[entry_offset:entry_offset + 3] != b"\x31\xc0\xc3":
    backup = path.with_name("update.exe.original")
    if not backup.exists():
        backup.write_bytes(data)
    data[entry_offset:entry_offset + 3] = b"\x31\xc0\xc3"
    path.write_bytes(data)
    print("已禁用 update.exe 的更新执行入口（文件保留）。")
PY
fi

# 同时使用客户端自带配置屏蔽问题版本，作为第二层防护。
if [[ -f "$USER_DATA/saman_update_info" ]]; then
    python3 - "$USER_DATA/saman_update_info" <<'PY'
import json
import sys
from pathlib import Path
path = Path(sys.argv[1])
data = json.loads(path.read_text())
blocked = set(data.get("vulcan_update_block_versions", []))
blocked.add("2.32.6")
data["vulcan_update_block_versions"] = sorted(blocked)
path.write_text(json.dumps(data, ensure_ascii=False, indent=3) + "\n")
PY
fi

echo "完成：修复 $fixed 个软链接；当前豆包版本 ${current_version:-未知}；账号数据未清除。"
