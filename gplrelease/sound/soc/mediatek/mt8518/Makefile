ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/
ifeq ($(CONFIG_MTK_HIFI4DSP_SUPPORT),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hifi4dsp_audio_ipi/common/framework
endif

snd-soc-mt8518-pcm-objs := \
    mt8518-afe-pcm.o mt8518-afe-utils.o mt8518-snd-utils.o \
    mt8518-afe-controls.o mt8518-afe-debug.o

snd-soc-mt8518-spi-objs := \
    mt8518-audio-spi.o mt8518-adsp-utils.o mt8518-adsp-controls.o \
    mt8518-adsp-debug.o

snd-soc-mt8518-compress-objs := \
    mt8518-adsp-compress.o

# MTK Platform Support
obj-$(CONFIG_SND_SOC_MT8518) += snd-soc-mt8518-pcm.o
obj-$(CONFIG_MTK_HIFI4DSP_SUPPORT) += snd-soc-mt8518-spi.o
obj-$(CONFIG_SND_SOC_MT8518_COMPRESS) += snd-soc-mt8518-compress.o
# Machine support
obj-$(CONFIG_SND_SOC_MT8518_EVB) += mt8518-evb.o
obj-$(CONFIG_SND_SOC_MT8518A_P1) += mt8518-evb.o
obj-$(CONFIG_SND_SOC_MT8518B_P1) += mt8518-evb.o
