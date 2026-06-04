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

#ifndef __HWTCON_WF_LUT_CONFIG_H__
#define __HWTCON_WF_LUT_CONFIG_H__
#include <linux/types.h>
#include <linux/mailbox/mtk-cmdq-mailbox.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/consumer.h>
#include "cmdq_record.h"


#define TEMPERATURE_NUM		32
#define WAVEFORM_MODE_NUM	8
#define WAVEFORM_MODE_TOTAL_NUM			12
#define WAVEFORM_SIZE (3*1024*1024)
#define WAVEFORM_ADDR_OFFSET_TO_BEGIN	0xC0
#define WAVEFORM_LEN_OFFSET_TO_BEGIN	0x8C0
#define WAVEFORM_ADDR_OFFSET_PER_TEMP	0x40
#define WAVEFORM_LEN_OFFSET_PER_TEMP	0x40
#define WAVEFORM_TS_TO_BEGIN			0x80
#define WAVEFORM_TS_NUM_TO_BEGIN		0xA2



#define	WF_LUT_WDMA_ADD		0x15006000
/* WF_LUT_WDMA: 0x15006000 */
#define WDMA_INTEN               (WF_LUT_WDMA_ADD + 0x0)
#define WDMA_INTSTA              (WF_LUT_WDMA_ADD + 0x4)
#define WDMA_EN                  (WF_LUT_WDMA_ADD + 0x8)
#define WDMA_RST                 (WF_LUT_WDMA_ADD + 0xC)
#define WDMA_SMI_CON             (WF_LUT_WDMA_ADD + 0x10)
#define WDMA_CFG                 (WF_LUT_WDMA_ADD + 0x14)
#define WDMA_SRC_SIZE            (WF_LUT_WDMA_ADD + 0x18)
#define WDMA_CLIP_SIZE           (WF_LUT_WDMA_ADD + 0x1C)
#define WDMA_CLIP_COORD          (WF_LUT_WDMA_ADD + 0x20)
#define WDMA_DST_W_IN_BYTE       (WF_LUT_WDMA_ADD + 0x28)
#define WDMA_ALPHA               (WF_LUT_WDMA_ADD + 0x2C)
#define WDMA_BUF_CON1            (WF_LUT_WDMA_ADD + 0x38)
#define WDMA_BUF_CON2            (WF_LUT_WDMA_ADD + 0x3C)
#define WDMA_DST_UV_PITCH        (WF_LUT_WDMA_ADD + 0x78)
#define WDMA_DST_ADDR_OFFSET0    (WF_LUT_WDMA_ADD + 0x80)
#define WDMA_FLOW_CTRL_DBG       (WF_LUT_WDMA_ADD + 0xA0)
#define WDMA_EXEC_DBG            (WF_LUT_WDMA_ADD + 0xA4)
#define WDMA_CT_DBG              (WF_LUT_WDMA_ADD + 0xA8)
#define WDMA_SMI_TRAFFIC_DBG     (WF_LUT_WDMA_ADD + 0xAC)
#define WDMA_PROC_TRACK_DBG_0    (WF_LUT_WDMA_ADD + 0xB0)
#define WDMA_PROC_TRACK_DBG_1    (WF_LUT_WDMA_ADD + 0xB4)
#define WDMA_DEBUG               (WF_LUT_WDMA_ADD + 0xB8)
#define WDMA_DUMMY               (WF_LUT_WDMA_ADD + 0x100)
#define WDMA_DST_ADDR0           (WF_LUT_WDMA_ADD + 0xF00)

/* IMGSYS CONFIG: 0x15000000 */
#define	DISP_WDMA0_SEL_IN		0x15000F6C


enum OUTPUT_FORMAT_ENUM {
	OUTPUT_FORMAT_16BIT = 0,
	OUTPUT_FORMAT_8BIT = 1,
};


struct wf_lut_waveform {
	unsigned int start_addr;
	unsigned int len;
	unsigned int waveform_mode;
};

struct wf_lut_wb_rdma {
	unsigned int start_addr;
	unsigned int x;
	unsigned int y;
	unsigned int width;
	unsigned int height;
};

struct wf_lut_con_config {
	unsigned int gray_mode;
	unsigned int width;
	unsigned int height;
	unsigned int rdma_enable_mask;
	unsigned int DECFMT;    //decoder format 1T1pixel or 1T2pixel
	unsigned int layer_greq_num;
	unsigned int checksum_sel;
	unsigned int rg_lut_end_sel;
	unsigned int layer_smi_id_en;
	unsigned int checksum_en;
	unsigned int H_FLIP_EN;
	unsigned int V_FLIP_EN;
	unsigned int wf_lut_en;
	unsigned int wf_lut_inten;
	unsigned int base_addr;
	unsigned int base_addr1;
	unsigned int rg_8b_out;
	unsigned int rg_partial_up_en;
	unsigned int rg_partial_up_val;
	unsigned int rg_default_val;
	unsigned int wf_lut_mout;
	unsigned int byte_swap;
	struct wf_lut_waveform *waveform_table_current;
	unsigned int temperature_index;
	unsigned int direct_link;
	unsigned int rg_de_sel;
	struct wf_lut_wb_rdma wb_rdma[4];
};


void wf_lut_config_context(struct cmdqRecStruct *pkt);
u32 wf_lut_get_irq_status(void);
void wf_lut_clear_irq_status(struct cmdqRecStruct *pkt);
unsigned int wf_lut_get_rdma0_checksum(void);
unsigned int wf_lut_get_rdma1_checksum(void);
unsigned int wf_lut_get_rdma2_checksum(void);
unsigned int wf_lut_get_rdma3_checksum(void);
unsigned int wf_lut_get_wf_lut_output_checksum(void);
void wf_lut_clear_lut_end_irq_status(struct cmdqRecStruct *pkt);
void swtcon_config_context(struct cmdqRecStruct *pkt);
void swdata_hwtcon_config_context(struct cmdqRecStruct *pkt);
void wf_lut_dpi_config_context(struct cmdqRecStruct *pkt);
void wf_lut_waveform_select_by_temp(struct cmdqRecStruct *pkt, int temp);

void hwtcon_edp_pinmux_control(struct platform_device *pdev);
void hwtcon_edp_pinmux_active(void);
void hwtcon_edp_pinmux_inactive(void);
void hwtcon_edp_pinmux_release(void);

unsigned int wf_lut_get_waveform_len(int temp, int mode);

unsigned int *wf_lut_get_waveform_mode_in_hardware(void);
void wf_lut_waveform_replace(struct cmdqRecStruct *pkt,
		int old_index, int new_mode);
int wf_lut_waveform_get_temperature_threshold(unsigned char *waveform_addr);
int wf_lut_waveform_get_temperature_index(int temperature);
char *wf_lut_waveform_get_name(void);

void wf_lut_waveform_slot_association(struct cmdqRecStruct *pkt,
	unsigned int mode, unsigned int temp);

#endif /* __HWTCON_WF_LUT_CONFIG_H__ */
