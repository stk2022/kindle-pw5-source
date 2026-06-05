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

#include "hwtcon_paper_top_config.h"
#include "hwtcon_hal.h"


/*
 * enable HW Clock.
 * one call only open 1 HW. Need to call many times to enable HWs.
 */
void paper_enable_hw_clock(struct cmdqRecStruct *pkt,
	enum HW_GATE_CTL_BIT_ENUM hw_id)
{
	if (hw_id >= HW_GATE_CTL_BIT_MAX) {
		TCON_ERR("invalid HW ID:%d", hw_id);
		return;
	}
	pp_write_mask(pkt, PAPER_TCTOP_PWDC_CTL,
		BIT_ENABLE(hw_id),
		BIT_MASK(hw_id));
}


/* config update mode: partitial update or full update */
void paper_config_update_mode(struct cmdqRecStruct *pkt,
	enum PAPER_UPDATE_MODE_ENUM mode)
{
	pp_write_mask(pkt, PAPER_TCTOP_MAIN_CTL, mode, BIT_MASK(0));
}

/* config pipeline fake write enable */
void paper_config_fake_write_enable(struct cmdqRecStruct *pkt, bool enable)
{
	pp_write_mask(pkt, PAPER_TCTOP_MAIN_CTL, enable << 9, BIT_MASK(9));
}

/* init working buffer content */
void paper_init_working_buffer(struct cmdqRecStruct *pkt,
	enum PAPER_TOP_INIT_MODE_ENUM init_mode,
	int init_pre_pixel_data,
	int init_cur_pixel_data,
	enum WAVEFORM_MODE_ENUM init_wf_mode)
{
	struct paper_top_init_wb_config init_config = {0};

	init_config.INI_MODE = init_mode;
	init_config.INI_PRE_DATA = init_pre_pixel_data;
	init_config.INI_CUR_DATA = init_cur_pixel_data;
	init_config.INI_WF_TYPE = init_wf_mode;
	init_config.INI_REQ = true;


	pp_write(pkt, PAPER_TCTOP_INI_CFG,
		init_config.INI_REQ << 31 |
		init_config.INI_WF_TYPE << 24 |
		init_config.INI_CUR_DATA << 16 |
		init_config.INI_PRE_DATA << 8 |
		init_config.INI_MODE);

	/* VIP NOTE:
	 * when init working buffer, driver need to set INI_REQ = 1
	 * then INI_REQ = 0 to complete init working buffer request.
	 */
	pp_write_mask(pkt, PAPER_TCTOP_INI_CFG, 0 << 31, BIT_MASK(31));
}


void paper_config_pre_buffer_region(struct cmdqRecStruct *pkt,
	enum PRE_BUF_UPDATE_MODE_ENUM mode)
{
	pp_write_mask(pkt, PAPER_TCTOP_MAIN_CTL, mode, BIT_MASK(1));
}

/* config panel width & height */
void paper_config_panel_size(struct cmdqRecStruct *pkt,
	int panel_width, int panel_height)
{
	pp_write(pkt, PAPER_TCTOP_PANEL_SIZE,
		panel_height << 16 | panel_width);
}

/* enable regal */
void paper_regal_enable(struct cmdqRecStruct *pkt, bool enable)
{
	pp_write_mask(pkt, PAPER_TCTOP_REGAL_CFG, enable << 0, BIT_MASK(0));
}

/* clear regal interrupt */
void paper_regal_clear_interrupt(struct cmdqRecStruct *pkt)
{
	pp_write_mask(pkt, PAPER_TCTOP_REGAL_CFG, 1 << 1, BIT_MASK(1));
}

/* read regal irq status. */
bool paper_regal_read_interrupt_status(void)
{
	/* bit[2] irq status */
	return pp_read(PAPER_TCTOP_REGAL_CFG_VA) >> 2;
}

/* config data process fifo */
void paper_config_data_process_fifo(struct cmdqRecStruct *pkt,
	bool enable_fifo,
	int fifo_size, int fifo_read_start_threshold)
{
	struct paper_top_fifo_config fifo_config = {0};

	fifo_config.FIFO_READ_START_TH = fifo_read_start_threshold;
	fifo_config.FIFO_SIZE = fifo_size;
	fifo_config.FIFO_EN = enable_fifo;

	pp_write(pkt, PAPET_TCTOP_FIFO_CFG,
		fifo_config.FIFO_READ_START_TH << 0 |
		fifo_config.FIFO_SIZE << 8 |
		fifo_config.FIFO_EN << 16);
}

/* config hw sof select from HW or software  */
void paper_config_sof_sel(struct cmdqRecStruct *pkt,
	enum IMG_RDMA_SOF_SEL_ENUM img_rdma_sof_sel,
	enum WB_RDMA_SOF_SEL_ENUM wb_rdma_sof_sel,
	enum WB_WDMA_SOF_SEL_ENUM wb_wdma_sof_sel,
	enum PIPELINE_SOF_SEL_ENUM pipeline_sof_sel,
	enum WF_LUT_SOF_SEL_ENUM wf_lut_sof_sel,
	enum LUT_MERGE_SOF_SEL_ENUM lut_merge_sof_sel)
{
	pp_write_mask(pkt, PAPER_TCTOP_SOF_CTL,
		img_rdma_sof_sel |
		wb_rdma_sof_sel |
		wb_wdma_sof_sel |
		pipeline_sof_sel |
		wf_lut_sof_sel |
		lut_merge_sof_sel,
		0xFF);
}

/* config main sof : how main sof be triggered. */
void paper_config_main_sof_mode(struct cmdqRecStruct *pkt,
	enum MAIN_SOF_MODE_ENUM main_sof_mode)
{
	pp_write_mask(pkt, PAPER_TCTOP_DPI_CFG, main_sof_mode, GENMASK(2, 1));
}

/* config the max counter of cycle
 * the timer will start when main sof comes,
 * and end when counter = this max counter
 * other hw sof position must < max counter.
 * otherwise this hw sof will never come.
 */
void paper_config_main_sof_max_counter(struct cmdqRecStruct *pkt,
	u32 max_counter)
{
	pp_write(pkt, PAPER_TCTOP_SOF_MAIN_CTL, max_counter);
}


/* config dpi vsync trigger mode: trigger by WB WDMA SOF or HW auto. */
void paper_config_dpi_vsync_trigger_mode(struct cmdqRecStruct *pkt,
	enum DPI_VSYNC_SEL_ENUM mode)
{
	pp_write_mask(pkt, PAPER_TCTOP_DPI_CFG, mode, BIT_MASK(0));
}

/* pipeline sof position.  Delay cycle of main sof */
void paper_config_pipeline_sof_position(struct cmdqRecStruct *pkt,
	u32 pipeline_sof_position)
{
	pp_write_mask(pkt, PAPER_TCTOP_SOF_CTL,
		pipeline_sof_position << 16,
		GENMASK(31, 16));
}

/* lut merge sof position. must ready before wf_lut work,
 * so must before than wf_lut sof
 */
void paper_config_lut_merge_sof_position(struct cmdqRecStruct *pkt,
	u32 lut_merge_sof_position)
{
	pp_write(pkt, PAPER_TCTOP_SOF_MERGE_CTL, lut_merge_sof_position);
}

/* wf lut sof position. Delay cycle of main sof */
void paper_config_wf_lut_sof_position(struct cmdqRecStruct *pkt,
	u32 wf_lut_sof_position)
{
	pp_write(pkt, PAPER_TCTOP_SOF_WF_LUT_CTL, wf_lut_sof_position);
}

/* config software pipeline sof potion  Delay cycle of main sof */
void paper_config_sw_pipeline_sof_position(struct cmdqRecStruct *pkt,
	u32 pipeline_sof_position)
{
	pp_write(pkt, PAPER_TCTOP_SOF_PIPELINE_CTL, pipeline_sof_position);
}

/* config software img rdma / wb rdma / wb wdma
 * sof position Delay cycle of main sof
 */
void paper_config_sw_dma_sof_position(struct cmdqRecStruct *pkt,
	u32 dma_sof_position)
{
	pp_write(pkt, PAPER_TCTOP_SOF_DMA_CTL, dma_sof_position);
}

/* update a region */
void paper_config_update_lut(struct cmdqRecStruct *pkt,
	const struct update_lut_config *lut_config)
{
	pp_write(pkt, PAPER_TCTOP_UPD_CFG0,
		lut_config->lut_region.y << 17 |
		lut_config->lut_region.x << 4 |
		(lut_config->waveform_mode & GENMASK(3, 0)) << 0);

	pp_write(pkt, PAPER_TCTOP_UPD_CFG1,
		lut_config->lut_region.height << 13 |
		lut_config->lut_region.width << 0);

	if (lut_config->is_last_lut) {
		/* write 1 then 0 to trigger update & last_update */
		/* is the last lut need to update. trigger HW work. */
		pp_write_mask(pkt, PAPER_TCTOP_UPD_CFG2,
			lut_config->is_last_lut << 1 |
			1 << 0,	/* request to update a lut */
			GENMASK(1, 0));
		/* is the last lut need to update. trigger HW work. */
		pp_write_mask(pkt, PAPER_TCTOP_UPD_CFG2,
			0 << 1 |
			0 << 0,	/* request to update a lut */
			GENMASK(1, 0));
	} else {
		/* write 1 then 0 to trigger update. */
		pp_write_mask(pkt, PAPER_TCTOP_UPD_CFG2,
			1 << 0,	/* request to update a lut */
			BIT_MASK(0));
		pp_write_mask(pkt, PAPER_TCTOP_UPD_CFG2,
			0 << 0,	/* request to update a lut */
			BIT_MASK(0));
	}
}

int paper_get_config_lut_number(void)
{
	return pp_read(PAPER_TCTOP_UPD_CFG5_VA) & GENMASK(6, 0);
}

void paper_get_config_lut_info(struct cmdqRecStruct *pkt,
	int index, enum WAVEFORM_MODE_ENUM *wf_mode, struct rect *region)
{
	unsigned long long data = 0;
	u32 config3, config4;

	if (index >= MAX_LUT_REGION_COUNT) {
		TCON_ERR("invalid lut read index:%d", index);
		return;
	}

	/* request to read lut info */
	pp_write_mask(pkt, PAPER_TCTOP_UPD_CFG2,
		(index << 4) | (1 << 12),
		GENMASK(10, 4) | BIT_MASK(12));

	/*
	 * 12:0 image_st_h		x
	 * 25:13 image_st_v		y
	 * 38:26 image_wd_h		width
	 * 51:39 image_wd_v		height
	 * 55:52 image_wf		wf mode
	 * 62:56 image_addr
	 */
	config3 = pp_read(PAPER_TCTOP_UPD_CFG3_VA);
	config4 = pp_read(PAPER_TCTOP_UPD_CFG4_VA);
	data = config3 | ((unsigned long long)config4 << 32);
	*wf_mode = (data >> 52) & GENMASK(3, 0);
	region->x = (data >> 0) & GENMASK(12, 0);
	region->y = (data >> 13) & GENMASK(12, 0);
	region->width =  (data >> 26) & GENMASK(12, 0);
	region->height = (data >> 39) & GENMASK(12, 0);

}

/* config image buffer pitch.
 * if pitch_config_type == BUF_PITCH_SEL_FROM_PANEL,
 * the pitch param will have no use.
 * if pitch_config_type == BUF_PITCH_SEL_FROM_SW_CONFIG,
 * the pitch will take effect.
 */
void paper_config_image_buffer_pitch(struct cmdqRecStruct *pkt,
	enum BUF_PITCH_SEL_ENUM pitch_config_type, int pitch)
{
	pp_write_mask(pkt, PAPER_TCTOP_BUF_CFG0,
		pitch_config_type << 6,
		BIT_MASK(6));

	if (pitch_config_type == BUF_PITCH_SEL_FROM_PANEL)
		return;

	/* pitch_config_type ==  BUF_PITCH_SEL_FROM_SW_CONFIG
	 * the buffer pitch is configured by PAPER_TCTOP_BUF_CFG1.
	 */
	pp_write_mask(pkt, PAPER_TCTOP_BUF_CFG1, pitch << 0, GENMASK(15, 0));
}

/* config working buffer pitch.
 * if pitch_config_type == BUF_PITCH_SEL_FROM_PANEL,
 * the pitch param will have no use.
 * if pitch_config_type == BUF_PITCH_SEL_FROM_SW_CONFIG,
 * the pitch will take effect.
 */
void paper_config_working_buffer_pitch(struct cmdqRecStruct *pkt,
	enum BUF_PITCH_SEL_ENUM pitch_config_type, int pitch)
{
	pp_write_mask(pkt, PAPER_TCTOP_BUF_CFG0,
		pitch_config_type << 7,
		BIT_MASK(7));

	if (pitch_config_type == BUF_PITCH_SEL_FROM_PANEL)
		return;

	/* pitch_config_type ==  BUF_PITCH_SEL_FROM_SW_CONFIG
	 * the buffer pitch is configured by PAPER_TCTOP_BUF_CFG1.
	 */
	pp_write_mask(pkt, PAPER_TCTOP_BUF_CFG1, pitch << 16, GENMASK(31, 16));
}


/*
 * config dma(img rdma / wb rdma / wb wdma) method.
 * CONFIG_DMA_SOURCE_SEL_INTERNAL_CONFIG:
 * use img rdma / wb rdma / wb wdma internal config parameter.
 * CONFIG_DMA_SOURCE_SEL_AUTO_CONFIG:
 * paper top auto calculate. will not use DMA hw internal config param.
 */
void paper_config_dma_source_select(struct cmdqRecStruct *pkt,
	enum DMA_CONFIG_SOURCE_SEL_ENUM config_source)
{
	pp_write_mask(pkt, PAPER_TCTOP_BUF_CFG0, config_source, BIT_MASK(0));
}

/*
 * config img buffer read size
 */
void paper_config_img_buffer_size(struct cmdqRecStruct *pkt,
	enum IMG_BUFFER_SIZE_SEL_ENUM select_mode)
{
	pp_write_mask(pkt, PAPER_TCTOP_BUF_CFG0, select_mode, BIT_MASK(5));
}

/*
 * config img buffer format. {Y4, 4'b0}  or {Y4_1, Y4_0}
 */
void paper_config_img_buffer_format(struct cmdqRecStruct *pkt,
	enum IMG_BUFFER_FORMAT_ENUM format)
{
	pp_write_mask(pkt, PAPER_TCTOP_BUF_CFG0, format, BIT_MASK(4));
}

/* config working buffer mode: signal mode / pingpong mode */
void paper_config_working_buffer_mode(struct cmdqRecStruct *pkt,
	enum WB_BUFFER_MODE_ENUM mode)
{
	pp_write_mask(pkt, PAPER_TCTOP_BUF_CFG0, mode, BIT_MASK(1));
}

/*
 * config working buffer read buffer index.
 * if index = WB_READ_INDEX_ADDR0: rdma read 0, wdma write 1.
 * if index = WB_READ_INDEX_ADDR1: rdma read 1, wdma write 0.
 */
void paper_config_working_buffer_start_index(struct cmdqRecStruct *pkt,
	enum WB_READ_INDEX_ENUM index)
{
	pp_write_mask(pkt, PAPER_TCTOP_BUF_CFG0, index, BIT_MASK(3));
	/* use software buffer index trigger */
	pp_write_mask(pkt, PAPER_TCTOP_BUF_CFG0, 1 << 2, BIT_MASK(2));

	#ifdef HWTCON_AUTO_CHANGE_BUFFER_INDEX
	pp_write_mask(pkt, PAPER_TCTOP_BUF_CFG0, 0 << 2, BIT_MASK(2));
	#endif
}

/*
 * config working buffer address
 */
void paper_config_working_buffer_addr(struct cmdqRecStruct *pkt,
	u32 addr0, u32 addr1)
{
	pp_write(pkt, PAPER_TCTOP_WB_ST_ADDR0, addr0);
	pp_write(pkt, PAPER_TCTOP_WB_ST_ADDR1, addr1);
}

/*
 * config img buffer address
 */
void paper_config_img_buffer_addr(struct cmdqRecStruct *pkt, u32 addr)
{
	pp_write(pkt, PAPER_TCTOP_IMG_ST_ADDR, addr);
}

void paper_config_enable_histogram(struct cmdqRecStruct *pkt,
	bool enable, bool ignore_collision)
{
	pp_write_mask(pkt, PAPER_TCTOP_HIST_CFG0,
		enable << 0 | ignore_collision << 1,
		GENMASK(1, 0));
}

void paper_config_histogram_grey_level(struct cmdqRecStruct *pkt,
	u32 y2_grey_value,
	u32 y4_grey_value,
	u32 y8_grey_value,
	u32 y16_grey_value)
{
	pp_write(pkt, PAPER_TCTOP_HIST_CFG1, y2_grey_value);
	pp_write(pkt, PAPER_TCTOP_HIST_CFG2, y4_grey_value);
	pp_write(pkt, PAPER_TCTOP_HIST_CFG3, y8_grey_value);
	pp_write(pkt, PAPER_TCTOP_HIST_CFG4, y16_grey_value);
}

void paper_get_histogram_info(u32 *current_histogram_level,
	u32 *next_histogram_level,
	u32 *current_grey_level,
	u32 *next_grey_level)
{
	u32 data = 0;

	if (current_histogram_level)
		*current_histogram_level = pp_read(PAPER_TCTOP_HIST_STA0_VA);
	if (next_histogram_level)
		*next_histogram_level = pp_read(PAPER_TCTOP_HIST_STA1_VA);

	data = pp_read(PAPER_TCTOP_HIST_STA2_VA);
	if (current_grey_level)
		*current_grey_level = (data & GENMASK(2, 0)) >> 0;
	if (next_grey_level)
		*next_grey_level = (data & GENMASK(6, 4)) >> 4;
}


u32 paper_get_frame_count(void)
{
	u32 frame_count = 0;

	/* 0x1400700C hard code */
	pp_write_mask(NULL, TCON_GR3, 3 << 20, GENMASK(23, 20));
	/* 0x1400D004 */
	pp_write_mask(NULL, PAPER_TCTOP_MAIN_CTL, 1 << 16, GENMASK(19, 16));
	/* 0x1400D0C0 */
	frame_count = pp_read(PAPER_TCTOP_TCON_FRAME_COUNT_VA);
	//printf("get wf_lut frame count:%d\n", data);
	return frame_count;
}

u32 paper_get_write_buffer_index(void)
{
	return (pp_read(PAPER_TCTOP_BUF_CFG0_VA) & BIT_MASK(31)) >> 31;
}

/* get HWTCON status. */
u32 paper_get_hw_status(int *img_rd_status, int *wb_rd_status,
	int *wb_wr_status, int *pipeline_status, int *wf_lut_status)
{
	/*
	 * bit[8:6] WF_LUT_STATUS:
	 *	00:wf lut idle/wf_lut_done
	 *	01: wdma_done
	 *	10: wf_lut_sof
	 * bit[5:3] PIPELINE_WORK_STATUS:
	 *	00: idle/wdma_done done
	 *	01: main_sof triggle
	 *	10:pipeline_sof tiggle
	 *	11: dma_sof triggle
	 * bit[2] WB_WR_STATUS:
	 *	1: working buffer read done
	 * bit[1] WB_RD_STATUS:
	 *	1: working buffer read done
	 * bit[0] IMG_RD_STATUS:
	 *	1: image buffer read done
	 */
	u32 readback = pp_read(PAPER_TCTOP_STATUS_VA);

	if (img_rd_status)
		*img_rd_status = (readback & BIT_MASK(0)) >> 0;
	if (wb_rd_status)
		*wb_rd_status = (readback & BIT_MASK(1)) >> 1;
	if (wb_wr_status)
		*wb_wr_status = (readback & BIT_MASK(2)) >> 2;
	if (pipeline_status)
		*pipeline_status = (readback & GENMASK(5, 3)) >> 3;
	if (wf_lut_status)
		*wf_lut_status = (readback & GENMASK(8, 6)) >> 6;

	return readback;
}

bool paper_get_dpi_idle_status(void)
{
	/*
	 * write d004[19:16] = 4'h6
	 * read d0c0[17:16], ==1 idle,  not 1 busy
	 */
	u32 value = 0;

	pp_write_mask(NULL, PAPER_TCTOP_MAIN_CTL, 0x6 << 16, GENMASK(19, 16));
	value = pp_read(PAPER_TCTOP_TCON_FRAME_COUNT_VA);
	value = (value & GENMASK(17, 16)) >> 16;

#if 0
	TCON_ERR("d004:0x%08x d0c0:0x%08x value:%d",
		pp_read(PAPER_TCTOP_MAIN_CTL_VA),
		pp_read(PAPER_TCTOP_TCON_FRAME_COUNT_VA),
		value);
#endif

	if (value == 1)
		return true;	/* idle */
	return false;		/* busy */
}

