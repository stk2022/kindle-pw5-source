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

int hwtcon_mdp_convert(struct hwtcon_task *task);

int easy_mtk_mdp_func(
		u32 src_w, u32 src_h,
		u32 dst_w, u32 dst_h,
		u32 src_fmt, u32 dst_fmt,
		u32 src_pitch, u32 dst_pitch,
		dma_addr_t src_mva, dma_addr_t dst_mva,
		u32 crop_x, u32 crop_y,
		u32 crop_w, u32 crop_h,
		u8 dth_en, u32 dth_algo, u8 invert,
		u32 rotate, u32 gamma_flag);

int hwtcon_mdp_copy_buffer_with_region(char *dst_buffer,
	int dst_pitch, const struct rect *dst_region,
	char *src_buffer, int src_pitch,
	const struct rect *src_region);
int hwtcon_mdp_memcpy(dma_addr_t src_buffer, dma_addr_t dst_buffer);

#endif
