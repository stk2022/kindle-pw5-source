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

#include <linux/err.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/iopoll.h>

#include "mtk_imgrz_common.h"
#include "mtk_imgrz_reg.h"

#define MTK_IMGRZ_LOG_SHOWREG	BIT(0)
#define MTK_IMGRZ_LOG_DONEREG	BIT(1)

enum MTK_IMGRZ_SCALE_FACTOR {
	FACTOR_0 = 0,
	FACTOR_0_0625,
	FACTOR_0_125,
	FACTOR_0_25,
	FACTOR_0_5,
	FACTOR_1,
	FACTOR_RM,
	FACTOR_MAX
};

static unsigned int h_filtercoeff[FACTOR_MAX][18] = {
	/* FACTOR_0 */
	{0x40202020, 0x20202020, 0x20202020, 0x20202020, 0x20202020,
	 0x20202020, 0x20202020, 0x20202020, 0x20202020, 0x20202020,
	 0x20202020, 0x20202020, 0x20202020, 0x20202020, 0x20202020,
	 0x20202020, 0x00000000, 0x00000000},
	/* FACTOR_0_0625 */
	{0x3f21201f, 0x2121201f, 0x2121201f, 0x2121201f, 0x2121201f,
	 0x2121201f, 0x2121201f, 0x2121201e, 0x2121201e, 0x2121201e,
	 0x2021201e, 0x2021201e, 0x21211f1e, 0x22201f1e, 0x22201f1e,
	 0x22201f1e, 0x00000000, 0x00000000},
	/* FACTOR_0_125 */
	{0x3c24211d, 0x2524211c, 0x2524211c, 0x2524201b, 0x2524201b,
	 0x2523201b, 0x25231f1a, 0x25231f1a, 0x24231f1a, 0x24231f19,
	 0x26221e19, 0x25221e18, 0x24221e18, 0x25221d18, 0x24221d17,
	 0x25211d17, 0x00000000, 0x00000000},
	/* FACTOR_0_25 */
	{0x3833220f, 0x3831210e, 0x3830200d, 0x382f1f0c, 0x382f1d0b,
	 0x372e1c0a, 0x372d1b09, 0x372c1a08, 0x362b1807, 0x342a1706,
	 0x34291605, 0x34281504, 0x33271403, 0x34251302, 0x34241102,
	 0x34231001, 0x00000000, 0x00000000},
	/* FACTOR_0_5 */
	{0x845000ee, 0x844bfdef, 0x8345faf0, 0x8340f7f1, 0x813af5f2,
	 0x7f35f3f3, 0x7d2ff1f4, 0x7a29eff6, 0x7724eef7, 0x741fedf8,
	 0x6e1aedfa, 0x6915edfb, 0x6610ecfc, 0x5f0cedfd, 0x5c07edfe,
	 0x5404eeff, 0x33333331, 0x33333333},
	/* FACTOR_1 */
	{0x00000000, 0xfef40300, 0xf9ea0500, 0xf0e30700, 0xe5de0800,
	 0xd6db0800, 0xc5da0800, 0xb1db0700, 0x9ddd0600, 0x87e10500,
	 0x70e50400, 0x5aea0300, 0x44ef0200, 0x31f40100, 0x20f80000,
	 0x0ffc0000, 0x44444448, 0x44444444},
	/* FACTOR_RM */
	{0x00000000, 0xf0000000, 0xe0000000, 0xd0000000, 0xc0000000,
	 0xb0000000, 0xa0000000, 0x90000000, 0x80000000, 0x70000000,
	 0x60000000, 0x50000000, 0x40000000, 0x30000000, 0x20000000,
	 0x10000000, 0x00000008, 0x00000000}
};

static unsigned int v_filtercoeff[FACTOR_MAX][9] = {
	/* FACTOR_0 */
	{0x40408040, 0x40404040, 0x40404040, 0x40404040, 0x40404040,
	 0x40404040, 0x40404040, 0x40404040, 0x00000000},
	/* FACTOR_0_0625 */
	{0x41408040, 0x41404140, 0x41404140, 0x40404140, 0x413f4040,
	 0x403f403f, 0x403f403f, 0x403f403f, 0x00000000},
	/* FACTOR_0_125 */
	{0x43417e41, 0x42404340, 0x423f4240, 0x423e423f, 0x423e423e,
	 0x423d423d, 0x423c413d, 0x403c413c, 0x00000000},
	/* FACTOR_0_25 */
	{0x4b427a43, 0x4a3f4b40, 0x493c4a3e, 0x483a493b, 0x47374838,
	 0x47344636, 0x45324533, 0x442f4431, 0x00000000},
	/* FACTOR_0_5 */
	{0x72417446, 0x6f36703b, 0x6a2c6c31, 0x65226827, 0x6019621e,
	 0x59115c15, 0x510a560d, 0x4a034f06, 0x00000000},
	/* FACTOR_1 */
	{0x01f40000, 0xf8e2ffea, 0xdfd9eddd, 0xbad9ced8, 0x8edfa5db,
	 0x5fe976e4, 0x33f348ee, 0x0ffc1ff8, 0x00000005},
	/* FACTOR_RM */
	{0xf0000000, 0xd000e000, 0xb000c000, 0x9000a000, 0x70008000,
	 0x50006000, 0x30004000, 0x10002000, 0x00000001}
};

static int debug;
module_param(debug, int, 0644);

static void mtk_imgrz_hal_hw_reset(void __iomem *base)
{
	u32 reg;

	reg = readl_relaxed(base + RW_IMGRZ_START);
	reg |= IMGRZ_SW_RESET_ON;
	writel_relaxed(reg, base + RW_IMGRZ_START);

	reg |= IMGRZ_REGISTER_RESET_ON;
	writel_relaxed(reg, base + RW_IMGRZ_START);

	reg &= ~IMGRZ_REGISTER_RESET_ON;
	writel_relaxed(reg, base + RW_IMGRZ_START);
	reg &= ~(IMGRZ_SW_RESET_ON | IMGRZ_MMU_RESET_ON);
	reg &= ~IMGRZ_DMA_SW_RST;
	writel_relaxed(reg, base + RW_IMGRZ_START);
}

static void mtk_imgrz_hal_hw_enable(void __iomem *base)
{
	u32 reg;

	reg = readl_relaxed(base + RW_IMGRZ_START);
	reg |= IMGRZ_ENABLE + IMGRZ_CHK_SUM_CLR;
	writel_relaxed(reg, base + RW_IMGRZ_START);
}

static void mtk_imgrz_hal_hw_disable(void __iomem *base)
{
	u32 reg;

	reg = readl_relaxed(base + RW_IMGRZ_START);
	reg &= ~IMGRZ_ENABLE;
	writel_relaxed(reg, base + RW_IMGRZ_START);
}

static void mtk_imgrz_hal_interrupt_enable(void __iomem *base)
{
	u32 reg;

	reg = readl_relaxed(base + RW_IMGRZ_START);
	reg |= IMGRZ_INT_ON;
	writel_relaxed(reg, base + RW_IMGRZ_START);
}

static void mtk_imgrz_hal_waitdone_enable(void __iomem *base)
{
	u32 reg;

	reg = readl_relaxed(base + RW_IMGRZ_START);
	reg |= IMGRZ_WAIT_WR_DONE;
	writel_relaxed(reg, base + RW_IMGRZ_START);
}

static void mtk_imgrz_hal_ultra_onoff(void __iomem *base, bool onoff)
{
	u32 reg;

	reg = readl_relaxed(base + RW_IMGRZ_ULTRA_LIMIT);
	reg |= IMGRZ_ULTRA_OFF;

	if (onoff)
		reg &= ~IMGRZ_ULTRA_OFF;

	writel_relaxed(reg, base + RW_IMGRZ_ULTRA_LIMIT);
}

static void mtk_imgrz_hal_set_dram_burstlimit(void __iomem *base, int limit)
{
	u32 reg;

	reg = readl_relaxed(base + RW_IMGRZ_MEM_IF_MODE);

	reg &= ~(IMGRZ_DRAM_BURST_LIMIT_1 | IMGRZ_DRAM_BURST_LIMIT_2 |
		IMGRZ_DRAM_BURST_LIMIT_4 | IMGRZ_DRAM_BURST_LIMIT_8 |
		IMGRZ_DRAM_BURST_LIMIT_16);

	reg |= ((limit & 0x1f) << 8);

	writel_relaxed(reg, base + RW_IMGRZ_MEM_IF_MODE);
}

void mtk_imgrz_hal_s_wr_gmc_burstlimit(void __iomem *base, int limit)
{
	u32 reg;

	reg = readl_relaxed(base + RW_IMGRZ_VENC_SKIP);
	reg &= IMGRZ_WR_BURST_LIMIT_CLEAR;
	reg |= (limit << IMGRZ_WR_BURST_SHIFT);
	writel_relaxed(reg, base + RW_IMGRZ_VENC_SKIP);
}

void mtk_imgrz_hal_s_rd_gmc_burstlimit(void __iomem *base, int limit)
{
	u32 reg;

	reg = readl_relaxed(base + RW_IMGRZ_VENC_SKIP);
	reg &= IMGRZ_RD_BURST_LIMIT_CLEAR;
	reg |= (limit << IMGRZ_RD_BURST_SHIFT);
	writel_relaxed(reg, base + RW_IMGRZ_VENC_SKIP);
}

void mtk_imgrz_hal_hw_init(void __iomem *base)
{
	mtk_imgrz_hal_hw_enable(base);
	mtk_imgrz_hal_hw_reset(base);
	mtk_imgrz_hal_set_dram_burstlimit(base, 16);
	mtk_imgrz_hal_interrupt_enable(base);
	mtk_imgrz_hal_waitdone_enable(base);
	mtk_imgrz_hal_ultra_onoff(base, false);
}

void mtk_imgrz_hal_hw_uninit(void __iomem *base)
{
	mtk_imgrz_hal_hw_reset(base);
	mtk_imgrz_hal_hw_disable(base);
}

/* Help debug, Print the register. */
void mtk_imgrz_hal_print_reg(void __iomem *base)
{
	int temp = 0;
	uint32_t dump_sz = 144;

	pr_info("Begin to dump imgrz reg\n");
	/* UFO need more reg. */
	if (readl_relaxed(base + RW_IMGRZ_UFO_POWER) & IMGRZ_UFO_ON)
		dump_sz += 13 * 4;

	for (temp = 0; temp < dump_sz; temp += 4) {
		pr_info("[%p]0x%8x   0x%8x   0x%8x   0x%8x\n",
			base + temp * 4,
			readl_relaxed(base + temp * 4),
			readl_relaxed(base + temp * 4 + 0x4),
			readl_relaxed(base + temp * 4 + 0x8),
			readl_relaxed(base + temp * 4 + 0xc));
	}
}

void mtk_imgrz_hal_trigger_hw(void __iomem *base)
{
	u32 reg;

	if (debug & MTK_IMGRZ_LOG_SHOWREG) {
		pr_info("dump imgrz reg before trigger\n");
		mtk_imgrz_hal_print_reg(base);
	}

	reg = readl_relaxed(base + RW_IMGRZ_START);
	reg |= IMGRZ_ACTIVATE;
	writel(reg, base + RW_IMGRZ_START);

	/* Trigger UFO if current is UFO
	 *if (ufo)
	 *	imgresz_ufo_trigger(base);
	 */
}

void mtk_imgrz_hal_clr_irq(void __iomem *base)
{
	u32 reg;

	reg = readl_relaxed(base + RW_IMGRZ_START);
	reg |= IMGRZ_IRQ_CLEAR;
	writel(reg, base + RW_IMGRZ_START);

	if (debug & MTK_IMGRZ_LOG_DONEREG) {
		pr_info("dump imgrz reg after done\n");
		mtk_imgrz_hal_print_reg(base);
	}
}

int mtk_imgrz_hal_s_resz_mode(void __iomem *base,
			enum mtk_imgrz_scale_mode reszmode,
			enum mtk_imgrz_color_format format)
{
	u32 reg;

	reg = readl_relaxed(base + RW_IMGRZ_TYPE);

	switch (format) {
	case MTK_IMGRZ_COLOR_FORMAT_Y_C:
		reg &= ~IMGRZ_JPEG_MODE;
		reg &= ~IMGRZ_OSD_PARTIAL_MODE;
		break;
	case MTK_IMGRZ_COLOR_FORMAT_Y_CB_CR:
		reg |= IMGRZ_JPEG_MODE;
		reg &= ~IMGRZ_OSD_PARTIAL_MODE;
		break;
	case MTK_IMGRZ_COLOR_FORMAT_INDEX:
	case MTK_IMGRZ_COLOR_FORMAT_ARGB:
	case MTK_IMGRZ_COLOR_FORMAT_AYUV:
	case MTK_IMGRZ_COLOR_FORMAT_GREY:
		reg &= ~IMGRZ_JPEG_MODE;
		if (reszmode == MTK_IMGRZ_PARTIAL_SCALE)
			reg |= IMGRZ_OSD_PARTIAL_MODE;
		else
			reg &= ~IMGRZ_OSD_PARTIAL_MODE;
		break;
	default:
		return -EINVAL;
	}

	writel_relaxed(reg, base + RW_IMGRZ_TYPE);

	return 0;
}

void mtk_imgrz_hal_s_resample_method(void __iomem *base,
			enum mtk_imgrz_resample_method h_method,
			enum mtk_imgrz_resample_method v_method)
{
	u32 reg;

	reg = readl_relaxed(base + RW_IMGRZ_TYPE);
	if (v_method == MTK_IMGRZ_4_TAP_RESAMPLE)
		reg |= IMGRZ_V_4_TAP_FILTER;
	else if (v_method == MTK_IMGRZ_M_TAP_RESAMPLE)
		reg &= ~IMGRZ_V_4_TAP_FILTER;

	writel_relaxed(reg, base + RW_IMGRZ_TYPE);
}

void mtk_imgrz_hal_burst_enable(void __iomem *base,
		bool wr_enable, bool rd_enable)
{
	u32 reg;

	reg = readl_relaxed(base + RW_IMGRZ_START);
	reg &= ~IMGRZ_RD_BURST_ON;
	reg &= ~IMGRZ_WR_BURST_ON;
	reg |= IMGRZ_WR_BST_NCROSS;
	if (rd_enable)
		reg |= IMGRZ_RD_BURST_ON;
	if (wr_enable)
		reg |= IMGRZ_WR_BURST_ON;
	writel_relaxed(reg, base + RW_IMGRZ_START);

	mtk_imgrz_hal_s_wr_gmc_burstlimit(base, wr_enable?8:1);
	mtk_imgrz_hal_s_rd_gmc_burstlimit(base, rd_enable?8:1);
}

void mtk_imgrz_hal_s_src_buf_addr(void __iomem *base, uint32_t buf_addr[])
{
	writel_relaxed(buf_addr[0] >> 4, base + RW_IMGRZ_SRC_Y_ADDR_BASE1);
	writel_relaxed(buf_addr[1] >> 4, base + RW_IMGRZ_SRC_CB_ADDR_BASE1);
	writel_relaxed(buf_addr[2] >> 4, base + RW_IMGRZ_SRC_CR_ADDR_BASE1);
}

void mtk_imgrz_hal_s_pre_row_addr(void __iomem *base, uint32_t buf_addr[])
{
	writel_relaxed(buf_addr[0] >> 4, base + RW_IMGRZ_SRC_Y_ADDR_BASE2);
	writel_relaxed(buf_addr[1] >> 4, base + RW_IMGRZ_SRC_CB_ADDR_BASE2);
	writel_relaxed(buf_addr[2] >> 4, base + RW_IMGRZ_SRC_CR_ADDR_BASE2);

}

int mtk_imgrz_hal_s_src_buf_fmt(void __iomem *base,
				const struct mtk_imgrz_buf_info *src)
{
	u32 resz_type, osd_setting, reg;

	resz_type = readl_relaxed(base + RW_IMGRZ_TYPE);
	osd_setting = readl_relaxed(base + RW_IMGRZ_OSD_MODE_SETTING);

	/* If not clear, OSD mode will incorrect.*/
	resz_type &= ~IMGRZ_FIELD;

	switch (src->fmt) {
	case MTK_IMGRZ_COLOR_FORMAT_Y_C:
		resz_type &= ~IMGRZ_SEL_OSD_MODE;
		if (src->progressive) {
			resz_type &= ~IMGRZ_FIELD;
		} else {/* Interlace */
			resz_type |= IMGRZ_FIELD;
			if (src->top_field)
				resz_type |= IMGRZ_INTERLACE_TOP_FIELD;
			else
				resz_type &= ~IMGRZ_INTERLACE_TOP_FIELD;
		}
		if (src->block)
			resz_type &= ~IMGRZ_RASTER_SCAN_IN;
		else
			resz_type |= IMGRZ_RASTER_SCAN_IN;
		break;
	case MTK_IMGRZ_COLOR_FORMAT_Y_CB_CR:
	case MTK_IMGRZ_COLOR_FORMAT_GREY:
		resz_type &= ~IMGRZ_SEL_OSD_MODE;
		/* Set Sample Factor */
		reg = readl_relaxed(base + RW_IMGRZ_JPG_MODE);
		reg &= 0xFFFFF000;
		reg |= (((src->h_sample[0] - 1) & 3) << 10);
		reg |= (((src->v_sample[0] - 1) & 3) << 8);
		reg |= (((src->h_sample[1] - 1) & 3) << 6);
		reg |= (((src->v_sample[1] - 1) & 3) << 4);
		reg |= (((src->h_sample[2] - 1) & 3) << 2);
		reg |= (((src->v_sample[2] - 1) & 3) << 0);
		writel_relaxed(reg, base + RW_IMGRZ_JPG_MODE);
		break;

	case MTK_IMGRZ_COLOR_FORMAT_ARGB:
	case MTK_IMGRZ_COLOR_FORMAT_AYUV:
		resz_type |= IMGRZ_SEL_OSD_MODE;
		osd_setting |= IMGRZ_OSD_DIRECT_MODE;

		osd_setting &= ~(IMGRZ_OSD_DIRECT_RGB565 |
				IMGRZ_OSD_DIRECT_ARGB1555 |
				IMGRZ_OSD_DIRECT_ARGB4444 |
				IMGRZ_OSD_DIRECT_ARGB8888);
		switch (src->f.argb_fmt) {
		case MTK_IMGRZ_ARGB_FORMAT_0565:
			osd_setting |= IMGRZ_OSD_DIRECT_RGB565;
			break;
		case MTK_IMGRZ_ARGB_FORMAT_1555:
			osd_setting |= IMGRZ_OSD_DIRECT_ARGB1555;
			break;
		case MTK_IMGRZ_ARGB_FORMAT_4444:
			osd_setting |= IMGRZ_OSD_DIRECT_ARGB4444;
			break;
		case MTK_IMGRZ_ARGB_FORMAT_8888:
			osd_setting |= IMGRZ_OSD_DIRECT_ARGB8888;
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	writel_relaxed(resz_type, base + RW_IMGRZ_TYPE);
	writel_relaxed(osd_setting, base + RW_IMGRZ_OSD_MODE_SETTING);

	return 0;
}

void mtk_imgrz_hal_s_src_buf_pitch(void __iomem *base,
		enum mtk_imgrz_color_format fmt, u16 buf_width[])
{
	u32 buf_width_c = (fmt == MTK_IMGRZ_COLOR_FORMAT_AYUV) ?
			buf_width[0] : buf_width[1];

	writel_relaxed((buf_width[0] >> 4) | ((buf_width_c >> 4) << 12),
			base + RW_IMGRZ_SRC_BUF_LEN);
}

/* only needed by imgrz osd partial mode && jdec mcu row mode */
void mtk_imgrz_hal_s_rowbuf_height(void __iomem *base,
		uint32_t row_buf_hei, const struct mtk_imgrz_buf_info *src)
{
	u32 reg;
	u32 row_hei_y = 0, row_hei_cb = 0, row_hei_cr = 0;

	if (row_buf_hei == 0) {
		reg = readl_relaxed(base + RW_IMGRZ_JPG_MODE);
		reg &= ~IMGRZ_LINES_ASSIGNED_DIRECTLY;
		writel_relaxed(reg, base + RW_IMGRZ_JPG_MODE);
	} else {
		reg = readl_relaxed(base + RW_IMGRZ_JPG_MODE);
		reg |= IMGRZ_LINES_ASSIGNED_DIRECTLY;
		writel_relaxed(reg, base + RW_IMGRZ_JPG_MODE);

		row_hei_y = row_buf_hei;

		if (src->fmt == MTK_IMGRZ_COLOR_FORMAT_Y_CB_CR) {
			row_hei_cb = row_buf_hei *
				src->v_sample[1] / src->v_sample[0];
			row_hei_cr = row_buf_hei *
				src->v_sample[2] / src->v_sample[0];
		}
		writel_relaxed(row_hei_y, base + RW_IMGRZ_LINE_NUM_Y);
		writel_relaxed(row_hei_cb, base + RW_IMGRZ_LINE_NUM_CB);
		writel_relaxed(row_hei_cr, base + RW_IMGRZ_LINE_NUM_CR);
	}
}

int mtk_imgrz_hal_s_src_pic_w_h(void __iomem *base, uint32_t width,
		uint32_t height, const struct mtk_imgrz_buf_info *src)
{
	unsigned int src_w, src_h;

	switch (src->fmt) {
	case MTK_IMGRZ_COLOR_FORMAT_Y_C:
		writel_relaxed(((width & 0xFFFF) << 16) | (height & 0xFFFF),
			       base + RW_IMGRZ_SRC_SIZE_Y);
		switch (src->f.yc_fmt) {
		case MTK_IMGRZ_YC_FORMAT_420:
			writel_relaxed((((width / 2) & 0xFFFF) << 16) |
					((height / 2) & 0xFFFF),
					base + RW_IMGRZ_SRC_SIZE_CB);
			break;
		case MTK_IMGRZ_YC_FORMAT_422:
			writel_relaxed((((width / 2) & 0xFFFF) << 16) |
					(height & 0xFFFF),
					base + RW_IMGRZ_SRC_SIZE_CB);
			break;
		default:
			break;
		}
		break;
	case MTK_IMGRZ_COLOR_FORMAT_Y_CB_CR:
		writel_relaxed(((width & 0xFFFF) << 16) | (height & 0xFFFF),
			       base + RW_IMGRZ_SRC_SIZE_Y);

		if ((src->h_sample[1] != 0) && (src->v_sample[1] != 0)) {
			src_w = width * src->h_sample[1] / src->h_sample[0];
			src_h = height * src->v_sample[1] / src->v_sample[0];
			/* For jpeg picture mode, prevent source height 401
			 * come two interrupt (Y interrupt and C interrupt)
			 */
			if ((src_h * src->v_sample[0]) !=
			    (height * src->v_sample[1]))
				src_h++;
			writel_relaxed(((src_w & 0xFFFF) << 16) |
					(src_h & 0xFFFF),
					base + RW_IMGRZ_SRC_SIZE_CB);
		}

		if ((src->h_sample[2] != 0) && (src->v_sample[2] != 0)) {
			src_w = width * src->h_sample[2] / src->h_sample[0];
			src_h = height * src->v_sample[2] / src->v_sample[0];
			/* For jpeg picture mode, prevent source height 401
			 * come two interrupt (Y interrupt and C interrupt)
			 */
			if ((src_h * src->v_sample[0]) !=
			    (height * src->v_sample[2]))
				src_h++;
			writel_relaxed(((src_w & 0xFFFF) << 16) |
					(src_h & 0xFFFF),
					base + RW_IMGRZ_SRC_SIZE_CR);
		}
		break;
	case MTK_IMGRZ_COLOR_FORMAT_ARGB:
	case MTK_IMGRZ_COLOR_FORMAT_AYUV:
	case MTK_IMGRZ_COLOR_FORMAT_INDEX:
	case MTK_IMGRZ_COLOR_FORMAT_GREY:
		writel_relaxed(((width & 0xFFFF) << 16) | (height & 0xFFFF),
			       base + RW_IMGRZ_SRC_SIZE_Y);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

int mtk_imgrz_hal_s_src_pic_ofs(void __iomem *base, unsigned int x_offset,
		unsigned int y_offset, const struct mtk_imgrz_buf_info *src)
{
	unsigned int x_off1 = 0, y_off1 = 0;
	unsigned int x_off2 = 0, y_off2 = 0;
	unsigned int x_off3 = 0, y_off3 = 0;
	unsigned int h_sample, v_sample;

	x_off1 = x_offset;
	y_off1 = y_offset;

	switch (src->fmt) {
	case MTK_IMGRZ_COLOR_FORMAT_Y_C:
		switch (src->f.yc_fmt) {
		case MTK_IMGRZ_YC_FORMAT_420:
			x_off2 = x_offset >> 1;
			y_off2 = y_offset >> 1;
			break;
		case MTK_IMGRZ_YC_FORMAT_422:
			x_off2 = x_offset >> 1;
			y_off2 = y_offset;
			break;
		default:
			break;
		}
		break;
	case MTK_IMGRZ_COLOR_FORMAT_Y_CB_CR:
		if ((src->h_sample[1] != 0) && (src->v_sample[1] != 0)) {
			h_sample = src->h_sample[0] / src->h_sample[1];
			v_sample = src->v_sample[0] / src->v_sample[1];
			x_off2 = x_offset / h_sample;
			y_off2 = y_offset / v_sample;
		} else {
			x_off2 = 0;
			y_off2 = 0;
		}

		if ((src->h_sample[2] != 0) && (src->v_sample[2] != 0)) {
			h_sample = src->h_sample[0] / src->h_sample[2];
			v_sample = src->v_sample[0] / src->v_sample[2];
			x_off3 = x_offset / h_sample;
			y_off3 = y_offset / v_sample;
		} else {
			x_off3 = 0;
			y_off3 = 0;
		}
		break;
	default:
		break;
	}
	writel_relaxed(((x_off1 & 0xFFFF) << 16) | (y_off1 & 0xFFFF),
			base + RW_IMGRZ_SRC_OFFSET_Y);
	writel_relaxed(((x_off2 & 0xFFFF) << 16) | (y_off2 & 0xFFFF),
			base + RW_IMGRZ_SRC_OFFSET_CB);
	writel_relaxed(((x_off3 & 0xFFFF) << 16) | (y_off3 & 0xFFFF),
			base + RW_IMGRZ_SRC_OFFSET_CR);

	return 0;
}

void mtk_imgrz_hw_set_rm_rpr(void __iomem *base, bool rpr_mode, bool rpr_racing)
{
	u32 reg;

	reg = readl_relaxed(base + RW_IMGRZ_RPR);
	if (rpr_mode)
		reg |= IMGRZ_RPR_FLAG_ON;
	else
		reg &= ~IMGRZ_RPR_FLAG_ON;

	if (rpr_racing) {
		reg &= ~(0x1 << 10);
		reg |= IMGRZ_TRC_VDEC_EN;
		reg |= IMGRZ_TRC_VDEC_INT;
	} else {
		reg &= ~IMGRZ_TRC_VDEC_EN;
		reg &= ~IMGRZ_TRC_VDEC_INT;
	}
	writel_relaxed(reg, base + RW_IMGRZ_RPR);
}

void mtk_imgrz_hal_s_src_first_row(void __iomem *base, bool firstrow)
{
	u32 reg;

	reg = readl_relaxed(base + RW_IMGRZ_JPG_MODE);
	if (firstrow)
		reg |= IMGRZ_FIRST_BLOCK_LINE;
	else
		reg &= ~IMGRZ_FIRST_BLOCK_LINE;
	writel_relaxed(reg, base + RW_IMGRZ_JPG_MODE);
}

void mtk_imgrz_hal_s_src_last_row(void __iomem *base, bool lastrow)
{
	u32 reg;

	reg = readl_relaxed(base + RW_IMGRZ_JPG_MODE);
	if (lastrow)
		reg |= IMGRZ_LAST_BLOCK_LINE;
	else
		reg &= ~IMGRZ_LAST_BLOCK_LINE;
	writel_relaxed(reg, base + RW_IMGRZ_JPG_MODE);
}

void mtk_imgrz_hal_s_yc_cbcr_swap(void __iomem *base, bool cbcr_swap)
{
	u32 reg;

	reg = readl_relaxed(base + RW_IMGRZ_TYPE);
	if (cbcr_swap)
		reg |= IMGRZ_CBCRSWAP;
	else
		reg &= ~IMGRZ_CBCRSWAP;
	writel_relaxed(reg, base + RW_IMGRZ_TYPE);
}

int mtk_imgrz_hal_s_dst_buf_fmt(void __iomem *base,
			       struct mtk_imgrz_buf_info *src,
			       struct mtk_imgrz_buf_info *dst)
{
	u32 reg, osd_setting, addr_swap_setting, csc_setting;

	reg = readl_relaxed(base + RW_IMGRZ_TYPE);
	osd_setting = readl_relaxed(base + RW_IMGRZ_OSD_MODE_SETTING);
	addr_swap_setting = readl_relaxed(base + RW_IMGRZ_MEM_IF_MODE);
	csc_setting = readl_relaxed(base + RW_IMGRZ_OSD_CSC_SETTING);

	reg &= ~IMGRZ_V2OSD; /* If not clear, OSD mode will incorrect. */
	csc_setting &= ~IMGRZ_OSD_CSC_ENABLE;

	switch (dst->fmt) {
	case MTK_IMGRZ_COLOR_FORMAT_Y_C:
	case MTK_IMGRZ_COLOR_FORMAT_GREY:
		/* Set destination buffer YUV format */
		reg &= ~(IMGRZ_420_OUT | IMGRZ_422_OUT | IMGRZ_444_OUT);
		switch (dst->f.yc_fmt) {
		case MTK_IMGRZ_YC_FORMAT_420:
			reg |= IMGRZ_420_OUT;
			break;
		case MTK_IMGRZ_YC_FORMAT_422:
			reg |= IMGRZ_422_OUT;
			break;
		case MTK_IMGRZ_YC_FORMAT_444:
			reg |= IMGRZ_444_OUT;
			break;
		default:
			reg |= IMGRZ_420_OUT;
			break;
		}
		/* Set destination buffer Raster Scan/Block Mode format */
		if (dst->block)
			reg &= ~IMGRZ_RASTER_SCAN_OUT;
		else
			reg |= IMGRZ_RASTER_SCAN_OUT;
		break;
	case MTK_IMGRZ_COLOR_FORMAT_INDEX:
		break;
	case MTK_IMGRZ_COLOR_FORMAT_ARGB:
	case MTK_IMGRZ_COLOR_FORMAT_AYUV:
		osd_setting &= ~(IMGRZ_OSD_OUTPUT_RGB565 |
				IMGRZ_OSD_OUTPUT_ARGB1555 |
				IMGRZ_OSD_OUTPUT_ARGB4444 |
				IMGRZ_OSD_OUTPUT_ARGB8888);
		switch (dst->f.argb_fmt) {
		case MTK_IMGRZ_ARGB_FORMAT_0565:
			osd_setting |= IMGRZ_OSD_OUTPUT_RGB565;
			break;
		case MTK_IMGRZ_ARGB_FORMAT_1555:
			osd_setting |= IMGRZ_OSD_OUTPUT_ARGB1555;
			break;
		case MTK_IMGRZ_ARGB_FORMAT_4444:
			osd_setting |= IMGRZ_OSD_OUTPUT_ARGB4444;
			break;
		case MTK_IMGRZ_ARGB_FORMAT_8888:
			osd_setting |= IMGRZ_OSD_OUTPUT_ARGB8888;
			break;
		default:
			osd_setting |= IMGRZ_OSD_OUTPUT_ARGB8888;
			break;
		}

		if (dst->fmt == MTK_IMGRZ_COLOR_FORMAT_AYUV) {
			switch (src->fmt) {
			case MTK_IMGRZ_COLOR_FORMAT_Y_C:
			case MTK_IMGRZ_COLOR_FORMAT_Y_CB_CR:
				reg |= IMGRZ_V2OSD;
				reg &= ~(IMGRZ_420_OUT |
					IMGRZ_422_OUT |
					IMGRZ_444_OUT);
				reg |= IMGRZ_444_OUT;
				break;
			case MTK_IMGRZ_COLOR_FORMAT_ARGB:
				csc_setting |= IMGRZ_OSD_CSC_ENABLE;
				/* Do not down-scale Y */
				csc_setting &= ~IMGRZ_OSD_CSC_YIN_D16;
				csc_setting &= ~IMGRZ_OSD_CSC_CIN_D128;
				csc_setting &= ~IMGRZ_OSD_CSC_YOUT_A16;
				csc_setting |= IMGRZ_OSD_CSC_COUT_A128;
				writel_relaxed(0x132,
					base + RW_IMGRZ_OSD_CSC_COEF11);
				writel_relaxed(0x259,
					base + RW_IMGRZ_OSD_CSC_COEF12);
				writel_relaxed(0x75,
					base + RW_IMGRZ_OSD_CSC_COEF13);
				writel_relaxed(0x1F50,
					base + RW_IMGRZ_OSD_CSC_COEF21);
				writel_relaxed(0x1EA5,
					base + RW_IMGRZ_OSD_CSC_COEF22);
				writel_relaxed(0x20B,
					base + RW_IMGRZ_OSD_CSC_COEF23);
				writel_relaxed(0x20B,
					base + RW_IMGRZ_OSD_CSC_COEF31);
				writel_relaxed(0x1E4A,
					base + RW_IMGRZ_OSD_CSC_COEF32);
				writel_relaxed(0x1FAB,
					base + RW_IMGRZ_OSD_CSC_COEF33);
				break;
			default:
				break;
			}
		} else if (dst->fmt == MTK_IMGRZ_COLOR_FORMAT_ARGB) {
			switch (src->fmt) {
			case MTK_IMGRZ_COLOR_FORMAT_AYUV:
				csc_setting |= IMGRZ_OSD_CSC_ENABLE;
				/* Do not down-scale Y */
				csc_setting &= ~IMGRZ_OSD_CSC_YIN_D16;
				csc_setting |= IMGRZ_OSD_CSC_CIN_D128;
				csc_setting &= ~IMGRZ_OSD_CSC_YOUT_A16;
				csc_setting &= ~IMGRZ_OSD_CSC_COUT_A128;
				writel_relaxed(0x400,
					base + RW_IMGRZ_OSD_CSC_COEF11);
				writel_relaxed(0x0,
					base + RW_IMGRZ_OSD_CSC_COEF12);
				writel_relaxed(0x57C,
					base + RW_IMGRZ_OSD_CSC_COEF13);
				writel_relaxed(0x400,
					base + RW_IMGRZ_OSD_CSC_COEF21);
				writel_relaxed(0x1EA8,
					base + RW_IMGRZ_OSD_CSC_COEF22);
				writel_relaxed(0x1D35,
					base + RW_IMGRZ_OSD_CSC_COEF23);
				writel_relaxed(0x400,
					base + RW_IMGRZ_OSD_CSC_COEF31);
				writel_relaxed(0x6EE,
					base + RW_IMGRZ_OSD_CSC_COEF32);
				writel_relaxed(0x0,
					base + RW_IMGRZ_OSD_CSC_COEF33);
				break;
			default:
				break;
			}
		}
		break;
	default:
		return -EINVAL;
	}

	writel_relaxed(reg, base + RW_IMGRZ_TYPE);
	writel_relaxed(osd_setting, base + RW_IMGRZ_OSD_MODE_SETTING);
	writel_relaxed(addr_swap_setting, base + RW_IMGRZ_MEM_IF_MODE);
	writel_relaxed(csc_setting, base + RW_IMGRZ_OSD_CSC_SETTING);

	return 0;
}

void mtk_imgrz_hal_s_dst_buf_addr(void __iomem *base, uint32_t addr[])
{
	writel_relaxed(addr[0] >> 4, base + RW_IMGRZ_TGT_Y_ADDR_BASE);
	writel_relaxed(addr[1] >> 4, base + RW_IMGRZ_TGT_C_ADDR_BASE);
}

void mtk_imgrz_hal_s_dst_buf_pitch(void __iomem *base,
		enum mtk_imgrz_color_format fmt, u16 buf_width[])
{
	u32 buf_width_c = (fmt == MTK_IMGRZ_COLOR_FORMAT_AYUV) ?
			buf_width[0] : buf_width[1];
	u32 reg = readl_relaxed(base + RW_IMGRZ_TGT_BUF_LEN) & 0xFF000000;

	reg |= (buf_width[0] >> 4) | ((buf_width_c >> 4) << 12);
	writel_relaxed(reg, base + RW_IMGRZ_TGT_BUF_LEN);
}

void mtk_imgrz_hal_s_dst_pic_w_h(void __iomem *base,
		uint32_t width, uint32_t height)
{
	writel_relaxed(((width & 0xFFFF) << 16) | (height & 0xFFFF),
		       base + RW_IMGRZ_TGT_SIZE);
}

void mtk_imgrz_hal_s_dst_pic_ofs(void __iomem *base,
		unsigned int x_offset, unsigned int y_offset)
{
	writel_relaxed(((x_offset & 0xFFFF) << 16) | (y_offset & 0xFFFF),
			base + RW_IMGRZ_TGT_OFFSET);

}

void mtk_imgrz_hal_s_linebuflen_ext16(void __iomem *base, bool enable)
{
	u32 reg = readl_relaxed(base + RW_IMGRZ_TGT_BUF_LEN);

	if (enable)
		reg |= IMGRZ_BOUND_EXTEND_16_ON;
	else
		reg &= ~IMGRZ_BOUND_EXTEND_16_ON;

	writel_relaxed(reg, base + RW_IMGRZ_TGT_BUF_LEN);
}

void mtk_imgrz_hal_s_sram_linebuflen(void __iomem *base, uint32_t linebuf)
{
	u32 mask = ~(0x1F << IMGRZ_LINE_BUFFER_LEN_SHIFT);
	u32 reg = readl_relaxed(base + RW_IMGRZ_TGT_BUF_LEN);

	reg &= mask;
	reg += (linebuf << IMGRZ_LINE_BUFFER_LEN_SHIFT);
	writel_relaxed(reg, base + RW_IMGRZ_TGT_BUF_LEN);
}

void mtk_imgrz_hal_ufo_linebuf_eco(void __iomem *base)
{
	u32 reg = readl_relaxed(base + RW_IMGRZ_UFO_LINEB_ECO);

	writel_relaxed(reg | IMGRZ_UFO_LINEB_ECO,
		base + RW_IMGRZ_UFO_LINEB_ECO);
}

void mtk_imgrz_hal_s_jpeg_pic_mode(void __iomem *base, bool pic_mode)
{
	writel_relaxed(pic_mode ? IMGRZ_TRACKING_WITH_JPG_HW : 0,
			base + RW_IMGRZ_INTERFACE_SWITCH);
}

void mtk_imgrz_hal_ayuv_y_only(void __iomem *base, bool y_only)
{
	u32 val = readl_relaxed(base + RW_IMGRZ_TYPE);

	if (y_only)
		val |= IMGRZ_CBCR_PADDING;
	else
		val &= ~IMGRZ_CBCR_PADDING;
	writel_relaxed(val, base + RW_IMGRZ_TYPE);
}

void mtk_imgrz_hal_s_cbcr_pad(void __iomem *base,
			bool cb_exist, bool cr_exist)
{
	u32 reg = readl_relaxed(base + RW_IMGRZ_TYPE);

	if (!cb_exist)
		reg |= IMGRZ_CB_PADDING;
	if (!cr_exist)
		reg |= IMGRZ_CR_PADDING;

	writel_relaxed(reg, base + RW_IMGRZ_TYPE);
}

void mtk_imgrz_hal_jpg_component(void __iomem *base,
		bool y_exist, bool cb_exist, bool cr_exist)
{
	u32 reg = readl_relaxed(base + RW_IMGRZ_JPG_MODE);

	if (y_exist)
		reg |= IMGRZ_RECORD_Y;
	else
		reg &= ~IMGRZ_RECORD_Y;

	if (cb_exist)
		reg |= IMGRZ_RECORD_CB;
	else
		reg &= ~IMGRZ_RECORD_CB;

	if (cr_exist)
		reg |= IMGRZ_RECORD_CR;
	else
		reg &= ~IMGRZ_RECORD_CR;

	writel_relaxed(reg, base + RW_IMGRZ_JPG_MODE);
}

void mtk_imgrz_hal_s_tempbuf_addr(void __iomem *base, uint32_t addr)
{
	writel_relaxed(addr >> 4, base + RW_IMGRZ_TMP_ADDR_BASE);
}

void mtk_imgrz_hal_s_alpha_scale_type(void __iomem *base, unsigned int type)
{
	u32 reg = readl_relaxed(base + RW_IMGRZ_OSD_MODE_SETTING);

	switch (type) {
	case 0:
		reg |= IMGRZ_OSD_ALPHA_SCALE_NORMAL;
		break;
	case 1:
		reg |= IMGRZ_OSD_ALPHA_SCALE_REF_LEFT;
		break;
	case 2:
		reg |= IMGRZ_OSD_ALPHA_SCALE_REF_NEAREST;
		break;
	default:
		reg |= IMGRZ_OSD_ALPHA_SCALE_NORMAL;
		break;
	}
	writel_relaxed(reg, base + RW_IMGRZ_OSD_MODE_SETTING);
}

static void mtk_imgrz_hal_coeff_s_h8_y(void __iomem *base,
			   unsigned int offset, unsigned int factor)
{
	u32 reg = factor & 0x03FFFFFF;

	if (0 == (factor & 0xFC000000)) {
		writel_relaxed(reg, base + RW_IMGRZ_H8TAPS_FAC_Y);
		writel_relaxed(offset, base + RW_IMGRZ_H8TAP_OFSET_Y);
	} else {
		pr_err("%s- factor too large!\n", __func__);
	}
}

static void mtk_imgrz_hal_coeff_s_h8_cb(void __iomem *base,
			    unsigned int offset, unsigned int factor)
{
	u32 reg = factor & 0x03FFFFFF;

	if (0 == (factor & 0xFC000000)) {
		writel_relaxed(reg, base + RW_IMGRZ_H8TAPS_FAC_CB);
		writel_relaxed(offset, base + RW_IMGRZ_H8TAP_OFSET_CB);
	} else {
		pr_err("%s- factor too large!\n", __func__);
	}
}

static void mtk_imgrz_hal_coeff_s_h8_cr(void __iomem *base,
			    unsigned int offset, unsigned int factor)
{
	u32 reg = factor & 0x03FFFFFF;

	if (0 == (factor & 0xFC000000)) {
		writel_relaxed(reg, base + RW_IMGRZ_H8TAPS_FAC_CR);
		writel_relaxed(offset, base + RW_IMGRZ_H8TAP_OFSET_CR);
	} else {
		pr_err("%s- factor too large!\n", __func__);
	}
}

static void mtk_imgrz_hal_coeff_s_h8_coeffs(void __iomem *base,
		enum MTK_IMGRZ_SCALE_FACTOR factor)
{
	writel_relaxed(h_filtercoeff[factor][0], base + RW_IMGRZ_H_COEF0);
	writel_relaxed(h_filtercoeff[factor][1], base + RW_IMGRZ_H_COEF1);
	writel_relaxed(h_filtercoeff[factor][2], base + RW_IMGRZ_H_COEF2);
	writel_relaxed(h_filtercoeff[factor][3], base + RW_IMGRZ_H_COEF3);
	writel_relaxed(h_filtercoeff[factor][4], base + RW_IMGRZ_H_COEF4);
	writel_relaxed(h_filtercoeff[factor][5], base + RW_IMGRZ_H_COEF5);
	writel_relaxed(h_filtercoeff[factor][6], base + RW_IMGRZ_H_COEF6);
	writel_relaxed(h_filtercoeff[factor][7], base + RW_IMGRZ_H_COEF7);
	writel_relaxed(h_filtercoeff[factor][8], base + RW_IMGRZ_H_COEF8);
	writel_relaxed(h_filtercoeff[factor][9], base + RW_IMGRZ_H_COEF9);
	writel_relaxed(h_filtercoeff[factor][10], base + RW_IMGRZ_H_COEF10);
	writel_relaxed(h_filtercoeff[factor][11], base + RW_IMGRZ_H_COEF11);
	writel_relaxed(h_filtercoeff[factor][12], base + RW_IMGRZ_H_COEF12);
	writel_relaxed(h_filtercoeff[factor][13], base + RW_IMGRZ_H_COEF13);
	writel_relaxed(h_filtercoeff[factor][14], base + RW_IMGRZ_H_COEF14);
	writel_relaxed(h_filtercoeff[factor][15], base + RW_IMGRZ_H_COEF15);
	writel_relaxed(h_filtercoeff[factor][16], base + RW_IMGRZ_H_COEF16);
	writel_relaxed(h_filtercoeff[factor][17], base + RW_IMGRZ_H_COEF17);
}

static void mtk_imgrz_hal_coeff_s_v4_coeffs(void __iomem *base,
		enum MTK_IMGRZ_SCALE_FACTOR factor)
{
	writel_relaxed(v_filtercoeff[factor][0], base + RW_IMGRZ_V_COEF0);
	writel_relaxed(v_filtercoeff[factor][1], base + RW_IMGRZ_V_COEF1);
	writel_relaxed(v_filtercoeff[factor][2], base + RW_IMGRZ_V_COEF2);
	writel_relaxed(v_filtercoeff[factor][3], base + RW_IMGRZ_V_COEF3);
	writel_relaxed(v_filtercoeff[factor][4], base + RW_IMGRZ_V_COEF4);
	writel_relaxed(v_filtercoeff[factor][5], base + RW_IMGRZ_V_COEF5);
	writel_relaxed(v_filtercoeff[factor][6], base + RW_IMGRZ_V_COEF6);
	writel_relaxed(v_filtercoeff[factor][7], base + RW_IMGRZ_V_COEF7);
	writel_relaxed(v_filtercoeff[factor][8], base + RW_IMGRZ_V_COEF8);
}

static enum MTK_IMGRZ_SCALE_FACTOR
mtk_imgrz_hal_coeff_g_factor(uint32_t src_size, uint32_t dst_size)
{
	uint32_t scale_ratio = dst_size * 10000 / src_size;

	if (scale_ratio >= 10000)
		return FACTOR_1;
	else if (scale_ratio >= 5000)
		return FACTOR_0_5;
	else if (scale_ratio >= 2500)
		return FACTOR_0_25;
	else if (scale_ratio >= 1250)
		return FACTOR_0_125;
	else if (scale_ratio >= 625)
		return FACTOR_0_0625;
	else
		return FACTOR_0;
}

static void mtk_imgrz_hal_coeff_s_hsa_y(void __iomem *base,
		unsigned int offset, unsigned int factor)
{
	u32 reg = factor & 0x00000FFF;

	reg += ((offset & 0x000007FF) << 12);
	writel_relaxed(reg, base + RW_IMGRZ_HSA_SCL_Y);
}

static void mtk_imgrz_hal_coeff_s_hsa_cb(void __iomem *base,
		unsigned int offset, unsigned int factor)
{
	u32 reg = factor & 0x00000FFF;

	reg += ((offset & 0x000007FF) << 12);
	writel_relaxed(reg, base + RW_IMGRZ_HSA_SCL_CB);
}

static void mtk_imgrz_hal_coeff_s_hsa_cr(void __iomem *base,
		unsigned int offset, unsigned int factor)
{
	u32 reg = factor & 0x00000FFF;

	reg += ((offset & 0x000007FF) << 12);
	writel_relaxed(reg, base + RW_IMGRZ_HSA_SCL_CR);
}

static int mtk_imgrz_hal_coeff_s_v4_y(void __iomem *base,
		unsigned int offset, unsigned int factor)
{
	u32 reg = factor & 0x03FFFFFF;

	if (0 != (factor & 0xFC000000))
		return -EINVAL;

	writel_relaxed(reg, base + RW_IMGRZ_V4TAPS_SCL_Y);
	writel_relaxed(offset, base + RW_IMGRZ_V4TAP_OFSET_Y);

	return 0;
}

static int mtk_imgrz_hal_coeff_s_v4_cb(void __iomem *base,
		unsigned int offset, unsigned int  factor)
{
	u32 reg = factor & 0x03FFFFFF;

	if (0 != (factor & 0xF3000000))
		return -EINVAL;

	writel_relaxed(reg, base + RW_IMGRZ_V4TAPS_SCL_CB);
	writel_relaxed(offset, base + RW_IMGRZ_V4TAP_OFSET_C);

	return 0;
}

static int mtk_imgrz_hal_coeff_s_v4_cr(void __iomem *base, unsigned int offset,
			    unsigned int factor)
{
	u32 reg = factor & 0x03FFFFFF;

	if (0 != (factor & 0xF3000000))
		return -EINVAL;

	writel_relaxed(reg, base + RW_IMGRZ_V4TAPS_SCL_CR);

	return 0;
}

static int mtk_imgrz_hal_coeff_s_vm_y(void __iomem *base,
		unsigned int offset, unsigned int factor, bool scale_up)
{
	u32 reg = factor & 0x000007FF;

	if ((0 != (factor & 0xFFFFF800)) || (0 != (offset & 0xFFFFF000)))
		return -EINVAL;

	reg += ((offset & 0x000007FF) << 12) + ((scale_up & 0x1) << 11);
	writel_relaxed(reg, base + RW_IMGRZ_V_SCL_Y);

	return 0;
}

static int mtk_imgrz_hal_coeff_s_vm_cb(void __iomem *base,
		unsigned int offset, unsigned int factor, bool scale_up)
{
	u32 reg = factor & 0x000007FF;

	if ((0 != (factor & 0xFFFFF800)) || (0 != (offset & 0xFFFFF000)))
		return -EINVAL;

	reg += ((offset & 0x000007FF) << 12) + ((scale_up & 0x1) << 11);
	writel_relaxed(reg, base + RW_IMGRZ_V_SCL_CB);

	return 0;
}

static int mtk_imgrz_hal_coeff_s_vm_cr(void __iomem *base,
		unsigned int offset, unsigned int factor, bool scale_up)
{
	u32 reg = factor & 0x000007FF;

	if ((0 != (factor & 0xFFFFF800)) || (0 != (offset & 0xFFFFF000)))
		return -EINVAL;

	reg += ((offset & 0x000007FF) << 12) + ((scale_up & 0x1) << 11);
	writel_relaxed(reg, base + RW_IMGRZ_V_SCL_CR);

	return 0;
}

int mtk_imgrz_hal_coeff_s_h_factor(void __iomem *base,
			struct mtk_imgrz_scale_param *scale_param)
{
	struct mtk_imgrz_buf_info *src = &scale_param->src;
	struct mtk_imgrz_buf_info *dst = &scale_param->dst;

	/* Y */
	mtk_imgrz_hal_coeff_s_h8_y(base,
		scale_param->h8_offset[0], scale_param->h8_factor[0]);
	mtk_imgrz_hal_coeff_s_hsa_y(base,
		scale_param->hsa_offset[0], scale_param->hsa_factor[0]);
	if ((scale_param->h_method == MTK_IMGRZ_8_TAP_RESAMPLE) ||
	    (src->pic_width <= dst->pic_width)) {
		mtk_imgrz_hal_coeff_s_h8_coeffs(base,
			mtk_imgrz_hal_coeff_g_factor(src->pic_width,
						dst->pic_width));
	}

	if ((src->fmt == MTK_IMGRZ_COLOR_FORMAT_ARGB) ||
	    (src->fmt == MTK_IMGRZ_COLOR_FORMAT_AYUV) ||
	    (src->fmt == MTK_IMGRZ_COLOR_FORMAT_INDEX)||
	    (src->fmt == MTK_IMGRZ_COLOR_FORMAT_GREY))

		return 0;

	/* Cb */
	mtk_imgrz_hal_coeff_s_h8_cb(base,
		scale_param->h8_offset[1], scale_param->h8_factor[1]);
	mtk_imgrz_hal_coeff_s_hsa_cb(base,
		scale_param->hsa_offset[1], scale_param->hsa_factor[1]);

	if (src->fmt == MTK_IMGRZ_COLOR_FORMAT_Y_C)
		return 0;

	/* Cr */
	mtk_imgrz_hal_coeff_s_h8_cr(base,
		scale_param->h8_offset[2], scale_param->h8_factor[2]);
	mtk_imgrz_hal_coeff_s_hsa_cr(base,
		scale_param->hsa_offset[2], scale_param->hsa_factor[2]);

	return 0;
}

int mtk_imgrz_hal_coeff_s_v_factor(void __iomem *base,
			struct mtk_imgrz_scale_param *scale_param)
{
	struct mtk_imgrz_buf_info *src = &scale_param->src;
	struct mtk_imgrz_buf_info *dst = &scale_param->dst;

	switch (scale_param->v_method) {
	case MTK_IMGRZ_4_TAP_RESAMPLE:
		mtk_imgrz_hal_coeff_s_v4_y(base,
			scale_param->v4_offset[0], scale_param->v4_factor[0]);
		if ((src->fmt == MTK_IMGRZ_COLOR_FORMAT_ARGB) ||
		    (src->fmt == MTK_IMGRZ_COLOR_FORMAT_AYUV) ||
		    (src->fmt == MTK_IMGRZ_COLOR_FORMAT_INDEX)||
		    (src->fmt == MTK_IMGRZ_COLOR_FORMAT_GREY))
			break;
		mtk_imgrz_hal_coeff_s_v4_cb(base,
			scale_param->v4_offset[1], scale_param->v4_factor[1]);
		if (src->fmt == MTK_IMGRZ_COLOR_FORMAT_Y_C)
			break;
		mtk_imgrz_hal_coeff_s_v4_cr(base,
			scale_param->v4_offset[2], scale_param->v4_factor[2]);
		mtk_imgrz_hal_coeff_s_v4_coeffs(base,
			mtk_imgrz_hal_coeff_g_factor(src->pic_height,
						dst->pic_height));
		break;

	case MTK_IMGRZ_M_TAP_RESAMPLE:
		mtk_imgrz_hal_coeff_s_vm_y(base, scale_param->vm_offset[0],
			scale_param->vm_factor[0], scale_param->vm_scale_up[0]);
		if ((src->fmt == MTK_IMGRZ_COLOR_FORMAT_ARGB) ||
		    (src->fmt == MTK_IMGRZ_COLOR_FORMAT_AYUV) ||
		    (src->fmt == MTK_IMGRZ_COLOR_FORMAT_INDEX)||
		    (src->fmt == MTK_IMGRZ_COLOR_FORMAT_GREY))
			break;
		mtk_imgrz_hal_coeff_s_vm_cb(base, scale_param->vm_offset[1],
			scale_param->vm_factor[1], scale_param->vm_scale_up[1]);
		if (src->fmt == MTK_IMGRZ_COLOR_FORMAT_Y_C)
			break;
		mtk_imgrz_hal_coeff_s_vm_cr(base, scale_param->vm_offset[2],
			scale_param->vm_factor[2], scale_param->vm_scale_up[2]);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

u32 mtk_imgrz_checksum_write(void __iomem *base)
{
	u32 reg;

	reg = readl_relaxed(base + RW_IMGRZ_FLIP);
	reg |= (0x1 << 5);
	writel_relaxed(reg, base + RW_IMGRZ_FLIP);

	return readl_relaxed(base + RO_IMGRZ_CHECK_SUM_REG);
}

u32 mtk_imgrz_checksum_read(void __iomem *base)
{
	u32 reg;

	reg = readl_relaxed(base + RW_IMGRZ_FLIP);
	reg &= ~(0x1 << 5);
	writel_relaxed(reg, base + RW_IMGRZ_FLIP);

	return readl_relaxed(base + RW_IMGRZ_READ_CHECKSUM);
}

