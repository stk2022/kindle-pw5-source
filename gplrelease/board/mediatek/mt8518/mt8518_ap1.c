// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 MediaTek Inc.
 */

#include <common.h>
#include <dm.h>

DECLARE_GLOBAL_DATA_PTR;

int board_init(void)
{
	/* address of boot parameters */
	gd->bd->bi_boot_params = CONFIG_SYS_SDRAM_BASE + 0x100;

	printf("gd->fdt_blob is %p\n", gd->fdt_blob);
	return 0;
}

int board_late_init(void)
{
	/*to load environment variable from persistent store*/
	gd->env_valid = 1;
	env_relocate();

	return 0;
}
