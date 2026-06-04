// SPDX-License-Identifier: GPL-2.0
/*
 * efuse cli for MediaTek MT8512 SoC
 *
 * Copyright (C) 2020 MediaTek Inc.
 * Author: Michael Mei <michael.mei@mediatek.com>
 */

#include <asm/arch/boot_args.h>
#include <common.h>
#include <command.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int do_eread(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	unsigned long index;
	unsigned long count = 1;

	if (argc < 2) {
		efuse_dump(0, 0);
		return CMD_RET_SUCCESS;
	}

	if (strict_strtoul(argv[1], 10, &index)) {
		printf("Error index number: %s\n", argv[1]);
		return CMD_RET_USAGE;
	}

	if (argc > 2) {
		if (strict_strtoul(argv[2], 10, &count)) {
			printf("Error count number: %s\n", argv[2]);
			return CMD_RET_USAGE;
		}
		if (count < 1) count = 1;
	}

	if (efuse_dump(index, count))
		return CMD_RET_FAILURE;
	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	eread,	4,	0,	do_eread,
	"read a efuse data via index",
	"[<index> [<count>]]\n"
);
