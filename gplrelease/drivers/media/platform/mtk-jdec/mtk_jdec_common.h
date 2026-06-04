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

#ifndef MTK_JDEC_COMMON_H
#define MTK_JDEC_COMMON_H

#include <linux/wait.h>
#include <linux/mutex.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fh.h>

#define MTK_JPEG_COMP_MAX	3
#define JDEC_FPGA_EP	0
#define MTK_JDEC_IRQ_WORKARROUND 1

enum mtk_jdec_dec_mode {
	JDEC_DEC_MODE_NONE,
	JDEC_DEC_MODE_BASELINE_PIC,
	JDEC_DEC_MODE_BASELINE_VIDEO_OUTPUT,
	JDEC_DEC_MODE_PROGRESSIVE_SCAN_MULTI_COLLECT,
	JDEC_DEC_MODE_PROGRESSIVE_SCAN_ENHANCE,
};

enum jpeg_enc_fmt {
	MTK_JPEG_UNKNOWN_FORMAT			= 0,
	MTK_JPEG_BASELINE			= 1,
	MTK_JPEG_EXTENDED_SEQUENTIAL_HUFFMAN	= 2,
	MTK_JPEG_PROGRESSIVE_HUFFMAN		= 3,
	MTK_JPEG_UNSUPPORT_FORMAT		= 4
};


struct mtk_jdec_dri_param {
	u16	restart_interval;
};

/**
 * struct mtk_jdec_dht_tab - JDEC hufman table
 * @num:		Number of Huffman codes
 * @val:		Value associated with each Huffman code
 * @val_oft:		index of 1st symbol of code length l,
 *			minus the minimum code of length l
 */
struct mtk_jdec_dht_tab {
	u8	num[17];
	u8	val[256];
	u8	val_cnt;
	u16	max_code[17];
	u16	val_oft[17];
};

struct mtk_jdec_dht_param {
	struct mtk_jdec_dht_tab	dc[4];
	struct mtk_jdec_dht_tab	ac[4];
};

struct mtk_jdec_dqt_param {
	bool	exist;
	u8	element[4][128];
	u8	precision[4];
	u8	num;
};

struct mtk_jdec_sos_param {
	u8	comp_num;
	u8	comp_id[MTK_JPEG_COMP_MAX];
	u8	dc_id[MTK_JPEG_COMP_MAX];
	u8	ac_id[MTK_JPEG_COMP_MAX];
	u8	ss;
	u8	se;
	u8	ah;
	u8	al;
};

struct mtk_jdec_sof_param {
	enum jpeg_enc_fmt	enc_fmt;
	bool	exist;
	u16	pic_w;
	u16	pic_h;
	u8	comp_num;
	u8	comp_id[MTK_JPEG_COMP_MAX];
	u16	sampling_h[MTK_JPEG_COMP_MAX];
	u16	sampling_v[MTK_JPEG_COMP_MAX];
	u8	qtbl_num[MTK_JPEG_COMP_MAX];
};

struct mtk_jdec_out_param {
	u16		pitch[3];
	void		*color_buf_va;
	size_t		color_buf_sz;
	dma_addr_t	color_buf;
	dma_addr_t	out_buf[3];
	dma_addr_t	bank0[3];
	dma_addr_t	bank1[3];
	dma_addr_t	nonzero_history;
	dma_addr_t	coef_buf[3];
};

struct mtk_jdec_disp_param {
	u32	pixelformat;
	u16	pic_w;
	u16	pic_h;
	u16	sampling_h[3];
	u16	sampling_v[3];
	u16	pitch[3];
	dma_addr_t	disp_buf[3];
};

struct mtk_jdec_stream {
	dma_addr_t	dma_addr;
	u8	*addr;
	u32	size;
	u32	curr;
};

struct mtk_jdec_dec_param {
	bool	err_conceal;
	u16	wid_per_mcu;
	u16	hei_per_mcu;
	u16	mcu_per_row;
	u16	total_mcu_row;
	enum mtk_jdec_dec_mode		dec_md;
	struct mtk_jdec_stream		stream;
	struct mtk_jdec_sof_param	sof;
	struct mtk_jdec_dht_param	dht;
	struct mtk_jdec_dqt_param	dqt;
	struct mtk_jdec_dri_param	dri;
	struct mtk_jdec_sos_param	sos;
	struct mtk_jdec_out_param	out;
	struct mtk_jdec_disp_param	disp;
};

#endif

