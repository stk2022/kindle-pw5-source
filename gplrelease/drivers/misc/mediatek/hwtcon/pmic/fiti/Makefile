$(warning harry build hwtcon)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hwtcon/hal
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hwtcon/pmic/fiti
ccflags-y += -I$(srctree)/include/soc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hwtcon/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/mt8512

# enable M4U
ccflags-y += -DHWTCON_M4U_SUPPORT

# clock always on
# ccflags-y += -DHWTCON_CLK_ALWAYS_ON

# ccflags-y += -DHWTCON_ENABLE_WF_LUT_IRQ

# use auto lut merge sof
# ccflags-y += -DHWTCON_USE_AUTO_LUT_MERGE
# ccflags-y += -DHWTCON_AUTO_CHANGE_BUFFER_INDEX

# use cpu config pipeline
# ccflags-y += -DHWTCON_USE_CPU_CONFIG

# pmic
obj-y += fiti_core.o
