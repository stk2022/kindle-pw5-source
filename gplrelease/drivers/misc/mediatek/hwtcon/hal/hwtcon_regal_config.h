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

#ifndef __HWTCON_REGAL_CONFIG_H__
#define __HWTCON_REGAL_CONFIG_H__

#include <linux/types.h>
#include <linux/mailbox/mtk-cmdq-mailbox.h>
#include <linux/types.h>
#include "cmdq_record.h"

enum REGAL_MODE_ENUM {
	REGAL_MODE_REGAL = 0,
	REGAL_MODE_REGAL_D = 1,
	REGAL_MODE_NIGHT = 2,
	REGAL_MODE_DRAK = 3,
	REGAL_MODE_DARK_D = 4,
	REGAL_MODE_DARK_DU = 5,
};

void hwtcon_regal_config_regal_mode(struct cmdqRecStruct *pkt,
	enum REGAL_MODE_ENUM regal_mode,
	int img_width, int img_height);


#endif /* __HWTCON_REGAL_CONFIG_H__ */
