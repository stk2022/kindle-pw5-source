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

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/dma-mapping.h>

#include <media/v4l2-ioctl.h>
#include <media/v4l2-mem2mem.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-dma-contig.h>
#include <soc/mediatek/smi.h>

#include "mtk_imgrz_common.h"
#include "mtk_imgrz_core.h"
#include "mtk_imgrz_ext.h"
#include "mtk_imgrz_hal.h"

#define MTK_IMGRZ_TIMEOUT	1000
static struct mtk_imgrz_dev *imgrz_dev;
static struct device *g_dev;
static int debug;
module_param(debug, int, 0644);

static const struct mtk_imgrz_fmt mtk_imgrz_formats[] = {
	{
		.pixelformat	= V4L2_PIX_FMT_ARGB32,
		.depth		= { 32 },
		.row_depth	= { 32 },
		.num_planes	= 1,
		.num_comp	= 1,
		.flags		= MTK_IMGRZ_FMT_FLAG_OUTPUT |
				  MTK_IMGRZ_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_AYUV,
		.depth		= { 32 },
		.row_depth	= { 32 },
		.num_planes	= 1,
		.num_comp	= 1,
		.flags		= MTK_IMGRZ_FMT_FLAG_OUTPUT |
				  MTK_IMGRZ_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV12M,
		.depth		= { 8, 4 },
		.row_depth	= { 8, 4 },
		.num_planes	= 2,
		.num_comp	= 2,
		.flags		= MTK_IMGRZ_FMT_FLAG_OUTPUT |
				  MTK_IMGRZ_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV21M,
		.depth		= { 8, 4 },
		.row_depth	= { 8, 4 },
		.num_planes	= 2,
		.num_comp	= 2,
		.flags		= MTK_IMGRZ_FMT_FLAG_OUTPUT |
				  MTK_IMGRZ_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV16M,
		.depth		= { 8, 8 },
		.row_depth	= { 8, 8 },
		.num_planes	= 2,
		.num_comp	= 2,
		.flags		= MTK_IMGRZ_FMT_FLAG_OUTPUT |
				  MTK_IMGRZ_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_YUV420M,
		.depth		= { 8, 2, 2 },
		.row_depth	= { 8, 2, 2 },
		.num_planes	= 3,
		.num_comp	= 3,
		.flags		= MTK_IMGRZ_FMT_FLAG_OUTPUT |
				  MTK_IMGRZ_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_YVU420M,
		.depth		= { 8, 2, 2 },
		.row_depth	= { 8, 2, 2 },
		.num_planes	= 3,
		.num_comp	= 3,
		.flags		= MTK_IMGRZ_FMT_FLAG_OUTPUT,
	}, {
		.pixelformat	= V4L2_PIX_FMT_YUV422M,
		.depth		= { 8, 4, 4 },
		.row_depth	= { 8, 4, 4 },
		.num_planes	= 3,
		.num_comp	= 3,
		.flags		= MTK_IMGRZ_FMT_FLAG_OUTPUT,
	}, {
		.pixelformat	= V4L2_PIX_FMT_YVU422M,
		.depth		= { 8, 4, 4 },
		.row_depth	= { 8, 4, 4 },
		.num_planes	= 3,
		.num_comp	= 3,
		.flags		= MTK_IMGRZ_FMT_FLAG_OUTPUT,
	}, {
		.pixelformat	= V4L2_PIX_FMT_YUV444M,
		.depth		= { 8, 8, 8 },
		.row_depth	= { 8, 8, 8 },
		.num_planes	= 3,
		.num_comp	= 3,
		.flags		= MTK_IMGRZ_FMT_FLAG_OUTPUT,
	}, {
		.pixelformat	= V4L2_PIX_FMT_YVU444M,
		.depth		= { 8, 8, 8 },
		.row_depth	= { 8, 8, 8 },
		.num_planes	= 3,
		.num_comp	= 3,
		.flags		= MTK_IMGRZ_FMT_FLAG_OUTPUT,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV12,
		.depth		= { 8, 4 },
		.row_depth	= { 8, 4 },
		.num_planes	= 2,
		.num_comp	= 2,
		.flags		= MTK_IMGRZ_FMT_FLAG_OUTPUT |
				  MTK_IMGRZ_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV21,
		.depth		= { 8, 4 },
		.row_depth	= { 8, 4 },
		.num_planes	= 2,
		.num_comp	= 2,
		.flags		= MTK_IMGRZ_FMT_FLAG_OUTPUT |
				  MTK_IMGRZ_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_NV16,
		.depth		= { 8, 8 },
		.row_depth	= { 8, 8 },
		.num_planes	= 2,
		.num_comp	= 2,
		.flags		= MTK_IMGRZ_FMT_FLAG_OUTPUT |
				  MTK_IMGRZ_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_YUV420,
		.depth		= { 8, 2, 2 },
		.row_depth	= { 8, 2, 2 },
		.num_planes	= 3,
		.num_comp	= 3,
		.flags		= MTK_IMGRZ_FMT_FLAG_OUTPUT,
	}, {
		.pixelformat	= V4L2_PIX_FMT_YVU420,
		.depth		= { 8, 2, 2 },
		.row_depth	= { 8, 2, 2 },
		.num_planes	= 3,
		.num_comp	= 3,
		.flags		= MTK_IMGRZ_FMT_FLAG_OUTPUT,
	}, {
		.pixelformat	= V4L2_PIX_FMT_RGB565,
		.depth		= { 16 },
		.row_depth	= { 16 },
		.num_planes	= 1,
		.num_comp	= 1,
		.flags		= MTK_IMGRZ_FMT_FLAG_OUTPUT |
				  MTK_IMGRZ_FMT_FLAG_CAPTURE,
	}, {
		.pixelformat	= V4L2_PIX_FMT_GREY,
		.depth		= { 8 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.flags		= MTK_IMGRZ_FMT_FLAG_OUTPUT |
				  MTK_IMGRZ_FMT_FLAG_CAPTURE,
	}
};

static inline struct mtk_imgrz_ctx *mtk_imgrz_fh_to_ctx(struct v4l2_fh *fh)
{
	return container_of(fh, struct mtk_imgrz_ctx, fh);
}

#if 0
static inline struct mtk_imgrz_src_buf *mtk_imgrz_vb2_to_srcbuf(
							struct vb2_buffer *vb)
{
	return container_of(to_vb2_v4l2_buffer(vb),
			struct mtk_imgrz_src_buf, b);
}
#endif

static bool mtk_imgrz_is_grey_compatible(struct mtk_imgrz_ctx *ctx)
{
	return ((ctx->out_q.pixelformat == ctx->cap_q.pixelformat) && (ctx->cap_q.pixelformat == V4L2_PIX_FMT_GREY));
}
static bool mtk_imgrz_is_partial_md(struct mtk_imgrz_ctx *ctx)
{
	return ((ctx->out_q.num_planes == 3) || (mtk_imgrz_is_grey_compatible(ctx)));
}

static struct mtk_imgrz_q_data *mtk_imgrz_get_q_data(struct mtk_imgrz_ctx *ctx,
						   enum v4l2_buf_type type)
{
	if (V4L2_TYPE_IS_OUTPUT(type))
		return &ctx->out_q;
	return &ctx->cap_q;
}

static int mtk_imgrz_querycap(struct file *file, void *priv,
			struct v4l2_capability *cap)
{
	struct mtk_imgrz_dev *imgrz = video_drvdata(file);

	strlcpy(cap->driver, MTK_IMGRZ_NAME, sizeof(cap->driver));
	strlcpy(cap->card, MTK_IMGRZ_NAME, sizeof(cap->card));
	snprintf(cap->bus_info, sizeof(cap->bus_info), "platform:%s",
		 dev_name(imgrz->dev));

	cap->device_caps = V4L2_CAP_STREAMING |
			   V4L2_CAP_VIDEO_M2M_MPLANE |
			   V4L2_CAP_VIDEO_CAPTURE_MPLANE |
			   V4L2_CAP_VIDEO_OUTPUT_MPLANE;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;
	return 0;
}

static int mtk_imgrz_g_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	struct mtk_imgrz_q_data *q_data = NULL;
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	struct mtk_imgrz_ctx *ctx = mtk_imgrz_fh_to_ctx(priv);
	struct mtk_imgrz_dev *imgrz = ctx->imgrz;
	struct vb2_queue *vq = v4l2_m2m_get_vq(ctx->fh.m2m_ctx, f->type);
	int i;

	if (!vq)
		return -EINVAL;

	q_data = mtk_imgrz_get_q_data(ctx, f->type);
	pix_mp->width = q_data->w;
	pix_mp->height = q_data->h;
	pix_mp->field = V4L2_FIELD_NONE;
	pix_mp->pixelformat = q_data->pixelformat;
	pix_mp->num_planes = q_data->num_planes;

	v4l2_dbg(1, debug, &imgrz->v4l2_dev, "(%d) g_fmt:%c%c%c%c w&h:%ux%u\n",
		 f->type, (pix_mp->pixelformat & 0xff),
		 (pix_mp->pixelformat >>  8 & 0xff),
		 (pix_mp->pixelformat >> 16 & 0xff),
		 (pix_mp->pixelformat >> 24 & 0xff),
		 pix_mp->width, pix_mp->height);

	for (i = 0; i < pix_mp->num_planes; i++) {
		struct v4l2_plane_pix_format *pfmt = &pix_mp->plane_fmt[i];

		pfmt->bytesperline = q_data->bytesperline[i];
		pfmt->sizeimage = q_data->sizeimage[i];
		memset(pfmt->reserved, 0, sizeof(pfmt->reserved));

		v4l2_dbg(1, debug, &imgrz->v4l2_dev,
			 "plane[%d] bpl=%u, size=%u\n",
			 i, pfmt->bytesperline, pfmt->sizeimage);
	}

	return 0;
}

static const struct mtk_imgrz_fmt *mtk_imgrz_find_fmt(u32 pixelformat, u32 type)
{
	u32 i, flag;

	flag = V4L2_TYPE_IS_OUTPUT(type) ? MTK_IMGRZ_FMT_FLAG_OUTPUT :
					   MTK_IMGRZ_FMT_FLAG_CAPTURE;

	for (i = 0; i < ARRAY_SIZE(mtk_imgrz_formats); ++i) {
		if (!(mtk_imgrz_formats[i].flags & flag))
			continue;
		if (mtk_imgrz_formats[i].pixelformat == pixelformat)
			return &mtk_imgrz_formats[i];
	}
	return NULL;
}

static int mtk_imgrz_s_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	struct mtk_imgrz_ctx *ctx = mtk_imgrz_fh_to_ctx(priv);
	struct mtk_imgrz_dev *imgrz  = ctx->imgrz;
	struct vb2_queue *vq = v4l2_m2m_get_vq(ctx->fh.m2m_ctx, f->type);
	struct mtk_imgrz_q_data *q_data = mtk_imgrz_get_q_data(ctx, f->type);
	int i;

	if (!vq)
		return -EINVAL;

	if (vb2_is_busy(vq)) {
		v4l2_err(&imgrz->v4l2_dev, "queue busy\n");
		return -EBUSY;
	}

	if (!mtk_imgrz_find_fmt(pix_mp->pixelformat, f->type)) {
		v4l2_err(&imgrz->v4l2_dev, "invalid format:%c%c%c%c\n",
			(q_data->pixelformat & 0xff),
			(q_data->pixelformat >>  8 & 0xff),
			(q_data->pixelformat >> 16 & 0xff),
			(q_data->pixelformat >> 24 & 0xff));
		return -EINVAL;
	}

	q_data->w = pix_mp->width;
	q_data->h = pix_mp->height;
	q_data->pixelformat = pix_mp->pixelformat;
	q_data->num_planes = pix_mp->num_planes;

	v4l2_dbg(1, debug, &imgrz->v4l2_dev,
		"(%d) s_fmt:%c%c%c%c, w&h:%ux%u\n",
		 f->type, (q_data->pixelformat & 0xff),
		 (q_data->pixelformat >>  8 & 0xff),
		 (q_data->pixelformat >> 16 & 0xff),
		 (q_data->pixelformat >> 24 & 0xff),
		 q_data->w, q_data->h);

	for (i = 0; i < q_data->num_planes; i++) {
		q_data->bytesperline[i] = pix_mp->plane_fmt[i].bytesperline;
		q_data->sizeimage[i] = pix_mp->plane_fmt[i].sizeimage;

		if (q_data->bytesperline[i] & 0xf) {
			v4l2_err(&imgrz->v4l2_dev,
				"type:%d plane:%d bytesperline:0x%x not align with 16 bytes\n",
				f->type, i, q_data->bytesperline[i]);
			return -EINVAL;
		}
		v4l2_dbg(1, debug, &imgrz->v4l2_dev,
			 "plane[%d] bpl=%u, size=%u\n",
			 i, q_data->bytesperline[i], q_data->sizeimage[i]);
	}

	return 0;
}

static const struct v4l2_ioctl_ops mtk_imgrz_ioctl_ops = {
	.vidioc_querycap		= mtk_imgrz_querycap,
	.vidioc_g_fmt_vid_cap_mplane	= mtk_imgrz_g_fmt,
	.vidioc_g_fmt_vid_out_mplane	= mtk_imgrz_g_fmt,
	.vidioc_s_fmt_vid_cap_mplane	= mtk_imgrz_s_fmt,
	.vidioc_s_fmt_vid_out_mplane	= mtk_imgrz_s_fmt,
	.vidioc_qbuf			= v4l2_m2m_ioctl_qbuf,
	.vidioc_create_bufs		= v4l2_m2m_ioctl_create_bufs,
	.vidioc_prepare_buf		= v4l2_m2m_ioctl_prepare_buf,
	.vidioc_reqbufs			= v4l2_m2m_ioctl_reqbufs,
	.vidioc_querybuf		= v4l2_m2m_ioctl_querybuf,
	.vidioc_dqbuf			= v4l2_m2m_ioctl_dqbuf,
	.vidioc_expbuf			= v4l2_m2m_ioctl_expbuf,
	.vidioc_streamon		= v4l2_m2m_ioctl_streamon,
	.vidioc_streamoff		= v4l2_m2m_ioctl_streamoff,
};


static int mtk_imgrz_queue_setup(struct vb2_queue *q,
			   unsigned int *num_buffers, unsigned int *num_planes,
			   unsigned int sizes[], struct device *alloc_ctxs[])
{
	struct mtk_imgrz_ctx *ctx = vb2_get_drv_priv(q);
	struct mtk_imgrz_q_data *q_data = NULL;
	struct mtk_imgrz_dev *imgrz = ctx->imgrz;
	int i;

	v4l2_dbg(1, debug, &imgrz->v4l2_dev, "(%d) buf_req count=%u\n",
		 q->type, *num_buffers);

	q_data = mtk_imgrz_get_q_data(ctx, q->type);
	if (!q_data)
		return -EINVAL;

	*num_planes = q_data->num_planes;
	for (i = 0; i < *num_planes; i++) {
		sizes[i] = q_data->sizeimage[i];
	#if SUPPORT_ALLOC_CTX
		alloc_ctxs[0] = imgrz->alloc_ctx;
	#endif
		v4l2_dbg(1, debug, &imgrz->v4l2_dev, "sizeimage[%d]=%u\n",
			 i, sizes[i]);
	}

	return 0;
}

static int mtk_imgrz_buf_prepare(struct vb2_buffer *vb)
{
	struct mtk_imgrz_q_data *q_data = NULL;
	struct mtk_imgrz_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	int i;

	if (V4L2_TYPE_IS_OUTPUT(vb->vb2_queue->type))
		return 0;

	q_data = mtk_imgrz_get_q_data(ctx, vb->vb2_queue->type);
	if (!q_data)
		return -EINVAL;

	for (i = 0; i < q_data->num_planes; i++)
		vb2_set_plane_payload(vb, i, q_data->sizeimage[i]);

	return 0;
}

static void mtk_imgrz_buf_queue(struct vb2_buffer *vb)
{
	struct mtk_imgrz_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);

	v4l2_dbg(3, debug, &ctx->imgrz->v4l2_dev,
		"%s type:%d\n", __func__, vb->type);
	v4l2_m2m_buf_queue(ctx->fh.m2m_ctx, to_vb2_v4l2_buffer(vb));
}

static void *mtk_imgrz_buf_remove(struct mtk_imgrz_ctx *ctx,
				 enum v4l2_buf_type type)
{
	v4l2_dbg(3, debug, &ctx->imgrz->v4l2_dev,
		"%s type:%d\n", __func__, type);

	if (V4L2_TYPE_IS_OUTPUT(type))
		return v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx);
	else
		return v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx);
}

static int mtk_imgrz_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct mtk_imgrz_ctx *ctx = vb2_get_drv_priv(q);
	struct mtk_imgrz_q_data *src_q;
	int ret = 0;

	v4l2_dbg(1, debug, &ctx->imgrz->v4l2_dev,
		"%s count:%d\n", __func__, count);

	if (V4L2_TYPE_IS_OUTPUT(q->type) && mtk_imgrz_is_partial_md(ctx)) {
		src_q = mtk_imgrz_get_q_data(ctx, q->type);
		ctx->temp_buf_sz = src_q->bytesperline[0] * 3 * 4;
		ctx->temp_buf_va = dma_alloc_coherent(ctx->imgrz->dev,
					ctx->temp_buf_sz,
					&ctx->temp_buf,
					GFP_KERNEL | __GFP_ZERO);
		if (!ctx->temp_buf_va)
			return -ENOMEM;
	}

	ret = pm_runtime_get_sync(ctx->imgrz->dev);
	if (ret < 0) {
		v4l2_err(&ctx->imgrz->v4l2_dev,
			"%s: pm_runtime_get_sync failed with %d\n",
			__func__, ret);
		return ret;
	}
	return 0;
}

static void mtk_imgrz_stop_streaming(struct vb2_queue *q)
{
	struct mtk_imgrz_ctx *ctx = vb2_get_drv_priv(q);
	struct vb2_buffer *vb;

	v4l2_dbg(1, debug, &ctx->imgrz->v4l2_dev, "%s\n", __func__);

	pm_runtime_put_sync(ctx->imgrz->dev);

	while ((vb = mtk_imgrz_buf_remove(ctx, q->type)))
		v4l2_m2m_buf_done(to_vb2_v4l2_buffer(vb), VB2_BUF_STATE_ERROR);

	if (V4L2_TYPE_IS_OUTPUT(q->type) && ctx->temp_buf) {
		dma_free_coherent(ctx->imgrz->dev, ctx->temp_buf_sz,
				ctx->temp_buf_va, ctx->temp_buf);
		ctx->temp_buf = 0;
		ctx->temp_buf_sz = 0;
		ctx->temp_buf_va = NULL;
	}
}

static struct vb2_ops mtk_imgrz_qops = {
	.queue_setup        = mtk_imgrz_queue_setup,
	.buf_prepare        = mtk_imgrz_buf_prepare,
	.buf_queue          = mtk_imgrz_buf_queue,
	.wait_prepare       = vb2_ops_wait_prepare,
	.wait_finish        = vb2_ops_wait_finish,
	.start_streaming    = mtk_imgrz_start_streaming,
	.stop_streaming     = mtk_imgrz_stop_streaming,
};

static bool mtk_imgrz_need_2scale(struct mtk_imgrz_ctx *ctx,
	struct vb2_buffer *src_buf, struct vb2_buffer *dst_buf)
{
	struct mtk_imgrz_q_data *src_q, *dst_q;

	src_q = mtk_imgrz_get_q_data(ctx, src_buf->type);
	dst_q = mtk_imgrz_get_q_data(ctx, dst_buf->type);

	/* multi-planes YUV to RGB need scale two times
	 * multi-planes YUV->AYUV(1 plane)->RGB(1 plane)
	 */
	return (src_q->num_planes > 1 &&
		dst_q->num_planes == 1 &&
		dst_q->pixelformat != V4L2_PIX_FMT_AYUV &&
		dst_q->pixelformat != V4L2_PIX_FMT_GREY);
}

/*
 * Return: linebuf in the normal case.
 *         ch1 line-buf-len while onephase case.
 */
static int mtk_imgrz_calc_linebuflen(struct mtk_imgrz_dev *imgrz,
	bool ufo, bool onephase, bool osdmode, bool dst_blk,
	uint32_t src_width, uint32_t dst_width,
	unsigned int linebuflen)
{
	unsigned int cnt;

	if (ufo) {
		if ((src_width <= dst_width || src_width <= 992) &&
		    (!onephase))
			/* scale up */
			/* lineBuf v7 formula: ufo2osd one phase case,
			 * not care about whether width <= 1024
			 * 992 is for clip.
			 */
			linebuflen = 16;
		else
			/* scale down while srcwid > 1024. */
			/* osd one phase just use this
			 * no matter scale up or down
			 */
			linebuflen = (1024 - 32) * dst_width / (src_width * 32);

		/* osd one phase or Dst blk need even.*/
		if ((onephase && osdmode) || dst_blk)
			linebuflen &= 0xfffffffe;
		if (linebuflen > 16)
			linebuflen = 16;
	}

	if ((!ufo) && osdmode && onephase) {/* normal vdo2argb, lineB v8*/
		/* phoenix es2 verify 420vdo 520x480 onephase
		 * to argb8888 777x740 shows garbage in the right
		 */
		if (src_width < dst_width) {
			while ((dst_width % (linebuflen * 32) <= 16) &&
				(linebuflen > 1))
				linebuflen--;
		}
	}

	if (linebuflen < 1)
		linebuflen = 1;

	cnt = dst_width / (linebuflen * 32);
	if ((dst_width - cnt * linebuflen * 32 < 16) &&
	    (dst_width - cnt * linebuflen * 32 > 0) && linebuflen > 1)
		v4l2_dbg(1, debug, &imgrz->v4l2_dev,
			"%s- The last part wid too small:%u. SrcW:%u, DstW:%u, Count:%u, curline:%u\n",
			__func__, dst_width - cnt * linebuflen*32,
			src_width, dst_width, cnt, linebuflen);

	/* normal vdo2argb not div 2,phoenix es2. normal vdo2argb, lineB v8 */
	if (onephase && osdmode && ufo)
		linebuflen /= 2;

	v4l2_dbg(4, debug, &imgrz->v4l2_dev,
		"%s- srcW:%u dstW:%u lineB:%u Hw partial NUM:%d last part:%u, ufo:%d one phase %d(%d)\n",
		__func__, src_width, dst_width, linebuflen, cnt,
		dst_width - cnt * linebuflen * 32, ufo, onephase, osdmode);

	return linebuflen;
}

int mtk_imgrz_s_linebuflen(struct mtk_imgrz_dev	*imgrz,
		struct mtk_imgrz_scale_param *scale_param)
{
	struct mtk_imgrz_buf_info *src = &scale_param->src;
	struct mtk_imgrz_buf_info *dst = &scale_param->dst;
	unsigned int linebuflen = 0x10;
	bool ufo = false;
	bool one_phase = false;
	bool use_extend16 = false;
	bool osd_mode = dst->fmt == MTK_IMGRZ_COLOR_FORMAT_ARGB ||
			dst->fmt == MTK_IMGRZ_COLOR_FORMAT_AYUV;


	switch (scale_param->v_method) {
	case MTK_IMGRZ_M_TAP_RESAMPLE:
		if (src->pic_height < dst->pic_height) {
			linebuflen = 0x1F;
			use_extend16 = 0;
		} else {
			linebuflen = 0x10;
			use_extend16 = 1;
		}

		/* Adjust tmp linebuflen for HW */
		if (osd_mode) {
			while (((dst->pic_width * 4) % (linebuflen << 5))
				<= 8) {
				linebuflen--;
				if (linebuflen == 0)
					return -EINVAL;
			}
		} else {
			while ((dst->pic_width % (linebuflen << 5)) <= 8) {
				linebuflen--;
				if (linebuflen == 0)
					return -EINVAL;
			}
		}
		break;
	case MTK_IMGRZ_4_TAP_RESAMPLE:
		use_extend16 = 0;
		break;
	default:
		return -EINVAL;
	}

	if (dst->bit10)	/* 10bit can not extend 16. */
		use_extend16 = false;
	else if (ufo || one_phase)
		use_extend16 = false;	/* c-model always disable ext16. */
	mtk_imgrz_hal_s_linebuflen_ext16(imgrz->base, use_extend16);

	/* osd & one phase don't call line buffer here */
	if (!(one_phase && osd_mode))
		linebuflen = mtk_imgrz_calc_linebuflen(imgrz, ufo, one_phase,
				osd_mode, dst->block, src->pic_width,
				dst->pic_width, linebuflen);
	if (ufo)
		mtk_imgrz_hal_ufo_linebuf_eco(imgrz->base);

	mtk_imgrz_hal_s_sram_linebuflen(imgrz->base, linebuflen);

	return 0;
}

/* The color format other than ARGB/INDEX need to be implement */
static int mtk_imgrz_calc_factor(struct mtk_imgrz_scale_param *scale_param)
{
	unsigned int src_width[3] = {0}, src_height[3] = {0};
	unsigned int dst_width[3] = {0}, dst_height[3] = {0};
	unsigned int factor, offset;
	bool h_8tap, v_4tap;
	int ret = 0, i;

	src_width[0] = scale_param->src.pic_width;
	src_height[0] = scale_param->src.pic_height;
	switch (scale_param->src.fmt) {
	case MTK_IMGRZ_COLOR_FORMAT_ARGB:
	case MTK_IMGRZ_COLOR_FORMAT_INDEX:
	case MTK_IMGRZ_COLOR_FORMAT_AYUV:
	case MTK_IMGRZ_COLOR_FORMAT_GREY:
		break;
	case MTK_IMGRZ_COLOR_FORMAT_Y_C:
		switch (scale_param->src.f.yc_fmt) {
		case MTK_IMGRZ_YC_FORMAT_420:
			src_width[2] = src_width[1] = src_width[0] >> 1;
			src_height[2] = src_height[1] = src_height[0] >> 1;
			break;
		case MTK_IMGRZ_YC_FORMAT_422:
			src_width[2] = src_width[1] = src_width[0] >> 1;
			src_height[2] = src_height[1] = src_height[0];
			break;
		case MTK_IMGRZ_YC_FORMAT_444:
			src_width[2] = src_width[1] = src_width[0];
			src_height[2] = src_height[1] = src_height[0];
			break;
		}
		break;
	case MTK_IMGRZ_COLOR_FORMAT_Y_CB_CR:
		src_width[1] = src_width[0] * scale_param->src.h_sample[1] /
				scale_param->src.h_sample[0];
		src_width[2] = src_width[0] * scale_param->src.h_sample[2] /
				scale_param->src.h_sample[0];
		src_height[1] = src_height[0] * scale_param->src.v_sample[1] /
				scale_param->src.v_sample[0];
		src_height[2] = src_height[0] * scale_param->src.v_sample[2] /
				scale_param->src.v_sample[0];
		/* For jpeg picture mode, prevent source height 401
		 * come two interrupt (Y interrupt and C interrupt)
		 */
		if (src_height[1] * scale_param->src.v_sample[0] !=
		    src_height[0] * scale_param->src.v_sample[1])
			src_height[1]++;
		if (src_height[2] * scale_param->src.v_sample[0] !=
		    src_height[0] * scale_param->src.v_sample[2])
			src_height[2]++;
		break;
	default:
		break;
	}

	dst_width[0] = scale_param->dst.pic_width;
	dst_height[0] = scale_param->dst.pic_height;
	switch (scale_param->dst.fmt) {
	case MTK_IMGRZ_COLOR_FORMAT_ARGB:
	case MTK_IMGRZ_COLOR_FORMAT_INDEX:
	case MTK_IMGRZ_COLOR_FORMAT_AYUV:
		dst_width[1] = scale_param->dst.pic_width;
		dst_height[1] = scale_param->dst.pic_height;
		dst_width[2] = scale_param->dst.pic_width;
		dst_height[2] = scale_param->dst.pic_height;
		break;
	case MTK_IMGRZ_COLOR_FORMAT_Y_C:
		switch (scale_param->dst.f.yc_fmt) {
		case MTK_IMGRZ_YC_FORMAT_420:
			dst_width[2] = dst_width[1] = dst_width[0] >> 1;
			dst_height[2] = dst_height[1] = dst_height[0] >> 1;
			break;
		case MTK_IMGRZ_YC_FORMAT_422:
			dst_width[2] = dst_width[1] = dst_width[0] >> 1;
			dst_height[2] = dst_height[1] = dst_height[0];
			break;
		case MTK_IMGRZ_YC_FORMAT_444:
			dst_width[2] = dst_width[1] = dst_width[0];
			dst_height[2] = dst_height[1] = dst_height[0];
			break;
		}
		break;
	default:
		break;
	}

	h_8tap = ((scale_param->h_method == MTK_IMGRZ_8_TAP_RESAMPLE) ||
		(src_width[0] <= dst_width[0]));
	v_4tap = (scale_param->v_method == MTK_IMGRZ_4_TAP_RESAMPLE);

	for (i = 0; i < 3; i++) {
		/* set default value */
		scale_param->h8_factor[i] = 0x40000;
		scale_param->hsa_factor[i] = 0x800;
		scale_param->v4_factor[i] = 0x40000;
		scale_param->vm_factor[i] = 0x800;

		if (h_8tap) {
			if (src_width[i] == dst_width[i] ||
			    (i > 1 && dst_width[i] == 1))
				factor = (0x40000 * src_width[i] +
					(dst_width[i] >> 1)) / dst_width[i];
			else
				factor = (0x40000 * (src_width[i] - 1) +
					 ((dst_width[i] - 1) >> 1)) /
					 (dst_width[i] - 1);
			scale_param->h8_factor[i] = factor;
		} else {
			factor = (2048 * dst_width[i] + (src_width[i] >> 1)) /
				src_width[i];
			scale_param->hsa_factor[i] = factor;
			scale_param->hsa_offset[i] = 2048 - factor;
		}

		if (v_4tap) {
			scale_param->v4_factor[i] = (0x40000 * src_height[0] +
				(dst_height[0] >> 1)) / dst_height[0];
		} else {
			if (dst_height[i] < src_height[i])  {/* scale down */
				factor = (2048 * dst_height[i] +
					(src_height[i] >> 1)) /	src_height[i];
				offset = 2048 - factor;
				scale_param->vm_scale_up[i] = false;
			} else {/* scale up */
				if (dst_height[i] == src_height[i]) {
					factor = 0;
					offset = 0;
				} else {
					factor = (2048 * (src_height[i] - 1) +
						((dst_height[i] - 1) >> 1)) /
						(dst_height[i] - 1);
					offset = 0;
				}
				scale_param->vm_scale_up[i] = true;
			}
			scale_param->vm_factor[i] = factor;
			scale_param->vm_offset[i] = offset;
		}

		if (i == 0 &&
		   (scale_param->src.fmt == MTK_IMGRZ_COLOR_FORMAT_ARGB ||
		    scale_param->src.fmt == MTK_IMGRZ_COLOR_FORMAT_AYUV ||
		    scale_param->src.fmt == MTK_IMGRZ_COLOR_FORMAT_INDEX||
		    scale_param->src.fmt == MTK_IMGRZ_COLOR_FORMAT_GREY))
			break;
		if (i == 1 &&
		    scale_param->src.fmt == MTK_IMGRZ_COLOR_FORMAT_Y_C)
			break;
	}
	return ret;
}

int mtk_imgrz_timeout(struct mtk_imgrz_dev *imgrz)
{
	dev_err(imgrz->dev, "imgrz timeout\n");
	mtk_imgrz_hal_print_reg(imgrz->base);
	return 0;
}

static int mtk_imgrz_fill_fmt(struct mtk_imgrz_ctx *ctx,
		u32 pixelformat, struct mtk_imgrz_buf_info *buf_info)
{
	switch (pixelformat) {
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_YVU420M:
		buf_info->fmt = MTK_IMGRZ_COLOR_FORMAT_Y_CB_CR;
		buf_info->h_sample[0] = 2;
		buf_info->v_sample[0] = 2;
		buf_info->h_sample[1] = 1;
		buf_info->v_sample[1] = 1;
		buf_info->h_sample[2] = 1;
		buf_info->v_sample[2] = 1;
		break;
	case V4L2_PIX_FMT_YUV422M:
	case V4L2_PIX_FMT_YVU422M:
		buf_info->fmt = MTK_IMGRZ_COLOR_FORMAT_Y_CB_CR;
		buf_info->h_sample[0] = 2;
		buf_info->v_sample[0] = 1;
		buf_info->h_sample[1] = 1;
		buf_info->v_sample[1] = 1;
		buf_info->h_sample[2] = 1;
		buf_info->v_sample[2] = 1;
		break;
	case V4L2_PIX_FMT_YUV444M:
	case V4L2_PIX_FMT_YVU444M:
		buf_info->fmt = MTK_IMGRZ_COLOR_FORMAT_Y_CB_CR;
		buf_info->h_sample[0] = 1;
		buf_info->v_sample[0] = 1;
		buf_info->h_sample[1] = 1;
		buf_info->v_sample[1] = 1;
		buf_info->h_sample[2] = 1;
		buf_info->v_sample[2] = 1;
		break;
	case V4L2_PIX_FMT_ARGB32:
		buf_info->f.argb_fmt = MTK_IMGRZ_ARGB_FORMAT_8888;
		buf_info->fmt = MTK_IMGRZ_COLOR_FORMAT_ARGB;
		break;
	case V4L2_PIX_FMT_ARGB555:
		buf_info->f.argb_fmt = MTK_IMGRZ_ARGB_FORMAT_1555;
		buf_info->fmt = MTK_IMGRZ_COLOR_FORMAT_ARGB;
		break;
	case V4L2_PIX_FMT_ARGB444:
		buf_info->f.argb_fmt = MTK_IMGRZ_ARGB_FORMAT_4444;
		buf_info->fmt = MTK_IMGRZ_COLOR_FORMAT_ARGB;
		break;
	case V4L2_PIX_FMT_RGB565:
		buf_info->f.argb_fmt = MTK_IMGRZ_ARGB_FORMAT_0565;
		buf_info->fmt = MTK_IMGRZ_COLOR_FORMAT_ARGB;
		break;
		break;
	case V4L2_PIX_FMT_AYUV:
		buf_info->f.argb_fmt = MTK_IMGRZ_ARGB_FORMAT_8888;
		buf_info->fmt = MTK_IMGRZ_COLOR_FORMAT_AYUV;
		break;
	case V4L2_PIX_FMT_PAL8:
		buf_info->fmt = MTK_IMGRZ_COLOR_FORMAT_INDEX;
		break;
	case V4L2_PIX_FMT_NV12:
		buf_info->fmt = MTK_IMGRZ_COLOR_FORMAT_Y_C;
		buf_info->f.yc_fmt = MTK_IMGRZ_YC_FORMAT_420;
		break;
	case V4L2_PIX_FMT_GREY:
		buf_info->fmt = MTK_IMGRZ_COLOR_FORMAT_GREY;
		buf_info->h_sample[0] = 2;
		buf_info->v_sample[0] = 2;
		break;
	default:
		v4l2_err(&ctx->imgrz->v4l2_dev,
			"Unsupport dst color format: %c%c%c%c\n",
			(pixelformat & 0xff),
			(pixelformat >>  8 & 0xff),
			(pixelformat >> 16 & 0xff),
			(pixelformat >> 24 & 0xff));
		return -EINVAL;
	}
	return 0;
}

static int mtk_imgrz_fill_src_info(struct mtk_imgrz_ctx *ctx,
	struct vb2_buffer *src_buf, struct vb2_buffer *dst_buf,
	struct mtk_imgrz_scale_param *scale_param, bool scale_2nd)
{
	int i, ret;
	struct mtk_imgrz_partial_info *partial = &scale_param->partial;
	struct mtk_imgrz_q_data *src_q =
		mtk_imgrz_get_q_data(ctx, src_buf->type);
	u32 pixelformat = src_q->pixelformat;

	/* 2nd scale src info is same with dst buf info
	 * but only pixelformat is different
	 */
	if (mtk_imgrz_need_2scale(ctx, src_buf, dst_buf) && scale_2nd) {
		src_q =	mtk_imgrz_get_q_data(ctx, dst_buf->type);
		src_buf = dst_buf;
		pixelformat = V4L2_PIX_FMT_AYUV;
	}

	for (i = 0; i < src_q->num_planes; i++) {
		scale_param->src.buf_width[i] = src_q->bytesperline[i];
		scale_param->src.dma_addr[i] =
			*((dma_addr_t *)vb2_plane_cookie(src_buf, i));
	}
	scale_param->src.pic_width = src_q->w;
	scale_param->src.pic_height = src_q->h;

	ret = mtk_imgrz_fill_fmt(ctx, pixelformat, &scale_param->src);
	if (ret < 0)
		return ret;

	scale_param->h_method = MTK_IMGRZ_8_TAP_RESAMPLE;
	if (mtk_imgrz_is_partial_md(ctx)) {
		scale_param->scale_mode = MTK_IMGRZ_PARTIAL_SCALE;
		scale_param->v_method = MTK_IMGRZ_M_TAP_RESAMPLE;

	} else {
		scale_param->scale_mode = MTK_IMGRZ_FRAME_SCALE;
		scale_param->v_method = MTK_IMGRZ_4_TAP_RESAMPLE;
	}

	if (scale_param->scale_mode == MTK_IMGRZ_PARTIAL_SCALE) {
		partial->first_row = true;
		if (scale_param->src.pic_height < ROW_BUF_HEIGHT) {
			partial->rowbuf_hei = scale_param->src.pic_height;
			partial->last_row = true;
		} else
			partial->rowbuf_hei = ROW_BUF_HEIGHT;

		for (i = 0; i < src_q->num_planes; i++)
			partial->cur_row_addr[i] = scale_param->src.dma_addr[i];
		partial->temp_buf = ctx->temp_buf;
	} else {
		partial->first_row = true;
		partial->last_row = true;
	}

	return 0;
}

static int mtk_imgrz_fill_dst_info(struct mtk_imgrz_ctx *ctx,
	struct vb2_buffer *src_buf, struct vb2_buffer *dst_buf,
	struct mtk_imgrz_scale_param *scale_param, bool scale_2nd)
{
	int i;
	struct mtk_imgrz_q_data *dst_q =
		mtk_imgrz_get_q_data(ctx, dst_buf->type);
	u32 pixelformat = dst_q->pixelformat;

	if (mtk_imgrz_need_2scale(ctx, src_buf, dst_buf) && !scale_2nd)
		pixelformat = V4L2_PIX_FMT_AYUV;
	for (i = 0; i < dst_q->num_planes; i++) {
		scale_param->dst.buf_width[i] = dst_q->bytesperline[i];
		scale_param->dst.dma_addr[i] =
			*((dma_addr_t *)vb2_plane_cookie(dst_buf, i));
	}
	scale_param->dst.pic_width = dst_q->w;
	scale_param->dst.pic_height = dst_q->h;

	return mtk_imgrz_fill_fmt(ctx, pixelformat, &scale_param->dst);
}

static int mtk_imgrz_prepare_param(struct mtk_imgrz_ctx *ctx,
	struct vb2_buffer *src_buf, struct vb2_buffer *dst_buf,
	struct mtk_imgrz_scale_param *scale_param, bool scale_2nd)
{
	scale_param->y_exist = true;
	scale_param->cb_exist = true;
	scale_param->cr_exist = true;
	mtk_imgrz_fill_src_info(ctx, src_buf, dst_buf, scale_param, scale_2nd);
	mtk_imgrz_fill_dst_info(ctx, src_buf, dst_buf, scale_param, scale_2nd);
	mtk_imgrz_calc_factor(scale_param);

	v4l2_dbg(3, debug, &ctx->imgrz->v4l2_dev,
		"%s src fmt:%d[%u,%u], pitch:[%u,%u,%u], pad:[0x%pad,0x%pad,0x%pad]\n",
		__func__, scale_param->src.fmt,
		scale_param->src.pic_width, scale_param->src.pic_height,
		scale_param->src.buf_width[0], scale_param->src.buf_width[1],
		scale_param->src.buf_width[2], &scale_param->src.dma_addr[0],
		&scale_param->src.dma_addr[1], &scale_param->src.dma_addr[2]);
	v4l2_dbg(3, debug, &ctx->imgrz->v4l2_dev,
		"%s dst fmt:%d[%u,%u], pitch:[%u,%u], pad:[0x%pad,0x%pad]\n",
		__func__, scale_param->dst.fmt,
		scale_param->dst.pic_width, scale_param->dst.pic_height,
		scale_param->dst.buf_width[0], scale_param->dst.buf_width[1],
		&scale_param->dst.dma_addr[0], &scale_param->dst.dma_addr[1]);

	return 0;
}

static void mtk_imgrz_hw_do_scale(struct mtk_imgrz_dev *imgrz,
	struct mtk_imgrz_scale_param *scale_param)
{
	struct mtk_imgrz_buf_info *src = &scale_param->src;
	struct mtk_imgrz_buf_info *dst = &scale_param->dst;
	struct mtk_imgrz_partial_info *partial = &scale_param->partial;
	void __iomem *base = imgrz->base;

	/* not 1st row of partial mode, only update partial configiration */
	if (scale_param->scale_mode == MTK_IMGRZ_PARTIAL_SCALE &&
	    !scale_param->partial.first_row) {
		mtk_imgrz_hal_s_src_buf_addr(base, partial->cur_row_addr);
		mtk_imgrz_hal_s_pre_row_addr(base, partial->pre_row_addr);
		mtk_imgrz_hal_s_src_first_row(base, partial->first_row);
		mtk_imgrz_hal_s_src_last_row(base, partial->last_row);
		mtk_imgrz_hal_s_rowbuf_height(base,
				scale_param->partial.rowbuf_hei, src);
		goto trigger;
	}

	/* mtk_imgrz_hw_set_rm_rpr(base, false, false); */
	mtk_imgrz_hal_s_resz_mode(base, scale_param->scale_mode, src->fmt);
	mtk_imgrz_hal_s_resample_method(base,
		scale_param->h_method, scale_param->v_method);

	/* Set Source Information to HW */
	mtk_imgrz_hal_s_src_buf_fmt(base, src);
	mtk_imgrz_hal_s_src_buf_addr(base, src->dma_addr);
	mtk_imgrz_hal_s_src_first_row(base, partial->first_row);
	mtk_imgrz_hal_s_src_last_row(base, partial->last_row);

	if (scale_param->scale_mode == MTK_IMGRZ_PARTIAL_SCALE) {
		/* PARTIAL MODE */
		mtk_imgrz_hal_s_tempbuf_addr(base, partial->temp_buf);
		if (scale_param->jpeg_pic_mode)
			mtk_imgrz_hal_s_pre_row_addr(base,
					scale_param->partial.pre_row_addr);
		else
			mtk_imgrz_hal_s_rowbuf_height(base,
					scale_param->partial.rowbuf_hei, src);
	}

	mtk_imgrz_hal_s_src_buf_pitch(base, src->fmt, src->buf_width);
	mtk_imgrz_hal_s_src_pic_w_h(base, src->pic_width, src->pic_height, src);
	mtk_imgrz_hal_s_src_pic_ofs(base, src->x_offset, src->y_offset, src);

	/* 4. Set Destination Information to HW */
	mtk_imgrz_hal_s_dst_buf_fmt(base, src, dst);
	mtk_imgrz_hal_s_dst_buf_addr(base, dst->dma_addr);
	mtk_imgrz_hal_s_dst_buf_pitch(base, dst->fmt, dst->buf_width);
	mtk_imgrz_hal_s_dst_pic_w_h(base, dst->pic_width, dst->pic_height);
	mtk_imgrz_hal_s_dst_pic_ofs(base, dst->x_offset, dst->y_offset);
	mtk_imgrz_hal_s_yc_cbcr_swap(base, scale_param->cbcr_swap);

	/* 6. Set Scale Factor to HW */
	mtk_imgrz_hal_coeff_s_h_factor(base, scale_param);
	mtk_imgrz_hal_coeff_s_v_factor(base, scale_param);

	mtk_imgrz_s_linebuflen(imgrz, scale_param);

	if (src->fmt == MTK_IMGRZ_COLOR_FORMAT_Y_CB_CR || src->fmt == MTK_IMGRZ_COLOR_FORMAT_GREY) {
		mtk_imgrz_hal_jpg_component(base, scale_param->y_exist,
			scale_param->cb_exist, scale_param->cr_exist);
		mtk_imgrz_hal_s_jpeg_pic_mode(base, scale_param->jpeg_pic_mode);
		mtk_imgrz_hal_s_cbcr_pad(base,
			scale_param->cb_exist, scale_param->cr_exist);
	}
	mtk_imgrz_hal_ayuv_y_only(base, scale_param->y_only);

	mtk_imgrz_hal_burst_enable(base, !dst->block, !src->block);
	mtk_imgrz_hal_s_alpha_scale_type(base, 0);

trigger:
	mtk_imgrz_hal_trigger_hw(base);
}

static int __maybe_unused mtk_imgrz_pm_resume(struct device *dev)
{
	struct mtk_imgrz_dev *imgrz = dev_get_drvdata(dev);
	int ret = 0;

	ret = mtk_smi_larb_get(imgrz->smi_larb_dev);
	if (ret < 0) {
		v4l2_err(&imgrz->v4l2_dev,
			"%s: mtk_smi_larb_get failed with %d\n",
			__func__, ret);
		return ret;
	}

	ret = clk_prepare_enable(imgrz->clk);
	if (ret < 0) {
		v4l2_err(&imgrz->v4l2_dev,
			"%s: clk_prepare_enable failed with %d\n",
			__func__, ret);
	}

	return 0;
}

static int __maybe_unused mtk_imgrz_resume(struct device *dev)
{
	if (pm_runtime_suspended(dev))
		return 0;

	return mtk_imgrz_pm_resume(dev);
}
static int __mtk_imgrz_scale(struct mtk_imgrz_scale_param *scale_param)
{
	struct mtk_imgrz_dev *imgrz = dev_get_drvdata(g_dev);

	mtk_imgrz_calc_factor(scale_param);

	pm_runtime_get_sync(g_dev);
	atomic_inc(&imgrz->krn_req_cnt);
	wait_event(imgrz->wait_queue, imgrz->state == IMGRZ_STATE_IDLE);
	mutex_lock(&imgrz->hw_lock);
	imgrz->state = IMGRZ_STATE_BUSY;
	imgrz->cur_scale = scale_param;
	imgrz->time_start = sched_clock();
	mtk_imgrz_hal_hw_init(imgrz->base);
	mtk_imgrz_hw_do_scale(imgrz, scale_param);
	if (!wait_event_timeout(imgrz->wait_queue, imgrz->state == IMGRZ_STATE_DONE,
		msecs_to_jiffies(MTK_IMGRZ_TIMEOUT)))
		mtk_imgrz_timeout(imgrz);
	mtk_imgrz_hal_hw_uninit(imgrz->base);
	pm_runtime_put_sync(g_dev);

	imgrz->state = IMGRZ_STATE_IDLE;
	mutex_unlock(&imgrz->hw_lock);

	atomic_dec(&imgrz->krn_req_cnt);
	wake_up(&imgrz->wait_queue);

	return 0;
}

int mtk_imgrz_scale(struct mtk_imgrz_scale_kapi_param *scale_kapi_param)
{
	struct mtk_imgrz_scale_param scale_param;
	size_t	temp_buf_sz;
	void	*temp_buf_va;

	if (scale_kapi_param->src_info.buf_w % 16 ||
		scale_kapi_param->dst_info.buf_w % 16) {
		pr_err("src_buf_w or dst_buf_w not 16 align\n");
		return -EINVAL;
	}
	if (scale_kapi_param->src_info.dma_buf % 16 ||
		scale_kapi_param->dst_info.dma_buf % 16) {
		pr_err("src_buf or dst_buf addr not 16 align\n");
		return -EINVAL;
	}
	memset(&scale_param, 0, sizeof(scale_param));
	scale_param.src.pic_width = scale_kapi_param->src_info.pic_w;
	scale_param.src.pic_height = scale_kapi_param->src_info.pic_h;
	scale_param.src.buf_width[0] = scale_kapi_param->src_info.buf_w;
	scale_param.src.buf_height[0] = scale_kapi_param->src_info.buf_h;
	scale_param.src.x_offset = scale_kapi_param->src_info.x_offset;
	scale_param.src.y_offset= scale_kapi_param->src_info.y_offset;
	scale_param.src.dma_addr[0] = scale_kapi_param->src_info.dma_buf;
	scale_param.src.fmt = MTK_IMGRZ_COLOR_FORMAT_GREY;
	scale_param.src.h_sample[0] = 2;
	scale_param.src.v_sample[0] = 2;
	scale_param.scale_mode = MTK_IMGRZ_PARTIAL_SCALE;
	scale_param.h_method = MTK_IMGRZ_8_TAP_RESAMPLE;
	scale_param.v_method = MTK_IMGRZ_M_TAP_RESAMPLE;
	scale_param.partial.first_row = true;
	if (scale_param.src.pic_height < ROW_BUF_HEIGHT) {
		scale_param.partial.rowbuf_hei = scale_param.src.pic_height;
		scale_param.partial.last_row = true;
	} else
		scale_param.partial.rowbuf_hei = ROW_BUF_HEIGHT;

	scale_param.partial.cur_row_addr[0] = scale_param.src.dma_addr[0];
	temp_buf_sz = scale_param.src.buf_width[0] * 3 * 4;
	temp_buf_va = dma_alloc_coherent(g_dev, temp_buf_sz,
		&scale_param.partial.temp_buf, GFP_KERNEL | __GFP_ZERO);
	if (!temp_buf_va)
		return -ENOMEM;

	scale_param.dst.pic_width = scale_kapi_param->dst_info.pic_w;
	scale_param.dst.pic_height = scale_kapi_param->dst_info.pic_h;
	scale_param.dst.buf_width[0] = scale_kapi_param->dst_info.buf_w;
	scale_param.dst.buf_height[0] = scale_kapi_param->dst_info.buf_h;
	scale_param.dst.x_offset = scale_kapi_param->dst_info.x_offset;
	scale_param.dst.y_offset= scale_kapi_param->dst_info.y_offset;
	scale_param.dst.dma_addr[0] = scale_kapi_param->dst_info.dma_buf;
	scale_param.dst.fmt = MTK_IMGRZ_COLOR_FORMAT_GREY;
	scale_param.dst.h_sample[0] = 2;
	scale_param.dst.v_sample[0] = 2;
	scale_param.y_exist = true;
	scale_param.cb_exist = false;
	scale_param.cr_exist = false;
	scale_param.y_only = true;

	__mtk_imgrz_scale(&scale_param);
	dma_free_coherent(g_dev, temp_buf_sz, temp_buf_va,
		scale_param.partial.temp_buf);

	return 0;
}

EXPORT_SYMBOL(mtk_imgrz_scale);

static void mtk_imgrz_worker(struct work_struct *work)
{
	struct mtk_imgrz_ctx *ctx =
		container_of(work, struct mtk_imgrz_ctx, work);
	struct mtk_imgrz_dev *imgrz = ctx->imgrz;
	enum vb2_buffer_state buf_state = VB2_BUF_STATE_ERROR;
	struct vb2_buffer *src_buf, *dst_buf;
	struct mtk_imgrz_scale_param scale_param, scale_param_2nd;
	int ret = 0;
	bool scale_2nd = false;

	v4l2_dbg(5, debug, &ctx->imgrz->v4l2_dev, "%s called\n", __func__);
	wait_event(imgrz->wait_queue, atomic_read(&imgrz->krn_req_cnt) == 0 && imgrz->state == IMGRZ_STATE_IDLE);
	mutex_lock(&imgrz->hw_lock);
	imgrz->state = IMGRZ_STATE_BUSY;
	imgrz->cur_scale = &scale_param;

	src_buf = v4l2_m2m_next_src_buf(ctx->fh.m2m_ctx);
	dst_buf = v4l2_m2m_next_dst_buf(ctx->fh.m2m_ctx);

	v4l2_dbg(3, debug, &imgrz->v4l2_dev,
		"%s src index:%u, dst index:%u\n", __func__,
		src_buf->index, dst_buf->index);

	memset(&scale_param, 0, sizeof(scale_param));
	ret = mtk_imgrz_prepare_param(ctx, src_buf, dst_buf,
				&scale_param, false);
	if (ret < 0)
		goto resz_end;

	if (mtk_imgrz_need_2scale(ctx, src_buf, dst_buf)) {
		memset(&scale_param_2nd, 0, sizeof(scale_param_2nd));
		ret = mtk_imgrz_prepare_param(ctx, src_buf, dst_buf,
					&scale_param_2nd, true);
		if (ret < 0)
			goto resz_end;
	}

do_scale:
	imgrz->time_start = sched_clock();
	mtk_imgrz_hal_hw_init(imgrz->base);
	mtk_imgrz_hw_do_scale(imgrz, imgrz->cur_scale);
	if (!wait_event_timeout(imgrz->wait_queue,
			imgrz->state == IMGRZ_STATE_DONE,
			msecs_to_jiffies(MTK_IMGRZ_TIMEOUT)))
		mtk_imgrz_timeout(imgrz);
	mtk_imgrz_hal_hw_uninit(imgrz->base);
	if (!scale_2nd && mtk_imgrz_need_2scale(ctx, src_buf, dst_buf)) {
		v4l2_dbg(4, debug, &imgrz->v4l2_dev,
			"%s 2nd scale start.\n", __func__);
		scale_2nd = true;
		imgrz->cur_scale = &scale_param_2nd;
		imgrz->state = IMGRZ_STATE_BUSY;
		goto do_scale;
	}
	buf_state = VB2_BUF_STATE_DONE;

resz_end:
	v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx);
	v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx);
	v4l2_m2m_buf_done(to_vb2_v4l2_buffer(src_buf), buf_state);
	v4l2_m2m_buf_done(to_vb2_v4l2_buffer(dst_buf), buf_state);
	imgrz->state = IMGRZ_STATE_IDLE;
	mutex_unlock(&imgrz->hw_lock);
	v4l2_m2m_job_finish(imgrz->m2m_dev, ctx->fh.m2m_ctx);
}

#if MTK_IMGRZ_JDEC_DL
static int mtk_imgrz_prepare_param_for_jdec(struct mtk_imgrz_ext_ctx *ext_ctx,
			struct mtk_imgrz_ext_param *ext_param,
			struct mtk_imgrz_scale_param *scale_param)
{
	int i;
	size_t temp_buf_sz;

	scale_param->scale_mode = MTK_IMGRZ_PARTIAL_SCALE;
	scale_param->jpeg_pic_mode = true;
	scale_param->h_method = MTK_IMGRZ_M_TAP_RESAMPLE;
	scale_param->v_method = MTK_IMGRZ_M_TAP_RESAMPLE;
	scale_param->partial.first_row = true;
	scale_param->partial.last_row = false;

	scale_param->src.fmt = MTK_IMGRZ_COLOR_FORMAT_Y_CB_CR;
	scale_param->y_exist = ext_param->y_exist;
	scale_param->cb_exist = ext_param->cb_exist;
	scale_param->cr_exist = ext_param->cr_exist;
	scale_param->src.pic_width = ext_param->src_w;
	scale_param->src.pic_height = ext_param->src_h;
	for (i = 0; i < 3; i++) {
		scale_param->src.buf_width[i] = ext_param->src_pitch[i];
		scale_param->src.dma_addr[i] = ext_param->src_buf[i];
		scale_param->partial.pre_row_addr[i] = ext_param->src_buf1[i];
		scale_param->src.h_sample[i] = ext_param->h_sample[i];
		scale_param->src.v_sample[i] = ext_param->v_sample[i];
	}


	scale_param->y_only = ext_param->y_only;
	scale_param->dst.pic_width = ext_param->dst_w;
	scale_param->dst.pic_height = ext_param->dst_h;
	for (i = 0; i < 2; i++) {
		scale_param->dst.buf_width[i] = ext_param->dst_pitch[i];
		scale_param->dst.dma_addr[i] = ext_param->dst_buf[i];
	}
	switch (ext_param->dst_fmt) {
	case V4L2_PIX_FMT_AYUV:
		scale_param->dst.fmt = MTK_IMGRZ_COLOR_FORMAT_AYUV;
		break;
	case V4L2_PIX_FMT_NV12:
		scale_param->dst.fmt = MTK_IMGRZ_COLOR_FORMAT_Y_C;
		scale_param->dst.f.yc_fmt = MTK_IMGRZ_YC_FORMAT_420;
		break;
	case V4L2_PIX_FMT_NV16:
		scale_param->dst.fmt = MTK_IMGRZ_COLOR_FORMAT_Y_C;
		scale_param->dst.f.yc_fmt = MTK_IMGRZ_YC_FORMAT_422;
		break;
	case V4L2_PIX_FMT_NV24:
		scale_param->dst.fmt = MTK_IMGRZ_COLOR_FORMAT_Y_C;
		scale_param->dst.f.yc_fmt = MTK_IMGRZ_YC_FORMAT_444;
		break;
	default:
		pr_err("Unsupport dst color format: %c%c%c%c\n",
			(ext_param->dst_fmt & 0xff),
			(ext_param->dst_fmt >>  8 & 0xff),
			(ext_param->dst_fmt >> 16 & 0xff),
			(ext_param->dst_fmt >> 24 & 0xff));
		return -EINVAL;
	}

	mtk_imgrz_calc_factor(scale_param);

	temp_buf_sz = scale_param->src.buf_width[0] * 3 * 4 * 2;
	if (temp_buf_sz > ext_ctx->temp_buf_sz) {
		if (ext_ctx->temp_buf) {
			dma_free_coherent(ext_ctx->imgrz->dev,
					ext_ctx->temp_buf_sz,
					ext_ctx->temp_buf_va,
					ext_ctx->temp_buf);
		}
		ext_ctx->temp_buf_sz = temp_buf_sz;
		ext_ctx->temp_buf_va = dma_alloc_coherent(ext_ctx->imgrz->dev,
					ext_ctx->temp_buf_sz,
					&ext_ctx->temp_buf,
					GFP_KERNEL | __GFP_ZERO);
		scale_param->partial.temp_buf = ext_ctx->temp_buf;
	}

	v4l2_dbg(3, debug, &ext_ctx->imgrz->v4l2_dev,
		"%s src fmt:%d[%u,%u,%u], pad:0x%pad, dst fmt:%d[%u,%u,%u], pad:0x%pad\n",
		__func__, scale_param->src.fmt, scale_param->src.buf_width[0],
		scale_param->src.pic_width, scale_param->src.pic_height,
		&scale_param->src.dma_addr[0], scale_param->dst.fmt,
		scale_param->dst.buf_width[0], scale_param->dst.pic_width,
		scale_param->dst.pic_height, &scale_param->dst.dma_addr[0]);

	return 0;
}

int mtk_imgrz_config_jdec_pic_mode(void *priv,
			struct mtk_imgrz_ext_param *ext_param)
{
	struct mtk_imgrz_ext_ctx *ext_ctx = priv;
	struct mtk_imgrz_dev *imgrz = ext_ctx->imgrz;
	struct mtk_imgrz_scale_param scale_param;
	int ret = 0;

	memset(&scale_param, 0, sizeof(scale_param));
	ret = mtk_imgrz_prepare_param_for_jdec(ext_ctx,
				ext_param, &scale_param);
	if (ret)
		return ret;

	mtk_imgrz_hal_hw_init(imgrz->base);
	mtk_imgrz_hw_do_scale(imgrz, &scale_param);

	return ret;
}

/* get imgrz hw resource for directlink*/
int mtk_imgrz_ext_wait_dl_rdy(void *priv)
{
	struct mtk_imgrz_ext_ctx *ext_ctx = priv;
	struct mtk_imgrz_dev *imgrz = ext_ctx->imgrz;

	wait_event(imgrz->wait_queue, atomic_read(&imgrz->krn_req_cnt) == 0 && imgrz->state == IMGRZ_STATE_IDLE);
	mutex_lock(&imgrz->hw_lock);
	imgrz->state = IMGRZ_STATE_DL_BUSY;
	return 0;
}

int mtk_imgrz_ext_wait_dl_done(void *priv)
{
	struct mtk_imgrz_ext_ctx *ext_ctx = priv;
	struct mtk_imgrz_dev *imgrz = ext_ctx->imgrz;
	int ret;

	ret = wait_event_timeout(imgrz->wait_queue,
			imgrz->state == IMGRZ_STATE_DL_DONE,
			msecs_to_jiffies(MTK_IMGRZ_TIMEOUT));
	if (!ret)
		mtk_imgrz_timeout(imgrz);
	mtk_imgrz_hal_hw_uninit(imgrz->base);

	imgrz->state = IMGRZ_STATE_IDLE;
	mutex_unlock(&imgrz->hw_lock);
	wake_up(&imgrz->wait_queue);
	return ret;
}

int mtk_imgrz_ext_start_streaming(void *priv)
{
	struct mtk_imgrz_ext_ctx *ext_ctx = priv;
	int ret;

	ret = pm_runtime_get_sync(ext_ctx->imgrz->dev);
	if (ret < 0) {
		v4l2_err(&ext_ctx->imgrz->v4l2_dev,
			"%s: pm_runtime_get_sync failed with %d\n",
			__func__, ret);
		return ret;
	}
	return 0;
}

void mtk_imgrz_ext_stop_streaming(void *priv)
{
	struct mtk_imgrz_ext_ctx *ext_ctx = priv;

	if (!ext_ctx)
		return;
	pm_runtime_put_sync(ext_ctx->imgrz->dev);

	if (ext_ctx->temp_buf) {
		dma_free_coherent(ext_ctx->imgrz->dev, ext_ctx->temp_buf_sz,
				ext_ctx->temp_buf_va, ext_ctx->temp_buf);
		ext_ctx->temp_buf = 0;
		ext_ctx->temp_buf_sz = 0;
		ext_ctx->temp_buf_va = NULL;
	}
}

void *mtk_imgrz_ext_open(struct device *dev)
{
	struct mtk_imgrz_ext_ctx *ext_ctx;

	ext_ctx = kzalloc(sizeof(*ext_ctx), GFP_KERNEL);
	if (!ext_ctx)
		return ERR_PTR(-ENOMEM);
	ext_ctx->imgrz = dev_get_drvdata(dev);

	return ext_ctx;
}

void mtk_imgrz_ext_release(void *priv)
{
	kfree(priv);
}
#endif

static void mtk_imgrz_device_run(void *priv)
{
	struct mtk_imgrz_ctx *ctx = priv;

	queue_work(ctx->imgrz->job_wq, &ctx->work);
	v4l2_dbg(5, debug, &ctx->imgrz->v4l2_dev,
		"%s called\n", __func__);
}

static void mtk_imgrz_job_abort(void *priv)
{
	struct mtk_imgrz_ctx *ctx = priv;

	v4l2_dbg(1, debug, &ctx->imgrz->v4l2_dev, "%s\n", __func__);
}

static struct v4l2_m2m_ops mtk_imgrz_m2m_ops = {
	.device_run	= mtk_imgrz_device_run,
	.job_abort	= mtk_imgrz_job_abort,
};

static int mtk_imgrz_queue_init(void *priv, struct vb2_queue *src_vq,
				struct vb2_queue *dst_vq)
{
	struct mtk_imgrz_ctx *ctx = priv;
	int ret = 0;

	src_vq->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	src_vq->io_modes = VB2_USERPTR;
	src_vq->drv_priv = ctx;
	src_vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);
	src_vq->ops = &mtk_imgrz_qops;
	src_vq->mem_ops = &vb2_dma_contig_memops;
	src_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	src_vq->lock = &ctx->imgrz->lock;
	src_vq->dev = ctx->imgrz->dev;
	ret = vb2_queue_init(src_vq);
	if (ret)
		return ret;

	dst_vq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	dst_vq->io_modes = VB2_USERPTR;
	dst_vq->drv_priv = ctx;
	dst_vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);
	dst_vq->ops = &mtk_imgrz_qops;
	dst_vq->mem_ops = &vb2_dma_contig_memops;
	dst_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	dst_vq->lock = &ctx->imgrz->lock;
	dst_vq->dev = ctx->imgrz->dev;
	ret = vb2_queue_init(dst_vq);

	return ret;
}

static int mtk_imgrz_open(struct file *file)
{
	struct mtk_imgrz_dev *imgrz = video_drvdata(file);
	struct video_device *vfd = video_devdata(file);
	struct mtk_imgrz_ctx *ctx;
	int ret = 0;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	if (mutex_lock_interruptible(&imgrz->lock)) {
		ret = -ERESTARTSYS;
		goto free;
	}

	v4l2_fh_init(&ctx->fh, vfd);
	file->private_data = &ctx->fh;
	v4l2_fh_add(&ctx->fh);

	ctx->imgrz = imgrz;
	ctx->fh.m2m_ctx = v4l2_m2m_ctx_init(imgrz->m2m_dev, ctx,
					    mtk_imgrz_queue_init);
	if (IS_ERR(ctx->fh.m2m_ctx)) {
		ret = PTR_ERR(ctx->fh.m2m_ctx);
		goto error;
	}
	INIT_WORK(&ctx->work, mtk_imgrz_worker);
	mutex_unlock(&imgrz->lock);

	v4l2_dbg(1, debug, &ctx->imgrz->v4l2_dev, "%s success\n", __func__);
	return 0;

error:
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	mutex_unlock(&imgrz->lock);
free:
	kfree(ctx);
	return ret;
}

static int mtk_imgrz_release(struct file *file)
{
	struct mtk_imgrz_dev *imgrz = video_drvdata(file);
	struct mtk_imgrz_ctx *ctx = mtk_imgrz_fh_to_ctx(file->private_data);

	mutex_lock(&imgrz->lock);
	v4l2_m2m_ctx_release(ctx->fh.m2m_ctx);
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	kfree(ctx);
	mutex_unlock(&imgrz->lock);

	v4l2_dbg(1, debug, &imgrz->v4l2_dev, "%s success\n", __func__);
	return 0;
}

static const struct v4l2_file_operations mtk_imgrz_fops = {
	.owner		= THIS_MODULE,
	.open		= mtk_imgrz_open,
	.release	= mtk_imgrz_release,
	.poll		= v4l2_m2m_fop_poll,
	.unlocked_ioctl	= video_ioctl2,
	.mmap		= v4l2_m2m_fop_mmap,
};

static void
mtk_imgrz_update_partial_info(struct mtk_imgrz_scale_param *scale_param)
{
	struct mtk_imgrz_partial_info *partial = &scale_param->partial;
	struct mtk_imgrz_buf_info *src = &scale_param->src;
	int i;

	partial->cur_row += partial->rowbuf_hei;
	partial->first_row = false;
	for (i = 0; i < 3; i++) {
		partial->pre_row_addr[i] = partial->cur_row_addr[i];
		partial->cur_row_addr[i] += src->buf_width[i] *
			partial->rowbuf_hei * scale_param->src.v_sample[i] /
			scale_param->src.v_sample[0];
	}

	if (partial->cur_row + partial->rowbuf_hei >= src->pic_height) {
		partial->rowbuf_hei = src->pic_height - partial->cur_row;
		partial->last_row = true;
	}
}

static irqreturn_t mtk_imgrz_irq_handle(int irq, void *dev)
{
	struct mtk_imgrz_dev *imgrz = (struct mtk_imgrz_dev *)dev;

	imgrz->time_end = sched_clock();
	v4l2_dbg(4, debug, &imgrz->v4l2_dev, "Checksum Rd 0x%x Wr 0x%x\n",
		mtk_imgrz_checksum_read(imgrz->base),
		mtk_imgrz_checksum_write(imgrz->base));

	mtk_imgrz_hal_clr_irq(imgrz->base);

	if (imgrz->state == IMGRZ_STATE_DL_BUSY)
		imgrz->state = IMGRZ_STATE_DL_DONE;
	else if (imgrz->state == IMGRZ_STATE_BUSY) {
		if (imgrz->cur_scale->scale_mode == MTK_IMGRZ_PARTIAL_SCALE &&
		    !imgrz->cur_scale->partial.last_row) {
			/* not last row of partial mode, continue next row */
			mtk_imgrz_update_partial_info(imgrz->cur_scale);
			mtk_imgrz_hw_do_scale(imgrz, imgrz->cur_scale);
			return IRQ_HANDLED;
		}
		imgrz->state = IMGRZ_STATE_DONE;
	} else
		v4l2_err(&imgrz->v4l2_dev, "invalid state when hw done irq\n");

	wake_up(&imgrz->wait_queue);
	v4l2_dbg(2, debug, &imgrz->v4l2_dev, "imgrz done performance:%llu-ns\n",
		imgrz->time_end - imgrz->time_start);
	return IRQ_HANDLED;
}

static int mtk_imgrz_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_imgrz_dev *imgrz;
	struct resource *res;
	int ret = 0;
#ifdef CONFIG_MTK_IOMMU
	struct device_node *larb_node;
	struct platform_device *larb_pdev;
#endif

	imgrz = devm_kzalloc(dev, sizeof(*imgrz), GFP_KERNEL);
	if (!imgrz)
		return -ENOMEM;
	imgrz->dev = dev;
	g_dev = dev;
#ifdef CONFIG_MTK_IOMMU
	larb_node = of_parse_phandle(dev->of_node, "mediatek,larb", 0);
	if (!larb_node)
		return -EINVAL;

	larb_pdev = of_find_device_by_node(larb_node);
	of_node_put(larb_node);
	if ((!larb_pdev) || (!larb_pdev->dev.driver)) {
		dev_err(dev, "%s is earlier than SMI\n", __func__);
		return -EPROBE_DEFER;
	}
	imgrz->smi_larb_dev = &larb_pdev->dev;
#endif

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	imgrz->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(imgrz->base))
		return PTR_ERR(imgrz->base);
#if !IMGRZ_FPGA_EP
	imgrz->clk = devm_clk_get(dev, "imgrz");
	if (IS_ERR(imgrz->clk))
		return PTR_ERR(imgrz->clk);
#endif
	imgrz->irq = platform_get_irq(pdev, 0);
	if (imgrz->irq < 0)
		return imgrz->irq;
	if (devm_request_irq(dev, imgrz->irq, mtk_imgrz_irq_handle, 0,
		dev_name(dev), (void *)imgrz)) {
		dev_err(dev, "Failed @ IRQ-%d Request\n", imgrz->irq);
		return -ENODEV;
	}
	dev_info(dev, "imgrz base: %p\n", imgrz->base);

	mutex_init(&imgrz->lock);
	mutex_init(&imgrz->hw_lock);
	ret = v4l2_device_register(dev, &imgrz->v4l2_dev);
	if (ret) {
		dev_err(dev, "Failed to register v4l2 device\n");
		return -ENODEV;
	}

#if SUPPORT_ALLOC_CTX
	imgrz->alloc_ctx = vb2_dma_contig_init_ctx(dev);
	if (IS_ERR(imgrz->alloc_ctx)) {
		ret = PTR_ERR(imgrz->alloc_ctx);
		imgrz->alloc_ctx = NULL;
		v4l2_err(&imgrz->v4l2_dev, "Failed to alloc vb2 dma context 0\n");
		goto err_vb2_ctx_init;
	}
#endif

	imgrz->m2m_dev = v4l2_m2m_init(&mtk_imgrz_m2m_ops);
	if (IS_ERR(imgrz->m2m_dev)) {
		v4l2_err(&imgrz->v4l2_dev, "Failed to init mem2mem device\n");
		goto err_v4l2_mem_init;
	}
	imgrz->vdev.fops = &mtk_imgrz_fops;
	imgrz->vdev.ioctl_ops = &mtk_imgrz_ioctl_ops;
	imgrz->vdev.release = video_device_release_empty;
	imgrz->vdev.lock = &imgrz->lock;
	imgrz->vdev.vfl_dir = VFL_DIR_M2M;
	imgrz->vdev.v4l2_dev = &imgrz->v4l2_dev;
	ret = video_register_device(&imgrz->vdev, VFL_TYPE_GRABBER, 0);
	if (ret) {
		dev_err(dev, "failed to register video device\n");
		goto err_reg_dev;
	}

	video_set_drvdata(&imgrz->vdev, imgrz);
	platform_set_drvdata(pdev, imgrz);
	init_waitqueue_head(&imgrz->wait_queue);
	imgrz->job_wq = create_singlethread_workqueue(MTK_IMGRZ_NAME);
	if (!imgrz->job_wq) {
		dev_err(dev, "unable to alloc job workqueue\n");
		ret = -ENOMEM;
		goto err_create_wq;
	}
	atomic_set(&imgrz->krn_req_cnt, 0);
	imgrz->state = IMGRZ_STATE_IDLE;
#if !IMGRZ_FPGA_EP
	pm_runtime_enable(&pdev->dev);
#endif
	imgrz_dev = imgrz;

	return ret;

err_create_wq:
	video_unregister_device(&imgrz->vdev);
err_reg_dev:
	v4l2_m2m_release(imgrz->m2m_dev);
err_v4l2_mem_init:
#if SUPPORT_ALLOC_CTX
	vb2_dma_contig_cleanup_ctx(imgrz->alloc_ctx);
err_vb2_ctx_init:
#endif
	v4l2_device_unregister(&imgrz->v4l2_dev);

	return ret;
}

static int mtk_imgrz_remove(struct platform_device *pdev)
{
	return 0;
}

static int __maybe_unused mtk_imgrz_pm_suspend(struct device *dev)
{
	struct mtk_imgrz_dev *imgrz = dev_get_drvdata(dev);

	clk_disable_unprepare(imgrz->clk);
	mtk_smi_larb_put(imgrz->smi_larb_dev);

	return 0;
}


static int __maybe_unused mtk_imgrz_suspend(struct device *dev)
{
	if (pm_runtime_suspended(dev))
		return 0;

	return mtk_imgrz_pm_suspend(dev);
}

static const struct dev_pm_ops mtk_imgrz_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_imgrz_suspend, mtk_imgrz_resume)
	SET_RUNTIME_PM_OPS(mtk_imgrz_pm_suspend, mtk_imgrz_pm_resume, NULL)
};

static const struct of_device_id mtk_imgrz_of_match[] = {
	{.compatible = "mediatek,mt8518-imgrz", .data = NULL},
	{.compatible = "mediatek,mt8512-imgrz", .data = NULL},
	{}
};


static struct platform_driver mtk_imgrz_driver = {
	.probe = mtk_imgrz_probe,
	.remove = mtk_imgrz_remove,
	.driver = {
		.name	= MTK_IMGRZ_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(mtk_imgrz_of_match),
		.pm             = &mtk_imgrz_pm_ops,
	},
};

module_platform_driver(mtk_imgrz_driver);
