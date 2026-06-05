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
#include "hwtcon_epd.h"
#include "hwtcon_driver.h"
#include "hwtcon_debug.h"
#include <linux/kernel.h>
#include <linux/module.h>


#define HWTCON_EPD_DRIVER_NAME "epd"

struct edp_driver g_edp_driver;

int hwtcon_epd_init_device_info(struct device_node *node)
{
	of_property_read_u32(node, "panel_dpi_clk",
		&g_edp_driver.panel_dpi_clk);
	of_property_read_u32(node, "panel_width",
		&g_edp_driver.panel_width);
	of_property_read_u32(node, "panel_height",
		&g_edp_driver.panel_height);
	of_property_read_u32(node, "panel_area_w",
		&g_edp_driver.panel_area_width);
	of_property_read_u32(node, "panel_area_h",
		&g_edp_driver.panel_area_height);
	of_property_read_u32(node, "panel_waveform_type",
		&g_edp_driver.panel_waveform_type);
	of_property_read_u32(node, "panel_output_8bit",
		&g_edp_driver.panel_output_8bit);
	of_property_read_u32_array(node, "panel_tcon0",
		(unsigned int *)&g_edp_driver.panel_tcon[0], 8);
	of_property_read_u32_array(node, "panel_tcon1",
		(unsigned int *)&g_edp_driver.panel_tcon[1], 8);
	of_property_read_u32_array(node, "panel_tcon2",
		(unsigned int *)&g_edp_driver.panel_tcon[2], 8);
	of_property_read_u32_array(node, "panel_tcon3",
		(unsigned int *)&g_edp_driver.panel_tcon[3], 8);
	of_property_read_u32_array(node, "panel_tcon4",
		(unsigned int *)&g_edp_driver.panel_tcon[4], 8);
	of_property_read_u32_array(node, "panel_tcon5",
		(unsigned int *)&g_edp_driver.panel_tcon[5], 8);
	of_property_read_u32_array(node, "panel_dpi",
		(unsigned int *)&g_edp_driver.panel_dpi, 7);

	if (of_property_read_u32(node, "display_width", &g_edp_driver.display_width) ||
			g_edp_driver.display_width > g_edp_driver.panel_width) {
		g_edp_driver.display_width = g_edp_driver.panel_width;
	}
	if (of_property_read_u32(node, "display_height", &g_edp_driver.display_height) ||
			g_edp_driver.display_height > g_edp_driver.panel_height) {
		g_edp_driver.display_height = g_edp_driver.panel_height;
	}
	g_edp_driver.panel_rotate = 0;
	of_property_read_u32(node, "panel_rotate", &g_edp_driver.panel_rotate);
	g_edp_driver.panel_rotate %= 4;

	g_edp_driver.modify_wf_mode_vcounter = 0x400;
	of_property_read_u32(node, "modify_wf_mode_counter",
		&g_edp_driver.modify_wf_mode_vcounter);

	g_edp_driver.panel_material_type = 0x4;
	of_property_read_u32(node, "panel_material_type",
		&g_edp_driver.panel_material_type);

	TCON_LOG("dpi_clk:%d,width:%d,height:%d,waveform_type:%d",
		g_edp_driver.panel_dpi_clk, g_edp_driver.panel_width,
		g_edp_driver.panel_height, g_edp_driver.panel_waveform_type);

	TCON_LOG("panel_material_type:%d",
		g_edp_driver.panel_material_type);

	TCON_LOG("panel_output_8bit:%d", g_edp_driver.panel_output_8bit);

	TCON_LOG("tcon0_VS:%d,tcon0_VE:%d,tcon0_HS:%d,tcon0_HE:%d",
		g_edp_driver.panel_tcon[0].VS, g_edp_driver.panel_tcon[0].VE,
		g_edp_driver.panel_tcon[0].HS, g_edp_driver.panel_tcon[0].HE);

	TCON_LOG("tcon0_TCOPR:%d,INV:%d,VACTSEL:%d,HSPLCNT:%d",
		g_edp_driver.panel_tcon[0].INV, g_edp_driver.panel_tcon[0].INV,
		g_edp_driver.panel_tcon[0].VACTSEL,
		g_edp_driver.panel_tcon[0].HSPLCNT);

	TCON_LOG("tcon1_VS:%d,tcon1_VE:%d,tcon1_HS:%d,tcon1_HE:%d",
		g_edp_driver.panel_tcon[1].VS, g_edp_driver.panel_tcon[1].VE,
		g_edp_driver.panel_tcon[1].HS, g_edp_driver.panel_tcon[1].HE);

	TCON_LOG("tcon1_TCOPR:%d,INV:%d,VACTSEL:%d,HSPLCNT:%d",
		g_edp_driver.panel_tcon[1].INV, g_edp_driver.panel_tcon[1].INV,
		g_edp_driver.panel_tcon[1].VACTSEL,
		g_edp_driver.panel_tcon[1].HSPLCNT);

	TCON_LOG("HSA:%d,HFP:%d,HBP:%d,VSA:%d,VFP:%d,VBP:%d,CK_POL:%d",
		g_edp_driver.panel_dpi.HSA, g_edp_driver.panel_dpi.HFP,
		g_edp_driver.panel_dpi.HBP, g_edp_driver.panel_dpi.VSA,
		g_edp_driver.panel_dpi.VFP, g_edp_driver.panel_dpi.VBP,
		g_edp_driver.panel_dpi.CK_POL);
	return 0;
}


u32 hw_tcon_get_edp_clk(void)
{
	return g_edp_driver.panel_dpi_clk;
}

u32 hw_tcon_get_edp_width(void)
{
	return g_edp_driver.panel_width;
}

u32 hw_tcon_get_edp_height(void)
{
	return g_edp_driver.panel_height;
}

u32 hw_tcon_get_display_width(void)
{
	return g_edp_driver.display_width;
}

u32 hw_tcon_get_display_height(void)
{
	return g_edp_driver.display_height;
}

u32 hw_tcon_get_edp_area_width(void)
{
	return g_edp_driver.panel_area_width;
}

u32 hw_tcon_get_edp_area_height(void)
{
	return g_edp_driver.panel_area_height;
}

u32 hw_tcon_get_edp_rotate(void)
{
	return g_edp_driver.panel_rotate;
}

u32 hw_tcon_get_edp_out_8bit(void)
{
	return g_edp_driver.panel_output_8bit;
}

u32 hw_tcon_get_edp_material_type(void)
{
	return g_edp_driver.panel_material_type;
}

u32 hw_tcon_get_wf_lut_modify_vcounter(void)
{
	return g_edp_driver.modify_wf_mode_vcounter;
}

struct dpi_timing_para *hw_tcon_get_edp_dpi_para(void)
{
	return &g_edp_driver.panel_dpi;
}

struct tcon_timing_para *hw_tcon_get_edp_tcon0_para(void)
{
	return &g_edp_driver.panel_tcon[0];
}

struct tcon_timing_para *hw_tcon_get_edp_tcon1_para(void)
{
	return &g_edp_driver.panel_tcon[1];
}

struct tcon_timing_para *hw_tcon_get_edp_tcon2_para(void)
{
	return &g_edp_driver.panel_tcon[2];
}

struct tcon_timing_para *hw_tcon_get_edp_tcon3_para(void)
{
	return &g_edp_driver.panel_tcon[3];
}

struct tcon_timing_para *hw_tcon_get_edp_tcon4_para(void)
{
	return &g_edp_driver.panel_tcon[4];
}

struct tcon_timing_para *hw_tcon_get_edp_tcon5_para(void)
{
	return &g_edp_driver.panel_tcon[5];
}

