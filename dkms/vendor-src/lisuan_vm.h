/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __LISUAN_VM_H__
#define __LISUAN_VM_H__

#include <drm/drm_mm.h>

struct lisuan_device;
struct lisuan_kbo_mman;
struct lisuan_kbo_node;
struct lisuan_cf_node;

#include "lisuan_device.h"
#include "lisuan_scheduler.h"
#include "lisuan_gem.h"

#define LS_VM_PTE_COUNT(adev,level,mtype) (1 << (adev)->pt_count[mtype][level])

struct lisuan_buffer_config {
	bool byPass;		//0:no locate in bypass;1: in bypass
	bool vram;
	bool nsec;		//security or not,0 for security,1 for not.
	bool pa_exist;		//pa alread exist.
	bool large_page;	//0:<4KB,1:<64KB                
	bool snoop;
	bool do_map;
	bool block;
	bool direction; //0:DRM_MM_INSERT_LOW, 1:DRM_MM_INSERT_HIGH
};

struct lisuan_vm_node {
	struct lisuan_gem_bo *gem;
	struct lisuan_file *ls_file;
	struct list_head mm_item;	//add on mmgr
	struct list_head node_item;	//add on buf va list
	struct list_head vm_item;	//add on vm/file va list

	unsigned int size;	//node size
	unsigned long long offset;	//node start address
	unsigned long long age;
	bool byPass;
	bool sparse_path;

	struct drm_mm_node *mm_node;
	struct lisuan_mman *mmgr;
	void (*free)(struct lisuan_vm_node *);
	void (*free_locked)(struct lisuan_vm_node *);
	int ref_cnt;
};

struct lisuan_vm_pt_cursor {
	struct lisuan_vm_pt *parent;
	unsigned long long start;
	unsigned int level;
	unsigned int mtype;
	struct lisuan_vm_pt *entry;
};
struct lisuan_vm_iter_pd_params {
	struct lisuan_vm *vm;
	unsigned int pad;
	struct gf_job *job;
	unsigned long long offset;
	struct lisuan_vm_pt *parent;
	unsigned long long start;
	unsigned long long end;
	bool unmap;
	int mtype;
	int level;
	void (*bitop_func)(unsigned long *map, unsigned int start, int len);
};

struct lisuan_vm_update_params {
	int mtype;
	struct lisuan_vm *vm;
	struct gf_job *job;

	struct lisuan_vm_node *node;

	struct ttm_buffer_object *tbo;

	struct lisuan_buffer_config flag;
	bool unmap; //used to map or unmap bit
	bool sparse_path;//used for sparse update_gpu_va used
	bool sparse_clear_op;//used for sparse update_gpu_va used
	uint64_t pa_offset; //used for sparse update_gpu_va used
	void (*bitop_func)(unsigned long *map, unsigned int start, int len);
};

struct lisuan_vm_update_map_funcs {
	int (*prepare)(struct lisuan_device * ls_dev, struct lisuan_vm_update_params * params);
	int (*update_pde)(struct lisuan_device * ls_dev, struct lisuan_vm_iter_pd_params * params, unsigned pteoffset, struct lisuan_vm_pt * pt);
	unsigned long long (*update_pte)(struct lisuan_device * ls_dev, struct lisuan_vm_update_params * params, struct lisuan_vm_pt_cursor * cursor);
	int (*commit)(struct lisuan_device * ls_dev, struct lisuan_vm_update_params * params);
};

struct drm_mm_node *lisuan_vram_mgr_new(struct lisuan_mman *mmgr, unsigned int size, unsigned int align, int direction);
struct lisuan_vm_node *lisuan_vm_alloc_node(struct lisuan_mman *mmgr, struct lisuan_gem_bo * gem, unsigned int size, unsigned int align, int direction);
int lisuan_vm_entries_mask(struct lisuan_device *ls_dev, int level, unsigned int mtype);
unsigned lisuan_vm_num_entries(struct lisuan_device *ls_dev, int level, int mtype);
unsigned lisuan_vm_level_shift(struct lisuan_device *ls_dev, unsigned level, unsigned mtype);

void lisuan_vm_free_node(struct lisuan_vm_node *vm_node);
void lisuan_vm_free_node_locked(struct lisuan_vm_node *vm_node);

int lisuan_vm_map_va_range(struct lisuan_device *ls_dev, struct lisuan_file *ls_file, struct lisuan_gem_bo *buffer, struct lisuan_vm_node *vm_node);
void lisuan_vm_unmap_va_range(struct lisuan_device *ls_dev, struct lisuan_file *ls_file, struct lisuan_gem_bo *buffer, struct lisuan_vm_node *vm_node);
void lisuan_vm_map_bypass_pt(struct lisuan_device *ls_dev, struct lisuan_gem_bo *gbo, struct lisuan_vm_node *vm_node);
void lisuan_vm_unmap_bypass_pt(struct lisuan_device *ls_dev, struct lisuan_gem_bo *gbo, struct lisuan_vm_node *vm_node);
int lisuan_vm_unmap_and_release_pt(struct lisuan_device *ls_dev, struct lisuan_vm_update_params *params);
void lisuan_vm_init_vm(struct lisuan_file *ls_file);
void lisuan_vm_fini_vm(struct lisuan_file *ls_file);

int lisuan_do_mem_verify(struct lisuan_device *ls_dev);
int lisuan_vm_init_bypass_pt(struct lisuan_device *ls_dev);
void lisuan_vm_fini_bypass_pt(struct lisuan_device *ls_dev);
void lisuan_vm_reinit_bypass_pt(struct lisuan_device *ls_dev);

int lisuan_reserve_kbo(struct lisuan_device *ls_dev, struct lisuan_kbo_mman *kbo);
void lisuan_free_kbo(struct lisuan_device *ls_dev, struct lisuan_kbo_mman *kbo);

int lisuan_alloc_kbo_node(struct lisuan_kbo_mman *kbo, struct lisuan_kbo_node *kbo_node);
void lisuan_free_kbo_node(struct lisuan_kbo_node *kbo_node);

int lisuan_vm_bo_update_mapping(struct lisuan_device *ls_dev, struct lisuan_vm_update_params *params);
int lisuan_alloc_cf_node(struct lisuan_device *ls_dev, struct lisuan_cf_node *cf_node);
void lisuan_free_cf_node(struct lisuan_cf_node *cf_node);
int lisuan_init_cf_pool(struct lisuan_device *ls_dev);
int lisuan_fini_cf_pool(struct lisuan_device *ls_dev);
void gf_mm_print(const struct drm_mm *mm, unsigned long long *used, unsigned long long *free,  unsigned long long *total);

#endif
