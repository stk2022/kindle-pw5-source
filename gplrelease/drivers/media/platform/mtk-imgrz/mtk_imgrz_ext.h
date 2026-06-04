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

#ifndef MTK_IMGRZ_EXT_H
#define MTK_IMGRZ_EXT_H

struct mtk_imgrz_ext_param {
	unsigned int	src_w;
	unsigned int	src_h;
	u16		src_pitch[3];
	dma_addr_t	src_buf[3];
	dma_addr_t	src_buf1[3];
	unsigned int	h_sample[3];
	unsigned int	v_sample[3];
	bool	y_exist;
	bool	cb_exist;
	bool	cr_exist;

	u32	dst_fmt;
	unsigned int	dst_w;
	unsigned int	dst_h;
	u16		dst_pitch[3];
	dma_addr_t	dst_buf[3];
	bool	y_only;
};

struct mtk_imgrz_scale_buf_info {

	unsigned int	buf_w;   /* Known as pitch. */
	unsigned int	buf_h;
	unsigned int	pic_w;
	unsigned int	pic_h;
	unsigned int	x_offset;
	unsigned int	y_offset;
	dma_addr_t	dma_buf;
};

struct mtk_imgrz_scale_kapi_param {
	struct mtk_imgrz_scale_buf_info src_info;
	struct mtk_imgrz_scale_buf_info dst_info;
};

void *mtk_imgrz_ext_open(struct device *dev);
void mtk_imgrz_ext_release(void *priv);
int mtk_imgrz_ext_start_streaming(void *priv);
void mtk_imgrz_ext_stop_streaming(void *priv);

int mtk_imgrz_ext_wait_dl_rdy(void *priv);
int mtk_imgrz_ext_wait_dl_done(void *priv);
int mtk_imgrz_config_jdec_pic_mode(void *priv,
			struct mtk_imgrz_ext_param *ext_param);
int
mtk_imgrz_scale(struct mtk_imgrz_scale_kapi_param *scale_kapi_param);
#endif
