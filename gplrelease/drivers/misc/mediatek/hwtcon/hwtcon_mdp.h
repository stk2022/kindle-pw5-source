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

#ifndef __HWTCON_MDP_H__
#define __HWTCON_MDP_H__

#include "hwtcon_core.h"
#include "hwtcon_rect.h"

bool hwtcon_mdp_need_use_mdp(const struct mxcfb_update_data *task);

int hwtcon_mdp_convert(struct hwtcon_task *task);

int hwtcon_mdp_copy_buffer_with_region(char *dst_buffer,
	int dst_pitch, const struct rect *dst_region,
	char *src_buffer, int src_pitch,
	const struct rect *src_region);
#endif
