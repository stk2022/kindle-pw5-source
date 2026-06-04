// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Amazon.com Inc.
 */

#include <config.h>
#include <common.h>
#include <command.h>
#include <part.h>
#ifdef CONFIG_UFBL
#include <ufbl.h>
#endif

DECLARE_GLOBAL_DATA_PTR;

/* return 1 to use default in boot.img */
__weak int board_get_config(char *config) {
	return 1;
}

int do_boot_eink(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int ret = 0;
	u32 n, cnt;
	struct blk_desc *dev_desc;
	disk_partition_t info;
	struct fdt_header *fdt_header;
	void *load;

	char boot_config[32] = "#conf-";
	char *bootm_argv[] = { "bootm", boot_config };
	int bootm_argc = 2;

	/* disable console logs */
	if (ufbl_is_locked_production_device())
		gd->flags |= GD_FLG_DISABLE_CONSOLE;

	load = (void*)load_addr;

	/* find kernel partition */
	dev_desc = blk_get_dev("mmc", 0);
	if (!dev_desc) {
		pr_err("cannot find mmc device\n");
		return CMD_RET_FAILURE;
	}

	ret = blk_dselect_hwpart(dev_desc, 0);
	if (ret) {
		pr_err("cannot find user partition\n");
		return CMD_RET_FAILURE;
	}

	ret = part_get_info_by_name(dev_desc, DEFAULT_BOOT_PARTITION, &info);
	if (ret <= 0) {
		pr_err("cannot find boot partition: %s\n", DEFAULT_BOOT_PARTITION);
		return CMD_RET_FAILURE;
	}

	cnt = 1;
	n = blk_dread(dev_desc, info.start, cnt, load);
	if (n != cnt) {
		pr_err("fail to load header of kernel partition\n");
		return CMD_RET_FAILURE;
	}

	/* check and read fdt header */
	fdt_header = load;
	ret = fdt_check_header(fdt_header);
	if (ret) {
		pr_err("invalid fdt header\n");
		return CMD_RET_FAILURE;
	}

	/* read rest of kernel image */
	cnt = fdt_totalsize(fdt_header) / info.blksz;
	if (cnt > info.size || cnt > SZ_64K) { // limit to 64K blocks (32MB)
		pr_err("image size is too large\n");
		return CMD_RET_FAILURE;
	}
	n = blk_dread(dev_desc, info.start+1, cnt, load+info.blksz);
	if (n != cnt) {
		pr_err("fail to load kernel partition\n");
		return CMD_RET_FAILURE;
	}

	if (argc == 2) {
		/* use config from argv */
		sprintf(boot_config, "#conf-%s", argv[1]);
	} else {
		ret = board_get_config(boot_config+6);
		if (ret) {
			bootm_argc = 1;
			strcpy(boot_config, "");
		}
	}

	debug("run bootm with %d args: %s %s\n", bootm_argc, bootm_argv[0], bootm_argv[1]);
	while (do_bootm(find_cmd("do_bootm"), 0, bootm_argc, bootm_argv) != CMD_RET_SUCCESS) {
		run_command("fastboot", 0);
	}
	return CMD_RET_SUCCESS;
}

static char booteink_help_text[] =
    "[dtb_name]\n    - boot application image from default kernel partition\n"
	"\tpassing arguments 'dtb_name' to boot with a different dtb file\n";

U_BOOT_CMD(
	booteink,	2,	1,	do_boot_eink,
	"boot application image from default kernel partition", booteink_help_text
);
