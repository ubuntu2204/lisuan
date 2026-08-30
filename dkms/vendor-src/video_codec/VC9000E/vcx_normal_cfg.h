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

#ifndef __VCX_NORMAL_CFG_H__
#define __VCX_NORMAL_CFG_H__

/* Configure information without CMD, fill according to System Memory Map*/

#define RESOURCE_SHARED_INTER_SUBSYS        0 /*0:no resource sharing inter subsystems 1: existing resource sharing*/
/* Sub-System 0 */
#ifdef EMU
#define SUBSYS_0_IO_ADDR                (0x6020000)   /*customer specify according to own platform*/
#else
#define SUBSYS_0_IO_ADDR                (0x90000)   /*customer specify according to own platform*/
#endif
#define SUBSYS_0_IO_SIZE                (0x8000)   /* bytes */
#define INT_PIN_SUBSYS_0_VCE        (-1)
#define INT_PIN_SUBSYS_0_DEC400         (-1)
#define INT_PIN_SUBSYS_0_UFBC         (-1)
#define REG_OFFSET_SUBSYS_0_VCE     (0x1000)
#define REG_OFFSET_SUBSYS_0_DEC400      (0x2000)
#define REG_OFFSET_SUBSYS_0_UFBC      (0x1C00)

/* Sub-System 1 */
#ifdef EMU
#define SUBSYS_1_IO_ADDR                (0x6000000)   /*customer specify according to own platform*/
#else
#define SUBSYS_1_IO_ADDR                (0xA0000)
#endif
#define SUBSYS_1_IO_SIZE                (10000 * 4)
#define INT_PIN_SUBSYS_1_CUTREE         (-1)
#define REG_OFFSET_SUBSYS_1_CUTREE      (0x1000)

/* Subsystem configure
 * base_addr, iosize, resource_shared
 */
static SUBSYS_CONFIG subsys_array[] = {
	{ 0x60000, 0x8000, RESOURCE_SHARED_INTER_SUBSYS },
	{ 0x62000, 0x8000, RESOURCE_SHARED_INTER_SUBSYS},
};

/* Core configure
 * slice_idx, core_type, offset, reg_size, irq
 */
static CORE_CONFIG core_array[] = {
	{ 0, CORE_VCE,   0x0800, ENCODER_REGISTER_SIZE * 4, -1},
	{ 0, CORE_AXIFE, 0x1000, 0x800,                     -1},
	{ 1, CORE_VCE,   0x0800, ENCODER_REGISTER_SIZE * 4, -1},
	{ 1, CORE_AXIFE, 0x1000, 0x800,                     -1},
};

#endif

// BELOW PORT FROM vcx_vcmd_driver.c
// KEEP AS REFERENCE

#if 0
#define RESOURCE_SHARED_INTER_SUBSYS        0     /*0:no resource sharing inter subsystems 1: existing resource sharing*/
#define SUBSYS_0_IO_ADDR                 0x90000   /*customer specify according to own platform*/
#define SUBSYS_0_IO_SIZE                 (10000 * 4)    /* bytes */

#define SUBSYS_1_IO_ADDR                 0xA0000   /*customer specify according to own platform*/
#define SUBSYS_1_IO_SIZE                 (10000 * 4)    /* bytes */

#define INT_PIN_SUBSYS_0_VCE                    -1
#define INT_PIN_SUBSYS_0_CUTREE                     -1
#define INT_PIN_SUBSYS_0_DEC400                     -1
#define INT_PIN_SUBSYS_0_L2CACHE                    -1
#define INT_PIN_SUBSYS_1_VCE                    -1
#define INT_PIN_SUBSYS_1_CUTREE                     -1
#define INT_PIN_SUBSYS_1_DEC400                     -1

/*for all subsystem, the subsys info should be listed here for subsequent use*/
/*base_addr, iosize, resource_shared*/
SUBSYS_CONFIG subsys_array[] = {
	{SUBSYS_0_IO_ADDR, SUBSYS_0_IO_SIZE, RESOURCE_SHARED_INTER_SUBSYS}, //subsys_0
	//{SUBSYS_1_IO_ADDR, SUBSYS_1_IO_SIZE, RESOURCE_SHARED_INTER_SUBSYS}, //subsys_1
};

/*here config every core in all subsystem*/
/*NOTE: no matter what format(HEVC/H264/JPEG/AV1/...) is supported in VCE, just use [CORE_VCE] to indicate it's a VCE core*/
/*      CUTREE can work standalone, so it can be a subsytem or just one core of a subsytem.*/
/*subsys_idx, core_type, offset, reg_size, irq*/
CORE_CONFIG core_array[] = {
	{0, CORE_VCE, 0x1000, ENCODER_REGISTER_SIZE * 4, INT_PIN_SUBSYS_0_VCE}, //subsys_0_VCE
	//{0, CORE_MMU, 0x2000, DEC400_REGISTER_SIZE * 4, INT_PIN_SUBSYS_0_VCE}, //subsys_0_VCE
	//{0, CORE_AXIFE, 0x3000, AXIFE_REGISTER_SIZE * 4, INT_PIN_SUBSYS_0_VCE}, //subsys_0_AXIFE
	//{0, CORE_MMU_1, 0x4000, MMU_REGISTER_SIZE * 4, INT_PIN_SUBSYS_0_VCE}, //subsys_0_VCE
	//{0, CORE_AXIFE_1, 0x5000, AXIFE_REGISTER_SIZE * 4, INT_PIN_SUBSYS_0_VCE}, //subsys_0_AXIFE_1
	//{0, CORE_DEC400, 0x6000, DEC400_REGISTER_SIZE * 4, INT_PIN_SUBSYS_0_VCE}, //subsys_0_DEC400
	//{0, CORE_L2CACHE, 0xc000, L2CACHE_REGISTER_SIZE * 4, INT_PIN_SUBSYS_0_L2CACHE}, //subsys_0_l2cache
	//{0, CORE_CUTREE, 0xd000, IM_REGISTER_SIZE * 4, INT_PIN_SUBSYS_0_CUTREE}, //subsys_0_CUTREE

	//{1, CORE_CUTREE, 0x1000, IM_REGISTER_SIZE * 4, INT_PIN_SUBSYS_1_CUTREE}, //CUTREE
	//{1, CORE_MMU, 0x2000, MMU_REGISTER_SIZE * 4, INT_PIN_SUBSYS_0_CUTREE}, //subsys_1_MMU
	//{1, CORE_AXIFE, 0x3000, AXIFE_REGISTER_SIZE * 4, INT_PIN_SUBSYS_0_CUTREE}, //subsys_1_AXIFE

};
#endif
// END KEEP AS REFERENCE
