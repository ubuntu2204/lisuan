// SPDX-License-Identifier: GPL-2.0
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

#include "subsys.h"

/* subsystem configuration */
#include "subsys_cfg.h"
//extern struct vcmd_config vcmd_core_array[MAX_SUBSYS_NUM];
//extern u32 total_vcmd_core_num;
//extern unsigned long multicorebase[];
//extern int irq[];
//extern unsigned int iosize[];
//extern int reg_count[];

#include "vc9000_common.h"
/*
 * Convert subsys_array & core_array to subsys_config array
 */
void CheckSubsysCoreArray(struct subsys_config *subsys, int *subsys_num, int *vcmd)
{
	int num = ARRAY_SIZE(subsys_array);
	int i, j;

	gf_memset(subsys, 0, sizeof(subsys[0]) * MAX_SUBSYS_NUM);
	for (i = 0; i < MAX_SUBSYS_NUM; i++) {
		subsys[i].base_addr = (i < num) ? subsys_array[i].base : 0;
		subsys[i].irq = -1;
		for (j = 0; j < HW_CORE_MAX; j++) {
			subsys[i].submodule_offset[j] = 0xffff;
			subsys[i].submodule_iosize[j] = 0;
			subsys[i].submodule_hwregs[j] = NULL;
		}
	}

	for (i = 0; i < ARRAY_SIZE(core_array); i++) {
		if (!subsys[core_array[i].subsys].base_addr) {
			/* undefined subsystem */
			continue;
		}
		if (core_array[i].core_type == HW_VCDJ)
			core_array[i].core_type = HW_VCD;
		subsys[core_array[i].subsys].submodule_offset[core_array[i].core_type] =
			core_array[i].offset;
		subsys[core_array[i].subsys].submodule_iosize[core_array[i].core_type] =
			core_array[i].iosize;
		if (subsys[core_array[i].subsys].irq != -1 &&
		    core_array[i].irq != -1) {
			if (subsys[core_array[i].subsys].irq !=
			    core_array[i].irq) {
				gf_debug("hantrodec: hw core type %d irq %d != subsystem irq %d\n",
					core_array[i].core_type,
				       core_array[i].irq,
				       subsys[core_array[i].subsys].irq);
				gf_debug("hantrodec: hw cores ofa subsystem should have same irq\n");
			}
		} else if (core_array[i].irq != -1) {
			subsys[core_array[i].subsys].irq = core_array[i].irq;
		}
		subsys[core_array[i].subsys].has_apbfilter[core_array[i].core_type] =
			core_array[i].has_apb;
		/* vcmd found */
		if (core_array[i].core_type == HW_VCMD)
			*vcmd = 1;
		else if (core_array[i].core_type == HW_VCD)
			subsys[core_array[i].subsys].subsys_type = DECODER_TYPE; /* vcd */
	}

	gf_debug("hantrodec: vcmd = %d\n", *vcmd);

	*subsys_num = num;
}
