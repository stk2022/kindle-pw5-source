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

#ifndef __HWTCON_RDMA_CONFIG_H__
#define __HWTCON_RDMA_CONFIG_H__
#include <linux/types.h>
#include <linux/mailbox/mtk-cmdq-mailbox.h>
#include "cmdq_record.h"

enum RDMA_IRQ_BIT_ENUM {
	RDMA_IRQ_BIT_REG_UPDATE = 0,
	RDMA_IRQ_BIT_FRAME_STAR = 1,
	RDMA_IRQ_BIT_FRAME_END = 2,
	RDMA_IRQ_BIT_EOF_ABNORMAL = 3,
	RDMA_IRQ_BIT_FIFO_UNDERFLOW = 4,
	RDMA_IRQ_BIT_TARGET_LINE = 5,
	RDMA_IRQ_BIT_FIFO_EMPTY = 6,
};

struct rdma_global_config {
	bool ENGINE_EN;	/* bit[0] enable RDMA */
	bool MODE_SEL; /* bit[1] 0: direct link mode 1: memory mode. */
	/* bit[4] write 1 then 0 to reset, need to poll the reset state. */
	bool SOFT_RESET;
};

void rdma_config_image_rdma(struct cmdqRecStruct *pkt);
void rdma_config_wb_rdma(struct cmdqRecStruct *pkt);
void rdma_config_smi_setting(struct cmdqRecStruct *pkt);

#endif /* __HWTCON_RDMA_CONFIG_H__ */
