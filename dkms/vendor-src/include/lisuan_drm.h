/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __LISUAN_DRM_H__
#define __LISUAN_DRM_H__

#include "umd_kmd_share.h"
#include "hantrovcmd.h"
#include "hantrodec.h"
#include "vcx_driver.h"

#if defined(__cplusplus)
extern "C" {
#endif

struct lisuan_prefer_domains {
	union {
		struct {
			uint32_t DomainId0:5;
			uint32_t Direction0:1;
			uint32_t DomainId1:5;
			uint32_t Direction1:1;
			uint32_t DomainId2:5;
			uint32_t Direction2:1;
			uint32_t DomainId3:5;
			uint32_t Direction3:1;
			uint32_t DomainId4:5;
			uint32_t Direction4:1;
			uint32_t Reserved:2;
		};
		uint32_t Value;
	};
};

struct lisuan_buffer_flags {
	union {
		struct {
			uint32_t CpuVisible:1;
			uint32_t PermanentSysMem:1;
			uint32_t Cached:1;
			uint32_t Protected:1;
			uint32_t ExistingSysMem:1;
			uint32_t ExistingKernelSysMem:1;
			uint32_t FromEndOfDomain:1;
			uint32_t DisableLargePageMapping:1;
			uint32_t Overlay:1;
			uint32_t Capture:1;
			uint32_t CreateInVpr:1;
			uint32_t DXGK_ALLOC_RESERVED17:1;
			uint32_t LinkMirrored:1;
			uint32_t LinkInstanced:1;
			uint32_t HistoryBuffer:1;
			uint32_t AccessedPhysically:1;
			uint32_t ExplicitResidencyNotification:1;
			uint32_t HardwareProtected:1;
			uint32_t CpuVisibleOnDemand:1;
		};
		uint32_t Value;
	};
};

typedef struct segment_descriptor {
	uint64_t size;
	union {
		struct {
			uint32_t aperture:1;
			uint32_t cpu_visible:1;
			uint32_t cache_coherent:1;
			uint32_t reserved:29;

		};
		uint32_t value;
	};
} segment_descriptor;

struct lisuan_gem_create_info {
	uint64_t sizeInByte;
	uint64_t align;
	uint32_t priority;
	uint32_t buffer;
	uint32_t compressFormat;
	uint32_t is_present;
	uint32_t is_tiled;
	usage_hint_s usage_hint;

	struct lisuan_prefer_domains preferredDomain;

	struct allocation_info_flags_wddm2_0_s flags;
};

struct lisuan_gem_map_gpuva_info {
	uint64_t map_offset;
	uint64_t map_size;
	uint64_t min_addr_hint;
	uint64_t max_addr_hint;
	uint64_t driver_protection;

	union {
		struct {
			uint32_t write:1;
			uint32_t execute:1;
			uint32_t zero:1;
			uint32_t no_access:1;
			uint32_t system_use_only:1;
			uint32_t reserved:27;
		};
		uint32_t value;
	} protection_flag;
	uint32_t buffer;

	uint64_t gpu_va;
};

struct lisuan_gem_free_gpuva_info {
	uint64_t map_offset;
	uint64_t map_size;
	uint32_t map_buffer;
    uint32_t pad;
};

struct lisuan_make_resident_info {
	union {
		struct {
			uint32_t can_not_trim_further:1;
			uint32_t must_succeed:1;
			uint32_t reserved:30;
		};
		uint32_t value;
	} flags;
	uint32_t buffer;
};

struct lisuan_ctx_alloc_info {
	uint32_t ring_buffer;
	uint32_t gpu_device;
	uint32_t priority;
	uint32_t type;

	union {
		struct {
			uint32_t null_render:1;
		};
		uint32_t value;
	} flag;

	uint32_t context;
};

struct lisuan_ctx_free_info {
	uint32_t context;
	uint32_t pad;
};

struct lisuan_sync_cteate_info {
	uint32_t ring_buffer;
	uint32_t sync_object_handle;
	uint64_t gpu_va;
	uint64_t virtual_address;
	uint32_t buffer_handle;	//map gpu va
	uint32_t pad;
};

struct lisuan_sync_destroy_info {
	uint32_t sync_object_handle;
	uint32_t buffer_handle;
};

struct lisuan_dma_flag {
	union {
		struct {
			uint32_t updateTtbr:1;
			uint32_t reserved:31;
		};
		uint32_t value;
	};
};

struct lisuan_dma_info {
	uint64_t gpu_va;
	uint32_t size;
	uint32_t idb1_index;
};

struct drm_sync_object_submit_info {
	uint32_t syncobj_count;
    uint32_t pad;
    /** Pointer to array of  uint32_t */
	uint64_t syncobj;
    /** Pointer to array of  uint64_t */
	uint64_t points;
};

struct lisuan_job_bo_list_info {
	uint32_t bo_count;
	/** Pointer to array of uint64_t */
	uint64_t bo_handle_list;

	/** Pointer to array of uint64_t */
	uint64_t bo_op_list;
	
};

struct lisuan_submit_cmd_info {
	uint32_t context;
	uint32_t dma_count;
	struct lisuan_dma_flag dma_flag;
	uint32_t pad;
	/** Pointer to array of struct dma_info_s */
	uint64_t dma_info;
	/** Pointer to array of struct gpu_memory_list_c */
	struct lisuan_job_bo_list_info bo_list_info;

	struct drm_sync_object_submit_info signal_objects;
	struct drm_sync_object_submit_info wait_objects;
	struct exchange_parameter dec_para;
	struct enc_exchange_parameter enc_para;
};

struct lisuan_sync_signal_info {
	uint32_t context;
	uint32_t sync_object_handle;
	uint64_t fence_value;
};

struct lisuan_gem_map_offset_info {
	uint32_t buffer;
	uint32_t pad;
	uint64_t offset;
	uint32_t fault_map;
	uint32_t premap_num;
};

struct lisuan_drm_get_cellindex {
	uint32_t buffer;
	uint32_t cellindex;
	uint32_t should_map;
    uint32_t pad;
    uint64_t umd_cpu_va;
};

struct lisuan_drm_get_kmd_fmt {
	uint32_t buffer;
	uint32_t kmd_fmt;
};

struct lisuan_drm_alloc_cell_index {
	unsigned int buffer;
	unsigned int size;
	unsigned int compress_fmt;
	unsigned int cell_index;
};

struct lisuan_drm_query_gem_info
{
        uint32_t            h_allocation_;                 // in: handle for this allocation in this process
        uint32_t            private_driver_data_size;      // in: size in bytes of driver private buffer for this allocations
        /** Pointer to array of struct allocation_info_s */
        uint64_t                p_private_driver_data;         // in: ptr to driver private buffer for this allocations
};

struct lisuan_gem_busy_info {
	uint32_t handle;
	uint32_t status;
};

struct lisuan_drm_reserve_gpu_va
{
    uint64_t base;
    uint64_t min;
    uint64_t max;
    uint64_t size_in_bytes;
    uint64_t driver_protection;
    uint64_t paging_fence_value; //out
    uint64_t virtual_address; //out
};

enum update_op_e
{
    map = 0,
    unmap,
    copy,
    map_protect,
};

struct update_tile_mapping_info
{
    uint32_t operation_type;
    union
    {
        struct
        {
            uint64_t      tile_gpu_va;
            uint32_t      h_allocation;
            uint32_t      pad;
            uint64_t      offset;
            uint64_t      size;
        }map;
        struct
        {
            uint64_t tile_gpu_va;
            uint64_t size;
        }unmap;
        struct
        {
            uint64_t src_tile_gpu_va;
            uint64_t dst_tile_gpu_va;
            uint64_t size;
        }copy;
    };
};

enum update_gpu_va_flags_e
{
    none = 0x0,
    do_not_wait = 0x1,
};

struct lisuan_drm_update_gpu_va
{
    uint32_t                           num_ops;
    uint32_t                           flags;
    /** Pointer to array of struct update_tile_mapping_info */
    uint64_t                           p_ops;
};

struct query_clock_calibration
{
   uint32_t nodeOrdinal;    //in
   uint32_t engineAffinity; //in
   uint64_t gpu_counter;

   union
   {
      uint64_t perf_counter; //for win32
      struct   //for linux
      {
         uint64_t clock_monotonic;
         uint64_t clock_monotonic_raw;
      };
   };
};

// display HDR
#define HDR_DEGAMMA_LUT_LENGTH 37
typedef enum _hdr_cs_type_e
{
    hdr_cs_type_srgb = 0,       // BT709
    hdr_cs_type_adobeRgb,
    hdr_cs_type_bt2020,
    hdr_cs_type_dciP3,
    hdr_cs_type_bt601_525,
    hdr_cs_type_bt601_625,

    hdr_cs_type_end,            // cs count
} hdr_cs_type_e;
typedef enum _hdr_tf_type_e
{
    hdr_tf_type_srgb = 0,       // sRGB DisplayP3, same e2o func
    hdr_tf_type_adobeRgb,
    hdr_tf_type_dciP3,
    hdr_tf_type_ituOetf,        // BT601 BT709 BT2020, same e2o func
    hdr_tf_type_bt1886,
    hdr_tf_type_pq,
    hdr_tf_type_hlg,

    hdr_tf_type_end,            // tf count
} hdr_tf_type_e;
typedef union _property_crtc_hdr_info_s
{
    struct
    {
        unsigned long long hdr_en_0                 : 1;
        unsigned long long cs_in_0                  : 7; // hdr_cs_type_e
        unsigned long long tf_in_0                  : 8; // hdr_tf_type_e
        unsigned long long hdr_en_1                 : 1;
        unsigned long long cs_in_1                  : 7;
        unsigned long long tf_in_1                  : 8;
        unsigned long long max_lum_in_0             :16; // 1 nits
        unsigned long long max_lum_in_1             :16; // 1 nits
    } mem;
    unsigned long long u64_value;
} property_crtc_hdr_info_s;
typedef union _property_crtc_hdr_mode_s {
    struct {
        unsigned long long cs_out                   : 8; // hdr_cs_type_e
        unsigned long long tf_out                   : 8; // hdr_tf_type_e
        unsigned long long min_lum_1_10000          :16; // 1/10000 nits
        unsigned long long max_lum                  :16; // 1 nits
        unsigned long long max_fall                 :16; // 1 nits
    } mem;
    unsigned long long u64_value;
} property_crtc_hdr_mode_s;

// display sgnt
#define SGNT_GOLDEN_LEN 4
typedef struct _property_sgnt_param_s{
    // sgnt_en
    // sgnt_width
    // sgnt_height
    unsigned int golden[SGNT_GOLDEN_LEN];
} property_sgnt_param_s;

struct vem_info_ioctl
{
    uint64_t vm_root_gpuva;
    uint64_t rb_gpuva;
    uint64_t rb_cpuva;
};

#define GF_GEM_CREATE        0x00
#define GF_GEM_MAP_VA        0x01	//map gpu virtual address
#define GF_GEM_FREE_VA       0x08
#define GF_GEM_MAKE_RESIDENT 0x02

#define GF_CTX_ALLOC         0x03
#define GF_CTX_FREE          0x04
#define GF_SYNC_CREATE       0x05
#define GF_CS                0x06	//command submit
#define GF_SYNC_SIGNAL       0x07
#define GF_SYNC_WAIT         0x09
#define GF_SYNC_DESTROY      0x0a
#define GF_GEM_MAP_OFFSET    0x0b
#define GF_QUERY_CARD_INFO   0x0c
#define GF_GET_CELLINDEX     0x0d
#define GF_GEM_QUERY_INFOS   0x0e
#define GF_RESERVE_GPUVA     0x0f
#define GF_UPDATE_GPUVA      0x10
#define GF_QUERY_CLOCK_CALIBRATION 0x11
#define GF_GET_KMDFMT        0x12
#define GF_QUERY_VRAM_INFO   0x13

#define GF_GEM_IS_BUSY       0x14
#define GF_ALLOC_CELL_INDEX  0x15

#define GF_GET_VEM_INFO      0x24

#define HANTRO_VCMD_IOCH_GET_CMDBUF_PARAMETER 0x25
#define HANTRO_VCMD_IOCH_GET_VCMD_PARAMETER   0x26
#define HANTRO_VCMD_IOCH_RESERVE_CMDBUF       0x27
#define HANTRO_VCMD_IOCH_LINK_RUN_CMDBUF      0x28
#define HANTRO_VCMD_IOCH_WAIT_CMDBUF          0x29
#define HANTRO_VCMD_IOCH_RELEASE_CMDBUF       0x2a
#define HANTRO_VCMD_IOCH_POLLING_CMDBUF       0x2b
#define HANTRO_VCMD_IOCH_PUSH_SLICE_REG       0x2c

#define HANTRODEC_IOCGHWIOSIZE                0x2d
#define HANTRODEC_IOC_MC_OFFSETS              0x2e
#define HANTRODEC_IOC_MC_CORES                0x2f
#define HANTRODEC_IOCS_DEC_PUSH_REG           0x30
#define HANTRODEC_IOCH_DEC_RESERVE            0x31
#define HANTRODEC_IOCT_DEC_RELEASE            0x32
#define HANTRODEC_IOCX_DEC_WAIT               0x33
#define HANTRODEC_IOCS_DEC_PULL_REG           0x34
#define HANTRODEC_IOCG_CORE_WAIT              0x35
#define HANTRODEC_IOX_ASIC_ID                 0x36
#define HANTRODEC_IOCG_CORE_ID                0x37
#define HANTRODEC_IOCS_DEC_WRITE_REG          0x38
#define HANTRODEC_IOCS_DEC_READ_REG           0x39
#define HANTRODEC_IOX_ASIC_BUILD_ID           0x3a
#define HANTRODEC_IOX_SUBSYS                  0x3b
#define HANTRODEC_IOCX_POLL                   0x3c
#define HANTRODEC_IOCS_DEC_WRITE_APBFILTER_REG 0x3d
#define HANTRODEC_IOC_APBFILTER_CONFIG         0x3e
#define HANTRODEC_IOC_AXIFE_CONFIG             0x3f

#define HANTRO_IOCH_GET_CMDBUF_PARAMETER               0x40
#define HANTRO_IOCH_GET_VCMD_PARAMETER                 0x41
#define HANTRO_IOCH_RESERVE_CMDBUF                     0x42
#define HANTRO_IOCH_LINK_RUN_CMDBUF                    0x43
#define HANTRO_IOCH_WAIT_CMDBUF                        0x44
#define HANTRO_IOCH_RELEASE_CMDBUF                     0x45
#define HANTRO_IOCH_POLLING_CMDBUF                     0x46
#define HANTRO_IOCH_GET_VCMD_ENABLE                    0x47
#define HANTRO_IOCH_GET_MMU_ENABLE                     0x48

#define HANTRO_IOCG_HWOFFSET                           0x49
#define HANTRO_IOCG_HWIOSIZE                           0x4a
#define HANTRO_IOCG_SRAMOFFSET                         0x4b
#define HANTRO_IOCG_SRAMEIOSIZE                        0x4c
#define HANTRO_IOCH_ENC_RESERVE                        0x4d
#define HANTRO_IOCH_ENC_RELEASE                        0x4e
#define HANTRO_IOCG_CORE_NUM                           0x4f
#define HANTRO_IOCG_CORE_INFO                          0x50
#define HANTRO_IOCG_CORE_WAIT                          0x51
#define HANTRO_IOCG_ANYCORE_WAIT                       0x52
#define HANTRO_IOCG_ANYCORE_WAIT_POLLING               0x53

#define HANTRO_VCMD_IOCH_ACQUIRE_MC_TOKEN     0x54
//the biggest driver ioctl num is 0xA0 - 0x40 = 0x60

#define DRM_IOCTL_GF_QUERY_CARD_INFO          DRM_IOWR(DRM_COMMAND_BASE + GF_QUERY_CARD_INFO, struct adapter_info_s)
#define DRM_IOCTL_GF_GEM_CREATE               DRM_IOWR(DRM_COMMAND_BASE + GF_GEM_CREATE, struct lisuan_gem_create_info)
#define DRM_IOCTL_GF_GEM_MAP_VA               DRM_IOWR(DRM_COMMAND_BASE + GF_GEM_MAP_VA,     struct lisuan_gem_map_gpuva_info)
#define DRM_IOCTL_GF_GEM_FREE_VA              DRM_IOWR(DRM_COMMAND_BASE + GF_GEM_FREE_VA,struct lisuan_gem_free_gpuva_info)
#define DRM_IOCTL_GF_GEM_MAKE_RESIDENT        DRM_IOWR(DRM_COMMAND_BASE + GF_GEM_MAKE_RESIDENT, struct lisuan_make_resident_info)
#define DRM_IOCTL_GF_CTX_ALLOC                DRM_IOWR(DRM_COMMAND_BASE + GF_CTX_ALLOC, struct lisuan_ctx_alloc_info)
#define DRM_IOCTL_GF_CTX_FREE                 DRM_IOWR(DRM_COMMAND_BASE + GF_CTX_FREE, struct lisuan_ctx_free_info)
#define DRM_IOCTL_GF_SYNC_CREATE              DRM_IOWR(DRM_COMMAND_BASE + GF_SYNC_CREATE, struct lisuan_sync_cteate_info)
#define DRM_IOCTL_GF_CS                       DRM_IOWR(DRM_COMMAND_BASE + GF_CS, struct lisuan_submit_cmd_info)
#define DRM_IOCTL_GF_SYNC_SIGNAL              DRM_IOWR(DRM_COMMAND_BASE + GF_SYNC_SIGNAL, struct lisuan_sync_signal_info)
#define DRM_IOCTL_GF_SYNC_WAIT                DRM_IOWR(DRM_COMMAND_BASE + GF_SYNC_WAIT,struct lisuan_sync_signal_info)
#define DRM_IOCTL_GF_SYNC_DESTROY             DRM_IOWR(DRM_COMMAND_BASE + GF_SYNC_DESTROY,struct lisuan_sync_destroy_info)
#define DRM_IOCTL_GF_GEM_MAP_OFFSET           DRM_IOWR(DRM_COMMAND_BASE + GF_GEM_MAP_OFFSET, struct lisuan_gem_map_offset_info)
#define DRM_IOCTL_GF_GET_CELLINDEX            DRM_IOWR(DRM_COMMAND_BASE + GF_GET_CELLINDEX, struct lisuan_drm_get_cellindex)
#define DRM_IOCTL_GF_GEM_QUERY_INFOS          DRM_IOWR(DRM_COMMAND_BASE + GF_GEM_QUERY_INFOS, struct lisuan_drm_query_gem_info)
#define DRM_IOCTL_GF_RESERVE_GPUVA            DRM_IOWR(DRM_COMMAND_BASE + GF_RESERVE_GPUVA, struct lisuan_drm_reserve_gpu_va)
#define DRM_IOCTL_GF_UPDATE_GPUVA             DRM_IOWR(DRM_COMMAND_BASE + GF_UPDATE_GPUVA, struct lisuan_drm_update_gpu_va)
#define DRM_IOCTL_GF_QUERY_CLOCK_CALIBRATION  DRM_IOWR(DRM_COMMAND_BASE + GF_QUERY_CLOCK_CALIBRATION, struct query_clock_calibration)
#define DRM_IOCTL_GF_GET_KMDFMT               DRM_IOWR(DRM_COMMAND_BASE + GF_GET_KMDFMT, struct lisuan_drm_get_kmd_fmt)
#define DRM_IOCTL_GF_MAP_VCMD                 DRM_IOWR(DRM_COMMAND_BASE + GF_MAP_VCMD, uint64_t)
#define DRM_IOCTL_GF_GEM_IS_BUSY              DRM_IOWR(DRM_COMMAND_BASE + GF_GEM_IS_BUSY, struct lisuan_gem_busy_info)
#define DRM_IOCTL_GF_ALLOC_CELL_INDEX         DRM_IOWR(DRM_COMMAND_BASE + GF_ALLOC_CELL_INDEX, struct lisuan_drm_alloc_cell_index)
#define DRM_IOCTL_GF_QUERY_VRAM_INFO          DRM_IOWR(DRM_COMMAND_BASE + GF_QUERY_VRAM_INFO, struct query_vram_info_s)

#define DRM_IOCTL_GF_GET_VEM_INFO             DRM_IOWR(DRM_COMMAND_BASE + GF_GET_VEM_INFO, struct vem_info_ioctl)

//decoder vcmd ioctl strat
#define DRM_IOCTL_HANTRO_VCMD_IOCH_GET_CMDBUF_PARAMETER                                  \
	DRM_IOWR(DRM_COMMAND_BASE + HANTRO_VCMD_IOCH_GET_CMDBUF_PARAMETER, struct cmdbuf_mem_parameter)
#define DRM_IOCTL_HANTRO_VCMD_IOCH_GET_VCMD_PARAMETER                                    \
	DRM_IOWR(DRM_COMMAND_BASE + HANTRO_VCMD_IOCH_GET_VCMD_PARAMETER, struct config_parameter)
#define DRM_IOCTL_HANTRO_VCMD_IOCH_RESERVE_CMDBUF                                        \
	DRM_IOWR(DRM_COMMAND_BASE + HANTRO_VCMD_IOCH_RESERVE_CMDBUF, struct exchange_parameter)
#define DRM_IOCTL_HANTRO_VCMD_IOCH_LINK_RUN_CMDBUF DRM_IOWR(DRM_COMMAND_BASE + HANTRO_VCMD_IOCH_LINK_RUN_CMDBUF, struct exchange_parameter)
#define DRM_IOCTL_HANTRO_VCMD_IOCH_WAIT_CMDBUF DRM_IOWR(DRM_COMMAND_BASE + HANTRO_VCMD_IOCH_WAIT_CMDBUF, u16)
#define DRM_IOCTL_HANTRO_VCMD_IOCH_RELEASE_CMDBUF DRM_IOWR(DRM_COMMAND_BASE + HANTRO_VCMD_IOCH_RELEASE_CMDBUF, u16)
#define DRM_IOCTL_HANTRO_VCMD_IOCH_POLLING_CMDBUF DRM_IOWR(DRM_COMMAND_BASE + HANTRO_VCMD_IOCH_POLLING_CMDBUF, u16)
#define DRM_IOCTL_HANTRO_VCMD_IOCH_PUSH_SLICE_REG DRM_IOWR(DRM_COMMAND_BASE + HANTRO_VCMD_IOCH_PUSH_SLICE_REG, struct core_desc)
#define DRM_IOCTL_HANTRO_VCMD_IOCH_ACQUIRE_MC_TOKEN DRM_IOWR(DRM_COMMAND_BASE + HANTRO_VCMD_IOCH_ACQUIRE_MC_TOKEN, u32)
//decoder vcmd ioctl end

#define DRM_IOCTL_HANTRODEC_IOCGHWIOSIZE                                                 \
	DRM_IOWR(DRM_COMMAND_BASE + HANTRODEC_IOCGHWIOSIZE, struct regsize_desc)
#define DRM_IOCTL_HANTRODEC_IOC_MC_OFFSETS DRM_IOWR(DRM_COMMAND_BASE + HANTRODEC_IOC_MC_OFFSETS, unsigned long long)
#define DRM_IOCTL_HANTRODEC_IOC_MC_CORES DRM_IOWR(DRM_COMMAND_BASE + HANTRODEC_IOC_MC_CORES, unsigned int)
#define DRM_IOCTL_HANTRODEC_IOCS_DEC_PUSH_REG                                            \
	DRM_IOWR(DRM_COMMAND_BASE + HANTRODEC_IOCS_DEC_PUSH_REG, struct core_desc)
#define DRM_IOCTL_HANTRODEC_IOCH_DEC_RESERVE DRM_IO(DRM_COMMAND_BASE + HANTRODEC_IOCH_DEC_RESERVE)
#define DRM_IOCTL_HANTRODEC_IOCT_DEC_RELEASE DRM_IO(DRM_COMMAND_BASE + HANTRODEC_IOCT_DEC_RELEASE)
#define DRM_IOCTL_HANTRODEC_IOCX_DEC_WAIT                                                \
	DRM_IOWR(DRM_COMMAND_BASE + HANTRODEC_IOCX_DEC_WAIT, struct core_desc)
#define DRM_IOCTL_HANTRODEC_IOCS_DEC_PULL_REG                                            \
	DRM_IOWR(DRM_COMMAND_BASE + HANTRODEC_IOCS_DEC_PULL_REG, struct core_desc)
#define DRM_IOCTL_HANTRODEC_IOCG_CORE_WAIT DRM_IOWR(DRM_COMMAND_BASE + HANTRODEC_IOCG_CORE_WAIT, int)
#define DRM_IOCTL_HANTRODEC_IOX_ASIC_ID                                                  \
	DRM_IOWR(DRM_COMMAND_BASE + HANTRODEC_IOX_ASIC_ID, struct core_param *)
#define DRM_IOCTL_HANTRODEC_IOCG_CORE_ID DRM_IOWR(DRM_COMMAND_BASE + HANTRODEC_IOCG_CORE_ID, unsigned long)
#define DRM_IOCTL_HANTRODEC_IOCS_DEC_WRITE_REG                                           \
	DRM_IOWR(DRM_COMMAND_BASE + HANTRODEC_IOCS_DEC_WRITE_REG, struct core_desc *)
#define DRM_IOCTL_HANTRODEC_IOCS_DEC_READ_REG                                            \
	DRM_IOWR(DRM_COMMAND_BASE + HANTRODEC_IOCS_DEC_READ_REG, struct core_desc *)
#define DRM_IOCTL_HANTRODEC_IOX_ASIC_BUILD_ID DRM_IOWR(DRM_COMMAND_BASE + HANTRODEC_IOX_ASIC_BUILD_ID, __u32)
#define DRM_IOCTL_HANTRODEC_IOX_SUBSYS                                                   \
	DRM_IOWR(DRM_COMMAND_BASE + HANTRODEC_IOX_SUBSYS, struct subsys_desc)
#define DRM_IOCTL_HANTRODEC_IOCX_POLL DRM_IO(DRM_COMMAND_BASE + HANTRODEC_IOCX_POLL)
#define DRM_IOCTL_HANTRODEC_IOCS_DEC_WRITE_APBFILTER_REG                                 \
	DRM_IOWR(DRM_COMMAND_BASE + HANTRODEC_IOCS_DEC_WRITE_APBFILTER_REG, struct core_desc)
#define DRM_IOCTL_HANTRODEC_IOC_APBFILTER_CONFIG                                         \
	DRM_IOWR(DRM_COMMAND_BASE + HANTRODEC_IOC_APBFILTER_CONFIG, struct apbfilter_cfg)
#define DRM_IOCTL_HANTRODEC_IOC_AXIFE_CONFIG                                             \
	DRM_IOWR(DRM_COMMAND_BASE + HANTRODEC_IOC_AXIFE_CONFIG, struct axife_cfg)

#define DRM_IOCTL_HANTRO_IOCH_GET_CMDBUF_PARAMETER               DRM_IOWR(DRM_COMMAND_BASE + HANTRO_IOCH_GET_CMDBUF_PARAMETER, struct enc_cmdbuf_mem_parameter)
#define DRM_IOCTL_HANTRO_IOCH_GET_VCMD_PARAMETER                 DRM_IOWR(DRM_COMMAND_BASE + HANTRO_IOCH_GET_VCMD_PARAMETER, struct enc_config_parameter)
#define DRM_IOCTL_HANTRO_IOCH_RESERVE_CMDBUF                     DRM_IOWR(DRM_COMMAND_BASE + HANTRO_IOCH_RESERVE_CMDBUF, struct enc_exchange_parameter)
#define DRM_IOCTL_HANTRO_IOCH_LINK_RUN_CMDBUF                    DRM_IOWR(DRM_COMMAND_BASE + HANTRO_IOCH_LINK_RUN_CMDBUF, struct enc_exchange_parameter)
#define DRM_IOCTL_HANTRO_IOCH_WAIT_CMDBUF                        DRM_IOWR(DRM_COMMAND_BASE + HANTRO_IOCH_WAIT_CMDBUF, u16)
#define DRM_IOCTL_HANTRO_IOCH_RELEASE_CMDBUF                     DRM_IOWR(DRM_COMMAND_BASE + HANTRO_IOCH_RELEASE_CMDBUF, u16)
#define DRM_IOCTL_HANTRO_IOCH_POLLING_CMDBUF                     DRM_IOWR(DRM_COMMAND_BASE + HANTRO_IOCH_POLLING_CMDBUF, u16)
#define DRM_IOCTL_HANTRO_IOCH_GET_VCMD_ENABLE                    DRM_IOWR(DRM_COMMAND_BASE + HANTRO_IOCH_GET_VCMD_ENABLE, unsigned long)
#define DRM_IOCTL_HANTRO_IOCH_GET_MMU_ENABLE                     DRM_IOWR(DRM_COMMAND_BASE + HANTRO_IOCH_GET_MMU_ENABLE, unsigned long)

#define DRM_IOCTL_HANTRO_IOCG_HWOFFSET                           DRM_IOR(DRM_COMMAND_BASE + HANTRO_IOCG_HWOFFSET, unsigned long)
#define DRM_IOCTL_HANTRO_IOCG_HWIOSIZE                           DRM_IOR(DRM_COMMAND_BASE + HANTRO_IOCG_HWIOSIZE, unsigned int)
#define DRM_IOCTL_HANTRO_IOCG_SRAMOFFSET                         DRM_IOR(DRM_COMMAND_BASE + HANTRO_IOCG_SRAMOFFSET, unsigned long)
#define DRM_IOCTL_HANTRO_IOCG_SRAMEIOSIZE                        DRM_IOR(DRM_COMMAND_BASE + HANTRO_IOCG_SRAMEIOSIZE, unsigned int)
#define DRM_IOCTL_HANTRO_IOCH_ENC_RESERVE                        DRM_IOR(DRM_COMMAND_BASE + HANTRO_IOCH_ENC_RESERVE, unsigned int)
#define DRM_IOCTL_HANTRO_IOCH_ENC_RELEASE                        DRM_IOR(DRM_COMMAND_BASE + HANTRO_IOCH_ENC_RELEASE, unsigned int)
#define DRM_IOCTL_HANTRO_IOCG_CORE_NUM                           DRM_IOR(DRM_COMMAND_BASE + HANTRO_IOCG_CORE_NUM, unsigned int)
#define DRM_IOCTL_HANTRO_IOCG_CORE_INFO                          DRM_IOR(DRM_COMMAND_BASE + HANTRO_IOCG_CORE_INFO, SUBSYS_CORE_INFO)
#define DRM_IOCTL_HANTRO_IOCG_CORE_WAIT                          DRM_IOR(DRM_COMMAND_BASE + HANTRO_IOCG_CORE_WAIT, unsigned int)
#define DRM_IOCTL_HANTRO_IOCG_ANYCORE_WAIT                       DRM_IOR(DRM_COMMAND_BASE + HANTRO_IOCG_ANYCORE_WAIT, CORE_WAIT_OUT)
#define DRM_IOCTL_HANTRO_IOCG_ANYCORE_WAIT_POLLING               DRM_IOR(DRM_COMMAND_BASE + HANTRO_IOCG_ANYCORE_WAIT_POLLING, CORE_WAIT_OUT)

#if defined(__cplusplus)
}
#endif
#endif
