/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include "disp_edid.h"
#include "disp_mode.h"
#include "disp_callback.h"
#include "disp_displayid.h"

#define version_greater(edid, maj, min) \
    (((edid)->version > (maj)) || \
     ((edid)->version == (maj) && (edid)->revision > (min)))

#define EDID_EST_TIMINGS 16
#define EDID_STD_TIMINGS 8
#define EDID_DETAILED_TIMINGS 4

/*
 * EDID blocks out in the wild have a variety of bugs, try to collect
 * them here (note that userspace may work around broken monitors first,
 * but fixes should make their way here so that the kernel "just works"
 * on as many displays as possible).
 */

/* First detailed mode wrong, use largest 60Hz mode */
#define EDID_QUIRK_PREFER_LARGE_60              (1 << 0)
/* Reported 135MHz pixel clock is too high, needs adjustment */
#define EDID_QUIRK_135_CLOCK_TOO_HIGH           (1 << 1)
/* Prefer the largest mode at 75 Hz */
#define EDID_QUIRK_PREFER_LARGE_75              (1 << 2)
/* Detail timing is in cm not mm */
#define EDID_QUIRK_DETAILED_IN_CM               (1 << 3)
/* Detailed timing descriptors have bogus size values, so just take the
 * maximum size and use that.
 */
#define EDID_QUIRK_DETAILED_USE_MAXIMUM_SIZE    (1 << 4)
/* use +hsync +vsync for detailed mode */
#define EDID_QUIRK_DETAILED_SYNC_PP             (1 << 6)
/* Force reduced-blanking timings for detailed modes */
#define EDID_QUIRK_FORCE_REDUCED_BLANKING       (1 << 7)
/* Force 8bpc */
#define EDID_QUIRK_FORCE_8BPC                   (1 << 8)
/* Force 12bpc */
#define EDID_QUIRK_FORCE_12BPC                  (1 << 9)
/* Force 6bpc */
#define EDID_QUIRK_FORCE_6BPC                   (1 << 10)
/* Force 10bpc */
#define EDID_QUIRK_FORCE_10BPC                  (1 << 11)
/* Non desktop display (i.e. HMD) */
#define EDID_QUIRK_NON_DESKTOP                  (1 << 12)

#define disp_print(level, log, ...) disp_print_ex(DISP_PRINT_MASK_EDID, level, log, ##__VA_ARGS__)
#define disp_print_pure(level, log, ...) disp_print_pure_ex(DISP_PRINT_MASK_EDID, level, log, ##__VA_ARGS__)

typedef struct _detailed_mode_closure_t
{
    pdisp_connector_t pdisp_connector;
    pedid_t edid;
    disp_u8_t preferred;
    disp_u32_t quirks;
    disp_s32_t modes;
}detailed_mode_closure_t, *pdetailed_mode_closure_t;

#define LEVEL_DMT    0
#define LEVEL_GTF    1
#define LEVEL_GTF2    2
#define LEVEL_CVT    3

static struct edid_quirk 
{
    disp_u8_t vendor[4];
    disp_s32_t product_id;
    disp_u32_t quirks;
} edid_quirk_list[] = 
{
    /* Acer AL1706 */
    { "ACR", 44358, EDID_QUIRK_PREFER_LARGE_60 },
    /* Acer F51 */
    { "API", 0x7602, EDID_QUIRK_PREFER_LARGE_60 },

    /* AEO model 0 reports 8 bpc, but is a 6 bpc panel */
    { "AEO", 0, EDID_QUIRK_FORCE_6BPC },

    /* BOE model on HP Pavilion 15-n233sl reports 8 bpc, but is a 6 bpc panel */
    { "BOE", 0x78b, EDID_QUIRK_FORCE_6BPC },

    /* CPT panel of Asus UX303LA reports 8 bpc, but is a 6 bpc panel */
    { "CPT", 0x17df, EDID_QUIRK_FORCE_6BPC },

    /* SDC panel of Lenovo B50-80 reports 8 bpc, but is a 6 bpc panel */
    { "SDC", 0x3652, EDID_QUIRK_FORCE_6BPC },

    /* BOE model 0x0771 reports 8 bpc, but is a 6 bpc panel */
    { "BOE", 0x0771, EDID_QUIRK_FORCE_6BPC },

    /* Belinea 10 15 55 */
    { "MAX", 1516, EDID_QUIRK_PREFER_LARGE_60 },
    { "MAX", 0x77e, EDID_QUIRK_PREFER_LARGE_60 },

    /* Envision Peripherals, Inc. EN-7100e */
    { "EPI", 59264, EDID_QUIRK_135_CLOCK_TOO_HIGH },
    /* Envision EN2028 */
    { "EPI", 8232, EDID_QUIRK_PREFER_LARGE_60 },

    /* Funai Electronics PM36B */
    { "FCM", 13600, EDID_QUIRK_PREFER_LARGE_75 |
      EDID_QUIRK_DETAILED_IN_CM },

    /* LGD panel of HP zBook 17 G2, eDP 10 bpc, but reports unknown bpc */
    { "LGD", 764, EDID_QUIRK_FORCE_10BPC },

    /* LG Philips LCD LP154W01-A5 */
    { "LPL", 0, EDID_QUIRK_DETAILED_USE_MAXIMUM_SIZE },
    { "LPL", 0x2a00, EDID_QUIRK_DETAILED_USE_MAXIMUM_SIZE },

    /* Samsung SyncMaster 205BW.  Note: irony */
    { "SAM", 541, EDID_QUIRK_DETAILED_SYNC_PP },
    /* Samsung SyncMaster 22[5-6]BW */
    { "SAM", 596, EDID_QUIRK_PREFER_LARGE_60 },
    { "SAM", 638, EDID_QUIRK_PREFER_LARGE_60 },

    /* Sony PVM-2541A does up to 12 bpc, but only reports max 8 bpc */
    { "SNY", 0x2541, EDID_QUIRK_FORCE_12BPC },

    /* ViewSonic VA2026w */
    { "VSC", 5020, EDID_QUIRK_FORCE_REDUCED_BLANKING },

    /* Medion MD 30217 PG */
    { "MED", 0x7b8, EDID_QUIRK_PREFER_LARGE_75 },

    /* Lenovo G50 */
    { "SDC", 18514, EDID_QUIRK_FORCE_6BPC },

    /* Panel in Samsung NP700G7A-S01PL notebook reports 6bpc */
    { "SEC", 0xd033, EDID_QUIRK_FORCE_8BPC },

    /* Rotel RSX-1058 forwards sink's EDID but only does HDMI 1.1*/
    { "ETR", 13896, EDID_QUIRK_FORCE_8BPC },

    /* Valve Index Headset */
    { "VLV", 0x91a8, EDID_QUIRK_NON_DESKTOP },
    { "VLV", 0x91b0, EDID_QUIRK_NON_DESKTOP },
    { "VLV", 0x91b1, EDID_QUIRK_NON_DESKTOP },
    { "VLV", 0x91b2, EDID_QUIRK_NON_DESKTOP },
    { "VLV", 0x91b3, EDID_QUIRK_NON_DESKTOP },
    { "VLV", 0x91b4, EDID_QUIRK_NON_DESKTOP },
    { "VLV", 0x91b5, EDID_QUIRK_NON_DESKTOP },
    { "VLV", 0x91b6, EDID_QUIRK_NON_DESKTOP },
    { "VLV", 0x91b7, EDID_QUIRK_NON_DESKTOP },
    { "VLV", 0x91b8, EDID_QUIRK_NON_DESKTOP },
    { "VLV", 0x91b9, EDID_QUIRK_NON_DESKTOP },
    { "VLV", 0x91ba, EDID_QUIRK_NON_DESKTOP },
    { "VLV", 0x91bb, EDID_QUIRK_NON_DESKTOP },
    { "VLV", 0x91bc, EDID_QUIRK_NON_DESKTOP },
    { "VLV", 0x91bd, EDID_QUIRK_NON_DESKTOP },
    { "VLV", 0x91be, EDID_QUIRK_NON_DESKTOP },
    { "VLV", 0x91bf, EDID_QUIRK_NON_DESKTOP },

    /* HTC Vive and Vive Pro VR Headsets */
    { "HVR", 0xaa01, EDID_QUIRK_NON_DESKTOP },
    { "HVR", 0xaa02, EDID_QUIRK_NON_DESKTOP },

    /* Oculus Rift DK1, DK2, CV1 and Rift S VR Headsets */
    { "OVR", 0x0001, EDID_QUIRK_NON_DESKTOP },
    { "OVR", 0x0003, EDID_QUIRK_NON_DESKTOP },
    { "OVR", 0x0004, EDID_QUIRK_NON_DESKTOP },
    { "OVR", 0x0012, EDID_QUIRK_NON_DESKTOP },

    /* Windows Mixed Reality Headsets */
    { "ACR", 0x7fce, EDID_QUIRK_NON_DESKTOP },
    { "HPN", 0x3515, EDID_QUIRK_NON_DESKTOP },
    { "LEN", 0x0408, EDID_QUIRK_NON_DESKTOP },
    { "LEN", 0xb800, EDID_QUIRK_NON_DESKTOP },
    { "FUJ", 0x1970, EDID_QUIRK_NON_DESKTOP },
    { "DEL", 0x7fce, EDID_QUIRK_NON_DESKTOP },
    { "SEC", 0x144a, EDID_QUIRK_NON_DESKTOP },
    { "AUS", 0xc102, EDID_QUIRK_NON_DESKTOP },

    /* Sony PlayStation VR Headset */
    { "SNY", 0x0704, EDID_QUIRK_NON_DESKTOP },

    /* Sensics VR Headsets */
    { "SEN", 0x1019, EDID_QUIRK_NON_DESKTOP },

    /* OSVR HDK and HDK2 VR Headsets */
    { "SVR", 0x1019, EDID_QUIRK_NON_DESKTOP },
};

/*
 * Autogenerated from the DMT spec.
 * This table is copied from xfree86/modes/xf86EdidModes.c.
 */
static disp_display_mode_t disp_dmt_modes[] = 
{
    /* 0x01 - 640x350@85Hz */
    { DISP_MODE("640x350", DISP_MODE_TYPE_DRIVER, 31500, 640, 672,
           736, 832, 0, 350, 382, 385, 445, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_NVSYNC) },
    /* 0x02 - 640x400@85Hz */
    { DISP_MODE("640x400", DISP_MODE_TYPE_DRIVER, 31500, 640, 672,
           736, 832, 0, 400, 401, 404, 445, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x03 - 720x400@85Hz */
    { DISP_MODE("720x400", DISP_MODE_TYPE_DRIVER, 35500, 720, 756,
           828, 936, 0, 400, 401, 404, 446, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x04 - 640x480@60Hz */
    { DISP_MODE("640x480", DISP_MODE_TYPE_DRIVER, 25175, 640, 656,
           752, 800, 0, 480, 490, 492, 525, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC) },
    /* 0x05 - 640x480@72Hz */
    { DISP_MODE("640x480", DISP_MODE_TYPE_DRIVER, 31500, 640, 664,
           704, 832, 0, 480, 489, 492, 520, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC) },
    /* 0x06 - 640x480@75Hz */
    { DISP_MODE("640x480", DISP_MODE_TYPE_DRIVER, 31500, 640, 656,
           720, 840, 0, 480, 481, 484, 500, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC) },
    /* 0x07 - 640x480@85Hz */
    { DISP_MODE("640x480", DISP_MODE_TYPE_DRIVER, 36000, 640, 696,
           752, 832, 0, 480, 481, 484, 509, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC) },
    /* 0x08 - 800x600@56Hz */
    { DISP_MODE("800x600", DISP_MODE_TYPE_DRIVER, 36000, 800, 824,
           896, 1024, 0, 600, 601, 603, 625, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x09 - 800x600@60Hz */
    { DISP_MODE("800x600", DISP_MODE_TYPE_DRIVER, 40000, 800, 840,
           968, 1056, 0, 600, 601, 605, 628, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x0a - 800x600@72Hz */
    { DISP_MODE("800x600", DISP_MODE_TYPE_DRIVER, 50000, 800, 856,
           976, 1040, 0, 600, 637, 643, 666, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x0b - 800x600@75Hz */
    { DISP_MODE("800x600", DISP_MODE_TYPE_DRIVER, 49500, 800, 816,
           896, 1056, 0, 600, 601, 604, 625, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x0c - 800x600@85Hz */
    { DISP_MODE("800x600", DISP_MODE_TYPE_DRIVER, 56250, 800, 832,
           896, 1048, 0, 600, 601, 604, 631, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x0d - 800x600@120Hz RB */
    { DISP_MODE("800x600", DISP_MODE_TYPE_DRIVER, 73250, 800, 848,
           880, 960, 0, 600, 603, 607, 636, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_NVSYNC) },
    /* 0x0e - 848x480@60Hz */
    { DISP_MODE("848x480", DISP_MODE_TYPE_DRIVER, 33750, 848, 864,
           976, 1088, 0, 480, 486, 494, 517, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x0f - 1024x768@43Hz, interlace */
    { DISP_MODE("1024x768i", DISP_MODE_TYPE_DRIVER, 44900, 1024, 1032,
           1208, 1264, 0, 768, 768, 776, 817, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC |
           DISP_MODE_FLAG_INTERLACE) },
    /* 0x10 - 1024x768@60Hz */
    { DISP_MODE("1024x768", DISP_MODE_TYPE_DRIVER, 65000, 1024, 1048,
           1184, 1344, 0, 768, 771, 777, 806, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC) },
    /* 0x11 - 1024x768@70Hz */
    { DISP_MODE("1024x768", DISP_MODE_TYPE_DRIVER, 75000, 1024, 1048,
           1184, 1328, 0, 768, 771, 777, 806, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC) },
    /* 0x12 - 1024x768@75Hz */
    { DISP_MODE("1024x768", DISP_MODE_TYPE_DRIVER, 78750, 1024, 1040,
           1136, 1312, 0, 768, 769, 772, 800, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x13 - 1024x768@85Hz */
    { DISP_MODE("1024x768", DISP_MODE_TYPE_DRIVER, 94500, 1024, 1072,
           1168, 1376, 0, 768, 769, 772, 808, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x14 - 1024x768@120Hz RB */
    { DISP_MODE("1024x768", DISP_MODE_TYPE_DRIVER, 115500, 1024, 1072,
           1104, 1184, 0, 768, 771, 775, 813, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_NVSYNC) },
    /* 0x15 - 1152x864@75Hz */
    { DISP_MODE("1152x864", DISP_MODE_TYPE_DRIVER, 108000, 1152, 1216,
           1344, 1600, 0, 864, 865, 868, 900, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x55 - 1280x720@60Hz */
    { DISP_MODE("1280x720", DISP_MODE_TYPE_DRIVER, 74250, 1280, 1390,
           1430, 1650, 0, 720, 725, 730, 750, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x16 - 1280x768@60Hz RB */
    { DISP_MODE("1280x768", DISP_MODE_TYPE_DRIVER, 68250, 1280, 1328,
           1360, 1440, 0, 768, 771, 778, 790, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_NVSYNC) },
    /* 0x17 - 1280x768@60Hz */
    { DISP_MODE("1280x768", DISP_MODE_TYPE_DRIVER, 79500, 1280, 1344,
           1472, 1664, 0, 768, 771, 778, 798, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x18 - 1280x768@75Hz */
    { DISP_MODE("1280x768", DISP_MODE_TYPE_DRIVER, 102250, 1280, 1360,
           1488, 1696, 0, 768, 771, 778, 805, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x19 - 1280x768@85Hz */
    { DISP_MODE("1280x768", DISP_MODE_TYPE_DRIVER, 117500, 1280, 1360,
           1496, 1712, 0, 768, 771, 778, 809, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x1a - 1280x768@120Hz RB */
    { DISP_MODE("1280x768", DISP_MODE_TYPE_DRIVER, 140250, 1280, 1328,
           1360, 1440, 0, 768, 771, 778, 813, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_NVSYNC) },
    /* 0x1b - 1280x800@60Hz RB */
    { DISP_MODE("1280x800", DISP_MODE_TYPE_DRIVER, 71000, 1280, 1328,
           1360, 1440, 0, 800, 803, 809, 823, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_NVSYNC) },
    /* 0x1c - 1280x800@60Hz */
    { DISP_MODE("1280x800", DISP_MODE_TYPE_DRIVER, 83500, 1280, 1352,
           1480, 1680, 0, 800, 803, 809, 831, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x1d - 1280x800@75Hz */
    { DISP_MODE("1280x800", DISP_MODE_TYPE_DRIVER, 106500, 1280, 1360,
           1488, 1696, 0, 800, 803, 809, 838, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x1e - 1280x800@85Hz */
    { DISP_MODE("1280x800", DISP_MODE_TYPE_DRIVER, 122500, 1280, 1360,
           1496, 1712, 0, 800, 803, 809, 843, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x1f - 1280x800@120Hz RB */
    { DISP_MODE("1280x800", DISP_MODE_TYPE_DRIVER, 146250, 1280, 1328,
           1360, 1440, 0, 800, 803, 809, 847, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_NVSYNC) },
    /* 0x20 - 1280x960@60Hz */
    { DISP_MODE("1280x960", DISP_MODE_TYPE_DRIVER, 108000, 1280, 1376,
           1488, 1800, 0, 960, 961, 964, 1000, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x21 - 1280x960@85Hz */
    { DISP_MODE("1280x960", DISP_MODE_TYPE_DRIVER, 148500, 1280, 1344,
           1504, 1728, 0, 960, 961, 964, 1011, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x22 - 1280x960@120Hz RB */
    { DISP_MODE("1280x960", DISP_MODE_TYPE_DRIVER, 175500, 1280, 1328,
           1360, 1440, 0, 960, 963, 967, 1017, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_NVSYNC) },
    /* 0x23 - 1280x1024@60Hz */
    { DISP_MODE("1280x1024", DISP_MODE_TYPE_DRIVER, 108000, 1280, 1328,
           1440, 1688, 0, 1024, 1025, 1028, 1066, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x24 - 1280x1024@75Hz */
    { DISP_MODE("1280x1024", DISP_MODE_TYPE_DRIVER, 135000, 1280, 1296,
           1440, 1688, 0, 1024, 1025, 1028, 1066, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x25 - 1280x1024@85Hz */
    { DISP_MODE("1280x1024", DISP_MODE_TYPE_DRIVER, 157500, 1280, 1344,
           1504, 1728, 0, 1024, 1025, 1028, 1072, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x26 - 1280x1024@120Hz RB */
    { DISP_MODE("1280x1024", DISP_MODE_TYPE_DRIVER, 187250, 1280, 1328,
           1360, 1440, 0, 1024, 1027, 1034, 1084, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_NVSYNC) },
    /* 0x27 - 1360x768@60Hz */
    { DISP_MODE("1360x768", DISP_MODE_TYPE_DRIVER, 85500, 1360, 1424,
           1536, 1792, 0, 768, 771, 777, 795, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x28 - 1360x768@120Hz RB */
    { DISP_MODE("1360x768", DISP_MODE_TYPE_DRIVER, 148250, 1360, 1408,
           1440, 1520, 0, 768, 771, 776, 813, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_NVSYNC) },
    /* 0x51 - 1366x768@60Hz */
    { DISP_MODE("1366x768", DISP_MODE_TYPE_DRIVER, 85500, 1366, 1436,
           1579, 1792, 0, 768, 771, 774, 798, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x56 - 1366x768@60Hz */
    { DISP_MODE("1366x768", DISP_MODE_TYPE_DRIVER, 72000, 1366, 1380,
           1436, 1500, 0, 768, 769, 772, 800, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x29 - 1400x1050@60Hz RB */
    { DISP_MODE("1400x1050", DISP_MODE_TYPE_DRIVER, 101000, 1400, 1448,
           1480, 1560, 0, 1050, 1053, 1057, 1080, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_NVSYNC) },
    /* 0x2a - 1400x1050@60Hz */
    { DISP_MODE("1400x1050", DISP_MODE_TYPE_DRIVER, 121750, 1400, 1488,
           1632, 1864, 0, 1050, 1053, 1057, 1089, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x2b - 1400x1050@75Hz */
    { DISP_MODE("1400x1050", DISP_MODE_TYPE_DRIVER, 156000, 1400, 1504,
           1648, 1896, 0, 1050, 1053, 1057, 1099, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x2c - 1400x1050@85Hz */
    { DISP_MODE("1400x1050", DISP_MODE_TYPE_DRIVER, 179500, 1400, 1504,
           1656, 1912, 0, 1050, 1053, 1057, 1105, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x2d - 1400x1050@120Hz RB */
    { DISP_MODE("1400x1050", DISP_MODE_TYPE_DRIVER, 208000, 1400, 1448,
           1480, 1560, 0, 1050, 1053, 1057, 1112, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_NVSYNC) },
    /* 0x2e - 1440x900@60Hz RB */
    { DISP_MODE("1440x900", DISP_MODE_TYPE_DRIVER, 88750, 1440, 1488,
           1520, 1600, 0, 900, 903, 909, 926, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_NVSYNC) },
    /* 0x2f - 1440x900@60Hz */
    { DISP_MODE("1440x900", DISP_MODE_TYPE_DRIVER, 106500, 1440, 1520,
           1672, 1904, 0, 900, 903, 909, 934, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x30 - 1440x900@75Hz */
    { DISP_MODE("1440x900", DISP_MODE_TYPE_DRIVER, 136750, 1440, 1536,
           1688, 1936, 0, 900, 903, 909, 942, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x31 - 1440x900@85Hz */
    { DISP_MODE("1440x900", DISP_MODE_TYPE_DRIVER, 157000, 1440, 1544,
           1696, 1952, 0, 900, 903, 909, 948, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x32 - 1440x900@120Hz RB */
    { DISP_MODE("1440x900", DISP_MODE_TYPE_DRIVER, 182750, 1440, 1488,
           1520, 1600, 0, 900, 903, 909, 953, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_NVSYNC) },
    /* 0x53 - 1600x900@60Hz */
    { DISP_MODE("1600x900", DISP_MODE_TYPE_DRIVER, 108000, 1600, 1624,
           1704, 1800, 0, 900, 901, 904, 1000, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x33 - 1600x1200@60Hz */
    { DISP_MODE("1600x1200", DISP_MODE_TYPE_DRIVER, 162000, 1600, 1664,
           1856, 2160, 0, 1200, 1201, 1204, 1250, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x34 - 1600x1200@65Hz */
    { DISP_MODE("1600x1200", DISP_MODE_TYPE_DRIVER, 175500, 1600, 1664,
           1856, 2160, 0, 1200, 1201, 1204, 1250, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x35 - 1600x1200@70Hz */
    { DISP_MODE("1600x1200", DISP_MODE_TYPE_DRIVER, 189000, 1600, 1664,
           1856, 2160, 0, 1200, 1201, 1204, 1250, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x36 - 1600x1200@75Hz */
    { DISP_MODE("1600x1200", DISP_MODE_TYPE_DRIVER, 202500, 1600, 1664,
           1856, 2160, 0, 1200, 1201, 1204, 1250, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x37 - 1600x1200@85Hz */
    { DISP_MODE("1600x1200", DISP_MODE_TYPE_DRIVER, 229500, 1600, 1664,
           1856, 2160, 0, 1200, 1201, 1204, 1250, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x38 - 1600x1200@120Hz RB */
    { DISP_MODE("1600x1200", DISP_MODE_TYPE_DRIVER, 268250, 1600, 1648,
           1680, 1760, 0, 1200, 1203, 1207, 1271, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_NVSYNC) },
    /* 0x39 - 1680x1050@60Hz RB */
    { DISP_MODE("1680x1050", DISP_MODE_TYPE_DRIVER, 119000, 1680, 1728,
           1760, 1840, 0, 1050, 1053, 1059, 1080, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_NVSYNC) },
    /* 0x3a - 1680x1050@60Hz */
    { DISP_MODE("1680x1050", DISP_MODE_TYPE_DRIVER, 146250, 1680, 1784,
           1960, 2240, 0, 1050, 1053, 1059, 1089, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x3b - 1680x1050@75Hz */
    { DISP_MODE("1680x1050", DISP_MODE_TYPE_DRIVER, 187000, 1680, 1800,
           1976, 2272, 0, 1050, 1053, 1059, 1099, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x3c - 1680x1050@85Hz */
    { DISP_MODE("1680x1050", DISP_MODE_TYPE_DRIVER, 214750, 1680, 1808,
           1984, 2288, 0, 1050, 1053, 1059, 1105, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x3d - 1680x1050@120Hz RB */
    { DISP_MODE("1680x1050", DISP_MODE_TYPE_DRIVER, 245500, 1680, 1728,
           1760, 1840, 0, 1050, 1053, 1059, 1112, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_NVSYNC) },
    /* 0x3e - 1792x1344@60Hz */
    { DISP_MODE("1792x1344", DISP_MODE_TYPE_DRIVER, 204750, 1792, 1920,
           2120, 2448, 0, 1344, 1345, 1348, 1394, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x3f - 1792x1344@75Hz */
    { DISP_MODE("1792x1344", DISP_MODE_TYPE_DRIVER, 261000, 1792, 1888,
           2104, 2456, 0, 1344, 1345, 1348, 1417, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x40 - 1792x1344@120Hz RB */
    { DISP_MODE("1792x1344", DISP_MODE_TYPE_DRIVER, 333250, 1792, 1840,
           1872, 1952, 0, 1344, 1347, 1351, 1423, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_NVSYNC) },
    /* 0x41 - 1856x1392@60Hz */
    { DISP_MODE("1856x1392", DISP_MODE_TYPE_DRIVER, 218250, 1856, 1952,
           2176, 2528, 0, 1392, 1393, 1396, 1439, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x42 - 1856x1392@75Hz */
    { DISP_MODE("1856x1392", DISP_MODE_TYPE_DRIVER, 288000, 1856, 1984,
           2208, 2560, 0, 1392, 1393, 1396, 1500, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x43 - 1856x1392@120Hz RB */
    { DISP_MODE("1856x1392", DISP_MODE_TYPE_DRIVER, 356500, 1856, 1904,
           1936, 2016, 0, 1392, 1395, 1399, 1474, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_NVSYNC) },
    /* 0x52 - 1920x1080@60Hz */
    { DISP_MODE("1920x1080", DISP_MODE_TYPE_DRIVER, 148500, 1920, 2008,
           2052, 2200, 0, 1080, 1084, 1089, 1125, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC) },
    /* 0x44 - 1920x1200@60Hz RB */
    { DISP_MODE("1920x1200", DISP_MODE_TYPE_DRIVER, 154000, 1920, 1968,
           2000, 2080, 0, 1200, 1203, 1209, 1235, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_NVSYNC) },
    /* 0x45 - 1920x1200@60Hz */
    { DISP_MODE("1920x1200", DISP_MODE_TYPE_DRIVER, 193250, 1920, 2056,
           2256, 2592, 0, 1200, 1203, 1209, 1245, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x46 - 1920x1200@75Hz */
    { DISP_MODE("1920x1200", DISP_MODE_TYPE_DRIVER, 245250, 1920, 2056,
           2264, 2608, 0, 1200, 1203, 1209, 1255, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x47 - 1920x1200@85Hz */
    { DISP_MODE("1920x1200", DISP_MODE_TYPE_DRIVER, 281250, 1920, 2064,
           2272, 2624, 0, 1200, 1203, 1209, 1262, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x48 - 1920x1200@120Hz RB */
    { DISP_MODE("1920x1200", DISP_MODE_TYPE_DRIVER, 317000, 1920, 1968,
           2000, 2080, 0, 1200, 1203, 1209, 1271, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_NVSYNC) },
    /* 0x49 - 1920x1440@60Hz */
    { DISP_MODE("1920x1440", DISP_MODE_TYPE_DRIVER, 234000, 1920, 2048,
           2256, 2600, 0, 1440, 1441, 1444, 1500, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x4a - 1920x1440@75Hz */
    { DISP_MODE("1920x1440", DISP_MODE_TYPE_DRIVER, 297000, 1920, 2064,
           2288, 2640, 0, 1440, 1441, 1444, 1500, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x4b - 1920x1440@120Hz RB */
    { DISP_MODE("1920x1440", DISP_MODE_TYPE_DRIVER, 380500, 1920, 1968,
           2000, 2080, 0, 1440, 1443, 1447, 1525, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_NVSYNC) },
    /* 0x54 - 2048x1152@60Hz */
    { DISP_MODE("2048x1152", DISP_MODE_TYPE_DRIVER, 162000, 2048, 2074,
           2154, 2250, 0, 1152, 1153, 1156, 1200, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x4c - 2560x1600@60Hz RB */
    { DISP_MODE("2560x1600", DISP_MODE_TYPE_DRIVER, 268500, 2560, 2608,
           2640, 2720, 0, 1600, 1603, 1609, 1646, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_NVSYNC) },
    /* 0x4d - 2560x1600@60Hz */
    { DISP_MODE("2560x1600", DISP_MODE_TYPE_DRIVER, 348500, 2560, 2752,
           3032, 3504, 0, 1600, 1603, 1609, 1658, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x4e - 2560x1600@75Hz */
    { DISP_MODE("2560x1600", DISP_MODE_TYPE_DRIVER, 443250, 2560, 2768,
           3048, 3536, 0, 1600, 1603, 1609, 1672, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x4f - 2560x1600@85Hz */
    { DISP_MODE("2560x1600", DISP_MODE_TYPE_DRIVER, 505250, 2560, 2768,
           3048, 3536, 0, 1600, 1603, 1609, 1682, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_PVSYNC) },
    /* 0x50 - 2560x1600@120Hz RB */
    { DISP_MODE("2560x1600", DISP_MODE_TYPE_DRIVER, 552750, 2560, 2608,
           2640, 2720, 0, 1600, 1603, 1609, 1694, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_NVSYNC) },
    /* 0x57 - 4096x2160@60Hz RB */
    { DISP_MODE("4096x2160", DISP_MODE_TYPE_DRIVER, 556744, 4096, 4104,
           4136, 4176, 0, 2160, 2208, 2216, 2222, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_NVSYNC) },
    /* 0x58 - 4096x2160@59.94Hz RB */
    { DISP_MODE("4096x2160", DISP_MODE_TYPE_DRIVER, 556188, 4096, 4104,
           4136, 4176, 0, 2160, 2208, 2216, 2222, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_NVSYNC) },
};

/*
 * These more or less come from the DMT spec.  The 720x400 modes are
 * inferred from historical 80x25 practice.  The 640x480@67 and 832x624@75
 * modes are old-school Mac modes.  The EDID spec says the 1152x864@75 mode
 * should be 1152x870, again for the Mac, but instead we use the x864 DMT
 * mode.
 *
 * The DMT modes have been fact-checked; the rest are mild guesses.
 */
static disp_display_mode_t edid_est_modes[] = 
{
    { DISP_MODE("800x600", DISP_MODE_TYPE_DRIVER, 40000, 800, 840,
           968, 1056, 0, 600, 601, 605, 628, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC) }, /* 800x600@60Hz */
    { DISP_MODE("800x600", DISP_MODE_TYPE_DRIVER, 36000, 800, 824,
           896, 1024, 0, 600, 601, 603,  625, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC) }, /* 800x600@56Hz */
    { DISP_MODE("640x480", DISP_MODE_TYPE_DRIVER, 31500, 640, 656,
           720, 840, 0, 480, 481, 484, 500, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC) }, /* 640x480@75Hz */
    { DISP_MODE("640x480", DISP_MODE_TYPE_DRIVER, 31500, 640, 664,
           704,  832, 0, 480, 489, 492, 520, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC) }, /* 640x480@72Hz */
    { DISP_MODE("640x480", DISP_MODE_TYPE_DRIVER, 30240, 640, 704,
           768,  864, 0, 480, 483, 486, 525, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC) }, /* 640x480@67Hz */
    { DISP_MODE("640x480", DISP_MODE_TYPE_DRIVER, 25175, 640, 656,
           752, 800, 0, 480, 490, 492, 525, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC) }, /* 640x480@60Hz */
    { DISP_MODE("720x400", DISP_MODE_TYPE_DRIVER, 35500, 720, 738,
           846, 900, 0, 400, 421, 423,  449, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC) }, /* 720x400@88Hz */
    { DISP_MODE("720x400", DISP_MODE_TYPE_DRIVER, 28320, 720, 738,
           846,  900, 0, 400, 412, 414, 449, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_PVSYNC) }, /* 720x400@70Hz */
    { DISP_MODE("1280x1024", DISP_MODE_TYPE_DRIVER, 135000, 1280, 1296,
           1440, 1688, 0, 1024, 1025, 1028, 1066, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC) }, /* 1280x1024@75Hz */
    { DISP_MODE("1024x768", DISP_MODE_TYPE_DRIVER, 78750, 1024, 1040,
           1136, 1312, 0,  768, 769, 772, 800, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC) }, /* 1024x768@75Hz */
    { DISP_MODE("1024x768", DISP_MODE_TYPE_DRIVER, 75000, 1024, 1048,
           1184, 1328, 0,  768, 771, 777, 806, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC) }, /* 1024x768@70Hz */
    { DISP_MODE("1024x768", DISP_MODE_TYPE_DRIVER, 65000, 1024, 1048,
           1184, 1344, 0,  768, 771, 777, 806, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC) }, /* 1024x768@60Hz */
    { DISP_MODE("1024x768i", DISP_MODE_TYPE_DRIVER,44900, 1024, 1032,
           1208, 1264, 0, 768, 768, 776, 817, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC | DISP_MODE_FLAG_INTERLACE) }, /* 1024x768@43Hz */
    { DISP_MODE("832x624", DISP_MODE_TYPE_DRIVER, 57284, 832, 864,
           928, 1152, 0, 624, 625, 628, 667, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC) }, /* 832x624@75Hz */
    { DISP_MODE("800x600", DISP_MODE_TYPE_DRIVER, 49500, 800, 816,
           896, 1056, 0, 600, 601, 604,  625, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC) }, /* 800x600@75Hz */
    { DISP_MODE("800x600", DISP_MODE_TYPE_DRIVER, 50000, 800, 856,
           976, 1040, 0, 600, 637, 643, 666, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC) }, /* 800x600@72Hz */
    { DISP_MODE("1152x864", DISP_MODE_TYPE_DRIVER, 108000, 1152, 1216,
           1344, 1600, 0,  864, 865, 868, 900, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC) }, /* 1152x864@75Hz */
};

struct minimode 
{
    disp_s16_t w;
    disp_s16_t h;
    disp_s16_t r;
    disp_u8_t rb;
    disp_u8_t reserved;
};

struct disp_common_mode_t
{
    disp_s16_t hactive;
    disp_s16_t vactive;
    disp_u32_t flags;
};

static struct minimode est3_modes[] = 
{
    /* byte 6 */
    { 640, 350, 85, 0 },
    { 640, 400, 85, 0 },
    { 720, 400, 85, 0 },
    { 640, 480, 85, 0 },
    { 848, 480, 60, 0 },
    { 800, 600, 85, 0 },
    { 1024, 768, 85, 0 },
    { 1152, 864, 75, 0 },
    /* byte 7 */
    { 1280, 768, 60, 1 },
    { 1280, 768, 60, 0 },
    { 1280, 768, 75, 0 },
    { 1280, 768, 85, 0 },
    { 1280, 960, 60, 0 },
    { 1280, 960, 85, 0 },
    { 1280, 1024, 60, 0 },
    { 1280, 1024, 85, 0 },
    /* byte 8 */
    { 1360, 768, 60, 0 },
    { 1440, 900, 60, 1 },
    { 1440, 900, 60, 0 },
    { 1440, 900, 75, 0 },
    { 1440, 900, 85, 0 },
    { 1400, 1050, 60, 1 },
    { 1400, 1050, 60, 0 },
    { 1400, 1050, 75, 0 },
    /* byte 9 */
    { 1400, 1050, 85, 0 },
    { 1680, 1050, 60, 1 },
    { 1680, 1050, 60, 0 },
    { 1680, 1050, 75, 0 },
    { 1680, 1050, 85, 0 },
    { 1600, 1200, 60, 0 },
    { 1600, 1200, 65, 0 },
    { 1600, 1200, 70, 0 },
    /* byte 10 */
    { 1600, 1200, 75, 0 },
    { 1600, 1200, 85, 0 },
    { 1792, 1344, 60, 0 },
    { 1792, 1344, 75, 0 },
    { 1856, 1392, 60, 0 },
    { 1856, 1392, 75, 0 },
    { 1920, 1200, 60, 1 },
    { 1920, 1200, 60, 0 },
    /* byte 11 */
    { 1920, 1200, 75, 0 },
    { 1920, 1200, 85, 0 },
    { 1920, 1440, 60, 0 },
    { 1920, 1440, 75, 0 },
};

static struct minimode extra_modes[] = 
{
    { 1024, 576,  60, 0 },
    { 1366, 768,  60, 0 },
    { 1600, 900,  60, 0 },
    { 1680, 945,  60, 0 },
    { 1920, 1080, 60, 0 },
    { 2048, 1152, 60, 0 },
    { 2048, 1536, 60, 0 },
};

static struct disp_common_mode_t common_modes[] =
{
    {640,  480,  DISP_MODE_FLAG_ADAPTER_SOURCE},
    {800,  600,  DISP_MODE_FLAG_ADAPTER_SOURCE},
    {1024, 768,  DISP_MODE_FLAG_ADAPTER_SOURCE},
    {1280, 720,  DISP_MODE_FLAG_ADAPTER_SOURCE},
    {1280, 1024, DISP_MODE_FLAG_ADAPTER_SOURCE},
    {1360, 768,  DISP_MODE_FLAG_ADAPTER_SOURCE},
    {1366, 768,  DISP_MODE_FLAG_ADAPTER_SOURCE},
    {1920, 1080, DISP_MODE_FLAG_ADAPTER_SOURCE},

    /* This addition ref AMD.
     * Intel have non addition in Linux, but addition more modes than AMD in Win10.
     * There are too many modes that we do not follow until find it's necessary.
     */
    {1280, 800,  DISP_MODE_FLAG_ADAPTER_SOURCE},
    {1440, 900,  DISP_MODE_FLAG_ADAPTER_SOURCE},
    {1680, 1050, DISP_MODE_FLAG_ADAPTER_SOURCE},
    {1600, 1200, DISP_MODE_FLAG_ADAPTER_SOURCE},
    {1920, 1200, DISP_MODE_FLAG_ADAPTER_SOURCE}
};


/*
 * From CEA/CTA-861 spec.
 *
 * Do not access directly, instead always use cea_mode_for_vic().
 */
static disp_display_mode_t edid_cea_modes_1[] = 
{
    /* 1 - 640x480@60Hz 4:3 */
    { DISP_MODE("640x480", DISP_MODE_TYPE_DRIVER, 25175, 640, 656,
           752, 800, 0, 480, 490, 492, 525, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3, .vic = 1, },
    /* 2 - 720x480@60Hz 4:3 */
    { DISP_MODE("720x480", DISP_MODE_TYPE_DRIVER, 27000, 720, 736,
           798, 858, 0, 480, 489, 495, 525, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3, .vic = 2, },
    /* 3 - 720x480@60Hz 16:9 */
    { DISP_MODE("720x480", DISP_MODE_TYPE_DRIVER, 27000, 720, 736,
           798, 858, 0, 480, 489, 495, 525, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 3, },
    /* 4 - 1280x720@60Hz 16:9 */
    { DISP_MODE("1280x720", DISP_MODE_TYPE_DRIVER, 74250, 1280, 1390,
           1430, 1650, 0, 720, 725, 730, 750, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 4, },
    /* 5 - 1920x1080i@60Hz 16:9 */
    { DISP_MODE("1920x1080i", DISP_MODE_TYPE_DRIVER, 74250, 1920, 2008,
           2052, 2200, 0, 1080, 1084, 1094, 1125, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC |
           DISP_MODE_FLAG_INTERLACE),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 5, },
    /* 6 - 720(1440)x480i@60Hz 4:3 */
    { DISP_MODE("720x480i", DISP_MODE_TYPE_DRIVER, 13500, 720, 739,
           801, 858, 0, 480, 488, 494, 525, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC |
           DISP_MODE_FLAG_INTERLACE | DISP_MODE_FLAG_DBLCLK),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3, .vic = 6, },
    /* 7 - 720(1440)x480i@60Hz 16:9 */
    { DISP_MODE("720x480i", DISP_MODE_TYPE_DRIVER, 13500, 720, 739,
           801, 858, 0, 480, 488, 494, 525, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC |
           DISP_MODE_FLAG_INTERLACE | DISP_MODE_FLAG_DBLCLK),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 7, },
    /* 8 - 720(1440)x240@60Hz 4:3 */
    { DISP_MODE("720x240", DISP_MODE_TYPE_DRIVER, 13500, 720, 739,
           801, 858, 0, 240, 244, 247, 262, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC |
           DISP_MODE_FLAG_DBLCLK),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3, .vic = 8, },
    /* 9 - 720(1440)x240@60Hz 16:9 */
    { DISP_MODE("720x240", DISP_MODE_TYPE_DRIVER, 13500, 720, 739,
           801, 858, 0, 240, 244, 247, 262, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC |
           DISP_MODE_FLAG_DBLCLK),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 9, },
    /* 10 - 2880x480i@60Hz 4:3 */
    { DISP_MODE("2880x480i", DISP_MODE_TYPE_DRIVER, 54000, 2880, 2956,
           3204, 3432, 0, 480, 488, 494, 525, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC |
           DISP_MODE_FLAG_INTERLACE),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3, .vic = 10, },
    /* 11 - 2880x480i@60Hz 16:9 */
    { DISP_MODE("2880x480i", DISP_MODE_TYPE_DRIVER, 54000, 2880, 2956,
           3204, 3432, 0, 480, 488, 494, 525, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC |
           DISP_MODE_FLAG_INTERLACE),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 11, },
    /* 12 - 2880x240@60Hz 4:3 */
    { DISP_MODE("2880x240", DISP_MODE_TYPE_DRIVER, 54000, 2880, 2956,
           3204, 3432, 0, 240, 244, 247, 262, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3, .vic = 12, },
    /* 13 - 2880x240@60Hz 16:9 */
    { DISP_MODE("2880x240", DISP_MODE_TYPE_DRIVER, 54000, 2880, 2956,
           3204, 3432, 0, 240, 244, 247, 262, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 13, },
    /* 14 - 1440x480@60Hz 4:3 */
    { DISP_MODE("1440x480", DISP_MODE_TYPE_DRIVER, 54000, 1440, 1472,
           1596, 1716, 0, 480, 489, 495, 525, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3, .vic = 14, },
    /* 15 - 1440x480@60Hz 16:9 */
    { DISP_MODE("1440x480", DISP_MODE_TYPE_DRIVER, 54000, 1440, 1472,
           1596, 1716, 0, 480, 489, 495, 525, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 15, },
    /* 16 - 1920x1080@60Hz 16:9 */
    { DISP_MODE("1920x1080", DISP_MODE_TYPE_DRIVER, 148500, 1920, 2008,
           2052, 2200, 0, 1080, 1084, 1089, 1125, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 16, },
    /* 17 - 720x576@50Hz 4:3 */
    { DISP_MODE("720x576", DISP_MODE_TYPE_DRIVER, 27000, 720, 732,
           796, 864, 0, 576, 581, 586, 625, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3, .vic = 17, },
    /* 18 - 720x576@50Hz 16:9 */
    { DISP_MODE("720x576", DISP_MODE_TYPE_DRIVER, 27000, 720, 732,
           796, 864, 0, 576, 581, 586, 625, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 18, },
    /* 19 - 1280x720@50Hz 16:9 */
    { DISP_MODE("1280x720", DISP_MODE_TYPE_DRIVER, 74250, 1280, 1720,
           1760, 1980, 0, 720, 725, 730, 750, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 19, },
    /* 20 - 1920x1080i@50Hz 16:9 */
    { DISP_MODE("1920x1080i", DISP_MODE_TYPE_DRIVER, 74250, 1920, 2448,
           2492, 2640, 0, 1080, 1084, 1094, 1125, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC |
           DISP_MODE_FLAG_INTERLACE),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 20, },
    /* 21 - 720(1440)x576i@50Hz 4:3 */
    { DISP_MODE("720x576i", DISP_MODE_TYPE_DRIVER, 13500, 720, 732,
           795, 864, 0, 576, 580, 586, 625, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC |
           DISP_MODE_FLAG_INTERLACE | DISP_MODE_FLAG_DBLCLK),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3, .vic = 21, },
    /* 22 - 720(1440)x576i@50Hz 16:9 */
    { DISP_MODE("720x576i", DISP_MODE_TYPE_DRIVER, 13500, 720, 732,
           795, 864, 0, 576, 580, 586, 625, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC |
           DISP_MODE_FLAG_INTERLACE | DISP_MODE_FLAG_DBLCLK),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 22, },
    /* 23 - 720(1440)x288@50Hz 4:3 */
    { DISP_MODE("720x288", DISP_MODE_TYPE_DRIVER, 13500, 720, 732,
           795, 864, 0, 288, 290, 293, 312, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC |
           DISP_MODE_FLAG_DBLCLK),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3, .vic = 23, },
    /* 24 - 720(1440)x288@50Hz 16:9 */
    { DISP_MODE("720x288", DISP_MODE_TYPE_DRIVER, 13500, 720, 732,
           795, 864, 0, 288, 290, 293, 312, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC |
           DISP_MODE_FLAG_DBLCLK),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 24, },
    /* 25 - 2880x576i@50Hz 4:3 */
    { DISP_MODE("2880x576i", DISP_MODE_TYPE_DRIVER, 54000, 2880, 2928,
           3180, 3456, 0, 576, 580, 586, 625, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC |
           DISP_MODE_FLAG_INTERLACE),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3, .vic = 25, },
    /* 26 - 2880x576i@50Hz 16:9 */
    { DISP_MODE("2880x576i", DISP_MODE_TYPE_DRIVER, 54000, 2880, 2928,
           3180, 3456, 0, 576, 580, 586, 625, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC |
           DISP_MODE_FLAG_INTERLACE),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 26, },
    /* 27 - 2880x288@50Hz 4:3 */
    { DISP_MODE("2880x288", DISP_MODE_TYPE_DRIVER, 54000, 2880, 2928,
           3180, 3456, 0, 288, 290, 293, 312, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3, .vic = 27, },
    /* 28 - 2880x288@50Hz 16:9 */
    { DISP_MODE("2880x288", DISP_MODE_TYPE_DRIVER, 54000, 2880, 2928,
           3180, 3456, 0, 288, 290, 293, 312, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 28, },
    /* 29 - 1440x576@50Hz 4:3 */
    { DISP_MODE("1440x576", DISP_MODE_TYPE_DRIVER, 54000, 1440, 1464,
           1592, 1728, 0, 576, 581, 586, 625, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3, .vic = 29, },
    /* 30 - 1440x576@50Hz 16:9 */
    { DISP_MODE("1440x576", DISP_MODE_TYPE_DRIVER, 54000, 1440, 1464,
           1592, 1728, 0, 576, 581, 586, 625, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 30, },
    /* 31 - 1920x1080@50Hz 16:9 */
    { DISP_MODE("1920x1080", DISP_MODE_TYPE_DRIVER, 148500, 1920, 2448,
           2492, 2640, 0, 1080, 1084, 1089, 1125, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 31, },
    /* 32 - 1920x1080@24Hz 16:9 */
    { DISP_MODE("1920x1080", DISP_MODE_TYPE_DRIVER, 74250, 1920, 2558,
           2602, 2750, 0, 1080, 1084, 1089, 1125, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 32, },
    /* 33 - 1920x1080@25Hz 16:9 */
    { DISP_MODE("1920x1080", DISP_MODE_TYPE_DRIVER, 74250, 1920, 2448,
           2492, 2640, 0, 1080, 1084, 1089, 1125, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 33, },
    /* 34 - 1920x1080@30Hz 16:9 */
    { DISP_MODE("1920x1080", DISP_MODE_TYPE_DRIVER, 74250, 1920, 2008,
           2052, 2200, 0, 1080, 1084, 1089, 1125, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 34, },
    /* 35 - 2880x480@60Hz 4:3 */
    { DISP_MODE("2880x480", DISP_MODE_TYPE_DRIVER, 108000, 2880, 2944,
           3192, 3432, 0, 480, 489, 495, 525, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3, .vic = 35, },
    /* 36 - 2880x480@60Hz 16:9 */
    { DISP_MODE("2880x480", DISP_MODE_TYPE_DRIVER, 108000, 2880, 2944,
           3192, 3432, 0, 480, 489, 495, 525, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 36, },
    /* 37 - 2880x576@50Hz 4:3 */
    { DISP_MODE("2880x576", DISP_MODE_TYPE_DRIVER, 108000, 2880, 2928,
           3184, 3456, 0, 576, 581, 586, 625, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3, .vic = 37, },
    /* 38 - 2880x576@50Hz 16:9 */
    { DISP_MODE("2880x576", DISP_MODE_TYPE_DRIVER, 108000, 2880, 2928,
           3184, 3456, 0, 576, 581, 586, 625, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 38, },
    /* 39 - 1920x1080i@50Hz 16:9 */
    { DISP_MODE("1920x1080i", DISP_MODE_TYPE_DRIVER, 72000, 1920, 1952,
           2120, 2304, 0, 1080, 1126, 1136, 1250, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_NVSYNC |
           DISP_MODE_FLAG_INTERLACE),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 39, },
    /* 40 - 1920x1080i@100Hz 16:9 */
    { DISP_MODE("1920x1080i", DISP_MODE_TYPE_DRIVER, 148500, 1920, 2448,
           2492, 2640, 0, 1080, 1084, 1094, 1125, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC |
           DISP_MODE_FLAG_INTERLACE),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 40, },
    /* 41 - 1280x720@100Hz 16:9 */
    { DISP_MODE("1280x720", DISP_MODE_TYPE_DRIVER, 148500, 1280, 1720,
           1760, 1980, 0, 720, 725, 730, 750, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 41, },
    /* 42 - 720x576@100Hz 4:3 */
    { DISP_MODE("720x576", DISP_MODE_TYPE_DRIVER, 54000, 720, 732,
           796, 864, 0, 576, 581, 586, 625, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3, .vic = 42, },
    /* 43 - 720x576@100Hz 16:9 */
    { DISP_MODE("720x576", DISP_MODE_TYPE_DRIVER, 54000, 720, 732,
           796, 864, 0, 576, 581, 586, 625, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 43, },
    /* 44 - 720(1440)x576i@100Hz 4:3 */
    { DISP_MODE("720x576i", DISP_MODE_TYPE_DRIVER, 27000, 720, 732,
           795, 864, 0, 576, 580, 586, 625, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC |
           DISP_MODE_FLAG_INTERLACE | DISP_MODE_FLAG_DBLCLK),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3, .vic = 44, },
    /* 45 - 720(1440)x576i@100Hz 16:9 */
    { DISP_MODE("720x576i", DISP_MODE_TYPE_DRIVER, 27000, 720, 732,
           795, 864, 0, 576, 580, 586, 625, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC |
           DISP_MODE_FLAG_INTERLACE | DISP_MODE_FLAG_DBLCLK),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 45, },
    /* 46 - 1920x1080i@120Hz 16:9 */
    { DISP_MODE("1920x1080i", DISP_MODE_TYPE_DRIVER, 148500, 1920, 2008,
           2052, 2200, 0, 1080, 1084, 1094, 1125, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC |
           DISP_MODE_FLAG_INTERLACE),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 46, },
    /* 47 - 1280x720@120Hz 16:9 */
    { DISP_MODE("1280x720", DISP_MODE_TYPE_DRIVER, 148500, 1280, 1390,
           1430, 1650, 0, 720, 725, 730, 750, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 47, },
    /* 48 - 720x480@120Hz 4:3 */
    { DISP_MODE("720x480", DISP_MODE_TYPE_DRIVER, 54000, 720, 736,
           798, 858, 0, 480, 489, 495, 525, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3, .vic = 48, },
    /* 49 - 720x480@120Hz 16:9 */
    { DISP_MODE("720x480", DISP_MODE_TYPE_DRIVER, 54000, 720, 736,
           798, 858, 0, 480, 489, 495, 525, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 49, },
    /* 50 - 720(1440)x480i@120Hz 4:3 */
    { DISP_MODE("720x480i", DISP_MODE_TYPE_DRIVER, 27000, 720, 739,
           801, 858, 0, 480, 488, 494, 525, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC |
           DISP_MODE_FLAG_INTERLACE | DISP_MODE_FLAG_DBLCLK),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3, .vic = 50, },
    /* 51 - 720(1440)x480i@120Hz 16:9 */
    { DISP_MODE("720x480i", DISP_MODE_TYPE_DRIVER, 27000, 720, 739,
           801, 858, 0, 480, 488, 494, 525, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC |
           DISP_MODE_FLAG_INTERLACE | DISP_MODE_FLAG_DBLCLK),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 51, },
    /* 52 - 720x576@200Hz 4:3 */
    { DISP_MODE("720x576", DISP_MODE_TYPE_DRIVER, 108000, 720, 732,
           796, 864, 0, 576, 581, 586, 625, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3, .vic = 52, },
    /* 53 - 720x576@200Hz 16:9 */
    { DISP_MODE("720x576", DISP_MODE_TYPE_DRIVER, 108000, 720, 732,
           796, 864, 0, 576, 581, 586, 625, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 53, },
    /* 54 - 720(1440)x576i@200Hz 4:3 */
    { DISP_MODE("720x576i", DISP_MODE_TYPE_DRIVER, 54000, 720, 732,
           795, 864, 0, 576, 580, 586, 625, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC |
           DISP_MODE_FLAG_INTERLACE | DISP_MODE_FLAG_DBLCLK),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3, .vic = 54, },
    /* 55 - 720(1440)x576i@200Hz 16:9 */
    { DISP_MODE("720x576i", DISP_MODE_TYPE_DRIVER, 54000, 720, 732,
           795, 864, 0, 576, 580, 586, 625, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC |
           DISP_MODE_FLAG_INTERLACE | DISP_MODE_FLAG_DBLCLK),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 55, },
    /* 56 - 720x480@240Hz 4:3 */
    { DISP_MODE("720x480", DISP_MODE_TYPE_DRIVER, 108000, 720, 736,
           798, 858, 0, 480, 489, 495, 525, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3, .vic = 56, },
    /* 57 - 720x480@240Hz 16:9 */
    { DISP_MODE("720x480", DISP_MODE_TYPE_DRIVER, 108000, 720, 736,
           798, 858, 0, 480, 489, 495, 525, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 57, },
    /* 58 - 720(1440)x480i@240Hz 4:3 */
    { DISP_MODE("720x480i", DISP_MODE_TYPE_DRIVER, 54000, 720, 739,
           801, 858, 0, 480, 488, 494, 525, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC |
           DISP_MODE_FLAG_INTERLACE | DISP_MODE_FLAG_DBLCLK),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3, .vic = 58, },
    /* 59 - 720(1440)x480i@240Hz 16:9 */
    { DISP_MODE("720x480i", DISP_MODE_TYPE_DRIVER, 54000, 720, 739,
           801, 858, 0, 480, 488, 494, 525, 0,
           DISP_MODE_FLAG_NHSYNC | DISP_MODE_FLAG_NVSYNC |
           DISP_MODE_FLAG_INTERLACE | DISP_MODE_FLAG_DBLCLK),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 59, },
    /* 60 - 1280x720@24Hz 16:9 */
    { DISP_MODE("1280x720", DISP_MODE_TYPE_DRIVER, 59400, 1280, 3040,
           3080, 3300, 0, 720, 725, 730, 750, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 60, },
    /* 61 - 1280x720@25Hz 16:9 */
    { DISP_MODE("1280x720", DISP_MODE_TYPE_DRIVER, 74250, 1280, 3700,
           3740, 3960, 0, 720, 725, 730, 750, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 61, },
    /* 62 - 1280x720@30Hz 16:9 */
    { DISP_MODE("1280x720", DISP_MODE_TYPE_DRIVER, 74250, 1280, 3040,
           3080, 3300, 0, 720, 725, 730, 750, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 62, },
    /* 63 - 1920x1080@120Hz 16:9 */
    { DISP_MODE("1920x1080", DISP_MODE_TYPE_DRIVER, 297000, 1920, 2008,
           2052, 2200, 0, 1080, 1084, 1089, 1125, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 63, },
    /* 64 - 1920x1080@100Hz 16:9 */
    { DISP_MODE("1920x1080", DISP_MODE_TYPE_DRIVER, 297000, 1920, 2448,
           2492, 2640, 0, 1080, 1084, 1089, 1125, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 64, },
    /* 65 - 1280x720@24Hz 64:27 */
    { DISP_MODE("1280x720", DISP_MODE_TYPE_DRIVER, 59400, 1280, 3040,
           3080, 3300, 0, 720, 725, 730, 750, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 65, },
    /* 66 - 1280x720@25Hz 64:27 */
    { DISP_MODE("1280x720", DISP_MODE_TYPE_DRIVER, 74250, 1280, 3700,
           3740, 3960, 0, 720, 725, 730, 750, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 66, },
    /* 67 - 1280x720@30Hz 64:27 */
    { DISP_MODE("1280x720", DISP_MODE_TYPE_DRIVER, 74250, 1280, 3040,
           3080, 3300, 0, 720, 725, 730, 750, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 67, },
    /* 68 - 1280x720@50Hz 64:27 */
    { DISP_MODE("1280x720", DISP_MODE_TYPE_DRIVER, 74250, 1280, 1720,
           1760, 1980, 0, 720, 725, 730, 750, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 68, },
    /* 69 - 1280x720@60Hz 64:27 */
    { DISP_MODE("1280x720", DISP_MODE_TYPE_DRIVER, 74250, 1280, 1390,
           1430, 1650, 0, 720, 725, 730, 750, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 69, },
    /* 70 - 1280x720@100Hz 64:27 */
    { DISP_MODE("1280x720", DISP_MODE_TYPE_DRIVER, 148500, 1280, 1720,
           1760, 1980, 0, 720, 725, 730, 750, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 70, },
    /* 71 - 1280x720@120Hz 64:27 */
    { DISP_MODE("1280x720", DISP_MODE_TYPE_DRIVER, 148500, 1280, 1390,
           1430, 1650, 0, 720, 725, 730, 750, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 71, },
    /* 72 - 1920x1080@24Hz 64:27 */
    { DISP_MODE("1920x1080", DISP_MODE_TYPE_DRIVER, 74250, 1920, 2558,
           2602, 2750, 0, 1080, 1084, 1089, 1125, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 72, },
    /* 73 - 1920x1080@25Hz 64:27 */
    { DISP_MODE("1920x1080", DISP_MODE_TYPE_DRIVER, 74250, 1920, 2448,
           2492, 2640, 0, 1080, 1084, 1089, 1125, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 73, },
    /* 74 - 1920x1080@30Hz 64:27 */
    { DISP_MODE("1920x1080", DISP_MODE_TYPE_DRIVER, 74250, 1920, 2008,
           2052, 2200, 0, 1080, 1084, 1089, 1125, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 74, },
    /* 75 - 1920x1080@50Hz 64:27 */
    { DISP_MODE("1920x1080", DISP_MODE_TYPE_DRIVER, 148500, 1920, 2448,
           2492, 2640, 0, 1080, 1084, 1089, 1125, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 75, },
    /* 76 - 1920x1080@60Hz 64:27 */
    { DISP_MODE("1920x1080", DISP_MODE_TYPE_DRIVER, 148500, 1920, 2008,
           2052, 2200, 0, 1080, 1084, 1089, 1125, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 76, },
    /* 77 - 1920x1080@100Hz 64:27 */
    { DISP_MODE("1920x1080", DISP_MODE_TYPE_DRIVER, 297000, 1920, 2448,
           2492, 2640, 0, 1080, 1084, 1089, 1125, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 77, },
    /* 78 - 1920x1080@120Hz 64:27 */
    { DISP_MODE("1920x1080", DISP_MODE_TYPE_DRIVER, 297000, 1920, 2008,
           2052, 2200, 0, 1080, 1084, 1089, 1125, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 78, },
    /* 79 - 1680x720@24Hz 64:27 */
    { DISP_MODE("1680x720", DISP_MODE_TYPE_DRIVER, 59400, 1680, 3040,
           3080, 3300, 0, 720, 725, 730, 750, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 79, },
    /* 80 - 1680x720@25Hz 64:27 */
    { DISP_MODE("1680x720", DISP_MODE_TYPE_DRIVER, 59400, 1680, 2908,
           2948, 3168, 0, 720, 725, 730, 750, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 80, },
    /* 81 - 1680x720@30Hz 64:27 */
    { DISP_MODE("1680x720", DISP_MODE_TYPE_DRIVER, 59400, 1680, 2380,
           2420, 2640, 0, 720, 725, 730, 750, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 81, },
    /* 82 - 1680x720@50Hz 64:27 */
    { DISP_MODE("1680x720", DISP_MODE_TYPE_DRIVER, 82500, 1680, 1940,
           1980, 2200, 0, 720, 725, 730, 750, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 82, },
    /* 83 - 1680x720@60Hz 64:27 */
    { DISP_MODE("1680x720", DISP_MODE_TYPE_DRIVER, 99000, 1680, 1940,
           1980, 2200, 0, 720, 725, 730, 750, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 83, },
    /* 84 - 1680x720@100Hz 64:27 */
    { DISP_MODE("1680x720", DISP_MODE_TYPE_DRIVER, 165000, 1680, 1740,
           1780, 2000, 0, 720, 725, 730, 825, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 84, },
    /* 85 - 1680x720@120Hz 64:27 */
    { DISP_MODE("1680x720", DISP_MODE_TYPE_DRIVER, 198000, 1680, 1740,
           1780, 2000, 0, 720, 725, 730, 825, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 85, },
    /* 86 - 2560x1080@24Hz 64:27 */
    { DISP_MODE("2560x1080", DISP_MODE_TYPE_DRIVER, 99000, 2560, 3558,
           3602, 3750, 0, 1080, 1084, 1089, 1100, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 86, },
    /* 87 - 2560x1080@25Hz 64:27 */
    { DISP_MODE("2560x1080", DISP_MODE_TYPE_DRIVER, 90000, 2560, 3008,
           3052, 3200, 0, 1080, 1084, 1089, 1125, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 87, },
    /* 88 - 2560x1080@30Hz 64:27 */
    { DISP_MODE("2560x1080", DISP_MODE_TYPE_DRIVER, 118800, 2560, 3328,
           3372, 3520, 0, 1080, 1084, 1089, 1125, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 88, },
    /* 89 - 2560x1080@50Hz 64:27 */
    { DISP_MODE("2560x1080", DISP_MODE_TYPE_DRIVER, 185625, 2560, 3108,
           3152, 3300, 0, 1080, 1084, 1089, 1125, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 89, },
    /* 90 - 2560x1080@60Hz 64:27 */
    { DISP_MODE("2560x1080", DISP_MODE_TYPE_DRIVER, 198000, 2560, 2808,
           2852, 3000, 0, 1080, 1084, 1089, 1100, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 90, },
    /* 91 - 2560x1080@100Hz 64:27 */
    { DISP_MODE("2560x1080", DISP_MODE_TYPE_DRIVER, 371250, 2560, 2778,
           2822, 2970, 0, 1080, 1084, 1089, 1250, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 91, },
    /* 92 - 2560x1080@120Hz 64:27 */
    { DISP_MODE("2560x1080", DISP_MODE_TYPE_DRIVER, 495000, 2560, 3108,
           3152, 3300, 0, 1080, 1084, 1089, 1250, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 92, },
    /* 93 - 3840x2160@24Hz 16:9 */
    { DISP_MODE("3840x2160", DISP_MODE_TYPE_DRIVER, 297000, 3840, 5116,
           5204, 5500, 0, 2160, 2168, 2178, 2250, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 93, },
    /* 94 - 3840x2160@25Hz 16:9 */
    { DISP_MODE("3840x2160", DISP_MODE_TYPE_DRIVER, 297000, 3840, 4896,
           4984, 5280, 0, 2160, 2168, 2178, 2250, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 94, },
    /* 95 - 3840x2160@30Hz 16:9 */
    { DISP_MODE("3840x2160", DISP_MODE_TYPE_DRIVER, 297000, 3840, 4016,
           4104, 4400, 0, 2160, 2168, 2178, 2250, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 95, },
    /* 96 - 3840x2160@50Hz 16:9 */
    { DISP_MODE("3840x2160", DISP_MODE_TYPE_DRIVER, 594000, 3840, 4896,
           4984, 5280, 0, 2160, 2168, 2178, 2250, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 96, },
    /* 97 - 3840x2160@60Hz 16:9 */
    { DISP_MODE("3840x2160", DISP_MODE_TYPE_DRIVER, 594000, 3840, 4016,
           4104, 4400, 0, 2160, 2168, 2178, 2250, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 97, },
    /* 98 - 4096x2160@24Hz 256:135 */
    { DISP_MODE("4096x2160", DISP_MODE_TYPE_DRIVER, 297000, 4096, 5116,
           5204, 5500, 0, 2160, 2168, 2178, 2250, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_256_135, .vic = 98, },
    /* 99 - 4096x2160@25Hz 256:135 */
    { DISP_MODE("4096x2160", DISP_MODE_TYPE_DRIVER, 297000, 4096, 5064,
           5152, 5280, 0, 2160, 2168, 2178, 2250, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_256_135, .vic = 99, },
    /* 100 - 4096x2160@30Hz 256:135 */
    { DISP_MODE("4096x2160", DISP_MODE_TYPE_DRIVER, 297000, 4096, 4184,
           4272, 4400, 0, 2160, 2168, 2178, 2250, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_256_135, .vic = 100, },
    /* 101 - 4096x2160@50Hz 256:135 */
    { DISP_MODE("4096x2160", DISP_MODE_TYPE_DRIVER, 594000, 4096, 5064,
           5152, 5280, 0, 2160, 2168, 2178, 2250, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_256_135, .vic = 101, },
    /* 102 - 4096x2160@60Hz 256:135 */
    { DISP_MODE("4096x2160", DISP_MODE_TYPE_DRIVER, 594000, 4096, 4184,
           4272, 4400, 0, 2160, 2168, 2178, 2250, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_256_135, .vic = 102, },
    /* 103 - 3840x2160@24Hz 64:27 */
    { DISP_MODE("3840x2160", DISP_MODE_TYPE_DRIVER, 297000, 3840, 5116,
           5204, 5500, 0, 2160, 2168, 2178, 2250, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 103, },
    /* 104 - 3840x2160@25Hz 64:27 */
    { DISP_MODE("3840x2160", DISP_MODE_TYPE_DRIVER, 297000, 3840, 4896,
           4984, 5280, 0, 2160, 2168, 2178, 2250, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 104, },
    /* 105 - 3840x2160@30Hz 64:27 */
    { DISP_MODE("3840x2160", DISP_MODE_TYPE_DRIVER, 297000, 3840, 4016,
           4104, 4400, 0, 2160, 2168, 2178, 2250, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 105, },
    /* 106 - 3840x2160@50Hz 64:27 */
    { DISP_MODE("3840x2160", DISP_MODE_TYPE_DRIVER, 594000, 3840, 4896,
           4984, 5280, 0, 2160, 2168, 2178, 2250, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 106, },
    /* 107 - 3840x2160@60Hz 64:27 */
    { DISP_MODE("3840x2160", DISP_MODE_TYPE_DRIVER, 594000, 3840, 4016,
           4104, 4400, 0, 2160, 2168, 2178, 2250, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 107, },
    /* 108 - 1280x720@48Hz 16:9 */
    { DISP_MODE("1280x720", DISP_MODE_TYPE_DRIVER, 90000, 1280, 2240,
           2280, 2500, 0, 720, 725, 730, 750, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 108, },
    /* 109 - 1280x720@48Hz 64:27 */
    { DISP_MODE("1280x720", DISP_MODE_TYPE_DRIVER, 90000, 1280, 2240,
           2280, 2500, 0, 720, 725, 730, 750, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 109, },
    /* 110 - 1680x720@48Hz 64:27 */
    { DISP_MODE("1680x720", DISP_MODE_TYPE_DRIVER, 99000, 1680, 2490,
           2530, 2750, 0, 720, 725, 730, 750, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 110, },
    /* 111 - 1920x1080@48Hz 16:9 */
    { DISP_MODE("1920x1080", DISP_MODE_TYPE_DRIVER, 148500, 1920, 2558,
           2602, 2750, 0, 1080, 1084, 1089, 1125, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 111, },
    /* 112 - 1920x1080@48Hz 64:27 */
    { DISP_MODE("1920x1080", DISP_MODE_TYPE_DRIVER, 148500, 1920, 2558,
           2602, 2750, 0, 1080, 1084, 1089, 1125, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 112, },
    /* 113 - 2560x1080@48Hz 64:27 */
    { DISP_MODE("2560x1080", DISP_MODE_TYPE_DRIVER, 198000, 2560, 3558,
           3602, 3750, 0, 1080, 1084, 1089, 1100, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 113, },
    /* 114 - 3840x2160@48Hz 16:9 */
    { DISP_MODE("3840x2160", DISP_MODE_TYPE_DRIVER, 594000, 3840, 5116,
           5204, 5500, 0, 2160, 2168, 2178, 2250, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 114, },
    /* 115 - 4096x2160@48Hz 256:135 */
    { DISP_MODE("4096x2160", DISP_MODE_TYPE_DRIVER, 594000, 4096, 5116,
           5204, 5500, 0, 2160, 2168, 2178, 2250, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_256_135, .vic = 115, },
    /* 116 - 3840x2160@48Hz 64:27 */
    { DISP_MODE("3840x2160", DISP_MODE_TYPE_DRIVER, 594000, 3840, 5116,
           5204, 5500, 0, 2160, 2168, 2178, 2250, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 116, },
    /* 117 - 3840x2160@100Hz 16:9 */
    { DISP_MODE("3840x2160", DISP_MODE_TYPE_DRIVER, 1188000, 3840, 4896,
           4984, 5280, 0, 2160, 2168, 2178, 2250, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 117, },
    /* 118 - 3840x2160@120Hz 16:9 */
    { DISP_MODE("3840x2160", DISP_MODE_TYPE_DRIVER, 1188000, 3840, 4016,
           4104, 4400, 0, 2160, 2168, 2178, 2250, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 118, },
    /* 119 - 3840x2160@100Hz 64:27 */
    { DISP_MODE("3840x2160", DISP_MODE_TYPE_DRIVER, 1188000, 3840, 4896,
           4984, 5280, 0, 2160, 2168, 2178, 2250, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 119, },
    /* 120 - 3840x2160@120Hz 64:27 */
    { DISP_MODE("3840x2160", DISP_MODE_TYPE_DRIVER, 1188000, 3840, 4016,
           4104, 4400, 0, 2160, 2168, 2178, 2250, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 120, },
    /* 121 - 5120x2160@24Hz 64:27 */
    { DISP_MODE("5120x2160", DISP_MODE_TYPE_DRIVER, 396000, 5120, 7116,
           7204, 7500, 0, 2160, 2168, 2178, 2200, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 121, },
    /* 122 - 5120x2160@25Hz 64:27 */
    { DISP_MODE("5120x2160", DISP_MODE_TYPE_DRIVER, 396000, 5120, 6816,
           6904, 7200, 0, 2160, 2168, 2178, 2200, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 122, },
    /* 123 - 5120x2160@30Hz 64:27 */
    { DISP_MODE("5120x2160", DISP_MODE_TYPE_DRIVER, 396000, 5120, 5784,
           5872, 6000, 0, 2160, 2168, 2178, 2200, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 123, },
    /* 124 - 5120x2160@48Hz 64:27 */
    { DISP_MODE("5120x2160", DISP_MODE_TYPE_DRIVER, 742500, 5120, 5866,
           5954, 6250, 0, 2160, 2168, 2178, 2475, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 124, },
    /* 125 - 5120x2160@50Hz 64:27 */
    { DISP_MODE("5120x2160", DISP_MODE_TYPE_DRIVER, 742500, 5120, 6216,
           6304, 6600, 0, 2160, 2168, 2178, 2250, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 125, },
    /* 126 - 5120x2160@60Hz 64:27 */
    { DISP_MODE("5120x2160", DISP_MODE_TYPE_DRIVER, 742500, 5120, 5284,
           5372, 5500, 0, 2160, 2168, 2178, 2250, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 126, },
    /* 127 - 5120x2160@100Hz 64:27 */
    { DISP_MODE("5120x2160", DISP_MODE_TYPE_DRIVER, 1485000, 5120, 6216,
           6304, 6600, 0, 2160, 2168, 2178, 2250, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 127, },
};

/*
 * From CEA/CTA-861 spec.
 *
 * Do not access directly, instead always use cea_mode_for_vic().
 */
static disp_display_mode_t edid_cea_modes_193[] = 
{
    /* 193 - 5120x2160@120Hz 64:27 */
    { DISP_MODE("5120x2160", DISP_MODE_TYPE_DRIVER, 1485000, 5120, 5284,
           5372, 5500, 0, 2160, 2168, 2178, 2250, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 193, },
    /* 194 - 7680x4320@24Hz 16:9 */
    { DISP_MODE("7680x4320", DISP_MODE_TYPE_DRIVER, 1188000, 7680, 10232,
           10408, 11000, 0, 4320, 4336, 4356, 4500, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 194, },
    /* 195 - 7680x4320@25Hz 16:9 */
    { DISP_MODE("7680x4320", DISP_MODE_TYPE_DRIVER, 1188000, 7680, 10032,
           10208, 10800, 0, 4320, 4336, 4356, 4400, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 195, },
    /* 196 - 7680x4320@30Hz 16:9 */
    { DISP_MODE("7680x4320", DISP_MODE_TYPE_DRIVER, 1188000, 7680, 8232,
           8408, 9000, 0, 4320, 4336, 4356, 4400, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 196, },
    /* 197 - 7680x4320@48Hz 16:9 */
    { DISP_MODE("7680x4320", DISP_MODE_TYPE_DRIVER, 2376000, 7680, 10232,
           10408, 11000, 0, 4320, 4336, 4356, 4500, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 197, },
    /* 198 - 7680x4320@50Hz 16:9 */
    { DISP_MODE("7680x4320", DISP_MODE_TYPE_DRIVER, 2376000, 7680, 10032,
           10208, 10800, 0, 4320, 4336, 4356, 4400, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 198, },
    /* 199 - 7680x4320@60Hz 16:9 */
    { DISP_MODE("7680x4320", DISP_MODE_TYPE_DRIVER, 2376000, 7680, 8232,
           8408, 9000, 0, 4320, 4336, 4356, 4400, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 199, },
    /* 200 - 7680x4320@100Hz 16:9 */
    { DISP_MODE("7680x4320", DISP_MODE_TYPE_DRIVER, 4752000, 7680, 9792,
           9968, 10560, 0, 4320, 4336, 4356, 4500, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 200, },
    /* 201 - 7680x4320@120Hz 16:9 */
    { DISP_MODE("7680x4320", DISP_MODE_TYPE_DRIVER, 4752000, 7680, 8032,
           8208, 8800, 0, 4320, 4336, 4356, 4500, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .vic = 201, },
    /* 202 - 7680x4320@24Hz 64:27 */
    { DISP_MODE("7680x4320", DISP_MODE_TYPE_DRIVER, 1188000, 7680, 10232,
           10408, 11000, 0, 4320, 4336, 4356, 4500, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 202, },
    /* 203 - 7680x4320@25Hz 64:27 */
    { DISP_MODE("7680x4320", DISP_MODE_TYPE_DRIVER, 1188000, 7680, 10032,
           10208, 10800, 0, 4320, 4336, 4356, 4400, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 203, },
    /* 204 - 7680x4320@30Hz 64:27 */
    { DISP_MODE("7680x4320", DISP_MODE_TYPE_DRIVER, 1188000, 7680, 8232,
           8408, 9000, 0, 4320, 4336, 4356, 4400, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 204, },
    /* 205 - 7680x4320@48Hz 64:27 */
    { DISP_MODE("7680x4320", DISP_MODE_TYPE_DRIVER, 2376000, 7680, 10232,
           10408, 11000, 0, 4320, 4336, 4356, 4500, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 205, },
    /* 206 - 7680x4320@50Hz 64:27 */
    { DISP_MODE("7680x4320", DISP_MODE_TYPE_DRIVER, 2376000, 7680, 10032,
           10208, 10800, 0, 4320, 4336, 4356, 4400, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 206, },
    /* 207 - 7680x4320@60Hz 64:27 */
    { DISP_MODE("7680x4320", DISP_MODE_TYPE_DRIVER, 2376000, 7680, 8232,
           8408, 9000, 0, 4320, 4336, 4356, 4400, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 207, },
    /* 208 - 7680x4320@100Hz 64:27 */
    { DISP_MODE("7680x4320", DISP_MODE_TYPE_DRIVER, 4752000, 7680, 9792,
           9968, 10560, 0, 4320, 4336, 4356, 4500, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 208, },
    /* 209 - 7680x4320@120Hz 64:27 */
    { DISP_MODE("7680x4320", DISP_MODE_TYPE_DRIVER, 4752000, 7680, 8032,
           8208, 8800, 0, 4320, 4336, 4356, 4500, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 209, },
    /* 210 - 10240x4320@24Hz 64:27 */
    { DISP_MODE("10240x4320", DISP_MODE_TYPE_DRIVER, 1485000, 10240, 11732,
           11908, 12500, 0, 4320, 4336, 4356, 4950, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 210, },
    /* 211 - 10240x4320@25Hz 64:27 */
    { DISP_MODE("10240x4320", DISP_MODE_TYPE_DRIVER, 1485000, 10240, 12732,
           12908, 13500, 0, 4320, 4336, 4356, 4400, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 211, },
    /* 212 - 10240x4320@30Hz 64:27 */
    { DISP_MODE("10240x4320", DISP_MODE_TYPE_DRIVER, 1485000, 10240, 10528,
           10704, 11000, 0, 4320, 4336, 4356, 4500, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 212, },
    /* 213 - 10240x4320@48Hz 64:27 */
    { DISP_MODE("10240x4320", DISP_MODE_TYPE_DRIVER, 2970000, 10240, 11732,
           11908, 12500, 0, 4320, 4336, 4356, 4950, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 213, },
    /* 214 - 10240x4320@50Hz 64:27 */
    { DISP_MODE("10240x4320", DISP_MODE_TYPE_DRIVER, 2970000, 10240, 12732,
           12908, 13500, 0, 4320, 4336, 4356, 4400, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 214, },
    /* 215 - 10240x4320@60Hz 64:27 */
    { DISP_MODE("10240x4320", DISP_MODE_TYPE_DRIVER, 2970000, 10240, 10528,
           10704, 11000, 0, 4320, 4336, 4356, 4500, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 215, },
    /* 216 - 10240x4320@100Hz 64:27 */
    { DISP_MODE("10240x4320", DISP_MODE_TYPE_DRIVER, 5940000, 10240, 12432,
           12608, 13200, 0, 4320, 4336, 4356, 4500, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 216, },
    /* 217 - 10240x4320@120Hz 64:27 */
    { DISP_MODE("10240x4320", DISP_MODE_TYPE_DRIVER, 5940000, 10240, 10528,
           10704, 11000, 0, 4320, 4336, 4356, 4500, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, .vic = 217, },
    /* 218 - 4096x2160@100Hz 256:135 */
    { DISP_MODE("4096x2160", DISP_MODE_TYPE_DRIVER, 1188000, 4096, 4896,
           4984, 5280, 0, 2160, 2168, 2178, 2250, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_256_135, .vic = 218, },
    /* 219 - 4096x2160@120Hz 256:135 */
    { DISP_MODE("4096x2160", DISP_MODE_TYPE_DRIVER, 1188000, 4096, 4184,
           4272, 4400, 0, 2160, 2168, 2178, 2250, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_256_135, .vic = 219, },
};

/*
 * HDMI 1.4 4k modes. Index using the VIC.
 */
static disp_display_mode_t edid_4k_modes[] = 
{
    /* 0 - dummy, VICs start at 1 */
    { 0},
    /* 1 - 3840x2160@30Hz */
    { DISP_MODE("3840x2160", DISP_MODE_TYPE_DRIVER, 297000,
           3840, 4016, 4104, 4400, 0,
           2160, 2168, 2178, 2250, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .hdmi_vic = 1 },
    /* 2 - 3840x2160@25Hz */
    { DISP_MODE("3840x2160", DISP_MODE_TYPE_DRIVER, 297000,
           3840, 4896, 4984, 5280, 0,
           2160, 2168, 2178, 2250, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .hdmi_vic = 2 },
    /* 3 - 3840x2160@24Hz */
    { DISP_MODE("3840x2160", DISP_MODE_TYPE_DRIVER, 297000,
           3840, 5116, 5204, 5500, 0,
           2160, 2168, 2178, 2250, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, .hdmi_vic = 3 },
    /* 4 - 4096x2160@24Hz (SMPTE) */
    { DISP_MODE("4096x2160", DISP_MODE_TYPE_DRIVER, 297000,
           4096, 5116, 5204, 5500, 0,
           2160, 2168, 2178, 2250, 0,
           DISP_MODE_FLAG_PHSYNC | DISP_MODE_FLAG_PVSYNC),
      .picture_aspect_ratio = HDMI_PICTURE_ASPECT_256_135, .hdmi_vic = 4 },
};

/*** DDC fetch and block validation ***/

static disp_u8_t edid_header[] = 
{
    0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00
};

/**
 * disp_edid_header_is_valid - sanity check the header of the base EDID block
 * @raw_edid: pointer to raw base EDID block
 *
 * Sanity check the header of the base EDID block.
 *
 * Return: 8 if the header is perfect, down to 0 if it's totally wrong.
 */
disp_s32_t disp_edid_header_is_valid(disp_u8_t *raw_edid)
{
    disp_s32_t i, score = 0;

    for(i = 0; i < sizeof(edid_header); i++)
        if(raw_edid[i] == edid_header[i])
            score++;

    return score;
}

disp_u8_t disp_edid_block_checksum(disp_u8_t *raw_edid)
{
    disp_s32_t i;
    disp_u8_t csum = 0, crc = 0;

    for(i = 0; i < EDID_LENGTH - 1; i++)
        csum += raw_edid[i];

    crc = (disp_u8_t)(0x100 - csum);

    return crc;
}

static disp_u8_t disp_edid_block_checksum_diff(disp_u8_t *raw_edid, disp_u8_t real_checksum)
{
    if(raw_edid[EDID_LENGTH - 1] != real_checksum)
        return DISP_TRUE;
    else
        return DISP_FALSE;
}

#if 0 // not use yet
static disp_u8_t disp_edid_is_zero(disp_u8_t *in_edid, disp_s32_t length)
{
    //refine later
    //if(memchr_inv(in_edid, 0, length))
        return DISP_FALSE;

    return DISP_TRUE;
}
#endif

/**
 * disp_edid_are_equal - compare two edid blobs.
 * @edid1: pointer to first blob
 * @edid2: pointer to second blob
 * This helper can be used during probing to determine if
 * edid had changed.
 */
disp_u8_t disp_edid_are_equal(pedid_t edid1, pedid_t edid2)
{
    disp_s32_t edid1_len, edid2_len;
    disp_u8_t edid1_present = edid1 != DISP_NULL;
    disp_u8_t edid2_present = edid2 != DISP_NULL;

    if(edid1_present != edid2_present)
        return DISP_FALSE;

    if(edid1) 
    {
        edid1_len = EDID_LENGTH * (1 + edid1->extensions);
        edid2_len = EDID_LENGTH * (1 + edid2->extensions);

        if(edid1_len != edid2_len)
            return DISP_FALSE;

        if(disp_memcmp(edid1, edid2, edid1_len))
            return DISP_FALSE;
    }

    return DISP_TRUE;
}

disp_u8_t disp_edid_block_valid(disp_u8_t *raw_edid, disp_u32_t block)
{
    disp_u8_t csum;
    pedid_t edid = (pedid_t )raw_edid;

    if(!raw_edid)
        return DISP_FALSE;

    if(block == 0) 
    {
        disp_s32_t score = disp_edid_header_is_valid(raw_edid);

        if(score != 8) 
            goto bad;
    }

    csum = disp_edid_block_checksum(raw_edid);
    if(disp_edid_block_checksum_diff(raw_edid, csum))
    {
    
        /* allow CEA to slide through, switches mangle this */
        if(raw_edid[0] == CEA_EXT) 
        {
            disp_print(DDM_TRACE, "EDID checksum is invalid, remainder is %d\n", csum);
            disp_print(DDM_TRACE, "Assuming a KVM switch modified the CEA block but left the original checksum\n");
        }
        else
            goto bad;
    }

    /* per-block-type checks */
    switch(raw_edid[0]) {
    case 0: /* base */
        if(edid->version != 1) 
        {
            disp_print(DDM_TRACE, "EDID has major version %d, instead of 1\n", edid->version);
            goto bad;
        }

        if(edid->revision > 4)
            disp_print(DDM_TRACE, "EDID minor > 4, assuming backward compatibility\n");
        break;

    default:
        break;
    }

    return DISP_TRUE;

bad:
    return DISP_FALSE;
}

disp_u8_t disp_edid_is_valid(pedid_t edid)
{
    disp_s32_t i;
    disp_u8_t *raw = (disp_u8_t *)edid;

    if(!edid)
        return DISP_FALSE;

    for(i = 0; i <= edid->extensions; i++){
        if(!disp_edid_block_valid(raw + i * EDID_LENGTH, i))
            return DISP_FALSE;}

    return DISP_TRUE;
}

/*** EDID parsing ***/

/**
 * edid_vendor - match a string against EDID's obfuscated vendor field
 * @edid: EDID to match
 * @vendor: vendor string
 *
 * Returns true if @vendor is in @edid, false otherwise
 */
static disp_u8_t edid_vendor(pedid_t edid, disp_u8_t *vendor)
{
    disp_u8_t edid_vendor[3];

    edid_vendor[0] = ((edid->mfg_id[0] & 0x7c) >> 2) + '@';
    edid_vendor[1] = (((edid->mfg_id[0] & 0x3) << 3) |
              ((edid->mfg_id[1] & 0xe0) >> 5)) + '@';
    edid_vendor[2] = (edid->mfg_id[1] & 0x1f) + '@';

    return !disp_strncmp(edid_vendor, vendor, 3);
}

/**
 * edid_get_quirks - return quirk flags for a given EDID
 * @edid: EDID to process
 *
 * This tells subsequent routines what fixes they need to apply.
 */
static disp_u32_t edid_get_quirks(pedid_t edid)
{
    struct edid_quirk *quirk;
    disp_s32_t i;

    for(i = 0; i < ARRAY_SIZE(edid_quirk_list); i++) 
    {
        quirk = &edid_quirk_list[i];

        if(edid_vendor(edid, quirk->vendor) &&
            (EDID_PRODUCT_ID(edid) == quirk->product_id))
            return quirk->quirks;
    }

    return 0;
}

#define MODE_SIZE(m) ((m)->hdisplay * (m)->vdisplay)
#define MODE_REFRESH_DIFF(c,t) (abs((c) - (t)))

static disp_u8_t
mode_is_rb(pdisp_display_mode_t mode)
{
    return (mode->htotal - mode->hdisplay == 160) &&
           (mode->hsync_end - mode->hdisplay == 80) &&
           (mode->hsync_end - mode->hsync_start == 32) &&
           (mode->vsync_start - mode->vdisplay == 3);
}

/*
 * disp_mode_find_dmt - Create a copy of a mode if present in DMT
 * @dev: Device to duplicate against
 * @hsize: Mode width
 * @vsize: Mode height
 * @fresh: Mode refresh rate
 * @rb: Mode reduced-blanking-ness
 *
 * Walk the DMT mode list looking for a match for the given parameters.
 *
 * Return: A newly allocated copy of the mode, or NULL if not found.
 */
disp_u8_t disp_mode_find_dmt(disp_s32_t hsize, disp_s32_t vsize, disp_s32_t fresh, disp_u8_t rb, pdisp_display_mode_t pmode)
{
    disp_s32_t i;

    for(i = 0; i < ARRAY_SIZE(disp_dmt_modes); i++) 
    {
        pdisp_display_mode_t ptr = &disp_dmt_modes[i];

        if(hsize != ptr->hdisplay)
            continue;
        if(vsize != ptr->vdisplay)
            continue;
        if(fresh != disp_mode_vrefresh(ptr))
            continue;
        if(rb != mode_is_rb(ptr))
            continue;

        disp_memcpy(pmode, ptr, sizeof(disp_display_mode_t));
        return DISP_TRUE;
    }

    return DISP_FALSE;
}

static disp_u8_t is_display_descriptor(disp_u8_t d[18], disp_u8_t tag)
{
    return d[0] == 0x00 && d[1] == 0x00 &&
        d[2] == 0x00 && d[3] == tag;
}

static disp_u8_t is_detailed_timing_descriptor(disp_u8_t d[18])
{
    return d[0] != 0x00 || d[1] != 0x00;
}

typedef disp_void_t detailed_cb(pdetailed_timing_t timing, disp_void_t *closure);

static disp_void_t
cea_for_each_detailed_block(disp_u8_t *ext, detailed_cb *cb, disp_void_t *closure)
{
    disp_s32_t i, n;
    disp_u8_t d = ext[0x02];
    disp_u8_t *det_base = ext + d;

    if(d < 4 || d > 127)
        return;

    n = (127 - d) / 18;
    for(i = 0; i < n; i++)
        cb((pdetailed_timing_t)(det_base + 18 * i), closure);
}

static disp_void_t
vtb_for_each_detailed_block(disp_u8_t *ext, detailed_cb *cb, disp_void_t *closure)
{
    disp_u32_t i, n = DISP_MIN((disp_s32_t)ext[0x02], 6);
    disp_u8_t *det_base = ext + 5;

    if(ext[0x01] != 1)
        return; /* unknown version */

    for(i = 0; i < n; i++)
        cb((pdetailed_timing_t)(det_base + 18 * i), closure);
}

static disp_void_t
disp_for_each_detailed_block(disp_u8_t *raw_edid, detailed_cb *cb, disp_void_t *closure)
{
    disp_s32_t i;
    pedid_t edid = (pedid_t )raw_edid;

    if(edid == DISP_NULL)
        return;

    for(i = 0; i < EDID_DETAILED_TIMINGS; i++)
        cb(&(edid->detailed_timings[i]), closure);

    for(i = 1; i <= raw_edid[0x7e]; i++) 
    {
        disp_u8_t *ext = raw_edid + (i * EDID_LENGTH);

        switch(*ext) {
        case CEA_EXT:
            cea_for_each_detailed_block(ext, cb, closure);
            break;
        case VTB_EXT:
            vtb_for_each_detailed_block(ext, cb, closure);
            break;
        default:
            break;
        }
    }
}

static disp_void_t
is_rb(pdetailed_timing_t t, disp_void_t *data)
{
    disp_u8_t *r = (disp_u8_t *)t;

    if(!is_display_descriptor(r, EDID_DETAIL_MONITOR_RANGE))
        return;

    if(r[15] & 0x10)
        *(disp_u8_t *)data = DISP_TRUE;
}

/* EDID 1.4 defines this explicitly.  For EDID 1.3, we guess, badly. */
static disp_u8_t
disp_monitor_supports_rb(pedid_t edid)
{
    if(edid->revision >= 4) 
    {
        disp_u8_t ret = DISP_FALSE;

        disp_for_each_detailed_block((disp_u8_t *)edid, is_rb, &ret);
        return ret;
    }

    return ((edid->input & DISP_EDID_INPUT_DIGITAL) != 0);
}

static disp_void_t
find_gtf2(pdetailed_timing_t t, disp_void_t *data)
{
    disp_u8_t *r = (disp_u8_t *)t;

    if(!is_display_descriptor(r, EDID_DETAIL_MONITOR_RANGE))
        return;

    if(r[10] == 0x02)
        *(disp_u8_t **)data = r;
}

/* Secondary GTF curve kicks in above some break frequency */
static disp_s32_t
disp_gtf2_hbreak(pedid_t edid)
{
    disp_u8_t *r = DISP_NULL;

    disp_for_each_detailed_block((disp_u8_t *)edid, find_gtf2, &r);
    return r ? (r[12] * 2) : 0;
}

static disp_s32_t
disp_gtf2_2c(pedid_t edid)
{
    disp_u8_t *r = DISP_NULL;

    disp_for_each_detailed_block((disp_u8_t *)edid, find_gtf2, &r);
    return r ? r[13] : 0;
}

static disp_s32_t
disp_gtf2_m(pedid_t edid)
{
    disp_u8_t *r = DISP_NULL;

    disp_for_each_detailed_block((disp_u8_t *)edid, find_gtf2, &r);
    return r ? (r[15] << 8) + r[14] : 0;
}

static disp_s32_t
disp_gtf2_k(pedid_t edid)
{
    disp_u8_t *r = DISP_NULL;

    disp_for_each_detailed_block((disp_u8_t *)edid, find_gtf2, &r);
    return r ? r[16] : 0;
}

static disp_s32_t
disp_gtf2_2j(pedid_t edid)
{
    disp_u8_t *r = DISP_NULL;

    disp_for_each_detailed_block((disp_u8_t *)edid, find_gtf2, &r);
    return r ? r[17] : 0;
}

/**
 * standard_timing_level - get std. timing level(CVT/GTF/DMT)
 * @edid: EDID block to scan
 */
static disp_s32_t standard_timing_level(pedid_t edid)
{
    if(edid->revision >= 2) 
    {
        if(edid->revision >= 4 && (edid->features & DISP_EDID_FEATURE_DEFAULT_GTF))
            return LEVEL_CVT;
        if(disp_gtf2_hbreak(edid))
            return LEVEL_GTF2;
        if(edid->features & DISP_EDID_FEATURE_DEFAULT_GTF)
            return LEVEL_GTF;
    }
    return LEVEL_DMT;
}

/*
 * 0 is reserved.  The spec says 0x01 fill for unused timings.  Some old
 * monitors fill with ascii space (0x20) instead.
 */
static disp_s32_t
bad_std_timing(disp_u8_t a, disp_u8_t b)
{
    return (a == 0x00 && b == 0x00) ||
           (a == 0x01 && b == 0x01) ||
           (a == 0x20 && b == 0x20);
}

static disp_s32_t disp_mode_hsync(pdisp_display_mode_t mode)
{
    if(mode->htotal <= 0)
        return 0;

    return (disp_s32_t)DIV_ROUND_CLOSEST(mode->clock, mode->htotal);
}

/**
 * disp_mode_std - convert standard mode info (width, height, refresh) into mode
 * @connector: connector of for the EDID block
 * @edid: EDID block to scan
 * @t: standard timing params
 *
 * Take the standard timing params (in this case width, aspect, and refresh)
 * and convert them into a real mode using CVT/GTF/DMT.
 */
static disp_u8_t disp_mode_std(pedid_t edid, pstd_timing_t pt, pdisp_display_mode_t mode)
{
    disp_s32_t hsize, vsize;
    disp_s32_t vrefresh_rate;
    disp_u32_t aspect_ratio = (pt->vfreq_aspect & EDID_TIMING_ASPECT_MASK)
        >> EDID_TIMING_ASPECT_SHIFT;
    disp_u32_t vfreq = (pt->vfreq_aspect & EDID_TIMING_VFREQ_MASK)
        >> EDID_TIMING_VFREQ_SHIFT;
    disp_s32_t timing_level = standard_timing_level(edid);
    disp_u8_t ret = DISP_FALSE;

    if(bad_std_timing(pt->hsize, pt->vfreq_aspect))
        return DISP_FALSE;

    /* According to the EDID spec, the hdisplay = hsize * 8 + 248 */
    hsize = pt->hsize * 8 + 248;
    /* vrefresh_rate = vfreq + 60 */
    vrefresh_rate = vfreq + 60;
    /* the vdisplay is calculated based on the aspect ratio */
    if(aspect_ratio == 0) 
    {
        if(edid->revision < 3)
            vsize = hsize;
        else
            vsize = (hsize * 10) / 16;
    } 
    else if(aspect_ratio == 1)
        vsize = (hsize * 3) / 4;
    else if(aspect_ratio == 2)
        vsize = (hsize * 4) / 5;
    else
        vsize = (hsize * 9) / 16;

    /* HDTV hack, part 1 */
    if(vrefresh_rate == 60 &&
        ((hsize == 1360 && vsize == 765) ||
         (hsize == 1368 && vsize == 769))) {
        hsize = 1366;
        vsize = 768;
    }

//refine later
    /*
     * If this connector already has a mode for this size and refresh
     * rate (because it came from detailed or CVT info), use that
     * instead.  This way we don't have to guess at interlace or
     * reduced blanking.
     */
    //list_for_each_entry(m, &connector->probed_modes, head)
    //    if(m->hdisplay == hsize && m->vdisplay == vsize &&
    //        disp_mode_vrefresh(m) == vrefresh_rate)
    //        return DISP_FALSE;

    /* HDTV hack, part 2 */
    if(hsize == 1366 && vsize == 768 && vrefresh_rate == 60) 
    {
        if(!disp_cvt_mode(1366, 768, vrefresh_rate, 0, 0, DISP_FALSE, mode))
            return DISP_FALSE;
        mode->hdisplay = 1366;
        mode->hsync_start = mode->hsync_start - 1;
        mode->hsync_end = mode->hsync_end - 1;
        return DISP_TRUE;
    }

    /* check whether it can be found in default mode table */
    if(disp_monitor_supports_rb(edid)) 
    {
        ret = disp_mode_find_dmt(hsize, vsize, vrefresh_rate, DISP_TRUE, mode);
        if(ret)
            return DISP_TRUE;
    }
    ret = disp_mode_find_dmt(hsize, vsize, vrefresh_rate, DISP_FALSE, mode);
    if(ret)
        return DISP_TRUE;

    /* okay, generate it */
    switch(timing_level) 
    {
    case LEVEL_DMT:
        break;
    case LEVEL_GTF:
        ret = disp_gtf_mode(hsize, vsize, vrefresh_rate, 0, 0, mode);
        break;
    case LEVEL_GTF2:
        /*
         * This is potentially wrong if there's ever a monitor with
         * more than one ranges section, each claiming a different
         * secondary GTF curve.  Please don't do that.
         */
        ret = disp_gtf_mode(hsize, vsize, vrefresh_rate, 0, 0, mode);
        if(!ret)
            return DISP_FALSE;
        if(disp_mode_hsync(mode) > disp_gtf2_hbreak(edid))
        {
            ret = disp_gtf_mode_complex(hsize, vsize, vrefresh_rate, 0, 0, disp_gtf2_m(edid),
                disp_gtf2_2c(edid), disp_gtf2_k(edid), disp_gtf2_2j(edid), mode);
        }
        break;
    case LEVEL_CVT:
        ret = disp_cvt_mode(hsize, vsize, vrefresh_rate, 0, 0, DISP_FALSE, mode);
        break;
    }
    return ret;
}

/*
 * EDID is delightfully ambiguous about how interlaced modes are to be
 * encoded.  Our internal representation is of frame height, but some
 * HDTV detailed timings are encoded as field height.
 *
 * The format list here is from CEA, in frame size.  Technically we
 * should be checking refresh rate too.  Whatever.
 */
static disp_void_t
disp_mode_do_interlace_quirk(pdisp_display_mode_t mode, pdetailed_pixel_timing_t pt)
{
    disp_s32_t i;
    static struct {
        disp_s32_t w, h;
    } cea_interlaced[] = {
        { 1920, 1080 },
        {  720,  480 },
        { 1440,  480 },
        { 2880,  480 },
        {  720,  576 },
        { 1440,  576 },
        { 2880,  576 },
    };

    if(!(pt->misc & DISP_EDID_PT_INTERLACED))
        return;

    for(i = 0; i < ARRAY_SIZE(cea_interlaced); i++) 
    {
        if((mode->hdisplay == cea_interlaced[i].w) &&
            (mode->vdisplay == cea_interlaced[i].h / 2)) {
            mode->vdisplay *= 2;
            mode->vsync_start *= 2;
            mode->vsync_end *= 2;
            mode->vtotal *= 2;
            mode->vtotal |= 1;
        }
    }

    mode->flags |= DISP_MODE_FLAG_INTERLACE;
}

/**
 * disp_mode_detailed - create a new mode from an EDID detailed timing section
 * @edid: EDID block
 * @timing: EDID detailed timing info
 * @quirks: quirks to apply
 *
 * An EDID detailed timing block contains enough info for us to create and
 * return a new disp_display_mode_t.
 */
static disp_u8_t disp_mode_detailed(
                          pedid_t edid,
                          pdetailed_timing_t timing,
                          disp_u32_t quirks, pdisp_display_mode_t pmode)
{
    pdetailed_pixel_timing_t pt = &timing->data.pixel_data;
    disp_u32_t hactive = (pt->hactive_hblank_hi & 0xf0) << 4 | pt->hactive_lo;
    disp_u32_t vactive = (pt->vactive_vblank_hi & 0xf0) << 4 | pt->vactive_lo;
    disp_u32_t hblank = (pt->hactive_hblank_hi & 0xf) << 8 | pt->hblank_lo;
    disp_u32_t vblank = (pt->vactive_vblank_hi & 0xf) << 8 | pt->vblank_lo;
    disp_u32_t hsync_offset = (pt->hsync_vsync_offset_pulse_width_hi & 0xc0) << 2 | pt->hsync_offset_lo;
    disp_u32_t hsync_pulse_width = (pt->hsync_vsync_offset_pulse_width_hi & 0x30) << 4 | pt->hsync_pulse_width_lo;
    disp_u32_t vsync_offset = (pt->hsync_vsync_offset_pulse_width_hi & 0xc) << 2 | pt->vsync_offset_pulse_width_lo >> 4;
    disp_u32_t vsync_pulse_width = (pt->hsync_vsync_offset_pulse_width_hi & 0x3) << 4 | (pt->vsync_offset_pulse_width_lo & 0xf);

    /* ignore tiny modes */
    if(hactive < 64 || vactive < 64)
        return DISP_FALSE;

    if(pt->misc & DISP_EDID_PT_STEREO)
    {
        disp_print(DDM_INFO, "stereo mode not supported\n");
        return DISP_FALSE;
    }
    if(!(pt->misc & DISP_EDID_PT_SEPARATE_SYNC)) 
    {
        disp_print(DDM_INFO, "composite sync not supported\n");
    }

    /* it is incorrect if hsync/vsync width is zero */
    if(!hsync_pulse_width || !vsync_pulse_width) 
    {
        disp_print(DDM_WARNING, "Incorrect Detailed timing. "
                "Wrong Hsync/Vsync pulse width\n");
        return DISP_FALSE;
    }

    if(quirks & EDID_QUIRK_FORCE_REDUCED_BLANKING) 
    {
        if(!disp_cvt_mode(hactive, vactive, 60, DISP_TRUE, DISP_FALSE, DISP_FALSE, pmode))
            return DISP_FALSE;

        goto set_size;

    }

    disp_memset(pmode, 0, sizeof(disp_display_mode_t));
    pmode->clock = (timing->pixel_clock_low + (timing->pixel_clock_high << 8)) * 10;

    pmode->hdisplay = (disp_u16_t)hactive;
    pmode->hsync_start = pmode->hdisplay + (disp_u16_t)hsync_offset;
    pmode->hsync_end = pmode->hsync_start + (disp_u16_t)hsync_pulse_width;
    pmode->htotal = pmode->hdisplay + (disp_u16_t)hblank;

    pmode->vdisplay = (disp_u16_t)vactive;
    pmode->vsync_start = pmode->vdisplay + (disp_u16_t)vsync_offset;
    pmode->vsync_end = pmode->vsync_start + (disp_u16_t)vsync_pulse_width;
    pmode->vtotal = pmode->vdisplay + (disp_u16_t)vblank;

    /* Some EDIDs have bogus h/vtotal values */
    if(pmode->hsync_end > pmode->htotal)
        pmode->htotal = pmode->hsync_end + 1;
    if(pmode->vsync_end > pmode->vtotal)
        pmode->vtotal = pmode->vsync_end + 1;

    disp_mode_do_interlace_quirk(pmode, pt);

    if(quirks & EDID_QUIRK_DETAILED_SYNC_PP)
        pt->misc |= DISP_EDID_PT_HSYNC_POSITIVE | DISP_EDID_PT_VSYNC_POSITIVE;

    pmode->flags |= (pt->misc & DISP_EDID_PT_HSYNC_POSITIVE) ?
        DISP_MODE_FLAG_PHSYNC : DISP_MODE_FLAG_NHSYNC;
    pmode->flags |= (pt->misc & DISP_EDID_PT_VSYNC_POSITIVE) ?
        DISP_MODE_FLAG_PVSYNC : DISP_MODE_FLAG_NVSYNC;

set_size:
    pmode->width_mm = pt->width_mm_lo | (pt->width_height_mm_hi & 0xf0) << 4;
    pmode->height_mm = pt->height_mm_lo | (pt->width_height_mm_hi & 0xf) << 8;

    if(quirks & EDID_QUIRK_DETAILED_IN_CM) 
    {
        pmode->width_mm *= 10;
        pmode->height_mm *= 10;
    }

    if(quirks & EDID_QUIRK_DETAILED_USE_MAXIMUM_SIZE) 
    {
        pmode->width_mm = edid->width_cm * 10;
        pmode->height_mm = edid->height_cm * 10;
    }

    pmode->type = DISP_MODE_TYPE_DRIVER;
    disp_mode_set_name(pmode);

    return DISP_TRUE;
}

static disp_u8_t
mode_in_hsync_range(pdisp_display_mode_t mode,
            pedid_t edid, disp_u8_t *t)
{
    disp_s32_t hsync, hmin, hmax;

    hmin = t[7];
    if(edid->revision >= 4)
        hmin += ((t[4] & 0x04) ? 255 : 0);
    hmax = t[8];
    if(edid->revision >= 4)
        hmax += ((t[4] & 0x08) ? 255 : 0);
    hsync = disp_mode_hsync(mode);

    /* AOC U28G2X_D hmax/hmax both 0xff, no mode could be added.
     * But these modes could light on, if ignore hsync limit.
     * Should we add these mode when hmax/hmax 0xff ?
     */
    //if (hmin == hmax)
    //    hmin = 0;

    return (hsync <= hmax && hsync >= hmin);
}

static disp_u8_t
mode_in_vsync_range(pdisp_display_mode_t mode,
            pedid_t edid, disp_u8_t *t)
{
    disp_s32_t vsync, vmin, vmax;

    vmin = t[5];
    if(edid->revision >= 4)
        vmin += ((t[4] & 0x01) ? 255 : 0);
    vmax = t[6];
    if(edid->revision >= 4)
        vmax += ((t[4] & 0x02) ? 255 : 0);
    vsync = disp_mode_vrefresh(mode);

    return (vsync <= vmax && vsync >= vmin);
}

static disp_s32_t
range_pixel_clock(pedid_t edid, disp_u8_t *t)
{
    /* unspecified */
    if(t[9] == 0 || t[9] == 255)
        return 0;

    /* 1.4 with CVT support gives us real precision, yay */
    if(edid->revision >= 4 && t[10] == 0x04)
        return (t[9] * 10000) - ((t[12] >> 2) * 250);

    /* 1.3 is pathetic, so fuzz up a bit */
    return t[9] * 10000 + 5001;
}

static disp_u8_t
mode_in_range(pdisp_display_mode_t mode, pedid_t edid,
          pdetailed_timing_t timing)
{
    disp_s32_t max_clock;
    disp_u8_t *t = (disp_u8_t *)timing;

    if(!mode_in_hsync_range(mode, edid, t))
        return DISP_FALSE;

    if(!mode_in_vsync_range(mode, edid, t))
        return DISP_FALSE;

    max_clock = range_pixel_clock(edid, t);
    if(max_clock && mode->clock > max_clock)
        return DISP_FALSE;

    /* 1.4 max horizontal check */
    if(edid->revision >= 4 && t[10] == 0x04)
        if(t[13] && mode->hdisplay > 8 * (t[13] + (256 * (t[12]&0x3))))
            return DISP_FALSE;

    if(mode_is_rb(mode) && !disp_monitor_supports_rb(edid))
        return DISP_FALSE;

    return DISP_TRUE;
}

static disp_u8_t valid_inferred_mode(pdisp_connector_t pdisp_connector,
                pdisp_display_mode_t mode)
{
    pdisp_display_mode_t m;
    disp_u8_t ok = DISP_FALSE;
    disp_s32_t i;

    for(i = 0; i < pdisp_connector->mode_list.num; i++)
    {
        m = &pdisp_connector->mode_list.modes[i];
        if(mode->hdisplay == m->hdisplay &&
            mode->vdisplay == m->vdisplay &&
            disp_mode_vrefresh(mode) == disp_mode_vrefresh(m))
            return DISP_FALSE; /* duplicated */
        if(mode->hdisplay <= m->hdisplay &&
            mode->vdisplay <= m->vdisplay)
            ok = DISP_TRUE;
    }
    return ok;
}

static disp_s32_t
disp_dmt_modes_for_range(pdisp_connector_t pdisp_connector, pedid_t edid,
            pdetailed_timing_t timing)
{
    disp_s32_t i, modes = 0;

    for(i = 0; i < ARRAY_SIZE(disp_dmt_modes); i++) 
    {
        if(mode_in_range(disp_dmt_modes + i, edid, timing) &&
            valid_inferred_mode(pdisp_connector, disp_dmt_modes + i)) 
        {
            if(disp_mode_probed_add(pdisp_connector, &disp_dmt_modes[i], DISP_FALSE))
                modes++;
        }
    }

    return modes;
}

/* fix up 1366x768 mode from 1368x768;
 * GFT/CVT can't express 1366 width which isn't dividable by 8
 */
disp_void_t disp_mode_fixup_1366x768(pdisp_display_mode_t mode)
{
    if(mode->hdisplay == 1368 && mode->vdisplay == 768) 
    {
        mode->hdisplay = 1366;
        mode->hsync_start--;
        mode->hsync_end--;
        disp_mode_set_name(mode);
    }
}

static disp_s32_t disp_gtf_modes_for_range(pdisp_connector_t pdisp_connector, pedid_t edid, pdetailed_timing_t timing)
{
    disp_s32_t i, modes = 0;
    disp_display_mode_t newmode = {0};
    disp_u8_t ret = DISP_FALSE;

    for(i = 0; i < ARRAY_SIZE(extra_modes); i++) 
    {
        struct minimode *m = &extra_modes[i];

        ret = disp_gtf_mode(m->w, m->h, m->r, 0, 0, &newmode);
        if(!ret)
            return modes;

        disp_mode_fixup_1366x768(&newmode);
        if(!mode_in_range(&newmode, edid, timing) ||
            !valid_inferred_mode(pdisp_connector, &newmode)) 
        {
            continue;
        }

        if(disp_mode_probed_add(pdisp_connector, &newmode, DISP_FALSE))
            modes++;
    }

    return modes;
}

static disp_s32_t
disp_cvt_modes_for_range(pdisp_connector_t pdisp_connector, pedid_t edid,
            pdetailed_timing_t timing)
{
    disp_s32_t i, modes = 0;
    disp_display_mode_t newmode = {0};

    disp_u8_t rb = disp_monitor_supports_rb(edid);

    for(i = 0; i < ARRAY_SIZE(extra_modes); i++) 
    {
        struct minimode *m = &extra_modes[i];

        if (!disp_cvt_mode(m->w, m->h, m->r, rb, 0, 0, &newmode))
            return modes;

        disp_mode_fixup_1366x768(&newmode);
        if(!mode_in_range(&newmode, edid, timing) ||
            !valid_inferred_mode(pdisp_connector, &newmode)) 
        {
            //refine later
            //disp_mode_destroy(dev, newmode);
            continue;
        }
//refine later
        //if(disp_mode_probed_add(connector, newmode))
        modes++;
    }

    return modes;
}

static disp_void_t
do_inferred_modes(pdetailed_timing_t timing, disp_void_t *c)
{
    pdetailed_mode_closure_t pclosure = c;
    pdetailed_non_pixel_t pdata = &timing->data.other_data;
    pdetailed_data_monitor_range_t prange = &pdata->data.range;

    if(!is_display_descriptor((disp_u8_t *)timing, EDID_DETAIL_MONITOR_RANGE))
        return;

    pclosure->modes += disp_dmt_modes_for_range(pclosure->pdisp_connector,
                          pclosure->edid,
                          timing);

    if(!version_greater(pclosure->edid, 1, 1))
        return; /* GTF not defined yet */

    switch(prange->flags) 
    {
    case 0x02: /* secondary gtf, XXX could do more */
    case 0x00: /* default gtf */
        pclosure->modes += disp_gtf_modes_for_range(pclosure->pdisp_connector,
                              pclosure->edid,
                              timing);
        break;
    case 0x04: /* cvt, only in 1.4+ */
        if(!version_greater(pclosure->edid, 1, 3))
            break;

        pclosure->modes += disp_cvt_modes_for_range(pclosure->pdisp_connector,
                              pclosure->edid,
                              timing);
        break;
    case 0x01: /* just the ranges, no formula */
    default:
        break;
    }
}

static disp_s32_t
add_inferred_modes(pdisp_connector_t pdisp_connector, pedid_t edid)
{
    detailed_mode_closure_t closure = 
    {
        .pdisp_connector = pdisp_connector,
        .edid = edid,
    };

    if(version_greater(edid, 1, 0))
        disp_for_each_detailed_block((disp_u8_t *)edid, do_inferred_modes,
                        &closure);

    return closure.modes;
}

static disp_s32_t disp_est3_modes(pdisp_connector_t pdisp_connector, pdetailed_timing_t timing)
{
    disp_s32_t i, j, m, modes = 0;
    disp_display_mode_t mode = {0};
    disp_u8_t *est = ((disp_u8_t *)timing) + 6;

    for(i = 0; i < 6; i++) 
    {
        for(j = 7; j >= 0; j--) 
        {
            m = (i * 8) + (7 - j);
            if(m >= ARRAY_SIZE(est3_modes))
                break;
            if(est[i] & (1 << j)) 
            {
                if(disp_mode_find_dmt(est3_modes[m].w, est3_modes[m].h,
                    est3_modes[m].r, est3_modes[m].rb, &mode)) 
                {
                    if(disp_mode_probed_add(pdisp_connector, &mode, DISP_FALSE))
                        modes++;
                }
            }
        }
    }

    return modes;
}

static disp_void_t
do_established_modes(pdetailed_timing_t timing, disp_void_t *c)
{
    pdetailed_mode_closure_t pclosure = c;

    if(!is_display_descriptor((disp_u8_t *)timing, EDID_DETAIL_EST_TIMINGS))
        return;

    pclosure->modes += disp_est3_modes(pclosure->pdisp_connector, timing);
}

/**
 * add_established_modes - get est. modes from EDID and add them
 * @connector: connector to add mode(s) to
 * @edid: EDID block to scan
 *
 * Each EDID block contains a bitmap of the supported "established modes" list
 * (defined above).  Tease them out and add them to the global modes list.
 */
static disp_s32_t
add_established_modes(pdisp_connector_t pdisp_connector, pedid_t edid)
{
    disp_u32_t est_bits = edid->established_timings.t1 |
        (edid->established_timings.t2 << 8) |
        ((edid->established_timings.mfg_rsvd & 0x80) << 9);
    disp_s32_t i, modes = 0;
    detailed_mode_closure_t closure = 
    {
        .pdisp_connector = pdisp_connector,
        .edid = edid,
    };
    disp_display_mode_t mode_src = {0};

    for(i = 0; i <= EDID_EST_TIMINGS; i++) 
    {
        if(est_bits & (1<<i)) 
        {
            disp_memcpy(&mode_src, &edid_est_modes[i], sizeof(disp_display_mode_t));
            if(disp_mode_probed_add(pdisp_connector, &mode_src, DISP_FALSE))
                modes++;

        }
    }

    if(version_greater(edid, 1, 0))
            disp_for_each_detailed_block((disp_u8_t *)edid,
                        do_established_modes, &closure);

    return modes + closure.modes;
}

static disp_void_t do_standard_modes(pdetailed_timing_t timing, disp_void_t *c)
{
    pdetailed_mode_closure_t pclosure = c;
    pdetailed_non_pixel_t pdata = &timing->data.other_data;
    pdisp_connector_t pdisp_connector = pclosure->pdisp_connector;
    pedid_t edid = pclosure->edid;
    disp_s32_t i;
    disp_display_mode_t newmode = {0};
    disp_u8_t ret = DISP_FALSE;

    if(!is_display_descriptor((disp_u8_t *)timing, EDID_DETAIL_STD_MODES))
        return;

    for(i = 0; i < 6; i++) 
    {
        pstd_timing_t std = &pdata->data.timings.timings[i];

        ret = disp_mode_std(edid, std, &newmode);
        if(ret)
        {
            if(disp_mode_probed_add(pdisp_connector, &newmode, DISP_FALSE))
                pclosure->modes++;
        }
    }
}

/**
 * add_standard_modes - get std. modes from EDID and add them
 * @connector: connector to add mode(s) to
 * @edid: EDID block to scan
 *
 * Standard modes can be calculated using the appropriate standard (DMT,
 * GTF or CVT. Grab them from @edid and add them to the list.
 */
static disp_s32_t add_standard_modes(pdisp_connector_t pdisp_connector, pedid_t edid)
{
    disp_s32_t i, modes = 0;
    disp_display_mode_t newmode = {0};
    disp_u8_t ret = DISP_FALSE;
    detailed_mode_closure_t closure = {
        .pdisp_connector = pdisp_connector,
        .edid = edid,
    };

    for(i = 0; i < EDID_STD_TIMINGS; i++) {

        ret = disp_mode_std(edid, &edid->standard_timings[i], &newmode);
        if(ret) {
            if(disp_mode_probed_add(pdisp_connector, &newmode, DISP_FALSE))
                modes++;
        }
    }

    if(version_greater(edid, 1, 0))
        disp_for_each_detailed_block((disp_u8_t *)edid, do_standard_modes,
                        &closure);

    /* XXX should also look for standard codes in VTB blocks */

    return modes + closure.modes;
}

static disp_s32_t disp_cvt_modes(pdisp_connector_t pdisp_connector,
             pdetailed_timing_t timing)
{
    disp_s32_t i, j, modes = 0;
    disp_display_mode_t new_mode = {0};
    pcvt_timing_t cvt;
    disp_s32_t rates[] = { 60, 85, 75, 60, 50 };
    disp_u8_t empty[3] = { 0, 0, 0 };
    disp_u8_t find = DISP_FALSE;

    for(i = 0; i < 4; i++) {
        disp_s32_t width, height;

        cvt = &(timing->data.other_data.data.cvt.cvt[i]);

        if(!disp_memcmp(cvt->code, empty, 3))
            continue;

        height = (cvt->code[0] + ((cvt->code[1] & 0xf0) << 4) + 1) * 2;
        switch(cvt->code[1] & 0x0c)
        {
        /* default - because compiler doesn't see that we've enumerated all cases */
        default:
        case 0x00:
            width = height * 4 / 3;
            break;
        case 0x04:
            width = height * 16 / 9;
            break;
        case 0x08:
            width = height * 16 / 10;
            break;
        case 0x0c:
            width = height * 15 / 9;
            break;
        }

        for(j = 1; j < 5; j++)
        {
            if(cvt->code[2] & (1 << j))
            {
                find = disp_cvt_mode(width, height, rates[j], j == 0, DISP_FALSE, DISP_FALSE, &new_mode);
                if(find && disp_mode_probed_add(pdisp_connector, &new_mode, DISP_FALSE))
                    modes++;
            }
        }
    }

    return modes;
}

static disp_void_t
do_cvt_mode(pdetailed_timing_t timing, disp_void_t *c)
{
    pdetailed_mode_closure_t pclosure = c;

    if(!is_display_descriptor((disp_u8_t *)timing, EDID_DETAIL_CVT_3BYTE))
        return;

    pclosure->modes += disp_cvt_modes(pclosure->pdisp_connector, timing);
}

static disp_s32_t
add_cvt_modes(pdisp_connector_t pdisp_connector, pedid_t edid)
{
    detailed_mode_closure_t closure = {
        .pdisp_connector = pdisp_connector,
        .edid = edid,
    };

    if(version_greater(edid, 1, 2))
        disp_for_each_detailed_block((disp_u8_t *)edid, do_cvt_mode, &closure);

    /* XXX should also look for CVT codes in VTB blocks */

    return closure.modes;
}

static disp_void_t fixup_detailed_cea_mode_clock(pdisp_display_mode_t mode);

static disp_void_t
do_detailed_mode(pdetailed_timing_t timing, disp_void_t *c)
{
    pdetailed_mode_closure_t pclosure = c;
    disp_display_mode_t newmode = {0};

    if(!is_detailed_timing_descriptor((disp_u8_t *)timing))
        return;

    if(!disp_mode_detailed(pclosure->edid, timing, pclosure->quirks, &newmode))
        return;

    if(pclosure->preferred)
        newmode.type |= DISP_MODE_TYPE_PREFERRED;

    /*
     * Detailed modes are limited to 10kHz pixel clock resolution,
     * so fix up anything that looks like CEA/HDMI mode, but the clock
     * is just slightly off.
     */
    fixup_detailed_cea_mode_clock(&newmode);

    if(disp_mode_probed_add(pclosure->pdisp_connector, &newmode, DISP_FALSE))
        pclosure->modes++;
    pclosure->preferred = DISP_FALSE;
}

/*
 * add_detailed_modes - Add modes from detailed timings
 * @connector: attached connector
 * @edid: EDID block to scan
 * @quirks: quirks to apply
 */
static disp_s32_t
add_detailed_modes(pdisp_connector_t pdisp_connector, pedid_t edid,
           disp_u32_t quirks)
{
    detailed_mode_closure_t closure = 
    {
        .pdisp_connector = pdisp_connector,
        .edid = edid,
        .preferred = DISP_TRUE,
        .quirks = quirks,
    };

    if(closure.preferred && !version_greater(edid, 1, 3))
        closure.preferred =
            (edid->features & DISP_EDID_FEATURE_PREFERRED_TIMING);

    disp_for_each_detailed_block((disp_u8_t *)edid, do_detailed_mode, &closure);

    return closure.modes;
}

#define AUDIO_BLOCK         0x01
#define VIDEO_BLOCK         0x02
#define VENDOR_BLOCK        0x03    // Vendor Specific Data Block
#define SPEAKER_BLOCK       0x04
#define USE_EXTENDED_TAG    0x07
#define EXT_VIDEO_CAPABILITY_BLOCK      0x00
#define EXT_VENDOR_VIDEO_BLOCK          0x01 // Vendor-Specific Video Data Block
#define EXT_COLORIMETRY_BLOCK           0x05
#define EXT_HDR_STATIC_METADATA_BLOCK   0x06
#define EXT_VIDEO_DATA_BLOCK_420        0x0E
#define EXT_VIDEO_CAP_BLOCK_Y420CMDB    0x0F
#define EDID_BASIC_AUDIO        (1 << 6)
#define EDID_CEA_YCRCB444       (1 << 5)
#define EDID_CEA_YCRCB422       (1 << 4)
#define EDID_CEA_VCDB_QS        (1 << 6)

/*
 * Search EDID for CEA extension block.
 */
disp_u8_t *disp_find_edid_extension(pedid_t edid,
                  disp_s32_t ext_id, disp_s32_t *ext_index)
{
    disp_u8_t *edid_ext = DISP_NULL;
    disp_s32_t i;

    /* No EDID or EDID extensions */
    if(edid == DISP_NULL || edid->extensions == 0)
        return DISP_NULL;

    /* Find CEA extension */
    for(i = *ext_index; i < edid->extensions; i++)
    {
        edid_ext = (disp_u8_t *)edid + EDID_LENGTH * (i + 1);
        if(edid_ext[0] == ext_id)
            break;
    }

    if(i >= edid->extensions)
        return DISP_NULL;

    *ext_index = i + 1;

    return edid_ext;
}

static disp_u8_t *disp_find_cea_extension(pedid_t edid)
{
    disp_u8_t *cea;
    disp_s32_t ext_index = 0;

    /* Look for a top level CEA extension block */
    /* FIXME: make callers iterate through multiple CEA ext blocks? */
    cea = disp_find_edid_extension(edid, CEA_EXT, &ext_index);
    if(cea)
        return cea;

    return cea;
}

static pdisp_display_mode_t cea_mode_for_vic(disp_u8_t vic)
{
    if(vic >= 1 && vic < 1 + ARRAY_SIZE(edid_cea_modes_1))
        return &edid_cea_modes_1[vic - 1];
    if(vic >= 193 && vic < 193 + ARRAY_SIZE(edid_cea_modes_193))
        return &edid_cea_modes_193[vic - 193];

    disp_print(DDM_TRACE, "cea_mode_for_vic vic[%u] invalid!\n", vic);
    return DISP_NULL;
}

static disp_s32_t add_common_modes(pdisp_connector_t pdisp_connector)
{
    disp_display_mode_t mode_src = {0};
    pdisp_display_mode_t pnative_mode = pdisp_connector->native_mode;
    disp_s32_t mode_num = 0, idx = 0, idx1 = 0;
    disp_u8_t mode_existed = DISP_FALSE;

    if (pnative_mode)
    {
        for (idx = 0; idx < ARRAY_SIZE(common_modes); idx++)
        {
            if (common_modes[idx].hactive > pnative_mode->hdisplay ||
                common_modes[idx].vactive > pnative_mode->vdisplay ||
                (common_modes[idx].hactive == pnative_mode->hdisplay &&
                common_modes[idx].vactive == pnative_mode->vdisplay))
                continue;

            mode_existed = DISP_FALSE;
            for (idx1 = 0; idx1 < pdisp_connector->mode_list.num; idx1++)
            {
                if (common_modes[idx].hactive == pdisp_connector->mode_list.modes[idx1].hdisplay &&
                    common_modes[idx].vactive == pdisp_connector->mode_list.modes[idx1].vdisplay)
                {
                    mode_existed = DISP_TRUE;
                    break;
                }
            }
            if (mode_existed)
                continue;

            disp_memcpy(&mode_src, pnative_mode, sizeof(disp_display_mode_t));
            mode_src.hdisplay = common_modes[idx].hactive;
            mode_src.vdisplay = common_modes[idx].vactive;
            mode_src.type &= ~DISP_MODE_TYPE_PREFERRED;
            mode_src.flags |= common_modes[idx].flags;
            disp_mode_set_name(&mode_src);

            if(disp_mode_probed_add(pdisp_connector, &mode_src, DISP_FALSE))
                mode_num++;
        }
    }

    return mode_num;
}

static disp_u8_t cea_num_vics(disp_void_t)
{
    return 193 + ARRAY_SIZE(edid_cea_modes_193);
}

static disp_u8_t cea_next_vic(disp_u8_t vic)
{
    if(++vic == 1 + ARRAY_SIZE(edid_cea_modes_1))
        vic = 193;
    return vic;
}

/*
 * Calculate the alternate clock for the CEA mode
 * (60Hz vs. 59.94Hz etc.)
 */
static disp_s32_t
cea_mode_alternate_clock(pdisp_display_mode_t cea_mode)
{
    disp_s32_t clock = cea_mode->clock;

    if(disp_mode_vrefresh(cea_mode) % 6 != 0)
        return clock;

    /*
     * edid_cea_modes contains the 59.94Hz
     * variant for 240 and 480 line modes,
     * and the 60Hz variant otherwise.
     */
    if(cea_mode->vdisplay == 240 || cea_mode->vdisplay == 480)
        clock = (disp_s32_t)DIV_ROUND_CLOSEST(clock * 1001, 1000);
    else
        clock = (disp_s32_t)DIV_ROUND_CLOSEST(clock * 1000, 1001);

    return clock;
}

static disp_u8_t
cea_mode_alternate_timings(disp_u8_t vic, pdisp_display_mode_t mode)
{
    /*
     * For certain VICs the spec allows the vertical
     * front porch to vary by one or two lines.
     *
     * cea_modes[] stores the variant with the shortest
     * vertical front porch. We can adjust the mode to
     * get the other variants by simply increasing the
     * vertical front porch length.
     */
    if(cea_mode_for_vic(8)->vtotal != 262 ||
             cea_mode_for_vic(9)->vtotal != 262 ||
             cea_mode_for_vic(12)->vtotal != 262 ||
             cea_mode_for_vic(13)->vtotal != 262 ||
             cea_mode_for_vic(23)->vtotal != 312 ||
             cea_mode_for_vic(24)->vtotal != 312 ||
             cea_mode_for_vic(27)->vtotal != 312 ||
             cea_mode_for_vic(28)->vtotal != 312)
    {
        disp_print(DDM_ERROR, "timings vtotal wrong\n");
    }

    if(((vic == 8 || vic == 9 ||
          vic == 12 || vic == 13) && mode->vtotal < 263) ||
        ((vic == 23 || vic == 24 ||
          vic == 27 || vic == 28) && mode->vtotal < 314))
    {
        mode->vsync_start++;
        mode->vsync_end++;
        mode->vtotal++;

        return DISP_TRUE;
    }

    return DISP_FALSE;
}

static disp_u8_t disp_match_cea_mode_clock_tolerance(pdisp_display_mode_t to_match,
                         disp_u32_t clock_tolerance)
{
    disp_u32_t match_flags = DISP_MODE_MATCH_TIMINGS | DISP_MODE_MATCH_FLAGS;
    disp_u8_t vic;

    if(!to_match->clock)
        return 0;

    if(to_match->picture_aspect_ratio)
        match_flags |= DISP_MODE_MATCH_ASPECT_RATIO;

    for(vic = 1; vic < cea_num_vics(); vic = cea_next_vic(vic)) {
        disp_display_mode_t cea_mode = *cea_mode_for_vic(vic);
        disp_u32_t clock1, clock2;

        /* Check both 60Hz and 59.94Hz */
        clock1 = cea_mode.clock;
        clock2 = cea_mode_alternate_clock(&cea_mode);

        if(ABS(to_match->clock - clock1) > clock_tolerance &&
            ABS(to_match->clock - clock2) > clock_tolerance)
            continue;

        do
        {
            if(disp_mode_match(to_match, &cea_mode, match_flags))
                return vic;
        } while(cea_mode_alternate_timings(vic, &cea_mode));
    }

    return 0;
}

/**
 * disp_match_cea_mode - look for a CEA mode matching given mode
 * @to_match: display mode
 *
 * Return: The CEA Video ID (VIC) of the mode or 0 if it isn't a CEA-861
 * mode.
 */
disp_u8_t disp_match_cea_mode(pdisp_display_mode_t to_match)
{
    disp_u32_t match_flags = DISP_MODE_MATCH_TIMINGS | DISP_MODE_MATCH_FLAGS;
    disp_u8_t vic;

    if(!to_match->clock)
        return 0;

    if(to_match->picture_aspect_ratio)
        match_flags |= DISP_MODE_MATCH_ASPECT_RATIO;

    for(vic = 1; vic < cea_num_vics(); vic = cea_next_vic(vic))
    {
        disp_display_mode_t cea_mode = *cea_mode_for_vic(vic);
        disp_u32_t clock1, clock2;

        /* Check both 60Hz and 59.94Hz */
        clock1 = cea_mode.clock;
        clock2 = cea_mode_alternate_clock(&cea_mode);

        if(KHZ2PICOS(to_match->clock) != KHZ2PICOS(clock1) &&
            KHZ2PICOS(to_match->clock) != KHZ2PICOS(clock2))
            continue;

        do
        {
            if(disp_mode_match(to_match, &cea_mode, match_flags))
                return vic;
        } while(cea_mode_alternate_timings(vic, &cea_mode));
    }

    return 0;
}

static disp_u8_t disp_valid_cea_vic(disp_u8_t vic)
{
    return cea_mode_for_vic(vic) != DISP_NULL;
}

static HDMI_PICTURE_ASPECT_TYPE disp_get_cea_aspect_ratio(disp_u8_t video_code)
{
    pdisp_display_mode_t mode = cea_mode_for_vic(video_code);

    if(mode)
        return mode->picture_aspect_ratio;

    return HDMI_PICTURE_ASPECT_NONE;
}

static HDMI_PICTURE_ASPECT_TYPE disp_get_hdmi_aspect_ratio(disp_u8_t video_code)
{
    return edid_4k_modes[video_code].picture_aspect_ratio;
}

/*
 * Calculate the alternate clock for HDMI modes (those from the HDMI vendor
 * specific block).
 */
static disp_s32_t
hdmi_mode_alternate_clock(pdisp_display_mode_t hdmi_mode)
{
    return cea_mode_alternate_clock(hdmi_mode);
}

static disp_u8_t disp_match_hdmi_mode_clock_tolerance(pdisp_display_mode_t to_match,
                          disp_s32_t clock_tolerance)
{
    disp_u32_t match_flags = DISP_MODE_MATCH_TIMINGS | DISP_MODE_MATCH_FLAGS;
    disp_u8_t vic;

    if(!to_match->clock)
        return 0;

    if(to_match->picture_aspect_ratio)
        match_flags |= DISP_MODE_MATCH_ASPECT_RATIO;

    for(vic = 1; vic < ARRAY_SIZE(edid_4k_modes); vic++) {
        pdisp_display_mode_t hdmi_mode = &edid_4k_modes[vic];
        disp_s32_t clock1, clock2;

        /* Make sure to also match alternate clocks */
        clock1 = hdmi_mode->clock;
        clock2 = hdmi_mode_alternate_clock(hdmi_mode);

        if(ABS(to_match->clock - clock1) > clock_tolerance &&
            ABS(to_match->clock - clock2) > clock_tolerance)
            continue;

        if(disp_mode_match(to_match, hdmi_mode, match_flags))
            return vic;
    }

    return 0;
}

/*
 * disp_match_hdmi_mode - look for a HDMI mode matching given mode
 * @to_match: display mode
 *
 * An HDMI mode is one defined in the HDMI vendor specific block.
 *
 * Returns the HDMI Video ID (VIC) of the mode or 0 if it isn't one.
 */
static disp_u8_t disp_match_hdmi_mode(pdisp_display_mode_t to_match)
{
    disp_u32_t match_flags = DISP_MODE_MATCH_TIMINGS | DISP_MODE_MATCH_FLAGS;
    disp_u8_t vic;

    if(!to_match->clock)
        return 0;

    if(to_match->picture_aspect_ratio)
        match_flags |= DISP_MODE_MATCH_ASPECT_RATIO;

    for(vic = 1; vic < ARRAY_SIZE(edid_4k_modes); vic++) {
        pdisp_display_mode_t hdmi_mode = &edid_4k_modes[vic];
        disp_s32_t clock1, clock2;

        /* Make sure to also match alternate clocks */
        clock1 = hdmi_mode->clock;
        clock2 = hdmi_mode_alternate_clock(hdmi_mode);

        if((KHZ2PICOS(to_match->clock) == KHZ2PICOS(clock1) ||
             KHZ2PICOS(to_match->clock) == KHZ2PICOS(clock2)) &&
            disp_mode_match(to_match, hdmi_mode, match_flags))
            return vic;
    }
    return 0;
}

static disp_u8_t disp_valid_hdmi_vic(disp_u8_t vic)
{
    return vic > 0 && vic < ARRAY_SIZE(edid_4k_modes);
}

#if 0 // VS compile warning, add it when real call
static disp_s32_t
add_alternate_cea_modes(pdisp_connector_t pdisp_connector, pedid_t edid)
{
    pdisp_display_mode_t mode, *tmp;
//refine later
//    LIST_HEAD(list);
    disp_s32_t modes = 0;

    /* Don't add CEA modes if the CEA extension block is missing */
    if(!disp_find_cea_extension(edid))
        return 0;

//refine later
    /*
     * Go through all probed modes and create a new mode
     * with the alternate clock for certain CEA modes.
     */
    //list_for_each_entry(mode, &connector->probed_modes, head) 
    {
        pdisp_display_mode_t cea_mode = DISP_NULL;
        pdisp_display_mode_t newmode;
        disp_u8_t vic = disp_match_cea_mode(mode);
        disp_s32_t clock1, clock2;

        if(disp_valid_cea_vic(vic))
        {
            cea_mode = cea_mode_for_vic(vic);
            clock2 = cea_mode_alternate_clock(cea_mode);
        }
        else
        {
            vic = disp_match_hdmi_mode(mode);
            if(disp_valid_hdmi_vic(vic))
            {
                cea_mode = &edid_4k_modes[vic];
                clock2 = hdmi_mode_alternate_clock(cea_mode);
            }
        }

        if(!cea_mode)
            //continue;

        clock1 = cea_mode->clock;

        if(clock1 == clock2)
            //continue;

        if(mode->clock != clock1 && mode->clock != clock2)
            //continue;
//refine later
        //newmode = disp_mode_duplicate(dev, cea_mode);
        if(!newmode)
            //continue;

        /* Carry over the stereo flags */
        newmode->flags |= mode->flags & DISP_MODE_FLAG_3D_MASK;

        /*
         * The current mode could be either variant. Make
         * sure to pick the "other" clock for the new mode.
         */
        if(mode->clock != clock1)
            newmode->clock = clock1;
        else
            newmode->clock = clock2;

        //list_add_tail(&newmode->head, &list);
    }

    //list_for_each_entry_safe(mode, tmp, &list, head) 
    {
        //list_del(&mode->head);
        if(disp_mode_probed_add(pdisp_connector, mode))
            modes++;
    }

    return modes;
}
#endif

static disp_u8_t svd_to_vic(disp_u8_t svd)
{
    /* 0-6 bit vic, 7th bit native mode indicator */
    if((svd >= 1 &&  svd <= 64) || (svd >= 129 && svd <= 192))
        return svd & 127;

    return svd;
}

static disp_u8_t disp_display_mode_from_vic_index( disp_u8_t *video_db,
    disp_u8_t video_len, disp_u8_t video_index, pdisp_display_mode_t pnewmode)
{
    disp_u8_t vic;

    if(video_db == DISP_NULL || video_index >= video_len)
        return DISP_FALSE;

    /* CEA modes are numbered 1..127 */
    vic = svd_to_vic(video_db[video_index]);
    if(!disp_valid_cea_vic(vic))
        return DISP_FALSE;

    disp_memcpy(pnewmode, cea_mode_for_vic(vic), sizeof(disp_display_mode_t));

    return DISP_TRUE;
}

static disp_void_t bitmap_set(disp_u32_t *map, disp_u32_t start, disp_s32_t len)
{
    disp_u32_t *p = map + BIT_WORD(start);
    const disp_u32_t size = start + len;
    disp_s32_t bits_to_set = BITS_PER_LONG - (start % BITS_PER_LONG);
    disp_u32_t mask_to_set = BITMAP_FIRST_WORD_MASK(start);

    while(len - bits_to_set >= 0) {
        *p |= mask_to_set;
        len -= bits_to_set;
        bits_to_set = BITS_PER_LONG;
        mask_to_set = ~0UL;
        p++;
    }
    if(len) {
        mask_to_set &= BITMAP_LAST_WORD_MASK((disp_s64_t)size);
        *p |= mask_to_set;
    }
}

/*
 * do_y420vdb_modes - Parse YCBCR 420 only modes
 * @connector: connector corresponding to the HDMI sink
 * @svds: start of the data block of CEA YCBCR 420 VDB
 * @len: length of the CEA YCBCR 420 VDB
 *
 * Parse the CEA-861-F YCBCR 420 Video Data Block (Y420VDB)
 * which contains modes which can be supported in YCBCR 420
 * output format only.
 */
static disp_s32_t do_y420vdb_modes(pdisp_connector_t pdisp_connector,
                disp_u8_t *svds, disp_u8_t svds_len)
{
    disp_s32_t modes = 0, i;
    pdisp_display_info_t pinfo = &pdisp_connector->disp_info;
    pdisp_hdmi_info_t phdmi = &pinfo->hdmi;
    disp_display_mode_t cur_mode = {0};

    for(i = 0; i < svds_len; i++) {
        disp_u8_t vic = svd_to_vic(svds[i]);

        if(!disp_valid_cea_vic(vic))
            continue;

        bitmap_set(phdmi->y420_vdb_modes, vic, 1);

        disp_memcpy(&cur_mode, cea_mode_for_vic(vic), sizeof(disp_display_mode_t));
        cur_mode.output_format_mask |= DISP_COLOR_FORMAT_YCRCB420;
        if(disp_mode_probed_add(pdisp_connector, &cur_mode, DISP_FALSE))
        {
            modes++;
        }
    }

    if(modes > 0)
        pinfo->color_formats |= DISP_COLOR_FORMAT_YCRCB420;
    return modes;
}

/*
 * disp_add_cmdb_modes - Add a YCBCR 420 mode into bitmap
 * @connector: connector corresponding to the HDMI sink
 * @vic: CEA vic for the video mode to be added in the map
 *
 * Makes an entry for a videomode in the YCBCR 420 bitmap
 */
static disp_void_t
disp_add_cmdb_modes(pdisp_connector_t pdisp_connector, disp_u8_t svd)
{
    disp_u8_t vic = svd_to_vic(svd);
    pdisp_hdmi_info_t phdmi = &pdisp_connector->disp_info.hdmi;

    if(!disp_valid_cea_vic(vic))
        return;

    bitmap_set(phdmi->y420_cmdb_modes, vic, 1);
}

/**
 * disp_display_mode_from_cea_vic() - return a mode for CEA VIC
 * @dev: DRM device
 * @video_code: CEA VIC of the mode
 *
 * Creates a new mode matching the specified CEA VIC.
 *
 * Returns: A new disp_display_mode_t on success or NULL on failure
 */
pdisp_display_mode_t 
disp_display_mode_from_cea_vic(
                  disp_u8_t video_code)
{
    pdisp_display_mode_t cea_mode;
    pdisp_display_mode_t newmode = DISP_NULL;

    cea_mode = cea_mode_for_vic(video_code);
    if(!cea_mode)
        return DISP_NULL;
//refine later
    //newmode = disp_mode_duplicate(dev, cea_mode);
    if(!newmode)
        return DISP_NULL;

    return newmode;
}


static disp_s32_t do_cea_modes(pdisp_connector_t pdisp_connector, disp_u8_t *db, disp_u8_t len)
{
    disp_s32_t modes = 0;
    disp_u8_t i = 0;
    pdisp_hdmi_info_t phdmi = &pdisp_connector->disp_info.hdmi;
    disp_display_mode_t mode = {0};

    for(i = 0; i < len; i++) 
    {
        if(disp_display_mode_from_vic_index(db, len, i, &mode))
        {
            /*
             * YCBCR420 capability block contains a bitmap which
             * gives the index of CEA modes from CEA VDB, which
             * can support YCBCR 420 sampling output also (apart
             * from RGB/YCBCR444 etc).
             * For example, if the bit 0 in bitmap is set,
             * first mode in VDB can support YCBCR420 output too.
             * Add YCBCR420 modes only if sink is HDMI 2.0 capable.
             */
            if(i < 64 && phdmi->y420_cmdb_map & (1ULL << i))
            {
                disp_add_cmdb_modes(pdisp_connector, db[i]);
                mode.output_format_mask |= DISP_COLOR_FORMAT_YCRCB420;
            }

            if(disp_mode_probed_add(pdisp_connector, &mode, DISP_FALSE))
            {
                modes++;
            }
        }
    }

    return modes;
}

struct stereo_mandatory_mode {
    disp_s32_t width, height, vrefresh;
    disp_u32_t flags;
};

static struct stereo_mandatory_mode stereo_mandatory_modes[] = {
    { 1920, 1080, 24, DISP_MODE_FLAG_3D_TOP_AND_BOTTOM },
    { 1920, 1080, 24, DISP_MODE_FLAG_3D_FRAME_PACKING },
    { 1920, 1080, 50,
      DISP_MODE_FLAG_INTERLACE | DISP_MODE_FLAG_3D_SIDE_BY_SIDE_HALF },
    { 1920, 1080, 60,
      DISP_MODE_FLAG_INTERLACE | DISP_MODE_FLAG_3D_SIDE_BY_SIDE_HALF },
    { 1280, 720,  50, DISP_MODE_FLAG_3D_TOP_AND_BOTTOM },
    { 1280, 720,  50, DISP_MODE_FLAG_3D_FRAME_PACKING },
    { 1280, 720,  60, DISP_MODE_FLAG_3D_TOP_AND_BOTTOM },
    { 1280, 720,  60, DISP_MODE_FLAG_3D_FRAME_PACKING }
};

static disp_u8_t
stereo_match_mandatory(pdisp_display_mode_t mode,
               struct stereo_mandatory_mode *stereo_mode)
{
    disp_u32_t interlaced = mode->flags & DISP_MODE_FLAG_INTERLACE;

    return mode->hdisplay == stereo_mode->width &&
           mode->vdisplay == stereo_mode->height &&
           interlaced == (stereo_mode->flags & DISP_MODE_FLAG_INTERLACE) &&
           disp_mode_vrefresh(mode) == stereo_mode->vrefresh;
}

static disp_s32_t add_hdmi_mandatory_stereo_modes(void)
{
    disp_s32_t modes = 0;
#if 0 // not use
    //pdisp_display_mode_t mode;
    //refine later
    //struct list_head stereo_modes;
    //disp_s32_t i;

    //INIT_LIST_HEAD(&stereo_modes);

//refine later
    //list_for_each_entry(mode, &connector->probed_modes, head) 
    {
        for(i = 0; i < ARRAY_SIZE(stereo_mandatory_modes); i++) {
            struct stereo_mandatory_mode *mandatory;
            pdisp_display_mode_t new_mode;

            if(!stereo_match_mandatory(mode,
                            &stereo_mandatory_modes[i]))
                continue;

            mandatory = &stereo_mandatory_modes[i];
            //refine later
            //new_mode = disp_mode_duplicate(dev, mode);
            if(!new_mode)
                continue;

            new_mode->flags |= mandatory->flags;
            //list_add_tail(&new_mode->head, &stereo_modes);
            modes++;
        }
    }
//refine later
    //list_splice_tail(&stereo_modes, &connector->probed_modes);
#endif
    return modes;
}

static disp_s32_t add_hdmi_mode(pdisp_connector_t pdisp_connector, disp_u8_t vic)
{
    disp_s32_t mode_num = 0;

    if(!disp_valid_hdmi_vic(vic)) {
        disp_print(DDM_INFO, "Unknown HDMI VIC: %d\n", vic);
        return 0;
    }

    if(disp_mode_probed_add(pdisp_connector, &edid_4k_modes[vic], DISP_FALSE))
        mode_num++;

    return mode_num;
}

static disp_s32_t add_3d_struct_modes(pdisp_connector_t pdisp_connector, disp_u16_t structure,
                   disp_u8_t *video_db, disp_u8_t video_len, disp_u8_t video_index)
{
    disp_display_mode_t newmode= {0};
    disp_s32_t modes = 0;

    if ((structure & (1 << 0)) || (structure & (1 << 6)) || (structure & (1 << 8)))
    {
        if(disp_display_mode_from_vic_index(video_db, video_len, video_index, &newmode)) 
        {
            if (structure & (1 << 0))
                newmode.flags |= DISP_MODE_FLAG_3D_FRAME_PACKING;
            else if (structure & (1 << 6))
                newmode.flags |= DISP_MODE_FLAG_3D_TOP_AND_BOTTOM;
            else if (structure & (1 << 8))
                newmode.flags |= DISP_MODE_FLAG_3D_SIDE_BY_SIDE_HALF;

            if(disp_mode_probed_add(pdisp_connector, &newmode, DISP_FALSE))
                modes++;
        }
    }

    return modes;
}

/*
 * do_hdmi_vsdb_modes - Parse the HDMI Vendor Specific data block
 * @connector: connector corresponding to the HDMI sink
 * @db: start of the CEA vendor specific block
 * @len: length of the CEA block payload, ie. one can access up to db[len]
 *
 * Parses the HDMI VSDB looking for modes to add to @connector. This function
 * also adds the stereo 3d modes when applicable.
 */
static disp_s32_t
do_hdmi_vsdb_modes(pdisp_connector_t pdisp_connector, disp_u8_t *db, disp_u8_t len,
           disp_u8_t *video_db, disp_u8_t video_len)
{
    pdisp_display_info_t pinfo = &pdisp_connector->disp_info;
    disp_s32_t modes = 0, offset = 0, i, multi_present = 0, multi_len;
    disp_u8_t vic_len, hdmi_3d_len = 0;
    disp_u16_t mask;
    disp_u16_t structure_all;
    disp_display_mode_t newmode = {0};

    if(len < 8)
        goto out;

    /* no HDMI_Video_Present */
    if(!(db[8] & (1 << 5)))
        goto out;

    /* Latency_Fields_Present */
    if(db[8] & (1 << 7))
        offset += 2;

    /* I_Latency_Fields_Present */
    if(db[8] & (1 << 6))
        offset += 2;

    /* the declared length is not long enough for the 2 first bytes
     * of additional video format capabilities */
    if(len < (8 + offset + 2))
        goto out;

    /* 3D_Present */
    offset++;
    if(db[8 + offset] & (1 << 7)) 
    {
        modes += add_hdmi_mandatory_stereo_modes();

        /* 3D_Multi_present */
        multi_present = (db[8 + offset] & 0x60) >> 5;
    }

    offset++;
    vic_len = db[8 + offset] >> 5;
    hdmi_3d_len = db[8 + offset] & 0x1f;

    for(i = 0; i < vic_len && len >= (9 + offset + i); i++) {
        disp_u8_t vic;

        vic = db[9 + offset + i];
        modes += add_hdmi_mode(pdisp_connector, vic);
    }
    offset += 1 + vic_len;

    if(multi_present == 1)
        multi_len = 2;
    else if(multi_present == 2)
        multi_len = 4;
    else
        multi_len = 0;

    if(len < (8 + offset + hdmi_3d_len - 1))
        goto out;

    if(hdmi_3d_len < multi_len)
        goto out;

    if(multi_present == 1 || multi_present == 2) {
        /* 3D_Structure_ALL */
        structure_all = (db[8 + offset] << 8) | db[9 + offset];

        /* check if 3D_MASK is present */
        if(multi_present == 2)
            mask = (db[10 + offset] << 8) | db[11 + offset];
        else
            mask = 0xffff;

        for(i = 0; i < 16; i++) 
        {
            if(mask & (1 << i))
                modes += add_3d_struct_modes(pdisp_connector,
                        structure_all,
                        video_db,
                        video_len, (disp_u8_t)i);
        }
    }

    offset += multi_len;

    for(i = 0; i < (hdmi_3d_len - multi_len); i++) {
        disp_u32_t newflag = 0;
        disp_u8_t detail_present, vic_index;

        detail_present = ((db[8 + offset + i] & 0x0f) > 7);

        if(detail_present && (i + 1 == hdmi_3d_len - multi_len))
            break;

        /* 2D_VIC_order_X */
        vic_index = db[8 + offset + i] >> 4;

        /* 3D_Structure_X */
        switch(db[8 + offset + i] & 0x0f) 
        {
        case 0:
            newflag = DISP_MODE_FLAG_3D_FRAME_PACKING;
            break;
        case 6:
            newflag = DISP_MODE_FLAG_3D_TOP_AND_BOTTOM;
            break;
        case 8:
            /* 3D_Detail_X */
            if((db[9 + offset + i] >> 4) == 1)
                newflag = DISP_MODE_FLAG_3D_SIDE_BY_SIDE_HALF;
            break;
        }

        if(newflag != 0) 
        {
            if(disp_display_mode_from_vic_index(video_db, video_len, vic_index, &newmode)) 
            {
                newmode.flags |= newflag;
                if(disp_mode_probed_add(pdisp_connector, &newmode, DISP_FALSE))
                    modes++;
            }
        }

        if(detail_present)
            i++;
    }

out:
    if(modes > 0)
    {
        pinfo->has_hdmi_infoframe = DISP_TRUE;
    }
    return modes;
}

static disp_u8_t
cea_db_payload_len(disp_u8_t *db)
{
    return db[0] & 0x1f;
}

static disp_s32_t
cea_db_extended_tag(disp_u8_t *db)
{
    return db[1];
}

static disp_s32_t
cea_db_tag(disp_u8_t *db)
{
    return db[0] >> 5;
}

static disp_s32_t
cea_revision(disp_u8_t *cea)
{
    /*
     * FIXME is this correct for the DispID variant?
     * The DispID spec doesn't really specify whether
     * this is the revision of the CEA extension or
     * the DispID CEA data block. And the only value
     * given as an example is 0.
     */
    return cea[1];
}

#define DATA_BLOCK_CTA 0x81
static disp_s32_t
cea_db_offsets(disp_u8_t *cea, disp_s32_t *start, disp_s32_t *end)
{
    /* DisplayID CTA extension blocks and top-level CEA EDID
     * block header definitions differ in the following bytes:
     *   1) Byte 2 of the header specifies length differently,
     *   2) Byte 3 is only present in the CEA top level block.
     *
     * The different definitions for byte 2 follow.
     *
     * DisplayID CTA extension block defines byte 2 as:
     *   Number of payload bytes
     *
     * CEA EDID block defines byte 2 as:
     *   Byte number (decimal) within this block where the 18-byte
     *   DTDs begin. If no non-DTD data is present in this extension
     *   block, the value should be set to 04h (the byte after next).
     *   If set to 00h, there are no DTDs present in this block and
     *   no non-DTD data.
     */
    if(cea[0] == DATA_BLOCK_CTA) 
    {
        /*
         * for_each_displayid_db() has already verified
         * that these stay within expected bounds.
         */
        *start = 3;
        *end = *start + cea[2];
    } 
    else if(cea[0] == CEA_EXT) 
    {
        /* Data block offset in CEA extension block */
        *start = 4;
        *end = cea[2];
        if(*end == 0)
            *end = 127;
        if(*end < 4 || *end > 127)
            return -DISP_ERANGE;
    } 
    else 
    {
        return -DISP_EINVAL;
    }

    return 0;
}

static disp_u8_t cea_db_is_hdmi_vsdb(disp_u8_t *db)
{
    disp_s32_t hdmi_id;

    if(cea_db_tag(db) != VENDOR_BLOCK)
        return DISP_FALSE;

    if(cea_db_payload_len(db) < 5)
        return DISP_FALSE;

    hdmi_id = db[1] | (db[2] << 8) | (db[3] << 16);

    return hdmi_id == HDMI_IEEE_OUI;
}

static disp_u8_t cea_db_is_hdmi_forum_vsdb(disp_u8_t *db)
{
    disp_u32_t oui;

    if(cea_db_tag(db) != VENDOR_BLOCK)
        return DISP_FALSE;

    if(cea_db_payload_len(db) < 7)
        return DISP_FALSE;

    oui = db[3] << 16 | db[2] << 8 | db[1];

    return oui == HDMI_FORUM_IEEE_OUI;
}

static disp_u8_t cea_db_is_vcdb(disp_u8_t *db)
{
    if(cea_db_tag(db) != USE_EXTENDED_TAG)
        return DISP_FALSE;

    if(cea_db_payload_len(db) != 2)
        return DISP_FALSE;

    if(cea_db_extended_tag(db) != EXT_VIDEO_CAPABILITY_BLOCK)
        return DISP_FALSE;

    return DISP_TRUE;
}

static disp_u8_t cea_db_is_y420cmdb(disp_u8_t *db)
{
    if(cea_db_tag(db) != USE_EXTENDED_TAG)
        return DISP_FALSE;

    if(!cea_db_payload_len(db))
        return DISP_FALSE;

    if(cea_db_extended_tag(db) != EXT_VIDEO_CAP_BLOCK_Y420CMDB)
        return DISP_FALSE;

    return DISP_TRUE;
}

static disp_u8_t cea_db_is_y420vdb(disp_u8_t *db)
{
    if(cea_db_tag(db) != USE_EXTENDED_TAG)
        return DISP_FALSE;

    if(!cea_db_payload_len(db))
        return DISP_FALSE;

    if(cea_db_extended_tag(db) != EXT_VIDEO_DATA_BLOCK_420)
        return DISP_FALSE;

    return DISP_TRUE;
}

#define for_each_cea_db(cea, i, start, end) \
    for((i) = (start); (i) < (end) && (i) + cea_db_payload_len(&(cea)[(i)]) < (end); (i) += cea_db_payload_len(&(cea)[(i)]) + 1)

static disp_void_t disp_parse_y420cmdb_bitmap(pdisp_connector_t pdisp_connector,
                      disp_u8_t *db)
{
    pdisp_display_info_t pinfo = &pdisp_connector->disp_info;
    pdisp_hdmi_info_t phdmi = &pinfo->hdmi;
    disp_u8_t map_len = cea_db_payload_len(db) - 1;
    disp_u8_t count;
    disp_u64_t map = 0;

    if(map_len == 0) {
        /* All CEA modes support ycbcr420 sampling also.*/
        //refine later
        phdmi->y420_cmdb_map = 0xFFFFFFFFFFFFFFFF - 1;//U64_MAX;
        pinfo->color_formats |= DISP_COLOR_FORMAT_YCRCB420;
        return;
    }

    /*
     * This map indicates which of the existing CEA block modes
     * from VDB can support YCBCR420 output too. So if bit=0 is
     * set, first mode from VDB can support YCBCR420 output too.
     * We will parse and keep this map, before parsing VDB itself
     * to avoid going through the same block again and again.
     *
     * Spec is not clear about max possible size of this block.
     * Clamping max bitmap block size at 8 bytes. Every byte can
     * address 8 CEA modes, in this way this map can address
     * 8*8 = first 64 SVDs.
     */
    if(map_len > 8)
        map_len = 8;

    for(count = 0; count < map_len; count++)
        map |= (disp_u64_t)db[2 + count] << (8 * count);

    if(map)
        pinfo->color_formats |= DISP_COLOR_FORMAT_YCRCB420;

    phdmi->y420_cmdb_map = map;
}

static disp_s32_t
add_cea_modes(pdisp_connector_t pdisp_connector, pedid_t edid)
{
    disp_u8_t *cea = disp_find_cea_extension(edid);
    disp_u8_t *db, *hdmi = DISP_NULL, *video = DISP_NULL;
    disp_u8_t dbl = 0, hdmi_len = 0, video_len = 0;
    disp_s32_t modes = 0;

    if(cea && cea_revision(cea) >= 3) 
    {
        disp_s32_t i, start, end;

        if(cea_db_offsets(cea, &start, &end))
            return 0;

        for_each_cea_db(cea, i, start, end) 
        {
            db = &cea[i];
            dbl = cea_db_payload_len(db);

            if(cea_db_tag(db) == VIDEO_BLOCK) 
            {
                video = db + 1;
                video_len = dbl;
                modes += do_cea_modes(pdisp_connector, video, dbl);
            } 
            else if(cea_db_is_hdmi_vsdb(db)) 
            {
                hdmi = db;
                hdmi_len = dbl;
            }
            else if(cea_db_is_y420vdb(db)) 
            {
                disp_u8_t *vdb420 = &db[2];

                /* Add 4:2:0(only) modes present in EDID */
                modes += do_y420vdb_modes(pdisp_connector,
                              vdb420,
                              dbl - 1);
            }
        }
    }

    /*
     * We parse the HDMI VSDB after having added the cea modes as we will
     * be patching their flags when the sink supports stereo 3D.
     */
    if(hdmi)
        modes += do_hdmi_vsdb_modes(pdisp_connector, hdmi, hdmi_len, video,
                        video_len);

    return modes;
}

static disp_void_t fixup_detailed_cea_mode_clock(pdisp_display_mode_t mode)
{
    pdisp_display_mode_t cea_mode;
    disp_s32_t clock1, clock2, clock;
    disp_u8_t vic;
    disp_s8_t *type;

    /*
     * allow 5kHz clock difference either way to account for
     * the 10kHz clock resolution limit of detailed timings.
     */
    vic = disp_match_cea_mode_clock_tolerance(mode, 5);
    if(disp_valid_cea_vic(vic)) 
    {
        type = "CEA";
        cea_mode = cea_mode_for_vic(vic);
        clock1 = cea_mode->clock;
        clock2 = cea_mode_alternate_clock(cea_mode);
    } 
    else 
    {
        vic = disp_match_hdmi_mode_clock_tolerance(mode, 5);
        if(disp_valid_hdmi_vic(vic)) 
        {
            type = "HDMI";
            cea_mode = &edid_4k_modes[vic];
            clock1 = cea_mode->clock;
            clock2 = hdmi_mode_alternate_clock(cea_mode);
        } 
        else 
            return;
    }

//refine later
    /* pick whichever is closest */
    if(ABS(mode->clock - clock1) < ABS(mode->clock - clock2))
        clock = clock1;
    else
        clock = clock2;

    if(mode->clock == clock)
        return;

    disp_print(DDM_TRACE, "detailed mode matches %s VIC %d, adjusting clock %d -> %d\n",
          type, vic, mode->clock, clock);
    mode->clock = clock;
}

static disp_u8_t cea_db_is_hdmi_hdr_metadata_block(disp_u8_t *db)
{
    if(cea_db_tag(db) != USE_EXTENDED_TAG)
        return DISP_FALSE;

    if(db[1] != EXT_HDR_STATIC_METADATA_BLOCK)
        return DISP_FALSE;

    if(cea_db_payload_len(db) < 3)
        return DISP_FALSE;

    return DISP_TRUE;
}

static disp_u8_t eotf_supported(disp_u8_t *edid_ext)
{
    return edid_ext[2] &
        (BIT(DISP_EOTF_TRADITIONAL_GAMMA_SDR) |
         BIT(DISP_EOTF_TRADITIONAL_GAMMA_HDR) |
         BIT(DISP_EOTF_SMPTE_ST2084) |
         BIT(DISP_EOTF_BT_2100_HLG));
}

static disp_u8_t hdr_metadata_type(disp_u8_t *edid_ext)
{
    return edid_ext[3] &
        BIT(HDMI_STATIC_METADATA_TYPE1);
}

static disp_void_t
disp_parse_hdr_metadata_block(pdisp_connector_t pdisp_connector, disp_u8_t *db)
{
    disp_u16_t len;

    len = cea_db_payload_len(db);

    pdisp_connector->disp_info.hdr_sink_metadata.data.hdmi_type1.eotf =
                        eotf_supported(db);
    pdisp_connector->disp_info.hdr_sink_metadata.data.hdmi_type1.metadata_type =
                        hdr_metadata_type(db);

    if(len >= 4)
        pdisp_connector->disp_info.hdr_sink_metadata.data.hdmi_type1.max_cll = db[4];
    if(len >= 5)
        pdisp_connector->disp_info.hdr_sink_metadata.data.hdmi_type1.max_fall = db[5];
    if(len >= 6)
        pdisp_connector->disp_info.hdr_sink_metadata.data.hdmi_type1.min_cll = db[6];
}

static disp_u8_t cea_db_is_colorimetry(disp_u8_t *db)
{
    if(cea_db_tag(db) != USE_EXTENDED_TAG)
        return DISP_FALSE;

    if(!cea_db_payload_len(db))
        return DISP_FALSE;

    if(cea_db_extended_tag(db) != EXT_COLORIMETRY_BLOCK)
        return DISP_FALSE;

    return DISP_TRUE;
}

static disp_u32_t colorimetry_supported(disp_u8_t *edid_ext, pdisp_connector_t pdisp_connector)
{
    disp_u32_t colorimetry_support = 0;

    // spec CTA/861-G
    colorimetry_support |= (edid_ext[2] & 0x01) ? BIT(DISP_COLORIMETRY_XVYCC_601) : 0;
    colorimetry_support |= (edid_ext[2] & 0x02) ? BIT(DISP_COLORIMETRY_XVYCC_709) : 0;
    colorimetry_support |= (edid_ext[2] & 0x04) ? BIT(DISP_COLORIMETRY_SYCC_601) : 0;
    colorimetry_support |= (edid_ext[2] & 0x08) ? BIT(DISP_COLORIMETRY_YCC_ADOBE_601) : 0;
    colorimetry_support |= (edid_ext[2] & 0x10) ? BIT(DISP_COLORIMETRY_RGB_ADOBE) : 0;
    colorimetry_support |= (edid_ext[2] & 0x20) ? BIT(DISP_COLORIMETRY_CYCC_BT2020) : 0;
    colorimetry_support |= (edid_ext[2] & 0x40) ? BIT(DISP_COLORIMETRY_YCC_BT2020) : 0;
    colorimetry_support |= (edid_ext[2] & 0x80) ? BIT(DISP_COLORIMETRY_RGB_BT2020) : 0;

    // d65 white pointer: [0.3127, 0.3290] -> [0x140 or 0x141, 0x151]
    if(edid_ext[3] & 0x80)
        colorimetry_support |= (pdisp_connector->disp_info.rgbw_position[3].y == 0x0151) ?
            BIT(DISP_COLORIMETRY_RGB_DCI_D65) : BIT(DISP_COLORIMETRY_RGB_DCI);

    return colorimetry_support;
}

static disp_void_t
disp_parse_colorimetry_block(pdisp_connector_t pdisp_connector, disp_u8_t *db)
{
    disp_u16_t len;

    len = cea_db_payload_len(db);
    if(len != 3)
        disp_print(DDM_WARNING, "disp_parse_colorimetry_block, wrong len[%u]\n", len);

    pdisp_connector->disp_info.colorimetry_support = colorimetry_supported(db, pdisp_connector);
}

static disp_void_t
disp_parse_hdmi_vsdb_audio(pdisp_connector_t pdisp_connector, disp_u8_t *db)
{
    disp_u8_t len = cea_db_payload_len(db);

    if(len >= 6 && (db[6] & (1 << 7)))
        pdisp_connector->audio_info.eld[DISP_ELD_SAD_COUNT_CONN_TYPE] |= DISP_ELD_SUPPORTS_AI;
    if(len >= 8) 
    {
        pdisp_connector->audio_info.latency_present[0] = db[8] >> 7;
        pdisp_connector->audio_info.latency_present[1] = (db[8] >> 6) & 1;
    }
    if(len >= 9)
        pdisp_connector->audio_info.video_latency[0] = db[9];
    if(len >= 10)
        pdisp_connector->audio_info.audio_latency[0] = db[10];
    if(len >= 11)
        pdisp_connector->audio_info.video_latency[1] = db[11];
    if(len >= 12)
        pdisp_connector->audio_info.audio_latency[1] = db[12];

    disp_print(DDM_TRACE, "HDMI: latency present %d %d, "
              "video latency %d %d, "
              "audio latency %d %d\n",
              pdisp_connector->audio_info.latency_present[0],
              pdisp_connector->audio_info.latency_present[1],
              pdisp_connector->audio_info.video_latency[0],
              pdisp_connector->audio_info.video_latency[1],
              pdisp_connector->audio_info.audio_latency[0],
              pdisp_connector->audio_info.audio_latency[1]);
}

static disp_void_t
monitor_name(pdetailed_timing_t t, disp_void_t *data)
{
    if(!is_display_descriptor((disp_u8_t *)t, EDID_DETAIL_MONITOR_NAME))
        return;

    *(disp_u8_t **)data = t->data.other_data.data.str.str;
}

static disp_s32_t get_monitor_name(pedid_t edid, disp_u8_t name[13])
{
    disp_u8_t *edid_name = DISP_NULL;
    disp_s32_t mnl;

    if(!edid || !name)
        return 0;

    disp_for_each_detailed_block((disp_u8_t *)edid, monitor_name, &edid_name);
    for(mnl = 0; edid_name && mnl < 13; mnl++) 
    {
        if(edid_name[mnl] == 0x0a)
            break;

        name[mnl] = edid_name[mnl];
    }

    return mnl;
}

/**
 * disp_edid_get_monitor_name - fetch the monitor name from the edid
 * @edid: monitor EDID information
 * @name: pointer to a character array to hold the name of the monitor
 * @bufsize: The size of the name buffer (should be at least 14 chars.)
 *
 */
disp_void_t disp_edid_get_monitor_name(pedid_t edid, pdisp_s8_t name, disp_s32_t bufsize)
{
    disp_s32_t name_length;
    disp_u8_t buf[13];

    if(bufsize <= 0)
        return;

    name_length = DISP_MIN(get_monitor_name(edid, buf), bufsize - 1);
    disp_memcpy(name, buf, name_length);
    name[name_length] = '\0';
}

static disp_void_t clear_eld(pdisp_connector_t pdisp_connector)
{
    disp_memset(pdisp_connector->audio_info.eld, 0, sizeof(pdisp_connector->audio_info.eld));

    pdisp_connector->audio_info.latency_present[0] = DISP_FALSE;
    pdisp_connector->audio_info.latency_present[1] = DISP_FALSE;
    pdisp_connector->audio_info.video_latency[0] = 0;
    pdisp_connector->audio_info.audio_latency[0] = 0;
    pdisp_connector->audio_info.video_latency[1] = 0;
    pdisp_connector->audio_info.audio_latency[1] = 0;
}

/*
 * disp_edid_to_eld - build ELD from EDID
 * @connector: connector corresponding to the HDMI/DP sink
 * @edid: EDID to parse
 *
 * Fill the ELD (EDID-Like Data) buffer for passing to the audio driver. The
 * HDCP and Port_ID ELD fields are left for the graphics driver to fill in.
 */
static disp_void_t disp_edid_to_eld(pdisp_connector_t pdisp_connector, pedid_t edid)
{
    disp_u8_t *eld = pdisp_connector->audio_info.eld;
    disp_u8_t eld_default[] = "Digital Audio ";
    disp_u8_t *cea;
    disp_u8_t *db;
    disp_s32_t total_sad_count = 0;
    disp_s32_t mnl;
    disp_s32_t dbl;

    clear_eld(pdisp_connector);

    if(!edid)
        return;

    cea = disp_find_cea_extension(edid);
    if(!cea) 
    {
        disp_print(DDM_TRACE, "ELD: no CEA Extension found\n");
        return;
    }

    mnl = get_monitor_name(edid, &eld[DISP_ELD_MONITOR_NAME_STRING]);
    if(mnl == 0)
    {
        disp_memcpy(&eld[DISP_ELD_MONITOR_NAME_STRING], eld_default, sizeof(eld_default));
        mnl = sizeof(eld_default);
        eld[DISP_ELD_MONITOR_NAME_STRING + mnl - 1] = '0' + (disp_u8_t)pdisp_connector->index;
    }
    disp_print(DDM_TRACE, "ELD monitor %s\n", &eld[DISP_ELD_MONITOR_NAME_STRING]);

    eld[DISP_ELD_CEA_EDID_VER_MNL] = cea[1] << DISP_ELD_CEA_EDID_VER_SHIFT;
    eld[DISP_ELD_CEA_EDID_VER_MNL] |= mnl;

    eld[DISP_ELD_VER] = DISP_ELD_VER_CEA861D;

    eld[DISP_ELD_MANUFACTURER_NAME0] = edid->mfg_id[0];
    eld[DISP_ELD_MANUFACTURER_NAME1] = edid->mfg_id[1];
    eld[DISP_ELD_PRODUCT_CODE0] = edid->prod_code[0];
    eld[DISP_ELD_PRODUCT_CODE1] = edid->prod_code[1];

    if(cea_revision(cea) >= 3) 
    {
        disp_s32_t i, start, end;
        disp_s32_t sad_count;

        if(cea_db_offsets(cea, &start, &end)) 
        {
            start = 0;
            end = 0;
        }

        for_each_cea_db(cea, i, start, end) 
        {
            db = &cea[i];
            dbl = cea_db_payload_len(db);

            switch(cea_db_tag(db)) 
            {
            case AUDIO_BLOCK:
                /* Audio Data Block, contains SADs */
                sad_count = DISP_MIN(dbl / 3, 15 - total_sad_count);
                if(sad_count >= 1)
                    disp_memcpy(&eld[DISP_ELD_CEA_SAD(mnl, total_sad_count)],
                           &db[1], sad_count * 3);
                total_sad_count += sad_count;
                break;
            case SPEAKER_BLOCK:
                /* Speaker Allocation Data Block */
                if(dbl >= 1)
                    eld[DISP_ELD_SPEAKER] = db[1];
                break;
            case VENDOR_BLOCK:
                /* HDMI Vendor-Specific Data Block */
                if(cea_db_is_hdmi_vsdb(db))
                    disp_parse_hdmi_vsdb_audio(pdisp_connector, db);
                break;
            default:
                break;
            }
        }
    }
    eld[DISP_ELD_SAD_COUNT_CONN_TYPE] |= total_sad_count << DISP_ELD_SAD_COUNT_SHIFT;

    if(pdisp_connector->type == DISP_CONNECTOR_DP)
        eld[DISP_ELD_SAD_COUNT_CONN_TYPE] |= DISP_ELD_CONN_TYPE_DP;
    else
        eld[DISP_ELD_SAD_COUNT_CONN_TYPE] |= DISP_ELD_CONN_TYPE_HDMI;

    eld[DISP_ELD_BASELINE_ELD_LEN] =
        (disp_u8_t)DIV_ROUND_UP(disp_eld_calc_baseline_block_size(eld), 4);

    disp_print(DDM_TRACE, "ELD size %d, SAD count %d\n",
              disp_eld_size(eld), total_sad_count);
}

disp_u8_t disp_edid_ext_count(pedid_t edid)
{
    return edid->extensions;
}

/**
 * disp_edid_to_sad - extracts SADs from EDID
 * @edid: EDID to parse
 * @sads: pointer that will be set to the extracted SADs
 *
 * Looks for CEA EDID block and extracts SADs (Short Audio Descriptors) from it.
 *
 * Note: The returned pointer needs to be freed using kfree().
 *
 * Return: The number of found SADs or negative number on error.
 */
disp_s32_t disp_edid_to_sad(pedid_t edid, pcea_sad_t *ppsads)
{
    disp_s32_t count = 0;
    disp_s32_t i, start, end=0, dbl;
    disp_u8_t *cea;

    cea = disp_find_cea_extension(edid);
    if(!cea) 
    {
        disp_print(DDM_TRACE, "SAD: no CEA Extension found\n");
        return 0;
    }

    if(cea_revision(cea) < 3) 
    {
        disp_print(DDM_INFO,"SAD: wrong CEA revision\n");
        return 0;
    }

    if(cea_db_offsets(cea, &start, &end)) 
    {
        disp_print(DDM_INFO,"SAD: invalid data block offsets\n");
        //refine later
        //return -DISP_EPROTO;
    }

    for_each_cea_db(cea, i, start, end) 
    {
        disp_u8_t *db = &cea[i];

        if(cea_db_tag(db) == AUDIO_BLOCK) 
        {
            disp_s32_t j;

            dbl = cea_db_payload_len(db);

            count = dbl / 3; /* SAD is 3B */
            *ppsads = disp_malloc(count * sizeof(**ppsads));
            if(!*ppsads)
            {
                disp_print(DDM_ERROR, "disp_edid_to_sad, alloc sad failed\n");
                return -DISP_ENOMEM;
            }
            for(j = 0; j < count; j++) 
            {
                disp_u8_t *sad = &db[1 + j * 3];

                (*ppsads)[j].format = (sad[0] & 0x78) >> 3;
                (*ppsads)[j].channels = sad[0] & 0x7;
                (*ppsads)[j].freq = sad[1] & 0x7F;
                (*ppsads)[j].byte2 = sad[2];
            }
            break;
        }
    }

    return count;
}

/**
 * disp_edid_to_speaker_allocation - extracts Speaker Allocation Data Blocks from EDID
 * @edid: EDID to parse
 * @sadb: pointer to the speaker block
 *
 * Looks for CEA EDID block and extracts the Speaker Allocation Data Block from it.
 *
 * Note: The returned pointer needs to be freed using kfree().
 *
 * Return: The number of found Speaker Allocation Blocks or negative number on
 * error.
 */
disp_s32_t disp_edid_to_speaker_allocation(pedid_t edid, disp_u8_t **sadb)
{
    disp_s32_t count = 0;
    disp_s32_t i, start, end=0, dbl;
    disp_u8_t *cea;

    cea = disp_find_cea_extension(edid);
    if(!cea) 
    {
        disp_print(DDM_TRACE, "SAD: no CEA Extension found\n");
        return 0;
    }

    if(cea_revision(cea) < 3) 
    {
        disp_print(DDM_INFO, "SAD: wrong CEA revision\n");
        return 0;
    }

    if(cea_db_offsets(cea, &start, &end)) 
    {
        disp_print(DDM_INFO, "SAD: invalid data block offsets\n");
        //refine later
        //return -DISP_EPROTO;
    }

    for_each_cea_db(cea, i, start, end) 
    {
        disp_u8_t *db = &cea[i];

        if(cea_db_tag(db) == SPEAKER_BLOCK) 
        {
            dbl = cea_db_payload_len(db);

            /* Speaker Allocation Data Block */
            if(dbl == 3) 
            {
                //refine later
                //*sadb = kmemdup(&db[1], dbl, GFP_KERNEL);
                if(!*sadb)
                    return -DISP_ENOMEM;
                count = dbl;
                break;
            }
        }
    }

    return count;
}

/**
 * disp_av_sync_delay - compute the HDMI/DP sink audio-video sync delay
 * @connector: connector associated with the HDMI/DP sink
 * @mode: the display mode
 *
 * Return: The HDMI/DP sink's audio-video sync delay in milliseconds or 0 if
 * the sink doesn't support audio or video.
 */
disp_s32_t disp_av_sync_delay(pdisp_connector_t pdisp_connector,
              pdisp_display_mode_t mode)
{
    disp_s32_t i = !!(mode->flags & DISP_MODE_FLAG_INTERLACE);
    disp_s32_t a, v;

    if(!pdisp_connector->audio_info.latency_present[0])
        return 0;
    if(!pdisp_connector->audio_info.latency_present[1])
        i = 0;

    a = pdisp_connector->audio_info.audio_latency[i];
    v = pdisp_connector->audio_info.video_latency[i];

    /*
     * HDMI/DP sink doesn't support audio or video?
     */
    if(a == 255 || v == 255)
        return 0;

    /*
     * Convert raw EDID values to millisecond.
     * Treat unknown latency as 0ms.
     */
    if(a)
        a = DISP_MIN(2 * (a - 1), 500);
    if(v)
        v = DISP_MIN(2 * (v - 1), 500);

    return DISP_MAX(v - a, 0);
}

/**
 * disp_detect_hdmi_monitor - detect whether monitor is HDMI
 * @edid: monitor EDID information
 *
 * Parse the CEA extension according to CEA-861-B.
 *
 * Drivers that have added the modes parsed from EDID to disp_display_info
 * should use &disp_display_info.is_hdmi instead of calling this function.
 *
 * Return: True if the monitor is HDMI, false if not or unknown.
 */
disp_u8_t disp_detect_hdmi_monitor(pedid_t edid)
{
    disp_u8_t *edid_ext;
    disp_s32_t i;
    disp_s32_t start_offset, end_offset;

    edid_ext = disp_find_cea_extension(edid);
    if(!edid_ext)
        return DISP_FALSE;

    if(cea_db_offsets(edid_ext, &start_offset, &end_offset))
        return DISP_FALSE;

    /*
     * Because HDMI identifier is in Vendor Specific Block,
     * search it from all data blocks of CEA extension.
     */
    for_each_cea_db(edid_ext, i, start_offset, end_offset) 
        if(cea_db_is_hdmi_vsdb(&edid_ext[i]))
            return DISP_TRUE;

    return DISP_FALSE;
}

/**
 * disp_detect_monitor_audio - check monitor audio capability
 * @edid: EDID block to scan
 *
 * Monitor should have CEA extension block.
 * If monitor has 'basic audio', but no CEA audio blocks, it's 'basic
 * audio' only. If there is any audio extension block and supported
 * audio format, assume at least 'basic audio' support, even if 'basic
 * audio' is not defined in EDID.
 *
 * Return: True if the monitor supports audio, false otherwise.
 */
disp_u8_t disp_detect_monitor_audio(pedid_t edid)
{
    disp_u8_t *edid_ext;
    disp_s32_t i, j;
    disp_u8_t has_audio = DISP_FALSE;
    disp_s32_t start_offset, end_offset;

    edid_ext = disp_find_cea_extension(edid);
    if(!edid_ext)
        goto end;

    has_audio = ((edid_ext[3] & EDID_BASIC_AUDIO) != 0);

    if(has_audio) 
    {
        disp_print(DDM_TRACE, "Monitor has basic audio support\n");
        goto end;
    }

    if(cea_db_offsets(edid_ext, &start_offset, &end_offset))
        goto end;

    for_each_cea_db(edid_ext, i, start_offset, end_offset) 
    {
        if(cea_db_tag(&edid_ext[i]) == AUDIO_BLOCK) 
        {
            has_audio = DISP_FALSE;
            for(j = 1; j < cea_db_payload_len(&edid_ext[i]) + 1; j += 3)
                disp_print(DDM_TRACE, "CEA audio format %d\n",
                          (edid_ext[i + j] >> 3) & 0xf);
            goto end;
        }
    }
end:
    return has_audio;
}


/**
 * disp_default_rgb_quant_range - default RGB quantization range
 * @mode: display mode
 *
 * Determine the default RGB quantization range for the mode,
 * as specified in CEA-861.
 *
 * Return: The default RGB quantization range for the mode
 */
HDMI_QUANTIZATION_RANGE_TYPE disp_default_rgb_quant_range(pdisp_display_mode_t mode)
{
    /* All CEA modes other than VIC 1 use limited quantization range. */
    return disp_match_cea_mode(mode) > 1 ?
        HDMI_QUANTIZATION_RANGE_LIMITED :
        HDMI_QUANTIZATION_RANGE_FULL;
}

static disp_void_t disp_parse_vcdb(pdisp_connector_t pdisp_connector, disp_u8_t *db)
{
    pdisp_display_info_t pinfo = &pdisp_connector->disp_info;

    disp_print(DDM_TRACE, "CEA VCDB 0x%02x\n", db[2]);

    if(db[2] & EDID_CEA_VCDB_QS)
        pinfo->rgb_quant_range_selectable = DISP_TRUE;
}

static
disp_void_t disp_get_max_frl_rate(disp_s32_t max_frl_rate, disp_u8_t *max_lanes, disp_u8_t *max_rate_per_lane)
{
    switch(max_frl_rate) {
    case 1:
        *max_lanes = 3;
        *max_rate_per_lane = 3;
        break;
    case 2:
        *max_lanes = 3;
        *max_rate_per_lane = 6;
        break;
    case 3:
        *max_lanes = 4;
        *max_rate_per_lane = 6;
        break;
    case 4:
        *max_lanes = 4;
        *max_rate_per_lane = 8;
        break;
    case 5:
        *max_lanes = 4;
        *max_rate_per_lane = 10;
        break;
    case 6:
        *max_lanes = 4;
        *max_rate_per_lane = 12;
        break;
    case 0:
    default:
        *max_lanes = 0;
        *max_rate_per_lane = 0;
    }
}

static disp_void_t disp_parse_ycbcr420_deep_color_info(pdisp_connector_t pdisp_connector,
                           disp_u8_t *db)
{
    disp_u8_t dc_mask;
    pdisp_hdmi_info_t phdmi = &pdisp_connector->disp_info.hdmi;

    dc_mask = db[7] & DISP_EDID_YCBCR420_DC_MASK;
    phdmi->y420_dc_modes = dc_mask;
}

static disp_void_t disp_update_dsc_caps_by_hdmi_forum_vsdb(pdisp_connector_t pdisp_connector, disp_u8_t *hf_vsdb)
{
    pdisp_dsc_caps_t pdsc_caps = &pdisp_connector->disp_info.dsc_sink_caps;
    disp_u8_t dsc_max_slices = 0;
    disp_u32_t clk_per_slice = 0;

    // prevent re-write DP dpcd dsc analysis
    if (DISP_CONNECTOR_HDMI == pdisp_connector->type)
    {
        disp_memset(pdsc_caps, 0, sizeof(disp_dsc_caps_t));
        if (!(hf_vsdb[11] & DISP_EDID_DSC_1P2))
            return;

        pdsc_caps->flag.bits.hdmi = 1;
        pdsc_caps->flag.bits.fec_caps = 1;      // check if hdmi dsc must fec_en
        pdsc_caps->flag.bits.dsc_caps = 1;
        pdsc_caps->flag.bits.dsc_ver_1_2 = 1;   // must set dsc1.2a

        pdsc_caps->flag.bits.rgb = 1;
        pdsc_caps->flag.bits.yuv_444 = 1;
        pdsc_caps->flag.bits.yuv_simple422 = 0; // must not support simple422
        pdsc_caps->flag.bits.yuv_native422 = 1; // must support native422
        pdsc_caps->flag.bits.yuv_native420 = (hf_vsdb[11] & DISP_EDID_DSC_NATIVE_420) ? 1 : 0;
        pdsc_caps->min_increment_of_bpp = (hf_vsdb[11] & DISP_EDID_DSC_ALL_BPP) ? 0 : 4;

        pdsc_caps->flag.bits.bpc16 = (hf_vsdb[11] & DISP_EDID_DSC_16BPC) ? 1 : 0;
        pdsc_caps->flag.bits.bpc12 = (hf_vsdb[11] & DISP_EDID_DSC_12BPC) ? 1 : 0;
        pdsc_caps->flag.bits.bpc10 = (hf_vsdb[11] & DISP_EDID_DSC_10BPC) ? 1 : 0;
        pdsc_caps->flag.bits.bpc8 = 1;          // default support 8bpc

        // check how to use FRL(fixed rate link)
        pdsc_caps->dsc_max_frl_rate = (hf_vsdb[12] & DISP_EDID_DSC_MAX_FRL_RATE_MASK) >> 4;
        pdsc_caps->min_rc_buf_size_kb = hf_vsdb[13] & DISP_EDID_DSC_TOTAL_CHUNK_KBYTES;

        dsc_max_slices = hf_vsdb[12] & DISP_EDID_DSC_MAX_SLICES;
        if (dsc_max_slices >= 1)
        {
            pdsc_caps->flag.bits.slice_num_1 = 1;
            clk_per_slice = 340;
        }
        if (dsc_max_slices >= 2)
            pdsc_caps->flag.bits.slice_num_2 = 1;
        if (dsc_max_slices >= 3)
            pdsc_caps->flag.bits.slice_num_4 = 1;
        if (dsc_max_slices >= 4)
            pdsc_caps->flag.bits.slice_num_8 = 1;
        if (dsc_max_slices >= 5)
            clk_per_slice = 400;
        if (dsc_max_slices >= 6)
            pdsc_caps->flag.bits.slice_num_12 = 1;
        if (dsc_max_slices >= 7)
            pdsc_caps->flag.bits.slice_num_16 = 1;

        pdsc_caps->max_throughput_mode0_mps = clk_per_slice;
        pdsc_caps->max_throughput_mode1_mps = clk_per_slice;

        pdsc_caps->max_linebuf_depth = 13;          // must set to 13
        pdsc_caps->flag.bits.block_prediction = 1;  // must support
        pdsc_caps->max_slice_width = 2560;
        pdsc_caps->min_slice_height = 108;
    }
}

static disp_void_t disp_parse_hdmi_forum_vsdb(pdisp_connector_t pdisp_connector,
                 disp_u8_t *hf_vsdb)
{
    pdisp_display_info_t pdisplay = &pdisp_connector->disp_info;
    pdisp_hdmi_info_t phdmi = &pdisplay->hdmi;

    pdisplay->has_hdmi_infoframe = DISP_TRUE;

    if(hf_vsdb[6] & 0x80) 
    {
        phdmi->scdc.supported = DISP_TRUE;
        if(hf_vsdb[6] & 0x40)
            phdmi->scdc.read_request = DISP_TRUE;
    }

    /*
     * All HDMI 2.0 monitors must support scrambling at rates > 340 MHz.
     * And as per the spec, three factors confirm this:
     * * Availability of a HF-VSDB block in EDID (check)
     * * Non zero Max_TMDS_Char_Rate filed in HF-VSDB (let's check)
     * * SCDC support available (let's check)
     * Lets check it out.
     */

    if(hf_vsdb[5]) 
    {
        /* max clock is 5000 KHz times block value */
        disp_u32_t max_tmds_clock = hf_vsdb[5] * 5000;
        pdisp_scdc_t pscdc = &phdmi->scdc;

        if(max_tmds_clock > 340000) 
        {
            pdisplay->max_tmds_clock = max_tmds_clock;
            disp_print(DDM_TRACE, "HF-VSDB: max TMDS clock %d kHz\n",
                pdisplay->max_tmds_clock);
        }

        if(pscdc->supported) 
        {
            pscdc->scrambling.supported = DISP_TRUE;

            /* Few sinks support scrambling for clocks < 340M */
            if((hf_vsdb[6] & 0x8))
                pscdc->scrambling.low_rates = DISP_TRUE;
        }
    }

    if(hf_vsdb[7]) 
    {
        disp_u8_t max_frl_rate;
        disp_u8_t dsc_max_frl_rate;
        disp_u8_t dsc_max_slices;
        pdisp_hdmi_dsc_cap_t phdmi_dsc = &phdmi->dsc_cap;

        disp_print(DDM_TRACE, "hdmi_21 sink detected. parsing edid\n");
        max_frl_rate = (hf_vsdb[7] & DISP_EDID_MAX_FRL_RATE_MASK) >> 4;
        disp_get_max_frl_rate(max_frl_rate, &phdmi->max_lanes,
                     &phdmi->max_frl_rate_per_lane);

        /* in the future, unifiy dsc caps to disp_dsc_caps_t, remove disp_hdmi_dsc_cap_t */
        disp_update_dsc_caps_by_hdmi_forum_vsdb(pdisp_connector, hf_vsdb);

        phdmi_dsc->v_1p2 = hf_vsdb[11] & DISP_EDID_DSC_1P2;
        if(phdmi_dsc->v_1p2) 
        {
            phdmi_dsc->native_420 = hf_vsdb[11] & DISP_EDID_DSC_NATIVE_420;
            phdmi_dsc->all_bpp = hf_vsdb[11] & DISP_EDID_DSC_ALL_BPP;

            if(hf_vsdb[11] & DISP_EDID_DSC_16BPC)
                phdmi_dsc->bpc_supported = 16;
            else if(hf_vsdb[11] & DISP_EDID_DSC_12BPC)
                phdmi_dsc->bpc_supported = 12;
            else if(hf_vsdb[11] & DISP_EDID_DSC_10BPC)
                phdmi_dsc->bpc_supported = 10;
            else
                phdmi_dsc->bpc_supported = 0;

            dsc_max_frl_rate = (hf_vsdb[12] & DISP_EDID_DSC_MAX_FRL_RATE_MASK) >> 4;
            disp_get_max_frl_rate(dsc_max_frl_rate, &phdmi_dsc->max_lanes,
                         &phdmi_dsc->max_frl_rate_per_lane);
            phdmi_dsc->total_chunk_kbytes = hf_vsdb[13] & DISP_EDID_DSC_TOTAL_CHUNK_KBYTES;

            dsc_max_slices = hf_vsdb[12] & DISP_EDID_DSC_MAX_SLICES;
            switch(dsc_max_slices) 
            {
            case 1:
                phdmi_dsc->max_slices = 1;
                phdmi_dsc->clk_per_slice = 340;
                break;
            case 2:
                phdmi_dsc->max_slices = 2;
                phdmi_dsc->clk_per_slice = 340;
                break;
            case 3:
                phdmi_dsc->max_slices = 4;
                phdmi_dsc->clk_per_slice = 340;
                break;
            case 4:
                phdmi_dsc->max_slices = 8;
                phdmi_dsc->clk_per_slice = 340;
                break;
            case 5:
                phdmi_dsc->max_slices = 8;
                phdmi_dsc->clk_per_slice = 400;
                break;
            case 6:
                phdmi_dsc->max_slices = 12;
                phdmi_dsc->clk_per_slice = 400;
                break;
            case 7:
                phdmi_dsc->max_slices = 16;
                phdmi_dsc->clk_per_slice = 400;
                break;
            case 0:
            default:
                phdmi_dsc->max_slices = 0;
                phdmi_dsc->clk_per_slice = 0;
            }
        }
    }

    disp_parse_ycbcr420_deep_color_info(pdisp_connector, hf_vsdb);
}

static disp_void_t disp_parse_hdmi_deep_color_info(pdisp_connector_t pdisp_connector,
                       disp_u8_t *hdmi)
{
    pdisp_display_info_t pinfo = &pdisp_connector->disp_info;
    disp_u32_t dc_bpc = 0;

    /* HDMI supports at least 8 bpc */
    if (pinfo->bpc < 8)
        pinfo->bpc = 8;

    if(cea_db_payload_len(hdmi) < 6)
        return;

    if(hdmi[6] & DISP_EDID_HDMI_DC_30) 
    {
        dc_bpc = 10;
        pinfo->edid_hdmi_dc_modes |= DISP_EDID_HDMI_DC_30;
        disp_print(DDM_TRACE, "HDMI sink does deep color 30.\n");
    }

    if(hdmi[6] & DISP_EDID_HDMI_DC_36) 
    {
        dc_bpc = 12;
        pinfo->edid_hdmi_dc_modes |= DISP_EDID_HDMI_DC_36;
        disp_print(DDM_TRACE, "HDMI sink does deep color 36.\n");
    }

    if(hdmi[6] & DISP_EDID_HDMI_DC_48) 
    {
        dc_bpc = 16;
        pinfo->edid_hdmi_dc_modes |= DISP_EDID_HDMI_DC_48;
        disp_print(DDM_TRACE, "HDMI sink does deep color 48.\n");
    }

    if(dc_bpc == 0) 
    {
        disp_print(DDM_INFO, "No deep color support on this HDMI sink.\n");
        return;
    }

    disp_print(DDM_TRACE, "Assigning HDMI sink color depth as %d bpc.\n", dc_bpc);
    if (pinfo->bpc < dc_bpc)
        pinfo->bpc = dc_bpc;

    /*
     * Deep color support mandates RGB444 support for all video
     * modes and forbids YCRCB422 support for all video modes per
     * HDMI 1.3 spec.
     */
    pinfo->color_formats |= DISP_COLOR_FORMAT_RGB444;

    /* YCRCB444 is optional according to spec. */
    if(hdmi[6] & DISP_EDID_HDMI_DC_Y444) 
    {
        pinfo->color_formats |= DISP_COLOR_FORMAT_YCRCB444;
        disp_print(DDM_TRACE, "HDMI sink does YCRCB444 in deep color.\n");
    }

    /*
     * Spec says that if any deep color mode is supported at all,
     * then deep color 36 bit must be supported.
     */
    if(!(hdmi[6] & DISP_EDID_HDMI_DC_36)) 
    {
        disp_print(DDM_WARNING, "HDMI sink should do DC_36, but does not!\n");
    }
}

static disp_void_t
disp_parse_hdmi_vsdb_video(pdisp_connector_t pdisp_connector, disp_u8_t *db)
{
    pdisp_display_info_t pinfo = &pdisp_connector->disp_info;
    disp_u8_t len = cea_db_payload_len(db);

    pinfo->is_hdmi = DISP_TRUE;

    if(len >= 6)
        pinfo->dvi_dual = db[6] & 1;
    if(len >= 7)
        pinfo->max_tmds_clock = db[7] * 5000;

    disp_print(DDM_TRACE, "HDMI: DVI dual %d, "
              "max TMDS clock %d kHz\n",
              pinfo->dvi_dual,
              pinfo->max_tmds_clock);

    disp_parse_hdmi_deep_color_info(pdisp_connector, db);
}

static disp_void_t disp_parse_cea_ext(pdisp_connector_t pdisp_connector,
                  pedid_t edid)
{
    pdisp_display_info_t pinfo = &pdisp_connector->disp_info;
    disp_u8_t *edid_ext;
    disp_s32_t i, start, end;

    edid_ext = disp_find_cea_extension(edid);
    if(!edid_ext)
        return;

    pinfo->cea_rev = edid_ext[1];

    /* The existence of a CEA block should imply RGB support */
    pinfo->color_formats |= DISP_COLOR_FORMAT_RGB444;
    if(edid_ext[3] & EDID_CEA_YCRCB444)
        pinfo->color_formats |= DISP_COLOR_FORMAT_YCRCB444;
    if(edid_ext[3] & EDID_CEA_YCRCB422)
        pinfo->color_formats |= DISP_COLOR_FORMAT_YCRCB422;

    if(cea_db_offsets(edid_ext, &start, &end))
        return;

    for_each_cea_db(edid_ext, i, start, end) 
    {
        disp_u8_t *db = &edid_ext[i];

        if(cea_db_is_hdmi_vsdb(db))
            disp_parse_hdmi_vsdb_video(pdisp_connector, db);
        if(cea_db_is_hdmi_forum_vsdb(db))
            disp_parse_hdmi_forum_vsdb(pdisp_connector, db);
        if(cea_db_is_y420cmdb(db))
            disp_parse_y420cmdb_bitmap(pdisp_connector, db);
        if(cea_db_is_vcdb(db))
            disp_parse_vcdb(pdisp_connector, db);
        if(cea_db_is_hdmi_hdr_metadata_block(db))
            disp_parse_hdr_metadata_block(pdisp_connector, db);
        if(cea_db_is_colorimetry(db))
            disp_parse_colorimetry_block(pdisp_connector, db);
    }
}

static
disp_void_t get_monitor_range(pdetailed_timing_t timing,
               disp_void_t *info_monitor_range)
{
    pdisp_monitor_range_info_t pmonitor_range = info_monitor_range;
    pdetailed_non_pixel_t pdata = &timing->data.other_data;
    pdetailed_data_monitor_range_t prange = &pdata->data.range;
    disp_u16_t min_vf_addition = 0, max_vf_addition = 0, min_hf_addition = 0, max_hf_addition = 0;

    if(!is_display_descriptor((disp_u8_t *)timing, EDID_DETAIL_MONITOR_RANGE))
        return;

    /*
     * Check for flag range limits only. If flag == 1 then
     * no additional timing information provided.
     * Default GTF, GTF Secondary curve and CVT are not
     * supported
     */
    if(prange->flags != DISP_EDID_RANGE_LIMITS_ONLY_FLAG)
        return;

    // tofix: analysis detailed_data_monitor_range_t later

    min_vf_addition = (pdata->pad2 & 0x1) ? 255 : 0;
    max_vf_addition = (pdata->pad2 & 0x2) ? 255 : 0;
    min_hf_addition = (pdata->pad2 & 0x4) ? 255 : 0;
    max_hf_addition = (pdata->pad2 & 0x8) ? 255 : 0;

    pmonitor_range->min_vfreq = prange->min_vfreq + min_vf_addition;
    pmonitor_range->max_vfreq = prange->max_vfreq + max_vf_addition;
    pmonitor_range->min_hfreq_khz = prange->min_hfreq_khz + min_hf_addition;
    pmonitor_range->max_hfreq_khz = prange->max_hfreq_khz + max_hf_addition;
    pmonitor_range->max_pixel_clock = (disp_s32_t)prange->pixel_clock_mhz * 10000;
}

static
disp_void_t disp_get_monitor_range(pdisp_connector_t pdisp_connector,
               pedid_t edid)
{
    pdisp_display_info_t pinfo = &pdisp_connector->disp_info;

    if(!version_greater(edid, 1, 1))
        return;

    disp_for_each_detailed_block((disp_u8_t *)edid, get_monitor_range,
                    &pinfo->monitor_range);

    disp_print(DDM_TRACE, "Supported Monitor Refresh rate range is %d Hz - %d Hz, max_pixel_clock: %d khz\n",
        pinfo->monitor_range.min_vfreq, pinfo->monitor_range.max_vfreq, pinfo->monitor_range.max_pixel_clock);
}

/* A connector has no EDID information, so we've got no EDID to compute quirks from. Reset
 * all of the values which would have been set from EDID
 */
disp_void_t
disp_reset_display_info(pdisp_connector_t pdisp_connector)
{
    pdisp_display_info_t pinfo = &pdisp_connector->disp_info;

    pinfo->width_mm = 0;
    pinfo->height_mm = 0;

    pinfo->bpc = 0;
    pinfo->color_formats = 0;
    pinfo->cea_rev = 0;
    pinfo->max_tmds_clock = 0;
    pinfo->dvi_dual = DISP_FALSE;
    pinfo->is_hdmi = DISP_FALSE;
    pinfo->has_hdmi_infoframe = DISP_FALSE;
    pinfo->rgb_quant_range_selectable = DISP_FALSE;
    disp_memset(&pinfo->hdmi, 0, sizeof(pinfo->hdmi));

    pinfo->non_desktop = 0;
    disp_memset(&pinfo->monitor_range, 0, sizeof(pinfo->monitor_range));
}

disp_u32_t disp_add_display_info(pdisp_connector_t pdisp_connector, pedid_t edid)
{
    pdisp_display_info_t pinfo = &pdisp_connector->disp_info;

    disp_u32_t quirks = edid_get_quirks(edid);

    disp_reset_display_info(pdisp_connector);

    pinfo->width_mm = edid->width_cm * 10;
    pinfo->height_mm = edid->height_cm * 10;
    pinfo->rgbw_position[0].x = ((disp_u16_t)edid->red_x << 2) | ((edid->red_green_lo & 0xc0) >> 6);
    pinfo->rgbw_position[0].y = ((disp_u16_t)edid->red_y << 2) | ((edid->red_green_lo & 0x30) >> 4);
    pinfo->rgbw_position[1].x = ((disp_u16_t)edid->green_x << 2) | ((edid->red_green_lo & 0x0c) >> 2);
    pinfo->rgbw_position[1].y = ((disp_u16_t)edid->green_y << 2) | (edid->red_green_lo & 0x03);
    pinfo->rgbw_position[2].x = ((disp_u16_t)edid->blue_x << 2) | ((edid->blue_white_lo & 0xc0) >> 6);
    pinfo->rgbw_position[2].y = ((disp_u16_t)edid->blue_y << 2) | ((edid->blue_white_lo & 0x30) >> 4);
    pinfo->rgbw_position[3].x = ((disp_u16_t)edid->white_x << 2) | ((edid->blue_white_lo & 0x0c) >> 2);
    pinfo->rgbw_position[3].y = ((disp_u16_t)edid->white_y << 2) | (edid->blue_white_lo & 0x03);

    pinfo->quirks = quirks;
    pinfo->non_desktop = !!(quirks & EDID_QUIRK_NON_DESKTOP);

    disp_get_monitor_range(pdisp_connector, edid);

    disp_print(DDM_TRACE, "non_desktop set to %d\n", pinfo->non_desktop);

    if(edid->revision < 3)
        return quirks;

    if(!(edid->input & DISP_EDID_INPUT_DIGITAL))
        return quirks;

    disp_parse_cea_ext(pdisp_connector, edid);

    /*
     * Digital sink with "DFP 1.x compliant TMDS" according to EDID 1.3?
     *
     * For such displays, the DFP spec 1.0, section 3.10 "EDID support"
     * tells us to assume 8 bpc color depth if the EDID doesn't have
     * extensions which tell otherwise.
     */
    if(pinfo->bpc == 0 && edid->revision == 3 &&
        edid->input & DISP_EDID_DIGITAL_DFP_1_X) 
    {
        pinfo->bpc = 8;
        disp_print(DDM_TRACE, "Assigning DFP sink color depth as %d bpc.\n", pinfo->bpc);
    }

    /* Only defined for 1.4 with digital displays */
    if(edid->revision < 4)
        return quirks;

    switch(edid->input & DISP_EDID_DIGITAL_DEPTH_MASK) 
    {
    case DISP_EDID_DIGITAL_DEPTH_6:
        pinfo->bpc = 6;
        break;
    case DISP_EDID_DIGITAL_DEPTH_8:
        pinfo->bpc = 8;
        break;
    case DISP_EDID_DIGITAL_DEPTH_10:
        pinfo->bpc = 10;
        break;
    case DISP_EDID_DIGITAL_DEPTH_12:
        pinfo->bpc = 12;
        break;
    case DISP_EDID_DIGITAL_DEPTH_14:
        pinfo->bpc = 14;
        break;
    case DISP_EDID_DIGITAL_DEPTH_16:
        pinfo->bpc = 16;
        break;
    case DISP_EDID_DIGITAL_DEPTH_UNDEF:
    default:
        pinfo->bpc = 0;
        break;
    }

    disp_print(DDM_TRACE, "Assigning EDID-1.4 digital sink color depth as %d bpc.\n", pinfo->bpc);

    pinfo->color_formats |= DISP_COLOR_FORMAT_RGB444;
    if(edid->features & DISP_EDID_FEATURE_RGB_YCRCB444)
        pinfo->color_formats |= DISP_COLOR_FORMAT_YCRCB444;
    if(edid->features & DISP_EDID_FEATURE_RGB_YCRCB422)
        pinfo->color_formats |= DISP_COLOR_FORMAT_YCRCB422;
    return quirks;
}

static disp_void_t disp_modes_color_info_dump(pdisp_display_info_t pinfo, disp_u32_t level)
{
    disp_u32_t prt_tmp = 0;

    disp_print(level, "color RGBWxy: R[%#x, %#x] G[%#x, %#x] B[%#x, %#x] W[%#x, %#x]\n",
        pinfo->rgbw_position[0].x, pinfo->rgbw_position[0].y, pinfo->rgbw_position[1].x, pinfo->rgbw_position[1].y,
        pinfo->rgbw_position[2].x, pinfo->rgbw_position[2].y, pinfo->rgbw_position[3].x, pinfo->rgbw_position[3].y);

    disp_print(level, "hdr, type[%d], eotf[%u] metaType[%u], Luminance[%u, %u] MaxLumAverage[%u], colorimetry[%#x]\n",
        pinfo->hdr_sink_metadata.metadata_type, pinfo->hdr_sink_metadata.data.hdmi_type1.eotf, pinfo->hdr_sink_metadata.data.hdmi_type1.metadata_type,
        pinfo->hdr_sink_metadata.data.hdmi_type1.min_cll, pinfo->hdr_sink_metadata.data.hdmi_type1.max_cll,
        pinfo->hdr_sink_metadata.data.hdmi_type1.max_fall, pinfo->colorimetry_support);

    prt_tmp = pinfo->hdr_sink_metadata.data.hdmi_type1.eotf;
    disp_print(level, "hdr-eotf support: %s%s%s%s\n",
        (prt_tmp & BIT(DISP_EOTF_TRADITIONAL_GAMMA_SDR)) ? "SDR, " : "",
        (prt_tmp & BIT(DISP_EOTF_TRADITIONAL_GAMMA_HDR)) ? "HDR, " : "",
        (prt_tmp & BIT(DISP_EOTF_SMPTE_ST2084)) ? "smpte-st2084, " : "",
        (prt_tmp & BIT(DISP_EOTF_BT_2100_HLG)) ? "bt-2100-hlg, " : "");

    prt_tmp = pinfo->colorimetry_support;
    disp_print(level, "hdr-colorimetry support: %s%s%s%s%s%s%s%s%s%s%s%s%s%s\n",
        (prt_tmp & BIT(DISP_COLORIMETRY_SRGB)) ? "sRGB, " : "",
        (prt_tmp & BIT(DISP_COLORIMETRY_RGB_ADOBE)) ? "RGBadobe, " : "",
        (prt_tmp & BIT(DISP_COLORIMETRY_RGB_BT2020)) ? "RGBbt2020, " : "",
        (prt_tmp & BIT(DISP_COLORIMETRY_RGB_DCI)) ? "DCI-none, " : "",
        (prt_tmp & BIT(DISP_COLORIMETRY_RGB_DCI_D65)) ? "DCI-P3-d65, " : "",
        (prt_tmp & BIT(DISP_COLORIMETRY_YCC_NONE)) ? "YCCnone, " : "",
        (prt_tmp & BIT(DISP_COLORIMETRY_YCC_SMPTE_170M)) ? "YCC-smpte-170m, " : "",
        (prt_tmp & BIT(DISP_COLORIMETRY_YCC_BT709)) ? "YCCbt709, " : "",
        (prt_tmp & BIT(DISP_COLORIMETRY_XVYCC_601)) ? "YCCxv601, " : "",
        (prt_tmp & BIT(DISP_COLORIMETRY_XVYCC_709)) ? "YCCxv709, " : "",
        (prt_tmp & BIT(DISP_COLORIMETRY_SYCC_601)) ? "sYCC601, " : "",
        (prt_tmp & BIT(DISP_COLORIMETRY_YCC_ADOBE_601)) ? "YCCadobe601, " : "",
        (prt_tmp & BIT(DISP_COLORIMETRY_CYCC_BT2020)) ? "bt2020cycc, " : "",
        (prt_tmp & BIT(DISP_COLORIMETRY_YCC_BT2020)) ? "bt2020ycc, " : "");
}

static disp_void_t disp_mode_displayid_detailed(
    displayid_detailed_timings_1_t *ptimings, disp_u8_t type_7, pdisp_display_mode_t pmode)
{
    disp_u32_t pixel_clock = (ptimings->pixel_clock[0] |
        (ptimings->pixel_clock[1] << 8) |
        (ptimings->pixel_clock[2] << 16)) + 1;
    disp_u32_t hactive = (ptimings->hactive[0] | ptimings->hactive[1] << 8) + 1;
    disp_u32_t hblank = (ptimings->hblank[0] | ptimings->hblank[1] << 8) + 1;
    disp_u32_t hsync = (ptimings->hsync[0] | (ptimings->hsync[1] & 0x7f) << 8) + 1;
    disp_u32_t hsync_width = (ptimings->hsw[0] | ptimings->hsw[1] << 8) + 1;
    disp_u32_t vactive = (ptimings->vactive[0] | ptimings->vactive[1] << 8) + 1;
    disp_u32_t vblank = (ptimings->vblank[0] | ptimings->vblank[1] << 8) + 1;
    disp_u32_t vsync = (ptimings->vsync[0] | (ptimings->vsync[1] & 0x7f) << 8) + 1;
    disp_u32_t vsync_width = (ptimings->vsw[0] | ptimings->vsw[1] << 8) + 1;
    disp_u8_t hsync_positive = (ptimings->hsync[1] >> 7) & 0x1;
    disp_u8_t vsync_positive = (ptimings->vsync[1] >> 7) & 0x1;

    /* resolution is kHz for type VII, and 10 kHz for type I */
    pmode->clock = type_7 ? pixel_clock : pixel_clock * 10;
    pmode->hdisplay = (disp_u16_t)hactive;
    pmode->hsync_start = (disp_u16_t)(pmode->hdisplay + hsync);
    pmode->hsync_end = (disp_u16_t)(pmode->hsync_start + hsync_width);
    pmode->htotal = (disp_u16_t)(pmode->hdisplay + hblank);

    pmode->vdisplay = (disp_u16_t)vactive;
    pmode->vsync_start = (disp_u16_t)(pmode->vdisplay + vsync);
    pmode->vsync_end = (disp_u16_t)(pmode->vsync_start + vsync_width);
    pmode->vtotal = (disp_u16_t)(pmode->vdisplay + vblank);

    pmode->flags = 0;
    pmode->flags |= hsync_positive ? DISP_MODE_FLAG_PHSYNC : DISP_MODE_FLAG_NHSYNC;
    pmode->flags |= vsync_positive ? DISP_MODE_FLAG_PVSYNC : DISP_MODE_FLAG_NVSYNC;
    pmode->type = DISP_MODE_TYPE_DRIVER;

    if (ptimings->flags & 0x80)
        pmode->type |= DISP_MODE_TYPE_PREFERRED;
    disp_mode_set_name(pmode);
}

static disp_s32_t add_displayid_detailed_1_modes(pdisp_connector_t pconnector, const displayid_block_t *block)
{
    displayid_detailed_timing_block_t *det = (displayid_detailed_timing_block_t *)block;
    disp_s32_t idx = 0, num_modes = 0, num_timings = 0;
    disp_display_mode_t newmode = {0};
    disp_u8_t type_7 = block->tag == DATA_BLOCK_2_TYPE_7_DETAILED_TIMING;
    displayid_detailed_timings_1_t *ptimings = DISP_NULL;

    /* blocks must be multiple of 20 bytes length */
    if (block->num_bytes % 20)
        return 0;

    num_timings = block->num_bytes / 20;
    for (idx = 0; idx < num_timings; idx++) {
        ptimings = &det->timings[0] + idx;

        disp_mode_displayid_detailed(ptimings, type_7, &newmode);
        if (disp_mode_probed_add(pconnector, &newmode, DISP_FALSE))
            num_modes++;
    }
    return num_modes;
}

static disp_s32_t add_displayid_detailed_modes(pdisp_connector_t pconnector, pedid_t pedid)
{
    const displayid_block_t *block;
    displayid_iter_t iter;
    disp_edid_t disp_edid;
    disp_s32_t num_modes = 0;

    disp_edid.edid = pedid;
    disp_edid.size = (disp_u64_t)(disp_edid_ext_count(pedid) + 1) * EDID_LENGTH;

    displayid_iter_edid_begin(&disp_edid, &iter);
    displayid_iter_for_each(block, &iter) {
        if (block->tag == DATA_BLOCK_TYPE_1_DETAILED_TIMING ||
            block->tag == DATA_BLOCK_2_TYPE_7_DETAILED_TIMING)
            num_modes += add_displayid_detailed_1_modes(pconnector, block);
    }
    displayid_iter_end(&iter);

    return num_modes;
}

static pdisp_display_mode_t disp_find_max_mode(pdisp_connector_t pdisp_connector)
{
    pdisp_display_mode_t pmax_mode = DISP_NULL;
    disp_s32_t idx = 0;
    disp_s32_t vfresh = 0;

    if (pdisp_connector->mode_list.num)
    {
        pmax_mode = &pdisp_connector->mode_list.modes[0];
        vfresh = disp_mode_vrefresh(pmax_mode);

        for (idx = 1; idx < pdisp_connector->mode_list.num; idx++)
        {
            if (pmax_mode->hdisplay < pdisp_connector->mode_list.modes[idx].hdisplay &&
                pmax_mode->vdisplay < pdisp_connector->mode_list.modes[idx].vdisplay)
            {
                pmax_mode = &pdisp_connector->mode_list.modes[idx];
                vfresh = disp_mode_vrefresh(pmax_mode);
            }
            else if (pmax_mode->hdisplay == pdisp_connector->mode_list.modes[idx].hdisplay &&
                pmax_mode->vdisplay == pdisp_connector->mode_list.modes[idx].vdisplay &&
                vfresh != 60 && vfresh < disp_mode_vrefresh(&pdisp_connector->mode_list.modes[idx]))
            {   // Same wh, prefer 60 Hz mode, If not have, prefer max fresh mode.
                pmax_mode = &pdisp_connector->mode_list.modes[idx];
                vfresh = disp_mode_vrefresh(pmax_mode);
            }
        }
    }

    return pmax_mode;
}

disp_s32_t disp_add_edid_info(pdisp_connector_t pdisp_connector, pedid_t edid)
{
    disp_u32_t quirks;

    if(edid == DISP_NULL)
    {
        clear_eld(pdisp_connector);
        return -1;
    }
    if(!disp_edid_is_valid(edid))
    {
        clear_eld(pdisp_connector);
        disp_print(DDM_INFO, "connector %d EDID invalid\n", pdisp_connector->index);
        return -1;
    }

    disp_edid_to_eld(pdisp_connector, edid);
    quirks = disp_add_display_info(pdisp_connector, edid);

    if(quirks & EDID_QUIRK_FORCE_6BPC)
        pdisp_connector->disp_info.bpc = 6;
    if(quirks & EDID_QUIRK_FORCE_8BPC)
        pdisp_connector->disp_info.bpc = 8;
    if(quirks & EDID_QUIRK_FORCE_10BPC)
        pdisp_connector->disp_info.bpc = 10;
    if(quirks & EDID_QUIRK_FORCE_12BPC)
        pdisp_connector->disp_info.bpc = 12;

    if (!pdisp_connector->disp_info.bpc)
    {
        disp_print(DDM_KEY, "disp_add_edid_modes, unexpected 0 bpc, force 8\n");
        pdisp_connector->disp_info.bpc = 8;
    }

    disp_modes_color_info_dump(&pdisp_connector->disp_info, DDM_TRACE);
    return 0;
}

/**
 * disp_add_edid_modes - add modes from EDID data, if available
 * @connector: connector we're probing
 * @edid: EDID data
 *
 * Add the specified modes to the connector's mode list. Also fills out the
 * &disp_display_info structure and ELD in @connector with any information which
 * can be derived from the edid.
 *
 * Return: The number of modes added or 0 if we couldn't find any.
 */
disp_s32_t disp_add_edid_modes(pdisp_connector_t pdisp_connector, pedid_t edid, disp_u8_t add_mode_to_modelist)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_connector->disp_device;
    disp_s32_t num_modes = 0;

    pdisp_connector->flag.info.add_mode_to_modelist = add_mode_to_modelist ? 1 : 0;
    num_modes += add_detailed_modes(pdisp_connector, edid, pdisp_connector->disp_info.quirks);
    num_modes += add_cvt_modes(pdisp_connector, edid);
    num_modes += add_standard_modes(pdisp_connector, edid);
    num_modes += add_established_modes(pdisp_connector, edid);
    num_modes += add_cea_modes(pdisp_connector, edid);
    //num_modes += add_alternate_cea_modes(pdisp_connector, edid);
    num_modes += add_displayid_detailed_modes(pdisp_connector, edid);

    if (pdisp_connector->flag.info.cheater)
    {
        num_modes += disp_mode_probed_add(pdisp_connector, cea_mode_for_vic(16), DISP_FALSE); // 1k  60Hz
        num_modes += disp_mode_probed_add(pdisp_connector, cea_mode_for_vic(97), DISP_FALSE); // 4k  60Hz
    }

    if (pdd->flag.info.mode_8k)
    {
        num_modes += disp_mode_probed_add(pdisp_connector, cea_mode_for_vic(123), DISP_FALSE); // 5k  30Hz
        num_modes += disp_mode_probed_add(pdisp_connector, cea_mode_for_vic(126), DISP_FALSE); // 5k  60Hz
        num_modes += disp_mode_probed_add(pdisp_connector, cea_mode_for_vic(127), DISP_FALSE); // 5k 100Hz
        num_modes += disp_mode_probed_add(pdisp_connector, cea_mode_for_vic(193), DISP_FALSE); // 5k 120Hz
        num_modes += disp_mode_probed_add(pdisp_connector, cea_mode_for_vic(196), DISP_FALSE); // 8k  30Hz
        num_modes += disp_mode_probed_add(pdisp_connector, cea_mode_for_vic(199), DISP_FALSE); // 8k  60Hz
        num_modes += disp_mode_probed_add(pdisp_connector, cea_mode_for_vic(200), DISP_FALSE); // 8k 100Hz
        num_modes += disp_mode_probed_add(pdisp_connector, cea_mode_for_vic(201), DISP_FALSE); // 8k 120Hz
    }

    return num_modes;
}

disp_s32_t disp_add_edid_modes_extension(pdisp_connector_t pdisp_connector, pedid_t edid)
{
    pdisp_display_mode_t p_base_modes = DISP_NULL;
    pdisp_display_mode_t p_native_mode = DISP_NULL;
    disp_s32_t common_modes_cnt = 0, ext_modes_cnt = 0, base_modes_cnt = 0;
    disp_s32_t idx = 0;
    disp_u32_t level = DDM_TRACE;

    if (!pdisp_connector->native_mode)
    {
        disp_print(DDM_INFO, "Not found native mode. conn[%d], use max wh mode instead.\n", pdisp_connector->index);
        pdisp_connector->native_mode = disp_find_max_mode(pdisp_connector);
    }

    base_modes_cnt = pdisp_connector->mode_list.num;
    p_base_modes = pdisp_connector->mode_list.modes;
    p_native_mode = pdisp_connector->native_mode;
    if(edid->features & DISP_EDID_FEATURE_DEFAULT_GTF)
    {
        pdisp_connector->flag.info.add_mode_to_modelist = 0;
        ext_modes_cnt += add_inferred_modes(pdisp_connector, edid);

        if (ext_modes_cnt)
        {
            pdisp_connector->mode_list.modes = (pdisp_display_mode_t)disp_malloc(
                (base_modes_cnt + ext_modes_cnt) * sizeof(disp_display_mode_t));
            if (!pdisp_connector->mode_list.modes)
            {
                disp_print(DDM_INFO, "disp_add_edid_modes_extension, malloc ext_modes failed!\n");
                return 0;
            }
            disp_memcpy(pdisp_connector->mode_list.modes, p_base_modes, base_modes_cnt * sizeof(disp_display_mode_t));
            pdisp_connector->mode_list.num = base_modes_cnt;
            pdisp_connector->flag.info.add_mode_to_modelist = 1;
            add_inferred_modes(pdisp_connector, edid);

            pdisp_connector->native_mode = p_native_mode ? (pdisp_connector->mode_list.modes + (p_native_mode - p_base_modes)) : DISP_NULL;

            disp_free(p_base_modes);
            p_base_modes = DISP_NULL;
        }
    }

    base_modes_cnt = pdisp_connector->mode_list.num;
    p_base_modes = pdisp_connector->mode_list.modes;
    p_native_mode = pdisp_connector->native_mode;
    pdisp_connector->flag.info.add_mode_to_modelist = 0;
    common_modes_cnt = add_common_modes(pdisp_connector);
    if (common_modes_cnt)
    {
        pdisp_connector->mode_list.modes = (pdisp_display_mode_t)disp_malloc(
            (base_modes_cnt + common_modes_cnt) * sizeof(disp_display_mode_t));
        if (!pdisp_connector->mode_list.modes)
        {
            disp_print(DDM_INFO, "disp_add_edid_modes_extension, malloc common_modes failed!\n");
            return 0;
        }
        disp_memcpy(pdisp_connector->mode_list.modes, p_base_modes, base_modes_cnt * sizeof(disp_display_mode_t));
        pdisp_connector->mode_list.num = base_modes_cnt;
        pdisp_connector->flag.info.add_mode_to_modelist = 1;
        add_common_modes(pdisp_connector);

        pdisp_connector->native_mode = p_native_mode ? (pdisp_connector->mode_list.modes + (p_native_mode - p_base_modes)) : DISP_NULL;

        disp_free(p_base_modes);
        p_base_modes = DISP_NULL;
    }

    disp_print(level, "disp_add_edid_modes_extension, cnt: ext[%d] common[%d] all[%d]\n",
        ext_modes_cnt, common_modes_cnt, pdisp_connector->mode_list.num);

    if (!pdisp_connector->disp_info.color_formats)
    {   /* Some legacy monitor may support YUVxxx only, so make sure no valid format before add RGB. */
        disp_print(DDM_INFO, "disp_add_edid_modes_extension, legacy edid no valid format, force RGB.\n");
        pdisp_connector->disp_info.color_formats |= DISP_COLOR_FORMAT_RGB444;
    }

    for (idx = 0; idx < pdisp_connector->mode_list.num; idx++)
    {
        disp_print_pure(level, "\t%u: ", idx);
        disp_mode_debug_printmodeline(&pdisp_connector->mode_list.modes[idx], level);
    }

    return pdisp_connector->mode_list.num;
}
