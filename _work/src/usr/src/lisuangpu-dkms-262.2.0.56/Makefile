DRM_DRIVER_NAME := gf

ifndef KERNEL_VERSION
# DKMS invokes this Makefile with KERNELVER=<target kernel> (see dkms.conf).
# Honor it so the module is built against the target kernel's headers instead
# of the kernel that merely happens to be running at build time. Fall back to
# the running kernel only for a plain manual `make`.
KERNEL_VERSION := $(if $(KERNELVER),$(KERNELVER),$(shell uname -r))
endif
LINUXDIR ?= /lib/modules/$(KERNEL_VERSION)/build
KERNEL_GF_DIR := $(shell \
	if [ -d "/lib/modules/$(KERNEL_VERSION)/kernel/drivers/gpu" ];then\
		echo "/lib/modules/$(KERNEL_VERSION)/kernel/drivers/gpu/drm/$(DRM_DRIVER_NAME)";\
	else\
		echo "/lib/modules/$(KERNEL_VERSION)/kernel/drivers/char/drm";\
	fi)

DRM_VER = $(shell sed -n 's/^RHEL_DRM_VERSION = \(.*\)/\1/p' $(LINUXDIR)/Makefile)
ifneq (,$(DRM_VER))
DRM_PATCH = $(shell sed -n 's/^RHEL_DRM_PATCHLEVEL = \(.*\)/\1/p' $(LINUXDIR)/Makefile)
DRM_SUBLEVEL = $(shell sed -n 's/^RHEL_DRM_SUBLEVEL = \(.*\)/\1/p' $(LINUXDIR)/Makefile)
DRM_CODE = $(shell expr $(DRM_VER) \* 65536 + 0$(DRM_PATCH) \* 256 + 0$(DRM_SUBLEVEL))
EXTRA_CFLAGS += -DDRM_VERSION_CODE=$(DRM_CODE)
endif

DEBUG ?= 0
FAKE_CHIP = 0

ifeq ($(DEBUG), 1)
	EXTRA_CFLAGS += -ggdb3
	EXTRA_CFLAGS += -O1
	EXTRA_CFLAGS += -D_DEBUG_
else
	EXTRA_CFLAGS += -O2
endif

ifeq ($(FAKE_CHIP), 1)
	EXTRA_CFLAGS += -DFAKE_CHIP
endif

DISPLAY_PATH = display
VIDEO_CODEC_PATH = video_codec
VIDEO_ENCODER_PATH = video_codec/VC9000E
VIDEO_DECODER_PATH = video_codec/VC9000D
VIDEO_INC_PATH = include/video
#EXTRA_CFLAGS += -D__VIRTAUL_VBLANK__

EXTRA_CFLAGS += -D__LINUX__
EXTRA_CFLAGS += -Wall -Wno-undef -Wno-unused -Wno-missing-braces -Wno-overflow -Wno-missing-prototypes -Wno-missing-declarations -Werror -Wno-array-bounds
EXTRA_CFLAGS += -fno-strict-aliasing
EXTRA_CFLAGS += -D__DISP_LINUX__

#EXTRA_CFLAGS += -DHANTRODEC_DEBUG
#EXTRA_CFLAGS += -DREGISTER_DEBUG
#EXTRA_CFLAGS += -DSUPPORT_DBGFS

#EXTRA_CFLAGS += -DHANTRO_VCMD_DRIVER_DEBUG
EXTRA_CFLAGS += -DHANTROVCMD_ENABLE_IP_SUPPORT
#EXTRA_CFLAGS += -DDEBUG_PRINT_REGS
EXTRA_CFLAGS += -DHANTROAXIFE_SUPPORT
#EXTRA_CFLAGS += -DVCMD_DEBUG_INTERNAL


BIN_TYPE ?= $(shell uname -m |sed -e s/i.86/i386/)
ifeq ($(BIN_TYPE), x86_64)
	EXTRA_CFLAGS += -m64 -mhard-float
endif

ifeq ($(BIN_TYPE), i386)
	EXTRA_CFLAGS += -m32 -mhard-float
endif

quiet_cmd_xxd = XXD     $@
cmd_xxd = xxd -r $< $@

HEX_TARGETS = $(subst .o.hex,.o,$(wildcard objs/$(BIN_TYPE)/*.o.hex))

%.o: %.o.hex
	$(call cmd,xxd)

EXTRA_CFLAGS += \
	-I$(src)/include \
	-I$(src)/kms \
	-I$(src)/$(DISPLAY_PATH) \
	-I$(src)/$(DISPLAY_PATH)/crtc \
	-I$(src)/$(DISPLAY_PATH)/hdaudio \
	-I$(src)/$(DISPLAY_PATH)/connector \
	-I$(src)/$(DISPLAY_PATH)/encoder \
	-I$(src)/$(DISPLAY_PATH)/register \
	-I$(src)/$(DISPLAY_PATH)/intr \
	-I$(src)/$(DISPLAY_PATH)/common \
	-I$(src)/$(DISPLAY_PATH)/vbt \
	-I$(src)/$(VIDEO_CODEC_PATH) \
	-I$(src)/$(VIDEO_ENCODER_PATH) \
	-I$(src)/$(VIDEO_DECODER_PATH) \
	-I$(src)/$(VIDEO_INC_PATH) 

ccflags-y += $(EXTRA_CFLAGS)
KBUILD_CFLAGS += -I$(src)
KBUILD_CFLAGS += -fno-sanitize=all

binary := lisuan_proprietary.o

$(DRM_DRIVER_NAME)-objs := \
	lisuan_vm.o \
	lisuan_sync.o \
	lisuan_gem.o \
	lisuan_context.o \
	lisuan_drv.o \
	lisuan_device.o \
	lisuan_mailbox.o \
	lisuan_procid.o \
	lisuan_sched_entity.o \
	lisuan_sched_fence.o \
	lisuan_sched_main.o \
	lisuan_trace_points.o \
	lisuan_ring.o \
	lisuan_fb.o \
	lisuan_vm_gpu.o \
	lisuan_vm_cpu.o \
	lisuan_p2pdma.o \
	gf_os_interface.o \
	gf_debugfs.o \
	gf_irq.o \
	gf_job.o \
	gf_ringbuffer.o \
	gf_trans.o \
	gf_kmd_event.o \
	kms/gf_kms.o \
	kms/gf_kms_virt.o \
	kms/gf_kms_dm.o \
	kms/gf_kms_pipe_crc.o \
	kms/gf_kms_hdr.o \
	$(VIDEO_CODEC_PATH)/bidirect_list.o \
	$(VIDEO_CODEC_PATH)/vcmdswhwregisters.o \
	$(VIDEO_ENCODER_PATH)/vcx_abnormal_irq.o \
	$(VIDEO_ENCODER_PATH)/vcx_axife.o \
	$(VIDEO_ENCODER_PATH)/vcx_normal_driver.o \
	$(VIDEO_ENCODER_PATH)/vcx_vcmd_driver.o \
	$(VIDEO_DECODER_PATH)/hantro_axife.o \
	$(VIDEO_DECODER_PATH)/hantro_dec.o \
	$(VIDEO_DECODER_PATH)/hantro_vcmd.o \
	$(VIDEO_DECODER_PATH)/subsys.o \
	$(DISPLAY_PATH)/display.o \
	$(DISPLAY_PATH)/disp_device.o \
	$(DISPLAY_PATH)/disp_callback.o \
	$(DISPLAY_PATH)/crtc/disp_plane.o \
	$(DISPLAY_PATH)/crtc/disp_dhi.o \
	$(DISPLAY_PATH)/crtc/disp_pipe.o \
	$(DISPLAY_PATH)/crtc/disp_crtc.o \
	$(DISPLAY_PATH)/crtc/disp_pll.o \
	$(DISPLAY_PATH)/crtc/disp_hdr.o \
	$(DISPLAY_PATH)/encoder/disp_dvo_encoder.o \
	$(DISPLAY_PATH)/encoder/disp_dp_encoder.o \
	$(DISPLAY_PATH)/encoder/disp_hdmi_encoder.o \
	$(DISPLAY_PATH)/encoder/disp_encoder.o \
	$(DISPLAY_PATH)/connector/disp_dvo_connector.o \
	$(DISPLAY_PATH)/connector/disp_dp_connector.o \
	$(DISPLAY_PATH)/connector/disp_hdmi_connector.o \
	$(DISPLAY_PATH)/connector/disp_pwm.o \
	$(DISPLAY_PATH)/connector/disp_displayid.o \
	$(DISPLAY_PATH)/connector/disp_connector.o \
	$(DISPLAY_PATH)/connector/disp_edid.o \
	$(DISPLAY_PATH)/connector/disp_mode.o \
	$(DISPLAY_PATH)/hdaudio/disp_codec.o \
	$(DISPLAY_PATH)/intr/disp_intr.o \
	$(DISPLAY_PATH)/vbt/disp_vbt.o \
	$(DISPLAY_PATH)/common/disp_hw_common.o \
	$(DISPLAY_PATH)/connector/disp_dp_aux.o \
	$(DISPLAY_PATH)/encoder/disp_dp_phy.o \
	$(DISPLAY_PATH)/encoder/disp_dsc.o \
	objs/$(BIN_TYPE)/$(binary)

obj-m := $(DRM_DRIVER_NAME).o

modules: component
	make -j8 -C $(LINUXDIR) M=`pwd` modules

	@rm -f modules.* Module.* *.ur-safe .cache.mk *.o* *.dwo *.mod* .*d
	@rm -f $(DRM_DRIVER_NAME)-objs $(obj-m)
	@rm -f kms/.*.o.cmd kms/*.o
	@rm -f objs/$(BIN_TYPE)/.*.o.cmd objs/$(BIN_TYPE)/*.o
	@find $(DISPLAY_PATH) -name "*.o.*" -type f | xargs rm -rf
	@find $(DISPLAY_PATH) -name "*.o" -type f | xargs rm -rf
	@find $(VIDEO_ENCODER_PATH) -name "*.o.*" -type f | xargs rm -rf
	@find $(VIDEO_ENCODER_PATH) -name "*.o" -type f | xargs rm -rf
	@find $(VIDEO_DECODER_PATH) -name "*.o.*" -type f | xargs rm -rf
	@find $(VIDEO_DECODER_PATH) -name "*.o" -type f | xargs rm -rf
	@find $(VIDEO_CODEC_PATH) -name "*.o.*" -type f | xargs rm -rf
	@find $(VIDEO_CODEC_PATH) -name "*.o" -type f | xargs rm -rf
component: $(HEX_TARGETS)
	@echo "Prepare other component..."
	@echo '$(binary):' > objs/$(BIN_TYPE)/.$(binary).cmd

clean:
	@rm -f modules.* Module.* *.ur-safe .cache.mk *.o* *.dwo *.ko *.mod* .*d
	@rm -f $(DRM_DRIVER_NAME)-objs $(obj-m)
	@rm -f kms/.*.o.cmd kms/*.o
	@rm -f objs/$(BIN_TYPE)/.*.o.cmd objs/$(BIN_TYPE)/*.o
	@find $(DISPLAY_PATH) -name "*.o.*" -type f | xargs rm -rf
	@find $(DISPLAY_PATH) -name "*.o" -type f | xargs rm -rf
	@find $(VIDEO_ENCODER_PATH) -name "*.o.*" -type f | xargs rm -rf
	@find $(VIDEO_ENCODER_PATH) -name "*.o" -type f | xargs rm -rf
	@find $(VIDEO_DECODER_PATH) -name "*.o.*" -type f | xargs rm -rf
	@find $(VIDEO_DECODER_PATH) -name "*.o" -type f | xargs rm -rf
	@find $(VIDEO_CODEC_PATH) -name "*.o.*" -type f | xargs rm -rf
	@find $(VIDEO_CODEC_PATH) -name "*.o" -type f | xargs rm -rf


install:
	@if [ `whoami` != "root" ] && [ `whoami` != "ROOT" ]; then \
	echo "need root permission"; \
	exit 1; \
	fi
	@mkdir -p $(KERNEL_GF_DIR)
	install -c -m 0744 ./$(DRM_DRIVER_NAME).ko $(KERNEL_GF_DIR)

