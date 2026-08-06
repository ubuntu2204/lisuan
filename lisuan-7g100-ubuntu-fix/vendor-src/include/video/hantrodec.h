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

#ifndef _HANTRODEC_H_
#define _HANTRODEC_H_

#ifdef __cplusplus
extern "C" {
#endif

enum CoreType {
	/* Decoder */
	HW_VCD = 0,
	HW_VCDJ,
	HW_BIGOCEAN,
	HW_VCMD,
	HW_MMU, //if set HW_MMU_WR, then HW_MMU means HW_MMU_RD
	HW_MMU_WR,
	HW_DEC400,
	HW_L2CACHE,
	HW_SHAPER,
	/* Encoder*/
	/* Auxiliary IPs */
	HW_AXIFE,
	HW_AFBC,
	HW_CORE_MAX /* max number of cores supported */
};

struct core_desc {
	unsigned int id; /* id of the subsystem */
	unsigned int type; /* type of core to be written */
	unsigned int *regs; /* pointer to user registers */
	unsigned int size; /* size of register space */
	unsigned int reg_id; /* id of reigster to be read/written */
};

struct regsize_desc {
	unsigned int slice; /* id of the slice */
	unsigned int id; /* id of the subsystem */
	unsigned int type; /* type of core to be written */
	unsigned int size; /* iosize of the core */
};

struct core_param {
	unsigned int slice; /* id of the slice */
	unsigned int id; /* id of the subsystem */
	unsigned int type; /* type of core to be written */
	unsigned int size; /* iosize of the core */
	unsigned int asic_id; /* asic id of the core */
};

struct subsys_desc {
	unsigned int subsys_num; /* total subsystems count */
	unsigned int subsys_vcmd_num; /* subsystems with vcmd */
};

struct axife_cfg {
	unsigned char axi_rd_chn_num;
	unsigned char axi_wr_chn_num;
	unsigned char axi_rd_burst_length;
	unsigned char axi_wr_burst_length;
	unsigned char fe_mode;
	unsigned int id;
};

struct apbfilter_cfg {
	unsigned int nbr_mask_regs;
	unsigned int mask_reg_offset;
	unsigned int page_sel_addr;
	unsigned char num_mode;
	unsigned char mask_bits_per_reg;
	unsigned int id; /* id of the subsystem */
	unsigned int type; /* type of core to be written */
	unsigned int has_apbfilter;
};

#ifdef __cplusplus
}
#endif

#endif /* !_HANTRODEC_H_ */
