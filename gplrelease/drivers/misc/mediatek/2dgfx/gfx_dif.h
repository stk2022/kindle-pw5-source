/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef GFX_DIF_H
#define GFX_DIF_H

#define MA_DIF_HAVE_GFX_SW(u4GfxHwId) \
	(GFX_DifGetData(u4GfxHwId)->u4Config & GFX_HAVE_SW_MOD)
#define MA_DIF_HAVE_GFX_HW8520(u4GfxHwId) \
	(GFX_DifGetData(u4GfxHwId)->u4Config & GFX_HAVE_HW_8520_MOD)
#define MA_DIF_HAVE_GFX_FB(u4GfxHwId) \
	(GFX_DifGetData(u4GfxHwId)->u4Config & GFX_HAVE_FB_MOD)
#define MA_DIF_GFX_SW_MOD_OK(u4GfxHwId) \
	(GFX_DifGetData(u4GfxHwId)->u4ModInit |= GFX_HAVE_SW_MOD)
#define MA_DIF_GFX_HW8520_MOD_OK(u4GfxHwId) \
	(GFX_DifGetData(u4GfxHwId)->u4ModInit |= GFX_HAVE_HW_8520_MOD)
#define MA_DIF_GFX_FB_MOD_OK(u4GfxHwId) \
	(GFX_DifGetData(u4GfxHwId)->u4ModInit |= GFX_HAVE_FB_MOD)

#define GREG_FILE_SIZE          0x6E
#define GFX_ENABLE              1
#define GFX_DISABLE             0
#define GFX_CMD_BUF_CYLIC       0
#define GFX_CMD_BUF_SINGLE      1
#define GFX_SOFT_RESET          3
#define GFX_HW_IDLE             1
#define GFX_HW_BUSY             0
#define GFX_CMD_BUF_32KB        0
#define GFX_CMD_BUF_64KB        1
#define GFX_CMD_BUF_128KB       2
#define GFX_CMD_BUF_256KB       3
#define GFX_ENGINE_FIRE         1

struct MI_DIF_T {
	unsigned int u4Config;
	unsigned int u4GfxMode;
	unsigned int u4ModInit;
	unsigned int *pu4CrBase;
	int i4DifIdle;
	bool fgFlushing;
};

enum E_GFX_REG_T {
	GREG_G_CONFIG = 0x00,	// 0x4000
	GREG_G_STATUS = 0x01,	// 0x4004
	GREG_DRAMQ_STAD = 0x02,	// 0x4008
	GREG_DRAMQ_LEN = 0x03,	// 0x400C
	GREG_G_MODE = 0x04,	// 0x4010
	GREG_RECT_COLOR = 0x05,	// 0x4014
	GREG_SRC_BSAD = 0x06,	// 0x4018
	GREG_DST_BSAD = 0x07,	// 0x401C
	GREG_SRC_XY = 0x08,	// 0x4020
	GREG_DST_XY = 0x09,	// 0x4024
	GREG_SRC_SIZE = 0x0A,	// 0x4028
	GREG_S_OSD_WIDTH = 0x0B,	// 0x402C
	GREG_CLIP_BR = 0x0C,	// 0x4030
	GREG_CLIP_TL = 0x0D,	// 0x4034
	GREG_GRAD_X_DELTA = 0x0E,	// 0x4038
	GREG_GRAD_Y_DELTA = 0x0F,	// 0x403C
	GREG_GRAD_XY_INC = 0x10,	// 0x4040
	GREG_BITBLT_MODE = 0x11,	// 0x4044
	GREG_KEY_DATA0 = 0x12,	// 0x4048
	GREG_KEY_DATA1 = 0x13,	// 0x404C
	GREG_SRCCBCR_BSAD = 0x14,	// 0x4050
	GREG_SRCCBCR_PITCH = 0x15,	// 0x4054
	GREG_DSTCBCR_BSAD = 0x16,	// 0x4058
	GREG_F_COLOR = 0x17,	// 0x405C
	GREG_B_COLOR = 0x18,	// 0x4060
	GREG_COL_TRAN0 = 0x19,	// 0x4064
	GREG_COL_TRAN1 = 0x1A,	// 0x4068
	GREG_COL_TRAN2 = 0x1B,	// 0x406C
	GREG_COL_TRAN3 = 0x1C,	// 0x4070
	GREG_COL_TRAN4 = 0x1D,	// 0x4074
	GREG_COL_TRAN5 = 0x1E,	// 0x4078
	GREG_COL_TRAN6 = 0x1F,	// 0x407C
	GREG_COL_TRAN7 = 0x20,	// 0x4080
	GREG_STR_BLT_H = 0x21,	// 0x4084
	GREG_STR_BLT_V = 0x22,	// 0x4088
	GREG_STR_DST_SIZE = 0x23,	// 0x408C
	GREG_LEGAL_START_ADDR = 0x24,	// 0x4090
	GREG_LEGAL_END_ADDR = 0x25,	// 0x4094
	GREG_DUMMY = 0x26,	// 0x4098
	GREG_ALCOM_LOOP = 0x27,	// 0x409C
	GREG_ROP = 0x28,	// 0x40A0
	GREG_IDX2DIR = 0x29,	// 0x40A4
	GREG_SRC_WBBSAD = 0x2A,	// 0x40A8
	GREG_DST_WBBSAD = 0x2B,	// 0x40AC
	GREG_SRCCBCR_WBBSAD = 0x2C,	// 0x40B0
	GREG_XOR_COLOR = 0x2D,	// 0x40B4
	GREG_VA_MSB = 0x2E,	// 0x40B8
	GREG_RACING_MODE = 0x2F,	// 0x40BC
	GREG_BPCOMP_CFG = 0x30,	// 0x40C0
	GREG_BPCOMP_AD_END = 0x31,	// 0x40C4
	GREG_BPCOMP_DBG1 = 0x32,	// 0x40C8
	GREG_BPCOMP_CHKSUM = 0x33,	// 0x40CC
	GREG_BPCOMP_STOP_ID_0 = 0x34,	// 0x40D0
	GREG_BPCOMP_STOP_ID_1 = 0x35,	// 0x40D4
	GREG_BPCOMP_STOP_ID_2 = 0x36,	// 0x40D8
	GREG_BPCOMP_STOP_ID_3 = 0x37,	// 0x40DC
	GREG_0x40E0 = 0x38,	// 0x40E0:BPCOMP_PACKET
	GREG_0x40E4 = 0x39,	// 0x40E4:BPCOMP_OFFSET1
	GREG_0x40E8 = 0x3A,	// 0x40E8:BPCOMP_OFFSET2
	GREG_0x40EC = 0x3B,	// 0x40EC:BPCOMP_INDEX_BASD
	GREG_0x40F0 = 0x3C,	// 0x40F0:reserved?
	GREG_0x40F4 = 0x3D,	// 0x40F4:reserved?
	GREG_0x40F8 = 0x3E,	// 0x40F8:reserved?
	GREG_0x40FC = 0x3F,	// 0x40FC:reserved?
	GREG_0x4100 = 0x40,	// 0x4100:reserved?
	GREG_BMP_STATUS = 0x41,	// 0x4104:reserved?
	GREG_G_256CPT = 0x42,	// 0x4108
	GREG_0x410C = 0x43,	// 0x410C:reserved?
	GREG_0x4110 = 0x44,	// 0x4110:reserved?
	GREG_0x4114 = 0x45,	// 0x4114:reserved?
	GREG_0x4118 = 0x46,	// 0x4118:reserved?
	GREG_0x411C = 0x47,	// 0x411C:reserved?
	GREG_0x4120 = 0x48,	// 0x4120:reserved?
	GREG_G_PGIG = 0x49,	// 0x4124
	GREG_BMP_SIZE = 0x4A,	// 0x4128:reserved?
	GREG_HDMV_IDX_256 = 0x4B,	// 0x412C
	GREG_SRC_RING = 0x4C,	// 0x4130:reserved?
	GREG_DST_RING = 0x4D,	// 0x4134:reserved?
	GREG_CPT_ADDR_RD = 0x4E,	// 0x4138
	GREG_CPT_DATA_RD = 0x4F,	// 0x413C
	GREG_0x4140 = 0x50,	// 0x4140
	GREG_0x4144 = 0x51,	// 0x4144
	GREG_0x4148 = 0x52,	// 0x4148
	GREG_0x414C = 0x53,	// 0x414C
	GREG_0x4150 = 0x54,	// 0x4150
	GREG_0x4154 = 0x55,	// 0x4154
	GREG_0x4158 = 0x56,	// 0x4158
	GREG_0x415C = 0x57,	// 0x415C
	GREG_0x4160 = 0x58,	// 0x4160
	GREG_0x4164 = 0x59,	// 0x4164
	GREG_0x4168 = 0x5A,	// 0x4168
	GREG_0x416C = 0x5B,	// 0x416C
	GREG_0x4170 = 0x5C,	// 0x4170
	GREG_0x4174 = 0x5D,	// 0x4174
	GREG_0x4178 = 0x5E,	// 0x4178
	GREG_0x417C = 0x5F,	// 0x417C
	GREG_IOMMU_CFG0 = 0x60,	// 0x4180
	GREG_IOMMU_CFG1 = 0x61,	// 0x4184
	GREG_IOMMU_CFG2 = 0x62,	// 0x4188
	GREG_IOMMU_CFG3 = 0x63,	// 0x418C
	GREG_IOMMU_CFG4 = 0x64,	// 0x4190
	GREG_IOMMU_CFG5 = 0x65,	// 0x4194
	GREG_IOMMU_CFG6 = 0x66,	// 0x4198
	GREG_IOMMU_CFG7 = 0x67,	// 0x419C
	GREG_IOMMU_CFG8 = 0x68,	// 0x41A0
	GREG_IOMMU_CFG9 = 0x69,	// 0x41A4
	GREG_IOMMU_CFG10 = 0x6A,	// 0x41A8
	GREG_IOMMU_CFG11 = 0x6B,	// 0x41AC
	GREG_IOMMU_CFG12 = 0x6C,	// 0x41B0
	GREG_IOMMU_CFG13 = 0x6D,	// 0x41B4
	GREG_LAST = 0x6E	// 0x41B8
};

struct MI_DIF_REG_T {
	unsigned int u4_G_CONFIG;	// 0x4000
	unsigned int u4_G_STATUS;	// 0x4004
	unsigned int u4_DRAMQ_STAD;	// 0x4008
	unsigned int u4_DRAMQ_LEN;	// 0x400C
	unsigned int u4_G_MODE;	// 0x4010
	unsigned int u4_RECT_COLOR;	// 0x4014
	unsigned int u4_SRC_BSAD;	// 0x4018
	unsigned int u4_DST_BSAD;	// 0x401C
	unsigned int u4_SRC_XY;	// 0x4020
	unsigned int u4_DST_XY;	// 0x4024
	unsigned int u4_SRC_SIZE;	// 0x4028
	unsigned int u4_S_OSD_WIDTH;	// 0x402C
	unsigned int u4_CLIP_BR;	// 0x4030
	unsigned int u4_CLIP_TL;	// 0x4034
	unsigned int u4_GRAD_X_DELTA;	// 0x4038
	unsigned int u4_GRAD_Y_DELTA;	// 0x403C
	unsigned int u4_GRAD_XY_INC;	// 0x4040
	unsigned int u4_BITBLT_MODE;	// 0x4044
	unsigned int u4_KEY_DATA0;	// 0x4048
	unsigned int u4_KEY_DATA1;	// 0x404C
	unsigned int u4_SRCCBCR_BSA;	// 0x4050
	unsigned int u4_SRCCBCR_PITC;	// 0x4054
	unsigned int u4_DSTCBCR_BSA;	// 0x4058
	unsigned int u4_F_COLOR;	// 0x405C
	unsigned int u4_B_COLOR;	// 0x4060
	unsigned int u4_COL_TRAN0;	// 0x4064
	unsigned int u4_COL_TRAN1;	// 0x4068
	unsigned int u4_COL_TRAN2;	// 0x406C
	unsigned int u4_COL_TRAN3;	// 0x4070
	unsigned int u4_COL_TRAN4;	// 0x4074
	unsigned int u4_COL_TRAN5;	// 0x4078
	unsigned int u4_COL_TRAN6;	// 0x407C
	unsigned int u4_COL_TRAN7;	// 0x4080
	unsigned int u4_STR_BLT_H;	// 0x4084
	unsigned int u4_STR_BLT_V;	// 0x4088
	unsigned int u4_STR_DST_SIZE;	// 0x408C
	unsigned int u4_LEGAL_START_ADDR;	// 0x4090
	unsigned int u4_LEGAL_END_ADDR;	// 0x4094
	unsigned int u4_DUMMY;	// 0x4098
	unsigned int u4_ALCOM_LOOP;	// 0x409C
	unsigned int u4_ROP;	// 0x40A0
	unsigned int u4_IDX2DIR;	// 0x40A4
	unsigned int u4_SRC_WBBSAD;	// 0x40A8
	unsigned int u4_DST_WBBSAD;	// 0x40AC
	unsigned int u4_SRCCBCR_WBBSAD;	// 0x40B0
	unsigned int u4_Res40B4;	// 0x40B4
	unsigned int u4_VA_MSB;	// 0x40B8
	unsigned int u4_RACING_MODE;	// 0x40BC
	unsigned int u4_BPCOMP_CFG;	// 0x40C0
	unsigned int u4_BPCOMP_AD_END;	// 0x40C4
	unsigned int u4_Res40C8;	// 0x40C8
	unsigned int u4_Res40CC;	// 0x40CC
	unsigned int u4_Res40D0;	// 0x40D0
	unsigned int u4_Res40D4;	// 0x40D4
	unsigned int u4_Res40D8;	// 0x40D8
	unsigned int u4_Res40DC;	// 0x40DC
	unsigned int u4_Res40E0;	// 0x40E0
	unsigned int u4_Res40E4;	// 0x40E4
	unsigned int u4_Res40E8;	// 0x40E8
	unsigned int u4_Res40EC;	// 0x40EC
	unsigned int u4_Res40F0;	// 0x40F0
	unsigned int u4_Res40F4;	// 0x40F4
	unsigned int u4_Res40F8;	// 0x40F8
	unsigned int u4_Res40FC;	// 0x40FC
	unsigned int u4_Res4100;	// 0x4100
	unsigned int u4_BMP_STATUS;	// 0x4104
	unsigned int u4_G_256CPT;	// 0x4108
	unsigned int u4_Res410C;	// 0x410C
	unsigned int u4_Res4110;	// 0x4110
	unsigned int u4_Res4114;	// 0x4114
	unsigned int u4_Res4118;	// 0x4118
	unsigned int u4_Res411C;	// 0x411C
	unsigned int u4_Res4120;	// 0x4120
	unsigned int u4_G_PGIG;	// 0x4124
	unsigned int u4_BMP_SIZE;	// 0x4128
	unsigned int u4_GREG_HDMV_IDX_256;	// 0x412C
	unsigned int u4_SRC_RING;	// 0x4130
	unsigned int u4_DST_RING;	// 0x4134
	unsigned int u4_CPT_ADDR_RD;	// 0x4138
	unsigned int u4_CPT_DATA_RD;	// 0x413C
	unsigned int u4_0x4140;	// 0x4140
	unsigned int u4_0x4144;	// 0x4144
	unsigned int u4_0x4148;	// 0x4148
	unsigned int u4_0x414C;	// 0x414C
	unsigned int u4_0x4150;	// 0x4150
	unsigned int u4_0x4154;	// 0x4154
	unsigned int u4_0x4158;	// 0x4158
	unsigned int u4_0x415C;	// 0x415C
	unsigned int u4_0x4160;	// 0x4160
	unsigned int u4_0x4164;	// 0x4164
	unsigned int u4_0x4168;	// 0x4168
	unsigned int u4_0x416C;	// 0x416C
	unsigned int u4_0x4170;	// 0x4170
	unsigned int u4_0x4174;	// 0x4174
	unsigned int u4_0x4178;	// 0x4178
	unsigned int u4_0x417C;	// 0x417C
	unsigned int u4_0x4180;	// 0x4180
	unsigned int u4_0x4184;	// 0x4184
	unsigned int u4_0x4188;	// 0x4188
	unsigned int u4_0x418C;	// 0x418C
	unsigned int u4_0x4190;	// 0x4190
	unsigned int u4_0x4194;	// 0x4194
	unsigned int u4_0x4198;	// 0x4198
	unsigned int u4_0x419C;	// 0x419C
	unsigned int u4_0x41A0;	// 0x41A0
	unsigned int u4_0x41A4;	// 0x41A4
	unsigned int u4_0x41A8;	// 0x41A8
	unsigned int u4_0x41AC;	// 0x41AC
	unsigned int u4_0x41B0;	// 0x41BC
	unsigned int u4_LAST;	// 0x41B0
} MI_DIF_REG_T;

struct MI_DIF_FIELD_T {
	// DWORD - G_CONFIG         (4000h)
	unsigned int fg_EN_DRAMQ:1;
	unsigned int fg_INT_MASK:1;
	unsigned int fg_POST_THRS:2;
	unsigned int fg_CMDFIFO_THRS:2;
	unsigned int fg_REQ_INTVAL:2;
	unsigned int fg_DRAMQ_MODE:1;
	unsigned int fg_SDFIFO_THRS:2;
	unsigned int:5;
	unsigned int fg_SRAM_LP:1;
	unsigned int fg_ENG_LP:1;
	unsigned int:5;
	unsigned int fg_MMU_CMDQ_EN:1;
	unsigned int fg_DYNAMIC_HIGH_PRIORITY:1;
	unsigned int fg_SHORT_CMDQ:1;
	unsigned int:2;
	unsigned int fg_CQ_RST:2;
	unsigned int fg_G_RST:2;

	// DWORD - G_STATUS         (4004h)
	unsigned int fg_IDLE:1;
	unsigned int fg_HWQ_LEN:4;
	unsigned int:3;
	unsigned int fg_VERSION_ID:8;
	unsigned int fg_CURR_Y_LINE:11;
	unsigned int:5;

	// DWORD - DRAMQ_STAD       (4008h)
	unsigned int fg_DRAMQ_BSAD:30;
	unsigned int fg_CYC_SIZE:2;

	// DWORD - DRAMQ_LEN        (400Ch)
	unsigned int:3;
	unsigned int fg_DRAMQ_LEN:15;
	unsigned int:10;
	unsigned int fg_NEW_SW:1;
	unsigned int:3;

	// DWORD - G_MODE           (4010h)
	unsigned int fg_CM:4;
	unsigned int fg_OP_MODE:5;
	unsigned int:2;
	unsigned int fg_FIRE:1;
	unsigned int fg_BURST_EN:1;
	unsigned int fg_BURST_MODE:2;
	unsigned int:1;
	unsigned int fg_DSTOWN:1;
	unsigned int fg_SRCOWN:1;
	unsigned int fg_CHAR_CM:2;
	unsigned int fg_SRC_CM:4;
	unsigned int:1;
	unsigned int fg_MMU_CLKOFF:1;
	unsigned int fg_STATIC_HIGH_PRIORITY:1;
	unsigned int fg_BURST_PROTECT_DIS:1;
	unsigned int fg_DST_YUV_EN:1;
	unsigned int fg_SRC_YUV_EN:1;
	unsigned int fg_DST_WT_EN:1;
	unsigned int fg_SRC_WT_EN:1;

	// DWORD - RECT_COLOR       (4014h)
	unsigned int fg_RECT_COLOR:32;

	// DWORD - SRC_BSAD         (4018h)

	unsigned int fg_SRC_BSAD:32;

	// DWORD - DST_BSAD         (401Ch)

	unsigned int fg_DST_BSAD:32;
	// DWORD - SRC_XY           (4020h)
	unsigned int fg_SRCX:15;
	unsigned int:1;
	unsigned int fg_SRCY:11;
	unsigned int:5;

	// DWORD - DST_XY           (4024h)
	unsigned int fg_DSTX:15;
	unsigned int:1;
	unsigned int fg_DSTY:11;
	unsigned int:5;

	// DWORD - SRC_SIZE         (4028h)
	unsigned int fg_SRC_WIDTH:15;
	unsigned int fg_RL_DEC:1;
	unsigned int fg_SRC_HEIGHT:11;
	unsigned int fg_SRC_CM_2:4;
	unsigned int fg_SRC_HEIGHT_15:1;

	// DWORD - S_OSD_WIDTH      (402Ch)
	unsigned int fg_OSD_WIDTH:16;
	unsigned int fg_SRC_PITCH:16;

	// DWORD - CLIP_BR          (4030h)
	unsigned int fg_CLIP_RIGHT:15;
	unsigned int fg_CLR_ENA:1;
	unsigned int fg_CLIP_BOT:14;
	unsigned int:1;
	unsigned int fg_CLB_ENA:1;

	// DWORD - CLIP_TL          (4034h)
	unsigned int fg_CLIP_LEFT:15;
	unsigned int fg_CLL_ENA:1;
	unsigned int fg_CLIP_TOP:14;
	unsigned int:1;
	unsigned int fg_CLT_ENA:1;

	// DWORD - GRAD_X_DELTA     (4038h)
	unsigned int fg_DELTA_X_C0:8;
	unsigned int fg_DELTA_X_C1:8;
	unsigned int fg_DELTA_X_C2:8;
	unsigned int fg_DELTA_X_C3:8;

	// DWORD - GRAD_Y_DELTA     (403Ch)
	unsigned int fg_DELTA_Y_C0:8;
	unsigned int fg_DELTA_Y_C1:8;
	unsigned int fg_DELTA_Y_C2:8;
	unsigned int fg_DELTA_Y_C3:8;

	// DWORD - GRAD_XY_INC      (4040h)
	unsigned int fg_GRAD_X_PIX_INC:11;
	unsigned int:5;
	unsigned int fg_GRAD_Y_PIX_INC:11;
	unsigned int:3;
	unsigned int fg_GRAD_MODE:2;

	// DWORD - BITBLT_MODE      (4044h)
	unsigned int fg_TRANS_ENA:1;
	unsigned int fg_KEYNOT_ENA:1;
	unsigned int fg_COLCHG_ENA:1;
	unsigned int fg_BITBLT_CLIP_ENA:1;
	unsigned int fg_CFMT_ENA:1;
	unsigned int fg_KEYSDSEL:1;
	unsigned int fg_PRCN_OPT:1;
	unsigned int fg_CLIP_ENA:1;
	unsigned int fg_ALPHA_VALUE:8;
	unsigned int fg_ALCOM_PASS:3;
	unsigned int:4;
	unsigned int fg_DIFF_CM:1;
	unsigned int fg_DSTPITCH_DEC:1;
	unsigned int fg_DST_MIRR_OR:1;
	unsigned int fg_SRCPITCH_DEC:1;
	unsigned int fg_SRC_MIRR_OR:1;
	unsigned int fg_DST_WR_ROTATE:1;
	unsigned int fg_DST_RD_ROTATE:1;
	unsigned int fg_BARREL_ENA:1;
	unsigned int fg_PASS_ALU_ENA:1;

	// DWORD - KEY_DATA0        (4048h)
	unsigned int fg_COLOR_KEY_MIN:32;

	// DWORD - KEY_DATA1        (404Ch)
	unsigned int fg_COLOR_KEY_MAX:32;

	// DWORD - SRCCBCR_BSA      (4050h)
	unsigned int fg_SRCCBCR_BSAD:30;
	unsigned int fg_YC_FMT:2;

	// DWORD - SRCCBCR_PITCH    (4054h)
	unsigned int fg_SRCCBCR_PITCH:16;
	unsigned int fg_VSTD:1;
	unsigned int fg_VSYS:1;
	unsigned int fg_VSCLIP:1;
	unsigned int fg_FLD_PIC:1;
	unsigned int fg_SWAP_MODE:2;
	unsigned int fg_SWAP_NEW_MODE:2;
	unsigned int fg_SRC2_BSAD_ENA:1;
	unsigned int:3;
	unsigned int fg_SRC2_CM:4;

	// DWORD - DSTCBCR_BSAD     (4058h)
	unsigned int fg_DSTCBCR_BSAD:28;
	unsigned int:4;

	// DWORD - F_COLOR (1)      (405Ch)
	unsigned int fg_FORE_COLOR:32;

	// DWORD - B_COLOR (0)      (4060h)
	unsigned int fg_BACK_COLOR:32;

	// DWORD - COL_TRAN0        (4064h)
	unsigned int fg_COLOR_TRANS0:32;

	// DWORD - COL_TRAN1        (4068h)
	unsigned int fg_COLOR_TRANS1:32;

	// DWORD - COL_TRAN2        (406Ch)
	unsigned int fg_COLOR_TRANS2:32;

	// DWORD - COL_TRAN3        (4070h)
	unsigned int fg_COLOR_TRANS3:32;

	// DWORD - COL_TRAN4        (4074h)
	unsigned int fg_COLOR_TRANS4:32;

	// DWORD - COL_TRAN5        (4078h)
	unsigned int fg_COLOR_TRANS5:32;

	// DWORD - COL_TRAN6        (407Ch)
	unsigned int fg_COLOR_TRANS6:32;

	// DWORD - COL_TRAN7        (4080h)
	unsigned int fg_COLOR_TRANS7:32;

	// DWORD - STR_BLT_H        (4084h)
	unsigned int fg_STR_BLT_H:24;
	unsigned int:8;

	// DWORD - STR_BLT_V        (4088h)
	unsigned int fg_STR_BLT_V:24;
	unsigned int:8;

	// DWORD - STR_DST_SIZE     (408Ch)
	unsigned int fg_STR_DST_WIDTH:15;
	unsigned int:1;
	unsigned int fg_STR_DST_HEIGHT:11;
	unsigned int:5;

	// DWORD - LEGAL_START_ADDR (4090h)
	unsigned int fg_LEGAL_AD_START:30;
	unsigned int:1;
	unsigned int fg_WR_PROT_EN:1;

	// DWORD - LEGAL_END_ADDR   (4094h)
	unsigned int fg_LEGAL_AD_END:30;
	unsigned int:2;

	// DWORD - DUMMY            (4098h)
	unsigned int fg_DUMMY:32;

	// DWORD - ALCOM_LOOP       (409C)
	unsigned int fg_ALCOM_AR:8;
	unsigned int fg_ALCOM_OPCODE:4;
	unsigned int:4;
	unsigned int fg_ALCOM_RECT_SRC:1;
	unsigned int fg_ALCOM_NORMAL:1;
	unsigned int fg_PREMULT_DSTWR_ENA:1;
	unsigned int fg_PREMULT_DSTRD_ENA:1;
	unsigned int fg_PREMULT_SRCRD_ENA:1;
	unsigned int fg_SRC_OVERFLOW_ENA:1;
	unsigned int:10;

	// DWORD - ROP              (40A0)
	unsigned int fg_ROP_OPCODE:4;
	unsigned int fg_SRCALPHA_CHECK:1;
	unsigned int:3;
	unsigned int fg_NO_WR:1;
	unsigned int fg_CMP_FLAG:1;
	unsigned int:6;
	unsigned int fg_YUVRGB_MODE:3;
	unsigned int:11;
	unsigned int fg_COLORIZE_REP:1;
	unsigned int fg_PRE_COLORIZE:1;

	// DWORD - IDX2DIR          (40A4)
	unsigned int fg_PAL_BSAD:30;
	unsigned int fg_MSB_LEFT:1;
	unsigned int fg_LN_ST_BYTE_AL:1;

	// DWORD - SRC_WBBSAD       (40A8)
	unsigned int fg_SRC_WBBSAD:30;
	unsigned int:2;

	// DWORD - DST_WBBSAD       (40AC)
	unsigned int fg_DST_WBBSAD:30;
	unsigned int:2;

	// DWORD - SRCCBCR_WBBSAD   (40B0)
	unsigned int fg_SRCCBCR_WBBSAD:30;
	unsigned int:2;

	// DWORD - XOR COLOR        (40B4)
    /** 8560:
     * ROP mode:3: xor color in set XOR mode.
     * ROP mode:2: [23:16]-Ar, [15:8]-Ag, [7:0]-Ab
     */
	unsigned int fg_JAVA_XOR_COLOR:32;
	// DWORD - DRAMQ_STADMSB -- Virtual Address MSB  (40B8)
	unsigned int:6;
	unsigned int fg_INDEX_BASD_H:2;
	unsigned int fg_BPCOMP_AD_END_H:2;
	unsigned int fg_SRCCBCR_WBBSAD_H:2;
	unsigned int fg_DST_WBBSAD_H:2;
	unsigned int fg_SRC_WBBSAD_H:2;
	unsigned int fg_PAL_BSAD_H:2;
	unsigned int fg_DSTCBCR_BSAD_H:2;
	unsigned int fg_SRCCBCR_BSAD_H:2;
	unsigned int fg_DST_BSAD_H:2;
	unsigned int fg_SRC_BSAD_H:2;
	unsigned int fg_LEGAL_AD_END_H:2;
	unsigned int fg_LEGAL_AD_START_H:2;
	unsigned int fg_DRAMQ_BSAD_H:2;

	// DWORD - RACING_MODE_REGISTER  (40BC)
	unsigned int:18;
	unsigned int fg_RACING_MODE_TEST:1;
	unsigned int fg_OSD3_RIGHT_FLIP_TEST:1;
	unsigned int fg_OSD3_LEFT_FLIP_TEST:1;
	unsigned int fg_OSD2_RIGHT_FLIP_TEST:1;
	unsigned int fg_OSD2_LEFT_FLIP_TEST:1;
	unsigned int fg_OSD3_NEXT_RIGHT_DRAW_CMD:1;
	unsigned int fg_OSD3_NEXT_LEFT_DRAW_CMD:1;
	unsigned int fg_OSD2_NEXT_RIGHT_DRAW_CMD:1;
	unsigned int fg_OSD2_NEXT_LEFT_DRAW_CMD:1;
	unsigned int fg_OSD3_RACING_RIGHT_COMP_EN:1;
	unsigned int fg_OSD3_RACING_LEFT_COMP_EN:1;
	unsigned int fg_OSD2_RACING_RIGHT_COMP_EN:1;
	unsigned int fg_OSD2_RACING_LEFT_COMP_EN:1;
	unsigned int fg_RACING_EN:1;

	// DWORD - BPCOMP_CFG       (40C0)
	unsigned int fg_ROLL_BACK_EN:1;
	unsigned int fg_QUALITY_MODE:2;
	unsigned int fg_LINE_SEPRATE:1;
	unsigned int fg_PIXEL_SEPARATE:1;
	unsigned int fg_DEC_ENA:1;
	unsigned int fg_DEC_MODE:2;
	unsigned int:24;

	// DWORD - BPCOMP_AD_END    (40C4)
	unsigned int fg_BPCOMP_AD_END:30;
	unsigned int fg_Res40C4:2;

	// DWORD - BPCOMP_DBG1      (40C8)
	unsigned int fg_BPCOMP_NIPPLE:26;
	unsigned int:5;
	unsigned int fg_BPCOMP_STPP:1;

	// DWORD - BPCOMP_DBG2      (40CC)
	unsigned int fg_BPCOMP_CHKSUM:32;

	// DWORD - BPCOMP_DBG3_0    (40D0)
	unsigned int fg_BPCOMP_STOP_ID0:32;

	// DWORD - BPCOMP_DBG3_1    (40D4)
	unsigned int fg_BPCOMP_STOP_ID1:32;

	// DWORD - BPCOMP_DBG3_2    (40D8)
	unsigned int fg_BPCOMP_STOP_ID2:32;

	// DWORD - BPCOMP_DBG3_3    (40DC)
	unsigned int fg_BPCOMP_STOP_ID3:32;

	// BPCOMP_PACKET            (40E0)
	unsigned int fg_PACKET_WIDTH:16;
	unsigned int fg_PIXEL_WIDTH:8;
	unsigned int:8;
	// BPCOMP_OFFSET1           (40E4)
	unsigned int fg_PACKET_ACTIVE_WIDTH:15;
	unsigned int:1;
	unsigned int fg_PACKET_X_START:15;
	unsigned int:1;
	// BPCOMP_OFFSET2           (40E8)
	unsigned int fg_LINE_WIDTH:15;
	unsigned int:17;
	// BPCOMP_INDEX_BASD        (40EC)
	unsigned int fg_INDEX_BASD:30;
	unsigned int:2;
	unsigned int fg_Res40F0:32;
	unsigned int fg_Res40F4:32;
	unsigned int fg_Res40F8:32;
	unsigned int fg_Res40FC:32;

	unsigned int fg_Res4100:32;

	// DWORD - BMP_STATUSDX2DIR (4104)
	unsigned int fg_MULTREG_ENA:1;
	unsigned int fg_Res4104:31;

	// DWRD G_256CPT            (4108)
	unsigned int fg_256CPT_FIRE:1;
	unsigned int fg_USE_256CPT:1;
	unsigned int fg_CPT_DRAM_EN:1;
	unsigned int:3;
	unsigned int fg_ALU_ENB:1;
	unsigned int fg_IDX2DIR_EN:1;
	unsigned int:11;
	unsigned int fg_SRC_PITCH_ENA:1;
	unsigned int:3;
	unsigned int fg_ALPHA_EN:1;
	unsigned int:8;

	unsigned int fg_Res410C:32;
	unsigned int fg_Res4110:32;
	unsigned int fg_Res4114:32;
	unsigned int fg_Res4118:32;
	unsigned int fg_Res411C:32;
	unsigned int fg_Res4120:32;

	// DWRD G_PGIG              (4124)
	unsigned int fg_SRC_HEIGHT_19_16:4;
	unsigned int:12;
	unsigned int fg_YR_SEL:2;
	unsigned int fg_UG_SEL:2;
	unsigned int fg_VB_SEL:2;
	unsigned int fg_A_SEL:2;
	unsigned int:8;

	unsigned int fg_Res4128:32;


	// DWRD HDMV_IDX_256        (412C)
	unsigned int fg_DMV_IDX_256:32;

	unsigned int fg_Res4130:32;

	//                          (4134)
	unsigned int fg_Res4134:32;

	// DWRD CPT_ADDR_RD         (4138)
	unsigned int fg_CPT_RD_ADDR:8;
	unsigned int:8;
	unsigned int fg_CPT_RD_EN:1;
	unsigned int:15;

	// DWRD CPT_DATA_RD         (413C)
	unsigned int fg_CPT_DATA_RD:32;

	unsigned int fg_Reg4140:32;
	unsigned int fg_Reg4144:32;
	unsigned int fg_Reg4148:32;
	unsigned int fg_Reg414C:32;
	unsigned int fg_Reg4150:32;
	unsigned int fg_Reg4154:32;
	unsigned int fg_Reg4158:32;
	unsigned int fg_Reg415C:32;
	unsigned int fg_Reg4160:32;
	unsigned int fg_Reg4164:32;
	unsigned int fg_Reg4168:32;
	unsigned int fg_Reg416C:32;
	unsigned int fg_Reg4170:32;
	unsigned int fg_Reg4174:32;
	unsigned int fg_Reg4178:32;
	unsigned int fg_Reg417C:32;

	// mmu mapping register (5f040 ~ 5f06c, 5f070)
	// DWRD  IOMMU_CFG0         (4180)
	unsigned int fg_IOMMU_EN:1;
	unsigned int fg_TLBLAST:1;
	unsigned int fg_AUTODISCARD:1;
	unsigned int fg_MIF_ENTRY:1;
	unsigned int fg_SMT_RESZ:1;
	unsigned int fg_CFG0_BIT5:1;
	unsigned int fg_CFG0_BIT6:1;
	unsigned int fg_CFG0_BIT7:1;
	unsigned int fg_Reg4180:24;

	// DWRD IOMMU_CFG1          (4184)
	unsigned int fg_RG_TTB:32;

	// DWRD IOMMU_CFG2          (4188)
	unsigned int fg_A0_MMU_EN:1;
	unsigned int fg_A0_MID:3;
	unsigned int fg_A0_PREFETCH:1;
	unsigned int fg_A0_2D_PREFETCH:1;
	unsigned int fg_A0_PREFETCH_DEC:1;
	unsigned int:1;
	unsigned int fg_A0_TWO_WAY:1;
	unsigned int fg_A0_MID_2ND:3;
	unsigned int:4;
	unsigned int fg_A1_MMU_EN:1;
	unsigned int fg_A1_MID:3;
	unsigned int fg_A1_PREFETCH:1;
	unsigned int fg_A1_2D_PREFETCH:1;
	unsigned int fg_A1_PREFETCH_DEC:1;
	unsigned int:1;
	unsigned int fg_A1_TWO_WAY:1;
	unsigned int fg_A1_MID_2ND:3;
	unsigned int:4;

	// DWORD - IOMMU_CFG3       (418ch)
	unsigned int fg_A2_MMU_EN:1;
	unsigned int fg_A2_MID:3;
	unsigned int fg_A2_PREFETCH:1;
	unsigned int fg_A2_2D_PREFETCH:1;
	unsigned int fg_A2_PREFETCH_DEC:1;
	unsigned int:1;
	unsigned int fg_A2_TWO_WAY:1;
	unsigned int fg_A2_MID_2ND:3;
	unsigned int:4;
	unsigned int fg_A3_MMU_EN:1;
	unsigned int fg_A3_MID:3;
	unsigned int fg_A3_PREFETCH:1;
	unsigned int fg_A3_2D_PREFETCH:1;
	unsigned int fg_A3_PREFETCH_DEC:1;
	unsigned int:1;
	unsigned int fg_A3_TWO_WAY:1;
	unsigned int fg_A3_MID_2ND:3;
	unsigned int:4;

	// DWORD - IOMMU_CFG4       (4190h)
	unsigned int fg_IOMMU_CFG4:31;
	unsigned int fg_IOMMU_FIRE:1;

	// DWORD - IOMMU_CFG5       (4194h)
	unsigned int fg_IOMMU_CFG5:32;

	// DWORD - IOMMU_CFG6       (4198h)
	unsigned int fg_IOMMU_CFG6:32;

	// DWORD - IOMMU_CFG7       (419ch)
	unsigned int fg_IOMMU_CFG7:32;

	// DWORD - IOMMU_CFG8       (41a0h)
	unsigned int fg_IOMMU_CFG8:32;

	// DWORD - IOMMU_CFG9       (41a4h)
	unsigned int fg_IOMMU_CFG9:32;

	// DWORD - IOMMU_CFG10      (41a8h)
	unsigned int fg_IOMMU_CFG10:32;

	// DWORD - IOMMU_CFG11       (41a4h)
	unsigned int fg_IOMMU_CFG11:32;

	// DWORD - IOMMU_CFG12      (41a8h)
	unsigned int fg_IOMMU_CFG12:32;

	// DWORD - IOMMU_CFG13      (41ach)
	unsigned int fg_IOMMU_CFG13:32;

	// DWORD - LAST            (41b0h)
	unsigned int fg_LAST:32;
} MI_DIF_FIELD_T;

union MI_DIF_UNION_T {
	MI_DIF_REG_T rReg;
	MI_DIF_FIELD_T rField;
} MI_DIF_UNION_T;

enum EGFX_OP_MODE_T {
	OP_TEXT_BLT = 1,
	OP_BITMAP_BLT = 1,
	OP_RECT_FILL = 2,
	OP_DRAW_HLINE = 3,
	OP_DRAW_POINT = 3,
	OP_DRAW_VLINE = 4,
	OP_GRAD_FILL = 5,
	OP_BITBLT = 6,
	OP_DMA = 7,
	OP_1D_BITBLT = 7,
	OP_ALPHA_BITBLT = 8,
	OP_ALPHA_COMPOS_BITBLT = 9,
	OP_YCRCB_RGB_CNV = 10,
	OP_STRETCH_BITBLT = 11,
	OP_ALPHA_MAP_BITBLT = 12,
	OP_LOOP_ALPAH_COMPOS = 13,
	OP_ROP_BITBLT = 14,
	OP_IDX2DIR_BITBLT = 15,
	OP_H2V_LINE = 16,
	OP_OBLIQUE_LINE = 17,
	OP_BPCOMP = 18,
	OP_STRETCH_LOOP_ALPHA_COMPOS = 19,
	OP_STRETCH_ROTATE_LOOP_ALPHA_COMPOS = 20,
	OP_STRETCH_JAVA_XOR = 21,
	OP_STRETCH_ROTATE_JAVA_XOR = 22,
	OP_IDX2DIR_LOOP_ALPHA_COMPOS = 23,
	OP_STRETCH_IDX2DIR_LOOP_ALPHA_COMPOS = 24,
	OP_YCBCR2RGB_ALCOM = 25,
	OP_STRETCH_YCBCR2RGB_ALCOM = 26,
	OP_STRETCH_YCBCR2RGB_JAVAXOR = 27,
};

int GFX_DifInit(unsigned long u4GfxHwId);
int GFX_DifIrqInit(unsigned int irq_gfx0, unsigned int irq_gfx1,
	unsigned long u4GfxHwId);
int GFX_DifUninit(void);
int GFX_DifSetRegBase(unsigned long u4GfxHwId, unsigned int *pu4Base);
int GFX_DifGetRegBase(unsigned long u4GfxHwId, unsigned int *pu4Base);
int GFX_DifReset(unsigned long u4GfxHwId, unsigned int u4Reset);
void GFX_DifSetMode(unsigned long u4GfxHwId, unsigned int u4GfxMode);
int GFX_DifSetCR(unsigned long u4GfxHwId, unsigned int u4CrName,
	unsigned int u4Val);
int GFX_DifGetCR(unsigned long u4GfxHwId, unsigned int u4CrName,
	unsigned int *pu4Val);
int GFX_DifGetIdle(unsigned long u4GfxHwId);
void GFX_DifSetIdle(unsigned long u4GfxHwId, int u4Idle);
MI_DIF_T *GFX_DifGetData(unsigned long u4GfxHwId);
int pfnGFX_DifAction(unsigned long u4GfxHwId);
int pfnGFX_DifGetInternalIdle(unsigned long u4GfxHwId);
void pfnGFX_DifWait(unsigned long u4GfxHwId);
#endif
