/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include "gf_os_interface.h"
#include "lisuan_drm.h"
#include "lisuan_device.h"
#include "lisuan_vm.h"
#include "gf_hw_misc.h"
#include "lisuan_procid.h"
#include "lisuan_sync.h"

//#include "vcx_driver.h"
#include "subsys.h"
#include "vcx_normal_driver.h"
#include "vcx_vcmd_driver.h"
#include <drm/drm_fb_helper.h>

extern unsigned int mhb_disable_mask;
extern unsigned int cluster_slice_mask;

struct os_mutex *lisuan_init_os_mutex()
{
	struct os_mutex *mutex_lock = (struct os_mutex *)gf_kvcalloc(1, sizeof(struct os_mutex), GFP_KERNEL);
	mutex_init(&mutex_lock->lock);
	return mutex_lock;

}

static int ls_hantroenc_open(struct lisuan_device *gf_device, struct drm_file *filp)
{
	if (0)//(vcmd_support == 0)
		return hantroenc_open(gf_device, filp);
	else
		return hantrovcmd_open(gf_device, filp);
}

static int ls_hantroenc_release(struct lisuan_device *gf_device, struct drm_file *filp)
{
	if (0)//(vcmd_support == 0)
		return hantroenc_release(gf_device, filp);
	else
		return hantrovcmd_release(gf_device, filp);
}

int lisuan_drm_open(struct drm_device *dev, struct drm_file *file)
{
	struct lisuan_device *ls_dev = (struct lisuan_device *)dev->dev_private;
	struct lisuan_file *ls_file = (struct lisuan_file *)gf_kvcalloc(1, sizeof(struct lisuan_file), GFP_KERNEL);

	file->driver_priv = ls_file;

	INIT_LIST_HEAD(&ls_file->dev_object);
	INIT_LIST_HEAD(&ls_file->residency_list);
	INIT_LIST_HEAD(&ls_file->evict_list);
	INIT_LIST_HEAD(&ls_file->va_64K_list);

	idr_init(&ls_file->context_handles);
	idr_init(&ls_file->sync_object_handles);

	mutex_lock(&ls_dev->file_lock.lock);
	list_add(&ls_file->dev_object, &ls_dev->file_list);
	mutex_unlock(&ls_dev->file_lock.lock);

	ls_file->tgid = (unsigned long long)current->group_leader->pid;
	ls_file->pid = (unsigned long long)current->pid;
	ls_file->used_tgid = (unsigned long long)current->group_leader->pid;
	ls_file->used_pid = (unsigned long long)current->pid;
	snprintf(ls_file->name, sizeof(current->comm), "%s",current->comm);
	snprintf(ls_file->usedname, sizeof(current->comm), "%s",current->comm);

	gf_debug("open device:%s, pid:%d, tgid:%d\n", ls_file->name, ls_file->pid, ls_file->tgid);
	ls_file->va_list_lock = lisuan_init_os_mutex();
	ls_file->context_lock = lisuan_init_os_mutex();
	ls_file->sync_object_lock = lisuan_init_os_mutex();
	ls_file->cf_pool_lock = lisuan_init_os_mutex();
	ls_file->vcmd_lock = lisuan_init_os_mutex();

	//init the ls_file drm related item     
	ls_file->drm_file = file;
	ls_file->drm_dev = dev;
	ls_file->ls_dev = ls_dev;

	//init vm
	lisuan_vm_init_vm(ls_file);

	//init video
	if (!ls_dev->hwctl.no_video) {
		ls_hantroenc_open(ls_dev, file);
		hantrodec_open(ls_dev, file);
	}

	return 0;
}

static void lisuan_syncobj_clean_up(struct lisuan_file *ls_file)
{
	struct lisuan_device *ls_dev = ls_file->ls_dev;
	struct lisuan_monitor_fence *monitor_fence;
	int id;

	idr_for_each_entry(&ls_file->sync_object_handles, monitor_fence, id)
	{
		//free monitor_fence
		//here we do not free the monitor->buffer, 
		//since the buffer will be free at drm_gem_release
		//when the ap exit abnormal.
		gf_kvfree(monitor_fence);
	}

	idr_destroy(&ls_file->sync_object_handles);
	mutex_destroy(&ls_file->sync_object_lock->lock);
	gf_kvfree(ls_file->sync_object_lock);
}

void lisuan_drm_postclose(struct drm_device *dev, struct drm_file *file)
{
	struct lisuan_file *ls_file = (struct lisuan_file *)file->driver_priv;
	struct lisuan_device *ls_dev = (struct lisuan_device *)dev->dev_private;

	//TODO wait context idle?
	if (!ls_dev->hwctl.no_video) {
		hantrodec_release(dev, file);
		ls_hantroenc_release(ls_dev, file);
	}

	lisuan_ctx_mgr_fini(ls_dev, ls_file);

	lisuan_syncobj_clean_up(ls_file);

	lisuan_vm_fini_vm(ls_file);

	mutex_lock(&ls_dev->file_lock.lock);
	list_del(&ls_file->dev_object);
	mutex_unlock(&ls_dev->file_lock.lock);

	mutex_destroy(&ls_file->vcmd_lock->lock);
	gf_kvfree(ls_file->vcmd_lock);

	mutex_destroy(&ls_file->cf_pool_lock->lock);
	gf_kvfree(ls_file->cf_pool_lock);

	mutex_destroy(&ls_file->va_list_lock->lock);
	gf_kvfree(ls_file->va_list_lock);
	gf_debug("close pid:%d, sys mem:0x%llx\n", ls_file->tgid, ls_file->sys_size);

	gf_kvfree(ls_file);
	file->driver_priv = NULL;

}

void lisuan_drm_last_close(struct drm_device *dev)
{
	struct drm_fb_helper *fb_helper = dev->fb_helper;
	bool do_delayed;

	/* Change drm_fb_helper_lastclose: update fb client state first, Than client commit. */
	//gf_info("lisuan_drm_last_close, delayed_hotplug[%u]\n", dev->fb_helper->delayed_hotplug);

	if (!fb_helper || READ_ONCE(fb_helper->deferred_setup))
		return;

	mutex_lock(&fb_helper->lock);
	do_delayed = fb_helper->delayed_hotplug;
	if (do_delayed)
		fb_helper->delayed_hotplug = false;
	mutex_unlock(&fb_helper->lock);

	if (do_delayed)
		drm_fb_helper_hotplug_event(fb_helper);

#if DRM_VERSION_CODE >= KERNEL_VERSION(5,4,0)
	 mutex_lock(&fb_helper->lock);
	 drm_client_modeset_commit(&fb_helper->client);
	 mutex_unlock(&fb_helper->lock);
#else
	 drm_fb_helper_restore_fbdev_mode_unlocked(fb_helper);	
#endif
}

static int lisuan_pci_rebar_bytes_to_size(u64 bytes)
{
	bytes = roundup_pow_of_two(bytes);

	/* Return BAR size as defined in the resizable BAR specification */
	return max(ilog2(bytes), 20) - 20;
}

#define mmio_bar 2
#define mem_bar 0

#ifndef FIELD_GET
#define FIELD_GET(mask, reg) (((reg) & (mask)) >> (__builtin_ffs(mask) - 1))
#endif
/**
 * pci_rebar_find_pos - find position of resize ctrl reg for BAR
 * @pdev: PCI device
 * @bar: BAR to find
 *
 * Helper to find the position of the ctrl register for a BAR.
 * Returns -ENOTSUPP if resizable BARs are not supported at all.
 * Returns -ENOENT if no ctrl register for the BAR could be found.
 */
static int pci_rebar_find_pos(struct pci_dev *pdev, int bar)
{
	unsigned int pos, nbars, i;
	u32 ctrl;

	pos = pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_REBAR);
	if (!pos)
		return -ENOTSUPP;

	pci_read_config_dword(pdev, pos + PCI_REBAR_CTRL, &ctrl);
	nbars = FIELD_GET(PCI_REBAR_CTRL_NBAR_MASK, ctrl);

	for (i = 0; i < nbars; i++, pos += 8) {
		int bar_idx;

		pci_read_config_dword(pdev, pos + PCI_REBAR_CTRL, &ctrl);
		bar_idx = FIELD_GET(PCI_REBAR_CTRL_BAR_IDX, ctrl);
		if (bar_idx == bar)
			return pos;
	}

	return -ENOENT;
}

/**
 * pci_rebar_get_possible_sizes - get possible sizes for BAR
 * @pdev: PCI device
 * @bar: BAR to query
 *
 * Get the possible sizes of a resizable BAR as bitmask defined in the spec
 * (bit 0=1MB, bit 19=512GB). Returns 0 if BAR isn't resizable.
 */
uint32_t compat_pci_rebar_get_possible_sizes(struct pci_dev *pdev, int bar)
{
	int pos;
	uint32_t cap;

	pos = pci_rebar_find_pos(pdev, bar);
	if (pos < 0)
		return 0;

	pci_read_config_dword(pdev, pos + PCI_REBAR_CAP, &cap);
	cap = FIELD_GET(PCI_REBAR_CAP_SIZES, cap);

	/* Sapphire RX 5600 XT Pulse has an invalid cap dword for BAR 0 */
	if (pdev->vendor == PCI_VENDOR_ID_ATI && pdev->device == 0x731f &&
	    bar == 0 && cap == 0x700)
		return 0x3f00;

	return cap;
}

/**
 * lisuan_device_resize_fb_bar - try to resize FB BAR
 *
 * @lisuandev: lisuan_device pointer
 *
 * Try to resize FB BAR to make all VRAM CPU accessible. We try very hard not
 * to fail, but if any of the BARs is not accessible after the size we abort
 * driver loading by returning -ENODEV.
 */
static int lisuan_device_resize_fb_bar(struct lisuan_device *lisuandev)
{

	int rbar_size = lisuan_pci_rebar_bytes_to_size(lisuandev->vram_size);	
	struct pci_bus *root;
	struct resource *res;
	unsigned i;
	unsigned short cmd;
	int r;

	/* Bypass for VF */
	if (lisuandev->is_virtfn)
		return 0;

	/* skip if the bios has already enabled large BAR */
	if (lisuandev->vram_size &&
	    (pci_resource_len(lisuandev->pdev, mem_bar) >= lisuandev->vram_size))
		return 0;

	/* Check if the root BUS has 64bit memory resources */
	root = lisuandev->pdev->bus;
	while (root->parent)
		root = root->parent;

	pci_bus_for_each_resource(root, res, i) {
		if (res && res->flags & (IORESOURCE_MEM | IORESOURCE_MEM_64) &&
		    res->start > 0x100000000ull)
			break;
	}

	/* Trying to resize is pointless without a root hub window above 4GB */
	if (!res)
		return 0;

	/* Limit the BAR size to what is available */
	rbar_size = min(fls(compat_pci_rebar_get_possible_sizes(lisuandev->pdev, mem_bar)) - 1,
			rbar_size);
	/* Disable memory decoding while we change the BAR addresses and size */
	pci_read_config_word(lisuandev->pdev, PCI_COMMAND, &cmd);
	pci_write_config_word(lisuandev->pdev, PCI_COMMAND,
			      cmd & ~PCI_COMMAND_MEMORY);


	pci_release_resource(lisuandev->pdev, mem_bar);
#if DRM_VERSION_CODE >= KERNEL_VERSION(6,19,0)   
	r = pci_resize_resource(lisuandev->pdev, mem_bar, rbar_size, 0);
#else
	r = pci_resize_resource(lisuandev->pdev, mem_bar, rbar_size);
#endif
	if (r == -ENOSPC)
		gf_error("Not enough PCI address space for a large BAR.\n");
	else if (r && r != -ENOTSUPP)
		gf_error("Problem resizing BAR0 (%d).\n", r);

	pci_assign_unassigned_bus_resources(lisuandev->pdev->bus);

	pci_write_config_word(lisuandev->pdev, PCI_COMMAND, cmd);

	return 0;
}

#define PCI_STD_NUM_BARS	6 /* Number of standard BARs */
void lisuan_init_pci_device_info(struct lisuan_device *ls_dev)
{
	resource_size_t start, end;

	struct pci_dev *pdev = ls_dev->pdev;
	struct lisuan_pcie_info *pdev_info = &ls_dev->pdev_info;

	//fetch the mmio & mem bar
	unsigned int flag = 0;
	unsigned mem_res_cnt = 0;
	unsigned mem_barno = 0, mmio_barno = 0, i = 0;
	unsigned long long aper_size = 0, visiable_size = 0;
	unsigned gpc_mask_reg = 0, fb_cfg = 0;

	for (i = 0; i < PCI_STD_NUM_BARS; i++) {
		flag = pci_resource_flags(pdev, i);

		if (flag & IORESOURCE_MEM) {
			if (flag & IORESOURCE_PREFETCH) {
				mem_barno = i;
			} else {
				mmio_barno = i;
			}
			mem_res_cnt++;
		}
	}
	if (mem_res_cnt != 2) {
		gf_info("not found correct mem resource, use the default value\n");
		mem_barno = mem_bar;
		mmio_barno = mmio_bar;
	}

	start = pdev_info->start_addr_io = pci_resource_start(pdev, mmio_barno);
	end = pdev_info->end_addr_io = pci_resource_end(pdev, mmio_barno);

#if defined(__arm64__) || defined(__aarch64__) || defined(CONFIG_SW64) || defined(__loongarch__)
	ls_dev->mmio_base = ioremap(start, end - start);
#else
	ls_dev->mmio_base = ioremap_uc(start, end - start);
#endif

	start = pdev_info->start_addr_rom = pci_resource_start(pdev, PCI_ROM_RESOURCE);
	end = pdev_info->end_addr_rom = pci_resource_end(pdev, PCI_ROM_RESOURCE);
	gf_debug("rom start: 0x%x, size: 0x%x\n", start, end - start);
	ls_dev->rom_base = (unsigned char*)ioremap(start, end - start);

	ls_dev->chip_id = CHIP_GF;
	ls_dev->slc_mask = cluster_slice_mask;
	ls_dev->mhu_disable_mask = mhb_disable_mask;
	
	gf_get_vf_info(ls_dev->mmio_base, &ls_dev->is_virtfn, &ls_dev->virtfn_id);
	gf_info("VF mode:%d, cur vfid:%d\n", ls_dev->is_virtfn, ls_dev->virtfn_id);

	if (ls_dev->is_virtfn)
	{
		unsigned hw_cfg = 0;

		ls_dev->mhu_disable_mask = gf_get_mhu_disable_mask(ls_dev->mmio_base);
		ls_dev->mhu_disable_mask &= 0x3F;

		hw_cfg = gf_get_vf_hw_cfg(ls_dev->mmio_base);
		if(((hw_cfg & 0xFFFF) == ((hw_cfg >> 16) & 0xFFFF)) && 
			((hw_cfg >> 14) & 0x3) == 0x2)
		{
			hw_cfg = hw_cfg & 0xFFFF;
			gpc_mask_reg = (hw_cfg & 0x3F);
			fb_cfg = !!(hw_cfg & 0x40);
		}

		if(fb_cfg)
		{
			ls_dev->max_mhb_access_size = 4*1024*1024*1024ULL;
		}
		else
		{
			ls_dev->max_mhb_access_size = 2*1024*1024*1024ULL;
		}

		ls_dev->mhb_num = 0;
		for (i = 0; i < 6; i++)
		{
			if ((ls_dev->mhu_disable_mask & (1 << i))==0)
			{
				ls_dev->mhb_num++;
			}
		}
		ls_dev->vram_size = gf_get_vf_fb_size(ls_dev->mmio_base) * 1024 * 1024ULL; 
	}
	else
	{
		gpc_mask_reg = gf_get_gpc_mask(ls_dev->mmio_base);
		gpc_mask_reg = (gpc_mask_reg >> 4) & 0x3F;

		fb_cfg = gf_get_fb_cfg(ls_dev->mmio_base);
		if (fb_cfg == HW_24G_LOCAL_VALUE)
		{
			ls_dev->max_mhb_access_size = 4*1024*1024*1024ULL;
		}else
		{
			ls_dev->max_mhb_access_size = 2*1024*1024*1024ULL;
		}

		ls_dev->mhb_num = 0;
		for (i = 0; i < 6; i++)
		{
			if ((ls_dev->mhu_disable_mask & (1 << i))==0)
			{
				ls_dev->mhb_num++;
			}
		}
		
		ls_dev->vram_size = ls_dev->mhb_num * ls_dev->max_mhb_access_size;
	}

	for (i = 0; i < MAX_GPC_NUMBER; i++)
	{
		if (gpc_mask_reg & (1 << i))
		{
			ls_dev->slc_mask = ls_dev->slc_mask & ~(0xF << (i * 4));
		}
	}

	if (ls_dev->mhb_num == 6)
	{
		ls_dev->gap_size = ((ls_dev->mhb_num * ls_dev->max_mhb_access_size) * 4 / (22 * 6));
		ls_dev->gap_size = (((ls_dev->gap_size >> 20) + 1) << 20);
	}
	else if (ls_dev->mhb_num == 3)
	{
		ls_dev->gap_size = ((ls_dev->mhb_num * ls_dev->max_mhb_access_size) / (43 * 3));
		ls_dev->gap_size = (((ls_dev->gap_size >> 20) + 1) << 20);
	}

	gf_debug("mhb disable_mask: 0x%x, mhb_num:%d, cluster slice mask:0x%x\n", ls_dev->mhu_disable_mask, ls_dev->mhb_num, ls_dev->slc_mask);

	lisuan_device_resize_fb_bar(ls_dev);

	pdev_info->start_addr_vram = pci_resource_start(pdev, mem_barno);
	pdev_info->end_addr_vram = pci_resource_end(pdev, mem_barno);
	aper_size = pdev_info->end_addr_vram - pdev_info->start_addr_vram + 1;
	
	/* In case the PCI BAR is larger than the actual amount of vram */
	if (aper_size > ls_dev->vram_size)
			pdev_info->end_addr_vram = pdev_info->start_addr_vram + ls_dev->vram_size -1;

	visiable_size = pdev_info->end_addr_vram - pdev_info->start_addr_vram + 1;
	if (ls_dev->hwctl.use_vram)
	{
		ls_dev->bufsel.fence_buffer_dom = dom_visual_vram;
		ls_dev->bufsel.kcmd_buffer_dom = dom_visual_vram;
		ls_dev->bufsel.ringbuf_cmd_buffer_dom = dom_visual_vram;
	}
	else if(!ls_dev->hwctl.support_wc)
	{
		ls_dev->bufsel.fence_buffer_dom = dom_snoop_system_pages;
		ls_dev->bufsel.kcmd_buffer_dom = dom_snoop_system_pages;
		ls_dev->bufsel.ringbuf_cmd_buffer_dom = dom_snoop_system_pages;
	}
	else
	{
		ls_dev->bufsel.fence_buffer_dom = dom_system_pages;
		ls_dev->bufsel.kcmd_buffer_dom = dom_system_pages;
		ls_dev->bufsel.ringbuf_cmd_buffer_dom = dom_system_pages;
	}

	ls_dev->bufsel.internal_cmd_buffer_dom = dom_visual_vram;
	ls_dev->bufsel.bypass_pt_buffer_dom = dom_visual_vram;
	ls_dev->bufsel.none_bypass_pt_buffer_dom = (ls_dev->hwctl.cpu_update_ptes ? 
      dom_visual_vram : (visiable_size * 2  < ls_dev->vram_size ) ? 
      dom_invisual_vram : dom_visual_vram);

	ls_dev->bufsel.cf_pool_dom = (visiable_size * 2  < ls_dev->vram_size ) ? 
      dom_invisual_vram : dom_visual_vram;

	ls_dev->sys_page_size = PAGE_SIZE;
	ls_dev->sys_page_shift = PAGE_SHIFT;
	ls_dev->sys_page_mask = PAGE_MASK;

	gf_debug("aper_size:0x%llx, sys page size-shift:0x%x, shift:%d\n",
			aper_size, ls_dev->sys_page_size, ls_dev->sys_page_shift);
}

void lisuan_init_mm_range(struct lisuan_mman *mmgr, unsigned long long start, unsigned long long end)
{
	unsigned int i = 0;

	spin_lock_init(&mmgr->lock);
	mmgr->mm = (struct drm_mm *)gf_kvcalloc(1, sizeof(struct drm_mm), GFP_KERNEL);
	mmgr->start_addr = start;
	mmgr->end_addr = end;
	mmgr->size = end - start;
	drm_mm_init(mmgr->mm, mmgr->start_addr, mmgr->end_addr - mmgr->start_addr);

	for(i = gem_priority_unused; i < gem_priority_count; i++)
	{
		INIT_LIST_HEAD(&mmgr->list_lru[i]);
	}
}

void lisuan_init_mm_range2(struct lisuan_mman *mmgr, unsigned long long start, unsigned long long end)
{
	unsigned int i = 0;

	spin_lock_init(&mmgr->lock);
	mmgr->mm = (struct drm_mm *)gf_kvcalloc(1, sizeof(struct drm_mm), GFP_KERNEL);
	mmgr->start_addr = start;
	mmgr->end_addr = end;
	mmgr->size = end - start;
	drm_mm_init(mmgr->mm, mmgr->start_addr, mmgr->end_addr - mmgr->start_addr);
	INIT_LIST_HEAD(&mmgr->list_head);
	mmgr->type_2 = true;
}

static void lisuan_show_leaks(struct drm_mm *mm)
{
	struct drm_mm_node *node, *tmp;

	list_for_each_entry_safe(node, tmp, drm_mm_nodes(mm), node_list) {
			gf_debug("node [%llx + %llx]: unknown owner\n",
				  node->start, node->size);
			drm_mm_remove_node(node);
	}
}

static void lisuan_check_mm_leak(struct drm_mm *mm)
{
	if (!drm_mm_clean(mm))
		lisuan_show_leaks(mm);
}

void lisuan_fini_mm_range(struct lisuan_mman *mmgr)
{
	spin_lock(&mmgr->lock);
	lisuan_check_mm_leak(mmgr->mm);
	drm_mm_takedown(mmgr->mm);
	spin_unlock(&mmgr->lock);

	gf_kvfree(mmgr->mm);
}

void lisuan_init_memory_manager(struct lisuan_device *ls_dev)
{
	struct lisuan_pcie_info *pdev_info = &ls_dev->pdev_info;
	resource_size_t start, end;
	struct lisuan_mman *mmgr = ls_dev->mmgr;
	struct lisuan_cl_info cl_info;

	start = pdev_info->start_addr_vram;
	end = pdev_info->end_addr_vram;

	ls_dev->visual_vram_size = end - start + 1;

	//for vf, bypass start coordinate with win driver, since bypass range has only one pair bypass reg for all vfs.
	//won't use GPUVA: 0~ls_dev->mmu_bypass_start 
	ls_dev->mmu_bypass_start = 0;//ls_dev->is_virtfn ? BYPASS_RANGE_START_GPUVA : 0;
	ls_dev->mmu_bypass_start += (ALIGN64_SIZE(ls_dev->vram_size, 0x100000000ULL) - ls_dev->vram_size);
	gf_assert(((ls_dev->mmu_bypass_start + ls_dev->vram_size) & 0xFFFFFFFF) == 0);//check bypass pcie start align to 4G
	ls_dev->mmu_bypass_end = ls_dev->mmu_bypass_start + ls_dev->vram_size + BYPASS_RANGE_PCIE_SPACE_SIZE;
	gf_info("visual vram = 0x%llx M, total vram = %llx M\n", ls_dev->visual_vram_size / 1024 / 1024, ls_dev->vram_size / 1024 / 1024);

	//bprange of visual vram
	lisuan_init_mm_range(mmgr, ls_dev->mmu_bypass_start, ls_dev->mmu_bypass_start + ls_dev->visual_vram_size);

	//bprange of invisual vram
	mmgr++;
	lisuan_init_mm_range(mmgr, ls_dev->mmu_bypass_start + ls_dev->visual_vram_size, ls_dev->mmu_bypass_start + ls_dev->vram_size);

	//bprange of pcie
	mmgr++;
	lisuan_init_mm_range2(mmgr, ls_dev->mmu_bypass_start + ls_dev->vram_size, ls_dev->mmu_bypass_end);
	ls_dev->mmu_bypass_pcie_start = mmgr->start_addr;
	gf_debug("mmu bypass range: 0x%llx ~ 0x%llx, mmu bypass pcie start:0x%llx\n",
		ls_dev->mmu_bypass_start, ls_dev->mmu_bypass_end, ls_dev->mmu_bypass_pcie_start);

	if(ls_dev->gap_size)
	{
		if(ls_dev->mmgr[dom_invisual_vram-1].end_addr - ls_dev->mmgr[dom_invisual_vram-1].start_addr > 0)
		{
			gf_assert(ls_dev->mmgr[dom_invisual_vram-1].end_addr - ls_dev->mmgr[dom_invisual_vram-1].start_addr >= ls_dev->gap_size);
			ls_dev->gap_node = lisuan_vram_mgr_new(&ls_dev->mmgr[dom_invisual_vram-1], ls_dev->gap_size, 0x10000, 1);
		}else{
			ls_dev->gap_node = lisuan_vram_mgr_new(&ls_dev->mmgr[dom_visual_vram-1], ls_dev->gap_size, 0x10000, 1);
		}
		gf_debug("gap range:0x%llx ~ 0x%llx, size:0x%llx\n", ls_dev->gap_node->start, ls_dev->gap_node->start + ls_dev->gap_node->size, ls_dev->gap_size);
	}

	//reserve the dummy page buffer
	//another purpose is used for fb overwrite issue.
	{
		struct lisuan_gem_bo *buf = gf_kvcalloc(1, sizeof(struct lisuan_gem_bo), GFP_KERNEL);
		buf->size = 2560*1440*4;
		buf->domain = dom_visual_vram;
		buf->flag.byPass = 1;
		buf->align = 4096;
		buf->pm_discard = 1;
		atomic_set(&buf->unpagable, 1);
		buf->priority = gem_priority_high;
		buf->flag.vram = (buf->domain <= dom_invisual_vram) ? 1 : 0;
		buf->flag.snoop = ((buf->domain == dom_snoop_system_pages) ? 1 : 0);

		lisuan_gem_alloc_oneshot(ls_dev, NULL, buf, 0);
		ls_dev->dummy_page_buf = buf;
	}

	lisuan_init_cf_pool(ls_dev);

	lisuan_query_cl_info(ls_dev,&cl_info);
	gf_hw_mhb_init(ls_dev->mmio_base,
                   ls_dev->vram_size / 1024 / 1024,
                   cl_info.cl_base,
                   cl_info.cl_size,
                   ls_dev->mhu_disable_mask,
                   ls_dev->mhb_num,
                   ls_dev->is_virtfn);

	gf_pci_mem_config_init(ls_dev->mmio_base, ls_dev->hwctl.enable_hw_cg, ls_dev->is_virtfn);

	if (ls_dev->hwctl.test_local_mem) {
		lisuan_do_mem_verify(ls_dev);
	}

	lisuan_vm_init_bypass_pt(ls_dev);
	
	//reserve the barrier local buffer for rw
	{
		struct lisuan_gem_bo *buf = gf_kvcalloc(1, sizeof(struct lisuan_gem_bo), GFP_KERNEL);
		buf->size = 0x1000;
		buf->domain = dom_visual_vram;
		buf->flag.byPass = 1;
		buf->align = 4096;
		buf->pm_discard = 1;
		atomic_set(&buf->unpagable, 1);
		buf->priority = gem_priority_high;
		buf->flag.vram = (buf->domain <= dom_invisual_vram) ? 1 : 0;
		buf->flag.snoop = ((buf->domain == dom_snoop_system_pages) ? 1 : 0);

		lisuan_gem_alloc_oneshot(ls_dev, NULL, buf, 1);
		ls_dev->barrier_buf = buf;
	}
}

int lisuan_query_cl_info(struct lisuan_device *ls_dev, struct lisuan_cl_info * cl_info)
{
    struct lisuan_cf_man *cf = &ls_dev->cf;

	cl_info->cl_base = (cf->mmgr.start_addr - ls_dev->mmu_bypass_start) >> 17;	
	cl_info->cl_size = ilog2(cf->cf_total_size/(4*1024*1024));
	ls_dev->cl_size = cl_info->cl_size ;

    return 0;
}

int lisuan_query_pci_msi_info(struct lisuan_device *ls_dev, struct lisuan_pci_info * pci_info)
{
    struct pci_dev *pdev = ls_dev->pdev;
	int pos = pci_find_capability(pdev, PCI_CAP_ID_MSI);


	pci_read_config_word(pdev, pos + PCI_MSI_FLAGS, &pci_info->msi_ctl);
	pci_read_config_word(pdev, pos + PCI_MSI_DATA_32, &pci_info->data);
	pci_read_config_dword(pdev, pos + PCI_MSI_ADDRESS_LO, &pci_info->address_lo);

	if (pci_info->msi_ctl & PCI_MSI_FLAGS_64BIT) {
		pci_read_config_dword(pdev, pos + PCI_MSI_ADDRESS_HI, &pci_info->address_hi);
		pci_read_config_word(pdev, pos + PCI_MSI_DATA_64, &pci_info->data);
	}

	pci_info->adr_64bit = !!(pci_info->msi_ctl & PCI_MSI_FLAGS_64BIT);
	pci_info->msi_enable = !!(pci_info->msi_ctl & PCI_MSI_FLAGS_ENABLE);
	pci_info->msi_mm_en= (pci_info->msi_ctl & PCI_MSI_FLAGS_QSIZE) >> 4;
	pci_info->pos = pos;
	//gf_info("msi pos %d, addr 64 en:%d, msi en:%d, msi mm:%d\n", pos, pci_info->adr_64bit, pci_info->msi_enable, pci_info->msi_mm_en);

    return 0;
}
int lisuan_query_card_info_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct lisuan_device *ls_device = dev->dev_private;

	struct adapter_info_s *info_ptr = (struct adapter_info_s *)data;
	struct sysinfo si;
	unsigned int i;

	si_meminfo(&si);

	gf_memset(info_ptr, 0, sizeof(struct adapter_info_s));
	
	info_ptr->local_heap_alignment = ls_device->hwctl.enable_local_huge_page?GPU_HUGE_PAGE_SIZE:GPU_PAGE_SIZE;
	info_ptr->pcie_heap_alignment = ls_device->sys_page_size;

	info_ptr->seg_count = dom_mm_max;
	
	info_ptr->seg_descriptors[0].size = ls_device->visual_vram_size;
	info_ptr->seg_descriptors[0].flags.value = 0;
	info_ptr->seg_descriptors[0].flags.cpu_visible = 1;
	
	info_ptr->seg_descriptors[1].size = ls_device->vram_size - ls_device->visual_vram_size;
	info_ptr->seg_descriptors[1].flags.value = 0;

	//caps set to max of sysmem/2 and BYPASS_RANGE_PCIE_SPACE_SIZE, but actual is unlimited
	info_ptr->seg_descriptors[2].size = max(BYPASS_RANGE_PCIE_SPACE_SIZE,(u64)si.totalram * si.mem_unit / 2);
	info_ptr->seg_descriptors[2].flags.value = 0;
	info_ptr->seg_descriptors[2].flags.cpu_visible = 1;
	info_ptr->seg_descriptors[2].flags.aperture = 1;
	info_ptr->seg_descriptors[2].flags.cache_coherent = 1;
	
	info_ptr->gpuva_start = 0;
	info_ptr->gpuva_size = 1LL << 40;
	info_ptr->gpuva_bypass_start = ls_device->mmu_bypass_start;
	info_ptr->gpuva_bypass_size = ls_device->mmu_bypass_end - ls_device->mmu_bypass_start;
	info_ptr->dma_buffer_private_data_size = DMA_BUFFER_PRIVATE_DATA_SIZE;
	info_ptr->gpc_slc_mask = ls_device->slc_mask;
	info_ptr->mhb_num = ls_device->mhb_num;
	info_ptr->cl_size = ls_device->cl_size;
	info_ptr->vendor_id = ls_device->pdev->vendor;
	info_ptr->device_id = ls_device->pdev->device;
	info_ptr->chip_id = (info_ptr->device_id & DEVICE_MASK);
	info_ptr->is_virtual = ls_device->is_virtfn;
	info_ptr->virtfn_id = ls_device->virtfn_id;

	info_ptr->pci_domain = pci_domain_nr(ls_device->pdev->bus);
	info_ptr->pci_bus = ls_device->pdev->bus->number;
	info_ptr->pci_dev = PCI_SLOT(ls_device->pdev->devfn);
	info_ptr->pci_func = PCI_FUNC(ls_device->pdev->devfn);

	info_ptr->enable_new_auto_clear = ls_device->hwctl.en_new_cl_clr;
	info_ptr->msaa_2x_enable = !ls_device->hwctl.en_new_cl_clr;

	return 0;
}

int lisuan_query_vram_info_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct lisuan_device *ls_dev = dev->dev_private;
	struct query_vram_info_s *info_ptr = (struct query_vram_info_s *)data;
	int ret = 0;
	unsigned long long total = 0, free = 0, used = 0;
	struct sysinfo *si = NULL;
	struct lisuan_mman *mmgr = NULL;

	switch(info_ptr->query_segment)
	{
		case cpuvisible_local_segment:
			mmgr = &ls_dev->mmgr[dom_visual_vram - 1];
			spin_lock(&mmgr->lock);
			gf_mm_print(mmgr->mm, &used, &free, &total);
			spin_unlock(&mmgr->lock);
			break;
		case noncpuvisible_local_segment:
			mmgr = &ls_dev->mmgr[dom_invisual_vram - 1];
			spin_lock(&mmgr->lock);
			gf_mm_print(mmgr->mm, &used, &free, &total);
			spin_unlock(&mmgr->lock);
			break;
		case pcie_segment:
			si = kmalloc(sizeof(struct sysinfo), GFP_KERNEL);
			if (!si){
				ret = -ENOMEM;
				break;
			}
			si_meminfo(si);
			total = si->totalram;
			free  = si->freeram;
			total = total << ls_dev->sys_page_shift;
			free  = free  << ls_dev->sys_page_shift;
			used  = total - free;

			//follow heap size usage
			total = total / 2;
			if(si) kfree(si);
			break;
		default:
			gf_error("invalid query vram info type:%d\n", info_ptr->query_segment);
			ret = -EINVAL;
			break;
	}

	info_ptr->current_usage = used;
	info_ptr->budget = total;

	return ret;
}
int lisuan_query_clock_calibration_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct lisuan_device *ls_device = dev->dev_private;
	uint64_t clock_lo = 0, clock_hi = 0;
	struct query_clock_calibration *p_arg = (struct query_clock_calibration *)data;

	preempt_disable();
	gf_get_time_stamp(ls_device->mmio_base, (uint32_t*)&clock_hi, (uint32_t*)&clock_lo);
	preempt_enable();
	p_arg->gpu_counter = clock_lo | (clock_hi << 32ULL);

	return 0;
}

void lisuan_fini_memory_manager(struct lisuan_device *ls_dev)
{
	struct lisuan_mman *mmgr;
	int i;


	lisuan_fini_cf_pool(ls_dev);

	lisuan_vm_fini_bypass_pt(ls_dev);

	lisuan_gem_free_oneshot(ls_dev, ls_dev->barrier_buf);
	lisuan_gem_free_oneshot(ls_dev, ls_dev->dummy_page_buf);

	if(ls_dev->gap_node) {
		drm_mm_remove_node(ls_dev->gap_node);
	}

	for (i = 0; i < dom_mm_max; i++) {
		mmgr = &ls_dev->mmgr[i];

		lisuan_fini_mm_range(mmgr);
	}
}
