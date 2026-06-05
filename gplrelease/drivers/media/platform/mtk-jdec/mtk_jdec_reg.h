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

#ifndef MTK_JDEC_REG_H
#define MTK_JDEC_REG_H

#define HW_JDEC_VLD_NUM 1
#define HW_JDEC_MC_NUM 1

#define RO_JDEC_BARL			0x0
#define RW_JDEC_BS_INIT			0x8C
	#define JDEC_INIT_FETCH			(0x1 << 20)
	#define JDEC_INIT_BS			(0x1 << 23)
#define RW_JDEC_PWR_DOWN		0xC4
#define RO_JDEC_BS_INIT_ST		0xE8
	#define JDEC_FETCH_READY		(0x1 << 0)
	#define JDEC_INIT_VALID			(0x1 << 2)
	#define JDEC_B_INIT_VALID		(0x1 << 3)
#define RO_JDEC_BS_DEBUG		0xF4
	#define JDEC_BITS_DMA_PROC		(1 << 2)
	#define JDEC_BITS_PROC_RDY		1

#define WO_JDEC_SW_RESET		0x108
	#define JDEC_SW_RST_CLR			0x0
	#define JDEC_DEC_SW_RST			0x1
	#define JDEC_LARB_SW_RST		(0x1 << 4)
	#define JDEC_AFIFO_SW_RST		(0x1 << 8)
	#define JDEC_IOMMU_SW_RST		(0x1 << 12)
#define WO_JDEC_MAXCODE_VALOFT_DAT	0x118
#define RW_JDEC_HUFF_TBL		0x11C
	#define VLD_AC_HUFF_FLAG		0x80000000
#define RW_JDEC_Q_TBL_NO		0x120

#define RW_JDEC_MAXCODE_VALOFT_IDX	0x124
#define RW_JDEC_MISC			0x12C
#define RW_JDEC_BLK_DC_TBL		0x130
#define RW_JDEC_BLK_AC_TBL		0x134
#define RW_JDEC_BLK_PLIST		0x138

#define RW_JDEC_MCU_MLIST		0x13C

#define RW_JDEC_HOST			0x144
	#define JDEC_INT_WAIT_BITS_RD		(0x1 << 3)
	#define JDEC_INT_ENABLE			(0x1 << 4)
#define RW_JDEC_RESTART			0x14C
	#define JDEC_MCU_RST			0x1
	#define JDEC_SOS_CLR			(0x1 << 8)
	#define JDEC_MKR_CLR			(0x1 << 16)

#define RW_JDEC_PRG_MODE		0x150
	#define JDEC_DC_REFINE			1
	#define JDEC_AC_FIRST			2
	#define JDEC_AC_REFINE			3
	#define JDEC_DC_FIRST			4
	#define JDEC_IDCT_OUT			(1<<3)
	#define JDEC_COEFF_OUT			(0<<3)
	#define JDEC_PRG_DEC			(1<<8)
	#define JDEC_EOB_AUTO_FILL		(1<<12)
	#define JDEC_COEF_ZERO_IN		(1<<13)
#define RW_JDEC_AH_AL			0x158
#define RW_JDEC_SS_SE			0x15C
#define RW_JDEC_COEF_ADDR0		0x16C
#define RW_JDEC_COEF_ADDR1		0x170
#define RW_JDEC_COEF_ADDR2		0x174
#define RW_JDEC_WR_ADDR0		0x178
#define RW_JDEC_WR_ADDR1		0x17C
#define RW_JDEC_WR_ADDR2		0x180

#define RW_JDEC_IDCT_WIDTH01		0x18C
#define RW_JDEC_IDCT_WIDTH2_COEF_WIDTH0	0x190
#define RW_JDEC_COEF_WIDTH12		0x194
#define RW_JDEC_MCU_H_SIZE		0x1C0

#define RW_JDEC_Q_TAB_IDX_IN		0x260
	#define JDEC_Q_TAB_IDX_SHIFT		6
#define RW_JDEC_Q_TAB_DAT_IN		0x264
#define RW_JDEC_DEC_MCU_ROW_TRIG	0x2A8
	#define JDEC_DEC_FIRST_ROW		0x1
	#define JDEC_DEC_NEXT_ROW		(0x1 << 8)

#define RW_JDEC_MB_ROW_DEC_SWITCH	0x2AC
	#define JDEC_ROW_DEC_WR_ADDR		0x100
	#define JDEC_ROW_DEC_WR_BANK1_ADDR	0x200
	#define JDEC_ROW_DEC_MCU_LEVEL		0x0
	#define JDEC_ROW_DEC_MCU_ROW_LEVEL	0x1
	#define JDEC_ROW_DEC_PIC_LEVEL		0x3
	#define JDEC_ROW_DEC_SCAN_LEVEL		0x2
#define RW_JDEC_ROW_DEC_COMP0_ADDR	0x2B0
#define RW_JDEC_ROW_DEC_COMP1_ADDR	0x2B4
#define RW_JDEC_ROW_DEC_COMP2_ADDR	0x2B8

#define RW_JDEC_PIC_SIZE		0x2C8
	#define JDEC_PIC_WIDTH_SHIFT		0x10

#define RW_JDEC_DRI			0x2D0
	#define JDEC_DRI_ENABLE			(0x1 << 16)
#define RW_JDEC_COEF_PITCH_0		0x2EC
#define RW_JDEC_COEF_PITCH_1		0x2F0
#define RW_JDEC_COEF_PITCH_2		0x2F4

#define RW_JDEC_NEW_DEC			0x3C4
	#define JDEC_COEF_GEN_NZ_MD_EH		0
	#define JDEC_COEF_GEN_NZ_MD_MC		1
	#define JDEC_MSRAM_PWRSAVE		(0x1 << 8)
	#define JDEC_HSRAM_PWRSAVE		(0x1 << 9)
	#define JDEC_QSRAM_PWRSAVE		(0x1 << 10)
#define RW_JDEC_ERROR_CONCEAL		0x3D0
#define JDEC_EC_COEF_DEC			(0x1 << 0)
#define JDEC_EC_BLK_OVERFLOW			(0x1 << 1)
#define JDEC_EC_RST_MKR				(0x1 << 4)

#define RW_JDEC_HUFF_TAB_SEL		0x3D4
	#define JDEC_DC_TABLE			(0 << 12)
	#define JDEC_AC_TABLE			(1 << 12)
#define RW_JDEC_HUFF_VAL		0x3D8

#define RW_JDEC_BITS_RPTR		0xF04
#define RW_JDEC_BITS_START		0xF08
#define RW_JDEC_BITS_VEND		0xF0C
#define RW_JDEC_SW_WP			0xF1C
#define RW_JDEC_NZ_HIST			0xF24
#define RW_JDEC_X_IN_MCU		0xF28
#define RW_JDEC_Y_IN_MCU		0xF2C

#endif

