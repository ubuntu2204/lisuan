/* SPDX-License-Identifier: GPL-2.0 */
/****************************************************************************
 *
 *    The MIT License (MIT)
 *
 *    Copyright (C) 2020  VeriSilicon Microelectronics Co., Ltd.
 *
 *    Permission is hereby granted, free of charge, to any person obtaining a
 *    copy of this software and associated documentation files (the "Software"),
 *    to deal in the Software without restriction, including without limitation
 *    the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *    and/or sell copies of the Software, and to permit persons to whom the
 *    Software is furnished to do so, subject to the following conditions:
 *
 *    The above copyright notice and this permission notice shall be included in
 *    all copies or substantial portions of the Software.
 *
 *    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 *    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *    DEALINGS IN THE SOFTWARE.
 *
 *****************************************************************************
 *
 *    The GPL License (GPL)
 *
 *    Copyright (C) 2020  VeriSilicon Microelectronics Co., Ltd.
 *
 *    This program is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU General Public License
 *    as published by the Free Software Foundation; either version 2
 *    of the License, or (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software Foundation,
 *    Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *****************************************************************************
 *
 *    Note: This software is released under dual MIT and GPL licenses. A
 *    recipient may use this file under the terms of either the MIT license or
 *    GPL License. If you wish to use only one license not the other, you can
 *    indicate your decision by deleting one of the above license notices
 *    in your version of this file.
 *
 *****************************************************************************
 */

#ifndef _SUBSYS_H_
#define _SUBSYS_H_

#ifdef __FREERTOS__
#define BIT(a) (1 << (a))
#elif defined(__linux__)
#include <linux/fs.h>
#include <linux/idr.h>
#include <drm/drm_file.h>
#endif
#include "hantrodec.h"
#include "lisuan_device.h"

/******************************************************************************
 * VCMD Command Definitions
 ******************************************************************************/
#define UNSIGNED_OPCODE(A)		  ((u32)(A))
#define OPCODE_MASK				  (UNSIGNED_OPCODE(0x1F) << 27)
#define OPCODE_WREG               (UNSIGNED_OPCODE(0x01) << 27)
#define OPCODE_END                (UNSIGNED_OPCODE(0x02) << 27)
#define OPCODE_NOP                (UNSIGNED_OPCODE(0x03) << 27)
#define OPCODE_RREG               (UNSIGNED_OPCODE(0x16) << 27)
#define OPCODE_INT                (UNSIGNED_OPCODE(0x18) << 27)
#define OPCODE_JMP                (UNSIGNED_OPCODE(0x19) << 27)
#define OPCODE_STALL              (UNSIGNED_OPCODE(0x09) << 27)
#define OPCODE_CLRINT             (UNSIGNED_OPCODE(0x1a) << 27)
#define OPCODE_JMP_RDY0           (UNSIGNED_OPCODE(0x19) << 27)
#define OPCODE_JMP_RDY1           ((UNSIGNED_OPCODE(0x19) << 27) | (1 << 26))
#define OPCODE_M2M                (UNSIGNED_OPCODE(0x1b) << 27)
#define OPCODE_MSET               (UNSIGNED_OPCODE(0x1c) << 27)
#define OPCODE_M2MP               (UNSIGNED_OPCODE(0x1d) << 27)
#define JMP_IE_1                  BIT(25)
#define JMP_RDY_1                 BIT(26)

/* Functions provided by all other subsystem IP - hantrodec_xxx.c */

/***********************************************/
/* subsys level */
/***********************************************/

#define HXDEC_MAX_CORES MAX_SUBSYS_NUM /* used in hantro_dec_xxx.c */
#define MAX_REG_COUNT 768

enum SubsysType {
	ENCODER_TYPE = 0,
	CUTREE_TYPE,
	DECODER_TYPE,
	JPEG_ENCODER_TYPE,
	MAX_TYPE
};

/* SubsysDesc & CoreDesc are used for configuration */
struct SubsysDesc {
	int slice_index; /* slice this subsys belongs to */
	int index; /* subsystem index */
	long long base;
};

struct CoreDesc {
	int slice;
	int subsys; /* subsys this core belongs to */
	enum CoreType core_type;
	int offset; /* offset to subsystem base */
	int iosize;
	int irq;
	int has_apb;
};

/* internal config struct (translated from SubsysDesc & CoreDesc) */
struct subsys_config {
	unsigned long long base_addr;
	int irq;
	/* identifier for each subsys vc8000e=0,
	 * IM=1,vcd=2,jpege=3,jpegd=4
	 */
	u32 subsys_type;
	u32 submodule_offset[HW_CORE_MAX]; /* in bytes */
	u16 submodule_iosize[HW_CORE_MAX]; /* in bytes */

	u8 *submodule_hwregs[HW_CORE_MAX]; /* virtual address */
	int has_apbfilter[HW_CORE_MAX];
};

void CheckSubsysCoreArray(struct subsys_config *subsys, int *subsys_num, int *vcmd);

/* Used in vcmd initialization in hantro_vcmd_xxx.c. */
/* May be unified in next step. */
struct vcmd_config {
	unsigned long long vcmd_base_addr;
	u32 vcmd_iosize;
	int vcmd_irq;
	/*input vce=0,IM=1,vcd=2,jpege=3, jpegd=4*/
	u32 sub_module_type;
	u16 submodule_main_addr; // in byte
	/* if submodule addr == 0xffff,
	 * this submodule does not exist.// in byte
	 */
	u16 submodule_dec400_addr;
	u16 submodule_MMU_addr; // in byte
	u16 submodule_MMUWrite_addr; // in byte
	u16 submodule_axife_addr; // in byte

	/* for Hw Register Print */
	volatile u8  *submodule_vcd_virtual_address;
	volatile u8 *submodule_dec400_virtual_address;
	volatile u8 *submodule_MMU_virtual_address;
	volatile u8 *submodule_MMUWrite_virtual_address;
	volatile u8 *submodule_axife_virtual_address;
	u32 submodule_vcd_iosize;
	u32 submodule_dec400_iosize;
	u32 submodule_MMU_iosize;
	u32 submodule_MMUWrite_iosize;
	u32 submodule_axife_iosize;

};

extern unsigned int has_apb_arbiter;
extern char *dec_dev_n;

#ifdef __FREERTOS__
/* nothing */
#elif defined(__linux__)
int hantrodec_init(struct lisuan_device *gf_device);
int hantrodec_open(struct lisuan_device *gf_dev, struct drm_file *file);
void hantrodec_cleanup(struct lisuan_device *gf_dev);
int hantrodec_release(struct drm_device *ddev, struct drm_file *filp);
int hantrodec_vcmd_open(struct lisuan_device *gf_dev, struct drm_file *file);
int hantrodec_vcmd_release(struct lisuan_device *gf_dev, struct drm_file *file);
int hantrodec_isr(struct lisuan_device *gf_dev);

int xx_HANTRODEC_IOCGHWOFFSET(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);
int xx_HANTRODEC_IOCGHWIOSIZE(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);
int xx_HANTRODEC_IOC_MC_OFFSETS(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);
int xx_HANTRODEC_IOC_MC_CORES(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);
int xx_HANTRODEC_IOCS_DEC_PUSH_REG(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);
int xx_HANTRODEC_IOCS_DEC_WRITE_REG(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);
int xx_HANTRODEC_IOCS_DEC_WRITE_APBFILTER_REG(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);
int xx_HANTRODEC_IOCS_DEC_PULL_REG(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);
int xx_HANTRODEC_IOCS_DEC_READ_REG(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);
int xx_HANTRODEC_IOCH_DEC_RESERVE(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);
int xx_HANTRODEC_IOCT_DEC_RELEASE(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);
int xx_HANTRODEC_IOCX_DEC_WAIT(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);
int xx_HANTRODEC_IOCG_CORE_WAIT(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);
int xx_HANTRODEC_IOX_ASIC_ID(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);
int xx_HANTRODEC_IOCG_CORE_ID(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);
int xx_HANTRODEC_IOX_ASIC_BUILD_ID(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);
int xx_HANTRODEC_IOX_SUBSYS(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);
int xx_HANTRODEC_IOCX_POLL(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);
int xx_HANTRODEC_IOC_APBFILTER_CONFIG(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);
int xx_HANTRODEC_IOC_AXIFE_CONFIG(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);

int xx_HANTRO_VCMD_IOCH_GET_CMDBUF_PARAMETER(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);
int xx_HANTRO_VCMD_IOCH_GET_VCMD_PARAMETER(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);
int xx_HANTRO_VCMD_IOCH_RESERVE_CMDBUF(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);
int xx_HANTRO_VCMD_IOCH_LINK_RUN_CMDBUF(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);
int xx_HANTRO_VCMD_IOCH_WAIT_CMDBUF(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);
int xx_HANTRO_VCMD_IOCH_PUSH_SLICE_REG(struct drm_device *dev, void *data,
		       struct drm_file *file_priv); 
int xx_HANTRO_VCMD_IOCH_ACQUIRE_MC_TOKEN(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);
int xx_HANTRO_VCMD_IOCH_RELEASE_CMDBUF(struct drm_device *dev, void *data,
		       struct drm_file *file_priv); 
int xx_HANTRO_VCMD_IOCH_POLLING_CMDBUF(struct drm_device *dev, void *data,
		       struct drm_file *file_priv); 

void *hantrodec_vcmd_init(struct lisuan_device *gf_device, struct subsys_config *subsys, int subsys_num, void *platformdev);
void hantrovcmd_cleanup(struct lisuan_device *gf_dev, void *_vcmd_mgr);
int hantrovcmd_isr(int irq, void *dev_id, int is_internal);
void lisuan_dec_vcmd_clear_irq_status(int irq, void *dev_id);
void lisuan_dec0_vcmd_worker(struct work_struct *work);
void lisuan_dec1_vcmd_worker(struct work_struct *work);
void hantro_dec_resume(struct lisuan_device *gf_device);
void vcmd_reset_asic(void *vcmd_mgr);
void* hantro_vcmd_resume(struct lisuan_device *gf_device, struct subsys_config *subsys);
int gf_dec_link_and_run(struct drm_device *dev, void *data, struct drm_file *file_priv, const unsigned long long fence, void* cb_fence, int procid);

/******************************************************************************/
/* DEC400 */
/******************************************************************************/

/******************************************************************************/
/* AXI FE */
/******************************************************************************/
#endif

#endif
