#!/usr/bin/env bash
# 用官方安装器在 Wine 11（Proton 11.0）前缀内安装/升级「即梦 AI」Windows 桌面客户端。
#
# 安装器：Dreamina-Desktop-<版本>-x64-setup.exe（NSIS v3.04，electron-builder 产物，
# asInvoker，不需要管理员）。安装到 C:\users\<用户>\AppData\Local\Programs\dreamina-desktop。
#
# ★ 关键坑（2026-11-08 实测）：**Wine 自带的 powershell 是空壳 stub，wmain 直接 exit 0。**
#   electron-builder 的 NSIS 安装器在安装前会用 powershell 探测「旧版本进程是否还在跑」：
#     1) if (Get-Command Get-CimInstance ...) { exit 0 } else { exit 1 }
#     2) Get-CimInstance ... | % { Stop-Process ... }
#     3) if ((Get-CimInstance ...).Count -gt 0) { exit 0 } else { exit 1 }
#     4) Get-CimInstance ... | % { Stop-Process -Force }
#   stub 一律返回 0，第 3 步被解读成「有进程在跑」，安装器转去弹「应用正在运行」并 Abort：
#   静默模式 /S 直接以返回码 2 退出，$INSTDIR 空、Temp 里什么都没有，什么都没装上。
#   解决：把前缀里的 powershell.exe 换成一个「凡命令行出现 Get-CimInstance 就返回 1」的
#   小替身（见 powershell-stub.c），安装器就会认为没有旧进程，正常解包安装。
#
# 用法：
#   ./dreamina/install-dreamina.sh [安装器.exe 路径]
#   DREAMINA_WINE11_PREFIX=~/.deepinwine/dreamina-wine11 ./dreamina/install-dreamina.sh
set -uo pipefail

PROTON="${DREAMINA_PROTON:-$HOME/.local/share/Steam/steamapps/common/Proton 11.0}"
PREFIX="${DREAMINA_WINE11_PREFIX:-$HOME/.deepinwine/dreamina-wine11}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INSTALLER="${1:-$SCRIPT_DIR/Dreamina-Desktop-1.0.59-x64-setup.exe}"
TIMEOUT="${DREAMINA_INSTALL_TIMEOUT:-900}"

WINE="$PROTON/files/bin/wine"
WINESERVER="$PROTON/files/bin/wineserver"

# 与 Proton 自带 proton 脚本保持一致，否则 wine 找不到自带 DLL/宿主库。
export WINEPREFIX="$PREFIX"
export WINEDLLPATH="$PROTON/files/lib/vkd3d:$PROTON/files/lib/wine"
export LD_LIBRARY_PATH="$PROTON/files/lib/x86_64-linux-gnu:$PROTON/files/lib/i386-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export WINEDLLOVERRIDES='mscoree,mshtml='

die() { echo "错误：$*" >&2; exit 1; }

[[ -x "$WINE" ]] || die "找不到 Proton 的 wine：$WINE"
[[ -f "$INSTALLER" ]] || die "找不到安装器：$INSTALLER"

# ---------------------------------------------------------------- 1. 前缀
if [[ ! -d "$PREFIX/drive_c" ]]; then
    echo "[1/4] 初始化 Wine 前缀：$PREFIX"
    mkdir -p "$PREFIX"
    WINEDEBUG=-all timeout 300 "$WINE" wineboot --init >/dev/null 2>&1
else
    echo "[1/4] 复用已有前缀：$PREFIX"
fi
[[ -d "$PREFIX/drive_c" ]] || die "前缀初始化失败：$PREFIX"

# ---------------------------------------------- 2. 换掉 Wine 的 powershell stub
# electron-builder 的安装器靠 powershell 探测旧进程，Wine 的 stub 一律返回 0，
# 会让安装器误判「应用正在运行」而 Abort（/S 返回码 2，什么都没装）。
PS_STUB_SRC="$SCRIPT_DIR/powershell-stub.c"
PS_TARGETS=(
    "$PREFIX/drive_c/windows/system32/WindowsPowerShell/v1.0/powershell.exe"
    "$PREFIX/drive_c/windows/syswow64/WindowsPowerShell/v1.0/powershell.exe"
)
need_stub=0
for t in "${PS_TARGETS[@]}"; do
    [[ -f "$t" ]] && grep -qa "Get-CimInstance" "$t" && need_stub=1
done
if [[ $need_stub -eq 1 ]]; then
    echo "[2/4] 替换 powershell stub（让安装器的进程探测正确返回「无进程」）"
    command -v i686-w64-mingw32-gcc >/dev/null || \
        die "缺少 i686-w64-mingw32-gcc，无法编译 powershell 替身（sudo apt install gcc-mingw-w64-i686）"
    tmp_exe=$(mktemp /tmp/powershell-stub-XXXXXX.exe)
    i686-w64-mingw32-gcc -O2 -m32 -o "$tmp_exe" "$PS_STUB_SRC" || die "编译 powershell 替身失败"
    for t in "${PS_TARGETS[@]}"; do
        mkdir -p "$(dirname "$t")"
        # 原文件留一份备份，便于回退
        [[ -f "$t" ]] && [[ ! -f "$t.wine-stub" ]] && cp "$t" "$t.wine-stub"
        cp "$tmp_exe" "$t"
    done
    rm -f "$tmp_exe"
else
    echo "[2/4] powershell 替身已就位，跳过"
fi

# ------------------------------------------------------------ 3. 跑安装器
echo "[3/4] 运行安装器（NSIS 静默模式）：$INSTALLER"
WINEDEBUG=-all timeout "$TIMEOUT" "$WINE" "$INSTALLER" /S >/tmp/dreamina-install.log 2>&1
rc=$?
echo "    安装器返回码：$rc"

# -------------------------------------------------------- 4. 校验安装结果
# 注意：User Data 也在 AppData\Roaming\dreamina-desktop，名字与安装目录相同，
# 所以必须优先锁定 Local\Programs 下的那个，否则会误认成安装目录。
INSTDIR=$(find "$PREFIX/drive_c/users" -maxdepth 8 -type d -path '*/AppData/Local/Programs/dreamina-desktop' -print -quit 2>/dev/null)
if [[ -z "$INSTDIR" ]]; then
    INSTDIR=$(find "$PREFIX/drive_c/users" -maxdepth 8 -type d -name 'dreamina-desktop' -print -quit 2>/dev/null)
fi
if [[ -z "$INSTDIR" ]]; then
    echo "错误：安装目录不存在，安装没成功（返回码 $rc）。" >&2
    echo "排查：/tmp/dreamina-install.log" >&2
    exit 1
fi
MAIN_EXE=$(find "$INSTDIR" -maxdepth 1 -name '*.exe' ! -name 'Uninstall*' -print -quit 2>/dev/null)
echo "[4/4] 安装完成"
echo "    安装目录：$INSTDIR"
echo "    主程序  ：$MAIN_EXE"
[[ -f "$INSTDIR/resources/app.asar" ]] && echo "    app.asar：$(stat -c%s "$INSTDIR/resources/app.asar") 字节"
echo
echo "启动：$SCRIPT_DIR/run-dreamina.sh"
