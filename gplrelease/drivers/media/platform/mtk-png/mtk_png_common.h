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

#ifndef PNG_COMMON_H
#define PNG_COMMON_H

#include <linux/wait.h>
#include <linux/mutex.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fh.h>

#define PNG_FPGA_EP	0
#define MTK_PNG_IRQ_WORKARROUND 1
#define PNG_LZ77_BUF_SIZE (32*1024)

enum mtk_png_state {
	PNG_STATE_IDLE = 0,
	PNG_STATE_BUSY = 1,
	PNG_STATE_PARSE = 2,
	PNG_STATE_DEC = 3,
	PNG_STATE_DONE = 4,
	PNG_STATE_TIMEOUT = 5,
	PNG_STATE_STOP = 6,
};

enum mtk_png_irq_type {
	PNG_DEC_IRQ = 0,
	PNG_SRC_IRQ = 1,
	PNG_DST_IRQ = 2,
};

struct lz77_buf {
	void		*va;
	dma_addr_t	iova;
};

struct mtk_png_dev {
	struct mutex	lock;
	void __iomem	*base;
	struct clk	*clk;
	unsigned int	dec_irq;
	unsigned int	src_irq;
	unsigned int	dst_irq;
	struct device	*dev;
	struct device	*smi_larb_dev;
	struct v4l2_device	v4l2_dev;
	struct v4l2_m2m_dev	*m2m_dev;
	struct video_device	vdev;
	void    *alloc_ctx;

	struct lz77_buf		lz77;
	enum mtk_png_state	state;
	wait_queue_head_t	wait_hw_done_queue;
	struct timer_list	timer;      /* For decode timeout */
	unsigned long long	time_start, time_end;

};

#endif
