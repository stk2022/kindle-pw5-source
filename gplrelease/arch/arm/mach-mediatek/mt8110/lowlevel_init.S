/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 * Author: Mingming Lee <mingming.lee@mediatek.com>
 */

/****************To instead what defined in start.S**************************
 *void save_boot_params(u32 r0, u32 r1, u32 r2, u32 r3) __attribute__((weak))
 ****************************************************************************/
.globl save_boot_params
save_boot_params:
	ldr     r6, =BOOT_ARGUMENT_LOCATION
	/* just use low 32bit since lk fbtool flow do not have 64bit atf */
	str     r0, [r6]
	b       save_boot_params_ret

.section .data
.global BOOT_ARGUMENT_LOCATION
BOOT_ARGUMENT_LOCATION:
	.word 0x0
