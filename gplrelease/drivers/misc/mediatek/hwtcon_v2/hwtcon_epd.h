/*****************************************************************************
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * Accelerometer Sensor Driver
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 *****************************************************************************/

#ifndef __HWTCON_EPD_H__
#define __HWTCON_EPD_H__
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>

#define MMSYS_CLK_RATE 312000000
#define VCORE_VOLTAGE 700000

enum GRAY_MODE_ENUM {
	GRAY_MODE_2_GRAY_LEVEL = 0,
	GRAY_MODE_4_GRAY_LEVEL = 1,
	GRAY_MODE_8_GRAY_LEVEL = 2,
	GRAY_MODE_16_GRAY_LEVEL = 3,
	GRAY_MODE_32_GRAY_LEVEL = 4,
};

enum PANEL_LIST_ENUM {
	PANEL_ED060_1448_1072_8BIT = 0,
	PANEL_ED070_1264_1680_16BIT = 1,
	PANEL_ED060_800_600_8BIT = 2,
	PANEL_LIST_ENUM_NUM,
};


struct tcon_timing_para {
	unsigned int VS;
	unsigned int VE;
	unsigned int HS;
	unsigned int HE;
	unsigned int TCOPR;
	unsigned int INV;
	unsigned int VACTSEL;
	unsigned int HSPLCNT;
};

struct dpi_timing_para {
	unsigned int HSA;
	unsigned int HFP;
	unsigned int HBP;
	unsigned int VSA;
	unsigned int VFP;
	unsigned int VBP;
	unsigned int CK_POL;
};

struct edp_driver {
	u32 panel_dpi_clk;
	u32 panel_width;
	u32 panel_height;
	u32 display_width;
	u32 display_height;
	u32 panel_area_width;
	u32 panel_area_height;
	u32 panel_waveform_type;
	u32 panel_output_8bit;
	u32 panel_material_type;
	u32 panel_rotate;
	u32 modify_wf_mode_vcounter;
	struct tcon_timing_para panel_tcon[6];
	struct dpi_timing_para panel_dpi;
};

u32 hw_tcon_get_edp_material_type(void);
u32 hw_tcon_get_wf_lut_modify_vcounter(void);
u32 hw_tcon_get_edp_clk(void);
u32 hw_tcon_get_edp_width(void);
u32 hw_tcon_get_edp_height(void);
u32 hw_tcon_get_display_width(void);
u32 hw_tcon_get_display_height(void);
u32 hw_tcon_get_edp_area_width(void);
u32 hw_tcon_get_edp_area_height(void);
u32 hw_tcon_get_edp_rotate(void);
u32 hw_tcon_get_edp_out_8bit(void);
u32 hw_tcon_get_edp_blank_vtotal(void);
u32 hw_tcon_get_edp_dpi_cnt_off(void);

struct dpi_timing_para *hw_tcon_get_edp_dpi_para(void);
struct tcon_timing_para *hw_tcon_get_edp_tcon0_para(void);
struct tcon_timing_para *hw_tcon_get_edp_tcon1_para(void);
struct tcon_timing_para *hw_tcon_get_edp_tcon2_para(void);
struct tcon_timing_para *hw_tcon_get_edp_tcon3_para(void);
struct tcon_timing_para *hw_tcon_get_edp_tcon4_para(void);
struct tcon_timing_para *hw_tcon_get_edp_tcon5_para(void);
int hwtcon_epd_init_device_info(struct device_node *node);

void hw_tcon_set_edp_width(u32 width);
void hw_tcon_set_edp_height(u32 height);


#endif
