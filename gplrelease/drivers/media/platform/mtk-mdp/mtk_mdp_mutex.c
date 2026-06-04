/*
 * Copyright (C) 2020 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include "mtk_mdp_base.h"
#include "mtk_mdp_mutex.h"

void __iomem *mdp_mutex_base;
void __iomem *mdp_config_base;

void mdp_mutex_init(void)
{
	u32 phy_base;

	mdp_mutex_base = mdp_init_comp("mediatek,mt8512-imgsys-mutex",
				       &phy_base);

	pr_info("mutex base 0x%p(0x%X)\n", mdp_mutex_base, phy_base);
}

void config_init(void)
{
	u32 phy_base;

	mdp_config_base = mdp_init_comp("mediatek,mt8512-imgsys",
				       &phy_base);

	pr_info("config base 0x%p(0x%X)\n", mdp_config_base, phy_base);
}

void reset_mutex(void)
{
	MDP_REG_MASK(MDP_MUTEX0_RST, 0x1, 0x1);
	MDP_REG_MASK(MDP_MUTEX0_RST, 0x0, 0x1);
}

void config_mutex(void)
{
	MDP_REG_MASK(MDP_MUTEX_INTEN, 0xFFFFFFFF, 0xFFFFFFFF);

	MDP_REG_MASK(MDP_MUTEX0_CTL, 0x400, 0x400);

	MDP_REG_MASK(MDP_MUTEX0_MOD, 0x18021, 0xFFFFFFFF);
}

void connect_path(void)
{
	MDP_REG_MASK(MDP_CONFIG_RDMA0_MOUT_EN, 0x1, 0x1F); /* 0xF08 */
	MDP_REG_MASK(MDP_CONFIG_GAMMA0_MOUT_EN, 0x2, 0x1F); /* 0xF1C */

	MDP_REG_MASK(MDP_CONFIG_DITHER0_SEL_IN, 0x0, 0x1); /* 0xF50 */
	MDP_REG_MASK(MDP_CONFIG_WROT0_SEL_IN, 0x0, 0x7);  /* 0xF38 */
}

void trigger_path(void)
{
	MDP_REG_MASK(MDP_MUTEX0_EN, 0x1, 0x1);
}


