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

#ifndef MTK_PNG_HAL_H
#define MTK_PNG_HAL_H

#define MTK_PNG_HW_TIMEOUT	msecs_to_jiffies(1000)

bool mtk_png_hw_init(struct mtk_png_dev *png);
void mtk_png_hw_clr_irq(enum mtk_png_irq_type irq, struct mtk_png_dev *png);
void mtk_png_hw_set_lz77_buf(void __iomem *base, u32 iova, unsigned int size);
void mtk_png_hw_set_line_buf(void __iomem *base, u32 iova, unsigned int size);
void mtk_png_hw_set_bs_fifo(void __iomem *base, u32 iova, unsigned int size);
void mtk_png_hw_set_src(void __iomem *base, struct mtk_png_ihdr_param *ihdr);
void mtk_png_hw_set_output(void __iomem *base,
			   u32 pixelformat, u32 bytesperline, u32 h);
void mtk_png_hw_set_crop(void __iomem *base,
		u32 crop_x, u32 crop_y, u32 crop_w, u32 crop_h);
bool mtk_png_set_plte(struct mtk_png_dev *png, struct mtk_png_plte_param *plte,
	dma_addr_t iova);
bool mtk_png_set_trns_for_plte(struct mtk_png_dev *png,
	struct mtk_png_trns_param *trns, dma_addr_t iova, void *va);
bool mtk_png_set_trns(struct mtk_png_dev *png, struct mtk_png_trns_param *trns,
	dma_addr_t iova, void *va);
bool mtk_png_dec_idat(struct mtk_png_dev *png, struct mtk_png_idat_param *idat,
	dma_addr_t src, dma_addr_t dst, unsigned int length);

#endif
