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
#include <linux/videodev2.h>
#include "hwtcon_fb.h"
#include "hwtcon_epd.h"
#include "hwtcon_debug.h"
#include "hwtcon_mdp.h"
#include "hwtcon_def.h"
#include "mtk_mdp_gamma.h"

int hwtcon_mdp_copy_buffer_with_region(char *dst_buffer,
	int dst_pitch, const struct rect *dst_region,
	char *src_buffer, int src_pitch,
	const struct rect *src_region)
{
	int i = 0;
	int j = 0;

	if (src_region->width != dst_region->width ||
		src_region->height != dst_region->height) {
		TCON_ERR("copy src & dst region not match");
		return HWTCON_STATUS_REGION_NOT_MATCH;
	}

	if (dst_pitch < dst_region->width) {
		TCON_ERR("invalid region: dst(%d %d %d %d) dst pitch:%d",
			dst_region->x,
			dst_region->y,
			dst_region->width,
			dst_region->height,
			dst_pitch);
		return HWTCON_STATUS_INVALID_PARAM;
	}
	if (src_pitch < src_region->width) {
		TCON_ERR("invalid region: src(%d %d %d %d) src pitch:%d",
			src_region->x,
			src_region->y,
			src_region->width,
			src_region->height,
			src_pitch);
		return HWTCON_STATUS_INVALID_PARAM;
	}

	for (i = 0; i < src_region->width; i++)
		for (j = 0; j < src_region->height; j++) {
			/* copy src region(src_region.x, src_region.y)
			 * to dst(dst_region.x, dst_region.y)
			 */
			dst_buffer[(dst_region->y + j) * dst_pitch +
				(dst_region->x + i)] =
				src_buffer[(src_region->y + j) * src_pitch +
				(src_region->x + i)] >> 4 << 4;
		}

	return 0;
}

int hwtcon_mdp_convert(struct hwtcon_task *task)
{
	u32 src_buffer_pa = 0;
	u32 dst_buffer_pa = 0;
	u32 src_buffer_width = 0;
	u32 dst_buffer_pitch = 0;
	u32 src_buffer_height = 0;
	u32 dst_buffer_height = 0;
	struct rect src_region = {0};
	struct rect dst_region = {0};
	u32 src_format = V4L2_PIX_FMT_Y8;
	u32 dst_format = V4L2_PIX_FMT_Y4_M0;
	u8 dither_enable = 0;
	u8 invert_enable = 0;
	int rotate = hwtcon_fb_get_rotation() * 90;
	u32 gamma_flag = MDP_GAMMA_USE_CMAP;

	if (task->update_data.flags & EPDC_FLAG_ENABLE_INVERSION)
		gamma_flag |= MDP_GAMMA_ENABLE_INVERSION;

	if (task->update_data.flags & EPDC_FLAG_FORCE_MONOCHROME)
		gamma_flag |= MDP_GAMMA_FORCE_MONOCHROME;
	

	src_region = hwtcon_core_get_mdp_region(task);
	dst_region = hwtcon_core_get_task_region(task);

	hwtcon_core_get_mdp_input_buffer_info(task, &src_buffer_pa,
		&src_buffer_width, &src_buffer_height);
	hwtcon_core_get_task_buffer_info(task, &dst_buffer_pa,
			&dst_buffer_pitch, &dst_buffer_height);

	if (task->update_data.flags & EPDC_FLAG_USE_DITHERING_Y4)
		dither_enable = 1;

	if (task->update_data.flags & EPDC_FLAG_ENABLE_INVERSION)
		invert_enable = !invert_enable;
	/* night mode, need to invert every update */
	if (hwtcon_fb_get_grayscale() == GRAYSCALE_8BIT_INVERTED)
		invert_enable = !invert_enable;

	TCON_LOG("[MARKER]:%d MDP convert, dither:%d invert:%d rotate:%d degree gamma:0x%08x",
		task->update_data.update_marker,
		dither_enable, invert_enable, rotate, gamma_flag);
	if (rotate) {
		TCON_LOG("[MARKER]:%d src width:%d height:%d region:[%d %d %d %d]",
			task->update_data.update_marker,
			src_buffer_width, src_buffer_height,
			src_region.x,
			src_region.y,
			src_region.width,
			src_region.height);
		TCON_LOG("MARKER:%d dst width:%d height:%d region:[%d %d %d %d]",
			task->update_data.update_marker,
			dst_buffer_pitch, dst_buffer_height,
			dst_region.x,
			dst_region.y,
			dst_region.width,
			dst_region.height);
	}

	easy_mtk_mdp_func(src_buffer_width, src_buffer_height,
				dst_buffer_pitch, dst_buffer_height,
				src_format, dst_format,
				hwtcon_fb_get_virtual_width(), dst_buffer_pitch,
				src_buffer_pa, dst_buffer_pa,
				src_region.x,
				src_region.y,
				src_region.width,
				src_region.height,
				dither_enable, MDP_DITHER_ALGO_Y8_Y4_S,
				invert_enable,
				rotate, gamma_flag);
	return 0;
}

int hwtcon_mdp_memcpy(dma_addr_t dst_buffer, dma_addr_t src_buffer)
{
	int buffer_width = hw_tcon_get_edp_width();
	int buffer_height = hw_tcon_get_edp_height();

	return easy_mtk_mdp_func(buffer_width,
				buffer_height,
				buffer_width,
				buffer_height,
				V4L2_PIX_FMT_Y8, V4L2_PIX_FMT_Y8,
				buffer_width, buffer_width,
				src_buffer, dst_buffer,
				0,
				0,
				buffer_width,
				buffer_height,
				0, MDP_DITHER_ALGO_Y8_Y4_S,
				0, 0, 0);
}

