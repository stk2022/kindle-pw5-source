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


#include <linux/videodev2.h>
#include "mtk_mdp_dither.h"
#include "mtk_mdp_base.h"

void __iomem *mdp_dither_base;

void dither_init(void)
{
	u32 phy_base;

	mdp_dither_base = mdp_init_comp("mediatek,mt8512-mdp-dth",
				       &phy_base);

	pr_info("dither base %p(0x%X)\n", mdp_dither_base, phy_base);
}

void dither_reset(void)
{
	MDP_REG_MASK(MDP_DITHER_RESET, 0x1, 0x1);
	MDP_REG_MASK(MDP_DITHER_RESET, 0x0, 0x1);
}

void dither_start(void)
{
	MDP_REG_MASK(MDP_DITHER_EN, 0x1, 0x1);

	MDP_REG_MASK(MDP_DITHER_INT_EN, 0x3, 0x3);
	MDP_REG_MASK(MDP_DITHER_INT_STAT, 0x0, 0x3);
}

void dither_stop(void)
{
	MDP_LOG_INFO("%s\n", __func__);

	MDP_REG_MASK(MDP_DITHER_EN, 0x0, 0x1);
}

void dither_config(unsigned int width, unsigned int height,
		unsigned int dth_en, unsigned int algo,
		unsigned int src_fmt, unsigned int dst_fmt)
{
	int val = (0x1 << 11);
	u32 dth_mode = algo & 0x00000003;

	MDP_LOG_INFO("DITHER: %dx%d %s->%s %s algo[0x%x]\n", width, height,
		     fmt_2_string(src_fmt), fmt_2_string(dst_fmt),
		     dth_en == 0 ? "Disable" : "Enable", algo);

	MDP_REG_MASK(MDP_DITHER_SIZE, height << 16 | width, 0x3fff3fff);

	if (dth_en != 0) {
		switch (src_fmt) {
		case V4L2_PIX_FMT_Y4_M0:
			val |= (0x1 << 7);
			break;
		case V4L2_PIX_FMT_Y8:
		default:
			break;
		}

		switch (dst_fmt) {
		case V4L2_PIX_FMT_Y2_M0:
			val |= (0x2 << 8);
			break;
		case V4L2_PIX_FMT_Y1_M0:
			val |= (0x3 << 8);
			break;
		case V4L2_PIX_FMT_Y4_M0:
		default:
			val |= (0x1 << 8);
			break;
		}
		val |= (dth_mode << 5);
		val |= (0x1 << 1);
	}

	if (dth_en == 0)
		val |= 0x1;

	MDP_REG_MASK(MDP_DITHER_CFG, val, 0x1fe3);

	MDP_REG_MASK(MDP_DITHER_SHADOW_CTRL, 0x1, 0x1);
}

