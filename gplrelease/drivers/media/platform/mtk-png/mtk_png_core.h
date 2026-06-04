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

#ifndef PNG_CORE_H
#define PNG_CORE_H

#include <media/videobuf2-v4l2.h>
#include "mtk_png_common.h"
#include "mtk_png_parse.h"

#define MTK_PNG_DECODER_NAME    "mtk-png"
#define SUPPORT_ALLOC_CTX	0

struct mtk_png_src_buf {
	void	*line_buf_va;
	dma_addr_t line_buf_iova;
	struct vb2_v4l2_buffer b;
	struct list_head list;
	struct mtk_png_dec_param dec_param;
};

/**
 * mtk_png_q_data - parameters of one queue
 * @fmt:	  driver-specific format of this queue
 * @w:		  image width
 * @h:		  image height
 * @bytesperline: distance in bytes between the leftmost pixels in two adjacent
 *                lines
 * @sizeimage:	  image buffer size in bytes
 */
struct mtk_png_q_data {
	u32	w;
	u32	h;
	u32	pixelformat;
	u8	num_planes;
	u32	bytesperline;
	u32	sizeimage;
};

/**
 * mtk_png_ctx - the device context data
 * @png:		png IP device for this context
 * @out_q:		source (output) queue information
 * @cap_q:		destination (capture) queue queue information
 * @fh:			V4L2 file handle
 */
struct mtk_png_ctx {
	struct mtk_png_dev	*png;
	struct mtk_png_q_data	out_q;
	struct mtk_png_q_data	cap_q;
	struct v4l2_fh		fh;

};

/**
 * struct png_fmt - driver's internal color format data
 * @fourcc:	the fourcc code, 0 if not applicable
 * @h_sample:	horizontal sample count of plane in 4 * 4 pixel image
 * @v_sample:	vertical sample count of plane in 4 * 4 pixel image
 * @colplanes:	number of color planes (1 for packed formats)
 * @h_align:	horizontal alignment order (align to 2^h_align)
 * @v_align:	vertical alignment order (align to 2^v_align)
 * @flags:	flags describing format applicability
 */
struct mtk_png_fmt {
	u32	fourcc;
	int	h_sample;
	int	v_sample;
	int	colplanes;
	int	h_align;
	int	v_align;
	u32	flags;
};

#endif
