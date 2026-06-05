/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Configuration for MediaTek MT8110 P1 SoC
 *
 * Copyright (C) 2019 MediaTek Inc.
 * Author: Mingming Lee <mingming.lee@mediatek.com>
 */

#ifndef __MT8110_P2_D1_H
#define __MT8110_P2_D1_H

#include <linux/sizes.h>

#define CONFIG_ENV_SIZE				SZ_4K

/* Machine ID */
#define CONFIG_SYS_NONCACHED_MEMORY		SZ_1M

#define CONFIG_CPU_ARMV8

#define COUNTER_FREQUENCY			13000000

/* DRAM definition */
#define CONFIG_SYS_SDRAM_BASE			0x40000000
#define CONFIG_SYS_SDRAM_SIZE			0x20000000
#define CONFIG_SYS_MEMTEST_START		0x50001000
#define CONFIG_SYS_MEMTEST_END			0x50002000

#define CONFIG_SYS_LOAD_ADDR			0x41000000
#define CONFIG_LOADADDR				CONFIG_SYS_LOAD_ADDR

#define CONFIG_SYS_MALLOC_LEN			SZ_32M
#define CONFIG_SYS_BOOTM_LEN			SZ_64M

/* Uboot definition */
#define CONFIG_SYS_UBOOT_START			CONFIG_SYS_TEXT_BASE
#define CONFIG_SYS_INIT_SP_ADDR			(CONFIG_SYS_TEXT_BASE + \
						SZ_2M - \
						GENERATED_GBL_DATA_SIZE)

/* ENV Setting */
#if defined(CONFIG_MMC_MTK)
#define CONFIG_SYS_MMC_ENV_DEV			0
#define CONFIG_ENV_OFFSET			0x1e00000
#define CONFIG_ENV_OVERWRITE

#define DEFAULT_BOOT_PARTITION	"kernel"

/* Console configuration */
#define ENV_DEVICE_SETTINGS \
	"stdin=serial\0" \
	"stdout=serial\0" \
	"stderr=serial\0"

#define CONFIG_EXTRA_ENV_SETTINGS \
	"fdt_high=0xffffffff\0" \
	"initrd_high=0xffffffff\0" \
	ENV_DEVICE_SETTINGS \
	"bootcmd=booteink;\0"
#endif

#endif
