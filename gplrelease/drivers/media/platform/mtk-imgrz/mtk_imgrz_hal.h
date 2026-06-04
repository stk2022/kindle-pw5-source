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

#ifndef MTK_IMGRESZ_HAL_H
#define MTK_IMGRESZ_HAL_H

#define ROW_BUF_HEIGHT 4094u

void mtk_imgrz_hal_hw_init(void __iomem *base);
void mtk_imgrz_hal_hw_uninit(void __iomem *base);
int mtk_imgrz_hal_s_resz_mode(void __iomem *base,
			enum mtk_imgrz_scale_mode reszmode,
			enum mtk_imgrz_color_format format);
void mtk_imgrz_hal_s_resample_method(void __iomem *base,
			enum mtk_imgrz_resample_method h_method,
			enum mtk_imgrz_resample_method v_method);
int mtk_imgrz_hal_s_src_buf_fmt(void __iomem *base,
				const struct mtk_imgrz_buf_info *src);
void mtk_imgrz_hal_s_src_buf_addr(void __iomem *base, uint32_t buf_addr[]);
void mtk_imgrz_hal_s_rowbuf_height(void __iomem *base,
		uint32_t row_buf_hei, const struct mtk_imgrz_buf_info *src);
void mtk_imgrz_hal_s_pre_row_addr(void __iomem *base, uint32_t buf_addr[]);
void mtk_imgrz_hal_s_src_first_row(void __iomem *base, bool firstrow);
void mtk_imgrz_hal_s_src_last_row(void __iomem *base, bool lastrow);
void mtk_imgrz_hal_s_src_buf_pitch(void __iomem *base,
			enum mtk_imgrz_color_format fmt, u16 buf_width[]);
int mtk_imgrz_hal_s_src_pic_w_h(void __iomem *base, uint32_t width,
		uint32_t height, const struct mtk_imgrz_buf_info *src);
int mtk_imgrz_hal_s_src_pic_ofs(void __iomem *base, unsigned int x_offset,
		unsigned int y_offset, const struct mtk_imgrz_buf_info *src);
int mtk_imgrz_hal_s_dst_buf_fmt(void __iomem *base,
			       struct mtk_imgrz_buf_info *src,
			       struct mtk_imgrz_buf_info *dst);
void mtk_imgrz_hal_s_dst_buf_addr(void __iomem *base, uint32_t addr[]);
void mtk_imgrz_hal_s_dst_buf_pitch(void __iomem *base,
			enum mtk_imgrz_color_format fmt, u16 buf_width[]);
void mtk_imgrz_hal_s_dst_pic_w_h(void __iomem *base,
		uint32_t width, uint32_t height);
void mtk_imgrz_hal_s_dst_pic_ofs(void __iomem *base,
		unsigned int x_offset, unsigned int y_offset);
void mtk_imgrz_hal_s_yc_cbcr_swap(void __iomem *base, bool cbcr_swap);
int mtk_imgrz_hal_coeff_s_h_factor(void __iomem *base,
			const struct mtk_imgrz_scale_param *scale_param);
int mtk_imgrz_hal_coeff_s_v_factor(void __iomem *base,
			const struct mtk_imgrz_scale_param *scale_param);
void mtk_imgrz_hal_s_linebuflen_ext16(void __iomem *base, bool enable);
void mtk_imgrz_hal_ufo_linebuf_eco(void __iomem *base);
void mtk_imgrz_hal_s_sram_linebuflen(void __iomem *base, uint32_t linebuf);
void mtk_imgrz_hal_jpg_component(void __iomem *base,
		bool y_exist, bool cb_exist, bool cr_exist);
void mtk_imgrz_hal_s_cbcr_pad(void __iomem *base,
			bool cb_exist, bool cr_exist);
void mtk_imgrz_hal_ayuv_y_only(void __iomem *base, bool y_only);

void mtk_imgrz_hal_s_jpeg_pic_mode(void __iomem *base, bool pic_mode);

void mtk_imgrz_hal_s_tempbuf_addr(void __iomem *base, uint32_t addr);
void mtk_imgrz_hal_s_alpha_scale_type(void __iomem *base, unsigned int type);

void mtk_imgrz_hal_burst_enable(void __iomem *base,
		bool wr_enable, bool rd_enable);
void mtk_imgrz_hal_s_scaling_type(void __iomem *base, unsigned int type);
void mtk_imgrz_hal_print_reg(void __iomem *base);
void mtk_imgrz_hal_trigger_hw(void __iomem *base);
void mtk_imgrz_hal_clr_irq(void __iomem *base);
u32 mtk_imgrz_checksum_read(void __iomem *base);
u32 mtk_imgrz_checksum_write(void __iomem *base);

#endif
