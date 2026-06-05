/*
 * Copyright (C) 2014 Freescale Semiconductor, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <command.h>
#include <lpm.h>
#include <power/bd71828.h>
#if defined(CONFIG_CHARGER_DET_MAX20342)
#include <power/max20342.h>
#endif

const pwr_ctrl_t *pwr_ctrl_ops = NULL;

enum {
	DEBUG_DISABLE = 0,
	DEBUG_ENABLE,
};

static int do_lpm_suspend(cmd_tbl_t *cmdtp, int flag,
	int argc, char *const argv[])
{
	if (!pwr_ctrl_ops) {
		printf("pwr ctrl not ready\n");
		return CMD_RET_FAILURE;
	}

	if (pwr_ctrl_ops->psci_ver_check) {
		if (!pwr_ctrl_ops->psci_ver_check()) {
			printf("psci version not match\n");
			return CMD_RET_FAILURE;
		}
	}

#if defined(CONFIG_CHARGER_DET_MAX20342)
	max20342_enable_shutdown_mode();
#endif

	if (pwr_ctrl_ops->plat_pwr_ctrl)
		pwr_ctrl_ops->plat_pwr_ctrl(0);

	if (pwr_ctrl_ops->pwr_domain_suspend)
		pwr_ctrl_ops->pwr_domain_suspend(DEBUG_DISABLE);

	return CMD_RET_SUCCESS;
}

static int do_lpm_dpidle(cmd_tbl_t *cmdtp, int flag,
	int argc, char *const argv[])
{
	if (!pwr_ctrl_ops) {
		printf("pwr ctrl not ready\n");
		return CMD_RET_FAILURE;
	}

	if (pwr_ctrl_ops->psci_ver_check) {
		if (!pwr_ctrl_ops->psci_ver_check()) {
			printf("psci version not match\n");
			return CMD_RET_FAILURE;
		}
	}

	if (pwr_ctrl_ops->plat_pwr_ctrl)
		pwr_ctrl_ops->plat_pwr_ctrl(0);

	if (pwr_ctrl_ops->pwr_domain_dpidle)
		pwr_ctrl_ops->pwr_domain_dpidle(DEBUG_DISABLE);

	return CMD_RET_SUCCESS;
}

static int do_hibernate(cmd_tbl_t *cmdtp, int flag,
	int argc, char *const argv[])
{
#if defined(CONFIG_CHARGER_DET_MAX20342)
	max20342_enable_shutdown_mode();
#endif
#if defined(CONFIG_PMIC_BD71828)
	bd71828_power_off();
#endif
	return CMD_RET_SUCCESS;
}

static int do_ship(cmd_tbl_t *cmdtp, int flag,
	int argc, char *const argv[])
{
#if defined(CONFIG_PMIC_BD71828)
	bd71828_enable_shipping_mode();
#endif
	return CMD_RET_SUCCESS;
}

static cmd_tbl_t cmd_lpm_sub[] = {
	U_BOOT_CMD_MKENT(suspend, 3, 0, do_lpm_suspend, "", ""),
	U_BOOT_CMD_MKENT(dpidle, 5, 0, do_lpm_dpidle, "", ""),
	U_BOOT_CMD_MKENT(hib, 5, 0, do_hibernate, "", ""),
	U_BOOT_CMD_MKENT(ship, 5, 0, do_ship, "", ""),
};

static int do_lpm(cmd_tbl_t *cmdtp, int flag,
	int argc, char *const argv[])
{
	cmd_tbl_t *c;

	/* Strip off leading 'bmp' command argument */
	argc--;
	argv++;

	c = find_cmd_tbl(argv[0], &cmd_lpm_sub[0], ARRAY_SIZE(cmd_lpm_sub));

	plat_pwrctrl_init(&pwr_ctrl_ops);

	if (c)
		return  c->cmd(cmdtp, flag, argc, argv);
	else
		return CMD_RET_USAGE;
}

U_BOOT_CMD(
	lpm,	5,	1,	do_lpm,
	"Low power mode tests suite",
	"suspend    - Enter Suspend Mode\n"
	"lpm dpidle     - Enter Low Power IDLE Mode\n"
	"lpm hib        - Enter hib Mode\n"
	"lpm ship       - Enter ship Mode\n"
);
