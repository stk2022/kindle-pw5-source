/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2018 MediaTek Inc.
 * Author: Jiaguang Zhang <jiaguang.zhang@mediatek.com>
 */
#ifndef __PANEL_SETTING_H__
#define __PANEL_SETTING_H__
#include "hwtcon_def.h"

struct platform_info_struct {
	u32 clock_setting;

	u32 PANEL_WIDTH;
	u32 PANEL_HEIGHT;
	u32 PANEL_8_BIT;

	/* sdce */
	u32 TIME0_HS;
	u32 TIME0_HE;
	u32 TIME0_VS;
	u32 TIME0_VE;
	u32 TIME0_INV;

	/* sdle */
	u32 TIME1_HS;
	u32 TIME1_HE;
	u32 TIME1_VS;
	u32 TIME1_VE;
	u32 TIME1_INV;

	/* time 2 sdoe all high */
	u32 TIME2_HS;
	u32 TIME2_HE;
	u32 TIME2_VS;
	u32 TIME2_VE;
	u32 TIME2_INV;
	u32 TIME2_HSPLCNT;
	u32 TIME2_VACTSEL;
	u32 TIME2_TCOPR;

	/* gdck */
	u32 TIME3_HS;
	u32 TIME3_HE;
	u32 TIME3_VS;
	u32 TIME3_VE;
	u32 TIME3_INV;
	
	/* time 4 gdoe all high */

	/* gdsp */
	u32 TIME5_HS;
	u32 TIME5_HE;
	u32 TIME5_VS;
	u32 TIME5_VE;
	u32 TIME5_HSPLCNT;
	u32 TIME5_VACTSEL;
	u32 TIME5_INV;
	u32 TIME5_TCOPR;
	
	u32 DPI_HSA;
	u32 DPI_HFP;
	u32 DPI_HBP;
	u32 DPI_VSA;
	u32 DPI_VFP;
	u32 DPI_VBP;
	u32 DPI_CK_POL;
};

extern const struct platform_info_struct panel_1264_1680_info;
extern const struct platform_info_struct panel_1448_1072_info;
extern const struct platform_info_struct panel_1648_1236_info;

extern const struct platform_info_struct *platform;

#endif
