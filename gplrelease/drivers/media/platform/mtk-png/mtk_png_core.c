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

#include "mtk_png_core.h"
#include "mtk_png_hal.h"

#define PNG_PLANES_NUM	1

static int debug;
module_param(debug, int, 0644);

static int v4l2_fmt_bpp(__u32 fmt)
{
	int bpp = 0;

	switch (fmt) {
	case V4L2_PIX_FMT_ARGB32:
		bpp = 4;
		break;
	case V4L2_PIX_FMT_ARGB444:
		bpp = 2;
		break;
	case V4L2_PIX_FMT_RGB565:
		bpp = 2;
		break;
	case V4L2_PIX_FMT_ARGB555:
		bpp = 2;
		break;
	case V4L2_PIX_FMT_PAL8:
		bpp = 2;
		break;
	default:
		pr_err("%s - unsupported fmt:%c%c%c%c\n", __func__,
			fmt & 0xff,
			(fmt >> 8) & 0xff,
			(fmt >> 16) & 0xff,
			(fmt >> 24) & 0xff);
		break;
	}
	return bpp;
}

static inline struct mtk_png_ctx *mtk_png_fh_to_ctx(struct v4l2_fh *fh)
{
	return container_of(fh, struct mtk_png_ctx, fh);
}

static inline struct mtk_png_src_buf *mtk_png_vb2_to_srcbuf(
							struct vb2_buffer *vb)
{
	return container_of(to_vb2_v4l2_buffer(vb), struct mtk_png_src_buf, b);
}

static struct mtk_png_q_data *mtk_png_get_q_data(struct mtk_png_ctx *ctx,
						   enum v4l2_buf_type type)
{
	if (V4L2_TYPE_IS_OUTPUT(type))
		return &ctx->out_q;
	return &ctx->cap_q;
}

static int mtk_png_querycap(struct file *file, void *priv,
			     struct v4l2_capability *cap)
{
	struct mtk_png_dev *png = video_drvdata(file);

	strlcpy(cap->driver, MTK_PNG_DECODER_NAME " decoder",
		sizeof(cap->driver));
	strlcpy(cap->card, MTK_PNG_DECODER_NAME " decoder", sizeof(cap->card));
	snprintf(cap->bus_info, sizeof(cap->bus_info), "platform:%s",
		 dev_name(png->dev));

	cap->capabilities = V4L2_CAP_STREAMING |
			    V4L2_CAP_VIDEO_M2M |
			    V4L2_CAP_VIDEO_CAPTURE_MPLANE|
			    V4L2_CAP_VIDEO_OUTPUT_MPLANE|
			    V4L2_CAP_DEVICE_CAPS;
	cap->device_caps =  V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_M2M_MPLANE;
	return 0;
}

static int mtk_png_g_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	struct mtk_png_q_data *q_data = NULL;
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	struct mtk_png_ctx *ctx = mtk_png_fh_to_ctx(priv);
	struct mtk_png_dev *png = ctx->png;
	struct vb2_queue *vq = v4l2_m2m_get_vq(ctx->fh.m2m_ctx, f->type);

	if (!vq)
		return -EINVAL;

	q_data = mtk_png_get_q_data(ctx, f->type);
	pix_mp->width = q_data->w;
	pix_mp->height = q_data->h;
	pix_mp->field = V4L2_FIELD_NONE;
	pix_mp->pixelformat = q_data->pixelformat;
	pix_mp->num_planes = q_data->num_planes;
	pix_mp->plane_fmt[0].bytesperline = q_data->bytesperline;
	pix_mp->plane_fmt[0].sizeimage = q_data->sizeimage;

	v4l2_dbg(1, debug, &png->v4l2_dev, "(%d) g_fmt:%c%c%c%c w&h:%ux%u bpl=%u size=%u\n",
		 f->type, (pix_mp->pixelformat & 0xff),
		 (pix_mp->pixelformat >>  8 & 0xff),
		 (pix_mp->pixelformat >> 16 & 0xff),
		 (pix_mp->pixelformat >> 24 & 0xff),
		 pix_mp->width, pix_mp->height,
		 pix_mp->plane_fmt[0].bytesperline,
		 pix_mp->plane_fmt[0].sizeimage);

	return 0;
}

static int mtk_png_s_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	struct mtk_png_q_data *q_data = NULL;
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	struct mtk_png_ctx *ctx = mtk_png_fh_to_ctx(priv);
	struct mtk_png_dev *png = ctx->png;
	struct vb2_queue *vq = v4l2_m2m_get_vq(ctx->fh.m2m_ctx, f->type);

	if (!vq)
		return -EINVAL;
	if (pix_mp->num_planes != 1 || pix_mp->plane_fmt[0].bytesperline & 0xf)
		return -EINVAL;

	q_data = mtk_png_get_q_data(ctx, f->type);

	if (vb2_is_busy(vq)) {
		v4l2_err(&png->v4l2_dev, "queue busy\n");
		return -EBUSY;
	}

	q_data->w = pix_mp->width;
	q_data->h = pix_mp->height;
	q_data->pixelformat = pix_mp->pixelformat;
	q_data->num_planes = pix_mp->num_planes;
	q_data->bytesperline = pix_mp->plane_fmt[0].bytesperline;
	q_data->sizeimage = pix_mp->plane_fmt[0].sizeimage;

	v4l2_dbg(1, debug, &png->v4l2_dev, "(%d) s_fmt:%c%c%c%c, w&h:%ux%u bpl=%u size=%u\n",
		 f->type, (q_data->pixelformat & 0xff),
		 (q_data->pixelformat >>  8 & 0xff),
		 (q_data->pixelformat >> 16 & 0xff),
		 (q_data->pixelformat >> 24 & 0xff),
		 q_data->w, q_data->h, q_data->bytesperline, q_data->sizeimage);

	return 0;
}

static int mtk_png_qbuf(struct file *file, void *priv, struct v4l2_buffer *buf)
{
	int ret;
	struct v4l2_fh *fh = file->private_data;
	struct mtk_png_ctx *ctx = priv;

	ret = v4l2_m2m_qbuf(file, fh->m2m_ctx, buf);

	v4l2_dbg(2, debug, &ctx->png->v4l2_dev,
		"%s ret %d\n", __func__, ret);

	return ret;
}

static const struct v4l2_ioctl_ops mtk_png_ioctl_ops = {
	.vidioc_querycap		= mtk_png_querycap,
	.vidioc_g_fmt_vid_cap_mplane	= mtk_png_g_fmt,
	.vidioc_g_fmt_vid_out_mplane	= mtk_png_g_fmt,
	.vidioc_s_fmt_vid_cap_mplane	= mtk_png_s_fmt,
	.vidioc_s_fmt_vid_out_mplane	= mtk_png_s_fmt,
	.vidioc_qbuf			= mtk_png_qbuf,
	.vidioc_create_bufs		= v4l2_m2m_ioctl_create_bufs,
	.vidioc_prepare_buf		= v4l2_m2m_ioctl_prepare_buf,
	.vidioc_reqbufs			= v4l2_m2m_ioctl_reqbufs,
	.vidioc_querybuf		= v4l2_m2m_ioctl_querybuf,
	.vidioc_dqbuf			= v4l2_m2m_ioctl_dqbuf,
	.vidioc_expbuf			= v4l2_m2m_ioctl_expbuf,
	.vidioc_streamon		= v4l2_m2m_ioctl_streamon,
	.vidioc_streamoff		= v4l2_m2m_ioctl_streamoff,
};


static int mtk_png_queue_setup(struct vb2_queue *q,
			   unsigned int *num_buffers, unsigned int *num_planes,
			   unsigned int sizes[], struct device *alloc_ctxs[])
{
	struct mtk_png_ctx *ctx = vb2_get_drv_priv(q);
	struct mtk_png_q_data *q_data = NULL;
	struct mtk_png_dev *png = ctx->png;

	v4l2_dbg(1, debug, &png->v4l2_dev, "(%d) buf_req count=%u\n",
		 q->type, *num_buffers);

	q_data = mtk_png_get_q_data(ctx, q->type);
	if (!q_data)
		return -EINVAL;
	if (q_data->num_planes != 1)
		return -EINVAL;

	*num_planes = q_data->num_planes;
	sizes[0] = V4L2_TYPE_IS_OUTPUT(q->type) ? 16 : q_data->sizeimage;
#if SUPPORT_ALLOC_CTX
	alloc_ctxs[0] = png->alloc_ctx;
#endif
	v4l2_dbg(1, debug, &png->v4l2_dev, "sizeimage=%u\n", sizes[0]);

	return 0;
}

static int mtk_png_buf_prepare(struct vb2_buffer *vb)
{
	struct mtk_png_q_data *q_data = NULL;
	struct mtk_png_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct mtk_png_dev *png = ctx->png;

	v4l2_dbg(1, debug, &png->v4l2_dev,
		"%s index:%u, byteused:0x%lx, length:0x%lx\n", __func__,
		vb->index, vb2_get_plane_payload(vb, 0), vb2_plane_size(vb, 0));

	q_data = mtk_png_get_q_data(ctx, vb->vb2_queue->type);
	if (!q_data)
		return -EINVAL;

	return 0;
}

static void mtk_png_set_queue_data(struct mtk_png_ctx *ctx,
				    struct mtk_png_dec_param *param)
{
	struct mtk_png_q_data *q_data;

	q_data = &ctx->out_q;
	q_data->w = param->ihdr.pic_w;
	q_data->h = param->ihdr.pic_h;

	q_data = &ctx->cap_q;
	q_data->w = param->ihdr.pic_w;
	q_data->h = param->ihdr.pic_h;
	q_data->pixelformat = V4L2_PIX_FMT_ARGB32;
	q_data->num_planes = 1;
	q_data->bytesperline =
		ALIGN(q_data->w * v4l2_fmt_bpp(q_data->pixelformat), 16);
	q_data->sizeimage = ALIGN(q_data->bytesperline * q_data->h,
				 dma_get_cache_alignment());
}

static void mtk_png_buf_queue(struct vb2_buffer *vb)
{
	struct mtk_png_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct mtk_png_dec_param *param;
	struct mtk_png_dev *png = ctx->png;
	struct mtk_png_src_buf *src_buf;
	bool header_valid;

	v4l2_dbg(2, debug, &png->v4l2_dev,
		"(%d) buf_q id=%d, vb=%p, userptr=0x%lx, %pad, payload=0x%lx\n",
		 vb->vb2_queue->type, vb->index, vb, vb->planes[0].m.userptr,
		 (dma_addr_t *)vb2_plane_cookie(vb, 0),
		 vb2_get_plane_payload(vb, 0));

	if (vb->vb2_queue->type != V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		goto end;

	src_buf = mtk_png_vb2_to_srcbuf(vb);
	param = &src_buf->dec_param;
	memset(param, 0, sizeof(*param));
	header_valid = mtk_png_parse(param, vb2_plane_vaddr(vb, 0),
				vb2_get_plane_payload(vb, 0));
	if (!header_valid) {
		v4l2_err(&png->v4l2_dev, "Header invalid.\n");
		vb2_buffer_done(vb, VB2_BUF_STATE_ERROR);
		return;
	}

	mtk_png_set_queue_data(ctx, param);
end:
	v4l2_m2m_buf_queue(ctx->fh.m2m_ctx, to_vb2_v4l2_buffer(vb));
}

static void mtk_png_buf_finish(struct vb2_buffer *vb)
{
	struct mtk_png_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct mtk_png_q_data *q_data =
		mtk_png_get_q_data(ctx, vb->vb2_queue->type);

	if (vb->vb2_queue->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		return;

	vb2_set_plane_payload(vb, 0, ALIGN(q_data->bytesperline * q_data->h,
				 dma_get_cache_alignment()));

#if !PNG_FPGA_EP
	pm_runtime_put_sync(ctx->png->dev);
#endif
}

static void *mtk_png_buf_remove(struct mtk_png_ctx *ctx,
				 enum v4l2_buf_type type)
{
	v4l2_dbg(2, debug, &ctx->png->v4l2_dev,
		"%s type:%d\n", __func__, type);

	if (V4L2_TYPE_IS_OUTPUT(type))
		return v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx);
	else
		return v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx);
}

static irqreturn_t mtk_png_src_irq_handle(int irq, void *dev);
static irqreturn_t mtk_png_dst_irq_handle(int irq, void *dev);
static irqreturn_t mtk_png_dec_irq_handle(int irq, void *dev);

static int mtk_png_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct mtk_png_ctx *ctx = vb2_get_drv_priv(q);
	struct mtk_png_dev *png = ctx->png;
	int ret = 0;

	v4l2_dbg(2, debug, &png->v4l2_dev,
		"%s count:%u\n", __func__, count);

	if (V4L2_TYPE_IS_OUTPUT(q->type))
		png->lz77.va = dma_alloc_coherent(png->dev,
			PNG_LZ77_BUF_SIZE, &png->lz77.iova, GFP_KERNEL);
	return ret;
}

static void mtk_png_stop_streaming(struct vb2_queue *q)
{
	struct mtk_png_ctx *ctx = vb2_get_drv_priv(q);
	struct mtk_png_dev *png = ctx->png;
	struct vb2_buffer *vb;

	v4l2_dbg(1, debug, &png->v4l2_dev, "%s\n", __func__);

	if (V4L2_TYPE_IS_OUTPUT(q->type))
		dma_free_coherent(png->dev, PNG_LZ77_BUF_SIZE,
				png->lz77.va, png->lz77.iova);
	while ((vb = mtk_png_buf_remove(ctx, q->type)))
		v4l2_m2m_buf_done(to_vb2_v4l2_buffer(vb), VB2_BUF_STATE_ERROR);
}

static struct vb2_ops mtk_png_qops = {
	.queue_setup        = mtk_png_queue_setup,
	.buf_prepare        = mtk_png_buf_prepare,
	.buf_queue          = mtk_png_buf_queue,
	.buf_finish         = mtk_png_buf_finish,
	.wait_prepare       = vb2_ops_wait_prepare,
	.wait_finish        = vb2_ops_wait_finish,
	.start_streaming    = mtk_png_start_streaming,
	.stop_streaming     = mtk_png_stop_streaming,
};

static int png_bbp(u8 color_type, u8 bit_depth)
{
	int ret = 0;

	switch (color_type) {
	case 0:	/* greyscale */
		ret = bit_depth;
		break;
	case 2: /* truecolor */
		ret = bit_depth * 3;
		break;
	case 3: /* index */
		ret = bit_depth;
		break;
	case 4: /* greyscale with alpha */
		ret = bit_depth * 2;
		break;
	case 6: /* truecolor with alpha */
		ret = bit_depth * 4;
		break;
	default:
		break;
	}

	return ret;
}


bool mtk_png_pic_info(struct mtk_png_ctx *ctx, struct vb2_buffer *src_vb)
{
	unsigned int line_buf_size;
	struct mtk_png_dev *png = ctx->png;
	struct mtk_png_src_buf *src_buf = mtk_png_vb2_to_srcbuf(src_vb);
	struct mtk_png_q_data *q_data = &ctx->cap_q;
	unsigned int bbp = png_bbp(src_buf->dec_param.ihdr.color_type,
		src_buf->dec_param.ihdr.bit_depth);

	memset(png->lz77.va, 0, PNG_LZ77_BUF_SIZE);
	mtk_png_hw_set_lz77_buf(png->base,
		(u32)png->lz77.iova, PNG_LZ77_BUF_SIZE);

	/* 32 bytes align */
	line_buf_size = ALIGN(src_buf->dec_param.ihdr.pic_w * bbp, 8*32)/8;
	src_buf->line_buf_va = dma_alloc_coherent(png->dev, line_buf_size,
				&src_buf->line_buf_iova, GFP_KERNEL);
	memset(src_buf->line_buf_va, 0, line_buf_size);
	mtk_png_hw_set_line_buf(png->base,
		(u32)src_buf->line_buf_iova, line_buf_size);

	mtk_png_hw_set_bs_fifo(png->base,
		(u32)vb2_dma_contig_plane_dma_addr(src_vb, 0),
		vb2_plane_size(src_vb, 0));

	mtk_png_hw_set_src(png->base, &src_buf->dec_param.ihdr);
	mtk_png_hw_set_output(png->base, q_data->pixelformat,
		q_data->bytesperline, src_buf->dec_param.ihdr.pic_h);
	mtk_png_hw_set_crop(png->base, 0, 0,
		src_buf->dec_param.ihdr.pic_w, src_buf->dec_param.ihdr.pic_h);

	return true;
}


static void mtk_png_device_run(void *priv)
{
	struct mtk_png_ctx *ctx = priv;
	struct mtk_png_dev *png = ctx->png;
	enum vb2_buffer_state buf_state = VB2_BUF_STATE_ERROR;
	struct vb2_buffer *src_vb, *dst_vb;
	struct mtk_png_src_buf *src_buf;
	int ret;

	src_vb = v4l2_m2m_next_src_buf(ctx->fh.m2m_ctx);
	dst_vb = v4l2_m2m_next_dst_buf(ctx->fh.m2m_ctx);
	src_buf = mtk_png_vb2_to_srcbuf(src_vb);

	v4l2_dbg(2, debug, &ctx->png->v4l2_dev,
		"%s src index:%u, dst index:%u\n", __func__,
		src_vb->index, dst_vb->index);
	png->time_start = sched_clock();
	ret = pm_runtime_get_sync(png->dev);
	if (ret < 0) {
		v4l2_err(&png->v4l2_dev,
		"%s: pm_runtime_get_sync failed with %d\n",
		__func__, ret);
	}
	mtk_png_hw_init(png);
	mtk_png_pic_info(ctx, src_vb);
	if (!mtk_png_set_plte(png, &src_buf->dec_param.plte,
			vb2_dma_contig_plane_dma_addr(src_vb, 0)))
		goto dec_end;
	if (!mtk_png_set_trns(png, &src_buf->dec_param.trns,
			vb2_dma_contig_plane_dma_addr(src_vb, 0),
			(void *)src_vb->planes[0].m.userptr))
		goto dec_end;
	if (!mtk_png_dec_idat(png, &src_buf->dec_param.idat,
			vb2_dma_contig_plane_dma_addr(src_vb, 0),
			vb2_dma_contig_plane_dma_addr(dst_vb, 0),
			vb2_plane_size(dst_vb, 0)))
		goto dec_end;

	return;

dec_end:
	v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx);
	v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx);
	v4l2_m2m_buf_done(to_vb2_v4l2_buffer(src_vb), buf_state);
	v4l2_m2m_buf_done(to_vb2_v4l2_buffer(dst_vb), buf_state);
	v4l2_m2m_job_finish(png->m2m_dev, ctx->fh.m2m_ctx);
}


static void mtk_png_job_abort(void *priv)
{
	struct mtk_png_ctx *ctx = priv;

	v4l2_dbg(1, debug, &ctx->png->v4l2_dev, "%s\n", __func__);
}

static struct v4l2_m2m_ops mtk_png_m2m_ops = {
	.device_run	= mtk_png_device_run,
	.job_abort	= mtk_png_job_abort,
};

static int mtk_png_queue_init(void *priv, struct vb2_queue *src_vq,
			       struct vb2_queue *dst_vq)
{
	struct mtk_png_ctx *ctx = priv;
	int ret;

	src_vq->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	src_vq->io_modes = VB2_USERPTR;
	src_vq->drv_priv = ctx;
	src_vq->buf_struct_size = sizeof(struct mtk_png_src_buf);
	src_vq->ops = &mtk_png_qops;
	src_vq->mem_ops = &vb2_dma_contig_memops;
	src_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	src_vq->lock = &ctx->png->lock;
	src_vq->dev = ctx->png->dev;
	ret = vb2_queue_init(src_vq);
	if (ret)
		return ret;

	dst_vq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	dst_vq->io_modes = VB2_USERPTR;
	dst_vq->drv_priv = ctx;
	dst_vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);
	dst_vq->ops = &mtk_png_qops;
	dst_vq->mem_ops = &vb2_dma_contig_memops;
	dst_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	dst_vq->lock = &ctx->png->lock;
	dst_vq->dev = ctx->png->dev;
	ret = vb2_queue_init(dst_vq);

	return ret;
}

static int mtk_png_open(struct file *file)
{
	struct mtk_png_dev *png = video_drvdata(file);
	struct video_device *vfd = video_devdata(file);
	struct mtk_png_ctx *ctx;
	int ret = 0;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	if (mutex_lock_interruptible(&png->lock)) {
		ret = -ERESTARTSYS;
		goto free;
	}

	v4l2_fh_init(&ctx->fh, vfd);
	file->private_data = &ctx->fh;
	v4l2_fh_add(&ctx->fh);

	ctx->png = png;
	ctx->fh.m2m_ctx = v4l2_m2m_ctx_init(png->m2m_dev, ctx,
					    mtk_png_queue_init);
	if (IS_ERR(ctx->fh.m2m_ctx)) {
		ret = PTR_ERR(ctx->fh.m2m_ctx);
		goto error;
	}

	/*mtk_png_set_default_params(ctx);*/
	mutex_unlock(&png->lock);

	v4l2_dbg(2, debug, &ctx->png->v4l2_dev,	"%s success\n", __func__);
	return 0;

error:
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	mutex_unlock(&png->lock);
free:
	kfree(ctx);
	return ret;
}

static int mtk_png_release(struct file *file)
{
	struct mtk_png_dev *png = video_drvdata(file);
	struct mtk_png_ctx *ctx = mtk_png_fh_to_ctx(file->private_data);

	mutex_lock(&png->lock);
	v4l2_m2m_ctx_release(ctx->fh.m2m_ctx);
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	kfree(ctx);
	mutex_unlock(&png->lock);

	v4l2_dbg(2, debug, &png->v4l2_dev, "%s success\n", __func__);
	return 0;
}

static const struct v4l2_file_operations mtk_png_fops = {
	.owner		= THIS_MODULE,
	.open		= mtk_png_open,
	.release	= mtk_png_release,
	.poll		= v4l2_m2m_fop_poll,
	.unlocked_ioctl	= video_ioctl2,
	.mmap		= v4l2_m2m_fop_mmap,
};


static irqreturn_t mtk_png_src_irq_handle(int irq, void *dev)
{
	struct mtk_png_dev *png = (struct mtk_png_dev *)dev;

	mtk_png_hw_clr_irq(PNG_SRC_IRQ, png);

	return IRQ_HANDLED;
}

static irqreturn_t mtk_png_dst_irq_handle(int irq, void *dev)
{
	struct mtk_png_dev *png = (struct mtk_png_dev *)dev;

	mtk_png_hw_clr_irq(PNG_DST_IRQ, png);

	return IRQ_HANDLED;
}

static irqreturn_t mtk_png_dec_irq_handle(int irq, void *dev)
{
	struct vb2_buffer *src_buf, *dst_buf;
	struct mtk_png_dev *png = (struct mtk_png_dev *)dev;
	struct mtk_png_ctx *ctx;
	enum mtk_png_state state = png->state;
	enum vb2_buffer_state buf_state = VB2_BUF_STATE_DONE;

	mtk_png_hw_clr_irq(PNG_DEC_IRQ, png);

	ctx = v4l2_m2m_get_curr_priv(png->m2m_dev);
	if (!ctx) {
		v4l2_err(&png->v4l2_dev, "Context is NULL\n");
		return IRQ_HANDLED;
	}

	png->state = PNG_STATE_DONE;
	if (state == PNG_STATE_PARSE)
		wake_up(&png->wait_hw_done_queue);
	else {
		png->time_end = sched_clock();
		v4l2_dbg(2, debug, &png->v4l2_dev,
			"png done performance:%llu-ns\n",
			png->time_end - png->time_start);
		src_buf = v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx);
		dst_buf = v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx);
		v4l2_m2m_buf_done(to_vb2_v4l2_buffer(src_buf), buf_state);
		v4l2_m2m_buf_done(to_vb2_v4l2_buffer(dst_buf), buf_state);
		v4l2_m2m_job_finish(png->m2m_dev, ctx->fh.m2m_ctx);
	}

	return IRQ_HANDLED;
}

static int mtk_png_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_png_dev *png;
	struct resource *res;
#ifdef CONFIG_MTK_IOMMU
	struct device_node *larb_node;
	struct platform_device *larb_pdev;
#endif
	int ret;

	pr_err("%s called\n", __func__);
	png = devm_kzalloc(dev, sizeof(*png), GFP_KERNEL);
	if (!png)
		return -ENOMEM;
	png->dev = dev;

#ifdef CONFIG_MTK_IOMMU
	larb_node = of_parse_phandle(pdev->dev.of_node, "mediatek,larb", 0);
	if (!larb_node)
		return -EINVAL;

	larb_pdev = of_find_device_by_node(larb_node);
	of_node_put(larb_node);
	if ((!larb_pdev) || (!larb_pdev->dev.driver)) {
		pr_err("imgresz_probe is earlier than SMI\n");
		return -EPROBE_DEFER;
	}
	png->smi_larb_dev = &larb_pdev->dev;
#endif

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	png->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(png->base))
		return PTR_ERR(png->base);

#if !PNG_FPGA_EP
	png->clk = devm_clk_get(dev, "png");
	if (IS_ERR(png->clk))
		return PTR_ERR(png->clk);
#endif
	png->dec_irq = platform_get_irq(pdev, 0);
	if (png->dec_irq < 0)
		return png->dec_irq;
#if !MTK_PNG_IRQ_WORKARROUND
	if (devm_request_irq(dev, png->dec_irq, mtk_png_dec_irq_handle, 0,
		dev_name(dev), (void *)png)) {
		dev_err(dev, "Failed @ IRQ-%d Request\n", png->dec_irq);
		return -ENODEV;
	}
#endif
	png->src_irq = platform_get_irq(pdev, 1);
	if (png->src_irq < 0)
		return png->src_irq;
#if !MTK_PNG_IRQ_WORKARROUND
	if (devm_request_irq(dev, png->src_irq, mtk_png_src_irq_handle, 0,
		dev_name(dev), (void *)png)) {
		dev_err(dev, "Failed @ IRQ-%d Request\n", png->src_irq);
		return -ENODEV;
	}
#endif
	png->dst_irq = platform_get_irq(pdev, 2);
	if (png->dst_irq < 0)
		return png->dst_irq;
#if !MTK_PNG_IRQ_WORKARROUND
	if (devm_request_irq(dev, png->dst_irq, mtk_png_dst_irq_handle, 0,
		dev_name(dev), (void *)png)) {
		dev_err(dev, "Failed @ IRQ-%d Request\n", png->dst_irq);
		return -ENODEV;
	}
#endif

	mutex_init(&png->lock);
	ret = v4l2_device_register(dev, &png->v4l2_dev);
	if (ret) {
		dev_err(dev, "Failed to register v4l2 device\n");
		return -ENODEV;
	}

#if SUPPORT_ALLOC_CTX
	png->alloc_ctx = vb2_dma_contig_init_ctx(dev);
	if (IS_ERR(png->alloc_ctx)) {
		ret = PTR_ERR(png->alloc_ctx);
		png->alloc_ctx = NULL;
		v4l2_err(&png->v4l2_dev, "Failed to alloc vb2 dma context 0\n");
		goto err_vb2_ctx_init;
	}
#endif
	png->m2m_dev = v4l2_m2m_init(&mtk_png_m2m_ops);
	if (IS_ERR(png->m2m_dev)) {
		v4l2_err(&png->v4l2_dev, "Failed to init mem2mem device\n");
		goto err_v4l2_mem_init;
	}
	png->vdev.fops = &mtk_png_fops;
	png->vdev.ioctl_ops = &mtk_png_ioctl_ops;
	png->vdev.release = video_device_release_empty;
	png->vdev.lock = &png->lock;
	png->vdev.vfl_dir = VFL_DIR_M2M;
	png->vdev.v4l2_dev = &png->v4l2_dev;
	ret = video_register_device(&png->vdev, VFL_TYPE_GRABBER, 0);
	if (ret) {
		dev_err(dev, "failed to register video device\n");
		goto err_reg_dev;
	}
	v4l2_info(&png->v4l2_dev, "driver registered as /dev/video%d",
		png->vdev.num);

	pr_err("driver registered as /dev/video%d",
		png->vdev.num);
	video_set_drvdata(&png->vdev, png);
	platform_set_drvdata(pdev, png);
	init_waitqueue_head(&png->wait_hw_done_queue);
#if !PNG_FPGA_EP
	pm_runtime_enable(&pdev->dev);
#endif
	return ret;

err_reg_dev:
	v4l2_m2m_release(png->m2m_dev);
err_v4l2_mem_init:
#if SUPPORT_ALLOC_CTX
	vb2_dma_contig_cleanup_ctx(png->alloc_ctx);
err_vb2_ctx_init:
#endif
	v4l2_device_unregister(&png->v4l2_dev);

	return ret;

}

static int mtk_png_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id mtk_png_of_match[] = {
	{.compatible = "mediatek,mt8518-png", .data = NULL},
	{.compatible = "mediatek,mt8512-png", .data = NULL},
	{}
};


static int mtk_png_clk_on(struct mtk_png_dev *png)
{
	int ret;

#if !PNG_FPGA_EP
	ret = mtk_smi_larb_get(png->smi_larb_dev);
	if (ret < 0) {
		v4l2_err(&png->v4l2_dev,
		"%s: mtk_smi_larb_get failed with %d\n",
		__func__, ret);
	return ret;
	}
	ret = clk_prepare_enable(png->clk);
	if (ret < 0) {
		v4l2_err(&png->v4l2_dev,
		"%s: clk_prepare_enable failed with %d\n",
		__func__, ret);
	return ret;
	}
#endif

#if MTK_PNG_IRQ_WORKARROUND
	if (devm_request_irq(png->dev, png->dec_irq, mtk_png_dec_irq_handle, 0,
		dev_name(png->dev), (void *)png)) {
		return -ENODEV;
	}
	if (devm_request_irq(png->dev, png->src_irq, mtk_png_src_irq_handle, 0,
		dev_name(png->dev), (void *)png)) {
		return -ENODEV;
	}
	if (devm_request_irq(png->dev, png->dst_irq, mtk_png_dst_irq_handle, 0,
		dev_name(png->dev), (void *)png)) {
		return -ENODEV;
	}
#endif

	return ret;
}

static void mtk_png_clk_off(struct mtk_png_dev *png)
{
#if MTK_PNG_IRQ_WORKARROUND
	devm_free_irq(png->dev, png->dec_irq, png);
	devm_free_irq(png->dev, png->src_irq, png);
	devm_free_irq(png->dev, png->dst_irq, png);
#endif

#if !PNG_FPGA_EP
	clk_disable_unprepare(png->clk);
	mtk_smi_larb_put(png->smi_larb_dev);
#endif

}

static int mtk_png_pm_suspend(struct device *dev)
{
	struct mtk_png_dev *png = dev_get_drvdata(dev);

	mtk_png_clk_off(png);

	return 0;
}

static int mtk_png_pm_resume(struct device *dev)
{
	struct mtk_png_dev *png = dev_get_drvdata(dev);

	return mtk_png_clk_on(png);
}

static int mtk_png_suspend(struct device *dev)
{
	int ret;

	if (pm_runtime_suspended(dev))
		return 0;

	ret = mtk_png_pm_suspend(dev);
	return ret;
}

static int mtk_png_resume(struct device *dev)
{
	int ret;

	if (pm_runtime_suspended(dev))
		return 0;

	ret = mtk_png_pm_resume(dev);

	return ret;
}

static const struct dev_pm_ops mtk_png_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_png_suspend, mtk_png_resume)
	SET_RUNTIME_PM_OPS(mtk_png_pm_suspend, mtk_png_pm_resume, NULL)
};


static struct platform_driver mtk_png_driver = {
	.probe = mtk_png_probe,
	.remove = mtk_png_remove,
	.driver = {
		.name	= MTK_PNG_DECODER_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(mtk_png_of_match),
		.pm = &mtk_png_pm_ops,
	},
};

module_platform_driver(mtk_png_driver);

MODULE_DESCRIPTION("MediaTek PNG decoder driver");
MODULE_LICENSE("GPL v2");
