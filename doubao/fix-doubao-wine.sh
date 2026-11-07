#!/usr/bin/env bash
# 修复豆包 Wine 前缀内失效的 WinSxS 软链接，以及 cryptnet 必需的 LocalLow 目录。
#
# 只做与客户端版本无关的前缀层修复：
#   - 不做版本回退（旧版 1.56.6 仅作历史对照，不作方案）；
#   - 不锁版本、不 patch update.exe、不屏蔽版本号；
#   - 不删除账号、配置与聊天数据。
#
# 前缀可用 DOUBAO_WINE_PREFIX 覆盖，例如 Wine 11 前缀：
#   DOUBAO_WINE_PREFIX=~/.deepinwine/doubao-wine11 ./fix-doubao-wine.sh
set -euo pipefail

PREFIX="${DOUBAO_WINE_PREFIX:-$HOME/.deepinwine/doubao-wine}"
# 按当前前缀动态生成标记，避免误伤其他前缀下的链接。
MARKER="/${PREFIX#/}/dosdevices/c:/"

if [[ ! -d "$PREFIX/drive_c/windows" ]]; then
    echo "错误：未找到豆包 Wine 前缀：$PREFIX" >&2
    exit 1
fi

# 前缀内的 Windows 用户名不一定是当前系统用户名
# （例如 Wine 11 前缀是 steamuser），必须按前缀实际内容解析。
PROFILE=''
for candidate in "$PREFIX/drive_c/users/$USER" "$PREFIX"/drive_c/users/*; do
    [[ -d "$candidate/AppData" ]] || continue
    PROFILE="$candidate"
    break
done
if [[ -z "$PROFILE" ]]; then
    echo "错误：未在前缀中找到 Windows 用户目录：$PREFIX/drive_c/users" >&2
    exit 1
fi

# Wine 8.16 cryptnet 的缓存函数在 SHGetKnownFolderPath(LocalAppDataLow)
# 失败时返回 -1，但调用方只检查 NULL，随后会把 -1 当 FILE* 使用并崩溃。
if [[ ! -d "$PROFILE/AppData/LocalLow" ]]; then
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

# 只读探测当前客户端版本，仅用于报告。
# 本脚本不做版本回退、不锁版本、不禁用更新器（见 豆包客户端版本约定.md）。
detect_version() {
    local manifest
    for manifest in \
        "$PROFILE/AppData/Local/Doubao/Application/app/manifest.json" \
        "$PREFIX/drive_c/Program Files/Doubao/app/manifest.json"
    do
        [[ -f "$manifest" ]] || continue
        sed -n 's/^[[:space:]]*"version":[[:space:]]*"\([^"]*\)".*/\1/p' "$manifest" | head -n 1
        return 0
    done
    printf '未知\n'
}

current_version=$(detect_version)

echo "完成：修复 $fixed 个软链接；当前豆包版本 ${current_version:-未知}；账号数据未清除。"
