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

#include "hwtcon_wf_lut_config.h"
#include "hwtcon_hal.h"
#include "hwtcon_dpi_config.h"
#include "hwtcon_wf_lut_rdma_config.h"
#include "hwtcon_tcon_config.h"
#include "hwtcon_fb.h"
#include "hwtcon_rdma_config.h"
#include "hwtcon_def.h"
#include "hwtcon_epd.h"
#include "fiti_core.h"
#include "hwtcon_core.h"

// #define WF_WDMA_SUPPORT      1
struct pinctrl *g_pctrl;
struct pinctrl_state *g_pin_state_active;
struct pinctrl_state *g_pin_state_inactive;

unsigned int g_current_waveform_mode_in_HW[8] = {0, 1, 2, 3, 4, 5, 6, 7};
int g_current_temperature;
int g_night_mode;
int g_ts_threshold[TEMPERATURE_NUM];

#ifdef WF_WDMA_SUPPORT
unsigned int g_wdma_addr = 0x5A000000;
void Wf_Lut_Wdma_addr(struct cmdqRecStruct *pkt, unsigned int addr)
{
	pp_write(pkt, WDMA_DST_ADDR0, addr);	//WDMA_DST_ADDR0
}

void Wf_Lut_Wdma_Config(struct cmdqRecStruct *pkt,
			unsigned int width, unsigned int height)
{
	pp_write_mask(pkt, DISP_WDMA0_SEL_IN, 0x1, BIT_MASK(0));
	pp_write(pkt, WDMA_INTEN, 0x00000001);
	pp_write(pkt, WDMA_CFG, 0x02020030);
	pp_write(pkt, WDMA_DST_W_IN_BYTE, 4 * width);
	pp_write(pkt, WDMA_DST_UV_PITCH, width);
	pp_write(pkt, WDMA_SRC_SIZE, height << 16 | width);
	pp_write(pkt, WDMA_CLIP_SIZE, height << 16 | width);
	pp_write(pkt, WDMA_CLIP_COORD, 0x00000000);
	Wf_Lut_Wdma_addr(pkt, hwtcon_fb_info()->swdata_pa);
	pp_write(pkt, WDMA_EN, 0x00000001);
}
#endif

static struct wf_lut_waveform
g_waveform_table[TEMPERATURE_NUM][WAVEFORM_MODE_TOTAL_NUM];

void wf_lut_config_mmsys(struct cmdqRecStruct *pkt,
			 struct wf_lut_con_config *wf_lut_config)
{
	if (wf_lut_config->rg_8b_out) {
		/*pmic control */
		//pp_write(pkt, MMSYS_DUMMY1, 0x0000007F);
		pp_write(pkt, MMSYS_MDP_DL_CFG_WD,
			 (wf_lut_config->height << 16) |
			 (wf_lut_config->width / 4));
	} else {
		//pp_write(pkt, MMSYS_DUMMY1, 0x0000003F);
		pp_write(pkt, MMSYS_MDP_DL_CFG_WD,
			 (wf_lut_config->height << 16) |
			 (wf_lut_config->width / 8));
	}
}

void wf_lut_config_lut_enable(struct cmdqRecStruct *pkt,
		unsigned int lut0_value,
		unsigned int lut1_value)
{
	pp_write(pkt, WF_LUT_EN_0, lut0_value);	//WF_LUT_EN_0-31 enable lut64
	pp_write(pkt, WF_LUT_EN_1, lut1_value);	//WF_LUT_EN_32-63 enable lut64
}


void wf_lut_config_waveform(struct cmdqRecStruct *pkt,
			    struct wf_lut_waveform *waveform)
{
	int i = 0;
	struct wf_lut_waveform *current_waveform = waveform;

	for (i = 0; i < WAVEFORM_MODE_NUM; i++) {
		switch (current_waveform->waveform_mode) {
		case WAVEFORM_MODE_INIT:
			pp_write(pkt, WF_LUT_ADDR_0,
				 current_waveform->start_addr);
			pp_write(pkt, WF_LUT_LEN_0, current_waveform->len);
			break;
		case WAVEFORM_MODE_DU:
			pp_write(pkt, WF_LUT_ADDR_1,
				 current_waveform->start_addr);
			pp_write(pkt, WF_LUT_LEN_1, current_waveform->len);
			break;
		case WAVEFORM_MODE_GC16:
			pp_write(pkt, WF_LUT_ADDR_2,
				 current_waveform->start_addr);
			pp_write(pkt, WF_LUT_LEN_2, current_waveform->len);
			break;
		case WAVEFORM_MODE_GL16:
			pp_write(pkt, WF_LUT_ADDR_3,
				 current_waveform->start_addr);
			pp_write(pkt, WF_LUT_LEN_3, current_waveform->len);
			break;
		case WAVEFORM_MODE_GLR16:
			pp_write(pkt, WF_LUT_ADDR_4,
				 current_waveform->start_addr);
			pp_write(pkt, WF_LUT_LEN_4, current_waveform->len);
			break;
		case WAVEFORM_MODE_GLD16:
			pp_write(pkt, WF_LUT_ADDR_5,
				 current_waveform->start_addr);
			pp_write(pkt, WF_LUT_LEN_5, current_waveform->len);
			break;
		case WAVEFORM_MODE_DU4:
			pp_write(pkt, WF_LUT_ADDR_6,
				 current_waveform->start_addr);
			pp_write(pkt, WF_LUT_LEN_6, current_waveform->len);
			break;
		case WAVEFORM_MODE_A2:
			pp_write(pkt, WF_LUT_ADDR_7,
				 current_waveform->start_addr);
			pp_write(pkt, WF_LUT_LEN_7, current_waveform->len);
			break;
		default:
			break;
		}
		TCON_ERR("i:%d, addr:%08x", i, current_waveform->start_addr);
		current_waveform = current_waveform + 1;
	}
}

/*below is for manual setting*/
void Wf_Lut_Mode_Select_Config(struct cmdqRecStruct *pkt,
			       unsigned int value1, unsigned int value2,
			       unsigned int value3, unsigned int value4,
			       unsigned int value5, unsigned int value6,
			       unsigned int value7, unsigned int value8)
{
	/* lut 0-8 select mode bit0-2 select mode */
	pp_write(pkt, WF_MODE_SEL_0, value1);
	pp_write(pkt, WF_MODE_SEL_1, value2);
	pp_write(pkt, WF_MODE_SEL_2, value3);
	pp_write(pkt, WF_MODE_SEL_3, value4);
	pp_write(pkt, WF_MODE_SEL_4, value5);
	pp_write(pkt, WF_MODE_SEL_5, value6);
	pp_write(pkt, WF_MODE_SEL_6, value7);
	/*lut55-63 select mode bit0-2 select mode */
	pp_write(pkt, WF_MODE_SEL_7, value8);

}


void wf_lut_config_common(struct cmdqRecStruct *pkt,
			  struct wf_lut_con_config *wf_lut_config)
{
	pp_write(pkt, WF_LUT_ROI_SIZE,
		 (wf_lut_config->height << 16) | wf_lut_config->width);
	pp_write(pkt, WF_LUT_SRC_CON, wf_lut_config->rdma_enable_mask & 0xf);

	pp_write(pkt, WF_LUT_DATAPATH_CON, 0x40000091);
	pp_write_mask(pkt, WF_LUT_DATAPATH_CON,
		      wf_lut_config->layer_greq_num << 26, GENMASK(31, 26));
	pp_write_mask(pkt, WF_LUT_DATAPATH_CON,
		      wf_lut_config->checksum_sel << 8, GENMASK(11, 8));
	pp_write_mask(pkt, WF_LUT_DATAPATH_CON,
		      wf_lut_config->rg_lut_end_sel << 7, BIT_MASK(7));
	pp_write_mask(pkt, WF_LUT_DATAPATH_CON,
		      wf_lut_config->rg_de_sel << 6, BIT_MASK(6));
	pp_write_mask(pkt, WF_LUT_DATAPATH_CON,
		      wf_lut_config->checksum_en << 4, BIT_MASK(4));
	pp_write_mask(pkt, WF_LUT_DATAPATH_CON,
		      wf_lut_config->layer_smi_id_en, BIT_MASK(0));
	/* pp_write(pkt, WF_LUT_DATAPATH_CON, 0x40000891); */

}

void wf_lut_config_inter_rdma(struct cmdqRecStruct *pkt,
			      struct wf_lut_con_config *wf_lut_config)
{
	unsigned int rdma_control_value = 0x0;

	/* bit8 need set to 1 */
	rdma_control_value = (wf_lut_config->byte_swap << 24) |
	    (wf_lut_config->DECFMT << 12) |
	    (wf_lut_config->H_FLIP_EN << 10) |
	    (wf_lut_config->V_FLIP_EN << 9) | 0x00000100;

	pp_write(pkt, WF_LUT_L0_CON, rdma_control_value);
	pp_write(pkt, WF_LUT_L1_CON, rdma_control_value);
	pp_write(pkt, WF_LUT_L2_CON, rdma_control_value);
	pp_write(pkt, WF_LUT_L3_CON, rdma_control_value);

	if (!wf_lut_config->direct_link) {
		pp_write(pkt, WF_LUT_L0_SRC_SIZE,
			 wf_lut_config->wb_rdma[0].
			 height << 16 | wf_lut_config->wb_rdma[0].width);
		pp_write(pkt, WF_LUT_L0_OFFSET,
			 wf_lut_config->wb_rdma[0].y << 16 | wf_lut_config->
			 wb_rdma[0].x);
		#if 0
		pp_write(pkt, WF_LUT_L0_ADDR,
			 wf_lut_config->wb_rdma[0].start_addr);
		#else
		pp_write(pkt, WF_LUT_L0_ADDR, hwtcon_fb_info()->wb_pa[0]);
		#endif
	}

	pp_write(pkt, WF_LUT_L0_PITCH, wf_lut_config->width * 2);
	pp_write(pkt, WF_LUT_RDMA0_CTRL, 0x00000001);
	pp_write(pkt, WF_LUT_RDMA0_MEM_GMC_SETTING1, 0x00005860);
	pp_write(pkt, WF_LUT_RDMA0_MEM_SLOW_CON, 0x00100000);
	pp_write(pkt, WF_LUT_RDMA0_FIFO_CTRL, 0x00800000);

	pp_write(pkt, WF_LUT_L1_PITCH, wf_lut_config->width * 2);
	pp_write(pkt, WF_LUT_RDMA1_CTRL, 0x00000001);
	pp_write(pkt, WF_LUT_RDMA1_MEM_GMC_SETTING1, 0x00005860);
	pp_write(pkt, WF_LUT_RDMA1_MEM_SLOW_CON, 0x00100000);
	pp_write(pkt, WF_LUT_RDMA1_FIFO_CTRL, 0x00800000);

	pp_write(pkt, WF_LUT_L2_PITCH, wf_lut_config->width * 2);
	pp_write(pkt, WF_LUT_RDMA2_CTRL, 0x00000001);
	pp_write(pkt, WF_LUT_RDMA2_MEM_GMC_SETTING1, 0x00005860);
	pp_write(pkt, WF_LUT_RDMA2_MEM_SLOW_CON, 0x00100000);
	pp_write(pkt, WF_LUT_RDMA2_FIFO_CTRL, 0x00800000);

	pp_write(pkt, WF_LUT_L3_PITCH, wf_lut_config->width * 2);
	pp_write(pkt, WF_LUT_RDMA3_CTRL, 0x00000001);
	pp_write(pkt, WF_LUT_RDMA3_MEM_GMC_SETTING1, 0x00005860);
	pp_write(pkt, WF_LUT_RDMA3_MEM_SLOW_CON, 0x00100000);
	pp_write(pkt, WF_LUT_RDMA3_FIFO_CTRL, 0x00800000);

}

void wf_lut_enable(struct cmdqRecStruct *pkt,
		   struct wf_lut_con_config *wf_lut_config)
{
	pp_write_mask(pkt, 0x10005760, 1<<12, GENMASK(14, 12));
	pp_write(pkt, WF_LUT_INTEN, wf_lut_config->wf_lut_inten);
	/* for ctp same */
	pp_write_mask(pkt, WF_LUT_EN, wf_lut_config->wf_lut_en, BIT_MASK(0));
	//pp_write(pkt, WF_LUT_EN, wf_lut_config->wf_lut_en);
}

void wf_lut_config_link_mode(struct cmdqRecStruct *pkt,
			     struct wf_lut_con_config *wf_lut_config)
{
	/* not setting,direct link alway using this setting */
	if (wf_lut_config->direct_link)
		pp_write(pkt, WF_LUT_LINK_MODE, 0xE4380ff2);
	else
		pp_write(pkt, WF_LUT_LINK_MODE, 0x00000002);

}

void wf_lut_config_base_addr(struct cmdqRecStruct *pkt,
			     struct wf_lut_con_config *wf_lut_config)
{
	pp_write(pkt, WF_LUT_LINK_BASE_ADDR, wf_lut_config->base_addr);
	pp_write(pkt, WF_LUT_LINK_BASE_ADDR_1, wf_lut_config->base_addr1);
}

void wf_lut_config_lut_con(struct cmdqRecStruct *pkt,
			   struct wf_lut_con_config *wf_lut_config)
{
	pp_write_mask(pkt, WF_LUT_CON, wf_lut_config->gray_mode, GENMASK(2, 0));
	pp_write_mask(pkt, WF_LUT_CON,
		      wf_lut_config->rg_8b_out << 12, BIT_MASK(12));
	pp_write_mask(pkt, WF_LUT_CON,
		      wf_lut_config->rg_partial_up_en << 19, BIT_MASK(19));
	pp_write_mask(pkt, WF_LUT_CON,
		      wf_lut_config->rg_partial_up_val << 20, GENMASK(23, 20));
	pp_write_mask(pkt, WF_LUT_CON,
		      wf_lut_config->rg_default_val << 8, GENMASK(11, 8));
	pp_write_mask(pkt, WF_LUT_CON,
	      0x3 << 25, GENMASK(26, 25));
	pp_write_mask(pkt, WF_LUT_CON,
	      0x1 << 24, BIT_MASK(24));

	pp_write_mask(pkt, WF_LUT_RDMA0_MEM_GMC_SETTING2,
		  0x80 << 16, GENMASK(23, 16));

	pp_write_mask(pkt, WF_LUT_RDMA1_MEM_GMC_SETTING2,
		  0x80 << 16, GENMASK(23, 16));

	pp_write_mask(pkt, WF_LUT_RDMA2_MEM_GMC_SETTING2,
		  0x80 << 16, GENMASK(23, 16));

	pp_write_mask(pkt, WF_LUT_RDMA3_MEM_GMC_SETTING2,
		  0x80 << 16, GENMASK(23, 16));

	pp_write_mask(pkt, WF_LUT_RDMA0_MEM_GMC_SETTING2,
		  0x40 << 24, GENMASK(31, 24));

	pp_write_mask(pkt, WF_LUT_RDMA1_MEM_GMC_SETTING2,
		  0x40 << 24, GENMASK(31, 24));

	pp_write_mask(pkt, WF_LUT_RDMA2_MEM_GMC_SETTING2,
		  0x40 << 24, GENMASK(31, 24));

	pp_write_mask(pkt, WF_LUT_RDMA3_MEM_GMC_SETTING2,
		  0x40 << 24, GENMASK(31, 24));

}

void wf_lut_config_mout(struct cmdqRecStruct *pkt,
			struct wf_lut_con_config *wf_lut_config)
{
	if (wf_lut_config->rg_8b_out) {
		pp_write(pkt, WF_LUT_MOUT, 0x00107001);
		pp_write_mask(pkt, WF_LUT_MOUT,
			      wf_lut_config->wf_lut_mout, GENMASK(1, 0));
	} else {
		pp_write(pkt, WF_LUT_MOUT, 0x00107241);
		pp_write_mask(pkt, WF_LUT_MOUT,
			      wf_lut_config->wf_lut_mout, GENMASK(1, 0));
	}
}

void wf_lut_config_wf_lut_checksum(struct cmdqRecStruct *pkt,
				   unsigned int enable, unsigned int sel)
{
	pp_write_mask(pkt, WF_LUT_DATAPATH_CON, sel << 8, GENMASK(11, 8));
	pp_write_mask(pkt, WF_LUT_DATAPATH_CON, enable << 4, BIT_MASK(4));
}

unsigned int wf_lut_convert_order(unsigned char *addr)
{
	unsigned int value = 0x00;

	value = ((*addr)<<24) + (*(addr+1)<<16) + (*(addr+2)<<8) + *(addr+3);
	return value;
}

char *wf_lut_waveform_get_name(void)
{
	hwtcon_core_load_init_setting_from_file();
	return hwtcon_fb_info()->waveform_va;
}

int wf_lut_waveform_get_temperature_threshold(unsigned char *waveform_addr)
{
	int i = 0;
	int temperature = 25;
	int ts_index = 9;
	int ts_total_number = *(waveform_addr + WAVEFORM_TS_NUM_TO_BEGIN);

	TCON_LOG("ts_total_number:%d", ts_total_number);

	if (ts_total_number == 0) {
		TCON_ERR("ts_total_number error! waveform may not load!");
		return -1;
	}

	for (i = 0; i < ts_total_number; i++)
		g_ts_threshold[i] = *(waveform_addr + WAVEFORM_TS_TO_BEGIN + i);

	temperature = fiti_read_temperature();

	for (i = 0; i < ts_total_number; i++) {
		if (temperature < g_ts_threshold[i]) {
			ts_index = i;
			break;
		}
	}

	TCON_LOG("ts_index:%d", ts_index);
	return ts_index;
}

int wf_lut_waveform_get_temperature_index(int temperature)
{
	int i = 0;
	int ts_index = 9;
	int ts_total_number = *(hwtcon_fb_info()->waveform_va +
		WAVEFORM_TS_NUM_TO_BEGIN);

	TCON_LOG("ts_total_number:%d", ts_total_number);

	if (ts_total_number <= 0 || ts_total_number > ARRAY_SIZE(g_ts_threshold)) {
		TCON_ERR("ts_total_number error: %d", ts_total_number);
		WARN_ON(1);
		return ts_index;
	}

	for (i = 0; i < ts_total_number; i++)
		g_ts_threshold[i] = *(hwtcon_fb_info()->waveform_va +
		WAVEFORM_TS_TO_BEGIN + i);

	for (i = 0; i < ts_total_number; i++) {
		if (temperature < g_ts_threshold[i]) {
			ts_index = i;
			break;
		}
	}

	TCON_LOG("temperature:%d temp_zone:%d",
		temperature, ts_index);
	return ts_index;
}

void wf_lut_waveform_table_init(void)
{
	int i = 0;
	int j = 0;

	memset((unsigned char *)&g_waveform_table, 0x00,
	       TEMPERATURE_NUM * WAVEFORM_MODE_TOTAL_NUM *
	       sizeof(struct wf_lut_waveform));
	for (i = 0; i < TEMPERATURE_NUM; i++) {
		for (j = 0; j < WAVEFORM_MODE_TOTAL_NUM; j++) {
			g_waveform_table[i][j].waveform_mode = j;
			g_waveform_table[i][j].start_addr =
		    hwtcon_fb_info()->waveform_pa +
		    wf_lut_convert_order(hwtcon_fb_info()->waveform_va
		    + WAVEFORM_ADDR_OFFSET_TO_BEGIN +
		    i * WAVEFORM_ADDR_OFFSET_PER_TEMP + 4 * j);
			g_waveform_table[i][j].len =
			wf_lut_convert_order(hwtcon_fb_info()->waveform_va
			+ WAVEFORM_LEN_OFFSET_TO_BEGIN +
		    i * WAVEFORM_LEN_OFFSET_PER_TEMP + 4 * j) / 0x100;
		}
	}
}


void wf_lut_waveform_replace(struct cmdqRecStruct *pkt,
		int old_index, int new_mode)
{
	struct wf_lut_waveform *current_waveform = NULL;

	current_waveform =
		((struct wf_lut_waveform *)
		&g_waveform_table[g_current_temperature][0])
			+new_mode;

	switch (old_index) {
	case 0:
		pp_write(pkt, WF_LUT_ADDR_0,
			 current_waveform->start_addr);
		pp_write(pkt, WF_LUT_LEN_0, current_waveform->len);
		break;
	case 1:
		pp_write(pkt, WF_LUT_ADDR_1,
			 current_waveform->start_addr);
		pp_write(pkt, WF_LUT_LEN_1, current_waveform->len);
		break;
	case 2:
		pp_write(pkt, WF_LUT_ADDR_2,
			 current_waveform->start_addr);
		pp_write(pkt, WF_LUT_LEN_2, current_waveform->len);
		break;
	case 3:
		pp_write(pkt, WF_LUT_ADDR_3,
			 current_waveform->start_addr);
		pp_write(pkt, WF_LUT_LEN_3, current_waveform->len);
		break;
	case 4:
		pp_write(pkt, WF_LUT_ADDR_4,
			 current_waveform->start_addr);
		pp_write(pkt, WF_LUT_LEN_4, current_waveform->len);
		break;
	case 5:
		pp_write(pkt, WF_LUT_ADDR_5,
			 current_waveform->start_addr);
		pp_write(pkt, WF_LUT_LEN_5, current_waveform->len);
		break;
	case 6:
		pp_write(pkt, WF_LUT_ADDR_6,
			 current_waveform->start_addr);
		pp_write(pkt, WF_LUT_LEN_6, current_waveform->len);
		break;
	case 7:
		pp_write(pkt, WF_LUT_ADDR_7,
			 current_waveform->start_addr);
		pp_write(pkt, WF_LUT_LEN_7, current_waveform->len);
		break;
	default:

		break;
	}


}

void wf_lut_waveform_day_mode_slot(struct cmdqRecStruct *pkt)
{
	wf_lut_waveform_replace(pkt, 0, 0);
	wf_lut_waveform_replace(pkt, 1, 1);
	wf_lut_waveform_replace(pkt, 2, 2);
	wf_lut_waveform_replace(pkt, 3, 10);
	wf_lut_waveform_replace(pkt, 4, 3);
	wf_lut_waveform_replace(pkt, 5, 4);
	wf_lut_waveform_replace(pkt, 6, 6);
	wf_lut_waveform_replace(pkt, 7, 2);
}

void wf_lut_waveform_night_mode_slot(struct cmdqRecStruct *pkt)
{
	wf_lut_waveform_replace(pkt, 0, 0);
	wf_lut_waveform_replace(pkt, 1, 1);
	wf_lut_waveform_replace(pkt, 2, 8);
	wf_lut_waveform_replace(pkt, 3, 11);
	wf_lut_waveform_replace(pkt, 4, 9);
	wf_lut_waveform_replace(pkt, 5, 9);
	wf_lut_waveform_replace(pkt, 6, 6);
	wf_lut_waveform_replace(pkt, 7, 8);
}

void wf_lut_waveform_slot_association(struct cmdqRecStruct *pkt,
	unsigned int mode, unsigned int temp)
{
	/* temperature assigned should insert here */
	g_current_temperature = temp;
	if (mode)
		wf_lut_waveform_night_mode_slot(pkt);
	else
		wf_lut_waveform_day_mode_slot(pkt);
}

unsigned int wf_lut_check_waveform_by_addr(unsigned int addr)
{
	unsigned int waveform_mode = 0;
	unsigned int j = 0;

	for (j = 0; j < WAVEFORM_MODE_TOTAL_NUM; j++) {
		if (addr ==
		g_waveform_table[g_current_temperature][j].start_addr) {
			waveform_mode = j;
			break;
		}
	}

	if (j == WAVEFORM_MODE_TOTAL_NUM)
		TCON_ERR("wf_lut_check_waveform_by_addr found error!");
#if 0
	TCON_ERR("this waveform mode addr:%08x, mode:%d,temperature:%d",
		addr, waveform_mode, g_current_temperature);
#endif
	return waveform_mode;
}

unsigned int *wf_lut_get_waveform_mode_in_hardware(void)
{
	g_current_waveform_mode_in_HW[0] =
		wf_lut_check_waveform_by_addr(pp_read(WF_LUT_ADDR_0_VA));

	g_current_waveform_mode_in_HW[1] =
		wf_lut_check_waveform_by_addr(pp_read(WF_LUT_ADDR_1_VA));

	g_current_waveform_mode_in_HW[2] =
		wf_lut_check_waveform_by_addr(pp_read(WF_LUT_ADDR_2_VA));

	g_current_waveform_mode_in_HW[3] =
		wf_lut_check_waveform_by_addr(pp_read(WF_LUT_ADDR_3_VA));

	g_current_waveform_mode_in_HW[4] =
		wf_lut_check_waveform_by_addr(pp_read(WF_LUT_ADDR_4_VA));

	g_current_waveform_mode_in_HW[5] =
		wf_lut_check_waveform_by_addr(pp_read(WF_LUT_ADDR_5_VA));

	g_current_waveform_mode_in_HW[6] =
		wf_lut_check_waveform_by_addr(pp_read(WF_LUT_ADDR_6_VA));

	g_current_waveform_mode_in_HW[7] =
		wf_lut_check_waveform_by_addr(pp_read(WF_LUT_ADDR_1_VA));

	return g_current_waveform_mode_in_HW;

}

void wf_lut_waveform_select_by_temp(struct cmdqRecStruct *pkt, int temp)
{
	int index = 0;

	if (temp != g_current_temperature) {
		TCON_LOG("temp change before temp:%d,after temp:%d\n",
			g_current_temperature, temp);
		/* get  before temperature waveform mode */
		wf_lut_get_waveform_mode_in_hardware();

		/* temperature assigned should insert here */
		g_current_temperature = temp;

		/* set  now temperature waveform mode */
		for (index = 0; index < 8; index++) {
			wf_lut_waveform_replace(pkt,
				index, g_current_waveform_mode_in_HW[index]);
		}
	}
}

unsigned int wf_lut_get_waveform_len(int temp, int mode)
{
	struct wf_lut_waveform *current_waveform = NULL;

	if ((temp < 0) || (temp >= TEMPERATURE_NUM)) {
		TCON_ERR("invalid temp zone:%d", temp);
		return 0;
	}

	current_waveform =
		((struct wf_lut_waveform *)
		&g_waveform_table[temp][0])
			+ mode;

	return current_waveform->len;
}

void wf_lut_config_waveform_v2(struct cmdqRecStruct *pkt)
{
	int index = 0;

	for (index = 0; index < 8; index++) {
		wf_lut_waveform_replace(pkt,
			index, g_current_waveform_mode_in_HW[index]);
	}
}

unsigned int wf_lut_get_rdma0_checksum(void)
{
	return pp_read(WF_LUT_CHKSUM_0_VA);
}

unsigned int wf_lut_get_rdma1_checksum(void)
{
	return pp_read(WF_LUT_CHKSUM_1_VA);
}

unsigned int wf_lut_get_rdma2_checksum(void)
{
	return pp_read(WF_LUT_CHKSUM_2_VA);
}

unsigned int wf_lut_get_rdma3_checksum(void)
{
	return pp_read(WF_LUT_CHKSUM_3_VA);
}

unsigned int wf_lut_get_wf_lut_output_checksum(void)
{
	return pp_read(WF_LUT_CHKSUM_4_VA);
}


u32 wf_lut_get_irq_status(void)
{

	return pp_read(WF_LUT_INTSTA_VA);
}

void wf_lut_clear_lut_end_irq_status(struct cmdqRecStruct *pkt)
{
	pp_write_mask(pkt, WF_LUT_CON, 0x01 << 15, BIT_MASK(15));
	pp_write_mask(pkt, WF_LUT_CON, 0x00 << 15, BIT_MASK(15));
}

void wf_lut_clear_irq_status(struct cmdqRecStruct *pkt)
{
	pp_write(pkt, WF_LUT_INTSTA, 0x0);
}

void wf_lut_config_context(struct cmdqRecStruct *pkt)
{
	struct wf_lut_con_config wf_lut_config = { 0x00 };

	wf_lut_config.base_addr = hwtcon_fb_info()->wb_pa[0];
	wf_lut_config.base_addr1 = hwtcon_fb_info()->wb_pa[1];
	wf_lut_config.gray_mode = GRAY_MODE_32_GRAY_LEVEL;
	wf_lut_config.width = hw_tcon_get_edp_width();
	wf_lut_config.height = hw_tcon_get_edp_height();
	wf_lut_config.rdma_enable_mask = 0xf;
	wf_lut_config.DECFMT = 0x0;
	wf_lut_config.checksum_en = 0x01;
	wf_lut_config.H_FLIP_EN = 0x00;
	wf_lut_config.V_FLIP_EN = 0x00;
	wf_lut_config.wf_lut_en = 0x01;
	wf_lut_config.wf_lut_inten = 0x02;
	wf_lut_config.rg_default_val = 0x00;
	wf_lut_config.rg_partial_up_en = 0x00;
	wf_lut_config.rg_partial_up_val = 0x00;
	wf_lut_config.layer_greq_num = 0x10;
	wf_lut_config.checksum_sel = 0x01;
	wf_lut_config.layer_smi_id_en = 0x01;
	wf_lut_config.rg_8b_out = hw_tcon_get_edp_out_8bit();
	wf_lut_config.byte_swap = 0x02;
	wf_lut_config.direct_link = 0x01;
	// 0: wf_lut send lut_end at fixed time after sof
	// 1: wf_lut send lut_end after read waveform and time is varied
	wf_lut_config.rg_lut_end_sel = 0x00;

	/*bit 0 output to dpi,bit1 output to wdma */
	wf_lut_config.wf_lut_mout = 0x01;
	wf_lut_config.temperature_index = g_current_temperature;
	wf_lut_config.waveform_table_current =
	    (struct wf_lut_waveform *)&g_waveform_table[wf_lut_config.
							temperature_index][0];

	wf_lut_waveform_table_init();

	wf_lut_config_mmsys(pkt, &wf_lut_config);

	/*smi config in rdma already config */
#if 0
	wf_lut_config_waveform_v2(pkt);
#endif
	wf_lut_config_link_mode(pkt, &wf_lut_config);

	wf_lut_config_base_addr(pkt, &wf_lut_config);

	/*lut enable info from pipeline in direct link mode, not need config */

	wf_lut_config_mout(pkt, &wf_lut_config);

	wf_lut_config_common(pkt, &wf_lut_config);

	wf_lut_config_inter_rdma(pkt, &wf_lut_config);

	wf_lut_config_lut_con(pkt, &wf_lut_config);

	wf_lut_enable(pkt, &wf_lut_config);

	if (wf_lut_config.rg_8b_out) {
		wf_lut_rdma_config_rdma(pkt, wf_lut_config.width / 4,
					wf_lut_config.height, 2, 0);

		/*dpi config */
		wf_lut_config_dpi_context(pkt, wf_lut_config.width / 4,
					  wf_lut_config.height);
	} else {
		wf_lut_rdma_config_rdma(pkt, wf_lut_config.width / 8,
					wf_lut_config.height, 2, 0);

		/*dpi config */
		wf_lut_config_dpi_context(pkt, wf_lut_config.width / 8,
					  wf_lut_config.height);
	}

}


void swtcon_config_context(struct cmdqRecStruct *pkt)
{

	int value = 0;
	int frame_cnt = 0;

	/* pmic control */
	//pp_write(pkt, MMSYS_DUMMY1, 0x0000007F);
	TCON_LOG("enter swtcon_config_context!\n");
	pp_write_mask(pkt, PAPER_TCTOP_PIN_INV, 0x001b0000, GENMASK(20, 16));


	tcon_config_swtcon_pin(pkt);



	/*rdma */
	wf_lut_rdma_config_rdma(pkt, 192, 405, 2, 1);

	rdma_config_smi_setting(pkt);

	/*600*400=192,405 */
	/*1448*1072=0x1A0,0x435 */

	/*dpi config */
	wf_lut_config_dpi_context(pkt, 192, 405);

	while (1) {
		value = pp_read(WF_LUT_RDMA_INT_STATUS_VA);
		if ((value & 0x04) == 0x04) {
			TCON_ERR("DPI_TEST_1: frame %d end!!\n", frame_cnt);
			if (frame_cnt >= 39) {
				TCON_ERR("DPI_TEST_1: frame %d FINISH!!\n",
					 frame_cnt);
				#if 0
				pp_write(NULL, WF_LUT_RDMA_MEM_START_ADDR,
					 0x56000000);
				#else
				pp_write(NULL, WF_LUT_RDMA_MEM_START_ADDR,
					 hwtcon_fb_info()->wb_pa[0]);
				#endif
				break;
			}
			pp_write(NULL, WF_LUT_RDMA_INT_STATUS, 0x0);
			TCON_ERR("==>WAIT:rdma INT STATUS:0x%x\n",
				 pp_read(WF_LUT_RDMA_INT_STATUS_VA));
			pp_write(NULL, WF_LUT_RDMA_MEM_START_ADDR, 0x56000000);
			frame_cnt++;
		}
	}

}

void swdata_hwtcon_config_context(struct cmdqRecStruct *pkt)
{

	int value = 0;
	int frame_cnt = 0;

	/* pmic control */
	//pp_write(pkt, MMSYS_DUMMY1, 0x0000007F);

	TCON_LOG("enter swdata_hwtcon_config_context!\n");
	tcon_setting(pkt);



	/*rdma */
	wf_lut_rdma_config_rdma(pkt, 150, 400, 2, 1);

	rdma_config_smi_setting(pkt);

	/*600*400=150,400 */
	/*1448*1072=362,1072 */

	/*dpi config */
	wf_lut_config_dpi_context(pkt, 150, 400);

	while (1) {
		value = pp_read(WF_LUT_RDMA_INT_STATUS_VA);
		if ((value & 0x04) == 0x04) {
			frame_cnt++;
			TCON_ERR("DPI_TEST_1: frame %d end!!\n", frame_cnt);
			if (frame_cnt >= 39) {
				TCON_ERR("DPI_TEST_1: frame %d FINISH!!\n",
					 frame_cnt);
				pp_write(NULL, WF_LUT_RDMA_MEM_START_ADDR,
					 hwtcon_fb_info()->swdata_pa +
					 frame_cnt * hw_tcon_get_edp_height() *
					 hw_tcon_get_edp_width());
				break;
			}
			pp_write(NULL, WF_LUT_RDMA_INT_STATUS, 0x0);
			TCON_ERR("==>WAIT:rdma INT STATUS:0x%x\n",
				 pp_read(WF_LUT_RDMA_INT_STATUS_VA));
			pp_write(NULL, WF_LUT_RDMA_MEM_START_ADDR,
				 hwtcon_fb_info()->swdata_pa +
				 frame_cnt * hw_tcon_get_edp_height() *
				 hw_tcon_get_edp_width());
		}
	}

}

void wf_lut_dpi_config_context(struct cmdqRecStruct *pkt)
{
	struct wf_lut_con_config wf_lut_config = { 0x00 };

	TCON_LOG("enter wf_lut_dpi_config_context!\n");
	wf_lut_config.base_addr = hwtcon_fb_info()->wb_pa[0];
	wf_lut_config.base_addr1 = hwtcon_fb_info()->wb_pa[1];
	wf_lut_config.gray_mode = GRAY_MODE_32_GRAY_LEVEL;
	wf_lut_config.width = hw_tcon_get_edp_width();
	wf_lut_config.height = hw_tcon_get_edp_height();
	wf_lut_config.rdma_enable_mask = 0x01;
	wf_lut_config.DECFMT = 0x0;
	wf_lut_config.checksum_en = 0x01;
	wf_lut_config.H_FLIP_EN = 0x00;
	wf_lut_config.V_FLIP_EN = 0x00;
	wf_lut_config.wf_lut_en = 0x01;
	wf_lut_config.wf_lut_inten = 0x02;
	wf_lut_config.rg_default_val = 0x00;
	wf_lut_config.rg_partial_up_en = 0x00;
	wf_lut_config.rg_partial_up_val = 0x00;
	wf_lut_config.layer_greq_num = 0x10;
	wf_lut_config.checksum_sel = 0x00;
	wf_lut_config.layer_smi_id_en = 0x01;
	wf_lut_config.rg_8b_out = hw_tcon_get_edp_out_8bit();
	wf_lut_config.byte_swap = 0x02;
	wf_lut_config.rg_de_sel = 0x01;
	wf_lut_config.rg_lut_end_sel = 0x01;

	/*bit 0 output to dpi,bit1 output to wdma */
#ifdef WF_WDMA_SUPPORT
	wf_lut_config.wf_lut_mout = 0x02;
#else
	wf_lut_config.wf_lut_mout = 0x01;
#endif
	wf_lut_config.temperature_index = 0x00;
	wf_lut_config.waveform_table_current =
	    (struct wf_lut_waveform *)&g_waveform_table[wf_lut_config.
							temperature_index][0];

	wf_lut_config.wb_rdma[0].width = hw_tcon_get_edp_width();
	wf_lut_config.wb_rdma[0].height = hw_tcon_get_edp_height();
	wf_lut_config.wb_rdma[0].start_addr = hwtcon_fb_info()->wb_pa[0];
	wf_lut_config.wb_rdma[0].x = 0x00;
	wf_lut_config.wb_rdma[0].y = 0x00;

	wf_lut_waveform_table_init();

	tcon_setting(pkt);

	rdma_config_smi_setting(pkt);

	wf_lut_config_mmsys(pkt, &wf_lut_config);

	/*smi config in rdma already config */

	wf_lut_config_lut_enable(NULL, 0x01, 0x00);

	wf_lut_config_waveform(pkt, wf_lut_config.waveform_table_current);

	wf_lut_config_link_mode(pkt, &wf_lut_config);

	wf_lut_config_base_addr(pkt, &wf_lut_config);

	/*lut enable info from pipeline in direct link mode, not need config */

	wf_lut_config_mout(pkt, &wf_lut_config);

	wf_lut_config_common(pkt, &wf_lut_config);

	wf_lut_config_inter_rdma(pkt, &wf_lut_config);

	wf_lut_config_lut_con(pkt, &wf_lut_config);

	wf_lut_enable(pkt, &wf_lut_config);

	if (wf_lut_config.rg_8b_out) {
		wf_lut_rdma_config_rdma(pkt, wf_lut_config.width / 4,
					wf_lut_config.height, 2, 0);

		/*dpi config */
		wf_lut_config_dpi_context(pkt, wf_lut_config.width / 4,
					  wf_lut_config.height);
	} else {
		wf_lut_rdma_config_rdma(pkt, wf_lut_config.width / 8,
					wf_lut_config.height, 2, 0);

		/*dpi config */
		wf_lut_config_dpi_context(pkt, wf_lut_config.width / 8,
					  wf_lut_config.height);
	}

#ifdef WF_WDMA_SUPPORT
		if (wf_lut_config.wf_lut_mout == 0x02) {
			Wf_Lut_Wdma_Config(pkt, wf_lut_config.width / 4,
					   wf_lut_config.height);
		}
#endif

}


void hwtcon_edp_pinmux_control(struct platform_device *pdev)
{
	g_pctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(g_pctrl))
		TCON_ERR("devm_pinctrl_get error!\n");

	g_pin_state_active = pinctrl_lookup_state(g_pctrl, "active");
	if (IS_ERR(g_pin_state_active))
		TCON_ERR("pinctrl_lookup_state active error!\n");

	g_pin_state_inactive = pinctrl_lookup_state(g_pctrl, "inactive");
	if (IS_ERR(g_pin_state_inactive))
		TCON_ERR("pinctrl_lookup_state inactive error!\n");
}

void hwtcon_edp_pinmux_release(void)
{
	devm_pinctrl_put(g_pctrl);
}

void hwtcon_edp_pinmux_active(void)
{
	TCON_LOG("hwtcon_edp_pinmux_active!\n");
	pinctrl_select_state(g_pctrl, g_pin_state_active);
}

void hwtcon_edp_pinmux_inactive(void)
{
	TCON_LOG("hwtcon_edp_pinmux_inactive!\n");
	pinctrl_select_state(g_pctrl, g_pin_state_inactive);
}


