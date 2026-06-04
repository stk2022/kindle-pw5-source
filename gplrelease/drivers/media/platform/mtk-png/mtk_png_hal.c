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
#include <linux/io.h>
#include <linux/module.h>

#include "mtk_png_common.h"
#include "mtk_png_parse.h"
#include "mtk_png_hal.h"
#include "mtk_png_reg.h"

static int debug;
module_param(debug, int, 0644);

#define png_writel(val, addr)						\
	do {								\
		if (debug & BIT(1))					\
			pr_info("RISCWrite('h%p, 32'h%x)\n", addr, val);\
		writel_relaxed(val, addr);				\
	} while (0)

#define png_writel_mb(val, addr)					\
	do {								\
		if (debug & BIT(1))					\
			pr_info("RISCWrite('h%p, 32'h%x)\n", addr, val);\
		writel(val, addr);					\
	} while (0)

static void mtk_png_hw_enable_idat_autoskip(void __iomem *base)
{
	png_writel(PNG_IDAT_AUTOSKIP_EN, base + PNG_IDAT_AUTOSKIP_REG);
	png_writel(PNG_IDAT_CLR_STATUS, base + PNG_IDAT_STATUS_CTRL_REG);
	png_writel(PNG_IDAT_EN_STATUS, base + PNG_IDAT_STATUS_CTRL_REG);

	png_writel(PNG_IBST_LAST_RESUME, base + PNG_IBST_LAST_RESUME_REG);
	png_writel(PNG_NOTLAST_IDAT_GRP, base + PNG_LAST_IDAT_GRP_REG);
	png_writel(PNG_LAST_SLICE, base + PNG_LAST_SLICE_REG);
}


static void mtk_png_hal_print_reg(void __iomem *base)
{
	int temp = 0;
	uint32_t dump_sz = 144;

	pr_info("ztm Begin to dump imgrz reg\n");

	for (temp = 0; temp < dump_sz; temp += 4) {
		pr_info("[%p]0x%8x   0x%8x   0x%8x   0x%8x\n",
			base + temp * 4,
			readl_relaxed(base + temp * 4),
			readl_relaxed(base + temp * 4 + 0x4),
			readl_relaxed(base + temp * 4 + 0x8),
			readl_relaxed(base + temp * 4 + 0xc));
	}
}

static void mtk_png_hw_fire_dec(struct mtk_png_dev *png)
{
	if (debug & BIT(1))
		mtk_png_hal_print_reg(png->base);

	png_writel_mb(0, png->base + PNG_BITSTRM_RESUME_REG);
	png_writel_mb(1, png->base + PNG_BITSTRM_RESUME_REG);
	png_writel_mb(0, png->base + PNG_BITSTRM_RESUME_REG);
	png_writel_mb(0, png->base + PNG_DECODE_START_REG);
	png_writel_mb(1, png->base + PNG_DECODE_START_REG);
	png_writel_mb(0, png->base + PNG_DECODE_START_REG);
}


static bool mtk_png_wait_dec_done(struct mtk_png_dev *png)
{
	if (wait_event_timeout(png->wait_hw_done_queue,
		png->state == PNG_STATE_DONE, MTK_PNG_HW_TIMEOUT))
		return true;

	png->state = PNG_STATE_TIMEOUT;
	return false;
}


void mtk_png_hw_clr_irq(enum mtk_png_irq_type irq, struct mtk_png_dev *png)
{
	unsigned int val = readl(png->base + PNG_TRANS_BG_REG);

	switch (irq) {
	case PNG_DEC_IRQ:
		png_writel(val | PNG_IRQ0_CLR, png->base + PNG_TRANS_BG_REG);
		png_writel_mb(val & (~PNG_IRQ0_CLR),
			      png->base + PNG_TRANS_BG_REG);
		break;
	case PNG_SRC_IRQ:
		png_writel(val | PNG_IRQ1_CLR, png->base + PNG_TRANS_BG_REG);
		png_writel_mb(val & (~PNG_IRQ1_CLR),
			      png->base + PNG_TRANS_BG_REG);
		break;
	case PNG_DST_IRQ:
		png_writel(val | PNG_IRQ1_CLR, png->base + PNG_TRANS_BG_REG);
		png_writel_mb(val & (~PNG_IRQ1_CLR),
			      png->base + PNG_TRANS_BG_REG);
		break;
	}
}


void mtk_png_hw_set_lz77_buf(void __iomem *base, u32 iova, unsigned int size)
{
	png_writel(iova >> 4, base + PNG_LZ77_STR_OWADDR_REG);
	png_writel((iova + size) >> 4, base + PNG_LZ77_END_OWADDR_REG);
}


void mtk_png_hw_set_line_buf(void __iomem *base, u32 iova, unsigned int size)
{
	png_writel(iova >> 4, base + PNG_LINE_BUF_STR_OWADDR_REG);
	png_writel((iova + size) >> 4, base + PNG_LINE_BUF_END_OWADDR_REG);
}


void mtk_png_hw_set_bs_fifo(void __iomem *base, u32 iova, unsigned int size)
{
	png_writel(iova >> 4, base + PNG_BS_FIFO_SRT_OWADDR_REG);
	png_writel(((iova + size) >> 4) - 1, base + PNG_BS_FIFO_END_OWADDR_REG);
}


void mtk_png_hw_set_src(void __iomem *base, struct mtk_png_ihdr_param *ihdr)
{
	u32 reg = ihdr->interlace_method
		| (ihdr->color_type << PNG_CLR_TYPE_SHIFT)
		| (ihdr->bit_depth << PNG_BIT_DEPTH_SHIFT);
	png_writel(reg, base + PNG_SRC_FORMAT_REG);

	reg = ((ihdr->pic_w - 1) << PNG_SRC_W_SHIFT) | (ihdr->pic_h - 1);
	png_writel(reg, base + PNG_SRC_WIDTH_HEIGHT_REG);
}


void mtk_png_hw_set_output(void __iomem *base,
			   u32 pixelformat, u32 bytesperline, u32 h)
{
	switch (pixelformat) {
	case V4L2_PIX_FMT_ARGB32:
		png_writel(PNG_OUT_FORMAT_ARGB8888,
			   base + PNG_OUTPUT_FORMAT_REG);
		png_writel((((bytesperline/4)-1) << PNG_REG_DISP_W_SHIFT)
			| (h - 1), base + PNG_DISP_WIDTH_SLICE_HEGIHT_REG);
		break;
	case V4L2_PIX_FMT_ARGB444:
		png_writel(PNG_OUT_FORMAT_ARGB4444,
			   base + PNG_OUTPUT_FORMAT_REG);
		png_writel((((bytesperline/2)-1) << PNG_REG_DISP_W_SHIFT)
			| (h - 1), base + PNG_DISP_WIDTH_SLICE_HEGIHT_REG);
		break;
	case V4L2_PIX_FMT_RGB565:
		png_writel(PNG_OUT_FORMAT_RGB565, base + PNG_OUTPUT_FORMAT_REG);
		png_writel((((bytesperline/2)-1) << PNG_REG_DISP_W_SHIFT)
			| (h - 1), base + PNG_DISP_WIDTH_SLICE_HEGIHT_REG);
		break;
	case V4L2_PIX_FMT_ARGB555:
		png_writel(PNG_OUT_FORMAT_ARGB1555,
			   base + PNG_OUTPUT_FORMAT_REG);
		png_writel((((bytesperline/2)-1) << PNG_REG_DISP_W_SHIFT)
			| (h - 1), base + PNG_DISP_WIDTH_SLICE_HEGIHT_REG);
		break;
	case V4L2_PIX_FMT_PAL8:
		png_writel(PNG_OUT_FORMAT_PALETTE,
			   base + PNG_OUTPUT_FORMAT_REG);
		png_writel(((bytesperline-1) << PNG_REG_DISP_W_SHIFT)
			| (h - 1), base + PNG_DISP_WIDTH_SLICE_HEGIHT_REG);
		break;
	default:
		break;

	}
}


void mtk_png_hw_set_crop(void __iomem *base,
		u32 crop_x, u32 crop_y, u32 crop_w, u32 crop_h)
{
	png_writel((crop_x << 16) | crop_y, base + PNG_CROP_ORG_XY_REG);
	png_writel(((crop_w-1) << 16) | (crop_h-1),
		base + PNG_CROP_WIDTH_HEIGHT_REG);
}


bool mtk_png_set_plte(struct mtk_png_dev *png, struct mtk_png_plte_param *plte,
	dma_addr_t iova)
{
	bool ret = false;

	if (!plte->exist)
		return true;

	png_writel(PNG_CHUNK_PLTE, png->base + PNG_CHUNK_TYPE_REG);
	png_writel(PNG_CHUNK_WR, png->base + PNG_CHUNK_TYPE_WR_REG);
	png_writel(PNG_READ_PLTE, png->base + PNG_READ_PLTE_CHUNK_REG);
	png_writel((plte->size/3)-1, png->base + PNG_PLTE_ENTRY_NUM_REG);
	png_writel(iova + plte->offset, png->base + PNG_BS_SA_REG);
	png_writel(PNG_SA_CHG, png->base + PNG_BS_SA_WR_REG);
	png_writel(plte->size, png->base + PNG_BS_LENGTH_REG);
	png->state = PNG_STATE_PARSE;
	mtk_png_hw_fire_dec(png);
	ret = mtk_png_wait_dec_done(png);
	png_writel(0, png->base + PNG_READ_PLTE_CHUNK_REG);

	return ret;
}


bool mtk_png_set_trns_for_plte(struct mtk_png_dev *png,
	struct mtk_png_trns_param *trns, dma_addr_t iova, void *va)
{
	bool ret = false;

	png_writel(PNG_CHUNK_tRNS, png->base + PNG_CHUNK_TYPE_REG);
	png_writel(PNG_CHUNK_WR, png->base + PNG_CHUNK_TYPE_WR_REG);
	png_writel(PNG_READ_TRNS, png->base + PNG_READ_TRNS_CHUNK_REG);
	png_writel(trns->size - 1, png->base + PNG_TRNS_ENTRY_NUM_REG);
	png_writel(iova + trns->offset, png->base + PNG_BS_SA_REG);
	png_writel(PNG_SA_CHG, png->base + PNG_BS_SA_WR_REG);
	png_writel(trns->size, png->base + PNG_BS_LENGTH_REG);
	png->state = PNG_STATE_PARSE;
	mtk_png_hw_fire_dec(png);
	ret = mtk_png_wait_dec_done(png);
	png_writel(0, png->base + PNG_READ_TRNS_CHUNK_REG);

	return ret;
}


bool mtk_png_set_trns(struct mtk_png_dev *png, struct mtk_png_trns_param *trns,
	dma_addr_t iova, void *va)
{
	void __iomem *base = png->base;
	struct mtk_png_dec_param *dec_param;
	u8 *trans_data;
	u16 r, g, b, gray;
	u32 trans_ctrl;

	if (!trns->exist)
		return true;

	dec_param = container_of(trns, struct mtk_png_dec_param, trns);
	if (dec_param->plte.exist)
		return mtk_png_set_trns_for_plte(png, trns, iova, va);

	trans_data = (u8 *)va;

	if (dec_param->ihdr.color_type == 0 ||
	    dec_param->ihdr.color_type == 4) {
		gray = (trans_data[0] << 8) + trans_data[1];
		png_writel(gray << PNG_TRNS_GRAY_SHIFT,
			   base + PNG_TRNS_KEY0_REG);
		png_writel(0, base + PNG_TRNS_KEY1_REG);
	} else {
		r = (trans_data[0] << 8) + trans_data[1];
		g = (trans_data[2] << 8) + trans_data[3];
		b = (trans_data[4] << 8) + trans_data[5];
		png_writel(r << PNG_TRNS_R_SHIFT, base + PNG_TRNS_KEY0_REG);
		png_writel((g << PNG_TRNS_G_SHIFT) + (b << PNG_TRNS_B_SHIFT),
			base + PNG_TRNS_KEY0_REG);
	}

	trans_ctrl = PNG_TRNS_EN | PNG_TRNS_OUT;
	if (dec_param->ihdr.bit_depth == 16)
		trans_ctrl |= PNG_TRNS_MATCH_16BIT;
	png_writel(trans_ctrl, base + PNG_TRANS_CTRL_REG);
	png_writel(0, base + PNG_ALPHA_MATCHED_PIXEL_REG);

	return true;
}


bool mtk_png_dec_idat(struct mtk_png_dev *png, struct mtk_png_idat_param *idat,
	dma_addr_t src, dma_addr_t dst, unsigned int length)
{
	void __iomem *base = png->base;

	mtk_png_hw_enable_idat_autoskip(base);
	png_writel(src + idat->first_offset, base + PNG_BS_SA_REG);
	png_writel(PNG_SA_CHG, png->base + PNG_BS_SA_WR_REG);
	png_writel(idat->size, png->base + PNG_BS_LENGTH_REG);
	png_writel(PNG_CHUNK_IDAT, png->base + PNG_CHUNK_TYPE_REG);
	png_writel(PNG_CHUNK_WR, png->base + PNG_CHUNK_TYPE_WR_REG);
	png_writel(dst, base + PNG_PIXEL_OUT_ADDR_REG);
	png_writel(dst >> 4, base + PNG_PIXEL_OUT_SRT_OWADDR_REG);
	png_writel((dst + length) >> 4, base + PNG_PIXEL_OUT_END_OWADDR_REG);
	png->state = PNG_STATE_DEC;
	mtk_png_hw_fire_dec(png);

	return true;
}



bool mtk_png_hw_init(struct mtk_png_dev *png)
{
	void __iomem *base = png->base;

	png_writel_mb(PNG_ENABLE | PNG_BURST_WRITE_EN, base + PNG_ENABLE_REG);
	png_writel_mb(PNG_REG_SRAM_ON, base + PNG_SRAM_CHIP_SELECT_REG);
	png_writel_mb(PNG_RESET_START, base + PNG_RESET_REG);
	png_writel_mb(PNG_RESET_END, base + PNG_RESET_REG);

	png_writel_mb(PNG_PELOUT_WDLE_EN1, base + PNG_PELOUT_WDLE_EN_REG);
	png_writel(0, base + PNG_IBST_LAST_RESUME_REG);
	png_writel(0, base + PNG_TRANS_CTRL_REG);
	png_writel(0xff, base + PNG_ALPHA_UNMATCHED_PIXEL_REG);

	png_writel(PNG_NOTLAST_SLICE, base + PNG_LAST_SLICE_REG);
	png_writel(0, base + PNG_IDAT_AUTOSKIP_REG);

	return true;
}

