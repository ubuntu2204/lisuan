/******************************************************************
 * powershell-stub.c —— 给 Wine 前缀里的 powershell.exe 换一个「会正确返回
 * 退出码」的替身。
 *
 * 为什么需要它（2026-11-08 实测）
 * --------------------------------
 * Wine 自带的 powershell 是**空壳 stub**：wmain 什么都不做，直接 exit 0。
 * 而 electron-builder 生成的 NSIS 安装器在安装前会用 powershell 检测
 * 「旧版本进程是否还在跑」：
 *
 *   1) if (Get-Command Get-CimInstance -ErrorAction SilentlyContinue)
 *        { exit 0 } else { exit 1 }          <- 探测有没有 CIM
 *   2) Get-CimInstance ... | % { Stop-Process ... }
 *   3) if ((Get-CimInstance ...).Count -gt 0) { exit 0 } else { exit 1 }
 *        <- 判断是否还有进程在跑（exit 0 = 有进程）
 *   4) Get-CimInstance ... | % { Stop-Process -Force }
 *
 * stub 一律返回 0，于是第 3 步被解读成「有进程在运行」，安装器转去
 * 弹「应用正在运行，是否强制关闭」并 Abort —— 静默模式下直接退出，
 * 表现为 `/S` 返回码 2、$INSTDIR 空、Temp 里什么都没有。
 *
 * 这里的替身只做一件事：**凡命令行里出现 Get-CimInstance，就返回 1**
 * （= 没有进程在跑），其余调用仍返回 0（与 wine 原 stub 行为一致，
 * 不影响安装器后续用 powershell 做别的事）。
 *
 * 用法（32 位 PE，装进前缀的 system32 / syswow64）：
 *   i686-w64-mingw32-gcc -O2 -o powershell.exe powershell-stub.c
 ******************************************************************/
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    int i;

    for (i = 1; i < argc; i++) {
        if (argv[i] && strstr(argv[i], "Get-CimInstance")) {
            /* 没有 Get-CimInstance 的进程可列 => 没有进程在跑 */
            return 1;
        }
    }
    return 0;
}
