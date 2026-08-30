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

#ifndef _VC8000_VCMD_DRIVER_H_
#define _VC8000_VCMD_DRIVER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "vc9000_common.h"

//#ifdef __linux__
#undef addr_t
#define addr_t ADDR_T_VCMD
typedef size_t addr_t;
//#endif

/*priority support*/




#if 1

#define VC8000E_FRAME_RDY_INT_MASK                        0x0001
#define VC8000E_CUTREE_RDY_INT_MASK                       0x0002
#define VC8000E_DEC400_INT_MASK                           0x0004
#define VC8000E_L2CACHE_INT_MASK                          0x0008
#define VC8000E_MMU_INT_MASK                              0x0010
#endif

#define ANY_CMDBUF_ID 0xFFFF

#define VCMD_ARBITER_WEIGHT 0x1D
#define VCMD_ARBITER_URGENT 0x0
#define VCMD_ARBITER_ENABLE 0x1
//#define VCMD_ARBITER_PARAMS (VCMD_ARBITER_WEIGHT << 16 | VCMD_ARBITER_URGENT << 9 | VCMD_ARBITER_ENABLE << 8 | VCMD_ARBITER_TIME_WINDOW_EXP)
// set reg28[20:16]=31 reg28[4:0]=31 to avoid overflow issue
#define VCMD_ARBITER_PARAMS (31 << 16 | VCMD_ARBITER_URGENT << 9 | VCMD_ARBITER_ENABLE << 8 | 31)

struct cmdbuf_mem_parameter {
	addr_t virt_cmdbuf_addr;
	//cmdbuf pool base physical address
	addr_t phy_cmdbuf_addr;
	//cmdbuf pool base mmu mapping address
	unsigned int mmu_phy_cmdbuf_addr;
	//cmdbuf pool total size in bytes.
	unsigned int cmdbuf_total_size;
	//one cmdbuf size in bytes. all cmdbuf have same size.
	unsigned short cmdbuf_unit_size;
	addr_t virt_status_cmdbuf_addr;
	//status cmdbuf pool base physical address
	addr_t phy_status_cmdbuf_addr;
	//status cmdbuf pool base mmu mapping address
	unsigned int mmu_phy_status_cmdbuf_addr;
	//status cmdbuf pool total size in bytes.
	unsigned int status_cmdbuf_total_size;
	//one status cmdbuf size in bytes. all status cmdbuf have same size.
	unsigned short status_cmdbuf_unit_size;
	/* for pcie interface, hw can only access
	 * phy_cmdbuf_addr-pcie_base_ddr_addr.
	 * for other interface, this value should be 0?
	 */
	addr_t base_ddr_addr;
};

struct config_parameter {
	/*input vce=0,cutree=1,vcd=2,jpege=3, jpegd=4 */
	unsigned short module_type;
	/* output, how many vcmd cores are there
	 * with corresponding module_type.
	 */
	unsigned short vcmd_core_num;
	/*output,if submodule addr == 0xffff, this submodule does not exist.*/
	unsigned short submodule_main_addr;
	/* output ,if submodule addr == 0xffff, this submodule does not exist.*/
	unsigned short submodule_dec400_addr;
	/* output,if submodule addr == 0xffff, this submodule does not exist. */
	unsigned short submodule_MMU_addr;
	/* output,if submodule addr == 0xffff, this submodule does not exist. */
	unsigned short submodule_MMUWrite_addr;
	/* output,if submodule addr == 0xffff, this submodule does not exist. */
	unsigned short submodule_axife_addr;
	/* output , this status comdbuf save the all register values read
	 * in driver init.//used for analyse configuration in cwl.
	 */
	unsigned short config_status_cmdbuf_id;
	unsigned int vcmd_hw_version_id;
};

/*need to consider how many memory should be allocated for status.*/
struct exchange_parameter {
	//input ;executing_time=encoded_image_size*(rdoLevel+1)*(rdoq+1);
	unsigned long long executing_time;
	/*input input vce=0,IM=1,vcd=2, jpege=3, jpegd=4 */
	unsigned short module_type;
	/*input, reserve is not used; link and run is input.*/
	unsigned short cmdbuf_size;
	/* input,normal=0, high/live=1 */
	unsigned short priority;
	/* output, it is unique in driver.*/
	unsigned short cmdbuf_id;
	/* just used for polling. */
	unsigned short core_id;
	/* core_mask for user to select cores: [0,15]core mask, [16,31]client type. */
	unsigned short core_mask;
};

#ifdef __cplusplus
}
#endif

#endif /* !_VC8000_VCMD_DRIVER_H_ */
