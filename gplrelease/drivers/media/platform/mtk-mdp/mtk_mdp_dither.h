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


#ifndef __MDP_DITHER_H__
#define __MDP_DITHER_H__

extern void __iomem *mdp_dither_base;

#define MDP_DITHER_BASE mdp_dither_base

#define MDP_DITHER_EN (MDP_DITHER_BASE + 0x00)
#define MDP_DITHER_RESET (MDP_DITHER_BASE + 0x04)
#define MDP_DITHER_INT_EN (MDP_DITHER_BASE + 0x08)
#define MDP_DITHER_INT_STAT (MDP_DITHER_BASE + 0x0c)
#define MDP_DITHER_CFG (MDP_DITHER_BASE + 0x20)
#define MDP_DITHER_SIZE (MDP_DITHER_BASE + 0x30)
#define MDP_DITHER_SHADOW_CTRL (MDP_DITHER_BASE + 0xB0)

void dither_init(void);
void dither_reset(void);
void dither_start(void);
void dither_stop(void);
void dither_config(unsigned int width, unsigned int height,
		unsigned int dth_en, unsigned int algo,
		unsigned int src_fmt, unsigned int dst_fmt);

#endif
