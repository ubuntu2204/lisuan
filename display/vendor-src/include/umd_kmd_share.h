/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef  UMD_KMD_SHARE_DATA
#define  UMD_KMD_SHARE_DATA

#define UMD_KMD_ALLOCATION_SIGNATURE   0xbeef0001
#define MAGIC_COOKIE    'UMD'
#define MAX_SEG_COUNT    4

#define RESOURCE_DESC_SIGNATURE_UMD        0xbeef0010
#define RESOURCE_DESC_SIGNATURE_UMD_LEGACY 0xbeef0011
#define RESOURCE_DESC_SIGNATURE_KMD        0xbeef0012

#define PCI_ID_G100             0x5000
#define PCI_ID_G100_VF          0x0100

#define PCI_ID_G200             0x5200

#define DEVICE_MASK             0xFF00 
#define CHIP_MASK               0x00FF

#define CHIP_G100 (PCI_ID_G100 & DEVICE_MASK)
#define CHIP_G200 (PCI_ID_G200 & DEVICE_MASK)

#ifndef __linux__
typedef unsigned __int64  uint64_t;
#else
 typedef unsigned long long uint64_t;
#include <linux/list.h>
#endif

#define INVALID_CL_SLOT     0xFFFFFFFF 
#define CP_RESERVED_FMT     0xFF

#ifndef LS_POINTER_64
/* build for WOW64 UserModeDX */
#if     defined(BUILD_WOW64) // build for WOW64
#define LS_POINTER_64  __ptr64
#define LS_POINTER_64_INT unsigned __int64
#pragma warning(disable: 4244) // warning in typecast from __int64 to __int32
#else 
#define LS_POINTER_64 
#define LS_POINTER_64_INT ULONG_PTR
#endif
#endif



typedef struct allocation_info_flags_wddm2_0_s
{
    union
    {
        struct
        {
            unsigned int    cpu_visible : 1;                    // 0x00000001
            unsigned int    permanent_sysmem : 1;               // 0x00000002
            unsigned int    cached : 1;                         // 0x00000004
            unsigned int    protect : 1;                        // 0x00000008
            unsigned int    existing_sysmem : 1;                // 0x00000010
            unsigned int    existing_kernel_sysmem : 1;         // 0x00000020
            unsigned int    from_end_of_segment : 1;            // 0x00000040
            unsigned int    disable_large_page_mapping : 1;     // 0x00000080   // Swizzled 
            unsigned int    overlay : 1;                        // 0x00000100
            unsigned int    capture : 1;                        // 0x00000200
            unsigned int    create_in_vpr : 1;                  // 0x00000400   // UseAlternateVA
            unsigned int    dxgk_alloc_reserved16 : 1;          // 0x00000800   // Reserved
            unsigned int    reserved15 : 1;                     // 0x00001000   // LinkMirrored
            unsigned int    reserved14 : 1;                     // 0x00002000   // LinkInstanced
            unsigned int    history_buffer : 1;                 // 0x00004000
            unsigned int    accessed_physically : 1;            // 0x00008000
            unsigned int    explicit_residency_notification : 1;// 0x00010000
            unsigned int    hardware_protected : 1;             // 0x00020000
            unsigned int    cpu_visible_on_demand : 1;          // 0x00040000 Following fields are RESERVED and
            unsigned int    is_kmd_stand_alloc : 1;             // 0x00080000 
            unsigned int    is_kmd_staging_suf : 1;             // 0x00100000 
            unsigned int    reserve11 : 1;                      // 0x00200000 
            unsigned int    reserve10 : 1;                      // 0x00400000 
            unsigned int    reserve9 : 1;                       // 0x00800000 
            unsigned int    reserve8 : 1;                       // 0x01000000 
            unsigned int    reserve7 : 1;                       // 0x02000000 
            unsigned int    reserve6 : 1;                       // 0x04000000 
            unsigned int    reserve5 : 1;                       // 0x08000000 
            unsigned int    reserve4 : 1;                       // 0x10000000 
            unsigned int    reserve3 : 1;                       // 0x20000000 
            unsigned int    reserve2 : 1;                       // 0x40000000 
            unsigned int    reserve1 : 1;                       // 0x80000000 
        };
        unsigned int value;
    };
} allocation_info_flags_wddm2_0_s;

typedef struct segment_preference_s {
    union {
        struct {
            unsigned int segment_id0 : 5;
            unsigned int direction0  : 1;
            unsigned int segment_id1 : 5;
            unsigned int direction1  : 1;
            unsigned int segment_id2 : 5;
            unsigned int direction2  : 1;
            unsigned int segment_id3 : 5;
            unsigned int direction3  : 1;
            unsigned int segment_id4 : 5;
            unsigned int direction4  : 1;
            unsigned int reserved    : 2;
        };
        unsigned int value;
    };
} segment_preference_s;

typedef enum allocation_shader_type_e
{
    vs = 0,
    ps,
    gs,
    hs,
    ds,
    cs,
}allocation_shader_type_e;

typedef union resource_flags_s
{
    struct {
        unsigned int b_depth : 1;
        unsigned int b_stencil : 1;
        unsigned int b_istiled : 1;
        unsigned int b_gdi_crossadapter : 1; //create from standard allocation
        unsigned int b_isbuffer : 1;
        unsigned int b_nondedicate :1;
        unsigned int b_isprimary : 1;
        unsigned int b_isstaging : 1;
        unsigned int reserved : 24;
    };
    unsigned int value;
}resource_flags_s;

typedef struct resource_desc_share_s
{
    unsigned int            width;
    unsigned int            height;
    unsigned int            depth;
    unsigned int            mip_levels;
    unsigned int            array_size;
    unsigned int            ddi_format;
    unsigned int            hw_format;
    unsigned int            bit_count;
    unsigned int            sample_count;
    unsigned int            quality_levels;
    unsigned int            pitch;
    unsigned int            depth_pitch;
    resource_flags_s        flags;
    unsigned int            resource_dimension;
    unsigned int            reserved[4];
}resource_desc_share_s;

typedef struct usage_hint_s
{
    uint32_t shader       : 1;
    uint32_t shader_type  : 3;
    uint32_t no_overwrite : 1;
    uint32_t is_cpu_accessible : 1;
    uint32_t zero_vram    : 1;
    uint32_t is_cmdbuf    : 1;
    uint32_t              : 24;
}usage_hint_s;

typedef struct allocation_info_s
{
    uint64_t                        size_in_byte;
    uint64_t                        align;
    unsigned int                    priority;
    segment_preference_s            preferred_segment;
    unsigned int                    supported_read_segment_set;
    unsigned int                    supported_write_segment_set;
    unsigned int                    eviction_segment_set;
    unsigned int                    physical_adapter_index;
    allocation_info_flags_wddm2_0_s flags;

    unsigned int                    width;              // todo remove
    unsigned int                    height;             // todo remove
    unsigned int                    bit_count;          // todo remove
    unsigned int                    hw_format;          // todo remove
    unsigned int                    pitch;              // todo remove
    unsigned int                    compress_format;
    unsigned int                    cl_cell_index;
    void* LS_POINTER_64             hcl_cell_node;
    unsigned int                    numerator;
    unsigned int                    denominator;
    unsigned int                    vidpn_source_id;
    unsigned int                    ddi_format;            // todo remove  cast from d3dddiformat
    unsigned int                    num_samples;           // todo remove
    unsigned int                    num_quality_levels;    // todo remove
    unsigned int                    rotation;              //cast from d3dddi_rotation
    bool                            is_tiled;              // todo remove
    bool                            is_present;
    usage_hint_s                    usage_hint;
    resource_desc_share_s           resource_desc;
    unsigned int                    private_data;         // store chip specific info
    unsigned int                    reserve1;
} allocation_info_s;

typedef enum segment_id_e
{
    cpuvisible_local_segment = 1,
    noncpuvisible_local_segment,
    pcie_segment,
    segment_end,
}segment_id_e;

typedef union cb_layout_s
{
    struct
    {
        unsigned short vs_const_base;
        unsigned short vs_const_cnt;
        unsigned short ps_const_base;
        unsigned short ps_const_cnt;
    };
    struct
    {
        unsigned short vs_cbreg_base;
        unsigned short vs_cbreg_cnt;
        unsigned short ps_cbreg_base;
        unsigned short ps_cbreg_cnt;
    };
}cb_layout_s;

typedef union ts_layout_s
{
    struct
    {
        unsigned char ps_heap_base;
    };
    struct
    {
        unsigned char vs_tsharp_base;
        unsigned char vs_tsharp_cnt;
        unsigned char vs_ssharp_base;
        unsigned char vs_ssharp_cnt;
        unsigned char ps_tsharp_base;
        unsigned char ps_tsharp_cnt;
        unsigned char ps_ssharp_base;
        unsigned char ps_ssharp_cnt;
    };
}ts_layout_s;

// when layout switch between kmd submits
// driver need add dummy draw to patch 
typedef struct layout_s
{
    unsigned char uav_buf_size  : 3;
    unsigned char b_const_cache : 1;
    unsigned char b_u_unlimited : 1;
    unsigned char b_ts_cache    : 1;
    unsigned char reserved_bits : 2;
    unsigned char reserved[1];
    unsigned char ps_usharp_base;
    unsigned char ps_usharp_cnt;
    cb_layout_s   cb;
    ts_layout_s   ts;
}layout_s;

typedef struct dma_info_s
{
    uint64_t          dma_gpu_va;
    unsigned int      dma_size_in_byte;
    unsigned int      idb_type;
    unsigned int      idb1_index;
    uint64_t          preempt_chip_image_gpu_va;
    unsigned int      preempt_chip_image_size_in_byte;
    unsigned int      reserved[16];
} dma_info_s;

typedef struct dma_flag_s
{
    union
    {
        struct
        {
            unsigned int update_ttbr : 1;
            unsigned int paging : 1;
            unsigned int reserved   : 30;
        };
        unsigned int value;
    };
}dma_flag_s;

typedef struct queue_type_s
{
    union
    {
        struct
        {
            unsigned int is_3d_queue_type : 1;
            unsigned int is_copy_queue_type : 1;
            unsigned int is_compute_queue_type : 1;
            unsigned int is_vem_queue_type : 1;
            unsigned int is_video_queue_type : 1;
            unsigned int reserved : 27;
        };
        unsigned int type;
    };
}queue_type_s;

typedef struct context_info_s
{
    unsigned int magic_number;
    char   app_name[260];
}context_info_s;

typedef struct submit_info_s
{
    unsigned int magic_number;
    unsigned int dma_info_number;
    dma_flag_s   dma_flags;
    unsigned int submit_index;
    unsigned int slc_mask;
    queue_type_s queue_tpye;
    layout_s     layout;
    unsigned int reserved[11];
}submit_info_s;

typedef struct submit_info_s_vem
{
    unsigned int        cmd_size_dword;
}submit_info_s_vem;

typedef struct segment_flags_s
{
    union
    {
        struct
        {
            unsigned int    aperture      : 1;
            unsigned int    cpu_visible   : 1;
            unsigned int    cache_coherent: 1;
            unsigned int    reserved      : 29;
       
        };
        unsigned int        value;
    };
} segment_flags_s;

typedef struct segment_descriptor_s
{
    uint64_t        size;
    segment_flags_s flags;
    unsigned int    pad;
}segment_descriptor_s;

typedef struct adapter_info_s
{
    unsigned int            device_id;
    unsigned int            vendor_id;
    unsigned int            chip_id;
    unsigned int            driver_model;
    uint64_t                pfn_get_cmodel_info;
    unsigned int            local_heap_alignment;
    unsigned int            pcie_heap_alignment;
    unsigned int            seg_count;
    segment_descriptor_s    seg_descriptors[MAX_SEG_COUNT];
    uint64_t                gpuva_start;
    uint64_t                gpuva_size;
    uint64_t                gpuva_bypass_start;
    uint64_t                gpuva_bypass_size;
    unsigned int            dma_buffer_private_data_size;
    unsigned int            gpc_slc_mask;
    unsigned int            mhb_num;
    unsigned int            cl_size;
    unsigned int            luid_low;
    unsigned int            luid_high;
    unsigned int            is_virtual;
    unsigned int            virtfn_id;
    unsigned int            enable_new_auto_clear;
    unsigned int            msaa_2x_enable;
    unsigned int            pcie_gen;
    unsigned int            pci_domain;
    unsigned int            pci_bus;
    unsigned int            pci_dev;
    unsigned int            pci_func;
    unsigned int            reserve[7];
}adapter_info_s;

typedef struct query_vram_info_s
{
    segment_id_e query_segment;
    uint64_t current_usage;
    uint64_t budget;
}query_vram_info_s; 

#define GF_MAX_METADATA_SIZE 72

typedef struct _hdr_into_s {
    unsigned char valid;
    unsigned char validOutput;
    //D3DDDI_COLOR_SPACE_TYPE
    unsigned int ColorSpace;
    //D3DDDI_HDR_METADATA_TYPE
    unsigned int MetaDataType;
    unsigned int MetaDataSize;
    unsigned char pMetaData[GF_MAX_METADATA_SIZE];
    // output
    unsigned int ColorSpaceOutput;
    unsigned int MetaDataTypeOutput;
    unsigned int MetaDataSizeOutput;
    unsigned char pMetaDataOutput[GF_MAX_METADATA_SIZE];
} hdr_info_s;

typedef struct present_res_info_s
{
    unsigned int        tiled;
    unsigned int        bpp;
    unsigned int        already_decompressed;
}present_res_info_s;

typedef struct resource_chip_private_data_s
{
    unsigned int          signature;
    unsigned int          magic_number;
    resource_desc_share_s resource_desc;
}resource_chip_private_data_s;

typedef struct open_allocation_info_s
{
    unsigned int               h_allocation;                 // in: handle for this allocation in this process
    const void* LS_POINTER_64  p_private_driver_data;         // in: ptr to driver private buffer for this allocations
    unsigned int               private_driver_data_size;      // in: size in bytes of driver private buffer for this allocations

}open_allocation_info_s;

typedef struct open_allocation_info2_s
{
    unsigned int               h_allocation;                 // in: handle for this allocation in this process
    const void* LS_POINTER_64  p_private_driver_data;         // in: ptr to driver private buffer for this allocations
    unsigned int               private_driver_data_size;      // in: size in bytes of driver private buffer for this allocations
    unsigned long long         GpuVirtualAddress;
    void* LS_POINTER_64        Reserved[6];
}open_allocation_info2_s;

typedef enum
{
    esc_rtsim_setting,                 // pass setting between umd and rtsim
    esc_umd_get_cl_index,
    esc_umd_preemption_info,
    esc_umd_log_info,
    esc_umd_get_cl_index_update_kmd_comp_fmt,
    esc_umd_get_kmd_comp_fmt,
    esc_umd_reserved0,
    esc_umd_call_video_codec_test,
    esc_umd_hack_resource,
    esc_umd_get_cl_buffer,
    esc_umd_update_hdr_info,
    esc_umd_get_hdr_info,
    esc_last,

    esc_display_start = 0x100,
    esc_display_reg_read,
    esc_display_reg_write,
    esc_display_aux_read,
    esc_display_aux_write,
    esc_display_get_modelist,
    esc_display_get_edid,
    esc_display_get_gamma_caps,
    esc_display_get_multiplane_overlay_caps,
    esc_display_get_multiplane_overlay_group_caps,
    esc_display_check_mpo_support,
    esc_display_check_present_duration,
    esc_display_check_mpo_color_space,
    esc_display_get_pci,
    esc_display_get_cts_type,
    esc_display_get_cts_active,
    esc_display_get_cts_data,
    esc_display_set_mode,
    esc_display_get_pinned_mode,
    esc_display_get_fp16_normalize_factor,
    esc_display_end = 0x1ff,

    esc_decoder_start = 0x200,
    HANTRO_VCMD_IOCH_GET_CMDBUF_PARAMETER,
    HANTRO_VCMD_IOCH_GET_VCMD_PARAMETER,
    HANTRO_VCMD_IOCH_RESERVE_CMDBUF,
    HANTRO_VCMD_IOCH_LINK_RUN_CMDBUF,
    HANTRO_VCMD_IOCH_WAIT_CMDBUF,
    HANTRO_VCMD_IOCH_PUSH_SLICE_REG,
    HANTRO_VCMD_IOCH_RELEASE_CMDBUF,
    HANTRO_VCMD_IOCH_POLLING_CMDBUF,
    HANTRO_VCMD_IOCH_MAP_CMDBUF,
    HANTRO_VCMD_IOCH_UNMAP_CMDBUF,
    HANTRO_VCMD_IOCH_ACQUIRE_MC_TOKEN,
    esc_decoder_end = 0x21f,
    esc_encoder_start = 0x220,
    esc_encoder_get_cmdbuf_parameter,
    esc_encoder_get_vcmd_parameter,
    esc_encoder_reserve_cmdbuf,
    esc_encoder_link_run_cmdbuf,
    esc_encoder_wait_cmdbuf,
    esc_encoder_release_cmdbuf,
    esc_encoder_polling_cmdbuf,
    esc_encoder_get_vcmd_enable,
    esc_encoder_get_mmu_enable,
    esc_encoder_map_cmdbuf,
    esc_encoder_unmap_cmdbuf,
    esc_encoder_end = 0x23f,

    esc_util_op = 0x300,
}escape_operation_e;

typedef enum
{
    util_get_adapter_info,
    util_set_boost_status,
    util_get_boost_status,
}escape_util_function_e;

typedef struct escape_rtsim_setting
{
    bool b_assert_rsheap_leak;
}escape_rtsim_setting;

typedef enum
{
    esc_fun_drvhelp_scriptname,
    esc_fun_drvhelp_appname,
    esc_fun_drvhelp_cmodelcounter_pointer,
    esc_fun_drvhelp_cmodelcounter_define,
    esc_fun_drvhelp_last,
}escape_function_drvhelp_info_e;

typedef struct escape_get_cl_index_update_kmd_comp_fmt_s
{
    uint64_t        memory_size;
    uint32_t        comp_fmt;
    uint32_t        allocation_handle;
}escape_get_cl_index_update_kmd_comp_fmt_s;

typedef struct escape_get_cl_buffer
{
    void*           cpu_ptr;
    uint32_t        allocation_handle;
}escape_get_cl_buffer;

typedef struct escape_get_new_ac_value_s
{
    uint32_t clear_color[4];
}escape_get_new_ac_value_s;

typedef struct ac_value_status
{
    unsigned int is_found : 1;
    unsigned int is_new : 1;
    unsigned int is_not_support : 1;
    unsigned int clear_cl_addr : 3;
    unsigned int reserved : 26;
}ac_value_status;

typedef struct escape_out_new_ac_value_s
{
    ac_value_status status;
}escape_out_new_ac_value_s;

typedef struct escape_get_multiplane_overlay_caps_arg_s
{
    unsigned int max_planes;
    unsigned int groups_num;
} escape_get_multiplane_overlay_caps_arg_s;

typedef struct escape_get_multiplane_overlay_group_caps_input_s
{
    unsigned int vidpn_source_id;
    unsigned int group_index;
} escape_get_multiplane_overlay_group_caps_input_s;

typedef struct escape_check_present_duration_input_s
{
    unsigned int vidpn_source_id;
    unsigned int desired_present_duration;
} escape_check_present_duration_input_s;

typedef struct escape_check_present_duration_output_s
{
    unsigned int  closest_smaller_duration;
    unsigned int  closest_larger_duration;
} escape_check_present_duration_output_s;

typedef struct escape_check_multiplane_color_space_support_s
{
    unsigned int vidpn_source_id;
    unsigned int format;        // ENCODED_HW_FORMAT
    unsigned int color_space;   // D3DDDI_COLOR_SPACE_TYPE
} escape_check_multiplane_color_space_support_s;

typedef union multiplane_overlay_caps
{
    struct
    {
        unsigned int Rotation                           : 1;
        unsigned int RotationWithoutIndependentFlip     : 1;
        unsigned int VerticalFlip                       : 1;
        unsigned int HorizontalFlip                     : 1;
        unsigned int StretchRGB                         : 1;
        unsigned int StretchYUV                         : 1;
        unsigned int BilinearFilter                     : 1;
        unsigned int HighFilter                         : 1;
        unsigned int Shared                             : 1;
        unsigned int Immediate                          : 1;
        unsigned int Plane0ForVirtualModeOnly           : 1;
        unsigned int Reserved                           :21;
    };
    unsigned int Value;
} multiplane_overlay_caps;

typedef struct escape_get_multiplane_overlay_group_caps_output_s
{
    unsigned int planes_num;
    float max_stretch_factor;
    float max_shrink_factor;
    multiplane_overlay_caps overlay_caps;
    unsigned int stereo_caps;
} escape_get_multiplane_overlay_group_caps_output_s;

typedef struct escape_get_multiplane_overlay_caps_s
{
    unsigned int max_planes;
    unsigned int groups_num;
} escape_get_multiplane_overlay_caps_s;

typedef struct escape_get_gamma_caps_arg_s
{
    unsigned char scale_and_offset_supported;
    float max_converted_value;
    float min_converted_value;
    unsigned int gamma_control_points_num;
    float* LS_POINTER_64 control_points_positions;
} escape_get_gamma_caps_arg_s;

typedef struct escape_get_fp16_normalize_factor_s
{
    unsigned int vidpn_source_id;
    unsigned int layer_index;
    float factor; // out
} escape_get_fp16_normalize_factor_s;

typedef struct escape_info_s
{
    unsigned int   magic_number;
    unsigned int   operation;
    unsigned int   function;
    void* LS_POINTER_64  input_buffer;
    unsigned int   input_buffer_length;
    void* LS_POINTER_64  output_buffer;
    unsigned int   output_buffer_length;
}escape_info_s;

typedef struct _escape_util_boost_gpu_s
{
    bool is_boost;
    bool is_boost_lock;
}escape_util_boost_gpu_s;


#endif
