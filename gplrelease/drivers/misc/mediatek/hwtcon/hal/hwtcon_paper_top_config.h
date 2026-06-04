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

#ifndef __HWTCON_PAPER_TOP_CONFIG_H__
#define __HWTCON_PAPER_TOP_CONFIG_H__
#include <linux/types.h>
#include <linux/mailbox/mtk-cmdq-mailbox.h>
#include "cmdq_record.h"

#include "hwtcon_def.h"
#include "hwtcon_rect.h"

enum PAPER_TOP_INIT_MODE_ENUM {
	PAPER_TOP_INIT_MODE_PRE_CUR_USE_REG_VAL = 0 << 0,
	PAPER_TOP_INIT_MODE_PRE_USE_REG_VAL_CUR_USE_IMG_BUF_DATA = 1 << 0,
	PAPER_TOP_INIT_MODE_PRE_USE_WB_DATA_CUR_USE_IMG_BUF_DATA = 2 << 0,
};

enum MAIN_SOF_MODE_ENUM {
	/* first main sof frame use img buffer last update trigger.
	 * then when wb_wdma first frame done will trigger dpi vsync.
	 * the following main sof frame will use dpi vsync trigger.
	 */
	MAIN_SOF_MODE_IMG_LAST_UPDATE_AND_DPI_VSYNC = 0 << 1,
	/* use dpi vsync for main sof trigger.
	 * software need to enable dpi vsync first in this mode.
	 * DPI_EN_SEL = DPI_VSYNC_SEL_AUTO.
	 */
	MAIN_SOF_MODE_DPI_VSYNC = 1 << 1,
	/*
	 * use LUT region last update trigger main sof.
	 */
	MAIN_SOF_MODE_LAST_LUT_UPDATE = 2 << 1,
};

enum DPI_VSYNC_SEL_ENUM {
	/* DPI VSYNC internal enable, config DPI register to enable. */
	DPI_VSYNC_SEL_SW = 0 << 0,
	/* DPI VSYNC auto. PIPELINE WDMA frame done enable DPI vsync,
	 *wf lut frame done disable dpi vsync
	 */
	DPI_VSYNC_SEL_AUTO = 1 << 0,
};

enum SOF_SEL_HW_BIT_ENUM {
	SOF_SEL_HW_BIT_PIPELINE = 0,
	SOF_SEL_HW_BIT_IMG_RDMA = 1,
	SOF_SEL_HW_BIT_WB_RDMA = 2,
	SOF_SEL_HW_BIT_WB_WDMA = 3,
	SOF_SEL_HW_BIT_WF_LUT = 6,
	SOF_SEL_HW_BIT_LUT_MERGE = 7,
};


enum PAPER_UPDATE_MODE_ENUM {
	PAPER_UPDATE_MODE_FULL = 0,	/* default value */
	PAPER_UPDATE_MODE_PARTIAL = 1,
};

enum PIPELINE_SOF_SEL_ENUM {
	PIPELINE_SOF_SEL_AUTO = BIT_USE_AUTO_SOF(SOF_SEL_HW_BIT_PIPELINE),
	PIPELINE_SOF_SEL_SW = BIT_USE_SW_SOF(SOF_SEL_HW_BIT_PIPELINE),
};

enum IMG_RDMA_SOF_SEL_ENUM {
	IMG_RDMA_SOF_SEL_AUTO = BIT_USE_AUTO_SOF(SOF_SEL_HW_BIT_IMG_RDMA),
	IMG_RDMA_SOF_SEL_SW = BIT_USE_SW_SOF(SOF_SEL_HW_BIT_IMG_RDMA),
};

enum WB_RDMA_SOF_SEL_ENUM {
	WB_RDMA_SOF_SEL_AUTO = BIT_USE_AUTO_SOF(SOF_SEL_HW_BIT_WB_RDMA),
	WB_RDMA_SOF_SEL_SW = BIT_USE_SW_SOF(SOF_SEL_HW_BIT_WB_RDMA),
};

enum WB_WDMA_SOF_SEL_ENUM {
	WB_WDMA_SOF_SEL_AUTO = BIT_USE_AUTO_SOF(SOF_SEL_HW_BIT_WB_WDMA),
	WB_WDMA_SOF_SEL_SW = BIT_USE_SW_SOF(SOF_SEL_HW_BIT_WB_WDMA),
};

enum WF_LUT_SOF_SEL_ENUM {
	WF_LUT_SOF_SEL_AUTO = BIT_USE_AUTO_SOF(SOF_SEL_HW_BIT_WF_LUT),
	WF_LUT_SOF_SEL_SW = BIT_USE_SW_SOF(SOF_SEL_HW_BIT_WF_LUT),
};

enum LUT_MERGE_SOF_SEL_ENUM {
	LUT_MERGE_SOF_SEL_AUTO = BIT_USE_AUTO_SOF(SOF_SEL_HW_BIT_LUT_MERGE),
	LUT_MERGE_SOF_SEL_SW = BIT_USE_SW_SOF(SOF_SEL_HW_BIT_LUT_MERGE),
};

#if 0
enum PAPER_SOF_SEL_ENUM {
	PAPER_SOF_SEL_PIPELINE_AUTO =
		BIT_USE_AUTO_SOF(SOF_SEL_HW_BIT_PIPELINE),
	PAPER_SOF_SEL_PIPELINE_SW =
		BIT_USE_SW_SOF(SOF_SEL_HW_BIT_PIPELINE),
	PAPER_SOF_SEL_IMG_RDMA_AUTO =
		BIT_USE_AUTO_SOF(SOF_SEL_HW_BIT_IMG_RDMA),
	PAPER_SOF_SEL_IMG_RDMA_SW =
		BIT_USE_SW_SOF(SOF_SEL_HW_BIT_IMG_RDMA),
	PAPER_SOF_SEL_WB_RDMA_AUTO =
		BIT_USE_AUTO_SOF(SOF_SEL_HW_BIT_WB_RDMA),
	PAPER_SOF_SEL_WB_RDMA_SW =
		BIT_USE_SW_SOF(SOF_SEL_HW_BIT_WB_RDMA),
	PAPER_SOF_SEL_WB_WDMA_AUTO =
		BIT_USE_AUTO_SOF(SOF_SEL_HW_BIT_WB_WDMA),
	PAPER_SOF_SEL_WB_WDMA_SW =
		BIT_USE_SW_SOF(SOF_SEL_HW_BIT_WB_WDMA),
	PAPER_SOF_SEL_WF_LUT_AUTO =
		BIT_USE_AUTO_SOF(SOF_SEL_HW_BIT_WF_LUT),
	PAPER_SOF_SEL_WF_LUT_SW =
		BIT_USE_SW_SOF(SOF_SEL_HW_BIT_WF_LUT),
	PAPER_SOF_SEL_LUT_MERGE_AUTO =
		BIT_USE_AUTO_SOF(SOF_SEL_HW_BIT_LUT_MERGE),
	PAPER_SOF_SEL_LUT_MERGE_SW =
		BIT_USE_SW_SOF(SOF_SEL_HW_BIT_LUT_MERGE),
};
#endif


/* clock control bit.
 * bit0: pipeline use hwgate
 * bit1: img_rdam use hw gate
 * bit2: wb_rdma use hw gate
 * bit3: wb_wdma use hw gate
 * bit4: wf_lut use hw gate
 * bit5: dpi use hw gate
 * bit6: tcon use hw gate
 */
enum HW_GATE_CTL_BIT_ENUM {
	HW_GATE_CTL_BIT_PIPELINE = 0,
	HW_GATE_CTL_BIT_IMG_RDMA = 1,
	HW_GATE_CTL_BIT_WB_RDMA = 2,
	HW_GATE_CTL_BIT_WB_WDMA = 3,
	HW_GATE_CTL_BIT_WF_LUT = 4,
	HW_GATE_CTL_BIT_DPI = 5,
	HW_GATE_CTL_BIT_TCON = 6,
	HW_GATE_CTL_BIT_MAX,
};


enum DMA_CONFIG_SOURCE_SEL_ENUM {
	/* use image RDMA / WB_RDMA / WB_WDMA internal config parameter. */
	DMA_CONFIG_SOURCE_SEL_INTERNAL_CONFIG = 0 << 0,
	/* use HW auto calclulate. */
	DMA_CONFIG_SOURCE_SEL_AUTO_CONFIG = 1 << 0,
};

enum WB_BUFFER_MODE_ENUM {
	WB_BUFFER_MODE_PINGPONG = 0 << 1,
	WB_BUFFER_MODE_SINGLE = 1 << 1,
};

enum IMG_BUFFER_SIZE_SEL_ENUM {
	/* img buffer size is same with panel size. */
	IMG_BUFFER_SIZE_SEL_PANEL = 0 << 5,
	/* img buffer size = update LUT size. */
	IMG_BUFFER_SIZE_SEL_SINGLE = 1 << 5,
};

enum IMG_BUFFER_FORMAT_ENUM {
	/* {Y4, 4'b0} */
	IMG_BUFFER_FORMAT_Y4_MODE1 = 0 << 4,
	/* {Y4_1, Y4_0} */
	IMG_BUFFER_FORMAT_Y4_MODE2 = 1 << 4,
};

enum WB_READ_INDEX_ENUM {
	/* wb rdma read index 0 && wb wdma write index 1 */
	WB_READ_INDEX_ADDR0 = 0 << 3,
	/* wb rdma read index 1 && wb wdma write index 0 */
	WB_READ_INDEX_ADDR1 = 1 << 3,
};

enum BUF_PITCH_SEL_ENUM {
	/* buffer pitch is same with PANEL width(for image buffer)
	 * or PANEL width * 2 (for working buffer)
	 */
	BUF_PITCH_SEL_FROM_PANEL = 0,
	/* buffer pitch is configured by software with
	 * register PAPER_TCTOP_BUF_CFG1
	 */
	BUF_PITCH_SEL_FROM_SW_CONFIG = 1,
};

enum PRE_BUF_UPDATE_MODE_ENUM {
	/* previous buffer only update lut region with current buffer */
	PRE_BUF_UPDATE_MODE_ONLY_UPDATE_LUT_REGION = 0 << 1,
	/* previous buffer update whole region with
	 * current buffer same as: previous = current.
	 */
	PRE_BUF_UPDATE_MODE_UPDATE_WHOLE_REGION = 1 << 1,
};


struct paper_top_init_wb_config {
	enum PAPER_TOP_INIT_MODE_ENUM INI_MODE; /*bit[1:0]*/
	int INI_PRE_DATA; /* bit[12:8]*/
	int INI_CUR_DATA; /* bit[20:16]*/
	enum WAVEFORM_MODE_ENUM INI_WF_TYPE; /* bit[27:24] */
	bool INI_REQ; /* bit[31]*/
};


struct paper_top_sof_config {
	enum MAIN_SOF_MODE_ENUM main_sof_mode;	/* bit[2:1] */
	u32 wf_lut_sof_position; /* bit[31:0] */
	u32 lut_merge_sof_position; /* bit[31:0] */
	u32 pipeline_sof_position; /* bit[31:16] */
};

struct paper_top_fifo_config {
	int FIFO_READ_START_TH; /* bit[7:0] */
	int FIFO_SIZE;	/* bit[15:8] */
	int FIFO_EN;	/* bit[16] */
};

struct update_lut_config {
	enum WAVEFORM_MODE_ENUM waveform_mode;
	struct rect lut_region;
	bool is_last_lut;
};




/* provide API for paper register config */

/*
 * enable HW Clock.
 * one call only open 1 HW. Need to call many times to enable HWs.
 */
void paper_enable_hw_clock(struct cmdqRecStruct *pkt,
	enum HW_GATE_CTL_BIT_ENUM hw_id);

/* config update mode: partitial update or full update */
void paper_config_update_mode(struct cmdqRecStruct *pkt,
	enum PAPER_UPDATE_MODE_ENUM mode);

/* config pipeline fake write enable */
void paper_config_fake_write_enable(struct cmdqRecStruct *pkt, bool enable);

/* init working buffer content */
void paper_init_working_buffer(struct cmdqRecStruct *pkt,
	enum PAPER_TOP_INIT_MODE_ENUM init_mode,
	int init_pre_pixel_data,
	int init_cur_pixel_data,
	enum WAVEFORM_MODE_ENUM init_wf_mode);

void paper_config_pre_buffer_region(struct cmdqRecStruct *pkt,
	enum PRE_BUF_UPDATE_MODE_ENUM mode);

/* config panel width & height */
void paper_config_panel_size(struct cmdqRecStruct *pkt,
	int panel_width, int panel_height);

/* enable regal */
void paper_regal_enable(struct cmdqRecStruct *pkt, bool enable);

/* clear regal interrupt */
void paper_regal_clear_interrupt(struct cmdqRecStruct *pkt);

/* read regal irq status. */
bool paper_regal_read_interrupt_status(void);

/* config data process fifo */
void paper_config_data_process_fifo(struct cmdqRecStruct *pkt,
	bool enable_fifo,
	int fifo_size, int fifo_read_start_threshold);

/* config hw sof select from HW or software  */
void paper_config_sof_sel(struct cmdqRecStruct *pkt,
	enum IMG_RDMA_SOF_SEL_ENUM img_rdma_sof_sel,
	enum WB_RDMA_SOF_SEL_ENUM wb_rdma_sof_sel,
	enum WB_WDMA_SOF_SEL_ENUM wb_wdma_sof_sel,
	enum PIPELINE_SOF_SEL_ENUM pipeline_sof_sel,
	enum WF_LUT_SOF_SEL_ENUM wf_lut_sof_sel,
	enum LUT_MERGE_SOF_SEL_ENUM lut_merge_sof_sel);

/* config main sof : how main sof be triggered. */
void paper_config_main_sof_mode(struct cmdqRecStruct *pkt,
	enum MAIN_SOF_MODE_ENUM main_sof_mode);

/* config the max counter of cycle
 * the timer will start when main sof comes,
 * and end when counter = this max counter
 * ther hw sof position must < max counter.
 * otherwise this hw sof will never come.
 */
void paper_config_main_sof_max_counter(struct cmdqRecStruct *pkt,
	u32 max_counter);

/* config dpi vsync trigger mode: trigger by WB WDMA SOF or HW auto. */
void paper_config_dpi_vsync_trigger_mode(struct cmdqRecStruct *pkt,
	enum DPI_VSYNC_SEL_ENUM mode);

/* pipeline sof position.  Delay cycle of main sof */
void paper_config_pipeline_sof_position(struct cmdqRecStruct *pkt,
	u32 pipeline_sof_position);

/* lut merge sof position. must ready before wf_lut work,
 * so must before than wf_lut sof
 */
void paper_config_lut_merge_sof_position(struct cmdqRecStruct *pkt,
	u32 lut_merge_sof_position);

/* wf lut sof position. Delay cycle of main sof */
void paper_config_wf_lut_sof_position(struct cmdqRecStruct *pkt,
	u32 wf_lut_sof_position);

/* config software pipeline sof potion  Delay cycle of main sof */
void paper_config_sw_pipeline_sof_position(struct cmdqRecStruct *pkt,
	u32 pipeline_sof_position);

/* config software img rdma / wb rdma / wb wdma
 * sof position Delay cycle of main sof
 */
void paper_config_sw_dma_sof_position(struct cmdqRecStruct *pkt,
	u32 dma_sof_position);

/* update a region */
void paper_config_update_lut(struct cmdqRecStruct *pkt,
	const struct update_lut_config *lut_config);

int paper_get_config_lut_number(void);

void paper_get_config_lut_info(struct cmdqRecStruct *pkt, int index,
	enum WAVEFORM_MODE_ENUM *wf_mode, struct rect *region);

/* config image buffer pitch.
 * if pitch_config_type == BUF_PITCH_SEL_FROM_PANEL,
 * the pitch param will have no use.
 * if pitch_config_type == BUF_PITCH_SEL_FROM_SW_CONFIG,
 * the pitch will take effect.
 */
void paper_config_image_buffer_pitch(struct cmdqRecStruct *pkt,
	enum BUF_PITCH_SEL_ENUM pitch_config_type, int pitch);

/* config working buffer pitch.
 * if pitch_config_type == BUF_PITCH_SEL_FROM_PANEL,
 * the pitch param will have no use.
 * if pitch_config_type == BUF_PITCH_SEL_FROM_SW_CONFIG,
 * the pitch will take effect.
 */
void paper_config_working_buffer_pitch(struct cmdqRecStruct *pkt,
	enum BUF_PITCH_SEL_ENUM pitch_config_type, int pitch);

/*
 * config dma(img rdma / wb rdma / wb wdma) method.
 * CONFIG_DMA_SOURCE_SEL_INTERNAL_CONFIG:
 * use img rdma / wb rdma / wb wdma internal config parameter.
 * CONFIG_DMA_SOURCE_SEL_AUTO_CONFIG:
 * paper top auto calculate. will not use DMA hw internal config param.
 */
void paper_config_dma_source_select(struct cmdqRecStruct *pkt,
	enum DMA_CONFIG_SOURCE_SEL_ENUM config_source);

/*
 * config img buffer read size
 */
void paper_config_img_buffer_size(struct cmdqRecStruct *pkt,
	enum IMG_BUFFER_SIZE_SEL_ENUM select_mode);

/*
 * config img buffer format. {Y4, 4'b0}  or {Y4_1, Y4_0}
 */
void paper_config_img_buffer_format(struct cmdqRecStruct *pkt,
	enum IMG_BUFFER_FORMAT_ENUM format);

/* config working buffer mode: signal mode / pingpong mode */
void paper_config_working_buffer_mode(struct cmdqRecStruct *pkt,
	enum WB_BUFFER_MODE_ENUM mode);

/*
 * config working buffer read buffer index.
 * if index = WB_READ_INDEX_ADDR0: rdma read 0, wdma write 1.
 * if index = WB_READ_INDEX_ADDR1: rdma read 1, wdma write 0.
 */
void paper_config_working_buffer_start_index(struct cmdqRecStruct *pkt,
	enum WB_READ_INDEX_ENUM index);

/*
 * config working buffer address
 */
void paper_config_working_buffer_addr(struct cmdqRecStruct *pkt,
	u32 addr0, u32 addr1);

/*
 * config img buffer address
 */
void paper_config_img_buffer_addr(struct cmdqRecStruct *pkt, u32 addr);

void paper_config_enable_histogram(struct cmdqRecStruct *pkt,
	bool enable, bool ignore_collision);

void paper_config_histogram_grey_level(struct cmdqRecStruct *pkt,
	u32 y2_grey_value,
	u32 y4_grey_value,
	u32 y8_grey_value,
	u32 y16_grey_value);

void paper_get_histogram_info(u32 *current_histogram_level,
	u32 *next_histogram_level,
	u32 *current_grey_level,
	u32 *next_grey_level);

u32 paper_get_write_buffer_index(void);

/* get waveform frame count */
u32 paper_get_frame_count(void);

/* get HWTCON status. */
u32 paper_get_hw_status(int *img_rd_status, int *wb_rd_status,
	int *wb_wr_status, int *pipeline_status, int *wf_lut_status);

bool paper_get_dpi_idle_status(void);


#endif /* __HWTCON_PAPER_TOP_CONFIG_H__ */
