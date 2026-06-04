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
#include <linux/iommu.h>

#include <media/v4l2-ioctl.h>
#include <media/v4l2-event.h>
#include <media/v4l2-mem2mem.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-dma-contig.h>
#include <soc/mediatek/smi.h>

#include "mtk_jdec_common.h"
#include "mtk_jdec_core.h"
#include "mtk_jdec_parse.h"
#include "mtk_jdec_hw.h"
#include "mtk_imgrz_ext.h"

#define MTK_JDEC_NAME    "mtk-jdec"
#define MTK_JDEC_TIMEOUT	1000
#define MTK_JDEC_FMT_TYPE_OUTPUT	1
#define MTK_JDEC_FMT_TYPE_CAPTURE	2

enum {
	MTK_JDEC_BUF_FLAGS_INIT			= 0,
	MTK_JDEC_BUF_FLAGS_LAST_FRAME		= 1,
};

struct mtk_jdec_src_buf {
	struct vb2_v4l2_buffer b;
	struct list_head list;
	int flags;
	struct mtk_jdec_dec_param dec_param;
	struct mtk_imgrz_ext_param imgrz_param;
};

static int debug;
module_param(debug, int, 0644);

static struct mtk_jdec_fmt mtk_jdec_formats[] = {
	{
		.fourcc		= V4L2_PIX_FMT_JPEG,
		.colplanes	= 1,
		.type		= MTK_JDEC_FMT_TYPE_OUTPUT,
	},
	{
		.fourcc		= V4L2_PIX_FMT_NV12,
		.h_sample	= {4, 2, 2},
		.v_sample	= {4, 2, 2},
		.colplanes	= 2,
		.h_align	= 1,
		.v_align	= 1,
		.type		= MTK_JDEC_FMT_TYPE_CAPTURE,
	},
	{
		.fourcc		= V4L2_PIX_FMT_NV16,
		.h_sample	= {4, 4, 4},
		.v_sample	= {4, 2, 2},
		.colplanes	= 2,
		.h_align	= 0,
		.v_align	= 1,
		.type		= MTK_JDEC_FMT_TYPE_CAPTURE,
	},
	{
		.fourcc		= V4L2_PIX_FMT_AYUV,
		.h_sample	= {4, 2, 2},
		.v_sample	= {4, 2, 2},
		.colplanes	= 1,
		.h_align	= 0,
		.v_align	= 0,
		.type		= MTK_JDEC_FMT_TYPE_CAPTURE,
	},
	{
		.fourcc		= V4L2_PIX_FMT_GREY,
		.h_sample	= {4, 0, 0},
		.v_sample	= {4, 0, 0},
		.colplanes	= 1,
		.h_align	= 0,
		.v_align	= 0,
		.type		= MTK_JDEC_FMT_TYPE_CAPTURE,
	}
};

#define MTK_JDEC_NUM_FORMATS ARRAY_SIZE(mtk_jdec_formats)

static inline struct mtk_jdec_ctx *mtk_jdec_fh_to_ctx(struct v4l2_fh *fh)
{
	return container_of(fh, struct mtk_jdec_ctx, fh);
}

static inline struct mtk_jdec_src_buf *mtk_jdec_vb2_to_srcbuf(
							struct vb2_buffer *vb)
{
	return container_of(to_vb2_v4l2_buffer(vb), struct mtk_jdec_src_buf, b);
}

static struct mtk_jdec_q_data *mtk_jdec_get_q_data(struct mtk_jdec_ctx *ctx,
						   enum v4l2_buf_type type)
{
	if (V4L2_TYPE_IS_OUTPUT(type))
		return &ctx->out_q;
	return &ctx->cap_q;
}

static struct mtk_jdec_fmt *mtk_jdec_find_format(struct mtk_jdec_ctx *ctx,
						 u32 pixelformat,
						 unsigned int type)
{
	unsigned int k;

	for (k = 0; k < MTK_JDEC_NUM_FORMATS; k++) {
		struct mtk_jdec_fmt *fmt = &mtk_jdec_formats[k];

		if (fmt->fourcc == pixelformat && (fmt->type & type))
			return fmt;
	}

	return NULL;
}

static int mtk_jdec_querycap(struct file *file, void *priv,
			     struct v4l2_capability *cap)
{
	struct mtk_jdec_dev *jdec = video_drvdata(file);

	strlcpy(cap->driver, MTK_JDEC_NAME " decoder", sizeof(cap->driver));
	strlcpy(cap->card, MTK_JDEC_NAME " decoder", sizeof(cap->card));
	snprintf(cap->bus_info, sizeof(cap->bus_info), "platform:%s",
		 dev_name(jdec->dev));
	cap->capabilities = V4L2_CAP_STREAMING |
			    V4L2_CAP_VIDEO_M2M |
			    V4L2_CAP_VIDEO_CAPTURE_MPLANE|
			    V4L2_CAP_VIDEO_OUTPUT_MPLANE|
			    V4L2_CAP_DEVICE_CAPS;
	cap->device_caps =  V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_M2M_MPLANE;

	return 0;
}

static int mtk_jdec_g_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	struct vb2_queue *vq;
	struct mtk_jdec_q_data *q_data = NULL;
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	struct mtk_jdec_ctx *ctx = mtk_jdec_fh_to_ctx(priv);
	struct mtk_jdec_dev *jdec = ctx->jdec;
	int i;

	vq = v4l2_m2m_get_vq(ctx->fh.m2m_ctx, f->type);
	if (!vq)
		return -EINVAL;

	q_data = mtk_jdec_get_q_data(ctx, f->type);
	if (!q_data->fmt) {
		v4l2_err(&jdec->v4l2_dev, "%s fmt not ready\n", __func__);
		return -EINVAL;
	}

	memset(pix_mp->reserved, 0, sizeof(pix_mp->reserved));
	pix_mp->width = q_data->w;
	pix_mp->height = q_data->h;
	pix_mp->field = V4L2_FIELD_NONE;
	pix_mp->pixelformat = q_data->fmt->fourcc;
	pix_mp->num_planes = q_data->fmt->colplanes;
	pix_mp->colorspace = ctx->colorspace;
	pix_mp->ycbcr_enc = ctx->ycbcr_enc;
	pix_mp->xfer_func = ctx->xfer_func;
	pix_mp->quantization = ctx->quantization;

	v4l2_dbg(1, debug, &jdec->v4l2_dev, "(%d) g_fmt:%c%c%c%c wxh:%ux%u\n",
		 f->type,
		 (pix_mp->pixelformat & 0xff),
		 (pix_mp->pixelformat >>  8 & 0xff),
		 (pix_mp->pixelformat >> 16 & 0xff),
		 (pix_mp->pixelformat >> 24 & 0xff),
		 pix_mp->width, pix_mp->height);

	for (i = 0; i < pix_mp->num_planes; i++) {
		struct v4l2_plane_pix_format *pfmt = &pix_mp->plane_fmt[i];

		pfmt->bytesperline = q_data->bytesperline[i];
		pfmt->sizeimage = q_data->sizeimage[i];
		memset(pfmt->reserved, 0, sizeof(pfmt->reserved));

		v4l2_dbg(1, debug, &jdec->v4l2_dev,
			 "plane[%d] bpl=%u, size=%u\n",
			 i,
			 pfmt->bytesperline,
			 pfmt->sizeimage);
	}
	return 0;
}

static int mtk_jdec_s_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	struct vb2_queue *vq;
	struct mtk_jdec_q_data *q_data = NULL;
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	struct mtk_jdec_ctx *ctx = mtk_jdec_fh_to_ctx(priv);
	struct mtk_jdec_dev *jdec = ctx->jdec;
	unsigned int f_type;
	int i;

	vq = v4l2_m2m_get_vq(ctx->fh.m2m_ctx, f->type);
	if (!vq)
		return -EINVAL;

	q_data = mtk_jdec_get_q_data(ctx, f->type);

	if (vb2_is_busy(vq)) {
		v4l2_err(&jdec->v4l2_dev, "queue busy\n");
		return -EBUSY;
	}

	f_type = V4L2_TYPE_IS_OUTPUT(f->type) ?
			 MTK_JDEC_FMT_TYPE_OUTPUT : MTK_JDEC_FMT_TYPE_CAPTURE;

	q_data->fmt = mtk_jdec_find_format(ctx, pix_mp->pixelformat, f_type);
	if (!q_data->fmt) {
		v4l2_err(&jdec->v4l2_dev, "invalid format:%c%c%c%c\n",
			(q_data->fmt->fourcc & 0xff),
			(q_data->fmt->fourcc >>  8 & 0xff),
			(q_data->fmt->fourcc >> 16 & 0xff),
			(q_data->fmt->fourcc >> 24 & 0xff));
		return -EINVAL;
	}
	q_data->w = pix_mp->width;
	q_data->h = pix_mp->height;
	ctx->colorspace = pix_mp->colorspace;
	ctx->ycbcr_enc = pix_mp->ycbcr_enc;
	ctx->xfer_func = pix_mp->xfer_func;
	ctx->quantization = pix_mp->quantization;

	v4l2_dbg(1, debug, &jdec->v4l2_dev, "(%d) s_fmt:%c%c%c%c wxh:%ux%u, %dplane\n",
		 f->type,
		 (q_data->fmt->fourcc & 0xff),
		 (q_data->fmt->fourcc >>  8 & 0xff),
		 (q_data->fmt->fourcc >> 16 & 0xff),
		 (q_data->fmt->fourcc >> 24 & 0xff),
		 q_data->w, q_data->h, q_data->fmt->colplanes);

	for (i = 0; i < q_data->fmt->colplanes; i++) {
		q_data->bytesperline[i] = pix_mp->plane_fmt[i].bytesperline;
		q_data->sizeimage[i] = pix_mp->plane_fmt[i].sizeimage;

		v4l2_dbg(1, debug, &jdec->v4l2_dev,
			 "plane[%d] bpl=%u, size=%u\n",
			 i, q_data->bytesperline[i], q_data->sizeimage[i]);
	}

	if (q_data->fmt->fourcc == V4L2_PIX_FMT_GREY) {
		q_data->bytesperline[1] = q_data->bytesperline[0];
		q_data->sizeimage[1] = q_data->sizeimage[0];
		if (ctx->temp_c_buf_va)
			dma_free_coherent(ctx->jdec->dev, ctx->temp_c_buf_sz,
				ctx->temp_c_buf_va, ctx->temp_c_buf);
		ctx->temp_c_buf_sz = q_data->sizeimage[1];
		ctx->temp_c_buf_va = dma_alloc_coherent(jdec->dev,
			ctx->temp_c_buf_sz, &ctx->temp_c_buf, GFP_KERNEL);
	}

	return 0;
}

static void mtk_jdec_queue_src_chg_event(struct mtk_jdec_ctx *ctx)
{
	static const struct v4l2_event ev_src_ch = {
		.type = V4L2_EVENT_SOURCE_CHANGE,
		.u.src_change.changes =
		V4L2_EVENT_SRC_CH_RESOLUTION,
	};

	v4l2_event_queue_fh(&ctx->fh, &ev_src_ch);
}

static int mtk_jdec_subscribe_event(struct v4l2_fh *fh,
				    const struct v4l2_event_subscription *sub)
{
	switch (sub->type) {
	case V4L2_EVENT_SOURCE_CHANGE:
		return v4l2_src_change_event_subscribe(fh, sub);
	case V4L2_EVENT_EOS:
		return v4l2_event_subscribe(fh, sub, 2, NULL);
	default:
		return -EINVAL;
	}
}

static int mtk_jdec_qbuf(struct file *file, void *priv, struct v4l2_buffer *buf)
{
	struct v4l2_fh *fh = file->private_data;
	struct mtk_jdec_ctx *ctx = mtk_jdec_fh_to_ctx(priv);
	struct vb2_queue *vq;
	struct vb2_buffer *vb;
	struct mtk_jdec_src_buf *jdec_src_buf;
	int ret;

	if (!V4L2_TYPE_IS_OUTPUT(buf->type))
		goto end;

	vq = v4l2_m2m_get_vq(fh->m2m_ctx, buf->type);
	if (buf->index >= vq->num_buffers) {
		dev_err(ctx->jdec->dev, "buffer index out of range\n");
		return -EINVAL;
	}

	vb = vq->bufs[buf->index];
	jdec_src_buf = mtk_jdec_vb2_to_srcbuf(vb);
	jdec_src_buf->flags = (buf->m.planes[0].bytesused == 0) ?
		MTK_JDEC_BUF_FLAGS_LAST_FRAME : MTK_JDEC_BUF_FLAGS_INIT;
end:
	ret = v4l2_m2m_qbuf(file, fh->m2m_ctx, buf);
	v4l2_dbg(2, debug, &ctx->jdec->v4l2_dev,
		"%s ret %d\n", __func__, ret);

	return ret;
}

static const struct v4l2_ioctl_ops mtk_jdec_ioctl_ops = {
	.vidioc_querycap                = mtk_jdec_querycap,

	.vidioc_g_fmt_vid_cap_mplane    = mtk_jdec_g_fmt,
	.vidioc_g_fmt_vid_out_mplane    = mtk_jdec_g_fmt,
	.vidioc_s_fmt_vid_cap_mplane    = mtk_jdec_s_fmt,
	.vidioc_s_fmt_vid_out_mplane    = mtk_jdec_s_fmt,
	.vidioc_qbuf                    = mtk_jdec_qbuf,
	.vidioc_subscribe_event         = mtk_jdec_subscribe_event,

	.vidioc_create_bufs		= v4l2_m2m_ioctl_create_bufs,
	.vidioc_prepare_buf		= v4l2_m2m_ioctl_prepare_buf,
	.vidioc_reqbufs                 = v4l2_m2m_ioctl_reqbufs,
	.vidioc_querybuf                = v4l2_m2m_ioctl_querybuf,
	.vidioc_dqbuf                   = v4l2_m2m_ioctl_dqbuf,
	.vidioc_expbuf                  = v4l2_m2m_ioctl_expbuf,
	.vidioc_streamon                = v4l2_m2m_ioctl_streamon,
	.vidioc_streamoff               = v4l2_m2m_ioctl_streamoff,

	.vidioc_unsubscribe_event	= v4l2_event_unsubscribe,
};

static int mtk_jdec_queue_setup(struct vb2_queue *q,
				unsigned int *num_buffers,
				unsigned int *num_planes,
				unsigned int sizes[],
				struct device *alloc_ctxs[])
{
	struct mtk_jdec_ctx *ctx = vb2_get_drv_priv(q);
	struct mtk_jdec_q_data *q_data = NULL;
	struct mtk_jdec_dev *jdec = ctx->jdec;
	int i;

	v4l2_dbg(1, debug, &jdec->v4l2_dev, "(%d) buf_req count=%u\n",
		 q->type, *num_buffers);

	q_data = mtk_jdec_get_q_data(ctx, q->type);
	if (!q_data)
		return -EINVAL;

	*num_planes = q_data->fmt->colplanes;
	for (i = 0; i < q_data->fmt->colplanes; i++) {
		sizes[i] = V4L2_TYPE_IS_OUTPUT(q->type) ?
			   16 : q_data->sizeimage[i];
		v4l2_dbg(1, debug, &jdec->v4l2_dev, "sizeimage[%d]=%u\n",
			 i, sizes[i]);
	}

	return 0;
}

static int mtk_jdec_buf_prepare(struct vb2_buffer *vb)
{
	struct mtk_jdec_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct mtk_jdec_q_data *q_data = NULL;
	int i;

	q_data = mtk_jdec_get_q_data(ctx, vb->vb2_queue->type);
	if (!q_data)
		return -EINVAL;

	if (V4L2_TYPE_IS_OUTPUT(vb->vb2_queue->type))
		return 0;

	for (i = 0; i < q_data->fmt->colplanes; i++)
		vb2_set_plane_payload(vb, i, q_data->sizeimage[i]);

	return 0;
}

static bool mtk_jdec_check_resolution_change(struct mtk_jdec_ctx *ctx,
					     struct mtk_jdec_dec_param *param)
{
	struct mtk_jdec_dev *jdec = ctx->jdec;
	struct mtk_jdec_q_data *q_data;

	q_data = &ctx->out_q;
	if (q_data->w != param->sof.pic_w || q_data->h != param->sof.pic_h) {
		v4l2_dbg(1, debug, &jdec->v4l2_dev, "Picture size change\n");
		return true;
	}

	#if 0/* need implement later, scott */
	q_data = &ctx->cap_q;
	if (q_data->fmt != mtk_jdec_find_format(ctx, param->dst_fourcc,
						MTK_JDEC_FMT_TYPE_CAPTURE)) {
		v4l2_dbg(1, debug, &jdec->v4l2_dev, "format change\n");
		return true;
	}
	#endif

	return false;
}

static void mtk_jdec_set_disp_param(struct mtk_jdec_ctx *ctx,
				    struct mtk_jdec_dec_param *param,
				    struct vb2_buffer *vb)
{
	struct mtk_jdec_q_data *q_data = &ctx->cap_q;
	int i;

	param->disp.pixelformat = q_data->fmt->fourcc;
	param->disp.pic_w = q_data->w;
	param->disp.pic_h = q_data->h;
	for (i = 0; i < q_data->fmt->colplanes; i++) {
		param->disp.pitch[i] = q_data->bytesperline[i];
		param->disp.disp_buf[i] = vb2_dma_contig_plane_dma_addr(vb, i);
	}
	if (q_data->fmt->fourcc == V4L2_PIX_FMT_GREY) {
		param->disp.pixelformat = V4L2_PIX_FMT_NV16;
		param->disp.pitch[1] = q_data->bytesperline[1];
		param->disp.disp_buf[1] = ctx->temp_c_buf;
	}
	v4l2_dbg(1, debug, &ctx->jdec->v4l2_dev,
		 "%s fmt:%c%c%c%c pic(%u, %u), pitch(0x%x, 0x%x), iova(0x%x, 0x%x)\n",
		 __func__, param->disp.pixelformat & 0xff,
		 (param->disp.pixelformat >>  8) & 0xff,
		 (param->disp.pixelformat >> 16) & 0xff,
		 (param->disp.pixelformat >> 24) & 0xff,
		 param->disp.pic_w, param->disp.pic_h,
		 param->disp.pitch[0], param->disp.pitch[1],
		 param->disp.disp_buf[0], param->disp.disp_buf[1]);
}

static void mtk_jdec_init_imgrz_ext_param(struct mtk_jdec_dec_param *param,
				struct mtk_imgrz_ext_param *imgrz_param)
{
	int i;

	imgrz_param->src_w = param->sof.pic_w;
	imgrz_param->src_h = param->sof.pic_h;
	for (i = 0; i < 3; i++) {
		imgrz_param->src_pitch[i] = param->out.pitch[i];
		imgrz_param->src_buf[i] = param->out.bank0[i];
		imgrz_param->src_buf1[i] = param->out.bank1[i];
		imgrz_param->h_sample[i] = param->sof.sampling_h[i];
		imgrz_param->v_sample[i] = param->sof.sampling_v[i];
	}
	if (param->sof.comp_num == 1) {
		imgrz_param->y_exist = true;
		imgrz_param->y_only = true;
	} else {
		imgrz_param->y_exist = true;
		imgrz_param->cb_exist = true;
		imgrz_param->cr_exist = true;
	}

	imgrz_param->dst_fmt = param->disp.pixelformat;
	imgrz_param->dst_w = param->disp.pic_w;
	imgrz_param->dst_h = param->disp.pic_h;
	for (i = 0; i < 2; i++) {
		imgrz_param->dst_pitch[i] = param->disp.pitch[i];
		imgrz_param->dst_buf[i] = param->disp.disp_buf[i];
	}
}

static void mtk_jdec_set_queue_data(struct mtk_jdec_ctx *ctx,
				    struct mtk_jdec_dec_param *param)
{
	struct mtk_jdec_dev *jdec = ctx->jdec;
	struct mtk_jdec_q_data *q_data;

	q_data = &ctx->out_q;
	q_data->w = param->sof.pic_w;
	q_data->h = param->sof.pic_h;

	q_data = &ctx->cap_q;
	q_data->w = param->sof.pic_w;
	q_data->h = param->sof.pic_h;

	q_data->fmt = mtk_jdec_find_format(ctx,
					   V4L2_PIX_FMT_NV12,
					   MTK_JDEC_FMT_TYPE_CAPTURE);

	if (q_data->fmt->fourcc == V4L2_PIX_FMT_NV12) {
		q_data->bytesperline[0] = ALIGN(q_data->w, 16);
		q_data->bytesperline[1] =
			ALIGN(DIV_ROUND_UP(q_data->w, 2), 16);
		q_data->sizeimage[0] = ALIGN(q_data->bytesperline[0] *
				q_data->h, dma_get_cache_alignment());
		q_data->sizeimage[1] = ALIGN(q_data->bytesperline[1] *
				q_data->h, dma_get_cache_alignment());
	}

	v4l2_dbg(1, debug, &jdec->v4l2_dev,
		 "%s fmt:%c%c%c%c pic(%u, %u), pitch(0x%x, 0x%x), sizeimage(0x%x, 0x%x)\n",
		 __func__, q_data->fmt->fourcc & 0xff,
		 (q_data->fmt->fourcc >>  8) & 0xff,
		 (q_data->fmt->fourcc >> 16) & 0xff,
		 (q_data->fmt->fourcc >> 24) & 0xff,
		 param->sof.pic_w, param->sof.pic_h,
		 q_data->bytesperline[0], q_data->bytesperline[1],
		 q_data->sizeimage[0], q_data->sizeimage[1]);
}

static int mtk_jdec_alloc_buf(struct mtk_jdec_dev *jdec,
				struct mtk_jdec_dec_param *param)
{
	int i, size[3];

	switch (param->dec_md) {
	case JDEC_DEC_MODE_BASELINE_PIC:
		param->out.color_buf_sz = 0;
		for (i = 0; i < 3; i++) {
			size[i] = param->out.pitch[i] * 8 *
				param->sof.sampling_v[i];
			param->out.color_buf_sz += size[i];
		}
		param->out.color_buf_sz *= 2;
		param->out.color_buf_va = dma_alloc_coherent(jdec->dev,
			param->out.color_buf_sz,
			&param->out.color_buf, GFP_KERNEL | __GFP_ZERO);
		param->out.bank0[0] = param->out.color_buf;
		param->out.bank0[1] = param->out.bank0[0] + size[0];
		param->out.bank0[2] = param->out.bank0[1] + size[1];
		param->out.bank1[0] = param->out.bank0[2] + size[2];
		param->out.bank1[1] = param->out.bank1[0] + size[0];
		param->out.bank1[2] = param->out.bank1[1] + size[1];
		param->out.out_buf[0] = param->out.bank0[0];
		param->out.out_buf[1] = param->out.bank0[1];
		param->out.out_buf[2] = param->out.bank0[2];
		v4l2_dbg(1, debug, &jdec->v4l2_dev,
			"bank0:(0x%pad, 0x%pad, 0x%pad), bank1:(0x%pad, 0x%pad, 0x%pad)\n",
			&param->out.bank0[0], &param->out.bank0[1],
			&param->out.bank0[2], &param->out.bank1[0],
			&param->out.bank1[1], &param->out.bank1[2]);
		break;
	case JDEC_DEC_MODE_PROGRESSIVE_SCAN_MULTI_COLLECT:
		break;
	case JDEC_DEC_MODE_PROGRESSIVE_SCAN_ENHANCE:
		break;
	default:
		v4l2_err(&jdec->v4l2_dev, "%s unsupport dec mode:%d.\n",
			__func__, param->dec_md);
		return -EINVAL;
	}

	return 0;
}

static int mtk_jdec_free_buf(struct mtk_jdec_dev *jdec,
				struct mtk_jdec_dec_param *param)
{
	switch (param->dec_md) {
	case JDEC_DEC_MODE_BASELINE_PIC:
		 dma_free_coherent(jdec->dev, param->out.color_buf_sz,
				param->out.color_buf_va, param->out.color_buf);
		break;
	case JDEC_DEC_MODE_PROGRESSIVE_SCAN_MULTI_COLLECT:
		break;
	case JDEC_DEC_MODE_PROGRESSIVE_SCAN_ENHANCE:
		break;
	default:
		v4l2_err(&jdec->v4l2_dev, "%s unsupport dec mode:%d.\n",
			__func__, param->dec_md);
		return -EINVAL;
	}

	return 0;
}

static void mtk_jdec_buf_queue(struct vb2_buffer *vb)
{
	struct mtk_jdec_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct mtk_jdec_dec_param *param;
	struct mtk_jdec_dev *jdec = ctx->jdec;
	struct mtk_jdec_src_buf *jdec_src_buf;
	struct mtk_jdec_q_data *q_data = NULL;
	bool header_valid;

	v4l2_dbg(2, debug, &jdec->v4l2_dev, "(%d) buf_q id=%d, vb=%p\n",
		 vb->vb2_queue->type, vb->index, vb);

	if (!V4L2_TYPE_IS_OUTPUT(vb->vb2_queue->type))
		goto end;

	q_data = mtk_jdec_get_q_data(ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	jdec_src_buf = mtk_jdec_vb2_to_srcbuf(vb);
	if (jdec_src_buf->flags & MTK_JDEC_BUF_FLAGS_LAST_FRAME) {
		v4l2_dbg(1, debug, &jdec->v4l2_dev, "Got eos\n");
		goto end;
	}

	param = &jdec_src_buf->dec_param;
	memset(param, 0, sizeof(*param));
	param->stream.addr = (u8 *)vb2_plane_vaddr(vb, 0);
	param->stream.dma_addr = vb2_dma_contig_plane_dma_addr(vb, 0);
	param->stream.size = vb2_get_plane_payload(vb, 0);
	param->stream.curr = 0;
	header_valid = mtk_jdec_parse(param);
	if (!header_valid) {
		v4l2_err(&jdec->v4l2_dev, "Header invalid.\n");
		vb2_buffer_done(vb, VB2_BUF_STATE_ERROR);
		return;
	}
	mtk_jdec_alloc_buf(jdec, param);
	if (ctx->state == MTK_JDEC_INIT) {
		struct vb2_queue *dst_vq = v4l2_m2m_get_vq(
			ctx->fh.m2m_ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);

		mtk_jdec_queue_src_chg_event(ctx);
		mtk_jdec_set_queue_data(ctx, param);
		ctx->state = vb2_is_streaming(dst_vq) ?
				MTK_JDEC_SOURCE_CHANGE : MTK_JDEC_RUNNING;
	}
end:
	v4l2_m2m_buf_queue(ctx->fh.m2m_ctx, to_vb2_v4l2_buffer(vb));
}

static void *mtk_jdec_buf_remove(struct mtk_jdec_ctx *ctx,
				 enum v4l2_buf_type type)
{
	if (V4L2_TYPE_IS_OUTPUT(type))
		return v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx);
	else
		return v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx);
}

static int mtk_jdec_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct mtk_jdec_ctx *ctx = vb2_get_drv_priv(q);
	struct vb2_buffer *vb;
	int ret;

	v4l2_dbg(2, debug, &ctx->jdec->v4l2_dev,
		"%s count:%d\n", __func__, count);

	ret = mtk_imgrz_ext_start_streaming(ctx->imgrz_ctx);
	if (ret < 0)
		goto err;
	ret = pm_runtime_get_sync(ctx->jdec->dev);
	if (ret < 0)
		goto err;
	return 0;
err:
	while ((vb = mtk_jdec_buf_remove(ctx, q->type)))
		v4l2_m2m_buf_done(to_vb2_v4l2_buffer(vb), VB2_BUF_STATE_QUEUED);
	return ret;
}

static void mtk_jdec_stop_streaming(struct vb2_queue *q)
{
	struct mtk_jdec_ctx *ctx = vb2_get_drv_priv(q);
	struct vb2_buffer *vb;

	pm_runtime_put_sync(ctx->jdec->dev);
	mtk_imgrz_ext_stop_streaming(ctx->imgrz_ctx);
	/*
	 * STREAMOFF is an acknowledgment for source change event.
	 * Before STREAMOFF, we still have to return the old resolution and
	 * subsampling. Update capture queue when the stream is off.
	 */
	if (ctx->state == MTK_JDEC_SOURCE_CHANGE &&
	    !V4L2_TYPE_IS_OUTPUT(q->type)) {
		struct mtk_jdec_src_buf *src_buf;

		vb = v4l2_m2m_next_src_buf(ctx->fh.m2m_ctx);
		src_buf = mtk_jdec_vb2_to_srcbuf(vb);
		mtk_jdec_set_queue_data(ctx, &src_buf->dec_param);
		ctx->state = MTK_JDEC_RUNNING;
	} else if (V4L2_TYPE_IS_OUTPUT(q->type)) {
		ctx->state = MTK_JDEC_INIT;
	}

	while ((vb = mtk_jdec_buf_remove(ctx, q->type)))
		v4l2_m2m_buf_done(to_vb2_v4l2_buffer(vb), VB2_BUF_STATE_ERROR);
}

static struct vb2_ops mtk_jdec_qops = {
	.queue_setup        = mtk_jdec_queue_setup,
	.buf_prepare        = mtk_jdec_buf_prepare,
	.buf_queue          = mtk_jdec_buf_queue,
	.wait_prepare       = vb2_ops_wait_prepare,
	.wait_finish        = vb2_ops_wait_finish,
	.start_streaming    = mtk_jdec_start_streaming,
	.stop_streaming     = mtk_jdec_stop_streaming,
};

static int mtk_jdec_time_out(struct mtk_jdec_dev *jdec)
{
	dev_err(jdec->dev, "jdec timeout\n");
	mtk_jdec_hw_dump_reg(jdec->base);
	return 0;
}

static int mtk_jdec_baseline_dec(struct mtk_jdec_dev *jdec,
				void *imgrz_ctx,
				struct mtk_jdec_dec_param *param,
				struct mtk_imgrz_ext_param *imgrz_param)
{
	int ret = 0;

	mtk_imgrz_ext_wait_dl_rdy(imgrz_ctx);
	jdec->time_start = sched_clock();

	mtk_jdec_hw_init(jdec->base);
	ret = mtk_imgrz_config_jdec_pic_mode(imgrz_ctx, imgrz_param);
	if (ret)
		goto imgrz_err;
	mtk_jdec_hw_config(jdec->base, param);
	mtk_jdec_hw_trig_dec(jdec->base, param);

	if (!wait_event_timeout(jdec->wait_queue, jdec->state == MTK_JDEC_DONE,
			msecs_to_jiffies(MTK_JDEC_TIMEOUT)))
		mtk_jdec_time_out(jdec);
	mtk_imgrz_ext_wait_dl_done(imgrz_ctx);

	jdec->time_end = sched_clock();
	v4l2_dbg(2, debug, &jdec->v4l2_dev, "jdec done performance:%llu-ns\n",
		jdec->time_end - jdec->time_start);

imgrz_err:
	mtk_jdec_hw_unint(jdec->base);

	return 0;
}

static void mtk_jdec_worker(struct work_struct *work)
{
	struct mtk_jdec_ctx *ctx =
		container_of(work, struct mtk_jdec_ctx, work);
	struct mtk_jdec_dev *jdec = ctx->jdec;
	struct vb2_buffer *src_buf, *dst_buf;
	enum vb2_buffer_state buf_state = VB2_BUF_STATE_ERROR;
	struct mtk_jdec_src_buf *jdec_src_buf;
	int i, ret;

	jdec->state = MTK_JDEC_BUSY;
	src_buf = v4l2_m2m_next_src_buf(ctx->fh.m2m_ctx);
	dst_buf = v4l2_m2m_next_dst_buf(ctx->fh.m2m_ctx);
	jdec_src_buf = mtk_jdec_vb2_to_srcbuf(src_buf);

	if (jdec_src_buf->flags & MTK_JDEC_BUF_FLAGS_LAST_FRAME) {
		for (i = 0; i < dst_buf->num_planes; i++)
			vb2_set_plane_payload(dst_buf, i, 0);
		buf_state = VB2_BUF_STATE_DONE;
		goto dec_end;
	}

	if (mtk_jdec_check_resolution_change(ctx, &jdec_src_buf->dec_param)) {
		mtk_jdec_queue_src_chg_event(ctx);
		ctx->state = MTK_JDEC_SOURCE_CHANGE;
		v4l2_m2m_job_finish(jdec->m2m_dev, ctx->fh.m2m_ctx);
		return;
	}

	dst_buf->timestamp = src_buf->timestamp;
	mtk_jdec_set_disp_param(ctx, &jdec_src_buf->dec_param, dst_buf);
	mtk_jdec_init_imgrz_ext_param(&jdec_src_buf->dec_param,
			&jdec_src_buf->imgrz_param);
	ret = mtk_jdec_baseline_dec(jdec, ctx->imgrz_ctx,
			&jdec_src_buf->dec_param, &jdec_src_buf->imgrz_param);
	if (!ret)
		buf_state = VB2_BUF_STATE_DONE;

dec_end:
	mtk_jdec_free_buf(jdec, &jdec_src_buf->dec_param);
	v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx);
	v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx);
	v4l2_m2m_buf_done(to_vb2_v4l2_buffer(src_buf), buf_state);
	v4l2_m2m_buf_done(to_vb2_v4l2_buffer(dst_buf), buf_state);
	v4l2_m2m_job_finish(jdec->m2m_dev, ctx->fh.m2m_ctx);
	jdec->state = MTK_JDEC_IDLE;
}

static void mtk_jdec_device_run(void *priv)
{
	struct mtk_jdec_ctx *ctx = priv;

	queue_work(ctx->jdec->job_wq, &ctx->work);
}

static int mtk_jdec_job_ready(void *priv)
{
	struct mtk_jdec_ctx *ctx = priv;

	return (ctx->state == MTK_JDEC_RUNNING) ? 1 : 0;
}

static void mtk_jdec_job_abort(void *priv)
{
}

static struct v4l2_m2m_ops mtk_jdec_m2m_ops = {
	.device_run = mtk_jdec_device_run,
	.job_ready  = mtk_jdec_job_ready,
	.job_abort  = mtk_jdec_job_abort,
};

static int mtk_jdec_queue_init(void *priv, struct vb2_queue *src_vq,
			       struct vb2_queue *dst_vq)
{
	struct mtk_jdec_ctx *ctx = priv;
	int ret;

	src_vq->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	src_vq->io_modes = VB2_USERPTR;
	src_vq->drv_priv = ctx;
	src_vq->buf_struct_size = sizeof(struct mtk_jdec_src_buf);
	src_vq->ops = &mtk_jdec_qops;
	src_vq->mem_ops = &vb2_dma_contig_memops;
	src_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	src_vq->lock = &ctx->jdec->lock;
	src_vq->dev = ctx->jdec->dev;
	ret = vb2_queue_init(src_vq);
	if (ret)
		return ret;

	dst_vq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	dst_vq->io_modes = VB2_USERPTR;
	dst_vq->drv_priv = ctx;
	dst_vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);
	dst_vq->ops = &mtk_jdec_qops;
	dst_vq->mem_ops = &vb2_dma_contig_memops;
	dst_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	dst_vq->lock = &ctx->jdec->lock;
	dst_vq->dev = ctx->jdec->dev;
	ret = vb2_queue_init(dst_vq);

	return ret;
}

static int mtk_jdec_open(struct file *file)
{
	struct mtk_jdec_dev *jdec = video_drvdata(file);
	struct video_device *vfd = video_devdata(file);
	struct mtk_jdec_ctx *ctx;
	int ret = 0;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	if (mutex_lock_interruptible(&jdec->lock)) {
		ret = -ERESTARTSYS;
		goto free;
	}

	v4l2_fh_init(&ctx->fh, vfd);
	file->private_data = &ctx->fh;
	v4l2_fh_add(&ctx->fh);

	ctx->jdec = jdec;
	ctx->fh.m2m_ctx = v4l2_m2m_ctx_init(jdec->m2m_dev, ctx,
					    mtk_jdec_queue_init);
	if (IS_ERR(ctx->fh.m2m_ctx)) {
		ret = PTR_ERR(ctx->fh.m2m_ctx);
		goto error;
	}

	ctx->imgrz_ctx = mtk_imgrz_ext_open(jdec->resizer);
	if (IS_ERR_OR_NULL(ctx->imgrz_ctx)) {
		ret = PTR_ERR(ctx->imgrz_ctx);
		goto error;
	}

	INIT_WORK(&ctx->work, mtk_jdec_worker);
	mutex_unlock(&jdec->lock);
	v4l2_dbg(2, debug, &ctx->jdec->v4l2_dev, "%s success\n", __func__);
	return 0;

error:
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	mutex_unlock(&jdec->lock);
free:
	kfree(ctx);
	return ret;
}

static int mtk_jdec_release(struct file *file)
{
	struct mtk_jdec_dev *jdec = video_drvdata(file);
	struct mtk_jdec_ctx *ctx = mtk_jdec_fh_to_ctx(file->private_data);

	mutex_lock(&jdec->lock);
	if (ctx->temp_c_buf_va) {
		dma_free_coherent(ctx->jdec->dev, ctx->temp_c_buf_sz,
			ctx->temp_c_buf_va, ctx->temp_c_buf);
		ctx->temp_c_buf_va = NULL;
	}
	ctx->state = MTK_JDEC_INIT;
	v4l2_m2m_ctx_release(ctx->fh.m2m_ctx);
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	mtk_imgrz_ext_release(ctx->imgrz_ctx);
	kfree(ctx);
	mutex_unlock(&jdec->lock);

	v4l2_dbg(2, debug, &jdec->v4l2_dev, "%s success\n", __func__);
	return 0;
}

static const struct v4l2_file_operations mtk_jdec_fops = {
	.owner          = THIS_MODULE,
	.open           = mtk_jdec_open,
	.release        = mtk_jdec_release,
	.poll           = v4l2_m2m_fop_poll,
	.unlocked_ioctl = video_ioctl2,
	.mmap           = v4l2_m2m_fop_mmap,
};

static irqreturn_t mtk_jdec_dec_irq(int irq, void *priv)
{
	struct mtk_jdec_dev *jdec = priv;

	mtk_jdec_hw_clr_irq(jdec->base);
	v4l2_dbg(2, debug, &jdec->v4l2_dev, "%s.\n", __func__);

	jdec->state = MTK_JDEC_DONE;
	wake_up(&jdec->wait_queue);

	return IRQ_HANDLED;
}

static int mtk_jdec_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_jdec_dev *jdec;
	struct resource *res;
	int ret;
	struct device_node *larb_node, *resizer_node;
	struct platform_device *larb_pdev, *resizer_pdev;

	jdec = devm_kzalloc(dev, sizeof(*jdec), GFP_KERNEL);
	if (!jdec)
		return -ENOMEM;

	if (!iommu_get_domain_for_dev(dev)) {
		dev_err(dev, "[IOMMU]iommu driver not ready");
		return -EPROBE_DEFER;
	}

	larb_node = of_parse_phandle(dev->of_node, "mediatek,larb", 0);
	if (!larb_node)
		return -EINVAL;
	larb_pdev = of_find_device_by_node(larb_node);
	of_node_put(larb_node);
	if ((!larb_pdev) || (!larb_pdev->dev.driver)) {
		dev_err(dev, "%s is earlier than SMI\n", __func__);
		return -EPROBE_DEFER;
	}
	jdec->dev = dev;
	jdec->larb = &larb_pdev->dev;

	resizer_node = of_parse_phandle(dev->of_node, "mediatek,resizer", 0);
	if (!resizer_node)
		return -EINVAL;
	resizer_pdev = of_find_device_by_node(resizer_node);
	of_node_put(resizer_node);
	if ((!resizer_pdev) || (!resizer_pdev->dev.driver)) {
		dev_err(dev, "%s is earlier than resizer\n", __func__);
		return -EPROBE_DEFER;
	}
	jdec->resizer = &resizer_pdev->dev;

	jdec->clk = devm_clk_get(dev, "jdec");
	if (IS_ERR(jdec->clk))
		return -EINVAL;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	jdec->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(jdec->base)) {
		ret = PTR_ERR(jdec->base);
		return ret;
	}

	jdec->dec_irq = platform_get_irq(pdev, 0);
	if (jdec->dec_irq < 0) {
		dev_err(&pdev->dev,
			"Failed to get dec_irq %d.\n", jdec->dec_irq);
		return -EINVAL;
	}
#if MTK_JDEC_IRQ_WORKARROUND
	irq_set_status_flags(jdec->dec_irq, IRQ_NOAUTOEN);
#endif
	ret = devm_request_irq(&pdev->dev, jdec->dec_irq, mtk_jdec_dec_irq, 0,
			       pdev->name, jdec);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request dec_irq %d (%d)\n",
			jdec->dec_irq, ret);
		return -EINVAL;
	}
	ret = v4l2_device_register(&pdev->dev, &jdec->v4l2_dev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register v4l2 device\n");
		ret = -EINVAL;
		goto err_dev_register;
	}

	jdec->m2m_dev = v4l2_m2m_init(&mtk_jdec_m2m_ops);
	if (IS_ERR(jdec->m2m_dev)) {
		v4l2_err(&jdec->v4l2_dev, "Failed to init mem2mem device\n");
		ret = PTR_ERR(jdec->m2m_dev);
		goto err_m2m_init;
	}

	jdec->vdev = video_device_alloc();
	if (!jdec->vdev) {
		ret = -ENOMEM;
		goto err_vdev_alloc;
	}

	jdec->vdev->fops = &mtk_jdec_fops;
	jdec->vdev->ioctl_ops = &mtk_jdec_ioctl_ops;
	jdec->vdev->minor = -1;
	jdec->vdev->release = video_device_release;
	jdec->vdev->lock = &jdec->lock;
	jdec->vdev->v4l2_dev = &jdec->v4l2_dev;
	jdec->vdev->vfl_dir = VFL_DIR_M2M;
	jdec->vdev->device_caps = V4L2_CAP_STREAMING |
				      V4L2_CAP_VIDEO_M2M_MPLANE;

	ret = video_register_device(jdec->vdev, VFL_TYPE_GRABBER, 0);
	if (ret) {
		v4l2_err(&jdec->v4l2_dev, "Failed to register video device\n");
		goto err_vdev_register;
	}

	video_set_drvdata(jdec->vdev, jdec);
	v4l2_info(&jdec->v4l2_dev,
		  "decoder device registered as /dev/video%d (%d,%d)\n",
		  jdec->vdev->num, VIDEO_MAJOR, jdec->vdev->minor);

	platform_set_drvdata(pdev, jdec);
	mutex_init(&jdec->lock);
	spin_lock_init(&jdec->hw_lock);
	init_waitqueue_head(&jdec->wait_queue);
	jdec->job_wq = create_singlethread_workqueue(MTK_JDEC_NAME);

	pm_runtime_enable(&pdev->dev);

	return 0;

err_vdev_register:
	video_device_release(jdec->vdev);

err_vdev_alloc:
	v4l2_m2m_release(jdec->m2m_dev);

err_m2m_init:
	v4l2_device_unregister(&jdec->v4l2_dev);

err_dev_register:

	return ret;
}

static int mtk_jdec_remove(struct platform_device *pdev)
{
	struct mtk_jdec_dev *jdec = platform_get_drvdata(pdev);

	pm_runtime_disable(&pdev->dev);
	video_unregister_device(jdec->vdev);
	video_device_release(jdec->vdev);
	v4l2_m2m_release(jdec->m2m_dev);
	v4l2_device_unregister(&jdec->v4l2_dev);

	return 0;
}

static int mtk_jdec_clk_on(struct mtk_jdec_dev *jdec)
{
	int ret;

	ret = mtk_smi_larb_get(jdec->larb);
	if (ret) {
		dev_err(jdec->dev, "mtk_smi_larb_get fail %d\n", ret);
		return ret;
	}
	ret = clk_prepare_enable(jdec->clk);
	if (ret) {
		dev_err(jdec->dev, "enable clk fail %d\n", ret);
		mtk_smi_larb_put(jdec->larb);
		return ret;
	}

#if MTK_JDEC_IRQ_WORKARROUND
	enable_irq(jdec->dec_irq);
#endif
	return ret;
}

static void mtk_jdec_clk_off(struct mtk_jdec_dev *jdec)
{
#if MTK_JDEC_IRQ_WORKARROUND
	disable_irq(jdec->dec_irq);
#endif
	clk_disable_unprepare(jdec->clk);
	mtk_smi_larb_put(jdec->larb);
}

static int mtk_jdec_pm_suspend(struct device *dev)
{
	struct mtk_jdec_dev *jdec = dev_get_drvdata(dev);

	mtk_jdec_clk_off(jdec);

	return 0;
}

static int mtk_jdec_pm_resume(struct device *dev)
{
	struct mtk_jdec_dev *jdec = dev_get_drvdata(dev);

	return mtk_jdec_clk_on(jdec);
}

static int mtk_jdec_suspend(struct device *dev)
{
	int ret;

	if (pm_runtime_suspended(dev))
		return 0;

	ret = mtk_jdec_pm_suspend(dev);
	return ret;
}

static int mtk_jdec_resume(struct device *dev)
{
	int ret;

	if (pm_runtime_suspended(dev))
		return 0;

	ret = mtk_jdec_pm_resume(dev);

	return ret;
}

static const struct dev_pm_ops mtk_jdec_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_jdec_suspend, mtk_jdec_resume)
	SET_RUNTIME_PM_OPS(mtk_jdec_pm_suspend, mtk_jdec_pm_resume, NULL)
};

static const struct of_device_id mtk_jdec_match[] = {
	{
		.compatible = "mediatek,mt8512-jdec",
		.data       = NULL,
	},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_jdec_match);

static struct platform_driver mtk_jdec_driver = {
	.probe = mtk_jdec_probe,
	.remove = mtk_jdec_remove,
	.driver = {
		.owner          = THIS_MODULE,
		.name           = MTK_JDEC_NAME,
		.of_match_table = mtk_jdec_match,
		.pm             = &mtk_jdec_pm_ops,
	},
};

module_platform_driver(mtk_jdec_driver);

MODULE_DESCRIPTION("MediaTek jdec decoder driver");
MODULE_LICENSE("GPL v2");
