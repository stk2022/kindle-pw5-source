/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#include <stdbool.h>
#include <common.h>
#include "include/types.h"

#ifndef __RAM_CONSOLE_COMMON_H__
#define __RAM_CONSOLE_COMMON_H__

enum AEE_EXP_TYPE_NUM {
	AEE_EXP_TYPE_HWT = 1,
	AEE_EXP_TYPE_KE = 2,
	AEE_EXP_TYPE_NESTED_PANIC = 3,
	AEE_EXP_TYPE_SMART_RESET = 4,
	AEE_EXP_TYPE_HANG_DETECT = 5,
	AEE_EXP_TYPE_LK_CRASH = 6,
	AEE_EXP_TYPE_DM_VERITY_CORRUPTION = 7
};

#define RAM_CONSOLE_EXP_TYPE_MAGIC 0xaeedead0
#define RAM_CONSOLE_EXP_TYPE_DEC(exp_type) ({ \
	typeof(exp_type) _exp_type1 = (exp_type); \
	((_exp_type1 ^ RAM_CONSOLE_EXP_TYPE_MAGIC) < 16 ? \
	_exp_type1 ^ RAM_CONSOLE_EXP_TYPE_MAGIC : _exp_type1); })

void ram_console_init(void);
int ram_console_get_wdt_status(unsigned int *ptr);
int ram_console_get_fiq_step(unsigned int *ptr);
int ram_console_get_exp_type(unsigned int *ptr);
int ram_console_set_exp_type(unsigned int exp_type);
int ram_console_is_abnormal_boot(void);
void ram_console_lk_save(unsigned int val, int index);
void ram_console_reboot_reason_save(u32 rgu_status);
void pl_ram_console_init(void);

void ram_console_addr_size(unsigned long *addr, unsigned long *size);
void ram_console_set_dump_step(unsigned int step);
int ram_console_get_dump_step(void);
#ifdef MTK_PMIC_FULL_RESET
bool ram_console_reboot_by_cold_reset(void);
#endif
#endif // #ifndef __RAM_CONSOLE_COMMON_H__

