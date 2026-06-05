# SPDX-License-Identifier: GPL-2.0+
#
# (C) Copyright 2020 Amazon.com Inc

ifdef CONFIG_EINK
obj-$(CONFIG_FRONTLIGHT) += frontlight.o
obj-$(CONFIG_DTB_SETUP) += dtb_setup.o
obj-$(CONFIG_HIBERNATION) += hibernation.o
obj-$(CONFIG_HIBERNATION_FALCON) += hibernation_falcon.o
obj-$(CONFIG_MISCDATA) += miscdata.o
obj-$(CONFIG_EINK_DISPLAY) += display.o
ccflags-$(CONFIG_EINK_DISPLAY) += -I$(srctree)/drivers/hwtcon/ -I$(srctree)/drivers/hwtcon/common/ -I$(srctree)/drivers/lpm/
obj-$(CONFIG_UFBL) += ufbl/
endif # CONFIG_EINK

