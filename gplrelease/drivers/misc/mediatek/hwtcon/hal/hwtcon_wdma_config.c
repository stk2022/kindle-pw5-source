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

#include "hwtcon_wdma_config.h"
#include "hwtcon_hal.h"

#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/iopoll.h>

void wdma_reset_hw(struct cmdqRecStruct *pkt)
{
	#define MAX_LOOP_COUNT 1000

	/* reset WB WDMA */
	pp_write(pkt, PP_WDMA_RST, 0x1);
	pp_write(pkt, PP_WDMA_RST, 0x0);

	if (pkt) {
		/* use gce poll */
		pp_poll(pkt, PP_WDMA_FLOW_CTRL_DBG, 0, GENMASK(9, 0));
	} else {
		int status = 0;
		u32 value = 0;

		/* use cpu poll */
		status = readl_poll_timeout_atomic(PP_WDMA_FLOW_CTRL_DBG_VA,
			value,
			(value & GENMASK(9, 0)) == 1,
			0,
			1000);
		/* return polling result */
		if (status == -ETIMEDOUT) {
			TCON_ERR("reset WB_WDMA timeout:0x%x",
				pp_read(PP_WDMA_FLOW_CTRL_DBG_VA));
			return;
		}

		TCON_LOG("reset WDMA done");
	}
}

void wdma_config_color_format(struct cmdqRecStruct *pkt)
{
	/* config WB WDMA output format: 0x5 YUY2*/
	pp_write(pkt, PP_WDMA_CFG, 0x50);
}

void wdma_config_fifo(struct cmdqRecStruct *pkt, u32 issue_request_th,
	u32 fifo_size)
{
	/* config wb WDMA FIFO */
	#if 0
	struct wb_wdma_fifo_config fifo_config = {0};
	/* fifo_config.Frame_End_Ultra = true; */
	fifo_config.issue_req_th = 5;
	fifo_config.fifo_pseudo_size = 0x80;
	#endif
	pp_write_mask(pkt, PP_WDMA_BUF_CON1,
		issue_request_th << 16 |
		fifo_size,
		GENMASK(24, 16) |
		GENMASK(8, 0));
}

void wdma_config_enable_ultra(struct cmdqRecStruct *pkt, bool enable)
{
	pp_write_mask(pkt, PP_WDMA_BUF_CON1, enable << 31, BIT_MASK(31));
}

void wdma_config_enable_preultra(struct cmdqRecStruct *pkt, bool enable)
{
	pp_write_mask(pkt, PP_WDMA_BUF_CON1, enable << 30, BIT_MASK(30));
}

void wdma_config_enable_frame_end_ultra(struct cmdqRecStruct *pkt,
	bool enable)
{
	pp_write_mask(pkt, PP_WDMA_BUF_CON1, enable << 28, BIT_MASK(28));
}

void wdma_config_buffer_pitch(struct cmdqRecStruct *pkt, u32 pitch)
{
	pp_write(pkt, PP_WDMA_DST_W_IN_BYTE, pitch);
}

void wdma_config_buffer_size(struct cmdqRecStruct *pkt, u32 width, u32 height)
{
	pp_write(pkt, PP_WDMA_SRC_SIZE, height << 16 | width);
}

void wdma_config_crop_size(struct cmdqRecStruct *pkt, u32 x, u32 y,
	u32 width, u32 height)
{
	pp_write(pkt, PP_WDMA_CLIP_SIZE, height << 16 | width);
	pp_write(pkt, PP_WDMA_CLIP_COORD, y << 16 | x);
}

void wdma_config_buffer_addr(struct cmdqRecStruct *pkt, u32 addr)
{
	pp_write(pkt, PP_WDMA_DST_ADDR0, addr);
}

void wdma_config_enable_interrupt(struct cmdqRecStruct *pkt, bool enable)
{
	if (enable)
		pp_write(pkt, PP_WDMA_INTEN, 0x3);	/* enable WDMA irq */
	else
		pp_write(pkt, PP_WDMA_INTEN, 0x0);	/* disable WDMA irq */
}

u32 wdma_config_get_irq_status(void)
{
	/*
	 * bit 0: Frame complete.
	 * bit 1: Frame under run.
	 */
	return pp_read(PP_WDMA_INTSTA_VA);
}

void wdma_config_clear_irq_status(struct cmdqRecStruct *pkt)
{
	pp_write(pkt, PP_WDMA_INTSTA, 0x0);
}

void wdma_enable_hw(struct cmdqRecStruct *pkt)
{
	pp_write(pkt, PP_WDMA_EN, 1);
}

void wdma_wait_hw_done(void)
{
	while (((pp_read(PP_WDMA_INTSTA_VA) >> 0) & 0x1) != 1)
		;
	//printf("wait pipeline end:0x%x\n", pp_read(PP_WDMA_INTSTA_VA));
	pp_write(NULL, PP_WDMA_INTSTA, 0);	/* clear wdma interrupt */
}

