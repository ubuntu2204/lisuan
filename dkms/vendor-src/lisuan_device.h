/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __LISUAN_DEVICE_H__
#define __LISUAN_DEVICE_H__

#include <drm/drm_device.h>
#include <drm/drm_file.h>
#include <linux/device.h>
#include <linux/pci.h>
#include <linux/list.h>
#include "lisuan_scheduler.h"
#include "gf_os_interface.h"
#include "lisuan_context.h"
#include "lisuan_gem.h"
#include "gf_kmd_event.h"
#include "lisuan_sync.h"

#include "gf_kms.h"
#include "gf_hw_cmds.h"

#define ALIGN_SIZE(Value, Alignment)  (((Value) + (Alignment - 1)) & ~(Alignment - 1))
#define ALIGN64_SIZE(Value, Alignment)  ((((unsigned long long)Value) + ((unsigned long long)Alignment - 1)) & ~((unsigned long long)Alignment - 1))

#define E_COMPRESS_RATE         (9)
#define SHADOW_IDB_SIZE (256 * 1024)

enum lisuan_mm_domains {
	dom_visual_vram = 1,
	dom_invisual_vram = 2,
	dom_system_pages = 3,
	dom_mm_max = 3,
	dom_snoop_system_pages = 4,
	dom_max = 5,
};


enum gem_priority
{
    gem_priority_unused    = 0x0, ///< Indicates that the allocation is not currently being used at all, and should be the first
                     ///  choice to be paged out.
    gem_priority_very_low  = 0x1, ///< Lowest priority to keep in its preferred heap.
    gem_priority_low       = 0x2, ///< Low priority to keep in its preferred heap.
    gem_priority_normal    = 0x3, ///< Normal priority to keep in its preferred heap.
    gem_priority_high      = 0x4, ///< High priority to keep in its preferred heap (e.g., render targets).
    gem_priority_very_high = 0x5, ///< Highest priority to keep in its preferred heap.  Last choice to be paged out (e.g., page
                     ///  tables, displayable allocations).
    gem_priority_count
};

#define CHIP_GF                 (0x1f)
#define GPU_PTE_SIZE             (4)

#define GPU_PAGE_SHIFT          (12)
#define GPU_PAGE_SIZE           (1ULL << GPU_PAGE_SHIFT)
#define GPU_PAGE_MASK           (~(GPU_PAGE_SIZE-1))

#define GPU_HUGE_PAGE_SHIFT     (16)
#define GPU_HUGE_PAGE_SIZE      (1ULL << GPU_HUGE_PAGE_SHIFT)
#define GPU_HUGE_PAGE_MASK      (~(GPU_HUGE_PAGE_SIZE-1))

#define GPU_BLOCK_SHIFT         (22)
#define GPU_BLOCK_SIZE          (1ULL << GPU_BLOCK_SHIFT)
#define GPU_BLOCK_MASK          (~(GPU_BLOCK_SIZE-1))

#define BYPASS_RANGE_START_GPUVA (4*1024*1024*1024ULL)	//cordinate with win driver(win os required), for virtualization purpose
#define BYPASS_RANGE_PCIE_SPACE_SIZE (12*1024*1024*1024ULL)

struct lisuan_device;
struct lisuan_ctx;
struct lisuan_file;
struct lisuan_gem_bo;

/*
*used for test or bring up flag
*/

struct lisuan_hw_ctl {
    bool use_vram;		//only use chip vram 
    bool use_byPass;	//only for test bring up
    bool iommu_enabled;	//iommu enabled or not.
    bool cpu_update_ptes;	//debug use only，use cpu
    bool no_gfx;
    bool no_gce;
    bool no_vem;
    bool test_local_mem;	//test local mem by cpu
    bool skip_invalidate;	//no use to invalidate pte
    bool validation_chip;
    bool recovery_enable;	//enable recovery by soft reset
    bool enable_local_huge_page;
    bool enable_hw_cg;
    bool enable_sgnt;
    bool no_video;  //for non-video DB
    char paging_rb;
    bool skip_clr;
    bool enable_gl2;
    bool dp_cts;
    bool display_cts; // add property for dat
    bool pz1;
    bool no_encoder;
    bool mode_8k;

    unsigned int fake_monitor_mask;
    bool support_wc;
    bool endebug;
    bool en_new_cl_clr;
};

struct lisuan_mman {
	/*memory manager */
	struct drm_mm *mm;
	struct list_head list_head;
	spinlock_t lock;

	unsigned long long start_addr;
	unsigned long long end_addr;
	unsigned long long size;

	struct list_head list_lru[gem_priority_count];
	bool type_2;
};

struct lisuan_pcie_info {
	unsigned short chip_vendor_id;
	unsigned short chip_device_id;
	unsigned char chip_revision_id;
	unsigned char chip_header_type;
	unsigned short sub_sys_vendor_id;
	unsigned short sub_sys_id;

	unsigned short dev_status;
	unsigned char dev_base_class;
	unsigned char dev_sub_class;
	unsigned short dev_command;
	unsigned char dev_latency_timer;
	unsigned char dev_bist;
	unsigned short dev_link_status;

	resource_size_t start_addr_io;
	resource_size_t end_addr_io;

	unsigned char prog_if;
	unsigned char cache_line_size;

	resource_size_t start_addr_vram;
	resource_size_t end_addr_vram;

	resource_size_t start_addr_rom;
	resource_size_t end_addr_rom;
};

/*next bufs all located in byPass range*/
struct lisuan_buf_domain {
	unsigned int fence_buffer_dom;
	unsigned int kcmd_buffer_dom;
	unsigned int ringbuf_cmd_buffer_dom;
	unsigned int internal_cmd_buffer_dom;
	unsigned int cf_pool_dom;
	unsigned int bypass_pt_buffer_dom;
	unsigned int none_bypass_pt_buffer_dom;
};

struct lisuan_kbo_mman {
	unsigned int alignment;
	struct lisuan_mman mman;
	struct lisuan_gem_bo *bufObj;
	void (*free)(struct lisuan_device * ls_dev, struct lisuan_kbo_mman * kbom);
};

struct lisuan_kbo_node {
	unsigned long long start;	//vm_start,gpu virtual address  
	unsigned int size;
	struct lisuan_mman *mman;
	struct drm_mm_node *mm_node;
	volatile unsigned long long *cpu_virt_addr;
	unsigned int *pCmd;
	void (*free)(struct lisuan_kbo_node * kbo_node);
};

struct lisuan_cf_man {
	struct lisuan_mman mmgr;
	struct lisuan_gem_bo *buf;
	int cf_total_size;
	int cf_cell_size;
	int cf_slot_num;
	int cf_start_align;
	unsigned int cf_cell_alloc_align;	// 4 mhu:2Kbits(256Byte) align, 2 mhu: 1Kbits(128Byte) align, 1 mhu:512bits(64Byte) align

	void *cf_cpu_virt_address;
};

struct lisuan_bypass_pt {
	struct os_mutex ptb_lock;
	struct lisuan_gem_bo *buf;

	unsigned int *PTB2VA;
	unsigned int *PTB1VA;
	unsigned int *PTB0VA;
};

enum mem_type {
	MEM_TYPE_4KB,
	MEM_TYPE_64KB,
	MEM_TYPE_4MB,
	MEM_TYPE_MAX
};

enum lisuan_vm_level {
	LS_VM_PD0,
	LS_VM_PD1,
	LS_VM_PD2,
	LS_VM_PT
};

struct gf_sync_fence {
	struct lisuan_ring *sched;
	struct dma_fence *fence;
	struct dma_fence_cb fence_cb;
	struct list_head list_item;
};

//translating between drm_device/device and lisuan_device
struct lisuan_device {
	struct device *dev;
	struct drm_device *ddev;
	struct pci_dev *pdev;
	unsigned int is_virtfn;
	unsigned int virtfn_id;	//0xFF means SRIOV not enable
	unsigned int dump_rb_cmd;
	struct lisuan_hw_ctl hwctl;
	struct lisuan_mman mmgr[dom_mm_max];
	struct lisuan_ring *sched[RB_INDEX_LAST];

	unsigned int chip_id;
	unsigned int slc_mask;
	unsigned int mhu_disable_mask;
	unsigned int rb_count;

	unsigned int sys_page_size;
	unsigned int sys_page_shift;
	unsigned int sys_page_mask;

	struct lisuan_pcie_info pdev_info;
	unsigned long long mmu_bypass_start;
	unsigned long long mmu_bypass_end;
	unsigned long long mmu_bypass_pcie_start;
	unsigned int max_address_bits;
	int mhb_num;
	unsigned int cl_size;
	struct os_mutex file_lock;
	struct os_mutex mailbox_lock;
	struct list_head file_list;

	void *debugfs;
	unsigned char *mmio_base;
	unsigned char *rom_base;
	struct lisuan_buf_domain bufsel;
	unsigned long long hang_timeout;
	struct lisuan_procid_mgr *procid_mgr;
	unsigned long long max_mhb_access_size;
	unsigned long long vram_size;
	unsigned long long visual_vram_size;
	unsigned long long gap_size;
	struct lisuan_cf_man cf;
	struct lisuan_bypass_pt pt;
	struct lisuan_kbo_mman kfence;
	struct lisuan_kbo_mman kcommand;
	struct lisuan_ctx *vm_ctx;
	struct drm_mm_node * gap_node;

	struct spinlock ring_fence_lock[RB_INDEX_LAST];
	struct list_head ring_fence_list[RB_INDEX_LAST];
	unsigned long long ring_fence_count[RB_INDEX_LAST];

	char ring_fence_thread_name[66];
	char job_prepare_thread_name[66];

	int vec_num;
	int irq[8];
	bool installed;

	int fbdev;
	struct lisuan_fbdev *gfbdev;
	unsigned int pt_count[MEM_TYPE_MAX][LS_VM_PT];
	struct display_adapter *adapter;
	//used for bypass inv
	unsigned int is_bypass_update;
	unsigned long long address;
	unsigned long long mask;

	void *rb_cmd_template;
	//struct gpu_invalidate_tlb_cmd invtlb_cmd;

	unsigned long                  vem_sync_fence_id;
	volatile unsigned long long*   vem_sync_fence_buf_ptr;
	unsigned long long             vem_sync_fence_buf_gpu_va;
	
	struct lisuan_gem_bo *barrier_buf;
	struct lisuan_gem_bo *dummy_page_buf;

	struct lisuan_gem_bo *dec_bufObj;
	void *vcmd_mgr;//store decvoder vcmd

	unsigned long long ring_fence_context;

	struct lisuan_gem_bo *enc_bufObj;
	void *enc_dev;//hantroenc_t *

	void *enc_vcmd_mgr;//use for isr

	struct os_mutex lock;
	spinlock_t bypass_lock;

	atomic_t checked_hang;//block for hang check reentry.
	atomic_t pf_req_reset;//reset req from pf if value ==1, otherwise reset is cur vf send to pf.
	atomic_t pf_finish_reset;//if pf hw reset finished or not.

	struct list_head list_lru[gem_priority_count] ;

	struct work_struct dec0_vcmd_work;
	struct work_struct dec1_vcmd_work;
	struct work_struct enc0_vcmd_work;
	struct work_struct im_vcmd_work;

	struct spinlock job_prepare_lock;
	struct list_head job_prepare_list;
	wait_queue_head_t job_prepare_wq;
	struct task_struct *job_prepare_thread;

	struct task_struct *ring_fence_signal_thread;

	//irq used
	struct tasklet_struct fenceirq_work;
	struct work_struct mailbox_work;
	struct work_struct therm_work;

	struct work_struct hdaudio_work;
	struct work_struct hpd_work;
	struct work_struct sgnt_work;

	wait_queue_head_t ring_fence_wake_up_task;
	gpu_event_log_t  gpu_event_log;
	gpu_usage_info_t gpu_usage_info;
	int debugfs_detail;
	struct lisuan_gem_bo *reserve_bufObj;//patch use for decoder
	struct lisuan_gem_bo *enc_reserve_bufObj; //use for encoder
	uint64_t reserve_offset[12];//use for decoder
	uint64_t enc_reserve_offset[12];//use for encoder	
};

struct lisuan_vm_pt {
	unsigned int level;
	unsigned int mtype;
	unsigned int update_shadow_buffer;
	struct lisuan_vm_pt *parent;
	struct lisuan_vm_pt *entries;
	struct lisuan_gem_bo *buffer;

	unsigned long *bitmaps;
};

struct lisuan_vm {
	struct lisuan_file *file;
	struct lisuan_mman mmgr;
	struct os_mutex *pt_lock;
	struct lisuan_vm_pt root;
	struct lisuan_procid *procid;
	struct lisuan_sched_entity *entity;
	struct lisuan_vm_update_map_funcs *update_func;
	char need_set_tlb;
	char is_L0_dirty;   // why use char, since when dirty, all engine need inv before use, so this will be a engine mask for invalidation indicate
	char is_L1_dirty;
	char is_L2_dirty;
	struct list_head free_pts;
	struct list_head defer_free_pts;//for cpu update path 

	struct gf_sync_fence last_update; //lastest updated pt fence
	struct gf_sync_fence last_used[RB_INDEX_LAST];

	uint64_t fence_context;
	uint32_t * shadow;
};

/*
 * file private structure
 */
struct lisuan_file {
	struct drm_device *drm_dev;
	struct lisuan_device *ls_dev;
	struct drm_file *drm_file;

	struct list_head residency_list;
	struct list_head evict_list;

	struct lisuan_vm vm;
	struct os_mutex *context_lock;
	struct idr context_handles;
	struct os_mutex *sync_object_lock;
	struct idr sync_object_handles;
	struct list_head dev_object;
	unsigned long tgid;//create
	unsigned long pid;//create
	unsigned long used_tgid;//used
	unsigned long used_pid;//used
	char name[128];
	char usedname[128];

	//used for cf pool map
	struct os_mutex *cf_pool_lock;
	void *gbo_cf;

	//used for vcmd map only
	struct os_mutex *vcmd_lock;
	void *dec_vcmd_map;
	void *enc_vcmd_map;
    void *vem_rb_map;
	struct os_mutex *va_list_lock;
	struct list_head va_64K_list;

	void *enc_vcmd_priv;//struct vcmd_priv_ctx *
	int proc_id;
	long long sys_size;
};

struct lisuan_cf_node {
	int cf_size;
	int cf_start;
	int cell_index;
	bool cf_map;
	struct lisuan_mman *cf_mmgr;
	struct drm_mm_node *cf_node;

	void (*free)(struct lisuan_cf_node * cf_node);
};

struct lisuan_cl_info {
    unsigned int cl_base;
    unsigned int cl_size;
};

struct lisuan_pci_info {
    int pos;
    unsigned int address_hi;
    unsigned int address_lo;
    unsigned short msi_ctl;
    unsigned short data;
    unsigned int adr_64bit;
    unsigned int msi_enable;
    unsigned int msi_mm_en;
};

struct mmap_args {
    unsigned int size;
    unsigned int domain;
    unsigned int snoop;

    struct sg_table* sg_table;
};


int lisuan_drm_open(struct drm_device *dev, struct drm_file *file);
void lisuan_drm_postclose(struct drm_device *dev, struct drm_file *file);
void lisuan_drm_last_close(struct drm_device *dev);
struct os_mutex *lisuan_init_os_mutex(void);
void lisuan_init_pci_device_info(struct lisuan_device *ls_dev);
void lisuan_init_mm_range(struct lisuan_mman *mmgr, unsigned long long start, unsigned long long end);
void lisuan_init_mm_range2(struct lisuan_mman *mmgr, unsigned long long start, unsigned long long end);
void lisuan_fini_mm_range(struct lisuan_mman *mmgr);
void lisuan_init_memory_manager(struct lisuan_device *ls_dev);
void lisuan_fini_memory_manager(struct lisuan_device *ls_dev);
int lisuan_query_card_info_ioctl(struct drm_device *dev, void *data, struct drm_file *filp);
int lisuan_query_vram_info_ioctl(struct drm_device *dev, void *data, struct drm_file *filp);
int lisuan_query_clock_calibration_ioctl(struct drm_device *dev, void *data, struct drm_file *filp);
int lisuan_query_cl_info(struct lisuan_device *ls_dev, struct lisuan_cl_info * cl_info);
int lisuan_query_pci_msi_info(struct lisuan_device *ls_dev, struct lisuan_pci_info * pci_info);
#endif
