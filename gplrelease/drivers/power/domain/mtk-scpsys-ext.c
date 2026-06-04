// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <common.h>
#include <asm/io.h>
#include <asm/processor.h>
#include <linux/iopoll.h>

#include <scpsys-ext.h>

#define MTK_POLL_DELAY_US   10
#define MTK_POLL_TIMEOUT    100

static int set_bus_protection(void __iomem *map, u32 mask, u32 ack_mask,
		u32 reg_set, u32 reg_sta, u32 reg_en)
{
	u32 val;

	if (reg_set)
		writel(mask, map + reg_set);
	else
		clrsetbits_le32(map + reg_en, mask, mask);

	return readl_poll_timeout(map + reg_sta, val,
				  (val & ack_mask) == ack_mask, 100);
}

static int clear_bus_protection(void __iomem *map, u32 mask, u32 ack_mask,
		u32 reg_clr, u32 reg_sta, u32 reg_en)
{
	u32 val;

	if (reg_clr)
		writel(mask, map + reg_clr);
	else
		clrbits_le32(map + reg_en, mask);

	return readl_poll_timeout(map + reg_sta, val,
				  !(val & ack_mask), 100);
}

int mtk_scpsys_ext_set_bus_protection(const struct bus_prot *bp_table,
	void __iomem *infracfg)
{
	int i;

	for (i = 0; i < MAX_STEPS; i++) {
		void __iomem *map = NULL;
		int ret = 0;

		if (bp_table[i].type == INVALID_TYPE)
			continue;
		else if (bp_table[i].type == IFR_TYPE)
			map = infracfg;

		if (map != NULL)
			ret = set_bus_protection(map,
					bp_table[i].mask, bp_table[i].mask,
					bp_table[i].set_ofs,
					bp_table[i].sta_ofs,
					bp_table[i].en_ofs);

		if (ret)
			return ret;
	}

	return 0;
}

int mtk_scpsys_ext_clear_bus_protection(const struct bus_prot *bp_table,
	void __iomem *infracfg)
{
	int i;

	for (i = MAX_STEPS - 1; i >= 0; i--) {
		void __iomem *map = NULL;
		int ret = 0;

		if (bp_table[i].type == INVALID_TYPE)
			continue;
		else if (bp_table[i].type == IFR_TYPE)
			map = infracfg;

		if (map != NULL)
			ret = clear_bus_protection(map,
					bp_table[i].mask,
					bp_table[i].clr_ack_mask,
					bp_table[i].clr_ofs,
					bp_table[i].sta_ofs,
					bp_table[i].en_ofs);

		if (ret)
			return ret;
	}

	return 0;
}
