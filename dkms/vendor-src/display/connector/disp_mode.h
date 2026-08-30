/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __DISP_MODE_H__
#define __DISP_MODE_H__

#include "display.h"
#include "disp_device.h"

/*
 * Note on terminology:  here, for brevity and convenience, we refer to connector
 * control chips as 'CRTCs'.  They can control any type of connector, VGA, LVDS,
 * DVI, etc.  And 'screen' refers to the whole of the visible display, which
 * may span multiple monitors (and therefore multiple CRTC and connector
 * structures).
 */

/**
 * enum DISP_MODE_STATUS - hardware support status of a mode
 * @MODE_OK: Mode OK
 * @MODE_HSYNC: hsync out of range
 * @MODE_VSYNC: vsync out of range
 * @MODE_H_ILLEGAL: mode has illegal horizontal timings
 * @MODE_V_ILLEGAL: mode has illegal vertical timings
 * @MODE_BAD_WIDTH: requires an unsupported linepitch
 * @MODE_NOMODE: no mode with a matching name
 * @MODE_NO_INTERLACE: interlaced mode not supported
 * @MODE_NO_DBLESCAN: doublescan mode not supported
 * @MODE_NO_VSCAN: multiscan mode not supported
 * @MODE_MEM: insufficient video memory
 * @MODE_VIRTUAL_X: mode width too large for specified virtual size
 * @MODE_VIRTUAL_Y: mode height too large for specified virtual size
 * @MODE_MEM_VIRT: insufficient video memory given virtual size
 * @MODE_NOCLOCK: no fixed clock available
 * @MODE_CLOCK_HIGH: clock required is too high
 * @MODE_CLOCK_LOW: clock required is too low
 * @MODE_CLOCK_RANGE: clock/mode isn't in a ClockRange
 * @MODE_BAD_HVALUE: horizontal timing was out of range
 * @MODE_BAD_VVALUE: vertical timing was out of range
 * @MODE_BAD_VSCAN: VScan value out of range
 * @MODE_HSYNC_NARROW: horizontal sync too narrow
 * @MODE_HSYNC_WIDE: horizontal sync too wide
 * @MODE_HBLANK_NARROW: horizontal blanking too narrow
 * @MODE_HBLANK_WIDE: horizontal blanking too wide
 * @MODE_VSYNC_NARROW: vertical sync too narrow
 * @MODE_VSYNC_WIDE: vertical sync too wide
 * @MODE_VBLANK_NARROW: vertical blanking too narrow
 * @MODE_VBLANK_WIDE: vertical blanking too wide
 * @MODE_PANEL: exceeds panel dimensions
 * @MODE_INTERLACE_WIDTH: width too large for interlaced mode
 * @MODE_ONE_WIDTH: only one width is supported
 * @MODE_ONE_HEIGHT: only one height is supported
 * @MODE_ONE_SIZE: only one resolution is supported
 * @MODE_NO_REDUCED: monitor doesn't accept reduced blanking
 * @MODE_NO_STEREO: stereo modes not supported
 * @MODE_NO_420: ycbcr 420 modes not supported
 * @MODE_STALE: mode has become stale
 * @MODE_BAD: unspecified reason
 * @MODE_ERROR: error condition
 *
 * This enum is used to filter out modes not supported by the driver/hardware
 * combination.
 */
typedef enum _DISP_MODE_STATUS 
{
    MODE_OK = 0,
    MODE_HSYNC,
    MODE_VSYNC,
    MODE_H_ILLEGAL,
    MODE_V_ILLEGAL,
    MODE_BAD_WIDTH,
    MODE_NOMODE,
    MODE_NO_INTERLACE,
    MODE_NO_DBLESCAN,
    MODE_NO_VSCAN,
    MODE_MEM,
    MODE_VIRTUAL_X,
    MODE_VIRTUAL_Y,
    MODE_MEM_VIRT,
    MODE_NOCLOCK,
    MODE_CLOCK_HIGH,
    MODE_CLOCK_LOW,
    MODE_CLOCK_RANGE,
    MODE_BAD_HVALUE,
    MODE_BAD_VVALUE,
    MODE_BAD_VSCAN,
    MODE_HSYNC_NARROW,
    MODE_HSYNC_WIDE,
    MODE_HBLANK_NARROW,
    MODE_HBLANK_WIDE,
    MODE_VSYNC_NARROW,
    MODE_VSYNC_WIDE,
    MODE_VBLANK_NARROW,
    MODE_VBLANK_WIDE,
    MODE_PANEL,
    MODE_INTERLACE_WIDTH,
    MODE_ONE_WIDTH,
    MODE_ONE_HEIGHT,
    MODE_ONE_SIZE,
    MODE_NO_REDUCED,
    MODE_NO_STEREO,
    MODE_NO_420,
    MODE_STALE = -3,
    MODE_BAD = -2,
    MODE_ERROR = -1
}DISP_MODE_STATUS;

#define DISP_MODE(nm, t, c, hd, hss, hse, ht, hsk, vd, vss, vse, vt, vs, f) \
    .name = nm, .status = 0, .type = (t), .clock = (c), \
    .hdisplay = (hd), .hsync_start = (hss), .hsync_end = (hse), \
    .htotal = (ht), .hskew = (hsk), .vdisplay = (vd), \
    .vsync_start = (vss), .vsync_end = (vse), .vtotal = (vt), \
    .vscan = (vs), .flags = (f)

/**
 * DISP_SIMPLE_MODE - Simple display mode
 * @hd: Horizontal resolution, width
 * @vd: Vertical resolution, height
 * @hd_mm: Display width in millimeters
 * @vd_mm: Display height in millimeters
 *
 * This macro initializes a &disp_display_mode_t that only contains info about
 * resolution and physical size.
 */
#define DISP_SIMPLE_MODE(hd, vd, hd_mm, vd_mm) \
    .type = DISP_MODE_TYPE_DRIVER, .clock = 1 /* pass validation */, \
    .hdisplay = (hd), .hsync_start = (hd), .hsync_end = (hd), \
    .htotal = (hd), .vdisplay = (vd), .vsync_start = (vd), \
    .vsync_end = (vd), .vtotal = (vd), .width_mm = (hd_mm), \
    .height_mm = (vd_mm)

#define CRTC_INTERLACE_HALVE_V    (1 << 0) /* halve V values for interlacing */
#define CRTC_STEREO_DOUBLE    (1 << 1) /* adjust timings for stereo modes */
#define CRTC_NO_DBLSCAN        (1 << 2) /* don't adjust doublescan */
#define CRTC_NO_VSCAN        (1 << 3) /* don't adjust doublescan */
#define CRTC_STEREO_DOUBLE_ONLY    (CRTC_STEREO_DOUBLE | CRTC_NO_DBLSCAN | CRTC_NO_VSCAN)

#define DISP_MODE_FLAG_3D_MAX    DISP_MODE_FLAG_3D_SIDE_BY_SIDE_HALF

#define DISP_MODE_MATCH_TIMINGS (1 << 0)
#define DISP_MODE_MATCH_CLOCK (1 << 1)
#define DISP_MODE_MATCH_FLAGS (1 << 2)
#define DISP_MODE_MATCH_3D_FLAGS (1 << 3)
#define DISP_MODE_MATCH_ASPECT_RATIO (1 << 4)

/**
 * DISP_MODE_FMT - printf string for &disp_display_mode_t
 */
#define DISP_MODE_FMT    "\"%s\": %d %d %d %d %d %d %d %d %d %d 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x"

/**
 * DISP_MODE_ARG - printf arguments for &disp_display_mode_t
 * @m: display mode
 */
#define DISP_MODE_ARG(m) \
    (m)->name, disp_mode_vrefresh(m), (m)->clock, \
    (m)->hdisplay, (m)->hsync_start, (m)->hsync_end, (m)->htotal, \
    (m)->vdisplay, (m)->vsync_start, (m)->vsync_end, (m)->vtotal, \
    (m)->type, (m)->flags, (m)->output_format_mask, \
    (m)->output_444_bpc_mask, (m)->output_422_bpc_mask, (m)->output_420_bpc_mask

#define obj_to_mode(x) container_of(x, disp_display_mode_t, base)

/**
 * disp_mode_is_stereo - check for stereo mode flags
 * @mode: disp_display_mode_t to check
 *
 * Returns:
 * True if the mode is one of the stereo modes (like side-by-side), false if
 * not.
 */
static inline disp_u8_t disp_mode_is_stereo(pdisp_display_mode_t pmode)
{
    return (pmode->flags & DISP_MODE_FLAG_3D_MASK) ? 1 : 0;
}

disp_void_t disp_mode_debug_printmodeline(pdisp_display_mode_t pmode, disp_u32_t level);
disp_s8_t disp_mode_probed_add(pdisp_connector_t pdisp_connector, pdisp_display_mode_t pmode, disp_u8_t free_mode);
disp_u8_t disp_cvt_mode(
                      disp_s32_t hdisplay, disp_s32_t vdisplay, disp_s32_t vrefresh,
                      disp_u8_t reduced, disp_u8_t interlaced,
                      disp_u8_t margins, pdisp_display_mode_t pmode);
disp_u8_t disp_gtf_mode(disp_s32_t hdisplay, disp_s32_t vdisplay, disp_s32_t vrefresh,
    disp_u8_t interlaced, disp_s32_t margins, pdisp_display_mode_t pmode);
disp_u8_t disp_gtf_mode_complex(disp_s32_t hdisplay, disp_s32_t vdisplay, disp_s32_t vrefresh, disp_u8_t interlaced,
    disp_s32_t margins, disp_s32_t GTF_M, disp_s32_t GTF_2C, disp_s32_t GTF_K, disp_s32_t GTF_2J, pdisp_display_mode_t pdisp_mode);
disp_void_t disp_mode_set_name(pdisp_display_mode_t pmode);
disp_s32_t disp_mode_vrefresh(pdisp_display_mode_t pmode);
disp_u8_t disp_mode_match(pdisp_display_mode_t pmode1,
            pdisp_display_mode_t pmode2,
            disp_u32_t match_flags);
disp_u8_t disp_mode_equal(pdisp_display_mode_t pmode1,
            pdisp_display_mode_t pmode2);
disp_u8_t disp_mode_equal_no_clocks(pdisp_display_mode_t pmode1,
                  pdisp_display_mode_t pmode2);
disp_u8_t disp_mode_equal_no_clocks_no_stereo(pdisp_display_mode_t pmode1,
                    pdisp_display_mode_t pmode2);
DISP_MODE_STATUS disp_mode_validate_ycbcr420(pdisp_display_mode_t pmode, pdisp_connector_t pdisp_connector);
disp_u8_t disp_mode_is_420_only(pdisp_display_info_t pdisplay, pdisp_display_mode_t pmode);
disp_u8_t disp_mode_is_420_also(pdisp_display_info_t pdisplay, pdisp_display_mode_t pmode);
disp_u8_t disp_mode_is_420(pdisp_display_info_t pdisplay, pdisp_display_mode_t pmode);

#endif
