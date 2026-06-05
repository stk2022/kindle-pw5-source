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

#ifndef __HWTCON_WF_LUT_RDMA_CONFIG_H__
#define __HWTCON_WF_LUT_RDMA_CONFIG_H__
#include <linux/types.h>
#include <linux/mailbox/mtk-cmdq-mailbox.h>
#include "cmdq_record.h"
#include "hwtcon_hal.h"

struct rdma_global_config {
	bool ENGINE_EN;	/* bit[0] enable RDMA */
	bool MODE_SEL; /* bit[1] 0: direct link mode 1: memory mode. */
	/* bit[4] write 1 then 0 to reset, need to poll the reset state. */
	bool SOFT_RESET;
};

void wf_lut_rdma_config_rdma(struct cmdqRecStruct *pkt,
	unsigned int width,
	unsigned int height,
	unsigned int inputFormat, unsigned int mode_sel);

void wf_lut_clear_disp_rdma_irq_status(struct cmdqRecStruct *pkt);
void wf_lut_rdma_config_smi_setting(struct cmdqRecStruct *pkt);

#endif /* __HWTCON_WF_LUT_RDMA_CONFIG_H__ */
