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


#ifndef __MDP_WROT_H__
#define __MDP_WROT_H__

#include <linux/types.h>

extern void __iomem *mdp_wrot_base;

#define MDP_WROT_BASE mdp_wrot_base

#define MDP_VIDO_CTRL (MDP_WROT_BASE + 0x00)
#define MDP_VIDO_MAIN_BUF_SIZE (MDP_WROT_BASE + 0x08)
#define MDP_VIDO_SOFT_RST (MDP_WROT_BASE + 0x10)
#define MDP_VIDO_SOFT_RST_STAT (MDP_WROT_BASE + 0x14)
#define MDP_VIDO_INT_EN (MDP_WROT_BASE + 0x18)
#define MDP_VIDO_INT (MDP_WROT_BASE + 0x1c)
#define MDP_VIDO_CROP_OFST (MDP_WROT_BASE + 0x20)
#define MDP_VIDO_TAR_SIZE (MDP_WROT_BASE + 0x24)
#define MDP_VIDO_OFST_ADDR (MDP_WROT_BASE + 0x2c)
#define MDP_VIDO_STRIDE (MDP_WROT_BASE + 0x30)
#define MDP_VIDO_IN_SIZE (MDP_WROT_BASE + 0x78)
#define MDP_VIDO_ROT_EN (MDP_WROT_BASE + 0x7c)
#define MDP_VIDO_Y_MODE (MDP_WROT_BASE + 0x88)
#define MDP_VIDO_BASE_ADDR (MDP_WROT_BASE + 0xf00)

void wrot_init(void);
void wrot_reset(void);
void wrot_enable_irq(void);
void wrot_clear_irq(void);
void wrot_start(void);
void wrot_stop(void);
void wrot_config(u32 width, u32 height, u32 pitch, u32 fmt, u32 addr,
		 u32 rotate, u32 crop_type, u32 crop_x, u32 crop_y, u32 crop_w,
		 u32 crop_h);

#endif
