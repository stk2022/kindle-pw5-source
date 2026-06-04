// SPDX-License-Identifier: GPL-2.0-only
/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly
 * prohibited.
 */
/* MediaTek Inc. (C) 2016. All rights reserved. */

#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <linux/io.h>
#include "include/mt8110/dev/mt_reg_base.h"
#include "include/aee_platform_debug.h"

#define LATCH_BUF_LENGTH	0x8000
#define MCUSYS_MP0_DBG_CTRL             (MCUCFG_BASE + 0x404)
#define MCUSYS_DBG_MON_SEL_A            (MCUCFG_BASE + 0x590)
#define MCUSYS_DBG_MON                  (MCUCFG_BASE + 0x594)
#define MCUSYS_MCU_ALL_PWR_ON_CTRL      (MCUCFG_BASE + 0xB58)
#define RG_MCU_ALL_PWR_ISO_DIS      (1 << 2)
#define RG_MCU_ALL_PWR_ON           (1 << 1)

static int lastpc_dump(char *buf, int *wp)
{
	unsigned int i, addr;
	unsigned int pc_lo, pc_hi, fp_32, sp_32;
	unsigned int fp_64_lo, fp_64_hi, sp_64_lo, sp_64_hi;

	if (buf == NULL || wp == NULL)
		return -1;

	//This is secure register: let all cores in power on state
	writel(readl(MCUSYS_MCU_ALL_PWR_ON_CTRL) | 0xFB, MCUSYS_MCU_ALL_PWR_ON_CTRL);

	printf("0xB58 = 0x%x\n", readl(MCUSYS_MCU_ALL_PWR_ON_CTRL));

	writel(0x1, MCUSYS_DBG_MON_SEL_A); //set to 1 to select cluster MP0

	*wp += snprintf(buf + *wp, LATCH_BUF_LENGTH - *wp, "\n*************************** lastpc ***************************\n");

	for (i = 0; i < 2; i++) {
		writel(readl(MCUSYS_MP0_DBG_CTRL) & (~0xf0), MCUSYS_MP0_DBG_CTRL);
		writel(readl(MCUSYS_MP0_DBG_CTRL) | (i << 4), MCUSYS_MP0_DBG_CTRL);

		writel(readl(MCUSYS_MP0_DBG_CTRL) & (~0xf), MCUSYS_MP0_DBG_CTRL);
		pc_lo = readl(MCUSYS_DBG_MON);

		writel(readl(MCUSYS_MP0_DBG_CTRL) & (~0xf), MCUSYS_MP0_DBG_CTRL);
		writel(readl(MCUSYS_MP0_DBG_CTRL) | (0x1), MCUSYS_MP0_DBG_CTRL);
		pc_hi = readl(MCUSYS_DBG_MON);

		writel(readl(MCUSYS_MP0_DBG_CTRL) & (~0xf), MCUSYS_MP0_DBG_CTRL);
		writel(readl(MCUSYS_MP0_DBG_CTRL) | (0x2), MCUSYS_MP0_DBG_CTRL);
		fp_32 = readl(MCUSYS_DBG_MON);

		writel(readl(MCUSYS_MP0_DBG_CTRL) & (~0xf), MCUSYS_MP0_DBG_CTRL);
		writel(readl(MCUSYS_MP0_DBG_CTRL) | (0x3), MCUSYS_MP0_DBG_CTRL);
		sp_32 = readl(MCUSYS_DBG_MON);

		writel(readl(MCUSYS_MP0_DBG_CTRL) & (~0xf), MCUSYS_MP0_DBG_CTRL);
		writel(readl(MCUSYS_MP0_DBG_CTRL) | (0x4), MCUSYS_MP0_DBG_CTRL);
		fp_64_lo = readl(MCUSYS_DBG_MON);

		writel(readl(MCUSYS_MP0_DBG_CTRL) & (~0xf), MCUSYS_MP0_DBG_CTRL);
		writel(readl(MCUSYS_MP0_DBG_CTRL) | (0x5), MCUSYS_MP0_DBG_CTRL);
		fp_64_hi = readl(MCUSYS_DBG_MON);

		writel(readl(MCUSYS_MP0_DBG_CTRL) & (~0xf), MCUSYS_MP0_DBG_CTRL);
		writel(readl(MCUSYS_MP0_DBG_CTRL) | (0x6), MCUSYS_MP0_DBG_CTRL);
		sp_64_lo = readl(MCUSYS_DBG_MON);

		writel(readl(MCUSYS_MP0_DBG_CTRL) & (~0xf), MCUSYS_MP0_DBG_CTRL);
		writel(readl(MCUSYS_MP0_DBG_CTRL) | (0x7), MCUSYS_MP0_DBG_CTRL);
		sp_64_hi = readl(MCUSYS_DBG_MON);

		*wp += snprintf(buf + *wp, LATCH_BUF_LENGTH - *wp,
				"[LAST PC] CORE_%d pc_lo=0x%x, pc_hi=0x%x, fp_32=0x%x, sp_32=0x%x\n",
				i, pc_lo, pc_hi, fp_32, sp_32);

		*wp += snprintf(buf + *wp, LATCH_BUF_LENGTH - *wp,
				"[LAST PC] CORE_%d fp_64_lo=0x%x, fp_64_hi=0x%x, sp_64_lo=0x%x, sp_64_hi=0x%x\n",
				i, fp_64_lo, fp_64_hi, sp_64_lo, sp_64_hi);
	}

	*wp += snprintf(buf + *wp, LATCH_BUF_LENGTH - *wp, "\n");

	/* Boot from watchdog is 0xFF, which will cause slave CPU boot fail,  need to clear bit2 & 1 to reset to default status */
	writel(readl(MCUSYS_MCU_ALL_PWR_ON_CTRL) & (~RG_MCU_ALL_PWR_ISO_DIS), MCUSYS_MCU_ALL_PWR_ON_CTRL);
	writel(readl(MCUSYS_MCU_ALL_PWR_ON_CTRL) & (~RG_MCU_ALL_PWR_ON), MCUSYS_MCU_ALL_PWR_ON_CTRL);
	printf("0xB58 = 0x%x\n", readl(MCUSYS_MCU_ALL_PWR_ON_CTRL));

	return 1;
}

int latch_get(void **data, int *len)
{
	int ret;
	*len = 0;
	*data = malloc(LATCH_BUF_LENGTH);
	if (*data == NULL)
		return 0;

	ret = lastpc_dump(*data, len);
	if (ret < 0 || *len > LATCH_BUF_LENGTH) {
		*len = (*len > LATCH_BUF_LENGTH) ? LATCH_BUF_LENGTH : *len;
		return ret;
	}

	return 1;
}


void latch_put(void **data)
{
	free(*data);
}

static unsigned int save_cpu_bus_data(u64 offset, int *len, CALLBACK dev_write)
{
	char *buf = NULL;
	int ret;
	unsigned int datasize = 0;

	/* Save latch buffer */
	ret = latch_get((void **)&buf, len);
	if (buf != NULL) {
		if (*len > 0)
			datasize = dev_write(buf, *len);
		latch_put((void **)&buf);
	}

	/* Save systracker buffer */
	ret = systracker_get((void **)&buf, len, 8);
	if (ret && (buf != NULL)) {
		if (*len > 0)
			datasize += dev_write(buf, *len);
		systracker_put((void **)&buf);
	}

	return datasize;
}

static unsigned int save_dfd_data(u64 offset, int *len, CALLBACK dev_write)
{
	return 0;
}

static unsigned int spm_wdt_latch_regs[] = {
	SLEEP_BASE + 0x800, /* PCM_WDT_LATCH_0 */
	SLEEP_BASE + 0x804, /* PCM_WDT_LATCH_1 */
	SLEEP_BASE + 0x808, /* PCM_WDT_LATCH_2 */
	SLEEP_BASE + 0x80c, /* PCM_WDT_LATCH_3 */
	SLEEP_BASE + 0x810, /* PCM_WDT_LATCH_4 */
	SLEEP_BASE + 0x814, /* PCM_WDT_LATCH_5 */
	SLEEP_BASE + 0x818, /* PCM_WDT_LATCH_6 */
	SLEEP_BASE + 0x81c, /* PCM_WDT_LATCH_7 */
	SLEEP_BASE + 0x820, /* PCM_WDT_LATCH_8 */
	SLEEP_BASE + 0x824, /* PCM_WDT_LATCH_9 */
	SLEEP_BASE + 0x838, /* PCM_WDT_LATCH_10 */
	SLEEP_BASE + 0x83c, /* PCM_WDT_LATCH_11 */
	SLEEP_BASE + 0x888, /* PCM_WDT_LATCH_12 */
	SLEEP_BASE + 0x88c, /* PCM_WDT_LATCH_13 */
};
#define SPM_DATA_BUF_LENGTH (4096)

static int spm_dump_data(char *buf, int *wp)
{
	unsigned int i;
	unsigned val;

	if (buf == NULL || wp == NULL)
		return -1;

	for (i = 0; i < (sizeof(spm_wdt_latch_regs)/sizeof(unsigned int)); i++) {
		val = readl(spm_wdt_latch_regs[i]);
		*wp += sprintf(buf + *wp,
				"SPM regs(0x%x) = 0x%x\n",
				spm_wdt_latch_regs[i], val);
	}

	if (*wp > SPM_DATA_BUF_LENGTH) 
		return 0;

	return 1;
}

int spm_data_get(void **data, int *len)
{
	int ret;

	*len = 0;
	*data = malloc(SPM_DATA_BUF_LENGTH);
	if (*data == NULL)
		return 0;

	ret = spm_dump_data(*data, len);
	if (ret < 0 || *len > SPM_DATA_BUF_LENGTH) {
		*len = (*len > SPM_DATA_BUF_LENGTH) ? SPM_DATA_BUF_LENGTH : *len;
		return ret;
	}

	return 1;
}

void spm_data_put(void **data)
{
	free(*data);
}

static unsigned int save_spm_data(u64 offset, int *len, CALLBACK dev_write)
{
	char *buf = NULL;
	unsigned int datasize = 0;

	/* Save SPM buffer */
	spm_data_get((void **)&buf, len);
	if (buf != NULL) {
		if (*len > 0)
			datasize = dev_write(buf, *len);
		spm_data_put((void **)&buf);
	}

	return datasize;
}

static unsigned int save_spm_sram_data(u64 offset, int *len, CALLBACK dev_write)
{
	return 0;
}

static unsigned int save_dram_data(u64 offset, int *len, CALLBACK dev_write)
{
	return 0;
}

#ifdef MTK_TINYSYS_SSPM_SUPPORT
static unsigned int save_sspm_coredump(u64 offset, int *len, CALLBACK dev_write)
{
	return 0;
}

static unsigned int save_sspm_data(u64 offset, int *len, CALLBACK dev_write)
{
	return 0;
}

static unsigned int save_sspm_xfile(u64 offset, int *len, CALLBACK dev_write)
{
	return 0;
}

static unsigned int save_sspm_last_log(u64 offset, int *len, CALLBACK dev_write)
{
	return 0;
}
#endif

static unsigned int save_hvfs_data(u64 offset, int *len, CALLBACK dev_write)
{
	return 0;
}

static unsigned int save_pllk_last_log(u64 offset, int *len, CALLBACK dev_write)
{
	return 0;
}

static unsigned int save_log_dur_lkdump(u64 offset, int *len,
					CALLBACK dev_write)
{
	return 0;
}

static unsigned int save_mcdi_data(u64 offset, int *len, CALLBACK dev_write)
{
	return 0;
}

#ifdef MTK_TINYSYS_SCP_SUPPORT
static unsigned int save_scp_coredump(u64 offset, int *len, CALLBACK dev_write)
{
	return 0;
}
#endif

/* platform initial function */
int platform_debug_init(void)
{
	modify_plat_dfd20_get(save_dfd_data);
	modify_plat_dram_get(save_dram_data);
	modify_plat_cpu_bus_get(save_cpu_bus_data);
	modify_plat_spm_data_get(save_spm_data);
	modify_plat_spm_sram_data_get(save_spm_sram_data);
	modify_plat_hvfs_get(save_hvfs_data);
#ifdef MTK_TINYSYS_SSPM_SUPPORT
	modify_plat_sspm_coredump_get(save_sspm_coredump);
	modify_plat_sspm_data_get(save_sspm_data);
	modify_plat_sspm_xfile_get(save_sspm_xfile);
	modify_plat_sspm_log_get(save_sspm_last_log);
#endif
	modify_plat_pllk_last_log_get(save_pllk_last_log);
	modify_plat_dur_lkdump_get(save_log_dur_lkdump);
	modify_plat_mcdi_get(save_mcdi_data);
#ifdef MTK_TINYSYS_SCP_SUPPORT
	modify_plat_scp_coredump_get(save_scp_coredump);
#endif

	return 1;
}
