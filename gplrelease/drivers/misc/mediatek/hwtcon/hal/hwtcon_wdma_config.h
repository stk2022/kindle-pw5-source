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

#ifndef __HWTCON_WDMA_CONFIG_H__
#define __HWTCON_WDMA_CONFIG_H__
#include <linux/types.h>
#include <linux/mailbox/mtk-cmdq-mailbox.h>
#include "hwtcon_def.h"
#include "cmdq_record.h"

void wdma_reset_hw(struct cmdqRecStruct *pkt);
void wdma_config_color_format(struct cmdqRecStruct *pkt);
void wdma_config_fifo(struct cmdqRecStruct *pkt, u32 issue_request_th,
	u32 fifo_size);
void wdma_config_enable_ultra(struct cmdqRecStruct *pkt, bool enable);
void wdma_config_enable_preultra(struct cmdqRecStruct *pkt, bool enable);
void wdma_config_enable_frame_end_ultra(struct cmdqRecStruct *pkt,
	bool enable);
void wdma_config_buffer_pitch(struct cmdqRecStruct *pkt, u32 pitch);
void wdma_config_buffer_size(struct cmdqRecStruct *pkt, u32 width,
	u32 height);
void wdma_config_crop_size(struct cmdqRecStruct *pkt, u32 x, u32 y, u32 width,
	u32 height);
void wdma_config_buffer_addr(struct cmdqRecStruct *pkt, u32 addr);
void wdma_config_enable_interrupt(struct cmdqRecStruct *pkt, bool enable);
void wdma_enable_hw(struct cmdqRecStruct *pkt);
void wdma_wait_hw_done(void);
u32 wdma_config_get_irq_status(void);
void wdma_config_clear_irq_status(struct cmdqRecStruct *pkt);

#endif /* __HWTCON_WDMA_CONFIG_H__ */
