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

#ifndef MTK_PNG_REG_H
#define MTK_PNG_REG_H

#define PNG_RESET_REG		0x00
#define PNG_RESET_START		0xF
#define PNG_RESET_END		0x0
#define PNG_GCE_OFF		(7 << 6)

#define PNG_ENABLE_REG		0x04
#define PNG_ENABLE              (1 << 0)
#define PNG_MMU_EN              (1 << 1)
#define PNG_RANDOM_RESET        (1 << 2)
#define PNG_PITCH_EN            (1 << 3)
#define PNG_BURST_WRITE_EN      (1 << 4)
#define PNG_128B_ALIGN_DISABLE	(1 << 5)

#define PNG_SRAM_CHIP_SELECT_REG	0x08
#define PNG_REG_SRAM_OFF		0x0
#define PNG_REG_TP_SRAM_OPTIONAL_ON	0x1
#define PNG_REG_SRAM_ON			0x2

#define PNG_DECODE_START_REG            0x0C
#define PNG_SLICE_RESUME_REG            0x10
#define PNG_BITSTRM_RESUME_REG          0x14

#define PNG_READ_PLTE_CHUNK_REG         0x18
#define PNG_READ_PLTE			0x1

#define PNG_READ_TRNS_CHUNK_REG         0x1C
#define PNG_READ_TRNS			0x1

#define PNG_PLTE_ENTRY_NUM_REG          0x20
#define PNG_PLTE_ENTRY_NUM_MASK		0xFF

#define PNG_TRNS_ENTRY_NUM_REG		0x24
#define PNG_TRNS_ENTRY_NUM_MASK		0xFF

#define PNG_LAST_IDAT_GRP_REG		0x28
#define PNG_LAST_IDAT_GRP		0x1
#define PNG_NOTLAST_IDAT_GRP		0x0

#define PNG_LAST_SLICE_REG		0x2C
#define PNG_LAST_SLICE			0x1
#define PNG_NOTLAST_SLICE		0x0

#define PNG_DRAM_REQ_SET_REG		0x30
#define PNG_DRAM_REQ_1			0x0
#define PNG_DRAM_REQ_2			0x1
#define PNG_DRAM_REQ_3			0x2
#define PNG_DRAM_REQ_4			0x3
#define FIFO_INT_EN			(0x1 << 11)

#define PNG_BS_SA_REG			0xf04
#define PNG_BS_SA_WR_REG		0x38
#define PNG_SA_CHG			0x1

#define PNG_BS_LENGTH_REG		0x3C
#define PNG_BS_FIFO_SRT_OWADDR_REG	0xf08
#define PNG_BS_FIFO_END_OWADDR_REG	0xf0c
#define PNG_PIXEL_OUT_ADDR_REG          0xf10
#define PNG_PIXEL_OUT_SRT_OWADDR_REG    0xf14
#define PNG_PIXEL_OUT_END_OWADDR_REG    0xf18
#define PNG_LZ77_STR_OWADDR_REG         0xf1c
#define PNG_LZ77_END_OWADDR_REG         0xf20
#define PNG_LINE_BUF_STR_OWADDR_REG     0xf24
#define PNG_LINE_BUF_END_OWADDR_REG     0xf28

#define PNG_DISP_WIDTH_SLICE_HEGIHT_REG 0x64
#define PNG_REG_DISP_W_SHIFT		16
#define PNG_REG_SLICE_H_SHIFT		0

#define PNG_SRC_WIDTH_HEIGHT_REG	0x68
#define PNG_SRC_W_SHIFT			16
#define PNG_SRC_H_SHIFT			0
#define PNG_SRC_W_MASK			0xFFFF
#define PNG_SRC_H_MASK			0xFFFF

#define PNG_CROP_ORG_XY_REG		0x6C
#define PNG_CROP_X_SHIFT		16
#define PNG_CROP_Y_SHIFT		0
#define PNG_CROP_X_MASK			0xFFFF
#define PNG_CROP_Y_MASK			0xFFFF

#define PNG_CROP_WIDTH_HEIGHT_REG	0x70
#define PNG_CROP_W_SHIFT		16
#define PNG_CROP_H_SHIFT		0
#define PNG_CROP_W_MASK			0xFFFF      //must minus 1
#define PNG_CROP_H_MASK			0xFFFF      //must minus 1

#define PNG_SRC_FORMAT_REG		0x78
#define PNG_BIT_DEPTH_SHIFT		0x4
#define PNG_CLR_TYPE_SHIFT		0x1
#define PNG_BIT_DEPTH_MASK		0x1F
#define PNG_CLR_TYPE_MASK		0x7
#define PNG_REG_INTERLACE_ON		0x1
#define PNG_REG_INTERLACE_OFF		0x0

#define PNG_OUTPUT_FORMAT_REG		0x7C
#define PNG_OUT_FORMAT_PALETTE		0x0
#define PNG_OUT_FORMAT_ARGB1555		0x1
#define PNG_OUT_FORMAT_RGB565		0x2
#define PNG_OUT_FORMAT_ARGB4444		0x3
#define PNG_OUT_FORMAT_ARGB8888		0x4

#define PNG_ALPHA_MATCHED_PIXEL_REG	0x80
#define PNG_ALPHA_UNMATCHED_PIXEL_REG	0x84
#define PNG_TRANS_CTRL_REG		0x88
#define PNG_TRNS_EN			(1 << 3)
#define PNG_TRNS_OUT			(1 << 2)
#define PNG_TRNS_ORG_ALPHA		(1 << 1)
#define PNG_TRNS_MATCH_16BIT		(1 << 0)

#define PNG_TRNS_KEY0_REG		0x8C
#define PNG_TRNS_GRAY_SHIFT		16
#define PNG_TRNS_R_SHIFT		0

#define PNG_TRNS_KEY1_REG		0x90
#define PNG_TRNS_G_SHIFT		16
#define PNG_TRNS_B_SHIFT		0

#define PNG_TRANS_BG_REG		0x94
#define PNG_IRQ2_CLR			0x4000000
#define PNG_IRQ1_CLR			0x2000000
#define PNG_IRQ0_CLR			0x1000000
#define PNG_TRNS_BG_GREY_SHIFT		24
#define PNG_TRNS_BG_R_SHIFT		16
#define PNG_TRNS_BG_G_SHIFT		8
#define PNG_TRNS_BG_B_SHIFT		0

#define PNG_CHUNK_TYPE_REG		0x98
#define PNG_CHUNK_TYPE_WR_REG		0x9C
#define PNG_CHUNK_WR			0x1

#define PNG_IDAT_AUTOSKIP_REG		0xfc
#define PNG_IDAT_AUTOSKIP_EN		0x1

#define PNG_IBST_LAST_RESUME_REG	0x100
/* indicate the last part of IDAT data in IDAT auto skip mode */
#define PNG_IBST_LAST_RESUME		0x1

#define PNG_PELOUT_WDLE_EN_REG		0x104
/* Assert dst or dec done irq after all dram request is done */
#define PNG_PELOUT_WDLE_EN1		(1 << 1)

#define PNG_IDAT_STATUS_CTRL_REG	0x108
#define PNG_IDAT_CLR_STATUS		0x1
#define PNG_IDAT_EN_STATUS		0x0

#endif
