/*
 * Copyright (c) 2015, Verisilicon Inc. - All Rights Reserved
 * Copyright (c) 2011-2014, Google Inc. - All Rights Reserved
 *
 * This software is dual licensed, either GPL-2.0 or BSD-3-Clause, at your
 * option.
 *
 ********************************************************************************
 *
 * GPL-2.0
 *
 ********************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
 * Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 ********************************************************************************
 *
 * Alternatively, This software may be distributed under the terms of
 * BSD-3-Clause, in which case the following provisions apply instead of the ones
 * mentioned above :
 *
 ********************************************************************************
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ********************************************************************************
 */

#ifndef __VCX_VCMD_CFG_H__
#define __VCX_VCMD_CFG_H__

#include "vcx_axife.h"

/* submodule config */
struct sub_mod_cfg {
	enum subsys_module_id sub_mod_id;

	u16 io_off; // submodule reg base (offset to vcmd reg-base)
	u16 io_size;   // submodule io size

	u16 rreg_id;  // start reg-id to read out when init driver,
				   // 0xffff means not need to read.
	u16 rreg_num;  // number of registers to read out when init driver
};

/*for all vcmds, the config info should be listed here for subsequent use*/
struct vcmd_config {
	unsigned long vcmd_base_addr;	//vcmd reg_base (bus address)
	int vcmd_irq;
	u32 sub_module_type; /*input vce=0,IM=1,vcd=2，jpege=3, jpegd=4*/
	u32 priority; //the priority of vcmd
	struct sub_mod_cfg submodule_cfg[SUB_MOD_MAX];
};

static struct vcmd_config vcmd_core_array[] = {
	/* subsys 0 */
	{
		0x60000, -1, VCMD_TYPE_ENCODER, 0,
		{ 
			{SUB_MOD_VCMD, 0, ASIC_VCMD_SWREG_AMOUNT * 4, 0xffff, 0},
			{SUB_MOD_MAIN, 0x0800, ASIC_SWREG_AMOUNT * 4, 0, ASIC_SWREG_AMOUNT},
			{SUB_MOD_AXIFE0, 0x1000, AXIFE_REG_SIZE, 0, 1},
			{SUB_MOD_L2CACHE, 0xffff, L2CACHE_REGISTER_SIZE, 0, 1},
			{SUB_MOD_MMU0, 0xffff, 0, 0xffff, 0},
			{SUB_MOD_MMU1, 0xffff, 0, 0xffff, 0},
			{SUB_MOD_DEC400, 0xffff, DEC400_REGISTER_SIZE, 0, 0x2b},
			{SUB_MOD_AXIFE1, 0xffff, AXIFE_REG_SIZE, 0, 1},
			{SUB_MOD_UFBC, 0xffff, UFBC_REGISTER_SIZE, 0, 10},
		},
	},
	/* subsys 1 */
	{
		0x62000, -1, VCMD_TYPE_CUTREE, 0,
		{
			{SUB_MOD_VCMD, 0, ASIC_VCMD_SWREG_AMOUNT * 4, 0xffff, 0},
			{SUB_MOD_MAIN, 0x0800, ASIC_SWREG_AMOUNT * 4, 0, ASIC_SWREG_AMOUNT},
			{SUB_MOD_AXIFE0, 0x1000, AXIFE_REG_SIZE, 0, 1},
			{SUB_MOD_L2CACHE, 0xffff, L2CACHE_REGISTER_SIZE, 0, 1},
			{SUB_MOD_MMU0, 0xffff, 0, 0xffff, 0},
			{SUB_MOD_MMU1, 0xffff, 0, 0xffff, 0},
			{SUB_MOD_DEC400, 0xffff, DEC400_REGISTER_SIZE, 0, 0x2b},
			{SUB_MOD_AXIFE1, 0xffff, AXIFE_REG_SIZE, 0, 1},
			{SUB_MOD_UFBC, 0xffff, UFBC_REGISTER_SIZE, 0, 10},
		},
	},
};

#endif /*__VCX_VCMD_CFG_H__ */
