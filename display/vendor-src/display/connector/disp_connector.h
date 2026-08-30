/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __DISP_CONNECTOR_H__
#define __DISP_CONNECTOR_H__

#include "display.h"

#define BITS_PER_BYTE        8
#define BITS_PER_TYPE(type)    (sizeof(type) * BITS_PER_BYTE)
#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))
#define BITS_TO_LONGS(nr)    DIV_ROUND_UP(nr, BITS_PER_TYPE(disp_s64_t))

//below should consider 32bit or 64bit system
#define BITS_PER_LONG       64
#define BITMAP_FIRST_WORD_MASK(start) (~0UL << ((start) & (BITS_PER_LONG - 1)))
#define BITMAP_LAST_WORD_MASK(nbits) (~0UL >> (-(nbits) & (BITS_PER_LONG - 1)))

#define BIT_MASK(nr)        (UL(1) << ((nr) % BITS_PER_LONG))
#define BIT_WORD(nr)        ((nr) / BITS_PER_LONG)

#define BIT(nr)            (1 << (nr))

/*
 * Divide positive or negative dividend by positive or negative divisor
 * and round to closest integer. Result is undefined for negative
 * divisors if the dividend variable type is unsigned and for negative
 * dividends if the divisor variable type is unsigned.
 */
#ifdef DISP_WIN
//#define DIV_ROUND_CLOSEST(x, divisor) ((((x) > 0) == ((divisor) > 0)) ? ((x) + (divisor)/2)/(divisor) : ((x) - (divisor)/2)/(divisor))
static inline disp_u64_t DIV_ROUND_CLOSEST(disp_u32_t x, disp_u32_t divisor)
{
    return ((x > 0) == (divisor > 0)) ? // smaller is impossible, always larger
        ((disp_u64_t)x + divisor/2) / divisor : ((disp_u64_t)x - divisor/2) / divisor;
}
#else
#define DIV_ROUND_CLOSEST(x, divisor)(            \
{                            \
    typeof(x) __x = x;                \
    typeof(divisor) __d = divisor;            \
    (((typeof(x))-1) > 0 ||                \
     ((typeof(divisor))-1) > 0 ||            \
     (((__x) > 0) == ((__d) > 0))) ?        \
        (((__x) + ((__d) / 2)) / (__d)) :    \
        (((__x) - ((__d) / 2)) / (__d));    \
}                            \
)
#endif

////////here should use do_div to caculate
/*
 * Same as above but for u64 dividends. divisor must be a 32-bit
 * number.
 */
#ifdef DISP_WIN
static inline disp_u64_t DIV_ROUND_CLOSEST_ULL(disp_u64_t x, disp_u64_t divisor)
{
    return (x + divisor/2) / divisor;
}
#else
#define DIV_ROUND_CLOSEST_ULL(x, divisor)(        \
{                            \
    typeof(divisor) __d = divisor;            \
    disp_u64_t _tmp = (x) + (__d) / 2;    \
    _tmp = _tmp / __d;                \
    _tmp;                        \
}                            \
)
#endif

#define PICOS2KHZ(a) (1000000000UL/(a))
#define KHZ2PICOS(a) (1000000000UL/(a))

typedef enum _DISP_CONNECT_STATE
{
    DISP_CONNECTTED,
    DISP_DISCONNECTED,
}DISP_CONNECT_STATE;

typedef enum _DISP_CONNECTOR_TYPE
{
    DISP_CONNECTOR_HDMI,
    DISP_CONNECTOR_DP,
    DISP_CONNECTOR_eDP,
    DISP_CONNECTOR_DVO,
}DISP_CONNECTOR_TYPE;

typedef enum _DISP_MODE_SUBCONNECTOR_TYPE
{
    DISP_MODE_SUBCONNECTOR_AUTOMATIC    = 0,  /* DVI-I, TV     */
    DISP_MODE_SUBCONNECTOR_UNKNOWN      = 0,  /* DVI-I, TV, DP */
    DISP_MODE_SUBCONNECTOR_VGA          = 1,  /*            DP */
    DISP_MODE_SUBCONNECTOR_DVID         = 3,  /* DVI-I,     DP */
    DISP_MODE_SUBCONNECTOR_DVIA         = 4,  /* DVI-I         */
    DISP_MODE_SUBCONNECTOR_COMPOSITE    = 5,  /*        TV     */
    DISP_MODE_SUBCONNECTOR_SVIDEO       = 6,  /*        TV     */
    DISP_MODE_SUBCONNECTOR_COMPONENT    = 8,  /*        TV     */
    DISP_MODE_SUBCONNECTOR_SCART        = 9,  /*        TV     */
    DISP_MODE_SUBCONNECTOR_DISPLAYPORT  = 10, /*            DP */
    DISP_MODE_SUBCONNECTOR_HDMIA        = 11, /*            DP */
    DISP_MODE_SUBCONNECTOR_NATIVE       = 15, /*            DP */
    DISP_MODE_SUBCONNECTOR_WIRELESS     = 18, /*            DP */
}DISP_MODE_SUBCONNECTOR_TYPE;

#define MAX_ELD_BYTES    128
typedef struct _disp_audio_info_t
{
    /** @eld: EDID-like data, if present */
    disp_u8_t eld[MAX_ELD_BYTES];
    /** @latency_present: AV delay info from ELD, if found */
    disp_u8_t latency_present[2];
    /**
     * @video_latency: Video latency info from ELD, if found.
     * [0]: progressive, [1]: interlaced
     */
    disp_s32_t video_latency[2];
    /**
     * @audio_latency: audio latency info from ELD, if found
     * [0]: progressive, [1]: interlaced
     */
    disp_s32_t audio_latency[2];
}disp_audio_info_t, *pdisp_audio_info_t;

/* Short Audio Descriptor */
typedef struct _cea_sad_t
{
    disp_u8_t format;
    disp_u8_t channels; /* max number of channels - 1 */
    disp_u8_t freq;
    disp_u8_t byte2; /* meaning depends on format */
}cea_sad_t, *pcea_sad_t;

typedef enum _SUBPIXEL_ORDER
{
    SUBPIXEL_ORDER_UNKNOWN = 0,
    SUBPIXEL_ORDER_HORIZONTAL_RGB,
    SUBPIXEL_ORDER_HORIZONTAL_BGR,
    SUBPIXEL_ORDER_VERTICAL_RGB,
    SUBPIXEL_ORDER_VERTICAL_BGR,
    SUBPIXEL_ORDER_NONE,
}SUBPIXEL_ORDER;

/**
 * struct disp_scrambling: sink's scrambling support.
 */
typedef struct _disp_scrambling_t
{
    /**
     * @supported: scrambling supported for rates > 340 Mhz.
     */
    disp_u8_t supported;
    /**
     * @low_rates: scrambling supported for rates <= 340 Mhz.
     */
    disp_u8_t low_rates;
}disp_scrambling_t, *pdisp_scrambling_t;

/*
 * struct disp_scdc - Information about scdc capabilities of a HDMI 2.0 sink
 *
 * Provides SCDC register support and capabilities related information on a
 * HDMI 2.0 sink. In case of a HDMI 1.4 sink, all parameter must be 0.
 */
typedef struct _disp_scdc_t
{
    /**
     * @supported: status control & data channel present.
     */
    disp_u8_t supported;
    /**
     * @read_request: sink is capable of generating scdc read request.
     */
    disp_u8_t read_request;
    /**
     * @scrambling: sink's scrambling capabilities
     */
    disp_scrambling_t scrambling;
}disp_scdc_t, *pdisp_scdc_t;

/**
 * struct disp_hdmi_dsc_cap - DSC capabilities of HDMI sink
 *
 * Describes the DSC support provided by HDMI 2.1 sink.
 * The information is fetched fom additional HFVSDB blocks defined
 * for HDMI 2.1.
 */
typedef struct _disp_hdmi_dsc_cap_t
{
    /** @v_1p2: flag for dsc1.2 version support by sink */
    disp_u8_t v_1p2;

    /** @native_420: Does sink support DSC with 4:2:0 compression */
    disp_u8_t native_420;

    /**
     * @all_bpp: Does sink support all bpp with 4:4:4: or 4:2:2
     * compressed formats
     */
    disp_u8_t all_bpp;

    /**
     * @bpc_supported: compressed bpc supported by sink : 10, 12 or 16 bpc
     */
    disp_u8_t bpc_supported;

    /** @max_slices: maximum number of Horizontal slices supported by */
    disp_u8_t max_slices;

    /** @clk_per_slice : max pixel clock in MHz supported per slice */
    disp_s32_t clk_per_slice;

    /** @max_lanes : dsc max lanes supported for Fixed rate Link training */
    disp_u8_t max_lanes;

    /** @max_frl_rate_per_lane : maximum frl rate with DSC per lane */
    disp_u8_t max_frl_rate_per_lane;

    /** @total_chunk_kbytes: max size of chunks in KBs supported per line*/
    disp_u8_t total_chunk_kbytes;
}disp_hdmi_dsc_cap_t, *pdisp_hdmi_dsc_cap_t;

/**
 * struct disp_hdmi_info - runtime information about the connected HDMI sink
 *
 * Describes if a given display supports advanced HDMI 2.0 features.
 * This information is available in CEA-861-F extension blocks (like HF-VSDB).
 */
typedef struct _disp_hdmi_info_t
{
    /** @scdc: sink's scdc support and capabilities */
    disp_scdc_t scdc;

    /**
     * @y420_vdb_modes: bitmap of modes which can support ycbcr420
     * output only (not normal RGB/YCBCR444/422 outputs). The max VIC
     * defined by the CEA-861-G spec is 219, so the size is 256 bits to map
     * up to 256 VICs.
     */
    disp_u32_t y420_vdb_modes[BITS_TO_LONGS(256)];

    /**
     * @y420_cmdb_modes: bitmap of modes which can support ycbcr420
     * output also, along with normal HDMI outputs. The max VIC defined by
     * the CEA-861-G spec is 219, so the size is 256 bits to map up to 256
     * VICs.
     */
    disp_u32_t y420_cmdb_modes[BITS_TO_LONGS(256)];

    /** @y420_cmdb_map: bitmap of SVD index, to extraxt vcb modes */
    disp_u64_t y420_cmdb_map;

    /** @y420_dc_modes: bitmap of deep color support index */
    disp_u8_t y420_dc_modes;

    /** @max_frl_rate_per_lane: support fixed rate link */
    disp_u8_t max_frl_rate_per_lane;

    /** @max_lanes: supported by sink */
    disp_u8_t max_lanes;

    /** @dsc_cap: DSC capabilities of the sink */
    disp_hdmi_dsc_cap_t dsc_cap;
}disp_hdmi_info_t, *pdisp_hdmi_info_t;

/**
 * struct disp_monitor_range_info - Panel's Monitor range in EDID for
 * &disp_display_info
 *
 * This struct is used to store a frequency range supported by panel
 * as parsed from EDID's detailed monitor range descriptor block.
 *
 * @min_vfreq: This is the min supported refresh rate in Hz from
 *             EDID's detailed monitor range.
 * @max_vfreq: This is the max supported refresh rate in Hz from
 *             EDID's detailed monitor range
 */
typedef struct _disp_monitor_range_info_t
{
    disp_u16_t min_vfreq;
    disp_u16_t max_vfreq;
    disp_u16_t min_hfreq_khz;
    disp_u16_t max_hfreq_khz;
    disp_s32_t max_pixel_clock; // khz
}disp_monitor_range_info_t, *pdisp_monitor_range_info_t;

/**
 * struct hdr_sink_metadata - HDR sink metadata
 *
 * Metadata Information read from Sink's EDID
 */
typedef struct _hdr_sink_metadata_t
{
    /**
     * @metadata_type: Static_Metadata_Descriptor_ID.
     */
    disp_u32_t metadata_type;
    /**
     * @hdmi_type1: HDR Metadata Infoframe.
     */
    union
    {
        hdr_static_metadata_t hdmi_type1;
    }data;
}hdr_sink_metadata_t, *phdr_sink_metadata_t;

typedef struct _disp_dsc_caps_t
{
    disp_u8_t max_linebuf_depth;
    disp_u8_t min_increment_of_bpp; // same as dpcd 0x6f value: 0, 1, 2, 3, 4 -> 1/16, 1/8, 1/4, 1/2, 1
    disp_u8_t dsc_max_frl_rate; // hdmi use
    disp_u16_t min_rc_buf_size_kb;
    disp_u32_t max_slice_width;
    disp_u32_t min_slice_height;
    disp_u32_t max_throughput_mode0_mps;
    disp_u32_t max_throughput_mode1_mps;

    union
    {
        struct {
            disp_u32_t fec_caps                     : 1;
            disp_u32_t dsc_caps                     : 1;
            disp_u32_t dsc_ver_1_1                  : 1;
            disp_u32_t dsc_ver_1_2                  : 1;
            disp_u32_t dp                           : 1;
            disp_u32_t hdmi                         : 1;
            disp_u32_t block_prediction             : 1;
            disp_u32_t rgb                          : 1;
            disp_u32_t yuv_444                      : 1;
            disp_u32_t yuv_simple422                : 1;
            disp_u32_t yuv_native422                : 1;
            disp_u32_t yuv_native420                : 1;
            disp_u32_t bpc8                         : 1;
            disp_u32_t bpc10                        : 1;
            disp_u32_t bpc12                        : 1;
            disp_u32_t bpc16                        : 1;
            disp_u32_t slice_num_1                  : 1;
            disp_u32_t slice_num_2                  : 1;
            disp_u32_t slice_num_4                  : 1;
            disp_u32_t slice_num_6                  : 1;
            disp_u32_t slice_num_8                  : 1;
            disp_u32_t slice_num_10                 : 1;
            disp_u32_t slice_num_12                 : 1;
            disp_u32_t slice_num_16                 : 1;
            disp_u32_t slice_num_20                 : 1;
            disp_u32_t slice_num_24                 : 1;
            disp_u32_t reserved                     : 6;
        }bits;
        disp_u32_t value;
    }flag;
}disp_dsc_caps_t, *pdisp_dsc_caps_t;

typedef struct _disp_display_info_t
{
    /**
     * @width_mm: Physical width in mm.
     */
    disp_u32_t width_mm;

    /**
     * @height_mm: Physical height in mm.
     */
    disp_u32_t height_mm;

    /**
     * @bpc: Maximum bits per color channel. Used by HDMI and DP outputs.
     */
    disp_u32_t bpc;

    /** Color Characteristics: RGBW position,
     ** ref edid v1.4: unit 1/1024
     */
    disp_u16xy_t rgbw_position[4];

    /**
     * @subpixel_order: Subpixel order of LCD panels.
     */
    SUBPIXEL_ORDER subpixel_order;

    /**
     * @panel_orientation: Read only connector property for built-in panels,
     * indicating the orientation of the panel vs the device's casing.
     * disp_connector_init() sets this to disp_MODE_PANEL_ORIENTATION_UNKNOWN.
     * When not UNKNOWN this gets used by the disp_fb_helpers to rotate the
     * fb to compensate and gets exported as prop to userspace.
     */
    disp_s32_t panel_orientation;

    /**
     * @color_formats: HDMI Color formats, selects between RGB and YCrCb
     * modes. Used disp_COLOR_FORMAT\_ defines, which are _not_ the same ones
     * as used to describe the pixel format in framebuffers, and also don't
     * match the formats in @bus_formats which are shared with v4l.
     */
    disp_u32_t color_formats;

    /**
     * @bus_formats: Pixel data format on the wire, somewhat redundant with
     * @color_formats. Array of size @num_bus_formats encoded using
     * MEDIA_BUS_FMT\_ defines shared with v4l and media drivers.
     */
    disp_u32_t *bus_formats;
    /**
     * @num_bus_formats: Size of @bus_formats array.
     */
    disp_s32_t num_bus_formats;

    /**
     * @bus_flags: Additional information (like pixel signal polarity) for
     * the pixel data on the bus, using &enum disp_bus_flags values
     * DISP_BUS_FLAGS\_.
     */
    disp_u32_t bus_flags;

    /**
     * @max_tmds_clock: Maximum TMDS clock rate supported by the
     * sink in kHz. 0 means undefined.
     */
    disp_s32_t max_tmds_clock;

    /**
     * @quirks: EDID based quirks. Internal to EDID parsing.
     */
    disp_u32_t quirks;

    /**
     * @dvi_dual: Dual-link DVI sink?
     */
    disp_u8_t dvi_dual;

    /**
     * @is_hdmi: True if the sink is an HDMI device.
     *
     * This field shall be used instead of calling
     * disp_detect_hdmi_monitor() when possible.
     */
    disp_u8_t is_hdmi;

    /**
     * @has_hdmi_infoframe: Does the sink support the HDMI infoframe?
     */
    disp_u8_t has_hdmi_infoframe;

    /**
     * @rgb_quant_range_selectable: Does the sink support selecting
     * the RGB quantization range?
     */
    disp_u8_t rgb_quant_range_selectable;

    /**
     * @edid_hdmi_dc_modes: Mask of supported hdmi deep color modes. Even
     * more stuff redundant with @bus_formats.
     */
    disp_u8_t edid_hdmi_dc_modes;

    /**
     * @cea_rev: CEA revision of the HDMI sink.
     */
    disp_u8_t cea_rev;

    /**
     * @non_desktop: Non desktop display (HMD).
     */
    disp_u8_t non_desktop;

    /**
     * @hdmi: advance features of a HDMI sink.
     */
    disp_hdmi_info_t hdmi;

    /**
     * @monitor_range: Frequency range supported by monitor range descriptor
     */
    disp_monitor_range_info_t monitor_range;
    
    /** @hdr_sink_metadata: HDR Metadata Information read from sink */
    hdr_sink_metadata_t hdr_sink_metadata;

    /** colorimetry, DISP_COLORIMETRY bit state */
    disp_u32_t colorimetry_support;

    /** caps of a dp sink */
    pdisp_void_t dp_sink_caps;

    /** dsc sink caps */
    disp_dsc_caps_t dsc_sink_caps;

    /** eDP info */
    pdisp_void_t edp_info;
}disp_display_info_t, *pdisp_display_info_t;

/**
 * struct disp_max_mode - max mode
 *
 * record max mode from added mode
 */
typedef struct _disp_max_mode_t
{
    disp_s16_t w;
    disp_s16_t h;
}disp_max_mode_t, *pdisp_max_mode_t;

struct disp_connector_state
{
    disp_u32_t crtc_index;
    disp_u32_t encoder_index;
    DISP_CONNECT_STATE connected;
    disp_u8_t bfake;
};

#define DISP_EDID_MAX_LENGTH 512 // hex 4 block
typedef struct _disp_connector_t
{
    disp_void_t *disp_device;
    disp_s32_t index;
    DISP_CONNECTOR_TYPE type;

    disp_u8_t edid_block_cnt;
    disp_u8_t pedid[DISP_EDID_MAX_LENGTH];
    disp_u8_t edid_origin_csum; // last block(first invalid block if edid failed)
    disp_mode_list_t mode_list;
    disp_display_info_t disp_info;
    disp_audio_info_t audio_info;

    disp_s32_t max_clk_khz;
    pdisp_display_mode_t native_mode;
    struct disp_connector_func* pfunc;
    pdisp_void_t ddc;
    struct disp_connector_state state;

    union
    {
        disp_u32_t uint;
        struct
        {
            disp_u32_t add_mode_to_modelist         : 1;
            disp_u32_t use_safe_mode                : 1;
            disp_u32_t cheater                      : 1;
            disp_u32_t dpcd_set_power_failed        : 1;
            disp_u32_t dpcd_get_failed              : 1;
            disp_u32_t dpcd_get_dsc_failed          : 1;
            disp_u32_t reserved                     :26;
        } info;
    } flag;
}disp_connector_t, *pdisp_connector_t;

typedef struct _i2c_msg_t
{
    disp_u16_t addr;    /* slave address            */
    disp_u16_t flags;
#define I2C_M_RD        0x0001    /* read data, from slave to master */
                        /* I2C_M_RD is guaranteed to be 0x0001! */
#define I2C_M_TEN       0x0010    /* this is a ten bit chip address */
#define I2C_M_DMA_SAFE        0x0200    /* the buffer of this message is DMA safe */
                    /* makes only sense in kernelspace */
                    /* userspace buffers are copied anyway */
#define I2C_M_RECV_LEN    0x0400    /* length will be first received byte */
#define I2C_M_NO_RD_ACK    0x0800    /* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_IGNORE_NAK    0x1000    /* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_REV_DIR_ADDR    0x2000    /* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_NOSTART    0x4000    /* if I2C_FUNC_NOSTART */
#define I2C_M_STOP    0x8000    /* if I2C_FUNC_PROTOCOL_MANGLING */
    disp_u16_t len;    /* msg length    */
    disp_u8_t *buf;    /* pointer to msg data    */
}i2c_msg_t, *pi2c_msg_t;

struct disp_connector_func
{
    DISP_CONNECT_STATE (*detect)(pdisp_connector_t pdisp_connector);
    disp_u32_t (*probe_edid_block)(pdisp_connector_t pdisp_connector, pdisp_u8_t buf, disp_u8_t block, disp_u16_t len);
    DISP_HPD_TYPE (*get_hpd_status)(pdisp_connector_t pdisp_connector);
    DISP_STATUS (*get_i2c_status)(pdisp_connector_t pdisp_connector, pdisp_get_i2c_status_t pi2c_status);
    DISP_STATUS (*backlight_query)(pdisp_connector_t pdisp_connector, pdisp_backlight_query_t pquery);
    DISP_STATUS (*backlight_set)(pdisp_connector_t pdisp_connector, pdisp_backlight_set_t pset);
    DISP_STATUS (*psr_query)(pdisp_connector_t pdisp_connector, pdisp_psr_query_t pquery);
    DISP_STATUS (*psr_set)(pdisp_connector_t pdisp_connector, pdisp_psr_set_t pset);
    DISP_STATUS (*irq_hpd_handle)(pdisp_connector_t pdisp_connector);
};

DISP_STATUS disp_connector_sw_init(pdisp_connector_t pdisp_connector);
DISP_STATUS disp_connector_sw_deinit(pdisp_connector_t pdisp_connector);
DISP_STATUS disp_connector_probe_edid(pdisp_connector_t pdisp_connector);
DISP_STATUS disp_connector_get_i2c_status(pdisp_connector_t pdisp_connector, pdisp_get_i2c_status_t pdisp_i2c_status);

#endif
