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


#ifndef __MDP_RDMA_H__
#define __MDP_RDMA_H__

#include <linux/types.h>

extern void __iomem *mdp_rdma_base;

#define MDP_RDMA_BASE mdp_rdma_base

#define MDP_RDMA_EN (MDP_RDMA_BASE + 0x00)
#define MDP_RDMA_RESET (MDP_RDMA_BASE + 0x08)
#define MDP_RDMA_INT_EN (MDP_RDMA_BASE + 0x10)
#define MDP_RDMA_INT_STA (MDP_RDMA_BASE + 0x18)
#define MDP_RDMA_CON (MDP_RDMA_BASE + 0x20)
#define MDP_RDMA_SRC_CON (MDP_RDMA_BASE + 0x30)
#define MDP_RDMA_Y4_MODE_CFG (MDP_RDMA_BASE + 0x38)
#define MDP_RDMA_MF_BKGD_SIZE_IN_BYTE (MDP_RDMA_BASE + 0x60)
#define MDP_RDMA_MF_SRC_SIZE (MDP_RDMA_BASE + 0x70)
#define MDP_RDMA_MF_CLIP_SIZE (MDP_RDMA_BASE + 0x78)
#define MDP_RDMA_MF_OFFSET_1 (MDP_RDMA_BASE + 0x80)
#define MDP_RDMA_MB_BASE (MDP_RDMA_BASE + 0xC8)
#define MDP_RDMA_SB_BASE (MDP_RDMA_BASE + 0xE0)
#define MDP_RDMA_SRC_END_0 (MDP_RDMA_BASE + 0x100)
#define MDP_RDMA_SRC_OFFSET_0 (MDP_RDMA_BASE + 0x118)
#define MDP_RDMA_SRC_OFFSET_W_0 (MDP_RDMA_BASE + 0x130)
#define MDP_RDMA_SRC_OFFSET_0_P (MDP_RDMA_BASE + 0x148)
#define MDP_RDMA_TRANSFORM_0 (MDP_RDMA_BASE + 0x200)
#define MDP_RDMA_SRC_BASE_0 (MDP_RDMA_BASE + 0xF00)

void rdma_init(void);
void rdma_reset(void);
void rdma_start(void);
void rdma_stop(void);
void rdma_config(u32 width, u32 height, u32 pitch, u32 fmt, dma_addr_t addr,
		 u32 crop_type, u32 crop_x, u32 crop_y, u32 crop_w, u32 crop_h);

#endif
