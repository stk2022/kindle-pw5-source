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


#ifndef __MDP_GAMMA_H__
#define __MDP_GAMMA_H__

#define MDP_8BIT_LUT_SIZE			256
#define MDP_4BIT_LUT_SIZE			16
#define MDP_LUT_10BIT_MASK		0x03ff

#define MDP_GAMMA_ENABLE_INVERSION		BIT(0)
#define MDP_GAMMA_FORCE_MONOCHROME		BIT(1)
#define MDP_GAMMA_USE_CMAP			BIT(2)

enum mtk_mdp_gamma_lut_id {
	MDP_GAMMA_DEFAULT,
	MDP_GAMMA_COLORMAP,
	MDP_GAMMA_FORCE_MONO,
	MDP_GAMMA_COLORMAP_FORCE_MONO,
	MDP_GAMMA_MAX,
};

extern void __iomem *mdp_gamma_base;

#define MDP_GAMMA_BASE mdp_gamma_base

#define MDP_GAMMA_EN		(MDP_GAMMA_BASE + 0x00)
#define MDP_GAMMA_RESET		(MDP_GAMMA_BASE + 0x04)
#define MDP_GAMMA_INT_EN	(MDP_GAMMA_BASE + 0x08)
#define MDP_GAMMA_INT_STAT	(MDP_GAMMA_BASE + 0x0c)
#define MDP_GAMMA_CFG		(MDP_GAMMA_BASE + 0x20)
#define MDP_GAMMA_SIZE		(MDP_GAMMA_BASE + 0x30)
#define MDP_GAMMA_LUT		(MDP_GAMMA_BASE + 0x700)

#define MDP_GAMMA_RELAY_MODE		BIT(0)
#define MDP_GAMMA_LUT_EN		BIT(1)
#define MDP_GAMMA_LUT_TYPE		BIT(2)
#define MDP_GAMMA_INT_4B_MODE		BIT(3)
#define MDP_GAMMA_LUT_OUT_LSHIFT_4B	BIT(4)


void gamma_init(void);
void gamma_reset(void);
void gamma_start(void);
void gamma_stop(void);
void gamma_config(u32 width, u32 height, u32 invert, u32 gamma_flag);
void gamma_gen_table(u32 gamma_flag);
void gamma_update_cmap_lut(u16* gamma_lut, u32 len);

#endif
