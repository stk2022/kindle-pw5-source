/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _MT_BOOT_ARGS_H_
#define _MT_BOOT_ARGS_H_

#include "include/types.h"

struct BOOT_ARGUMENT {
	u32 maggic_number;
	u32 boot_mode;
	u32 boot_reason;
	u32 rgu_mode;
	u32 ddr_reserve_enable;
	u32 ddr_reserve_success;
	u32 ddr_reserve_ready;
	u64 dram_size;
	u32 cold_reset;
};

#define BOOT_ARGUMENT_MAGIC 0x504c504c
#endif
