// SPDX-License-Identifier: GPL-2.0
/*
 * Configuration for MediaTek MT8110 SoC
 *
 * Copyright (C) 2019 MediaTek Inc.
 * Author: Mingming Lee <mingming.lee@mediatek.com>
 */

#include <clk.h>
#include <common.h>
#include <dm.h>
#include <fdtdec.h>
#include <ram.h>
#include <asm/arch/misc.h>
#include <asm/armv8/mmu.h>
#include <asm/sections.h>
#include <dm/uclass.h>
#include <dt-bindings/clock/mt8512-clk.h>
#include <asm/arch/boot_args.h>

DECLARE_GLOBAL_DATA_PTR;

__weak int ft_board_setup(void *blob, bd_t *bd) {
	return 0;
}

int dram_init(void)
{
	fdtdec_setup_mem_size_base();
	gd->ram_size = (phys_size_t)(get_dramsize_from_boot_args());
	return 0;
}

int dram_init_banksize(void)
{
	gd->bd->bi_dram[0].start = gd->ram_base;
	gd->bd->bi_dram[0].size = get_effective_memsize();

	return 0;
}

void reset_cpu(ulong addr)
{
	struct udevice *watchdog_dev = NULL;

	if (uclass_get_device_by_seq(UCLASS_WDT, 0, &watchdog_dev))
		if (uclass_get_device(UCLASS_WDT, 0, &watchdog_dev))
			printf("%s: pls implement wdt first!\n", __func__);

	wdt_expire_now(watchdog_dev, 0);
}

int print_cpuinfo(void)
{
	debug("CPU:   MediaTek MT8110\n");
	return 0;
}
