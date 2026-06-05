// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#include "dev/mtk_wdt.h"

extern int kedump_mini(void) __attribute__((weak));

void do_check_mrdump(void)
{
	extern int kedump_init(void para)__attribute((weak));
	if (kedump_init) {
		if (kedump_init())
			kedump_mini();
	}
}
