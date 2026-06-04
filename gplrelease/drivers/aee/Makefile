cur_dir := $(shell dirname $(lastword $(MAKEFILE_LIST)))

ccflags-$(CONFIG_MT8110) += -I$(cur_dir)/include/mt8110

obj-$(CONFIG_MTK_KEDUMP_MINI_SUPPORT) += \
	KEDump.o \
	ram_console.o \
	dump_joint.o \
	mrdump_setup.o \
	platform_debug.o

obj-$(CONFIG_MTK_AEE_SAVE_DEBUGINFO_RESERVED_BUFFER) += dev/mt8110/aee_platform_debug.o
obj-$(CONFIG_MTK_AEE_SAVE_DEBUGINFO_RESERVED_BUFFER) += dev/mt8110/systracker.o
