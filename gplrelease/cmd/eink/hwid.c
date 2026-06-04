// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Amazon.com Inc.
 */

#include <config.h>
#include <common.h>
#include <command.h>

__weak int board_get_hwid(char* string)
{
	strcpy(string, "Unknown");
	return 0;
}

static int do_hwid(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int hwid;
	char hwid_name[16];

	hwid = board_get_hwid(hwid_name);
	printf("HWID: %d (%s)\n", hwid, hwid_name);
	return CMD_RET_SUCCESS;
}
U_BOOT_CMD(hwid, 2, 1, do_hwid, "show HW ID", NULL);
