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
#include "mtk_mdp_rdma.h"
#include "mtk_mdp_base.h"

void __iomem *mdp_rdma_base;

void rdma_init(void)
{
	u32 phy_base;

	mdp_rdma_base = mdp_init_comp("mediatek,mt8512-mdp-rdma",
				       &phy_base);

	pr_info("rdma base %p(0x%X)\n", mdp_rdma_base, phy_base);
}

void rdma_reset(void)
{
	MDP_REG_MASK(MDP_RDMA_RESET, 0x1, 0x1);
	MDP_REG_MASK(MDP_RDMA_RESET, 0x0, 0x1);
}

void rdma_start(void)
{
	MDP_REG_MASK(MDP_RDMA_EN, 0x11, 0x7fffff);

	MDP_REG_MASK(MDP_RDMA_INT_EN, 0x7, 0x7);
	MDP_REG_MASK(MDP_RDMA_INT_STA, 0x0, 0x7);
}

void rdma_stop(void)
{
	MDP_LOG_INFO("%s\n", __func__);

	MDP_REG_MASK(MDP_RDMA_EN, 0x0, 0x7fffff);
}

void rdma_config(u32 width, u32 height, u32 pitch, u32 fmt, dma_addr_t addr,
		 u32 crop_type, u32 crop_x, u32 crop_y, u32 crop_w, u32 crop_h)
{
	MDP_LOG_INFO("RDMA: %dx%d pitch[%d] [%d] [%d %d %d %d] %s 0x%x\n",
		     width, height, pitch,
		     crop_type, crop_x, crop_y, crop_w, crop_h,
		     fmt_2_string(fmt), (unsigned int)addr);

	MDP_REG_MASK(MDP_RDMA_CON, 0x1010, 0x1010);
	MDP_REG_MASK(MDP_RDMA_MB_BASE, 0x0, 0xffff);
	MDP_REG_MASK(MDP_RDMA_SB_BASE, 0x1200, 0xffff);
	MDP_REG_MASK(MDP_RDMA_SRC_END_0, 0x0, 0xffffffff);
	MDP_REG_MASK(MDP_RDMA_SRC_OFFSET_0, 0x0, 0xffffffff);
	MDP_REG_MASK(MDP_RDMA_SRC_OFFSET_W_0, 0x0, 0xffff);
	MDP_REG_MASK(MDP_RDMA_SRC_OFFSET_0_P, 0x0, 0xffffffff);
	MDP_REG_MASK(MDP_RDMA_TRANSFORM_0, 0x0, 0x10000);

	//MDP_REG_SET(MDP_RDMA_MF_OFFSET_1, addr);

	if (crop_type == 1) {
		MDP_REG_MASK(MDP_RDMA_MF_CLIP_SIZE, crop_h << 16 | crop_w,
			     0x3fff3fff);
		MDP_REG_SET(MDP_RDMA_SRC_BASE_0, addr + crop_y * pitch + crop_x);
		//MDP_REG_SET(MDP_RDMA_SRC_BASE_0, addr);

		//only support Y8/Y4, not support package format and ARGB8888
		MDP_REG_MASK(MDP_RDMA_MF_SRC_SIZE, crop_h << 16 | crop_w, 0x3fff3fff);
	} else {
		MDP_REG_MASK(MDP_RDMA_MF_CLIP_SIZE, height << 16 | width,
			     0x3fff3fff);
		MDP_REG_SET(MDP_RDMA_SRC_BASE_0, addr);

		//only support Y8/Y4, not support package format and ARGB8888
		MDP_REG_MASK(MDP_RDMA_MF_SRC_SIZE, height << 16 | width, 0x3fff3fff);
	}

	MDP_REG_MASK(MDP_RDMA_MF_BKGD_SIZE_IN_BYTE, pitch, 0x1fffff);
	if (fmt == V4L2_PIX_FMT_Y8) {
		MDP_REG_MASK(MDP_RDMA_SRC_CON, 0x18127240 | 0x7, 0xffffffff);
		MDP_REG_MASK(MDP_RDMA_Y4_MODE_CFG, 0x0, 0xf);
	} else if (fmt == V4L2_PIX_FMT_Y4_M0) {
		MDP_REG_MASK(MDP_RDMA_SRC_CON, 0x18127240 | 0x7, 0xffffffff);
		MDP_REG_MASK(MDP_RDMA_Y4_MODE_CFG, 0x2, 0xf);
	} else {
		MDP_REG_MASK(MDP_RDMA_SRC_CON, 0x18127240 | 0x7, 0xffffffff);
		MDP_REG_MASK(MDP_RDMA_Y4_MODE_CFG, 0x0, 0xf);
	}

}

