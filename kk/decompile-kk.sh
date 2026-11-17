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
# 除原始 meta/strings/disasm 外，每个模块还生成若干「索引」小文件，便于直接
# grep 源码级线索：symbols.txt（导入导出符号）、source-paths.txt（构建机源码
# 路径，来自 __FILE__）、env-refs.txt（读取/写入的环境变量）、qrc-paths.txt
# （Qt 资源路径）、chromium-flags.txt、pdb.txt（CodeView 符号路径）。
# 这些是对已有产物的再加工，不引入 Ghidra/IDA，也不生成伪代码。
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
与元数据提取，不生成伪代码，不修改软件本身。每个模块另生成 symbols.txt /
source-paths.txt / env-refs.txt / qrc-paths.txt / chromium-flags.txt / pdb.txt
六个可 grep 的索引小文件，便于定位源码级线索。

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

    extract_indices "$dir"
}

# 从 meta.txt / strings-ascii.txt 再加工出可直接 grep 的索引文件。
# 全部使用 grep/sed/sort（本机已有），无新依赖；即便某个索引为空也生成空文件，
# 避免 --status 里出现「文件缺失」的歧义。
extract_indices() {
    local dir="$1"
    local meta="$dir/meta.txt" sa="$dir/strings-ascii.txt"

    # 导入/导出符号：objdump -x 的表格里「DLL Name:」后接若干成员行，导出在
    # [Ordinal/Name Pointer] Table 之后。只保留名字，去掉 vma/hint 等噪音。
    {
        echo "# imports (DLL :: member)"
        awk '
            /DLL Name:/ { dll=$0; sub(/^.*DLL Name: /,"",dll); next }
            dll && /^[[:space:]]+[0-9a-f]+[[:space:]]+[0-9]+[[:space:]]/ {
                line=$0
                sub(/^[[:space:]]+[0-9a-f]+[[:space:]]+[0-9]+[[:space:]]+/,"",line)
                sub(/[[:space:]].*$/,"",line)
                if (line != "" && line != "<none>") print dll " :: " line
            }
            /^The Export Tables/ { dll=""; print "\n# exports (ordinal :: name)" }
            /^[[:space:]]*\[[[:space:]]*[0-9]+\][[:space:]]+[A-Za-z_?]/ {
                line=$0; sub(/^[[:space:]]*\[[[:space:]]*[0-9]+\][[:space:]]+/,"",line)
                print line
            }
        ' "$meta" 2>/dev/null
    } > "$dir/symbols.txt" || true

    # 构建机源码路径：MSVC 的 __FILE__ 常量，泄露模块/源码目录结构。
    grep -aoE '[A-Z]:\\.*\.(cpp|hpp|h|c|cc)' "$sa" 2>/dev/null \
        | sort -u > "$dir/source-paths.txt" || true

    # 环境变量引用：QT_/QTWEBENGINE_*/WINE_/VK_/LIBGL_/GBM_/DXVK_/PROTON_/KK_ 前缀。
    # 注意 QTWEBENGINE 后紧跟的是大写名而非下划线（如 QTWEBENGINE_CHROMIUM_FLAGS），
    # 故前缀后允许若干大写字母再接下划线。
    grep -aoE '\b(QT|WINE|VK|LIBGL|GBM|DXVK|PROTON|KK)[A-Z_]*_[A-Z0-9_]+\b' "$sa" 2>/dev/null \
        | sort -u > "$dir/env-refs.txt" || true

    # Qt 资源路径（qrc）：:/... 形式，可据此用 WebEngine/CDP 直接读取内容。
    grep -aoE ':/[A-Za-z0-9_./-]+' "$sa" 2>/dev/null \
        | sort -u > "$dir/qrc-paths.txt" || true

    # Chromium/命令行开关：--xxx，含应用自定义的 --disable-nep 等。
    grep -aoE -- '--[a-z][a-z0-9-]{2,}' "$sa" 2>/dev/null \
        | sort -u > "$dir/chromium-flags.txt" || true

    # CodeView/PDB：RSDS GUID + 构建机 pdb 路径（本机通常无该 pdb）。
    grep -aiE 'RSDS signature|CodeView|pdb' "$meta" 2>/dev/null \
        | head -20 > "$dir/pdb.txt" || true
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
