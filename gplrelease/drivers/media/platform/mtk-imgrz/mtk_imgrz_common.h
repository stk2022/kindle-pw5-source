/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Scott Wang <Scott.Wang@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef MTK_IMGRZ_COMMON_H
#define MTK_IMGRZ_COMMON_H

#include <linux/wait.h>
#include <linux/mutex.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fh.h>

#ifdef CONFIG_VIDEO_MEDIATEK_JDEC
#define MTK_IMGRZ_JDEC_DL 1
#else
#define MTK_IMGRZ_JDEC_DL 0
#endif

#define IMGRZ_FPGA_EP	0

#define MTK_IMGRZ_LOG_DEBUG	0x10000
#define MTK_IMGRZ_LOG_INFO	0x20000
#define MTK_IMGRZ_LOG_DETAL	0x30000

enum mtk_imgrz_yc_format {
	MTK_IMGRZ_YC_FORMAT_420,
	MTK_IMGRZ_YC_FORMAT_422,
	MTK_IMGRZ_YC_FORMAT_444
};

enum mtk_imgrz_argb_format {
	MTK_IMGRZ_ARGB_FORMAT_0565,
	MTK_IMGRZ_ARGB_FORMAT_1555,
	MTK_IMGRZ_ARGB_FORMAT_4444,
	MTK_IMGRZ_ARGB_FORMAT_8888,
};

enum mtk_imgrz_color_format {
	MTK_IMGRZ_COLOR_FORMAT_UNKNOWN,
	MTK_IMGRZ_COLOR_FORMAT_Y_C,
	MTK_IMGRZ_COLOR_FORMAT_Y_CB_CR,
	MTK_IMGRZ_COLOR_FORMAT_INDEX,
	MTK_IMGRZ_COLOR_FORMAT_ARGB,
	MTK_IMGRZ_COLOR_FORMAT_AYUV,
	MTK_IMGRZ_COLOR_FORMAT_GREY
};

enum mtk_imgrz_scale_mode {
	MTK_IMGRZ_NONE_SCALE,
	MTK_IMGRZ_FRAME_SCALE,
	MTK_IMGRZ_PARTIAL_SCALE
};

enum mtk_imgrz_resample_method {
	MTK_IMGRZ_M_TAP_RESAMPLE, /* multi-tap */
	MTK_IMGRZ_4_TAP_RESAMPLE,
	MTK_IMGRZ_8_TAP_RESAMPLE
};

enum mtk_imgrz_state {
	IMGRZ_STATE_IDLE = 0,
	IMGRZ_STATE_BUSY = 1,
	IMGRZ_STATE_DL_BUSY = 2,
	IMGRZ_STATE_DONE = 3,
	IMGRZ_STATE_DL_DONE = 4,
	IMGRZ_STATE_TIMEOUT = 5,
	IMGRZ_STATE_STOP = 6
};

struct mtk_imgrz_partial_info {
	bool	first_row;
	bool	last_row;
	unsigned int	cur_row;
	unsigned int	rowbuf_hei;
	dma_addr_t	pre_row_addr[3];
	dma_addr_t	cur_row_addr[3];
	dma_addr_t	temp_buf;
};

struct mtk_imgrz_buf_info {
	enum mtk_imgrz_color_format	fmt;
	union {
		enum mtk_imgrz_yc_format yc_fmt;
		enum mtk_imgrz_argb_format argb_fmt;
	} f;

	u16		buf_width[3];   /* Known as pitch. */
	u16		buf_height[3];
	unsigned int	pic_width;
	unsigned int	pic_height;
	unsigned int	x_offset;
	unsigned int	y_offset;
	dma_addr_t	dma_addr[3];

	unsigned int	h_sample[3];
	unsigned int	v_sample[3];

	bool block;
	bool progressive;
	bool top_field;
	bool bit10;

};

struct mtk_imgrz_scale_param {
	enum mtk_imgrz_scale_mode	scale_mode;
	enum mtk_imgrz_resample_method	h_method;
	enum mtk_imgrz_resample_method	v_method;
	struct mtk_imgrz_buf_info	src;
	struct mtk_imgrz_buf_info	dst;
	struct mtk_imgrz_partial_info	partial;

	bool	cbcr_swap;
	bool	jpeg_pic_mode;
	bool	y_exist;
	bool	cb_exist;
	bool	cr_exist;
	bool	y_only;
	bool	vm_scale_up[3];
	unsigned int h8_factor[3];
	unsigned int h8_offset[3];
	unsigned int hsa_factor[3];
	unsigned int hsa_offset[3];
	unsigned int v4_factor[3];
	unsigned int v4_offset[3];
	unsigned int vm_factor[3];
	unsigned int vm_offset[3];
};
#endif
