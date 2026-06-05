// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <common.h>
#include <dm.h>
#include <wdt.h>
#ifdef CONFIG_MTK_THERMAL
#include <thermal.h>
#endif
DECLARE_GLOBAL_DATA_PTR;

int board_init(void)
{
	/* address of boot parameters */
	gd->bd->bi_boot_params = CONFIG_SYS_SDRAM_BASE + 0x100;

	debug("gd->fdt_blob is %p\n", gd->fdt_blob);
	return 0;
}

int board_late_init(void)
{
#if (CONFIG_USB_FUNCTION_FASTBOOT & CONFIG_WDT_MTK)
	/* check if we need to enter fastboot */
	pr_info("Check Fastboot...\n");
	if (check_fastboot_mode()) {
		pr_info("Clear Fastboot flag...\n");
		set_clr_fastboot_mode(0);
		if (run_command("fastboot usb 0", 0))
			pr_err("Failed to execute the fastboot command\n");
	}
#endif
#ifdef CONFIG_MTK_THERMAL
	thermal_init();
#endif
	return 0;
}

#if (CONFIG_USB_FUNCTION_FASTBOOT & CONFIG_WDT_MTK)
int fastboot_set_reboot_flag(void)
{
	pr_info("Set Fastboot flag...\n");
	set_clr_fastboot_mode(1);
	return 0;
}
#endif
