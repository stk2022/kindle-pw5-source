export KERNEL_SRC := /lib/modules/$(shell uname -r)/build

ccflags-y += -I$(srctree)/drivers/misc/mediatek/hwtcon/hal
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic/fiti
ccflags-y += -I$(srctree)/include/soc
ccflags-y += -I$(srctree)/include/linux/
ccflags-y += -I$(srctree)/lib/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hwtcon/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/mt8512
ccflags-y += -I$(srctree)/drivers/media/platform/mtk-mdp
ccflags-y += -I$(srctree)/drivers/media/platform/mtk-imgrz

# enable M4U
ccflags-y += -DHWTCON_M4U_SUPPORT

# clock always on
# ccflags-y += -DHWTCON_CLK_ALWAYS_ON

# ccflags-y += -DHWTCON_ENABLE_WF_LUT_IRQ

# ccflags-y += -DHWTCON_EANBLE_DISP_RDMA_IRQ

# use auto lut merge sof
# ccflags-y += -DHWTCON_USE_AUTO_LUT_MERGE
# ccflags-y += -DHWTCON_AUTO_CHANGE_BUFFER_INDEX

# use cpu config pipeline
# ccflags-y += -DHWTCON_USE_CPU_CONFIG


MODULE_NAME = hwtcon

$(MODULE_NAME)-objs := hwtcon_fb.o
$(MODULE_NAME)-objs += hwtcon_core.o
$(MODULE_NAME)-objs += hwtcon_debug.o
$(MODULE_NAME)-objs += hwtcon_driver.o
$(MODULE_NAME)-objs += hwtcon_rect.o
$(MODULE_NAME)-objs += hwtcon_file.o
$(MODULE_NAME)-objs += hwtcon_mdp.o
$(MODULE_NAME)-objs += hwtcon_epd.o
$(MODULE_NAME)-objs += decompress_inflate.o

# hal code
$(MODULE_NAME)-objs += hal/hwtcon_wdma_config.o
$(MODULE_NAME)-objs += hal/hwtcon_rdma_config.o
$(MODULE_NAME)-objs += hal/hwtcon_pipeline_config.o
$(MODULE_NAME)-objs += hal/hwtcon_paper_top_config.o
$(MODULE_NAME)-objs += hal/hwtcon_hal.o
$(MODULE_NAME)-objs += hal/hwtcon_regal_config.o
$(MODULE_NAME)-objs += hal/hwtcon_dpi_config.o
$(MODULE_NAME)-objs += hal/hwtcon_tcon_config.o
$(MODULE_NAME)-objs += hal/hwtcon_wf_lut_config.o
$(MODULE_NAME)-objs += hal/hwtcon_wf_lut_rdma_config.o

obj-$(CONFIG_MTK_HWTCON) += $(MODULE_NAME).o
