ccflags-y += -I$(srctree)/include/power/
ccflags-y += -I$(srctree)/drivers/hwtcon/common/
ccflags-y += -I$(srctree)/drivers/hwtcon/v1/
ccflags-y += -I$(srctree)/drivers/hwtcon/v2/
ccflags-y += -I$(srctree)/drivers/lpm/

# ccflags-y += -DHWTCON_LOG

obj-y += hwtcon.o
obj-y += common/cmdq.o
obj-y += common/gpio.o
obj-y += common/hwtcon_hal.o
obj-y += common/panel_setting.o
obj-y += common/dpi_config.o
obj-y += common/wf_lut_rdma_config.o
obj-y += common/rdma_config.o
obj-y += common/wdma_config.o
obj-y += common/tcon_config.o

obj-y += v1/pipeline_config.o
obj-y += v1/wf_lut_config.o
obj-y += v2/pipeline_config.o
obj-y += v2/wf_lut_config.o
