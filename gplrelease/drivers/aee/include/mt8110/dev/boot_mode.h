/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#ifndef _MT_BOOT_MODE_H_
#define _MT_BOOT_MODE_H_

/*
 *uint32_t get_boot_mode() - determine which boot mode to boot
 *
 * return:
 *     code of boot_mode which represent normal, recovery or fastboot mode.
 *
 */
uint32_t get_boot_mode(void);

enum {
	NORMAL_BOOT = 0,
	RECOVERY_BOOT,
	FASTBOOT_BOOT
};

#endif
