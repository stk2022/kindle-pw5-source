// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek lpm driver
 *
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <common.h>
#include <div64.h>
#include <lpm.h>
#include <linux/arm-smccc.h>
#include <linux/psci.h>
#include <asm/psci.h>
#include <asm/proc-armv/system.h>
#include  "pwr_ctrl.h"

#define DRV_WriteReg32(addr, value) writel(value, addr)
#define DRV_Reg32(addr) readl(addr)

struct arm_smccc_res res;
extern u32 wmt_pwr_off_consys_mcu(void);

static bool plat_psci_ver_check(void)
{
	arm_smccc_smc(ARM_PSCI_0_2_FN_PSCI_VERSION, 0, 0, 0, 0, 0, 0, 0, &res);

	if (PSCI_VERSION_MAJOR(res.a0) < 1 && PSCI_VERSION_MINOR(res.a0) >= 2)
		return false;

	return true;
}

static void plat_psci_suspend(int flag)
{
	if (flag) {
		//SMC_CALL(PWR_CTRL_ARGS, SPM_PWR_CTRL_SUSPEND, PW_SPM_APSRC_REQ, 0x1);
		SMC_CALL(PWR_CTRL_ARGS, SPM_PWR_CTRL_SUSPEND, 145, 0x1); /*log en*/
		SMC_CALL(PWR_CTRL_ARGS, SPM_PWR_CTRL_SUSPEND, PW_TIMER_VAL_CUST, 0x28000);
	}
	SMC_CALL(PWR_CTRL_ARGS, SPM_PWR_CTRL_SUSPEND, PW_WAKE_SRC_CUST, 0x41);

	arm_smccc_smc(ARM_PSCI_1_0_FN_SYSTEM_SUSPEND, 0, 0, 0, 0, 0, 0, 0, &res);
}

static void plat_psci_suspend_finish(int flag)
{
	if (flag) {
		//SMC_CALL(PWR_CTRL_ARGS, SPM_PWR_CTRL_SUSPEND, PW_SPM_APSRC_REQ, 0x0);
		SMC_CALL(PWR_CTRL_ARGS, SPM_PWR_CTRL_SUSPEND, 145, 0x0);
		SMC_CALL(PWR_CTRL_ARGS, SPM_PWR_CTRL_SUSPEND, PW_TIMER_VAL_CUST, 0x0);
	}
	SMC_CALL(PWR_CTRL_ARGS, SPM_PWR_CTRL_SUSPEND, PW_WAKE_SRC_CUST, 0x0);
}

static void plat_psci_dpidle(int flag)
{
	if (flag) {
		//SMC_CALL(PWR_CTRL_ARGS, SPM_PWR_CTRL_DPIDLE, PW_SPM_APSRC_REQ, 0x1);
		SMC_CALL(PWR_CTRL_ARGS, SPM_PWR_CTRL_DPIDLE, 145, 0x1);/*log en*/
		SMC_CALL(PWR_CTRL_ARGS, SPM_PWR_CTRL_DPIDLE, PW_TIMER_VAL_CUST, 0x28000);
	}

	SMC_CALL(PWR_CTRL_ARGS, SPM_PWR_CTRL_DPIDLE, PW_WAKE_SRC_CUST, 0x41);
	DRV_WriteReg32(0x1000C00C, DRV_Reg32(0x1000C00C) | 0x6);
	printf("0x1000C00C= 0x%x\n", DRV_Reg32(0x1000C00C));
	DRV_WriteReg32(0x1000C000, DRV_Reg32(0x1000C000) & ~0x1);
	printf("0x1000C000= 0x%x\n", DRV_Reg32(0x1000C000));
	DRV_WriteReg32(0x1000C000, DRV_Reg32(0x1000C000) | 0x80);
	printf("0x1000C000= 0x%x\n", DRV_Reg32(0x1000C000));

	arm_smccc_smc(ARM_PSCI_0_2_FN_CPU_SUSPEND, 0x01010004, 0, 0, 0, 0, 0, 0, &res);
}

static void plat_psci_dpidle_finish(int flag)
{
	if (flag) {
		//SMC_CALL(PWR_CTRL_ARGS, SPM_PWR_CTRL_DPIDLE, PW_SPM_APSRC_REQ, 0x0);
		SMC_CALL(PWR_CTRL_ARGS, SPM_PWR_CTRL_DPIDLE, 145, 0x0);
		SMC_CALL(PWR_CTRL_ARGS, SPM_PWR_CTRL_DPIDLE, PW_TIMER_VAL_CUST, 0x0);
	}

	SMC_CALL(PWR_CTRL_ARGS, SPM_PWR_CTRL_DPIDLE, PW_WAKE_SRC_CUST, 0x0);
}

static bool plat_pwr_ctrl(int flag)
{
	int temp = 0;
#ifdef CONFIG_CMD_CONSYS
	wmt_pwr_off_consys_mcu();
#endif
	gpio_show_pins_info();
	gpio_set_same_to_kernel();
	gpio_show_pins_info();
	pmic_ldo_power_off();
	mtcmos_all_off(0);
	dump_pwr_status();
	dcm_all_on();

	/* usb power down need univpll on */
	analog_off();
	subsys_cg_all_off();
	topck_all_off();
	pll_all_off(0);

	//AUXADC_TS power down
	temp = DRV_Reg32(0x1000C600);
	DRV_WriteReg32(0x1000C600, temp & 0xFFF03FFF);
	temp = DRV_Reg32(0x1000C600);
	DRV_WriteReg32(0x1000C600, temp | (3<<28));

}

static const pwr_ctrl_t pwr_ctrl_ops = {
	.psci_ver_check = plat_psci_ver_check,
	.pwr_domain_suspend = plat_psci_suspend,
	.pwr_domain_suspend_finish = plat_psci_suspend_finish,
	.pwr_domain_dpidle = plat_psci_dpidle,
	.pwr_domain_dpidle_finish = plat_psci_dpidle_finish,
	.plat_pwr_ctrl = plat_pwr_ctrl,
};

bool plat_pwrctrl_init(const pwr_ctrl_t **pwrctrl_ops)
{
	*pwrctrl_ops = &pwr_ctrl_ops;

	/*uboot jump to atf*/
	SMC_CALL(PWR_CTRL_ARGS, SPM_PWR_CTRL_SUSPEND, PW_PCM_FLAGS1_CUST, 0x80000000);
	SMC_CALL(PWR_CTRL_ARGS, SPM_PWR_CTRL_DPIDLE, PW_PCM_FLAGS1_CUST, 0x80000000);

	return true;
}
