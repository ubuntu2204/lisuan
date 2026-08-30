/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __GF_GLOBAL_INIT__H__
#define __GF_GLOBAL_INIT__H__

#include <linux/types.h>
#define MMIO_GPC_MASK_REG       0x22ed00
#define MMIO_HW_FB_SIZE_REG     0x22cc2c
#define MMIO_HW_CFG_VF_MIRROR   0x22C400
#define HW_12G_LOCAL_VALUE 0x12121212
#define HW_24G_LOCAL_VALUE 0x24242424
#define MAX_GPC_NUMBER 6
#define MHU_PHB_INDEX  6

#define MMIO_CPIA_START_ADDRESS 0x10000

#define DEBUG_BUS_ITEM_NUM (sizeof(g_debugbus_info)/sizeof(g_debugbus_info[0]))
#define DEBUG_BUS_ENABLE_REG 0x25008C 
#define DEBUG_BUS_CFG_REG 0x250090 
#define DEBUG_BUS_READ_BASE 0x22f340 

#define DMC0_BS0_MASK_REG_BASE_OFFSET 0x31801C

static unsigned int MMIO_MHU_START_ADDRESS = 0x18000;
static unsigned int MMIO_MMU_START_ADDRESS = 0x20000;

enum mailbox_request_event {
	MAILBOX_CLEAR = 0,
	MAILBOX_PREDEFINE_EVENT0,
	MAILBOX_PREDEFINE_EVENT1,
	MAILBOX_CUSTOM_MSG = 3,
	MAILBOX_CUSTOM_MSG_CHIP_RESET_NOTIFICATION,
	MAILBOX_CUSTOM_MSG_CHIP_RESET_NOTIFICATION_ACK,
	MAILBOX_CUSTOM_MSG_CHIP_RESET_FINISH_NOTIFICATION,
	MAILBOX_CUSTOM_MSG_CHIP_RESET_DROP_NOTIFICATION,
	MAILBOX_CUSTOM_MSG_VF_RESET_NOTIFICATION,
	MAILBOX_CUSTOM_MSG_VF_RESET_NOTIFICATION_ACK,
	MAILBOX_CUSTOM_MSG_VF_RESET_FINISH_NOTIFICATION,
};

struct voltage {
	unsigned engine_voltage;
	unsigned soc_voltage;
	unsigned total_power;
	unsigned engine_power;
	unsigned mem_power;
};

struct module_clk {
	unsigned cgm_clk;
	unsigned gl2_clk;
	unsigned gpc_clk;
	unsigned vdd0_clk;
	unsigned vdd1_clk;
	unsigned vem_clk;
	unsigned mhb_clk;
};

struct misc_info {
	char firmwareMemroyinfos[256];
	char realMemroyinfos[256];
	unsigned gpcMask;
};

struct therm {
	unsigned therm[6]; 
	unsigned therm_max_recorded[6];
	unsigned hitHighTempRange;
}__attribute__((aligned(8)));

struct irq_Int_Reg1{//clone from Reg_Int_Reg1_En
	union
	{
		unsigned int uint;
		struct
		{
			unsigned int MailBox         : 1;
			unsigned int Ts0             : 1;
			unsigned int Fs              : 1;
			unsigned int Video_0         : 1;//dec1
			unsigned int Phb             : 1;//phb fence bit
			unsigned int Video_2         : 1;//dec0
			unsigned int Dpe             : 1;
			unsigned int HDA_PPA_INTR    : 1;
			unsigned int Video_1         : 1;//enc
			unsigned int Uart            : 1;
			unsigned int I2c_0           : 1;
			unsigned int I2c_1           : 1;
			unsigned int Ts1             : 1;
			unsigned int Msg             : 1;
			unsigned int Bus             : 1;
			unsigned int Pcie_Contoller  : 1;
			unsigned int Gpio            : 1;
			unsigned int reserve_3       : 3;
			unsigned int I2c_3           : 1;
			unsigned int I2c_2           : 1;
			unsigned int Cpia_Cpu_Pwr_int: 1;

			unsigned int Mhu_PageError   : 1;
			unsigned int phy_ln_0        : 1;
			unsigned int reserve_4       : 1;
			unsigned int venc_va4 : 1;
			unsigned int vdec_1_va0 : 1;
			unsigned int vdec_0_va0 : 1;
			unsigned int venc_va0 : 1;
			unsigned int Flr_In_Progress_0 : 1; //pf0 to vf irq
			unsigned int Flr_In_Progress_1 : 1; //audio to vf irq
		};
	};
};

struct irq_Int_Reg2 {  //clone from  Reg_Int_Reg2_En
	union 
	{
		unsigned int uint;
		struct
		{
			unsigned int Vf_Flr_In_Progress  : 1; //
			unsigned int Pf_To_Vf_Irq        : 1; //pf to vf irq
			unsigned int reserved0           : 2;
			unsigned int Vf_To_Pf_Irq        : 1;//vf to pf irq
			unsigned int reserved            : 27;
		};
	};
};

struct debugbus_item_s
{
	unsigned int base;
	unsigned int length;
};

typedef union {
	unsigned int uint;
	struct {
		unsigned int reg_bs0_mask                            : 15;
		unsigned int reserved_0                              : 1;
		unsigned int reg_bs1_mask                            : 15;
		unsigned int cl_map                                  : 1;
	} reg;
} Reg_Bs_Mask_Reg_Base;


void gf_init_pwm_setting(unsigned char *mmio_base, bool enable_hw_cg, bool is_virtfn);
void gf_pci_mem_config_init(unsigned char *mmio_base, bool enable_hw_cg, bool is_virtfn);
/*
 * lisuan_device *dev;
 * struct lisuan_cl_info cl_info;
 * 
 * lisuan_query_cl_info(dev,&cl_info)
 * mmio_base : dev->mmio_base
 * vram_size : dev->vram_size / 1024 / 1024;//In MB
 * cl_base : cl_info.cl_base
 * cl_size : cl_info.cl_size
 * mhu_disable_mask : dev->mhu_disable_mask
 * mhb_num : dev->mhb_num
 * is_virtfn : dev->is_virtfn
 */
int gf_hw_mhb_init(unsigned char *mmio_base, unsigned int vram_size, unsigned int cl_base, unsigned int cl_size, unsigned int mhu_disable_mask, int mhb_num, bool is_virtfn);
/*
 * lisuan_device *dev;
 * struct lisuan_pci_info pci_info;
 * 
 * lisuan_query_pci_msi_info(dev,&pci_info)
 * mmio_base : dev->mmio_base;
 * pos : pci_info.pos
 * address_hi : pci_info.address_hi
 * address_lo : pci_info.address_lo
 * data : pci_info.data
 * adr_64bit : pci_info.adr_64bit
 * msi_enable : pci_info.msi_enable
 * msi_mm_en : pci_info.msi_mm_en
 */
void gf_msi_init(void* pls_dev, void* pci_dev, unsigned char *mmio_base, int pos, unsigned int address_hi, unsigned int address_lo, unsigned short msi_ctl, unsigned short data, unsigned int adr_64bit, unsigned int msi_enable, unsigned int msi_mm_en, unsigned int is_virtual, unsigned int msi_cap);
void gf_send_mail_box_req(unsigned char *mmio_base, unsigned int unitId, unsigned int funcid, unsigned int data_dw_size, unsigned int* data_ptr, bool need_wait);
void gf_clear_mail_box_irq_flag(unsigned char *mmio_base);
void gf_get_hw_clk(unsigned char *mmio_base, struct module_clk* pclks);
void gf_get_hw_voltage(unsigned char *mmio_base, struct voltage *pvoltage);
void gf_set_hw_therm_fan_mode(unsigned char *mmio_base, unsigned int mode, unsigned int fix_speed);
void gf_get_hw_therm_fan_speed(unsigned char *mmio_base, unsigned int *speed);
void gf_get_hw_therm(unsigned char *mmio_base, struct therm *ptherm);
void gf_get_hw_FM_version(unsigned char *mmio_base, unsigned char *buf);
void gf_get_customer_id(unsigned char *mmio_base, unsigned int *value);
void gf_get_device_id(unsigned char *mmio_base, unsigned int *value);
void gf_get_misc_infos(unsigned char *mmio_base, struct misc_info *misc_info);


void gf_interrupt_enable(unsigned char *mmio_base, unsigned is_virtfn);
void gf_interrupt_disable(unsigned char *mmio_base, unsigned is_virtfn);
void gf_reset_hw(unsigned char *mmio_base, unsigned int is_virtfn, unsigned int virtfn_id, unsigned char crtc_index);

void gf_handle_mhb_irq(unsigned char *mmio_base, unsigned int is_virtfn, unsigned int virtfn_id, unsigned mhb_num);

void gf_get_hw_irq(unsigned char *mmio_base, struct irq_Int_Reg1* shadow_reg1, struct irq_Int_Reg1* reg1, struct irq_Int_Reg2 *shadow_reg2);
void gf_clear_hw_irq(unsigned char *mmio_base, struct irq_Int_Reg1* shadow_reg1, struct irq_Int_Reg2 *shadow_reg2, unsigned is_virtfn);
void gf_clear_fence_irq(unsigned char *mmio_base);

void gf_hantro_reset(unsigned char *mmio_base, unsigned int is_virtfn, unsigned int virtfn_id);

void gf_dump_status(unsigned char*mmio_base, unsigned is_virtfn, unsigned virtfn_id, unsigned rb_index);

bool gf_is_ring_at_current_vf(unsigned char*mmio_base, unsigned is_virtfn, unsigned virtfn_id, unsigned rb_index);

void gf_map_cf_buffer(unsigned char *mmio_base);
void gf_dump_debug_status(unsigned char *mmio_base);
int gf_init_rb_hw(int rb_index, unsigned char *mmio_base, bool is_virtfn, unsigned long long rb_va, unsigned long long rb_size, unsigned char **start, unsigned char **size, unsigned char **head, unsigned char **tail);
int gf_send_request(unsigned char *mmio_base, unsigned int request_id);
int gf_send_event_ack(unsigned char *mmio_base, unsigned ack);
unsigned int gf_get_event_id(unsigned char *mmio_base);
void gf_set_mmu_pid_0_entry(unsigned char *mmio_base, int i, unsigned int pid0_entry_start);
void gf_set_mmu_bypass_begin(unsigned char *mmio_base, unsigned int begin);
void gf_set_mmu_bypass_end(unsigned char *mmio_base, unsigned int end);
void gf_mmu_bypass_inv(unsigned char *mmio_base);
void gf_set_mhb_fb_vf(unsigned char *mmio_base, unsigned int fb_base, unsigned int fb_size);
void gf_get_vf_info(unsigned char *mmio_base, unsigned int *is_virtfn, unsigned int *virtfn_id);
unsigned int gf_get_mhu_disable_mask(unsigned char *mmio_base);
unsigned int gf_get_vf_hw_cfg(unsigned char *mmio_base);
unsigned int gf_get_vf_fb_size(unsigned char *mmio_base);
unsigned int gf_get_gpc_mask(unsigned char *mmio_base);
unsigned int gf_get_fb_cfg(unsigned char *mmio_base);
int gf_parse_fm_version(unsigned char *mmio_base, unsigned char *major, unsigned char *minor, unsigned char *patch);

void gf_hw_init_tlb_level0_pte(unsigned int * p_level0_pte, unsigned int Valid_Page, unsigned int SEC, unsigned int Framebuf_PCIE, unsigned int CPU_SNOOP, unsigned int Address);
void gf_hw_init_tlb_level1_pte(unsigned int * p_level1_pte, unsigned int Valid_Page, unsigned int SEC, unsigned int Framebuf_PCIE, unsigned int CPU_SNOOP, unsigned int Page_Block, unsigned int Page_size, unsigned int Address);
void gf_hw_init_tlb_level2_pte(unsigned int * p_level2_pte, unsigned int Valid_Page, unsigned int SEC, unsigned int Framebuf_PCIE, unsigned int CPU_SNOOP, unsigned int Address);
void gf_get_time_stamp(unsigned char *mmio_base, unsigned int *hi, unsigned int *low);

#endif
