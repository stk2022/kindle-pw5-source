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

#ifndef MTK_IMGRZ_CORE_H
#define MTK_IMGRZ_CORE_H

#define MTK_IMGRZ_NAME    "mtk-imgrz"
#define SUPPORT_ALLOC_CTX	0

#define MTK_IMGRZ_FMT_FLAG_OUTPUT	BIT(0)
#define MTK_IMGRZ_FMT_FLAG_CAPTURE	BIT(1)

/**
 * struct mtk_imgrz_fmt - the driver's internal color format data
 * @pixelformat: the fourcc code for this format, 0 if not applicable
 * @num_planes: number of physically non-contiguous data planes
 * @num_comp: number of logical data planes
 * @depth: per plane driver's private 'number of bits per pixel'
 * @row_depth: per plane driver's private 'number of bits per pixel per row'
 * @flags: flags indicating which operation mode format applies to
	   MTK_IMGRZ_FMT_FLAG_OUTPUT is used in OUTPUT stream
	   MTK_IMGRZ_FMT_FLAG_CAPTURE is used in CAPTURE stream
 * @align: pointer to a pixel alignment struct, NULL if using default value
 */
struct mtk_imgrz_fmt {
	u32	pixelformat;
	u16	num_planes;
	u16	num_comp;
	u8	depth[VIDEO_MAX_PLANES];
	u8	row_depth[VIDEO_MAX_PLANES];
	u32	flags;
};

/**
 * mtk_imgrz_q_data - parameters of one queue
 * @w:		  image width
 * @h:		  image height
 * @bytesperline: distance in bytes between the leftmost pixels in two adjacent
 *                lines, need 16 bytes align
 * @sizeimage:	  image buffer size in bytes
 */
struct mtk_imgrz_q_data {
	u32	w;
	u32	h;
	u32	pixelformat;
	u8	num_planes;
	u32	bytesperline[3];
	u32	sizeimage[3];
};

struct mtk_imgrz_dev {
	struct mutex	lock;
	struct mutex	hw_lock;
	void __iomem	*base;
	struct clk	*clk;
	unsigned int	irq;
	struct device	*dev;
	struct device	*smi_larb_dev;
	struct v4l2_device	v4l2_dev;
	struct v4l2_m2m_dev	*m2m_dev;
	struct video_device	vdev;
	void    *alloc_ctx;

	atomic_t	krn_req_cnt;
	enum mtk_imgrz_state	state;
	wait_queue_head_t	wait_queue;
	struct workqueue_struct	*job_wq;
	struct mtk_imgrz_scale_param *cur_scale;

	struct timer_list	timer;      /* For decode timeout */
	unsigned long long	time_start, time_end;
};

/**
 * mtk_imgrz_ctx - the device context data
 * @imgrz:		imgrz IP device for this context
 * @out_q:		source (output) queue information
 * @cap_q:		destination (capture) queue information
 * @fh:			V4L2 file handle
 */
struct mtk_imgrz_ctx {
	struct mtk_imgrz_dev	*imgrz;
	struct mtk_imgrz_q_data	out_q;
	struct mtk_imgrz_q_data	cap_q;
	struct v4l2_fh		fh;
	struct work_struct	work;

	void	*temp_buf_va;
	size_t	temp_buf_sz;
	dma_addr_t	temp_buf;
};

struct mtk_imgrz_ext_ctx {
	struct mtk_imgrz_dev	*imgrz;

	void	*temp_buf_va;
	size_t	temp_buf_sz;
	dma_addr_t	temp_buf;
};
#endif
