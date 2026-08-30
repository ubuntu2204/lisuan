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

#ifndef _VCX_VCMD_DRIVER_H_
#define _VCX_VCMD_DRIVER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "vc9000_common.h"

#ifdef __linux__
#undef ptr_t
#define ptr_t PTR_T_KERNEL
typedef size_t ptr_t;
typedef size_t addr_t;
#endif

#define ENC_HW_ID1                  0x48320100
#define ENC_HW_ID2                  0x80006000
#define CORE_INFO_MODE_OFFSET       31
#define CORE_INFO_AMOUNT_OFFSET     28

/*
 * S means "Set" through a ptr,
 * T means "Tell" directly with the argument value
 * G means "Get": reply by setting through a pointer
 * Q means "Query": response is on the return value
 * X means "eXchange": G and S atomically
 * H means "sHift": T and Q atomically
 */

#define GET_ENCODER_IDX(type_info)                    (CORE_VCE)
#define CORETYPE(core)                                (1 << (core))
#define HANTRO_IOC_MAXNR                              60

/*priority support*/


#define MAX_VCMD_CORE_NUM 4





#define VCMD_ARBITER_BANDWIDTH0 0x2
#define ENC_VCMD_ARBITER_PARAMS (VCMD_ARBITER_TIME_WINDOW_EXP << 4 | VCMD_ARBITER_BANDWIDTH0)

#define VCE_FRAME_RDY_INT_MASK                        0x0001
#define VCE_CUTREE_RDY_INT_MASK                       0x0002
#define VCE_DEC400_INT_MASK                           0x0004
#define VCE_L2CACHE_INT_MASK                          0x0008
#define VCE_MMU_INT_MASK                              0x0010
#define CUTREE_MMU_INT_MASK                               0x0020



#define VCD_DEC400_INT_MASK_1_1_1                      0x0200
#define VCD_L2CACHE_INT_MASK_1_1_1                     0x0400
#define VCD_MMU_INT_MASK_1_1_1                         0x0800



#define VCMD_SLICE_RDY_INTERRUPT 510
#define VCMD_SLICE_RDY_NUM       511

/* time to wotchdog wait for one job */
#define ONE_JOB_WAIT_TIME                        (500 * 100)	//For FPGA platform
//#define ONE_JOB_WAIT_TIME                        (500)	//For SoC platform

#ifndef ASIC_SWREG_AMOUNT
#define ASIC_SWREG_AMOUNT                  512 //from encswhwregister.h
#endif

/*these size need to be modified according to hw config.*/
#define ENCODER_REGISTER_SIZE              ASIC_SWREG_AMOUNT
#define DECODER_REGISTER_SIZE              512
#define IM_REGISTER_SIZE                   ASIC_SWREG_AMOUNT
#define JPEG_ENCODER_REGISTER_SIZE         ASIC_SWREG_AMOUNT
#define JPEG_DECODER_REGISTER_SIZE         512

#define DEC400_REGISTER_SIZE               1600
#define MMU_REGISTER_SIZE                  500
#define L2CACHE_REGISTER_SIZE              500
#define AXIFE_REGISTER_SIZE                500
#define UFBC_REGISTER_SIZE                 10
#define SLCIE_ADDR_OFFSET                  0x100

#define ASIC_STATUS_SEGMENT_READY       0x1000
#define ASIC_STATUS_FUSE_ERROR          0x200
#define ASIC_STATUS_SLICE_READY         0x100
#define ASIC_STATUS_LINE_BUFFER_DONE    0x080  /* low latency */
#define ASIC_STATUS_POLL_SLICEINFO_TIMEOUT 0x2000
#ifdef LOW_LATENCY_SLICEINFO_SUPPORT
#define SLICEINFO_ADDR_OFFSET              0x200
#endif

#define ASIC_STATUS_UFBC_DEC_ERR 0x4000  //bit14

#define ASIC_STATUS_HW_TIMEOUT 0x040

#define ASIC_STATUS_BUFF_FULL           0x020
#define ASIC_STATUS_HW_RESET            0x010
#define ASIC_STATUS_ERROR               0x008
#define ASIC_STATUS_FRAME_READY         0x004
#define ASIC_IRQ_LINE                   0x001
#define ASIC_STATUS_ALL       (ASIC_STATUS_SEGMENT_READY |\
			       ASIC_STATUS_FUSE_ERROR |\
			       ASIC_STATUS_SLICE_READY |\
			       ASIC_STATUS_LINE_BUFFER_DONE |\
			       ASIC_STATUS_HW_TIMEOUT |\
			       ASIC_STATUS_BUFF_FULL |\
			       ASIC_STATUS_HW_RESET |\
			       ASIC_STATUS_ERROR |\
			       ASIC_STATUS_FRAME_READY |\
				   ASIC_STATUS_UFBC_DEC_ERR |\
				   ASIC_STATUS_POLL_SLICEINFO_TIMEOUT)

enum {
	CORE_VCE = 0,
	CORE_VCEJ = 1,
	CORE_CUTREE = 2,
	CORE_DEC400 = 3,
	CORE_MMU = 4,
	CORE_L2CACHE = 5,
	CORE_AXIFE = 6,
	CORE_APBFT = 7,
	CORE_MMU_1 = 8,
	CORE_AXIFE_1 = 9,
	CORE_UFBC = 10,
	CORE_MAX
};

//#define CORE_MAX  (CORE_MMU)



struct enc_cmdbuf_mem_parameter {
	unsigned long long cmd_virt_addr; //cmdbuf pool base virtual address
	unsigned long long cmd_phy_addr; //cmdbuf pool base physical address, it's for cpu
	unsigned long long cmd_hw_addr; //cmdbuf pool base hardware address, it's for hardware ip
	unsigned int cmd_total_size; //cmdbuf pool total size in bytes.
	unsigned short cmd_unit_size; //one cmdbuf size in bytes. all cmdbuf have same size.
	unsigned long long status_virt_addr;
	unsigned long long status_phy_addr; //status cmdbuf pool base physical address, it's for cpu
	unsigned long long status_hw_addr; //status cmdbuf pool base hardware address, it's for hardware ip
	unsigned int status_total_size; //status cmdbuf pool total size in bytes.
	unsigned short status_unit_size; //one status cmdbuf size in bytes. all status cmdbuf have same size.
	unsigned long long base_ddr_addr; //for pcie interface, hw can only access phy_cmdbuf_addr-pcie_base_ddr_addr.
		//for other interface, this value should be 0?
	unsigned long long reg_virt_addr; //register cmdbuf pool base virtual address
	unsigned long long reg_phy_addr; //register cmdbuf pool base physical address, it's for cpu
	unsigned long long reg_hw_addr; //register cmdbuf pool base hardware address, it's for hardware ip
	unsigned int reg_total_size; //register cmdbuf pool total size in bytes.
	unsigned int reg_unit_size; //one reg cmdbuf size in bytes. all status cmdbuf have same size.
};

struct enc_config_parameter {
	unsigned short module_type; //input vce=0,cutree=1,vcd=2，jpege=3, jpegd=4
	unsigned short vcmd_core_num; //output, how many vcmd cores are there with corresponding module_type.
	unsigned short submodule_main_addr; //output,if submodule addr == 0xffff, this submodule does not exist.
	unsigned short status_main_addr; //output, main ip offset instatus buffer.
	unsigned short submodule_dec400_addr; //output ,if submodule addr == 0xffff, this submodule does not exist.
	unsigned short status_dec400_addr; //output, dec400 ip offset instatus buffer.
	unsigned short submodule_L2Cache_addr; //output,if submodule addr == 0xffff, this submodule does not exist.
	unsigned short status_L2Cache_addr; //output, L2Cache ip offset instatus buffer.

	unsigned short submodule_MMU_addr[2]; //output,if submodule addr == 0xffff, this submodule does not exist.
	unsigned short status_MMU_addr[2]; //output, MMU ip offset instatus buffer.
	unsigned short submodule_axife_addr[2]; //output,if submodule addr == 0xffff, this submodule does not exist.
	unsigned short status_axife_addr[2]; //output, axife ip offset instatus buffer.
	unsigned short submodule_ufbc_addr; //output ,if submodule addr == 0xffff, this submodule does not exist.
	unsigned short status_ufbc_addr; //output, ufbc ip offset instatus buffer.
	unsigned int vcmd_hw_version_id;
	unsigned int vcmd_priority[MAX_VCMD_CORE_NUM]; //specify the priority of vcmd
};

/*need to consider how many memory should be allocated for status.*/
struct enc_exchange_parameter {
	/** control interrupt mode when generate JMP command
	 * bit31 is mode_flag.
	 *	- when mode_flag is 0, adapative interrupt mode is selected. in such
	 *	  mode, bit[30:0] is executing time estimated for current job;
	 *	- when mode_flag is 1, manual interrupt mode is selected. in such mode,
	 *	  bit[0] is used to set IE flag in JMP command.
	 */
	unsigned int interrupt_ctrl; //input ;executing_time=encoded_image_size*(rdoLevel+1)*(rdoq+1);
	unsigned short module_type; //input input vce=0,IM=1,vcd=2，jpege=3, jpegd=4
	unsigned short cmdbuf_size; //input, reserve is not used; link and run is input.
	unsigned short priority; //input,normal=0, high/live=1
	unsigned short cmdbuf_id; //output ,it is unique in driver.
	unsigned short core_id; //just used for polling.
	unsigned short core_mask; //core_mask for user to select cores
	/* last cmd is JMP (0) or END (1) command */
	unsigned short has_end_cmd;
};

typedef struct CoreWaitOut {
	unsigned int job_id[4];
	unsigned int irq_status[4];
	unsigned int irq_num;
} CORE_WAIT_OUT;

typedef struct {
	unsigned int subsys_idx;
	unsigned int core_type;
	unsigned long offset;
	unsigned int reg_size;
	int irq;
} CORE_CONFIG;

typedef struct {
	unsigned long base_addr;
	unsigned int iosize;
	unsigned int resource_shared; //indicate the core share resources with other cores or not.If 1, means cores can not work at the same time.
	unsigned int asic_id;
} SUBSYS_CONFIG;

typedef struct {
	unsigned int type_info; //indicate which IP is contained in this subsystem and each uses one bit of this variable
	unsigned long offset[CORE_MAX];
	unsigned long regSize[CORE_MAX];
	int irq[CORE_MAX];
} SUBSYS_CORE_INFO;

typedef struct {
	SUBSYS_CONFIG cfg;
	SUBSYS_CORE_INFO core_info;
} SUBSYS_DATA;

#ifdef __cplusplus
}
#endif

#endif /* !_VCX_VCMD_DRIVER_H_ */
