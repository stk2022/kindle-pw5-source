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

#ifndef MTK_IMGRZ_REG_H
#define MTK_IMGRZ_REG_H

#define RW_IMGRZ_START		0x000
#define IMGRZ_ACTIVATE		(0x1 << 0)
#define IMGRZ_ENABLE		(0x1 << 1)
#define IMGRZ_SW_RESET_ON	(0x3 << 2)
#define IMGRZ_CHK_SUM_CLR	(0x1 << 4)
#define IMGRZ_DMA_SW_RST	(0x1 << 5)
#define IMGRZ_WAIT_WR_DONE	(0x1 << 6)
#define IMGRZ_INT_ON		(0x1 << 7)
#define IMGRZ_REGISTER_RESET_ON	(0x3 << 8)
#define IMGRZ_GCE_OFF		(0x1 << 12)
#define IMGRZ_RD_BURST_ON	(0x1 << 16)
#define IMGRZ_DMA_SAFE_MODE	(0x1 << 18)
#define IMGRZ_MMU_RESET_ON	(0x3 << 20)
#define IMGRZ_MMU_ENABLE	(0x1 << 22)
#define IMGRZ_IRQ_CLEAR		(0x1 << 23)
#define IMGRZ_WR_BURST_ON	(0x1 << 24)
#define IMGRZ_WR_BST_NCROSS	(0x1 << 25)

#define RW_IMGRZ_TYPE			0x004
#define IMGRZ_VIDEO_MODE		(0x0 << 0)
#define IMGRZ_JPEG_MODE			(0x1 << 0)
#define IMGRZ_SEL_VID_MODE		(0x0 << 1)
#define IMGRZ_SEL_OSD_MODE		(0x1 << 1)
#define IMGRZ_INTERLACE_BOTTOM_FIELD	(0x0 << 2)
#define IMGRZ_INTERLACE_TOP_FIELD	(0x1 << 2)
#define IMGRZ_FRAME			(0x0 << 3)
#define IMGRZ_FIELD			(0x1 << 3)
#define IMGRZ_BLOCK_BASED_IN		(0x0 << 4)
#define IMGRZ_RASTER_SCAN_IN		(0x1 << 4)
#define IMGRZ_OSD_NORMAL_MODE		(0x0 << 5)
#define IMGRZ_OSD_PARTIAL_MODE		(0x1 << 5)
#define IMGRZ_BILINEAR_FILTER		(0x0 << 8)
#define IMGRZ_V_4_TAP_FILTER		(0x1 << 8)
/* Turbe Mode, alpha compostion will enable this */
#define IMGRZ_FIX4_ENABLE		(0x1 << 12)
#define IMGRZ_BLOCK_BASED_OUT		(0x0 << 16)
#define IMGRZ_RASTER_SCAN_OUT		(0x1 << 16)
#define IMGRZ_420_OUT			(0x0 << 17)
#define IMGRZ_422_OUT			(0x1 << 17)
#define IMGRZ_444_OUT			(0x2 << 17)
#define IMGRZ_CR_PADDING		(0x1 << 19)
#define IMGRZ_CB_PADDING		(0x2 << 19)
#define IMGRZ_CBCR_PADDING		(0x3 << 19)
#define IMGRZ_CBCRSWAP			(0x1 << 21)
#define IMGRZ_V2V			(0x0 << 24)
#define IMGRZ_V2OSD			(0x1 << 24)
#define IMGRZ_ARGB_ONE_PHASE		(0x1 << 25)
#define IMGRZ_ONE_PHASE_CSC		(0x1 << 26)
#define IMGRZ_10BIT_OUT_CALC		(0x1 << 29)
#define IMGRZ_10BIT_OUT			(0x1 << 30)
#define IMGRZ_10BIT_ON			(0x1 << 31)

#define RW_IMGRZ_JPG_MODE		0x008
#define IMGRZ_LAST_BLOCK_LINE		(0x1 << 12)
#define IMGRZ_FIRST_BLOCK_LINE		(0x1 << 13)
#define IMGRZ_RECORD_CR			(0x1 << 14)
#define IMGRZ_RECORD_CB			(0x1 << 15)
#define IMGRZ_RECORD_Y			(0x1 << 16)
#define IMGRZ_PRELOAD_DRAM_DATA		(0x1 << 18)
#define IMGRZ_LINES_ASSIGNED_DIRECTLY	(0x1 << 31)

#define RW_IMGRZ_MEM_IF_MODE		0x00C
#define IMGRZ_DRAM_BURST_LIMIT_1	(0x1 << 8)
#define IMGRZ_DRAM_BURST_LIMIT_2	(0x2 << 8)
#define IMGRZ_DRAM_BURST_LIMIT_3	(0x3 << 8)
#define IMGRZ_DRAM_BURST_LIMIT_4	(0x4 << 8)
#define IMGRZ_DRAM_BURST_LIMIT_8	(0x8 << 8)
#define IMGRZ_DRAM_BURST_LIMIT_16	(0x10 << 8)
#define IMGRZ_KEEP_DRAM_READ_REQUEST	(0x1 << 16)
#define IMGRZ_REST_ADDRESS		(0x1 << 20)
#define IMGRZ_MMU_TEMPBUF_AGENTID_MASK	(0x1 << 31)

#define RW_IMGRZ_SRC_BUF_LEN		0x010

#define RW_IMGRZ_INTERFACE_SWITCH	0x014
#define IMGRZ_TRACKING_WITH_JPG_HW	(0x1 << 0)

#define RW_IMGRZ_TGT_BUF_LEN		0x018
#define IMGRZ_BOUND_EXTEND_16_OFF	(0x0 << 31)
#define IMGRZ_BOUND_EXTEND_16_ON	(0x1 << 31)
#define IMGRZ_LINE_BUFFER_LEN_SHIFT	24
#define IMGRZ_TGT_BUFFER_LEN_SHIFT	0

#define RW_IMGRZ_SRC_Y_ADDR_BASE1	0xF1C
#define RW_IMGRZ_SRC_Y_ADDR_BASE2	0xF20
#define RW_IMGRZ_SRC_CB_ADDR_BASE1	0xF24
#define RW_IMGRZ_SRC_CB_ADDR_BASE2	0xF28
#define RW_IMGRZ_SRC_CR_ADDR_BASE1	0xF2C
#define RW_IMGRZ_SRC_CR_ADDR_BASE2	0xF30
#define RW_IMGRZ_TGT_Y_ADDR_BASE	0xF34
#define RW_IMGRZ_TGT_C_ADDR_BASE	0xF38
#define RW_IMGRZ_SRC_SIZE_Y		0x040
#define RW_IMGRZ_SRC_SIZE_CB		0x044
#define RW_IMGRZ_SRC_SIZE_CR		0x048
#define RW_IMGRZ_TGT_SIZE		0x04C
#define RW_IMGRZ_SRC_OFFSET_Y		0x054
#define RW_IMGRZ_SRC_OFFSET_CB		0x058
#define RW_IMGRZ_SRC_OFFSET_CR		0x05C
#define RW_IMGRZ_TGT_OFFSET		0x060
#define RW_IMGRZ_H8TAPS_FAC_Y		0x064
#define RW_IMGRZ_H8TAPS_FAC_CB		0x068
#define RW_IMGRZ_H8TAPS_FAC_CR		0x06C
#define RW_IMGRZ_HSA_SCL_Y		0x070
#define RW_IMGRZ_HSA_SCL_CB		0x074
#define RW_IMGRZ_HSA_SCL_CR		0x078
#define RW_IMGRZ_V_SCL_Y		0x07C
#define IMGRZ_Y_VERTICAL_DOWN_SCALING	(0x0 << 0)
#define IMGRZ_Y_VERTICAL_UP_SCALING	(0x1 << 0)

#define RW_IMGRZ_V_SCL_CB		0x080
#define IMGRZ_CB_VERTICAL_DOWN_SCALING	(0x0 << 0)
#define IMGRZ_CB_VERTICAL_UP_SCALING	(0x1 << 0)

#define RW_IMGRZ_V_SCL_CR		0x084
#define IMGRZ_CR_VERTICAL_DOWN_SCALING	(0x0 << 0)
#define IMGRZ_CR_VERTICAL_UP_SCALING	(0x1 << 0)

#define RW_IMGRZ_V4TAPS_SCL_Y		0x088
#define RW_IMGRZ_V4TAPS_SCL_CB		0x08C
#define RW_IMGRZ_V4TAPS_SCL_CR		0x090
#define RW_IMGRZ_TMP_ADDR_BASE		0xF94
#define RW_IMGRZ_Y_PRELOAD_OW_ADDR_BASE	0xF98
#define RW_IMGRZ_C_PRELOAD_OW_ADDR_BASE	0xF9C
#define RW_IMGRZ_H8TAP_OFSET_Y		0x0C8
#define RW_IMGRZ_H8TAP_OFSET_CB		0x0CC
#define RW_IMGRZ_H8TAP_OFSET_CR		0x0D0
#define RW_IMGRZ_V4TAP_OFSET_Y		0x0D4
#define RW_IMGRZ_V4TAP_OFSET_C		0x0D8
#define RO_IMGRZ_CHECK_SUM_REG		0x0E4
#define RO_IMGRZ_INTERFACE_MONITOR_REG	0x0EC
#define RO_IMGRZ_STATUS_MONITOR_REG	0x0F4
#define IMGRZ_RD_REQ			(0x1 << 0)

#define RO_IMGRZ_DATA_MONITOR_REG	0x0F8
#define RO_IMGRZ_DONE			0x0FC
#define RW_IMGRZ_OSD_MODE_SETTING	0x100
#define IMGRZ_OSD_INDEX_MODE		(0x0 << 0)
#define IMGRZ_OSD_DIRECT_MODE		(0x1 << 0)
#define IMGRZ_OSD_INDEX_2BPP		(0x1 << 1)
#define IMGRZ_OSD_INDEX_4BPP		(0x2 << 1)
#define IMGRZ_OSD_INDEX_8BPP		(0x3 << 1)
#define IMGRZ_OSD_DIRECT_RGB565		(0x0 << 1)
#define IMGRZ_OSD_DIRECT_ARGB1555	(0x1 << 1)
#define IMGRZ_OSD_DIRECT_ARGB4444	(0x2 << 1)
#define IMGRZ_OSD_DIRECT_ARGB8888	(0x3 << 1)
#define IMGRZ_OSD_REPEATING		(0x1 << 4)
#define IMGRZ_OSD_ALPHA_SCALE_NORMAL	(0x0 << 5)
#define IMGRZ_OSD_ALPHA_SCALE_REF_LEFT		(0x1 << 5)
#define IMGRZ_OSD_ALPHA_SCALE_REF_NEAREST	(0x2 << 5)
#define IMGRZ_OSD_ONLY_DISTINGUISH_ALPHA	(0x1 << 7)
#define IMGRZ_OSD_ALPHA_BILINEAR_BOUNDARY	(0x1 << 8)
#define IMGRZ_OSD_OUTPUT_RGB565		(0x0 << 9)
#define IMGRZ_OSD_OUTPUT_ARGB1555	(0x1 << 9)
#define IMGRZ_OSD_OUTPUT_ARGB4444	(0x2 << 9)
#define IMGRZ_OSD_OUTPUT_ARGB8888	(0x3 << 9)
#define IMGRZ_OSD_SWITCH_CBCR		(0x1 << 12)
#define IMGRZ_OSD_ARGB_RGBA		(0x1 << 14)
#define IMGRZ_A_BLEND_SHIFT		24

#define RW_IMGRZ_OSD_MD_CTRL		0x104
#define IMGRZ_OSD_WR_CPT		(0x0 << 0)
#define IMGRZ_OSD_ED_CPT		(0x1 << 0)
#define IMGRZ_OSD_CPT_DISABLE		(0x0 << 1)
#define IMGRZ_OSD_CPT_ENABLE		(0x1 << 1)
#define IMGRZ_OSD_ALU_ENABLE		(0x1 << 28)

#define RW_IMGRZ_OSD_ALPHA_TBL		0x108
#define RW_IMGRZ_OSD_COLOR_TRANSLATION0	0x10C
#define RW_IMGRZ_OSD_COLOR_TRANSLATION1	0x110
#define RW_IMGRZ_OSD_COLOR_TRANSLATION2	0x114
#define RW_IMGRZ_OSD_COLOR_TRANSLATION3	0x118
#define RW_IMGRZ_H_COEF0		0x124
#define RW_IMGRZ_H_COEF1		0x128
#define RW_IMGRZ_H_COEF2		0x12C
#define RW_IMGRZ_H_COEF3		0x130
#define RW_IMGRZ_H_COEF4		0x134
#define RW_IMGRZ_H_COEF5		0x138
#define RW_IMGRZ_H_COEF6		0x13C
#define RW_IMGRZ_H_COEF7		0x140
#define RW_IMGRZ_H_COEF8		0x144
#define RW_IMGRZ_H_COEF9		0x148
#define RW_IMGRZ_H_COEF10		0x14C
#define RW_IMGRZ_H_COEF11		0x150
#define RW_IMGRZ_H_COEF12		0x154
#define RW_IMGRZ_H_COEF13		0x158
#define RW_IMGRZ_H_COEF14		0x15C
#define RW_IMGRZ_H_COEF15		0x160
#define RW_IMGRZ_H_COEF16		0x164
#define RW_IMGRZ_H_COEF17		0x168
#define RW_IMGRZ_V_COEF0		0x16C
#define RW_IMGRZ_V_COEF1		0x170
#define RW_IMGRZ_V_COEF2		0x174
#define RW_IMGRZ_V_COEF3		0x178
#define RW_IMGRZ_V_COEF4		0x17C
#define RW_IMGRZ_V_COEF5		0x180
#define RW_IMGRZ_V_COEF6		0x184
#define RW_IMGRZ_V_COEF7		0x188
#define RW_IMGRZ_V_COEF8		0x18C
#define RW_IMGRZ_OSD_DITHER_SETTING	0x190
#define RW_IMGRZ_OSD_CSC_SETTING	0x194
#define IMGRZ_OSD_CSC_ENABLE		(0x1 << 0)
#define IMGRZ_OSD_CSC_YIN_D16		(0x1 << 1)
#define IMGRZ_OSD_CSC_CIN_D128		(0x1 << 2)
#define IMGRZ_OSD_CSC_YOUT_A16		(0x1 << 3)
#define IMGRZ_OSD_CSC_COUT_A128		(0x1 << 4)

#define RW_IMGRZ_OSD_CSC_COEF11		0x198
#define RW_IMGRZ_OSD_CSC_COEF12		0x19C
#define RW_IMGRZ_OSD_CSC_COEF13		0x1A0
#define RW_IMGRZ_OSD_CSC_COEF21		0x1A4
#define RW_IMGRZ_OSD_CSC_COEF22		0x1A8
#define RW_IMGRZ_OSD_CSC_COEF23		0x1AC
#define RW_IMGRZ_OSD_CSC_COEF31		0x1B0
#define RW_IMGRZ_OSD_CSC_COEF32		0x1B4
#define RW_IMGRZ_OSD_CSC_COEF33		0x1B8

#define RW_IMGRZ_RPR			0x1D0
#define IMGRZ_RPR_FLAG_ON		(0x1 << 0)
#define IMGRZ_URCRPR_ON			(0x1 << 1)
#define IMGRZ_TRC_VDEC_EN		(0x1 << 8)
#define IMGRZ_TRC_VDEC_INT		(0x1 << 9)

#define RW_IMGRZ_FLIP			0x1D8
#define IMGRZ_OUT_FLIP_ON		(0x1 << 0)

#define RW_IMGRZ_UFO_LINEB_ECO		0x1DC
#define IMGRZ_UFO_LINEB_ECO		(0x1 << 5)

#define RW_IMGRZ_LINE_NUM_Y		0x1C4
#define RW_IMGRZ_LINE_NUM_CB		0x1C8
#define RW_IMGRZ_LINE_NUM_CR		0x1CC
#define RW_IMGRZ_VENC_SKIP		0x1EC
#define RM_IMGRZ_VENC_SKIP_ON		(0x1 << 0)
#define IMGRZ_RD_BURST_SHIFT		(4)
#define IMGRZ_WR_BURST_SHIFT		(8)
#define IMGRZ_RD_BURST_LIMIT_CLEAR	0xFFFFFF0F
#define IMGRZ_WR_BURST_LIMIT_CLEAR	0xFFFFF0FF
#define IMGRZ_ENABLE_CHROMA		(0x1 << 16)

#define RW_IMGRZ_READ_CHECKSUM		0x1E4
#define RW_IMGRZ_ALPHA_COMPOSITION	0x200
#define RW_IMGRZ_UFO_TRIG		0x204
#define IMGRZ_UFO_TRIG			(0x1 << 0)
#define RW_IMGRZ_UFO_POWER		0x208
#define IMGRZ_UFO_ON			(0x1 << 0)
#define IMGRZ_BITS_POWER		(0x1 << 1)
#define IMGRZ_LEN_POWER			(0x1 << 2)
#define IMGRZ_DRAM_CLK			(0x1 << 3)
#define IMGRZ_UFO_CLK			(0x1 << 4)
/*1: ufo_interrupt function on  0: off*/
#define IMGRZ_UFO_INT_ON		(0x1 << 5)
/*1: make ufo line buffer ready after imgrz done  0: off*/
#define IMGRZ_UFO_LINEB_READY		(0x1 << 6)
#define IMGRZ_UFO_OUTSTANDING		(0x1 << 7)
#define RW_IMGRZ_UFO_CFG		0x20C
#define IMGRZ_COMPRESS_EN		(0x1 << 0)
#define IMGRZ_CHROMA			(0x1 << 3)
/*no need to wait for imgrz req up, then trig ufo_dec.*/
/*if this bit's up, after trig imgrz, ufo will be auto trig*/
#define IMGRZ_UFO_AUTO_TRIG		(0x1 << 5)
/* UFO PIC is the buffer wid(pitch) and buffer height */
#define	RW_IMGRZ_UFO_PIC_SZ		0x210
#define RW_IMGRZ_UFO_PAGE_SZ		0x214
#define RW_IMGRZ_UFO_START_POINT	0x218
#define RW_IMGRZ_UFO_Y_ADDR		0x21C
#define RW_IMGRZ_UFO_Y_LEN_ADDR		0x220
#define RW_IMGRZ_UFO_C_ADDR		0x224
#define	RW_IMGRZ_UFO_C_LEN_ADDR		0x228
#define RW_IMGRZ_UFO_JUMPMODE		0x238 /* Only for 10bit */
#define IMGRZ_UFO_JUMPMODE_EN		0x1

#define RW_IMGRZ_SEC_UFO_PAGE_SZ	0x300

#define RW_IMGRZ_ULTRA_LIMIT	0x304
#define IMGRZ_ULTRA_OFF		(0x1 << 24)
#endif
