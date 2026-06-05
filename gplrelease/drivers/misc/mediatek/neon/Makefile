obj-$(CONFIG_KERNEL_MODE_NEON) += neon_ops.o
KBUILD_CFLAGS += -mfpu=neon -mfloat-abi=softfp -ffreestanding -O3
KBUILD_CFLAGS := $(filter-out -msoft-float,$(KBUILD_CFLAGS))
KBUILD_CFLAGS := $(filter-out -mfpu=vfp,$(KBUILD_CFLAGS))
KBUILD_CFLAGS := $(filter-out -O2,$(KBUILD_CFLAGS))
