/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __DISPLAY_H__
#define __DISPLAY_H__

typedef unsigned char disp_u8_t, *pdisp_u8_t;
typedef char disp_s8_t, *pdisp_s8_t;
typedef unsigned short disp_u16_t, *pdisp_u16_t;
typedef short disp_s16_t, *pdisp_s16_t;
typedef unsigned int disp_u32_t, *pdisp_u32_t;
typedef int disp_s32_t, *pdisp_s32_t;
typedef unsigned long long disp_u64_t, *pdisp_u64_t;
typedef long long disp_s64_t, *pdisp_s64_t;
typedef void disp_void_t, *pdisp_void_t;

#ifndef DISP_TRUE
#define DISP_TRUE 1
#endif

#ifndef DISP_FALSE
#define DISP_FALSE 0
#endif

#ifndef DISP_NULL
#define DISP_NULL ((void*)0)
#endif

/**
 *duplicated from linux kernel 5.13.12
 */
#ifndef DISP_DISPLAY_MODE_LEN
#define DISP_DISPLAY_MODE_LEN   32
#endif

#ifndef DISP_MODE_TYPE_MASK
#define DISP_MODE_TYPE_MASK      (7<<0)
#endif
#ifndef DISP_MODE_TYPE_BUILTIN
#define DISP_MODE_TYPE_BUILTIN   (1<<0)                            /* deprecated */
#endif
#ifndef DISP_MODE_TYPE_CLOCK_C
#define DISP_MODE_TYPE_CLOCK_C   ((1<<1) | DISP_MODE_TYPE_BUILTIN) /* deprecated */
#endif
#ifndef DISP_MODE_TYPE_CRTC_C
#define DISP_MODE_TYPE_CRTC_C    ((1<<2) | DISP_MODE_TYPE_BUILTIN) /* deprecated */
#endif
#ifndef DISP_MODE_TYPE_PREFERRED
#define DISP_MODE_TYPE_PREFERRED (1<<3)
#endif
#ifndef DISP_MODE_TYPE_DEFAULT
#define DISP_MODE_TYPE_DEFAULT   (1<<4)                            /* deprecated */
#endif
#ifndef DISP_MODE_TYPE_USERDEF
#define DISP_MODE_TYPE_USERDEF   (1<<5)
#endif
#ifndef DISP_MODE_TYPE_DRIVER
#define DISP_MODE_TYPE_DRIVER    (1<<6)
#endif
#ifndef DISP_MODE_TYPE_ALL
#define DISP_MODE_TYPE_ALL      (DISP_MODE_TYPE_PREFERRED |    \
                                 DISP_MODE_TYPE_USERDEF |      \
                                 DISP_MODE_TYPE_DRIVER)
#endif

/**
 * Video mode flags
 * bit compatible with the xrandr RR_ definitions (bits 0-13)
 * ABI warning: Existing userspace really expects
 * the mode flags to match the xrandr definitions. Any
 * changes that don't match the xrandr definitions will
 * likely need a new client cap or some other mechanism
 * to avoid breaking existing userspace. This includes
 * allocating new flags in the previously unused bits!
 */
#ifndef DISP_MODE_FLAG_PHSYNC
#define DISP_MODE_FLAG_PHSYNC                   (1<<0)
#endif
#ifndef DISP_MODE_FLAG_NHSYNC
#define DISP_MODE_FLAG_NHSYNC                   (1<<1)
#endif
#ifndef DISP_MODE_FLAG_PVSYNC
#define DISP_MODE_FLAG_PVSYNC                   (1<<2)
#endif
#ifndef DISP_MODE_FLAG_NVSYNC
#define DISP_MODE_FLAG_NVSYNC                   (1<<3)
#endif
#ifndef DISP_MODE_FLAG_INTERLACE
#define DISP_MODE_FLAG_INTERLACE                (1<<4)
#endif
#ifndef DISP_MODE_FLAG_DBLSCAN
#define DISP_MODE_FLAG_DBLSCAN                  (1<<5)
#endif
#ifndef DISP_MODE_FLAG_CSYNC
#define DISP_MODE_FLAG_CSYNC                    (1<<6)
#endif
#ifndef DISP_MODE_FLAG_PCSYNC
#define DISP_MODE_FLAG_PCSYNC                   (1<<7)
#endif
#ifndef DISP_MODE_FLAG_NCSYNC
#define DISP_MODE_FLAG_NCSYNC                   (1<<8)
#endif
#ifndef DISP_MODE_FLAG_HSKEW
#define DISP_MODE_FLAG_HSKEW                    (1<<9)  /* hskew provided */
#endif
#ifndef DISP_MODE_FLAG_BCAST
#define DISP_MODE_FLAG_BCAST                    (1<<10) /* deprecated */
#endif
#ifndef DISP_MODE_FLAG_PIXMUX
#define DISP_MODE_FLAG_PIXMUX                   (1<<11) /* deprecated */
#endif
#ifndef DISP_MODE_FLAG_DBLCLK
#define DISP_MODE_FLAG_DBLCLK                   (1<<12)
#endif
#ifndef DISP_MODE_FLAG_CLKDIV2
#define DISP_MODE_FLAG_CLKDIV2                  (1<<13)
#endif

/**
 * When adding a new stereo mode don't forget to adjust DISP_MODE_FLAGS_3D_MAX
 * (define not exposed to user space).
 */
#ifndef DISP_MODE_FLAG_3D_MASK
#define DISP_MODE_FLAG_3D_MASK                       (0x1f<<14)
#endif
#ifndef DISP_MODE_FLAG_3D_NONE
#define DISP_MODE_FLAG_3D_NONE                       (0<<14)
#endif
#ifndef DISP_MODE_FLAG_3D_FRAME_PACKING
#define DISP_MODE_FLAG_3D_FRAME_PACKING              (1<<14)
#endif
#ifndef DISP_MODE_FLAG_3D_FIELD_ALTERNATIVE
#define DISP_MODE_FLAG_3D_FIELD_ALTERNATIVE          (2<<14)
#endif
#ifndef DISP_MODE_FLAG_3D_LINE_ALTERNATIVE
#define DISP_MODE_FLAG_3D_LINE_ALTERNATIVE           (3<<14)
#endif
#ifndef DISP_MODE_FLAG_3D_SIDE_BY_SIDE_FULL
#define DISP_MODE_FLAG_3D_SIDE_BY_SIDE_FULL          (4<<14)
#endif
#ifndef DISP_MODE_FLAG_3D_L_DEPTH
#define DISP_MODE_FLAG_3D_L_DEPTH                    (5<<14)
#endif
#ifndef DISP_MODE_FLAG_3D_L_DEPTH_GFX_GFX_DEPTH
#define DISP_MODE_FLAG_3D_L_DEPTH_GFX_GFX_DEPTH      (6<<14)
#endif
#ifndef DISP_MODE_FLAG_3D_TOP_AND_BOTTOM
#define DISP_MODE_FLAG_3D_TOP_AND_BOTTOM             (7<<14)
#endif
#ifndef DISP_MODE_FLAG_3D_SIDE_BY_SIDE_HALF
#define DISP_MODE_FLAG_3D_SIDE_BY_SIDE_HALF          (8<<14)
#endif

/* Picture aspect ratio options */
#ifndef DISP_MODE_PICTURE_ASPECT_NONE
#define DISP_MODE_PICTURE_ASPECT_NONE           0
#endif
#ifndef DISP_MODE_PICTURE_ASPECT_4_3
#define DISP_MODE_PICTURE_ASPECT_4_3            1
#endif
#ifndef DISP_MODE_PICTURE_ASPECT_16_9
#define DISP_MODE_PICTURE_ASPECT_16_9           2
#endif
#ifndef DISP_MODE_PICTURE_ASPECT_64_27
#define DISP_MODE_PICTURE_ASPECT_64_27          3
#endif
#ifndef DISP_MODE_PICTURE_ASPECT_256_135
#define DISP_MODE_PICTURE_ASPECT_256_135        4
#endif

/* Aspect ratio flag bitmask (4 bits 22:19) */
#ifndef DISP_MODE_FLAG_PIC_AR_MASK
#define DISP_MODE_FLAG_PIC_AR_MASK              (0x0F<<19)
#endif
#ifndef DISP_MODE_FLAG_PIC_AR_NONE
#define DISP_MODE_FLAG_PIC_AR_NONE     \
                        (DISP_MODE_PICTURE_ASPECT_NONE<<19)
#endif
#ifndef DISP_MODE_FLAG_PIC_AR_4_3
#define DISP_MODE_FLAG_PIC_AR_4_3      \
                        (DISP_MODE_PICTURE_ASPECT_4_3<<19)
#endif
#ifndef DISP_MODE_FLAG_PIC_AR_16_9
#define DISP_MODE_FLAG_PIC_AR_16_9     \
                        (DISP_MODE_PICTURE_ASPECT_16_9<<19)
#endif
#ifndef DISP_MODE_FLAG_PIC_AR_64_27
#define DISP_MODE_FLAG_PIC_AR_64_27    \
                        (DISP_MODE_PICTURE_ASPECT_64_27<<19)
#endif
#ifndef DISP_MODE_FLAG_PIC_AR_256_135
#define DISP_MODE_FLAG_PIC_AR_256_135  \
                        (DISP_MODE_PICTURE_ASPECT_256_135<<19)
#endif

#define DISP_MODE_FLAG_DSC                      (1ul << 28)
#define DISP_MODE_FLAG_DSC_10                   (2ul << 28) // 8 bpc normal mode, 10 bpc need dsc mode check.
#define DISP_MODE_FLAG_DSC_DISABLE              (4ul << 28)
#define DISP_MODE_FLAG_DSC_MASK                 (7ul << 28)

#define DISP_MODE_FLAG_ADAPTER_SOURCE           (1ul << 31)

#ifndef DISP_MODE_FLAG_ALL
#define DISP_MODE_FLAG_ALL      (DISP_MODE_FLAG_PHSYNC |    \
                                 DISP_MODE_FLAG_NHSYNC |    \
                                 DISP_MODE_FLAG_PVSYNC |    \
                                 DISP_MODE_FLAG_NVSYNC |    \
                                 DISP_MODE_FLAG_INTERLACE | \
                                 DISP_MODE_FLAG_DBLSCAN |   \
                                 DISP_MODE_FLAG_CSYNC |     \
                                 DISP_MODE_FLAG_PCSYNC |    \
                                 DISP_MODE_FLAG_NCSYNC |    \
                                 DISP_MODE_FLAG_HSKEW |     \
                                 DISP_MODE_FLAG_DBLCLK |    \
                                 DISP_MODE_FLAG_CLKDIV2 |   \
                                 DISP_MODE_FLAG_3D_MASK |   \
                                 DISP_MODE_FLAG_DSC_MASK |  \
                                 DISP_MODE_FLAG_ADAPTER_SOURCE)
#endif

typedef enum _DISP_STATUS
{
    DISP_OK =  0,
    DISP_EPERM,     /* Operation not permitted */
    DISP_ENOENT,    /* No such file or directory */
    DISP_ESRCH,     /* No such process */
    DISP_EINTR,     /* Interrupted system call */
    DISP_EIO,       /* I/O error */
    DISP_ENXIO,     /* No such device or address */
    DISP_E2BIG,     /* Argument list too long */
    DISP_ENOEXEC,   /* Exec format error */
    DISP_EBADF,     /* Bad file number */
    DISP_ECHILD,    /* No child processes */
    DISP_EAGAIN,    /* Try again */
    DISP_ENOMEM,    /* Out of memory */
    DISP_EACCES,    /* Permission denied */
    DISP_EFAULT,    /* Bad address */
    DISP_ENOTBLK,   /* Block device required */
    DISP_EBUSY,     /* Device or resource busy */
    DISP_EEXIST,    /* File exists */
    DISP_EXDEV,     /* Cross-device link */
    DISP_ENODEV,    /* No such device */
    DISP_ENOTDIR,   /* Not a directory */
    DISP_EISDIR,    /* Is a directory */
    DISP_EINVAL,    /* Invalid argument/parameter */
    DISP_ENFILE,    /* File table overflow */
    DISP_EMFILE,    /* Too many open files */
    DISP_ENOTTY,    /* Not a typewriter */
    DISP_ETXTBSY,   /* Text file busy */
    DISP_EFBIG,     /* File too large */
    DISP_ENOSPC,    /* No space left on device */
    DISP_ESPIPE,    /* Illegal seek */
    DISP_EROFS,     /* Read-only file system */
    DISP_EMLINK,    /* Too many links */
    DISP_EPIPE,     /* Broken pipe */
    DISP_EDOM,      /* Math argument out of domain of func */
    DISP_ERANGE,    /* Math result not representable */
    DISP_ENREADY,   /* Device not ready */
    DISP_ENSUPPORT, /* Device not support */

    DISP_EPRIV_BEGIN = 100,
    DISP_EPRIV_UNKNOWN,
    DISP_EPRIV_DEVICE_ID,
    DISP_EPRIV_INEDID,
    DISP_EPRIV_INTERNAL,
    DISP_EREMOTEIO,
    DISP_ETIMEDOUT,
    DISP_EPROTO,
}DISP_STATUS;

typedef enum _DISP_DEVICE_ID
{
    DISP_DEVICE_G100, 
}DISP_DEVICE_ID;

#define DISP_IN
#define DISP_OUT

typedef struct _disp_u16xy_t
{
    disp_u16_t x;
    disp_u16_t y;
}disp_u16xy_t, *pdisp_u16xy_t;

typedef struct _disp_callback_t
{
    pdisp_void_t pdisp_fn_vsnprintf;
    pdisp_void_t pdisp_fn_print;
    pdisp_void_t pdisp_fn_print_to_file;

    pdisp_void_t pdisp_fn_malloc;
    pdisp_void_t pdisp_fn_free;

    pdisp_void_t pdisp_fn_memset;
    pdisp_void_t pdisp_fn_memcpy;
    pdisp_void_t pdisp_fn_memcmp;

    pdisp_void_t pdisp_fn_read_u8;
    pdisp_void_t pdisp_fn_write_u8;
    pdisp_void_t pdisp_fn_read_u16;
    pdisp_void_t pdisp_fn_write_u16;
    pdisp_void_t pdisp_fn_read_u32;
    pdisp_void_t pdisp_fn_write_u32;
    pdisp_void_t pdisp_fn_read_rom_byte;

    pdisp_void_t pdisp_fn_strcmp;
    pdisp_void_t pdisp_fn_strcpy;
    pdisp_void_t pdisp_fn_strncmp;
    pdisp_void_t pdisp_fn_delay_us;
    pdisp_void_t pdisp_fn_do_div;
}disp_callback_t, *pdisp_callback_t;

typedef struct _disp_device_init_t
{
    DISP_OUT pdisp_void_t pdisplay_device;//generated internally, given back to driver to set down
    DISP_IN pdisp_void_t pdisplay_adapter;//generated in driver side,
    DISP_IN DISP_DEVICE_ID device_id;
    DISP_IN pdisp_callback_t pdisp_callback;
    union
    {
        struct
        {
            disp_u32_t pz1_config               : 1;
            disp_u32_t pz1_capture_pipe         : 1;
            disp_u32_t dvo_enable               : 1;
            disp_u32_t prefer_free_sync         : 1;
            disp_u32_t dp_cts                   : 1;
            disp_u32_t mode_8k                  : 1;
            disp_u32_t hau_sw_csb               : 1;
            disp_u32_t hau_sw_info              : 1;
            disp_u32_t hau_asp_8ch              : 1;
            disp_u32_t reserved                 :23;
        } bits;
        disp_u32_t val;
    } flag;
}disp_device_init_t, *pdisp_device_init_t;

typedef struct _disp_query_resource_t
{
    disp_u8_t crtc_num;
    disp_u8_t plane_num;//every crtc has same plane num, refine here it depends
    disp_u8_t pu_support;//every crtc will support panel upscaler or not at same time
    disp_u8_t connector_support;//connector_mask = 1 << connector_index
    //one crtc has 2 upscaler module, one is tied behind overlay module
    //the other is shared by all planes on this crtc, will add upscaler-lease logic to handle such case  
}disp_query_resource_t, *pdisp_query_resource_t;

typedef struct _disp_detect_connector_t
{
    disp_u8_t connector_to_detect;
    disp_u8_t connector_to_fake;
    disp_u8_t connector_detected;
}disp_detect_connector_t, *pdisp_detect_connector_t;

/**
 * @hdisplay: horizontal display size
 * @hsync_start: horizontal sync start
 * @hsync_end: horizontal sync end
 * @htotal: horizontal total size
 * @hskew: horizontal skew?!
 * @vdisplay: vertical display size
 * @vsync_start: vertical sync start
 * @vsync_end: vertical sync end
 * @vtotal: vertical total size
 * @vscan: vertical scan?!
 */
typedef struct _disp_display_mode_t
{
    /**
     * @clock:
     *
     * Pixel clock in kHz.
     */
    disp_s32_t clock;        /* in kHz */
    disp_u16_t hdisplay;
    disp_u16_t hsync_start;
    disp_u16_t hsync_end;
    disp_u16_t htotal;
    disp_u16_t hskew;
    disp_u16_t vdisplay;
    disp_u16_t vsync_start;
    disp_u16_t vsync_end;
    disp_u16_t vtotal;
    disp_u16_t vscan;
    /**
     * @flags:
     *
     * Sync and timing flags:
     *
     *  - DISP_MODE_FLAG_PHSYNC: horizontal sync is active high.
     *  - DISP_MODE_FLAG_NHSYNC: horizontal sync is active low.
     *  - DISP_MODE_FLAG_PVSYNC: vertical sync is active high.
     *  - DISP_MODE_FLAG_NVSYNC: vertical sync is active low.
     *  - DISP_MODE_FLAG_INTERLACE: mode is interlaced.
     *  - DISP_MODE_FLAG_DBLSCAN: mode uses doublescan.
     *  - DISP_MODE_FLAG_CSYNC: mode uses composite sync.
     *  - DISP_MODE_FLAG_PCSYNC: composite sync is active high.
     *  - DISP_MODE_FLAG_NCSYNC: composite sync is active low.
     *  - DISP_MODE_FLAG_HSKEW: hskew provided (not used?).
     *  - DISP_MODE_FLAG_BCAST: <deprecated>
     *  - DISP_MODE_FLAG_PIXMUX: <deprecated>
     *  - DISP_MODE_FLAG_DBLCLK: double-clocked mode.
     *  - DISP_MODE_FLAG_CLKDIV2: half-clocked mode.
     *
     * Additionally there's flags to specify how 3D modes are packed:
     *
     *  - DISP_MODE_FLAG_3D_NONE: normal, non-3D mode.
     *  - DISP_MODE_FLAG_3D_FRAME_PACKING: 2 full frames for left and right.
     *  - DISP_MODE_FLAG_3D_FIELD_ALTERNATIVE: interleaved like fields.
     *  - DISP_MODE_FLAG_3D_LINE_ALTERNATIVE: interleaved lines.
     *  - DISP_MODE_FLAG_3D_SIDE_BY_SIDE_FULL: side-by-side full frames.
     *  - DISP_MODE_FLAG_3D_L_DEPTH:
     *  - DISP_MODE_FLAG_3D_L_DEPTH_GFX_GFX_DEPTH:
     *  - DISP_MODE_FLAG_3D_TOP_AND_BOTTOM: frame split into top and bottom
     *    parts.
     *  - DISP_MODE_FLAG_3D_SIDE_BY_SIDE_HALF: frame split into left and
     *    right parts.
     */
    disp_u32_t flags;

    /**
     * @width_mm:
     *
     * Addressable size of the output in mm, projectors should set this to
     * 0.
     */
    disp_u16_t width_mm;

    /**
     * @height_mm:
     *
     * Addressable size of the output in mm, projectors should set this to
     * 0.
     */
    disp_u16_t height_mm;

    /**
     * @type:
     *
     * A bitmask of flags, mostly about the source of a mode. Possible flags
     * are:
     *
     *  - DISP_MODE_TYPE_PREFERRED: Preferred mode, usually the native
     *    resolution of an LCD panel. There should only be one preferred
     *    mode per connector at any given time.
     *  - DISP_MODE_TYPE_DRIVER: Mode created by the driver, which is all of
     *    them really. Drivers must set this bit for all modes they create
     *    and expose to userspace.
     *  - DISP_MODE_TYPE_USERDEF: Mode defined or selected via the kernel
     *    command line.
     *
     * Plus a big list of flags which shouldn't be used at all, but are
     * still around since these flags are also used in the userspace ABI.
     * We no longer accept modes with these types though:
     *
     *  - DISP_MODE_TYPE_BUILTIN: Meant for hard-coded modes, unused.
     *    Use DISP_MODE_TYPE_DRIVER instead.
     *  - DISP_MODE_TYPE_DEFAULT: Again a leftover, use
     *    DISP_MODE_TYPE_PREFERRED instead.
     *  - DISP_MODE_TYPE_CLOCK_C and DISP_MODE_TYPE_CRTC_C: Define leftovers
     *    which are stuck around for hysterical raisins only. No one has an
     *    idea what they were meant for. Don't use.
     */
    disp_u8_t type;

    /**
     * @name:
     *
     * Human-readable name of the mode, filled out with disp_mode_set_name().
     */
    disp_s8_t name[32];

    /* @status:
     *
     * Status of the mode, used to filter out modes not supported by the
     * hardware. See enum &disp_mode_status.
     */
    disp_u8_t status;
    disp_u8_t picture_aspect_ratio;
    disp_u8_t vic; // video_code, CEA mode only
    disp_u8_t hdmi_vic; // hdmi 4k mode

    disp_u8_t output_format_mask;   // DISP_COLOR_FORMAT_RGB444
    disp_u8_t output_444_bpc_mask;  // 1 << DISP_COLOR_DEPTH
    disp_u8_t output_422_bpc_mask;
    disp_u8_t output_420_bpc_mask;
}disp_display_mode_t, *pdisp_display_mode_t;

typedef struct _disp_mode_list_t
{
    disp_u8_t connector_index;
    disp_s32_t num;
    pdisp_display_mode_t modes;
}disp_mode_list_t, *pdisp_mode_list_t;

typedef struct _disp_get_edid_t
{
    disp_u8_t connector_index;  // in
    pdisp_u8_t buf;             // in  alloc by dirver
    disp_u32_t buf_size;        // in  buf size
    disp_u32_t edid_size;       // out edid size
}disp_get_edid_t, *pdisp_get_edid_t;

typedef struct _disp_crtc_dpms_t
{
    disp_u8_t crtc_index;
    disp_u8_t enable;
    disp_u8_t sgnt_global_enable; // 1 - set signature reg, 0 - not touch signature reg, default 0
}disp_crtc_dpms_t, *pdisp_crtc_dpms_t;

#define DISP_COLOR_FORMAT_RGB444    (1<<0)
#define DISP_COLOR_FORMAT_YCRCB444    (1<<1)
#define DISP_COLOR_FORMAT_YCRCB422    (1<<2)
#define DISP_COLOR_FORMAT_YCRCB420    (1<<3)

typedef enum _DISP_COLOR_FORMAT_TYPE
{
    DISP_COLOR_FORMAT_TYPE_RGB = 0,
    DISP_COLOR_FORMAT_TYPE_YUV444,
    DISP_COLOR_FORMAT_TYPE_YUV422,
    DISP_COLOR_FORMAT_TYPE_YUV420,
}DISP_COLOR_FORMAT_TYPE;

typedef enum _DISP_COLOR_DEPTH {
	DISP_COLOR_DEPTH_6 = 0,
	DISP_COLOR_DEPTH_8,
	DISP_COLOR_DEPTH_10,
	DISP_COLOR_DEPTH_12,
	DISP_COLOR_DEPTH_16,
}DISP_COLOR_DEPTH;

typedef enum _DISP_COLORIMETRY
{
    DISP_COLORIMETRY_SRGB = 0,
    DISP_COLORIMETRY_YCC_NONE,          // SMPTE 170M or ITU-R BT.709
    DISP_COLORIMETRY_YCC_SMPTE_170M,
    DISP_COLORIMETRY_YCC_BT709,

    DISP_COLORIMETRY_XVYCC_601,
    DISP_COLORIMETRY_XVYCC_709,
    DISP_COLORIMETRY_SYCC_601,
    DISP_COLORIMETRY_YCC_ADOBE_601,     // AdobeYCC601
    DISP_COLORIMETRY_RGB_ADOBE,         // AdobeRGB
    DISP_COLORIMETRY_CYCC_BT2020,       // ITU-R BT.2020 Y'cC'bcC'rc
    DISP_COLORIMETRY_YCC_BT2020,        // ITU-R BT.2020 Y'C'bC'r
    DISP_COLORIMETRY_RGB_BT2020,        // ITU-R BT.2020 R’G’B'
    DISP_COLORIMETRY_RGB_DCI,           // DCI-P3 R’G’B’ // origin DCI-P3 (SMPTE RP 431-2) [0.3140, 0.3510]
    DISP_COLORIMETRY_RGB_DCI_D65,       // DCI-P3 R’G’B’ (d65) // DCI-P3 with 6500k [0.3127, 0.3290]

    DISP_COLORIMETRY_END,
}DISP_COLORIMETRY;

typedef enum _DISP_RANGE_TYPE
{
    DISP_RANGE_TYPE_VESA = 0,
    DISP_RANGE_TYPE_CEA,
}DISP_RANGE_TYPE;

typedef enum _DISP_YUV_COEFFICIENTS
{
    DISP_YUV_COEFFICIENTS_ITU601 = 0,
    DISP_YUV_COEFFICIENTS_ITU709,
}DISP_YUV_COEFFICIENTS;

typedef enum _DISP_EOTF_TYPE
{
    DISP_EOTF_TRADITIONAL_GAMMA_SDR = 0,
    DISP_EOTF_TRADITIONAL_GAMMA_HDR,
    DISP_EOTF_SMPTE_ST2084,
    DISP_EOTF_BT_2100_HLG,
    DISP_EOTF_RESERVED, // 4 ~ 7
}DISP_EOTF_TYPE;

#define DISP_HDR_COLOR_POSITION_NORMALIZE_FACTOR_10BITS 1024    // unit 1/1024
#define DISP_HDR_COLOR_POSITION_NORMALIZE_FACTOR_16BITS 0xC350  // unit 1/50000
#define DISP_HDR_COLOR_POSITION_ERR_BOUNDARY_16BITS     25      // boundary: 1/2000
typedef struct _disp_hdr_static_metadata_t
{
    DISP_EOTF_TYPE eotf;
    disp_u16xy_t primary_point[3];  // ref CTA-861-G: unit 0.00002, [0x0, 0xC350] -> [0.0, 1.0]
    disp_u16xy_t white_point;       // unit 0.00002
    disp_u16_t max_luminance;               // 1 nits
    disp_u16_t min_luminance;               // 1/10000 nits
    disp_u16_t max_frame_average_luminance; // 1 nits
    disp_u16_t max_content_light_level;     // 1 nits
}disp_hdr_static_metadata_t, *pdisp_hdr_static_metadata_t;

typedef struct _disp_hdr_dynamic_metadata_t
{
    disp_u32_t size;
    pdisp_void_t pdata;
}disp_hdr_dynamic_metadata_t, *pdisp_hdr_dynamic_metadata_t;

typedef enum _DISP_HDR_METADATA_TYPE
{
    DISP_HDR_METADATA_TYPE_NONE,
    DISP_HDR_METADATA_TYPE_HDR10,
    DISP_HDR_METADATA_TYPE_HDR10PLUS,
}DISP_HDR_METADATA_TYPE;

typedef struct _disp_hdr_metadata_t
{
    DISP_HDR_METADATA_TYPE type;
    union
    {
        disp_hdr_static_metadata_t static_data;
        disp_hdr_dynamic_metadata_t dynamic_data;
    }data;
}disp_hdr_metadata_t, *pdisp_hdr_metadata_t;

#define DISP_HDR_REG_DG_LUT_LEN 37
#define DISP_HDR_REG_SAT_LUT_LEN 13
#define DISP_HDR_REG_RGB2RGB_LEN 9
#define DISP_HDR_REG_G_LUT_LEN 33
typedef struct _disp_hdr_module_t
{
    disp_u8_t hdr_en;
    disp_u8_t sat_en;

    /* UQ00.29[0]
     * UQ00.26[1,8]
     * UQ01.23[9,18]
     * UQ01.19[19,26]
     * UQ01.16[27,36]
     */
    disp_u32_t dg_lut_y[DISP_HDR_REG_DG_LUT_LEN];
    /* UQ00.26[0,8]
     * UQ00.23[9,16]
     * UQ00.19[17,35]
     */
    disp_u32_t dg_lut_s[DISP_HDR_REG_DG_LUT_LEN - 1];

    disp_u16_t sat_rgb_2_y[3];      // UQ 00.12

    /* UQ01.06[0,8]
     * UQ03.06[9,12]
     */
    disp_u32_t sat_lut_y[DISP_HDR_REG_SAT_LUT_LEN];
    /* UQ05.07[0,6]
     * UQ00.12[7,11]
     */
    disp_u32_t sat_lut_s[DISP_HDR_REG_SAT_LUT_LEN - 1];

    disp_u32_t dg_stretch_a;        // UQ 14.12
    disp_s32_t dg_stretch_b;        // Q  14.12

    /* Q02.12: 15bit(signed) = 3bit(int with sign)  12bit(decimal unsigned, always positive or 0)
     * Matrix[3x3]: [0][0]~...~[0]][2]~[1]][0]~...~[2][2]
     */
    disp_s16_t rgb_2_rgb_m[DISP_HDR_REG_RGB2RGB_LEN];

    /* UQ06.04[0,9]
     * UQ08.04[10,18]
     * UQ10.04[19,32]
     */
    disp_u32_t g_lut_y[DISP_HDR_REG_G_LUT_LEN];
    /* UQ27.01[0,3]
     * UQ25.01[4,16]
     * UQ18.01[17,22]
     * UQ14.01[23, 31]
     */
    disp_u32_t g_lut_s[DISP_HDR_REG_G_LUT_LEN - 1];

    disp_u32_t g_stretch_a;         // UQ 14.12
    disp_s32_t g_stretch_b;         // Q  14.12
} disp_hdr_module_t, *pdisp_hdr_module_t;

typedef struct _disp_crtc_set_mode_t
{
    disp_u8_t index;
    disp_display_mode_t mode;
    disp_display_mode_t hw_mode;
    disp_u8_t output_signal;
    DISP_COLORIMETRY colorimetry;
    disp_u8_t bpc;//default to 8 bit per component

    disp_u8_t hdr_mode; // hdr on/off
    disp_hdr_metadata_t hdr_metadata;
}disp_crtc_set_mode_t, *pdisp_crtc_set_mode_t;

typedef struct _disp_encoder_dpms_t
{
    disp_u8_t index;//crtc_index = encoder_index = connector_index
    disp_u8_t enable;// DISP_POWER_STATE
}disp_encoder_dpms_t, *pdisp_encoder_dpms_t;

typedef struct _disp_encoder_set_mode_t
{
    disp_u8_t index;
//get mode info from the corresponding crtc
}disp_encoder_set_mode_t, *pdisp_encoder_set_mode_t;

typedef enum _DISP_POWER_STATE
{
    DISP_POWER_OFF = 0,
    DISP_POWER_ON,
}DISP_POWER_STATE;

typedef enum _DISP_PLANE_TYPE
{
    DISP_PRIMARY_PLANE = 0,
    DISP_OVERLAY_PLANE = 1,
    DISP_CURSOR_PLANE = 2,
}DISP_PLANE_TYPE;

typedef struct _disp_rect_t
{
    disp_u32_t x;
    disp_u32_t y;
    disp_u32_t w;
    disp_u32_t h;
}disp_rect_t, *pdisp_rect_t;

typedef enum _DISP_FB_FMT // name order: high bit channel -> low bit channel
{
    DISP_FB_FMT_R5G6B5 = 0,
    DISP_FB_FMT_A8R8G8B8,
    DISP_FB_FMT_A8B8G8R8,
    DISP_FB_FMT_X8R8G8B8,
    DISP_FB_FMT_X8B8G8R8,
    DISP_FB_FMT_A2R10G10B10,
    DISP_FB_FMT_A2B10G10R10,
    DISP_FB_FMT_X2R10G10B10,
    DISP_FB_FMT_X2B10G10R10,
    DISP_FB_FMT_CRYCB422_16BIT,     // [31:0] Cr0:Y1:Cb0:Y0 - YUYV
    DISP_FB_FMT_YCBCR422_16BIT,     // [31:0] Y1:Cr0:Y0:Cb0 - UYVY
    DISP_FB_FMT_CRYCB422_20BIT,
    DISP_FB_FMT_YCBCR422_20BIT,
    DISP_FB_FMT_CRYCB422_24BIT,
    DISP_FB_FMT_YCBCR422_24BIT,
    DISP_FB_FMT_CRYCB422_32BIT,
    DISP_FB_FMT_YCBCR422_32BIT,
    DISP_FB_FMT_CRYCB8888_32BIT,    // [31:0] A:Cr:Y:Cb
    DISP_FB_FMT_YCBCR8888_32BIT,    // [31:0] A:Y:Cb:Cr - AYUV
    DISP_FB_FMT_CRYCB2101010_32BIT, // [31:0] X:Cr:Y:Cb 2:10:10:10 - XV30
    DISP_FB_FMT_YCBCR2101010_32BIT,
    DISP_FB_FMT_A16R16G16B16_FLOAT,
    DISP_FB_FMT_A16B16G16R16_FLOAT,
    DISP_FB_FMT_COUNT,
}DISP_FB_FMT;

typedef struct _disp_fb_t
{
    disp_u64_t address;
    disp_u64_t offset;//do not set, calculate by src window's start point
    DISP_FB_FMT format;
    disp_u32_t bit_cnt;//bpp, num of bit per pixel
    disp_u32_t pitch;
    disp_u32_t width;
    disp_u32_t height, align_width;

    //below is compress information
    disp_u32_t range_type;
    disp_u32_t bl_index;
    struct
    {
        disp_u32_t compress :1;
        disp_u32_t resv :31;
    }flag;

    //color information
    disp_u32_t color_space;
    disp_u32_t color_range;
}disp_fb_t, *pdisp_fb_t; 

typedef enum _DISP_BLEND
{
    DISP_BLEND_A_WINDOW_KEY = 0x01,
    DISP_BLEND_B_WINDOW_KEY = 0x02,
    DISP_BLEND_A_ALPHA_KEY = 0x03,
    DISP_BLEND_B_ALPHA_KEY = 0x04,
    DISP_BLEND_A_COLOR_KEY = 0x05,
    DISP_BLEND_B_COLOR_KEY = 0x06,
    DISP_BLEND_A_ALPHA_BLENDING = 0x07,
    DISP_BLEND_B_ALPHA_BLENDING = 0x08,
    DISP_BLEND_A_CONSTANT_ALPHA = 0x09,
    DISP_BLEND_B_CONSTANT_ALPHA = 0xA,
}DISP_BLEND;

typedef enum __DISP_FLIP_MODE
{
    DISP_FLIP_ENABLE = 0x00,
    DISP_FLIP_DISABLE,
}DISP_FLIP_MODE;

typedef enum _DISP_KEY_SEL {
    DISP_KEY_SEL_A_OR_B_KEY                     = 0 , // output key = a.key | b.key
    DISP_KEY_SEL_A_KEY                          = 1 , // output a key
    DISP_KEY_SEL_B_KEY                          = 2 , // output b key
} DISP_KEY_SEL;

typedef enum _DISP_ALP_KEY_SEL{
    DISP_ALP_SEL_MIX_ALPHA                      = 0 , // output mixed alpha
    DISP_ALP_SEL_A_ALPHA                        = 1 , // output a alpha
    DISP_ALP_SEL_B_ALPHA                        = 2 , // output b alpha
}DISP_ALP_SEL;

typedef struct _disp_blend_info_t
{
    DISP_BLEND blend_mode;
    disp_u8_t pre_multi;
    disp_u8_t invert_a;//inver alpha or not
    DISP_ALP_SEL alpha_sel;
    DISP_KEY_SEL key_sel;

    struct
    {
        //B factor in window/alpha/color key blending mode or constant_alpha[3:0] in constant alpha blending mode
        disp_u8_t kp_kmix;//the lower layer
        //A factor in window/alpha/color key blending mode or constant_alpha[7:4] in constant alpha blending mode
        disp_u8_t ks_kt;//the high layer
    } factor;
    disp_u8_t plane_alpha;//default to 0xFF， if not 0xFF, take care

    disp_u32_t key_value;//color key value or alpha key value
}disp_blend_info_t, *pdisp_blend_info_t;

typedef enum _DISP_CURSOR_TYPE
{
    DISP_CS_TYPE_MONO = 0,
    DISP_CS_TYPE_COLOR_ALPHA = 4,
    DISP_CS_TYPE_COLOR_PREALP  = 5,
    DISP_CS_TYPE_COLOR_AX = 6,
    DISP_CS_TYPE_COLOR_MASKED = 7,
}DISP_CURSOR_TYPE;

typedef struct _disp_scale_t
{
    disp_u32_t src_width;
    disp_u32_t src_height;
    disp_u32_t dst_width;
    disp_u32_t dst_height;
    disp_u32_t start_x;
    disp_u32_t start_y;
}disp_scale_t, *pdisp_scale_t;

typedef struct _disp_plane_update_para_t
{
    disp_u32_t crtc_index;
    DISP_PLANE_TYPE plane_index;
    DISP_FLIP_MODE flip_mode;
    disp_rect_t src;
    disp_rect_t dst;
    disp_fb_t fb;
    disp_blend_info_t blend;

    DISP_CURSOR_TYPE cs_type;
    disp_u32_t mono_bg;
    disp_u32_t mono_fg;

    disp_hdr_metadata_t hdr_metadata;
    disp_hdr_module_t hdr_module;
    disp_scale_t panel_scale;
    disp_u32_t sgnt_golden[4];

    union
    {
        struct
        {
            disp_u32_t hdr_metadata             : 1;
            disp_u32_t hdr_module               : 1;
            disp_u32_t panel_scale              : 1;
            disp_u32_t overlay_scale_flush      : 1; // Keep overlay show right when close panel scaler
            disp_u32_t flip_immediate           : 1;
            disp_u32_t reserved                 :27;
        } valid;
        disp_u32_t val;
    } flag;
}disp_plane_update_para_t, *pdisp_plane_update_para_t;

typedef union _disp_intr_t
{
    disp_u32_t intr;
    struct 
    {
        disp_u32_t Vgairq                          : 1 ;
        disp_u32_t Vsynca                          : 1 ;
        disp_u32_t Vsyncb                          : 1 ;
        disp_u32_t Vsyncc                          : 1 ;
        disp_u32_t Vsyncd                          : 1 ;
        disp_u32_t Codec1                          : 1 ;
        disp_u32_t Reserved_2                      : 1 ;
        disp_u32_t Dpa_Hpd                         : 1 ;
        disp_u32_t Dpa_Ephy                        : 1 ;
        disp_u32_t Dpa_Linktraining                : 1 ;
        disp_u32_t Dpb_Hpd                         : 1 ;
        disp_u32_t Dpb_Ephy                        : 1 ;
        disp_u32_t Dpb_Linktraining                : 1 ;
        disp_u32_t Dpc_Hpd                         : 1 ;
        disp_u32_t Dpc_Ephy                        : 1 ;
        disp_u32_t Dpc_Linktraining                : 1 ;
        disp_u32_t Dpd_Hpd                         : 1 ;
        disp_u32_t Dpd_Ephy                        : 1 ;
        disp_u32_t Dpd_Linktraining                : 1 ;
        disp_u32_t Pipea_Sgnt                      : 1 ;
        disp_u32_t Pipeb_Sgnt                      : 1 ;
        disp_u32_t Pipec_Sgnt                      : 1 ;
        disp_u32_t Piped_Sgnt                      : 1 ;
        disp_u32_t Reserved_1                      : 9 ;
    }info;
}disp_intr_t, *pdisp_intr_t;

typedef struct _disp_get_fb_addr_t
{
    disp_u32_t crtc_index;
    DISP_PLANE_TYPE plane_index;
    disp_u64_t address;
}disp_get_fb_addr_t, *pdisp_get_fb_addr_t;

typedef enum _DISP_ITG_CNT_TYPE 
{
    DISP_COUNTER_FRAME  = 0,
    DISP_COUNTER_LINE,
    DISP_COUNTER_PIXEL,
    DISP_COUNTER_LINE_DPC,
    DISP_COUNTER_PIXEL_DPC,
    DISP_COUNTER_NUM,
}DISP_ITG_CNT_TYPE;

typedef struct _disp_itg_counter_t 
{
    disp_u32_t crtc;
    disp_u32_t Value[DISP_COUNTER_NUM];
    disp_u8_t bInVblank;
    disp_u8_t bInHblank;
    disp_u8_t bInVsync;
    disp_u8_t bInHsync;
    struct 
    {
        disp_u32_t bGetFrameCnt     : 1;
        disp_u32_t bGetLineCnt      : 1;
        disp_u32_t bGetPixelCnt     : 1;
        disp_u32_t bGetLineDpcCnt   : 1; // start from vsync start
        disp_u32_t bGetPixelDpcCnt  : 1;
        disp_u32_t Reserved         :27;
    }flag;
}disp_itg_counter_t, *pdisp_itg_counter_t;

#define DISP_CRC_CNT 4

typedef struct _disp_sig_set_t 
{
    disp_u32_t crtc_index;
    disp_rect_t isr; //interest screen region
    disp_u8_t benable;
    disp_u32_t crc[DISP_CRC_CNT];
}disp_sig_set_t, *pdisp_sig_set_t;

typedef struct _disp_sig_get_t 
{
    disp_u32_t crtc_index;
    disp_u32_t crc[DISP_CRC_CNT];
}disp_sig_get_t, *pdisp_sig_get_t;

#define DISP_GAMMA_RAMP_RGB_LUT_LENGTH 256
typedef struct _disp_gamma_ramp_rgb_t
{
    disp_u16_t  Red[DISP_GAMMA_RAMP_RGB_LUT_LENGTH];
    disp_u16_t  Green[DISP_GAMMA_RAMP_RGB_LUT_LENGTH];
    disp_u16_t  Blue[DISP_GAMMA_RAMP_RGB_LUT_LENGTH];
} disp_gamma_ramp_rgb_t, *pdisp_gamma_ramp_rgb_t;

typedef struct _disp_gamma_info_t 
{
    disp_u32_t crtc_index;
    disp_u8_t gamma_enable;
    disp_u32_t gamma_bit;//6bit/8bit/10bit
    disp_u32_t lut_length;
    union
    {
        pdisp_gamma_ramp_rgb_t pgamma_ramp_rgb;
        pdisp_void_t praw;
    }data;
}disp_gamma_info_t, *pdisp_gamma_info_t;

typedef struct _disp_csc_para_t
{
    disp_u32_t crtc_index;

    disp_u32_t brightness;

//this matrix calculated by hue/contrast/situation
    disp_u16_t coef_1;
    disp_u16_t coef_2;
    disp_u16_t coef_3;
    disp_u16_t coef_4;
    disp_u16_t coef_5;
    disp_u16_t coef_6;
    disp_u16_t coef_7;
    disp_u16_t coef_8;
    disp_u16_t coef_9;

    struct
    {
        disp_u32_t bbypass :1;
        disp_u32_t bprogrammable :1;
        disp_u32_t reserved :30;
    }flag;
}disp_csc_para_t, *pdisp_csc_para_t;

typedef enum _DISP_DP_LINK_RATE_TYPE
{
    DISP_DP_LINK_RATE_TYPE_1_62 = 0,
    DISP_DP_LINK_RATE_TYPE_2_7,
    DISP_DP_LINK_RATE_TYPE_5_4,
    DISP_DP_LINK_RATE_TYPE_8_1,
}DISP_DP_LINK_RATE_TYPE;

typedef enum _DISP_DP_ASYNC_CLOCK
{
    DISP_DP_ASYNC_CLOCK_SYNC = 0,
    DISP_DP_ASYNC_CLOCK_ASYNC,
}DISP_DP_ASYNC_CLOCK;

typedef enum _DISP_DP_ENHANCED_FRAME_MODE
{
    DISP_DP_ENHANCED_FRAME_MODE_NORMAL = 0,
    DISP_DP_ENHANCED_FRAME_MODE_ENHANCED,
}DISP_DP_ENHANCED_FRAME_MODE;

typedef enum _DISP_DP_OP
{
    DISP_DP_NATIVE_AUX_READ = 0,
    DISP_DP_NATIVE_AUX_WRITE,
    DISP_DP_I2C_OVER_AUX_READ,
}DISP_DP_AUX_OP;

typedef struct _disp_aux_para_t
{
    disp_u32_t connector_index;
    disp_u32_t addr;
    disp_u32_t size;
    pdisp_u8_t pbuf;
    DISP_DP_AUX_OP op;
}disp_aux_para_t, *pdisp_aux_para_t;

/* HDR Metadata ref: CTA-861-G */
typedef struct _hdr_static_metadata_t
{
    disp_u8_t eotf;             // bits, DISP_EOTF_TYPE
    disp_u8_t metadata_type;    // bits, HDMI_STATIC_METADATA_TYPE
    disp_u16_t max_cll;         // real luminance(nits) = 50 * power(2, val/32)
    disp_u16_t max_fall;        // real luminance(nits) = 50 * power(2, val/32)
    disp_u16_t min_cll;         // real luminance(nits) = max_cll_nits * (pow(val/255, 2) / 100)
}hdr_static_metadata_t, *phdr_static_metadata_t;

typedef struct _disp_connector_info_t
{
    disp_u8_t connector_index;

    // disp_display_info_t
    disp_u32_t bpc;
    disp_u32_t color_formats;
    disp_u32_t colorimetry_support;
    disp_u16xy_t rgbw_position[4];
    hdr_static_metadata_t hdr_static_metadata;

    disp_u8_t free_sync_support;
    disp_u16_t min_vfreq;
    disp_u16_t max_vfreq;
}disp_connector_info_t, *pdisp_connector_info_t;

typedef struct _disp_codec_para_t
{
    DISP_POWER_STATE power;
    disp_u8_t encoder_index;
    disp_u8_t present;//sink is connected, and support audio
    disp_u8_t eld_valid;//sink is connected, and power on
    disp_u64_t port_id;
    disp_u16_t manufacture_name;
    disp_u16_t product_code;
}disp_codec_para_t, *pdisp_codec_para_t;

typedef enum _DISP_CTS_TYPE
{
    DISP_CTS_TYPE_DP = 0,
    DISP_CTS_TYPE_HDMI,
}DISP_CTS_TYPE;

typedef struct _disp_cts_dp_para_t
{
    disp_u8_t link_rate;
    disp_u8_t lane_count;
    disp_u8_t tps;
    disp_u8_t async_clock;
    disp_u8_t range_type;
    disp_u8_t color_format;
    disp_u8_t ycbcr_coefficients;
    disp_u8_t enhanced_frame;
    disp_u8_t bpc;
    disp_u8_t colorimetry;

    disp_u32_t valid_bits;
}disp_cts_dp_para_t, *pdisp_cts_dp_para_t;

typedef struct _disp_cts_hdmi_para_t
{
    disp_u32_t tmds_bit_clock_ratio;
    disp_u32_t read_request_enable;
    disp_u32_t scrambling_enable;
}disp_cts_hdmi_para_t, *pdisp_cts_hdmi_para_t;

typedef struct _disp_cts_para_t
{
    disp_u32_t connector_index;
    DISP_CTS_TYPE type;
    union
    {
        disp_cts_dp_para_t dp;
        disp_cts_hdmi_para_t hdmi;
    }para;
}disp_cts_para_t, *pdisp_cts_para_t;

#define DISP_DPCD_DPRX_EVENT_START 0x200
typedef enum _DISP_HPD_TYPE
{
    DISP_HPD_TYPE_IDLE = 0,
    DISP_HPD_TYPE_PLUG_IN,
    DISP_HPD_TYPE_EVENT,
    DISP_HPD_TYPE_PLUG_OUT,
}DISP_HPD_TYPE;

typedef struct _disp_get_hpd_state_t
{
    disp_u32_t connector_index; // in
    DISP_HPD_TYPE hpd_state;    // out
}disp_get_hpd_state_t, *pdisp_get_hpd_state_t;

typedef struct _disp_i2c_staus_t
{
    disp_u32_t connector_index;
    disp_u32_t i2c_nack_count;
    disp_u32_t i2c_defer_count;
}disp_get_i2c_status_t, *pdisp_get_i2c_status_t;

typedef enum _DISP_BACKLIGHT_QUERY_TYPE
{
    DISP_BACKLIGHT_QUERY_CAPS = 0,
    DISP_BACKLIGHT_QUERY_LEVLE,
    DISP_BACKLIGHT_QUERY_FREQ,
    DISP_BACKLIGHT_QUERY_ADAPTIVE,
} DISP_BACKLIGHT_QUERY_TYPE;

typedef enum _DISP_BACKLIGHT_LEVEL_TYPE
{
    DISP_BACKLIGHT_LEVEL_THOUSANDS_PERCENT = 0, // value 1000 = 1%
    DISP_BACKLIGHT_LEVEL_MILLINITS, // value 1000 = 1 nits
} DISP_BACKLIGHT_LEVEL_TYPE;

typedef union _disp_backlight_query_parma_t
{
    struct
    {
        disp_u32_t level           : 1; // basic caps, if backlight support level must support
        disp_u32_t freq            : 1;
        disp_u32_t smooth          : 1;
        disp_u32_t adaptive        : 1;
        disp_u32_t nits            : 1;
        disp_u32_t reserved             :27;
    } caps;

    struct
    {
        DISP_BACKLIGHT_LEVEL_TYPE type;
        disp_u32_t level_val;
        disp_u8_t enable;
    } level;

    struct
    {
        disp_u32_t freq_val;
        disp_u8_t enable;
    } freq;

    struct
    {
        DISP_BACKLIGHT_LEVEL_TYPE type;
        disp_u32_t adaptive_min_level;
        disp_u32_t adaptive_max_level;
        disp_u8_t enable;
    } adaptive;
} disp_backlight_query_parma_t;

typedef struct _disp_backlight_query_t
{
    disp_u8_t connector_index;
    DISP_BACKLIGHT_QUERY_TYPE type;
    disp_backlight_query_parma_t param;
} disp_backlight_query_t, *pdisp_backlight_query_t;

typedef struct _disp_backlight_set_t
{
    disp_u8_t connector_index;

    struct
    {
        disp_u8_t disable_level             : 1;
        disp_u8_t set_level                 : 1;
        disp_u8_t disable_freq              : 1;
        disp_u8_t set_freq                  : 1;
        disp_u8_t disable_adaptive          : 1;
        disp_u8_t set_adaptive              : 1;
        disp_u8_t set_smooth                : 1;
        disp_u8_t smooth_enable             : 1;
    } flag;

    DISP_BACKLIGHT_LEVEL_TYPE level_type;
    disp_u32_t level_val;
    disp_u32_t freq_val;
    disp_u32_t adaptive_min_level;
    disp_u32_t adaptive_max_level;
} disp_backlight_set_t, *pdisp_backlight_set_t;

typedef enum _DISP_PSR_SET_TYPE
{
    DISP_PSR_EXIT = 0,
    DISP_PSR_ENTRY,
    DISP_PSR_UPDATE,
    DISP_PSR_UPDATE_BURST_START,
    DISP_PSR_UPDATE_BURST_END,
    DISP_PSR_SLEEP,
} DISP_PSR_SET_TYPE;

typedef struct _disp_psr_query_t
{
    disp_u8_t connector_index;
    disp_u8_t psr_version;
    /* Psr detail config, that may need export to OS/gpu0 */
} disp_psr_query_t, *pdisp_psr_query_t;

typedef struct _disp_psr_set_t
{
    disp_u8_t connector_index;
    DISP_PSR_SET_TYPE psr_type;
} disp_psr_set_t, *pdisp_psr_set_t;

typedef struct _disp_wait_blank_t
{
    disp_u8_t crtc_index;
    disp_u8_t hblank;           // 0-vblank, 1-hblank
    disp_u8_t inblank;          // 0-in active, 1-in blank
    disp_u8_t wait_over_first;  // inblank wait active over first, inactive wait blank over first.
    disp_u32_t max_try_times;
    disp_u32_t left_try_times;  // out, 0 means timeout
    disp_u32_t reg_val;
} disp_wait_blank_t, *pdisp_wait_blank_t;

/**
 * display module interface begin
*/
#ifdef DISP_WIN
#define DISP_DLLEXPORTS __declspec(dllexport)
#define DISP_DLLIMPORT __declspec(dllimport)
#else
#define DISP_DLLEXPORTS
#define DISP_DLLIMPORT
#endif

DISP_DLLEXPORTS DISP_STATUS display_device_hw_init(pdisp_void_t pdisplay_device);
DISP_DLLEXPORTS DISP_STATUS display_device_init(pdisp_device_init_t pdisp_device_init);
DISP_DLLEXPORTS DISP_STATUS display_device_deinit(pdisp_void_t pdisp_device);
DISP_DLLEXPORTS DISP_STATUS display_query_resource(pdisp_void_t pdisp_device, pdisp_query_resource_t pdisp_query_resource);
DISP_DLLEXPORTS DISP_STATUS display_detect_connector(pdisp_void_t pdisp_device, pdisp_detect_connector_t pdisp_detect_connector);
DISP_DLLEXPORTS DISP_STATUS display_get_modelist(pdisp_void_t pdisp_device, pdisp_mode_list_t pdisp_mode_list);
DISP_DLLEXPORTS DISP_STATUS display_get_edid(pdisp_void_t pdisp_device, pdisp_get_edid_t pdisp_get_edid);
DISP_DLLEXPORTS DISP_STATUS display_crtc_dpms(pdisp_void_t pdisp_device, disp_crtc_dpms_t crtc_dpms);
DISP_DLLEXPORTS DISP_STATUS display_crtc_set_mode(pdisp_void_t pdisp_device, disp_crtc_set_mode_t crtc_set_mode);
DISP_DLLEXPORTS DISP_STATUS display_encoder_dpms(pdisp_void_t pdisp_device, disp_encoder_dpms_t encoder_dpms);
DISP_DLLEXPORTS DISP_STATUS display_encoder_set_mode(pdisp_void_t pdisp_device, disp_encoder_set_mode_t encoder_set_mode);
DISP_DLLEXPORTS DISP_STATUS display_plane_check(pdisp_void_t pdisp_device, pdisp_plane_update_para_t pupdate_para);
DISP_DLLEXPORTS DISP_STATUS display_plane_update(pdisp_void_t pdisp_device, pdisp_plane_update_para_t pupdate_para);
DISP_DLLEXPORTS DISP_STATUS display_plane_get_addr(pdisp_void_t pdisp_device, pdisp_get_fb_addr_t paddr);
DISP_DLLEXPORTS DISP_STATUS display_intr_enable(pdisp_void_t pdisp_device, disp_u8_t enable, disp_intr_t intr);
DISP_DLLEXPORTS DISP_STATUS display_intr_get_status(pdisp_void_t pdisp_device, pdisp_intr_t pintr);
DISP_DLLEXPORTS DISP_STATUS display_intr_multi_vsync_enable(pdisp_void_t pdisp_device, disp_u8_t enable);
DISP_DLLEXPORTS DISP_STATUS display_intr_clear_vsync_status(pdisp_void_t pdisp_device, disp_s32_t crtc_index);
DISP_DLLEXPORTS DISP_STATUS display_get_itg_counter(pdisp_void_t pdisp_device, pdisp_itg_counter_t pitg_cnt);
DISP_DLLEXPORTS DISP_STATUS display_set_signature(pdisp_void_t pdisp_device, disp_sig_set_t sig_set);
DISP_DLLEXPORTS DISP_STATUS display_get_signature(pdisp_void_t pdisp_device, pdisp_sig_get_t psig_get);
DISP_DLLEXPORTS DISP_STATUS display_get_sgnt_golden(pdisp_void_t pdisp_device, pdisp_sig_get_t psig_get);
DISP_DLLEXPORTS DISP_STATUS display_set_gamma(pdisp_void_t pdisp_device, pdisp_gamma_info_t pgamma_info);
DISP_DLLEXPORTS DISP_STATUS display_set_csc(pdisp_void_t pdisp_device, pdisp_csc_para_t pcsc_para);
DISP_DLLEXPORTS DISP_STATUS display_aux_transfer(pdisp_void_t pdisp_device, pdisp_aux_para_t paux_para);
DISP_DLLEXPORTS DISP_STATUS display_get_connector_info(pdisp_void_t pdisp_device, pdisp_connector_info_t pconnector_info);
DISP_DLLEXPORTS DISP_STATUS display_codec_dpms(pdisp_void_t pdisp_device, pdisp_codec_para_t hdac_para);
DISP_DLLEXPORTS DISP_STATUS display_set_cts(pdisp_void_t pdisp_device, pdisp_cts_para_t pcts_para);
DISP_DLLEXPORTS DISP_STATUS display_get_hpd_state(pdisp_void_t pdisp_device, pdisp_get_hpd_state_t phpd_state);
DISP_DLLEXPORTS DISP_STATUS display_get_i2c_status(pdisp_void_t pdisp_device, pdisp_get_i2c_status_t pdisp_get_i2c_status);
DISP_DLLEXPORTS DISP_STATUS display_check_underflow(pdisp_void_t pdisp_device);
DISP_DLLEXPORTS DISP_STATUS display_notify_irq_hpd(pdisp_void_t pdisp_device, disp_u8_t connector_index);
DISP_DLLEXPORTS DISP_STATUS display_backlight_query(pdisp_void_t pdisp_device, pdisp_backlight_query_t pquery);
DISP_DLLEXPORTS DISP_STATUS display_backlight_set(pdisp_void_t pdisp_device, pdisp_backlight_set_t pset);
DISP_DLLEXPORTS DISP_STATUS display_psr_query(pdisp_void_t pdisp_device, pdisp_psr_query_t pquery);
DISP_DLLEXPORTS DISP_STATUS display_psr_set(pdisp_void_t pdisp_device, pdisp_psr_set_t pset);
DISP_DLLEXPORTS DISP_STATUS display_hda_handle(pdisp_void_t pdisp_device);
#endif
