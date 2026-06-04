// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek clock driver for MT8110 SoC
 *
 * Copyright (C) 2019 BayLibre, SAS
 * Author: Jiaguang Zhang <jiaguang.zhang@mediatek.com>
 */

#include <common.h>
#include <dm.h>
#include <asm/io.h>
#include <stdarg.h>
#include <linux/kernel.h>
#include "hwtcon.h"
#include "rdma_config.h"
#include "cmdq.h"
#include "gpio.h"
#include "bd71828.h"
#include "spm_mtcmos.h"

#include "v1/wf_lut_config.h"
#include "v1/pipeline_config.h"
#include "v2/wf_lut_config.h"
#include "v2/pipeline_config.h"

#include "hwtcon_hal.h"
#include "tcon_config.h"
#include "panel_setting.h"

const struct platform_info_struct *platform = NULL;

struct hwtcon_buffer_info g_buffer_info = {0};

bool check_update_param_valid(int x, int y, int w, int h, int waveform, int update_mode)
{
	if (x < 0 ||
		y < 0 ||
		w <= 0 ||
		h <= 0 ||
		(x + w) > platform->PANEL_WIDTH ||
		(y + h) > platform->PANEL_HEIGHT) {
		TCON_ERR("invalid region [%d %d %d %d]", x, y, w, h);
		return false;
	}

	if (waveform < 0 || waveform >= 12 ||
		update_mode < 0 || update_mode >= 2) {
		TCON_ERR("invalid waveform mode:%d update_mode:%d", waveform, update_mode);
		return false;
	}
	return true;
}

void hwtcon_update(int waveform_mode, int update_mode, struct pp_rect *region)
{
	if (hwtcon_get_hw_ver() != HW_VERSION_MT8113)
		return hwtcon_update_region_v1(waveform_mode, update_mode, region);
	else
		return hwtcon_update_region_v2(waveform_mode, update_mode, region);
}

void hwtcon_wait_for_framedone(void)
{
	if (hwtcon_get_hw_ver() != HW_VERSION_MT8113)
		return wf_lut_wait_for_framedone_v1();
	else
		return wf_lut_wait_end_all_irq();
}

//---------------------------------------------------------------------------
/*
 * update_screen.
 * x and y are the coordinates of the update region on the screen
 * w and h are the width and height of the update region
 * wave is the waveform used for the update
 * mode is the update mode, either full update or partial update
 */
void update_screen(int x, int y, int w, int h, int wave, int mode)
{
	struct pp_rect region = {0};

	if (check_update_param_valid(x, y, w, h, wave, mode) == false) {
		TCON_ERR("update_screen check param fail");
		return;
	}

	TCON_LOG("update_screen  x=%d, y=%d, w=%d, h=%d, wave=%d, mode=%d",
		x, y, w, h, wave, mode);
	region.rect_x = x;
	region.rect_y = y;
	region.rect_width = w;
	region.rect_height = h;

	hwtcon_update(wave, mode, &region);
}

int mtk_hwtcon_init(void)
{
	mt_set_gpio_mode(GPIO4, GPIO_MODE_01);
	mt_set_gpio_mode(GPIO109, GPIO_MODE_03);
	mt_set_gpio_mode(GPIO5, GPIO_MODE_01);
	mt_set_gpio_mode(GPIO108, GPIO_MODE_03);
	mt_set_gpio_mode(GPIO6, GPIO_MODE_01);
	mt_set_gpio_mode(GPIO105, GPIO_MODE_03);
	mt_set_gpio_mode(GPIO98, GPIO_MODE_03);
	mt_set_gpio_mode(GPIO106, GPIO_MODE_03);
	mt_set_gpio_mode(GPIO97, GPIO_MODE_03);
	mt_set_gpio_mode(GPIO87, GPIO_MODE_03);
	mt_set_gpio_mode(GPIO96, GPIO_MODE_03);
	mt_set_gpio_mode(GPIO2, GPIO_MODE_01);
	mt_set_gpio_mode(GPIO95, GPIO_MODE_03);
	mt_set_gpio_mode(GPIO3, GPIO_MODE_01);
	mt_set_gpio_mode(GPIO101, GPIO_MODE_03);
	mt_set_gpio_mode(GPIO92, GPIO_MODE_03);
	mt_set_gpio_mode(GPIO94, GPIO_MODE_03);
	mt_set_gpio_mode(GPIO99, GPIO_MODE_03);
	mt_set_gpio_mode(GPIO107, GPIO_MODE_03);
	mt_set_gpio_mode(GPIO110, GPIO_MODE_03);
	mt_set_gpio_mode(GPIO111, GPIO_MODE_03);
	mt_set_gpio_mode(GPIO100, GPIO_MODE_00);	/* XON, output high */
	/* for test */
	mt_set_gpio_mode(GPIO93, GPIO_MODE_00);		/* GDOE, output high */
	mt_set_gpio_mode(GPIO7, GPIO_MODE_00);  	/* SDOE GPIO, output high */
	
	mt_set_gpio_dir(GPIO7, GPIO_DIR_OUT);
	mt_set_gpio_dir(GPIO100, GPIO_DIR_OUT);
	mt_set_gpio_dir(GPIO93, GPIO_DIR_OUT);

	mt_set_gpio_out(GPIO7, GPIO_OUT_ONE);		/* SDOE high */
	mt_set_gpio_out(GPIO100, GPIO_OUT_ONE);		/* XON high */
	mt_set_gpio_out(GPIO93, GPIO_OUT_ONE);		/* GDOE high */

	return 0;
}

int hwtcon_pinmux_enable(bool enable)
{
	if (enable) {
		mt_set_gpio_mode(GPIO4, GPIO_MODE_01);
		mt_set_gpio_mode(GPIO109, GPIO_MODE_03);
		mt_set_gpio_mode(GPIO5, GPIO_MODE_01);
		mt_set_gpio_mode(GPIO108, GPIO_MODE_03);
		mt_set_gpio_mode(GPIO6, GPIO_MODE_01);
		mt_set_gpio_mode(GPIO105, GPIO_MODE_03);
		mt_set_gpio_mode(GPIO98, GPIO_MODE_03);
		mt_set_gpio_mode(GPIO106, GPIO_MODE_03);
		mt_set_gpio_mode(GPIO97, GPIO_MODE_03);
		mt_set_gpio_mode(GPIO87, GPIO_MODE_03);
		mt_set_gpio_mode(GPIO96, GPIO_MODE_03);
		mt_set_gpio_mode(GPIO2, GPIO_MODE_01);
		mt_set_gpio_mode(GPIO95, GPIO_MODE_03);
		mt_set_gpio_mode(GPIO3, GPIO_MODE_01);
		mt_set_gpio_mode(GPIO101, GPIO_MODE_03);
		mt_set_gpio_mode(GPIO92, GPIO_MODE_03);
		mt_set_gpio_mode(GPIO94, GPIO_MODE_03);
		mt_set_gpio_mode(GPIO99, GPIO_MODE_03);
		mt_set_gpio_mode(GPIO107, GPIO_MODE_03);
		mt_set_gpio_mode(GPIO110, GPIO_MODE_03);
		mt_set_gpio_mode(GPIO111, GPIO_MODE_03);
		//mt_set_gpio_mode(GPIO100, GPIO_MODE_00);	/* XON, output high */
		/* for test */
		mt_set_gpio_mode(GPIO93, GPIO_MODE_00);		/* GDOE, output high */
		mt_set_gpio_mode(GPIO7, GPIO_MODE_01);  	/* SDOE GPIO, output high */

		mt_set_gpio_dir(GPIO93, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO93, GPIO_OUT_ONE);		/* GDOE high */
	} else {
		mt_set_gpio_mode(GPIO93, GPIO_MODE_00); 	/* GDOE, output high */
		mt_set_gpio_dir(GPIO93, GPIO_DIR_IN);
		mt_set_gpio_mode(GPIO4, GPIO_MODE_00);
		mt_set_gpio_mode(GPIO109, GPIO_MODE_00);
		mt_set_gpio_mode(GPIO5, GPIO_MODE_00);
		mt_set_gpio_mode(GPIO108, GPIO_MODE_00);
		mt_set_gpio_mode(GPIO6, GPIO_MODE_00);
		mt_set_gpio_mode(GPIO105, GPIO_MODE_00);
		mt_set_gpio_mode(GPIO98, GPIO_MODE_00);
		mt_set_gpio_mode(GPIO106, GPIO_MODE_00);
		mt_set_gpio_mode(GPIO97, GPIO_MODE_00);
		mt_set_gpio_mode(GPIO87, GPIO_MODE_00);
		mt_set_gpio_mode(GPIO96, GPIO_MODE_00);
		mt_set_gpio_mode(GPIO2, GPIO_MODE_00);
		mt_set_gpio_mode(GPIO95, GPIO_MODE_00);
		mt_set_gpio_mode(GPIO3, GPIO_MODE_00);
		mt_set_gpio_mode(GPIO101, GPIO_MODE_00);
		mt_set_gpio_mode(GPIO92, GPIO_MODE_00);
		mt_set_gpio_mode(GPIO94, GPIO_MODE_00);
		mt_set_gpio_mode(GPIO99, GPIO_MODE_00);
		mt_set_gpio_mode(GPIO107, GPIO_MODE_00);
		mt_set_gpio_mode(GPIO110, GPIO_MODE_00);
		mt_set_gpio_mode(GPIO111, GPIO_MODE_00);
		//mt_set_gpio_mode(GPIO100, GPIO_MODE_00);	/* XON, output high */
		/* for test */
		mt_set_gpio_mode(GPIO7, GPIO_MODE_00);  	/* SDOE GPIO, output high */
	}

	return 0;
}

void pmic_control_init(bool power_on)
{
	TCON_LOG("pmic %s", power_on ? "power on" : "power off");
	if (power_on) {
		hwtcon_pinmux_enable(true);
		bd71828_enable_ldo(LDO4, 1);
		bd71828_set_gpio_epden(1);
	} else {
		bd71828_set_gpio_epden(0);
		bd71828_enable_ldo(LDO4, 0);
		hwtcon_pinmux_enable(false);
	}
}

int do_hwtcon_init(cmd_tbl_t *cmdtp, int flag, int argc,
						char *const argv[])
{
	int project_name = 3;

	project_name = simple_strtol(argv[1], NULL, 0);
	switch (project_name) {
	case 1:
		platform = &panel_1448_1072_info;
		break;
	case 2:
		platform = &panel_1264_1680_info;
		break;
	case 3:
		platform = &panel_1648_1236_info;
		break;
	default:
		TCON_ERR("invalid project name:%s", argv[1]);
		return;
	}

	g_buffer_info.wf_file_buffer = simple_strtol(argv[2], NULL, 0);
	g_buffer_info.image_buffer = simple_strtol(argv[3], NULL, 0);
	g_buffer_info.wb_buffer_0 = simple_strtol(argv[4], NULL, 0);
	g_buffer_info.wb_buffer_1 = simple_strtol(argv[4], NULL, 0);

	TCON_ERR("set buffer info wf_file[0x%08x] img[0x%08x] wb[0x%08x]",
		g_buffer_info.wf_file_buffer,
		g_buffer_info.image_buffer,
		g_buffer_info.wb_buffer_1);

	spm_mtcmos_ctrl_mm(STA_POWER_ON);
	spm_mtcmos_ctrl_img(STA_POWER_ON);
	#if 0
	pp_write(MMSYS_CG_CLR0, 0x03f800bf);
	pp_write(IMGSYS_CG_CLR0, 0x00a18935);
	#else
	pp_write(NULL,MMSYS_CG_CON0, 0);
	pp_write(NULL, IMGSYS_CG_CON0, 0);
	#endif

	pmic_control_init(false);
	pmic_control_init(true);

	rdma_config_smi_setting(NULL);
	if (hwtcon_get_hw_ver() != HW_VERSION_MT8113) {
		/* this feature not support MT8110, continue use hard code */
		g_buffer_info.wf_file_buffer = 0x50000000;
		g_buffer_info.image_buffer = 0x51000000;
		g_buffer_info.wb_buffer_0 = 0x52000000;
		g_buffer_info.wb_buffer_1 = 0x53000000;

		/* config timing */
		pp_func_init_pipeline_and_dpi_setting(g_buffer_info.image_buffer,
			g_buffer_info.wb_buffer_0,
			g_buffer_info.wb_buffer_1,
			g_buffer_info.wf_file_buffer,
			platform->PANEL_WIDTH, platform->PANEL_HEIGHT,
			MAIN_SOF_MODE_IMG_LAST_UPDATE_AND_DPI_VSYNC, WF_LUT_MOUT_DPI);
		/* init slot */
		wf_lut_waveform_day_mode_slot_v1(NULL);
	} else {
		wf_lut_config_context_init_for_pipeline();
		wf_lut_waveform_day_mode_slot_v2(NULL);
	}
	TCON_LOG("config timing done");
	return 0;
}

int do_hwtcon_destroy(cmd_tbl_t *cmdtp, int flag, int argc,
						char *const argv[])
{
	/* fiti power off */
	pmic_control_init(false);
}

int do_showlogo(cmd_tbl_t *cmdtp, int flag, int argc,
						char *const argv[])
{
    int x;
	int y;
	int w; 
	int h;
	int wave;
	int mode;

    x = simple_strtol(argv[1], NULL, 0);
	y = simple_strtol(argv[2], NULL, 0);
	w = simple_strtol(argv[3], NULL, 0);
	h = simple_strtol(argv[4], NULL, 0);
	wave = simple_strtol(argv[5], NULL, 0);
	mode = simple_strtol(argv[6], NULL, 0);

	update_screen(x, y, w, h, wave, mode);

	return 0;
}

U_BOOT_CMD(showlogo, 7,	1,	do_showlogo,
	"show logo",
	"<x> <y> <w> <h> <wave> <mode>\n"
	"	 x and y are the coordinates of the update region on the screen\n"
	"	 w and h are the width and height of the update region\n"
	"	 wave is the waveform used for the update\n"
	"	 mode is the update mode, either full update or partial update"
);

U_BOOT_CMD(hwtcon_init, 5, 1,	do_hwtcon_init,
	"hwtcon_init",
	"do hwtcon power on & timing config\n"
	"  usage: hwtcon_init <platform> <wf_file_addr> <img_addr> <wb_addr>\n"
	"  platform: 1/2/3"
);

U_BOOT_CMD(hwtcon_destroy, 1, 1,	do_hwtcon_destroy,
	"hwtcon_destroy",
	"do hwtcon power off\n"
);
