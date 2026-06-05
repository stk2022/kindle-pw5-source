#ifndef __RDMA_CONFIG_H__
#define __RDMA_CONFIG_H__
#include "cmdq.h"
#include "hwtcon_def.h"

/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2018 MediaTek Inc.
 * Author: Jiaguang Zhang <jiaguang.zhang@mediatek.com>
 */


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
	 bool ENGINE_EN; /* bit[0] enable RDMA */
	 bool MODE_SEL; /* bit[1] 0: direct link mode 1: memory mode. */
	 bool SOFT_RESET; /* bit[4] write 1 then 0 to reset, need to poll the reset state. */
 };

void config_image_rdma(struct cmdq_pkt *pkt);
void config_wb_rdma(struct cmdq_pkt *pkt);


#endif /* endof __RDMA_CONFIG_H__ */
