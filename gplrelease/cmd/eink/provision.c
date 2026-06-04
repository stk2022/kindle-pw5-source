/*
 * provision devices
 *
 * Copyright (c) 2020 Amazon.com Inc
 *
 * Licensed under the GPL-2 or later.
 */

#include <common.h>
#include <command.h>
#include <errno.h>
#include <wdt.h>
#include <asm/arch/boot_args.h>

static int do_provision_status(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	printf("provision status:\n"
			"\tRPMB KEY: %d"
			"\n",
			get_rpmb_key_status_from_boot_args()
			);
	return CMD_RET_SUCCESS;
}

static int do_provision_rpmb(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	if (get_rpmb_key_status_from_boot_args()) {
		printf("RPMB Key is provisioned\n");
		return CMD_RET_FAILURE;
	}
	set_clr_rpmbpk_mode(true);
	printf("Please reboot to finish RPMB key provision\n");
	return CMD_RET_SUCCESS;
}

static cmd_tbl_t cmd_provision[] = {
	U_BOOT_CMD_MKENT(status, 2, 0, do_provision_status, "", ""),
	U_BOOT_CMD_MKENT(rpmb, 2, 0, do_provision_rpmb, "", ""),
};

static int do_provision(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	cmd_tbl_t *cp;

	cp = find_cmd_tbl(argv[1], cmd_provision, ARRAY_SIZE(cmd_provision));

	/* Drop the provision command */
	argc--;
	argv++;

	if (cp == NULL || argc > cp->maxargs)
		return CMD_RET_USAGE;
	if (flag == CMD_FLAG_REPEAT && !cmd_is_repeatable(cp))
		return CMD_RET_SUCCESS;
	return cp->cmd(cmdtp, flag, argc, argv);
}

U_BOOT_CMD(provision, 3, 0, do_provision,
		"provision device",
		"status\n"
		"    - show provision status\n"
		"provision rpmb\n"
		"    - provision rpmb");
