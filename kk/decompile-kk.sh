#!/usr/bin/env bash
# 至砺算：KK 官方对战平台 PE 静态逆向分析
#
# 把安装目录里 KK 自有的 PE 用 binutils 做 PE 头 / 节表 / 导入导出表 /
# 字符串 / 反汇编提取，产物落到 kk/reverse/，按仓库组织约定整体不入库
# （见 .gitignore 与 kk/README.md §1.1）。
#
# 说明：本机没有 Ghidra / IDA / radare2，这里只做到「反汇编 + 元数据」
# 级别，不生成高级语言伪代码。目的是理解 KK 的窗口 / CEF 行为，辅助排查
# Wine 下的兼容性问题；不修改软件本身，不联网，不做崩溃后自动恢复。
#
# 用法：./kk/decompile-kk.sh [--status | --clean | --force]
# 环境变量：KK_WINE11_PREFIX / KK_OBJDUMP / KK_MAX_DISASM
# License: Apache-2.0
set -euo pipefail

REPO_DIR="$(cd "$(dirname "$0")" && pwd)"
OUT="$REPO_DIR/reverse"
PREFIX="${KK_WINE11_PREFIX:-$HOME/.deepinwine/kkduizhan-wine11}"
INSTALL_DIR="$PREFIX/drive_c/Program Files (x86)/kkduizhan"
OBJDUMP="${KK_OBJDUMP:-i686-w64-mingw32-objdump}"
OBJDUMP64="${KK_OBJDUMP64:-objdump}"
MAX_DISASM="${KK_MAX_DISASM:-8388608}"
FORCE=0

# 只取 KK 自有模块；Qt5* / libcef / ffmpeg / msvc* / api-ms-win* 等第三方
# 运行时不属于逆向目标，避免产物膨胀。
TARGETS=(
    Platform.exe
    NEP2.dll
    PlatformUtils.dll
    PlatformDownloader.exe
    PlatformWebHelper.exe
    CrashHunter_PC3.dll
    patch_maker.dll
    KernelDumpAnalyzer.exe
    qlumpy.exe
    plugins/WebBrowser/PlatformWebBrowser.exe
    plugins/Game/kkapi.dll
    plugins/Game/War3Plugin.dll
)

usage() {
    cat <<'EOF'
用法：./kk/decompile-kk.sh [选项]

对 KK 官方对战平台安装目录里 KK 自有的 PE 做静态逆向分析，产物写入
kk/reverse/（不入库，见 .gitignore）。本机无 Ghidra/IDA，只做反汇编
与元数据提取，不生成伪代码，不修改软件本身。

选项：
  --status   只读列出已有产物及大小
  --clean    删除产物目录 kk/reverse/（不影响安装目录）
  --force    覆盖已存在的产物（默认跳过已生成的文件）
  -h, --help 显示本帮助

环境变量：
  KK_WINE11_PREFIX  wine 前缀，默认 ~/.deepinwine/kkduizhan-wine11
  KK_OBJDUMP        32 位 PE 反汇编工具，默认 i686-w64-mingw32-objdump
  KK_OBJDUMP64      64 位 PE（PE32+）反汇编工具，默认 objdump
  KK_MAX_DISASM     单文件全量反汇编的体积上限（字节），默认 8388608；
                    超过则只生成 PE 元数据与字符串，避免巨型文本
EOF
}

status() {
    if [[ ! -d "$OUT" ]]; then
        echo "尚无产物目录：$OUT"
        return 0
    fi
    echo "反编译产物目录：$OUT"
    find "$OUT" -type f -printf '%10s  %P\n' | sort -k2
    echo "合计：$(du -sh "$OUT" | cut -f1)"
}

clean() {
    if [[ -d "$OUT" ]]; then
        rm -rf "$OUT"
        echo "已删除 $OUT"
    else
        echo "无产物目录可删：$OUT"
    fi
}

dump_one() {
    local rel="$1"
    local src="$INSTALL_DIR/$rel"
    if [[ ! -f "$src" ]]; then
        echo "  跳过（文件不存在）：$rel"
        return 0
    fi
    local safe="${rel//\//__}"
    local base="${safe%.*}"
    local dir="$OUT/$base"
    mkdir -p "$dir"

    local size od="$OBJDUMP"
    size="$(stat -c %s "$src")"
    # KK 模块绝大多数是 32 位 PE32；KernelDumpAnalyzer.exe 等少量工具是
    # PE32+，i686 版 objdump 无法解析，改用 host 版。
    if command -v file > /dev/null 2>&1 && file -b "$src" | grep -q 'PE32+'; then
        od="$OBJDUMP64"
    fi
    echo "  [$rel] $((size / 1024 / 1024)) MiB [$(basename "$od")]"

    if [[ $FORCE -eq 1 || ! -s "$dir/meta.txt" ]]; then
        "$od" -f -h -p -x "$src" > "$dir/meta.txt" 2> "$dir/meta.err" || true
    fi
    if [[ $FORCE -eq 1 || ! -s "$dir/strings-ascii.txt" ]]; then
        strings -a -n 5 "$src" > "$dir/strings-ascii.txt" 2> /dev/null || true
    fi
    if [[ $FORCE -eq 1 || ! -s "$dir/strings-utf16.txt" ]]; then
        strings -a -e l -n 4 "$src" > "$dir/strings-utf16.txt" 2> /dev/null || true
    fi
    if (( size <= MAX_DISASM )); then
        if [[ $FORCE -eq 1 || ! -s "$dir/disasm.txt" ]]; then
            "$od" -d -M intel "$src" > "$dir/disasm.txt" 2> /dev/null || true
        fi
    else
        echo "PE 超过 ${MAX_DISASM} 字节，仅生成元数据与字符串（避免巨型反汇编文本）" \
            > "$dir/DISASM-SKIPPED.txt"
    fi
}

main() {
    case "${1:-}" in
        -h|--help) usage; return 0 ;;
        --status)  status; return 0 ;;
        --clean)   clean;  return 0 ;;
        --force)   FORCE=1 ;;
        "")        ;;
        *) echo "未知参数：$1" >&2; usage >&2; return 2 ;;
    esac

    command -v "$OBJDUMP" > /dev/null 2>&1 || {
        echo "错误：找不到反汇编工具：$OBJDUMP（用 KK_OBJDUMP 指定）" >&2
        return 1
    }
    command -v "$OBJDUMP64" > /dev/null 2>&1 || {
        echo "错误：找不到 64 位反汇编工具：$OBJDUMP64（用 KK_OBJDUMP64 指定）" >&2
        return 1
    }
    command -v strings > /dev/null 2>&1 || {
        echo "错误：找不到 strings（binutils）" >&2
        return 1
    }
    [[ -d "$INSTALL_DIR" ]] || {
        echo "错误：未找到 KK 安装目录：$INSTALL_DIR" >&2
        echo "先运行 kk/install-kk-platform.sh，或用 KK_WINE11_PREFIX 指定前缀" >&2
        return 1
    }

    echo "KK 安装目录：$INSTALL_DIR"
    echo "产物目录：$OUT"
    echo "目标模块：${#TARGETS[@]} 个"
    mkdir -p "$OUT"
    for rel in "${TARGETS[@]}"; do
        dump_one "$rel"
    done
    echo "完成。产物清单：./kk/decompile-kk.sh --status"
}

main "$@"
