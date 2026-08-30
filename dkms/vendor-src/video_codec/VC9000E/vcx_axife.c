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

#include "vcx_axife.h"
#include "gf_os_interface.h"

/* mode description
 * 1: OYB normal(enable)
 * 2: bypass
 */
u32 AXIFEEnable(u8 *hwregs, u32 mode)
{
#ifndef HANTROVCMD_ENABLE_IP_SUPPORT
	if (!hwregs)
		return -1;

	//AXI FE pass through
	if (mode == 1) {
		gf_write_mmio32((hwregs + AXI_REG10_SW_FRONTEND_EN), 0x02);
		gf_write_mmio32((hwregs + AXI_REG11_SW_WORK_MODE), 0x2);
	} else if (mode == 2) {
		gf_write_mmio32((hwregs + AXI_REG10_SW_FRONTEND_EN), 0x02);
		gf_write_mmio32((hwregs + AXI_REG11_SW_WORK_MODE), 0x40);
	}
	gf_debug("%s: axife_reg10_addr=0x%p, *axife_reg10_addr=0x%08x\n",
		__func__, hwregs + 10 * 4, gf_read_mmio32((hwregs + 10 * 4)));
	gf_debug("%s: axife_reg11_addr=0x%p, *axife_reg11_addr=0x%08x\n",
		__func__, hwregs + 11 * 4, gf_read_mmio32((hwregs + 11 * 4)));
#endif
	return 0;
}

int AXIFEFlush(u8 *hwregs)
{
#ifndef HANTROVCMD_ENABLE_IP_SUPPORT
	int loop_cnt = 0;

	if (!hwregs)
		return -1;

	/* trigger AXI FE flush, AXI FE will automatically read or empty data in its
	 * Master side until the Master status is IDLE.
	 */
	gf_write_mmio32((hwregs + AXI_REG35_SW_FLUSH_EN),0x01);
	gf_debug("%s: axife_reg35_addr=0x%p, *axife_reg35_addr=0x%08x\n",
		__func__, hwregs + 35 * 4, gf_read_mmio32((hwregs + 35 * 4)));

	//polling read flush status(swreg[0]). If it is set to 1, means flush is completed.
	while (!(gf_read_mmio32(((hwregs + AXI_REG0_SW_HWCFG)))) >> 31) {
		loop_cnt++;
		mdelay(10); // wait 10ms
		if (loop_cnt > 20) { // too long
			pr_err("AXI FE: too long before axife flushed successfully\n");
			return -1;
		}
	}
#endif
	return 0;
}
