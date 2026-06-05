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
#include "mtk_mdp_wrot.h"
#include "mtk_mdp_base.h"

void __iomem *mdp_wrot_base;

void wrot_init(void)
{
	u32 phy_base;

	mdp_wrot_base = mdp_init_comp("mediatek,mt8512-mdp-wrot",
				       &phy_base);

	pr_info("wrot base %p(0x%X)\n", mdp_wrot_base, phy_base);
}

void wrot_reset(void)
{
	MDP_REG_MASK(MDP_VIDO_SOFT_RST, 0x1, 0x1);
	MDP_REG_MASK(MDP_VIDO_SOFT_RST, 0x0, 0x1);
}

void wrot_enable_irq(void)
{
	MDP_REG_MASK(MDP_VIDO_INT_EN, 0x7, 0x7);
}

void wrot_clear_irq(void)
{
	MDP_LOG_INFO("%s status 0x%X\n", __func__, DISP_REG_GET(MDP_VIDO_INT));

	MDP_REG_MASK(MDP_VIDO_INT, 0x7, 0x7);

	MDP_LOG_INFO("%s done  0x%X\n", __func__, DISP_REG_GET(MDP_VIDO_INT));
}

void wrot_start(void)
{
	MDP_LOG_INFO("%s\n", __func__);

	MDP_REG_MASK(MDP_VIDO_ROT_EN, 0x10001, 0x10001);

	//MDP_REG_MASK(MDP_VIDO_INT_EN, 0x7, 0x7);
	//MDP_REG_MASK(MDP_VIDO_INT, 0x0, 0x7);
}

void wrot_stop(void)
{
	MDP_LOG_INFO("%s\n", __func__);

	MDP_REG_MASK(MDP_VIDO_ROT_EN, 0x0, 0x10001);
}

void wrot_config(u32 width, u32 height, u32 pitch, u32 fmt, u32 addr,
		 u32 rotate, u32 crop_type, u32 crop_x, u32 crop_y, u32 crop_w,
		 u32 crop_h)
{
	int ofs = 0;
	int addr_ofs = 0;

	MDP_LOG_INFO("WROT: %dx%d pitch[%d] [%d] [%d %d %d %d] %s 0x%x [%d]\n",
		     width, height, pitch,
		     crop_type, crop_x, crop_y, crop_w, crop_h,
		     fmt_2_string(fmt), (unsigned int)addr, rotate);

	MDP_REG_MASK(MDP_VIDO_CTRL, 0xA0001007, 0xFFFFFFFF);
	MDP_REG_MASK(MDP_VIDO_CROP_OFST, 0x0, 0xffffffff);

	if (crop_type == 2) {
		MDP_REG_SET(MDP_VIDO_MAIN_BUF_SIZE, 0x2000 | (crop_w << 16));
		MDP_REG_SET(MDP_VIDO_CROP_OFST, (crop_y << 16) | crop_x);
		MDP_REG_SET(MDP_VIDO_TAR_SIZE, (crop_h << 16) | crop_w);
	}

	if (rotate == 0) {
		MDP_REG_MASK(MDP_VIDO_MAIN_BUF_SIZE, 0x2000 | (crop_w << 16),
			     0xffffffff);
		MDP_REG_MASK(MDP_VIDO_IN_SIZE, (crop_h << 16) | crop_w, 0x3fff3fff);
		MDP_REG_MASK(MDP_VIDO_TAR_SIZE, (crop_h << 16) | crop_w, 0x3fff3fff);
		addr_ofs = crop_y * pitch + crop_x;
	} else if (rotate == 90) {
		MDP_REG_MASK(MDP_VIDO_MAIN_BUF_SIZE, 0x2000 | (width << 16),
			     0xffffffff);
		MDP_REG_MASK(MDP_VIDO_IN_SIZE, (height << 16) | width, 0x3fff3fff);
		MDP_REG_MASK(MDP_VIDO_TAR_SIZE, (height << 16) | width, 0x3fff3fff);
		MDP_REG_MASK(MDP_VIDO_CTRL, 0xA0101007, 0xffffffff);
		ofs = height - 1;
	} else if (rotate == 180) {
		MDP_REG_MASK(MDP_VIDO_MAIN_BUF_SIZE, 0x2000 | (crop_w << 16),
			     0xffffffff);
		MDP_REG_MASK(MDP_VIDO_IN_SIZE, (crop_h << 16) | crop_w, 0x3fff3fff);
		MDP_REG_MASK(MDP_VIDO_TAR_SIZE, (crop_h << 16) | crop_w, 0x3fff3fff);
		MDP_REG_MASK(MDP_VIDO_CTRL, 0xA0201007, 0xffffffff);
		addr_ofs = (height - crop_y - 1) * pitch + (width - crop_x - 1);
	} else if (rotate == 270) {
		MDP_REG_MASK(MDP_VIDO_MAIN_BUF_SIZE, 0x2000 | (width << 16),
			     0xffffffff);
		MDP_REG_MASK(MDP_VIDO_IN_SIZE, (height << 16) | width, 0x3fff3fff);
		MDP_REG_MASK(MDP_VIDO_TAR_SIZE, (height << 16) | width, 0x3fff3fff);
		MDP_REG_MASK(MDP_VIDO_CTRL, 0xA0301007, 0xffffffff);
	}
	MDP_REG_MASK(MDP_VIDO_BASE_ADDR, addr + addr_ofs, 0xffffffff);
	MDP_REG_MASK(MDP_VIDO_OFST_ADDR, ofs, 0xfffffff);
	if (fmt == V4L2_PIX_FMT_Y8) {
		MDP_REG_MASK(MDP_VIDO_Y_MODE, 0x0, 0xf);
		MDP_REG_MASK(MDP_VIDO_STRIDE, pitch, 0x3fff);
	} else if (fmt == V4L2_PIX_FMT_Y4_M0) {
		MDP_REG_MASK(MDP_VIDO_Y_MODE, 0x1, 0xf);
		MDP_REG_MASK(MDP_VIDO_STRIDE, pitch, 0x3fff);
	} else {
		MDP_REG_MASK(MDP_VIDO_Y_MODE, 0x0, 0xf);
		MDP_REG_MASK(MDP_VIDO_STRIDE, pitch, 0x3fff);
	}

	if (crop_type == 2)
		MDP_REG_MASK(MDP_VIDO_STRIDE, crop_w, 0x3fff);
}

