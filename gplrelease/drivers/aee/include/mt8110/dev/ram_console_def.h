/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#ifndef __RAM_CONSOLE_DEF_H__
#define __RAM_CONSOLE_DEF_H__

#include "dev/mt8512.h"

// ram_console over dram for mt8512

#ifdef RAM_CONSOLE_OVER_SRAM  // sram
#define RAM_CONSOLE_DEF_ADDR RAM_CONSOLE_SRAM_ADDR
#define RAM_CONSOLE_DEF_SIZE RAM_CONSOLE_SRAM_SIZE

#else  // dram
#define RAM_CONSOLE_DRAM_ADDR (DRAM_BASE_PHY + 0x04400000)
#define RAM_CONSOLE_DRAM_SIZE (0x10000)

#define RAM_CONSOLE_DEF_ADDR RAM_CONSOLE_DRAM_ADDR
#define RAM_CONSOLE_DEF_SIZE RAM_CONSOLE_DRAM_SIZE
#endif

// align minirdump-reserved-memory in dts
#define KE_RESERVED_MEM_ADDR (DRAM_BASE_PHY + 0x044f0000)

#define PSTORE_ADDR (DRAM_BASE_PHY + 0x04410000)
#define PSTORE_SIZE 0xe0000

#define KEDUMP_BUFFER_ADDR (DRAM_BASE_PHY + 0x04500000)
#define KEDUMP_BUFFER_SIZE 0x100000

#endif
