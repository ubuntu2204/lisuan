/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include "lisuan_procid.h"
#include "lisuan_device.h"
#include "lisuan_vm.h"
#include "gf_hw_misc.h"
#include "lisuan_ring.h"

static int lisuan_vm_cpu_prepare(struct lisuan_device *ls_dev, struct lisuan_vm_update_params *params)
{
	return 0;
}

static int lisuan_vm_cpu_update_pde(struct lisuan_device *ls_dev, struct lisuan_vm_iter_pd_params *params, unsigned pteoffset,
			      struct lisuan_vm_pt *pt)
{
	struct lisuan_vm *vm = params->vm;
	struct lisuan_vm_pt *parent = params->parent;
	unsigned mask, idx, num_ptes;
	unsigned long long shift;
	unsigned long long pa, dst_pa;
	unsigned int *dst_va = parent->buffer->virt_addres;
	struct lisuan_gem_bo *gbo = pt->buffer;
	int ret = 0;

	int page_size = (params->mtype == MEM_TYPE_4MB ? GPU_BLOCK_SIZE : (params->mtype == MEM_TYPE_64KB ? GPU_HUGE_PAGE_SIZE : GPU_PAGE_SIZE));

	if (params->unmap && ls_dev->hwctl.skip_invalidate)
	{
		vm_info("pde-cpu: tgid:%d, skip unmap the pde handle:0x%x\n", vm->file->tgid, gbo->handle);
		return 0;
	}

	if (params->level == LS_VM_PD1 && params->mtype == MEM_TYPE_4MB)
		return ret;	//for this case, pd1 is the last level, update in pte stage, since the buffer's info not pass in here

	mask = lisuan_vm_entries_mask(ls_dev, params->level, params->mtype);
	shift = lisuan_vm_level_shift(ls_dev, params->level, params->mtype);
	num_ptes = lisuan_vm_num_entries(ls_dev, params->level, params->mtype);

	ASSERT(pteoffset < num_ptes);

	lisuan_get_phys_addr(ls_dev, &gbo->tbo, 0, page_size, &pa, gbo->flag.vram);
	lisuan_get_phys_addr(ls_dev, &parent->buffer->tbo, 0, page_size, &dst_pa, parent->buffer->flag.vram);

	if (params->level == LS_VM_PD0) {
		unsigned int level0_pte = 0;

		gf_hw_init_tlb_level0_pte(&level0_pte, !params->unmap, 0, !gbo->flag.vram, gbo->flag.snoop,pa >> GPU_PAGE_SHIFT);

		*(dst_va + pteoffset) = level0_pte;

		vm_info("pde-cpu: tgid:%d, set PD0(pa:0x%llx) offset:%d to %08x (pa:0x%llx), unmap:%d\n", 
			vm->file->tgid, dst_pa, pteoffset, level0_pte, params->unmap);
		vm->is_L0_dirty = 0xFF;
	} else if (params->level == LS_VM_PD1) {
		unsigned int level1_pte = 0;
		gf_hw_init_tlb_level1_pte(&level1_pte,!params->unmap,0,!gbo->flag.vram,gbo->flag.snoop,params->mtype == MEM_TYPE_4MB,params->mtype == MEM_TYPE_64KB,
			(params->mtype == MEM_TYPE_4MB) ? (pa >> GPU_BLOCK_SHIFT) : (pa >> GPU_PAGE_SHIFT));
		*(dst_va + pteoffset) = level1_pte;
		
		vm_info("pde-cpu: tgid:%d ,set PD1(pa:0x%llx) offset:%d to %08x (pa:0x%llx), unmap:%d\n",
			vm->file->tgid, dst_pa, pteoffset, level1_pte, params->unmap);
		vm->is_L1_dirty = 0xFF;
	}
    params->bitop_func(params->parent->bitmaps, pteoffset, 1);

	return 0;
}

static unsigned long long lisuan_vm_cpu_update_pte(struct lisuan_device *ls_dev, struct lisuan_vm_update_params *params, struct lisuan_vm_pt_cursor *cursor)
{
	struct lisuan_vm *vm = params->vm;
	struct lisuan_vm_pt *pt = cursor->entry;
	unsigned long long pa=0;
	unsigned int *dst_va;
	unsigned long long shift, end, entry_end;	
	unsigned mtype=0, pdestart, nptes;
	unsigned mask, ret = 0, i, j, page_offset, page_shift;
	unsigned page_size;
	struct lisuan_vm_node *vm_node = NULL;
	unsigned int dump_one = 1;

	end = params->node->offset + params->node->size - 1;
	
	if (cursor->level == LS_VM_PD2) {
		mtype = cursor->entry->mtype;
	} 
	else if (params->mtype == MEM_TYPE_4MB) {//4M pd1 has remove the mtype now.
		ASSERT(cursor->level == LS_VM_PD1);
		mtype = params->mtype;
	}

	page_size = (mtype == MEM_TYPE_4MB ? GPU_BLOCK_SIZE : (mtype == MEM_TYPE_64KB ? GPU_HUGE_PAGE_SIZE : GPU_PAGE_SIZE));
	mask = lisuan_vm_entries_mask(ls_dev, cursor->level, mtype);
	shift = lisuan_vm_level_shift(ls_dev, cursor->level, mtype);
	
	entry_end = ((uint64_t) mask + 1) << shift;
	entry_end += (cursor->start & ~(entry_end - 1)) - 1;
	entry_end = min(entry_end, end);
	
	pdestart = (cursor->start >> shift) & mask;
	nptes = ((entry_end >> shift) & mask) - pdestart + 1;
	
	page_shift = (mtype == MEM_TYPE_4MB ? GPU_BLOCK_SHIFT : (mtype == MEM_TYPE_64KB ? GPU_HUGE_PAGE_SHIFT : GPU_PAGE_SHIFT));

	if(pt->buffer)
	{
		vm_node = container_of(pt->buffer->va_list.next, struct lisuan_vm_node, node_item);
	}
	else
	{
		vm_info("pte-cpu: update pte but pt buffer invalid tgid:%d, level:%d, mtype:%d\n", vm->file->tgid, cursor->level, mtype);
		return entry_end;
	}

	if(pt->buffer)
		dst_va= pt->buffer->virt_addres;
	else
		ASSERT(0);

	if (params->unmap && ls_dev->hwctl.skip_invalidate)
	{
		vm_info("pte-cpu: skip unmap tgid:%d, level:%d, mtype:%d, ptb PA:0x%llx, pteoffset:%d, npte:%d, va range[0x%llx-0x%llx], unmap:%d, resolve 64k to 4k:%d\n", 
			vm->file->tgid, cursor->level, mtype, (vm_node->offset - ls_dev->mmu_bypass_start), pdestart, nptes, cursor->start, entry_end, params->unmap, cursor->entry->mtype != params->mtype);
		return entry_end;
	}

	vm_info("pte-cpu: tgid:%d, level:%d, mtype:%d, ptb PA:0x%llx, pteoffset:%d, npte:%d, va range[0x%llx-0x%llx], unmap:%d, resolve 64k to 4k:%d\n", 
		vm->file->tgid, cursor->level, mtype, (vm_node->offset - ls_dev->mmu_bypass_start), pdestart, nptes, cursor->start, entry_end, params->unmap, cursor->entry->mtype != params->mtype);

	params->bitop_func(pt->bitmaps, pdestart, nptes);

	if (cursor->entry->mtype != params->mtype)
		ASSERT(cursor->entry->mtype == MEM_TYPE_4KB);

	if(params->sparse_path)
	{
		unsigned level2_pte = 0;

		if(!params->sparse_clear_op)
		{
			gf_hw_init_tlb_level2_pte(&level2_pte,!params->unmap,0,!params->flag.vram,params->flag.snoop,0);
		}

		page_offset = (cursor->start - params->node->offset + params->pa_offset) / page_size;
		
		for (j = 0; j < nptes; j++) {
			if(params->tbo){
				lisuan_get_phys_addr(ls_dev, params->tbo, page_offset  + j, page_size, &pa, params->flag.vram);
				pa >>= page_shift;
				if(params->flag.vram)
					pa = (pa << 2) & 0x7FFFFFC;
			}
			
			gf_hw_init_tlb_level2_pte(&level2_pte,!params->unmap,0,!params->flag.vram,params->flag.snoop,
				pa);
			
			*(dst_va + pdestart + j) = level2_pte;
			if(dump_one){
				vm_info("pte-cpu-sparse: tgid:%d, offset(%d): %08x - %08x, shifted PA：%x\n", 
					vm->file->tgid, pdestart  + j, level2_pte, *(dst_va + pdestart + j), pa);
				dump_one = 0;
			}
		}
		vm->is_L2_dirty = 0xFF;
		return entry_end;
	}

	if (cursor->level == LS_VM_PD1) {
		unsigned int level1_pte = 0;
		
		ASSERT(params->mtype == MEM_TYPE_4MB);


		page_offset = (cursor->start - params->node->offset) / GPU_BLOCK_SIZE;

		for (i = 0; i < nptes; i++) {
			if(params->tbo){
				lisuan_get_phys_addr(ls_dev, params->tbo, page_offset + i, page_size, &pa, params->flag.vram);
				gf_hw_init_tlb_level1_pte(&level1_pte,!params->unmap,0,!params->flag.vram,params->flag.snoop,0,0,
								pa >> GPU_BLOCK_SHIFT);
			}
			*(dst_va + pdestart + i) = level1_pte;
			if(dump_one){
				vm_info("pte-cpu: tgid:%d, block pte, level:%d, offset(%d): %08x - %08x, PA:0x%llx\n", 
					vm->file->tgid, cursor->level, pdestart + i, level1_pte, *(dst_va + pdestart + i), pa);
				dump_one = 0;
			}
		}
		vm->is_L1_dirty = 0xFF;
	} else if (cursor->level == LS_VM_PD2) {
		unsigned int level2_pte = 0;

		page_offset = (cursor->start - params->node->offset) / page_size;
		for (i = 0; i < nptes; i++) {
			if(params->tbo){
				lisuan_get_phys_addr(ls_dev, params->tbo, page_offset + i, page_size, &pa, params->flag.vram);
				pa >>= page_shift;

				if(params->flag.vram)
					pa = (pa << 2) & 0x7FFFFFC;
				else
					ASSERT(!(pa & ~0xFFFFFFFULL));

				gf_hw_init_tlb_level2_pte(&level2_pte,!params->unmap,0,!params->flag.vram,params->flag.snoop,
					pa);
				}		
			*(dst_va + pdestart + i) = level2_pte;
			if(dump_one){
				vm_info("pte-cpu: tgid:%d, level:%d, offset(%d): %08x - %08x, PA:0x%x\n", 
					vm->file->tgid, cursor->level, pdestart + i, level2_pte, *(dst_va + pdestart + i), pa);
				dump_one = 0;
			}

		}
		vm->is_L2_dirty = 0xFF;
	}

	return entry_end;
}

static int lisuan_vm_cpu_commit(struct lisuan_device *ls_dev, struct lisuan_vm_update_params *params)
{
	return 0;
}

struct lisuan_vm_update_map_funcs lisuan_vm_cpu_funcs = {
	.prepare = lisuan_vm_cpu_prepare,
	.update_pde = lisuan_vm_cpu_update_pde,
	.update_pte = lisuan_vm_cpu_update_pte,
	.commit = lisuan_vm_cpu_commit
};

