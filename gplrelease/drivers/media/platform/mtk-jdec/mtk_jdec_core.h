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

#ifndef MTK_JDEC_CORE_H
#define MTK_JDEC_CORE_H

enum mtk_jdec_ctx_state {
	MTK_JDEC_INIT = 0,
	MTK_JDEC_RUNNING,
	MTK_JDEC_SOURCE_CHANGE,
};

enum mtk_jdec_state {
	MTK_JDEC_IDLE = 0,
	MTK_JDEC_BUSY,
	MTK_JDEC_ERROR,
	MTK_JDEC_DONE,
};

/**
 * struct mtk_jdec_dev - JDEC IP abstraction
 * @lock:		the mutex protecting this structure
 * @hw_lock:		spinlock protecting the hw device resource
 * @job_wq:		decode work queue
 * @dev:		JDEC device
 * @v4l2_dev:		v4l2 device for mem2mem mode
 * @m2m_dev:		v4l2 mem2mem device data
 * @alloc_ctx:		videobuf2 memory allocator's context
 * @vdev:		video device node for decoder mem2mem mode
 * @dec_reg_base:	JDEC registers mapping
 * @clk:		JDEC hw working clock
 * @larb:		SMI device
 */
struct mtk_jdec_dev {
	struct device		*dev;
	struct v4l2_device	v4l2_dev;
	struct v4l2_m2m_dev	*m2m_dev;
	void			*alloc_ctx;
	struct video_device	*vdev;
	void __iomem		*base;
	struct clk		*clk;
	struct device		*larb;
	struct device		*resizer;
	int			dec_irq;

	enum mtk_jdec_state	state;
	struct mutex		lock;
	spinlock_t		hw_lock;
	struct workqueue_struct	*job_wq;
	wait_queue_head_t	wait_queue;
	unsigned long long	time_start, time_end;

};

/**
 * struct mtk_jdec_fmt - driver's internal color format data
 * @fourcc:	 the fourcc code, 0 if not applicable
 * @h_sample:	 horizontal sample count of plane in 4 * 4 pixel image
 * @v_sample:	 vertical sample count of plane in 4 * 4 pixel image
 * @colplanes:  number of color planes (1 for packed formats)
 * @h_align:	 horizontal alignment order (align to 2^h_align)
 * @v_align:	 vertical alignment order (align to 2^v_align)
 * @type:	 flags describing format applicability
 */
struct mtk_jdec_fmt {
	u32	fourcc;
	int	h_sample[VIDEO_MAX_PLANES];
	int	v_sample[VIDEO_MAX_PLANES];
	int	colplanes;
	int	h_align;
	int	v_align;
	u32	type;
};

/**
 * mtk_jdec_q_data - parameters of one queue
 * @fmt:	driver-specific format of this queue
 * @w:		image width
 * @h:		image height
 * @bytesperline: distance in bytes between the leftmost pixels in two adjacent
 *		  lines
 * @sizeimage:	image buffer size in bytes
 */
struct mtk_jdec_q_data {
	struct mtk_jdec_fmt	*fmt;
	u32			w;
	u32			h;
	u32			bytesperline[VIDEO_MAX_PLANES];
	u32			sizeimage[VIDEO_MAX_PLANES];
};

/**
 * mtk_jdec_ctx - the device context data
 * @jdec:		JDEC IP device for this context
 * @out_q:		source (output) queue information
 * @cap_q:		destination (capture) queue queue information
 * @fh:			V4L2 file handle
 * @dec_param		parameters for HW decoding
 * @state:		state of the context
 * @colorspace:		enum v4l2_colorspace; supplemental to pixelformat
 * @ycbcr_enc:		enum v4l2_ycbcr_encoding, Y'CbCr encoding
 * @quantization:	enum v4l2_quantization, colorspace quantization
 * @xfer_func:		enum v4l2_xfer_func, colorspace transfer function
 */
struct mtk_jdec_ctx {
	struct mtk_jdec_dev		*jdec;
	struct mtk_jdec_q_data		out_q;
	struct mtk_jdec_q_data		cap_q;
	struct v4l2_fh			fh;
	struct work_struct		work;
	enum mtk_jdec_ctx_state		state;
	enum v4l2_colorspace		colorspace;
	enum v4l2_ycbcr_encoding	ycbcr_enc;
	enum v4l2_quantization		quantization;
	enum v4l2_xfer_func		xfer_func;
	void		*imgrz_ctx;
	void		*temp_c_buf_va;
	size_t		temp_c_buf_sz;
	dma_addr_t	temp_c_buf;
};

#endif /* _MTK_JDEC_CORE_H */

