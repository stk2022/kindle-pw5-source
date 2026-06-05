// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#include "include/boot_args.h"
#include "include/debug.h"
#include "include/ram_console.h"
#include "dev/mtk_wdt.h"

static int kedump_get_bootreason(unsigned int wdt_status)
{
	unsigned int g_rgu_status = 0;

	if (wdt_status & MTK_WDT_STATUS_HWWDT_RST) {
		/* For E1 bug, that SW reset value is 0xC000,  */
		/* we using "==" to check */
		/* Time out reboot always by pass power key */
		g_rgu_status = RE_BOOT_BY_WDT_HW;
	} else if (wdt_status & MTK_WDT_STATUS_SWWDT_RST) {
		g_rgu_status = RE_BOOT_BY_WDT_SW;
	} else {
		g_rgu_status = RE_BOOT_REASON_UNKNOWN;
	}

	if (wdt_status & MTK_WDT_STATUS_IRQWDT_RST)
		g_rgu_status |= RE_BOOT_WITH_INTTERUPT;

#ifdef MTK_THERMAL_RESET_SUPPORT
	if (wdt_status & MTK_WDT_STATUS_SPM_THERMAL_RST)
		g_rgu_status |= RE_BOOT_BY_SPM_THERMAL;
#endif
	if (wdt_status & MTK_WDT_STATUS_SPMWDT_RST)
		g_rgu_status |= RE_BOOT_BY_SPM;
	if (wdt_status & MTK_WDT_STATUS_THERMAL_CTL_RST)
		g_rgu_status |= RE_BOOT_BY_THERMAL_DIRECT;
	if (wdt_status & MTK_WDT_STATUS_DEBUGWDT_RST)
		g_rgu_status |= RE_BOOT_BY_DEBUG;
	if (wdt_status & MTK_WDT_STATUS_SECURITY_RST)
		g_rgu_status |= RE_BOOT_BY_SECURITY;

#ifdef MTK_PMIC_FULL_RESET
	if (mtk_wdt_is_pmic_full_reset())
		g_rgu_status |= RE_BOOT_BY_PMIC_FULL_RST;
#endif
	return g_rgu_status;
}

int kedump_init(void)
{
	static int kedump_dumped;
	u32 reg_boot_reason = 0;

	reg_boot_reason = get_wdt_status_from_boot_args();
	unsigned int boot_reason = kedump_get_bootreason(reg_boot_reason);

	pl_ram_console_init();
	ram_console_reboot_reason_save(boot_reason);
	ram_console_init();

	/* this flow should be executed once only */
	if (kedump_dumped == 0) {
		if (ram_console_is_abnormal_boot())
			return 1;
	}
	return 0;
}
