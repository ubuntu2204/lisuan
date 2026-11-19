/* 11 对战平台 Wine 系统字体探针/设置器（32 位，仅作用于当前 WINEPREFIX）。
 *
 * check：打印 NONCLIENTMETRICS 中五类系统字体的 lfFaceName。
 * set <name>：用 SPI_SETNONCLIENTMETRICS 把五类系统字体改为 <name>。
 * License: Apache-2.0
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static LOGFONT **system_fonts(NONCLIENTMETRICS *nm)
{
    static LOGFONT *fonts[5];
    fonts[0] = &nm->lfCaptionFont;
    fonts[1] = &nm->lfSmCaptionFont;
    fonts[2] = &nm->lfMenuFont;
    fonts[3] = &nm->lfMessageFont;
    fonts[4] = &nm->lfStatusFont;
    return fonts;
}

static int get_metrics(NONCLIENTMETRICS *nm)
{
    memset(nm, 0, sizeof(*nm));
    nm->cbSize = sizeof(*nm);
    if (!SystemParametersInfo(SPI_GETNONCLIENTMETRICS, nm->cbSize, nm, 0)) {
        fprintf(stderr, "SystemParametersInfo(GET) failed: %lu\n", GetLastError());
        return 0;
    }
    return 1;
}

int main(int argc, char **argv)
{
    NONCLIENTMETRICS nm;
    const char *mode = argc > 1 ? argv[1] : "check";

    if (!get_metrics(&nm))
        return 1;

    if (strcmp(mode, "check") == 0) {
        LOGFONT **fonts = system_fonts(&nm);
        static const char *names[] = {"caption", "smcaption", "menu",
                                      "message", "status"};
        for (int i = 0; i < 5; ++i)
            printf("%s=%s\n", names[i], fonts[i]->lfFaceName);
        return 0;
    }

    if (strcmp(mode, "set") == 0 && argc > 2) {
        LOGFONT **fonts = system_fonts(&nm);
        for (int i = 0; i < 5; ++i) {
            memset(fonts[i]->lfFaceName, 0, sizeof(fonts[i]->lfFaceName));
            strncpy(fonts[i]->lfFaceName, argv[2], LF_FACESIZE - 1);
        }
        if (!SystemParametersInfo(SPI_SETNONCLIENTMETRICS, nm.cbSize, &nm,
                                  SPIF_UPDATEINIFILE | SPIF_SENDCHANGE)) {
            fprintf(stderr, "SystemParametersInfo(SET) failed: %lu\n",
                    GetLastError());
            return 1;
        }
        printf("set system fonts to %s\n", argv[2]);
        return 0;
    }

    fprintf(stderr, "usage: %s check | set <font>\n", argv[0]);
    return 2;
}
