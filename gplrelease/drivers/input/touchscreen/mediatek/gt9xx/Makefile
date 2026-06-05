# Linux driver folder
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/gt9xx/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/

gt9xx-ts-y := gt9xx.o gt9xx_update.o goodix_tool.o
obj-$(CONFIG_TOUCHSCREEN_MTK_GT9XX)	+= gt9xx-ts.o
