/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2020 Mediatel Inc.
 */

#ifndef _LPM_H
#define _LPM_H

/**
 * SIP cmd to spm driver
 *
 * RESET: reset after enter suspend/dpidle mode
 * LOG_EN: enable wakeup log
 * NOTIFY: notify ATF, this is uboot jump into atf
 */

enum {
	NO_CMD = 0,
	RESET,
	LOG_EN,
	NOTIFY,
};

/**
 * power ctrl flag
 */

enum {
	DIS_ALL = 0,
	CLK_EN,
	MTCMOS_EN,
	DCM_OFF,
	ALL_EN,
};


enum pwr_ctrl_enum {
	PW_PCM_FLAGS,
	PW_PCM_FLAGS_CUST,
	PW_PCM_FLAGS_CUST_SET,
	PW_PCM_FLAGS_CUST_CLR,
	PW_PCM_FLAGS1,
	PW_PCM_FLAGS1_CUST,
	PW_PCM_FLAGS1_CUST_SET,
	PW_PCM_FLAGS1_CUST_CLR,
	PW_TIMER_VAL,
	PW_TIMER_VAL_CUST,
	PW_TIMER_VAL_RAMP_EN,
	PW_TIMER_VAL_RAMP_EN_SEC,
	PW_WAKE_SRC,
	PW_WAKE_SRC_CUST,
	PW_WAKELOCK_TIMER_VAL,
	PW_WDT_DISABLE,

	/* SPM_AP_STANDBY_CON */
	PW_WFI_OP,
	PW_MP0_CPUTOP_IDLE_MASK,
	PW_MP1_CPUTOP_IDLE_MASK,
	PW_MCUSYS_IDLE_MASK,
	PW_MM_MASK_B,
	PW_MD_DDR_EN_0_DBC_EN,
	PW_MD_DDR_EN_1_DBC_EN,
	PW_MD_MASK_B,
	PW_SSPM_MASK_B,
	PW_SCP_MASK_B,
	PW_SRCCLKENI_MASK_B,
	PW_MD_APSRC_1_SEL,
	PW_MD_APSRC_0_SEL,
	PW_CONN_DDR_EN_DBC_EN,
	PW_CONN_MASK_B,
	PW_CONN_APSRC_SEL,
	PW_CONN_SRCCLKENA_SEL_MASK,

	/* SPM_SRC_REQ */
	PW_SPM_APSRC_REQ,
	PW_SPM_F26M_REQ,
	PW_SPM_INFRA_REQ,
	PW_SPM_VRF18_REQ,
	PW_MAX_COUNT,
};

/**
 * psci ops for jump to ATF
 *
 * psci_ver_check: check psci version, only support major version >= 1
 * pwr_domain_suspend: enter suspend mode
 * pwr_domain_dpidle: enter dpidle mode
 * xxx_finish: wakeup from suspend/dpidle, don't must implement
 */

typedef struct plat_pwr_ctrl {
	bool (*psci_ver_check)(void);
	void (*pwr_domain_suspend)(int);
	void (*pwr_domain_suspend_finish)(int);
	void (*pwr_domain_dpidle)(int);
	void (*pwr_domain_dpidle_finish)(int);
	bool (*plat_pwr_ctrl)(int);

} pwr_ctrl_t;

#ifdef CONFIG_ARM64
#define MTK_SIP_SMC_AARCH_BIT			0x40000000
#else
#define MTK_SIP_SMC_AARCH_BIT			0x00000000
#endif

/* SPM related SMC call */
#define MTK_SIP_KERNEL_SPM_SUSPEND_ARGS \
	(0x82000220 | MTK_SIP_SMC_AARCH_BIT)

/* SPM deepidle related SMC call */
#define MTK_SIP_KERNEL_SPM_DPIDLE_ARGS \
	(0x82000227 | MTK_SIP_SMC_AARCH_BIT)

/* SPM ARGS */
#define MTK_SIP_KERNEL_SPM_ARGS	\
	(0x8200022A | MTK_SIP_SMC_AARCH_BIT)

/* SPM get pwr_ctrl args */
#define MTK_SIP_KERNEL_SPM_GET_PWR_CTRL_ARGS \
	(0x8200022B | MTK_SIP_SMC_AARCH_BIT)

/*SPM set pwr_ctrl args*/
#define MTK_SIP_KERNEL_SPM_PWR_CTRL_ARGS \
		(0x82000224 | MTK_SIP_SMC_AARCH_BIT)


enum {
	SPM_SUSPEND,
	SPM_RESUME,
	SPM_DPIDLE_ENTER,
	SPM_DPIDLE_LEAVE,
	SPM_ENTER_SODI,
	SPM_LEAVE_SODI,
	SPM_ENTER_SODI3,
	SPM_LEAVE_SODI3,
	SPM_SUSPEND_PREPARE,
	SPM_POST_SUSPEND,
	SPM_DPIDLE_PREPARE,
	SPM_POST_DPIDLE,
	SPM_SODI_PREPARE,
	SPM_POST_SODI,
	SPM_SODI3_PREPARE,
	SPM_POST_SODI3,
	SPM_VCORE_PWARP_CMD,
	SPM_PWR_CTRL_SUSPEND,
	SPM_PWR_CTRL_DPIDLE,
	SPM_PWR_CTRL_SODI,
	SPM_PWR_CTRL_SODI3,
	SPM_PWR_CTRL_VCOREFS,
};


#define LOCAL_REG_SET_DECLARE \
	register size_t reg0 __asm__("r0") = function_id; \
	register size_t reg1 __asm__("r1") = arg0; \
	register size_t reg2 __asm__("r2") = arg1; \
	register size_t reg3 __asm__("r3") = arg2; \
	register size_t reg4 __asm__("r4") = arg3; \
	size_t ret

static inline size_t mt_secure_call_all(size_t function_id,
	size_t arg0, size_t arg1, size_t arg2,
	size_t arg3, size_t *r1, size_t *r2, size_t *r3)
{
	LOCAL_REG_SET_DECLARE;

#ifdef CONFIG_ARM64
	__asm__ volatile ("smc #0x0\n" : "+r"(reg0),
		"+r"(reg1), "+r"(reg2), "+r"(reg3), "+r"(reg4));
#else
	__asm__ volatile (".arch_extension sec\n" "smc #0" : "+r"(reg0),
		"+r"(reg1), "+r"(reg2), "+r"(reg3), "+r"(reg4));
#endif
	ret = reg0;
	if (r1 != NULL)
		*r1 = reg1;
	if (r2 != NULL)
		*r2 = reg2;
	if (r3 != NULL)
		*r3 = reg3;
	return ret;
}


#define mt_secure_call(_fun_id, _arg0, _arg1, _arg2, _arg3) \
	mt_secure_call_all(_fun_id, _arg0, _arg1, _arg2, _arg3, 0, 0, 0)

/* SMC call's marco */
#define SMC_CALL(_name, _arg0, _arg1, _arg2) \
		 mt_secure_call(MTK_SIP_KERNEL_SPM_##_name, \
				_arg0, _arg1, _arg2, 0)

bool plat_pwrctrl_init(const pwr_ctrl_t **pwrctrl_ops);
#endif
