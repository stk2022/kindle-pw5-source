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



#ifndef GFX_IF_H
#define GFX_IF_H

#include "gfx_hw.h"
#include "gfx_dif.h"
#include "gfx_manager.h"

#if defined(CC_DEBUG)		// debug mode
#define GFX_DEBUG_MODE
#endif

#define  GFX_RISC_MODE
#ifdef GFX_RISC_MODE
#define GFX_SYNC_OLD_METHOD   1
#else
#ifdef GFX_PERFORMANCE_TEST
#define GFX_SYNC_OLD_METHOD   0
#else
#define GFX_SYNC_OLD_METHOD   0
#endif
#endif

#define  GFX_DRV_ALPHA_PIXEL_DATA_NUM	24
#define GFX_SUPPORT_DIFF_CM
#define GFX_DEF_BUF_SIZE	30720
#define GFX_FONT_1BIT	0
#define GFX_FONT_2BIT   1
#define GFX_FONT_4BIT   2
#define GFX_FONT_8BIT   3
#define GFX_HAVE_SW_MOD			(1 << (int)E_GFX_SW_MOD)
#define GFX_HAVE_HW_8520_MOD	(1 << (int)E_GFX_HW_8520_MOD)
#define GFX_HAVE_FB_MOD			(1 << (int)E_GFX_FB_MOD)
#define GFX_RESET_ENGINE		0xC0
#define GFX_RESET_CMDQUE		0x30
#define GFX_RESET_BOTH			0xF0
#define GFX_RESET_POWERDOWN		0x10
#define GFX_IDX2DIR_LN_ST_BYTE_AL	1
#define GFX_IDX2DIR_MSB_LEFT		1
#define D_GFXFLAG_TRANSPARENT	(1 << (int)E_GFXBLT_TRANSPARENT)
#define D_GFXFLAG_KEYNOT		(1 << (int)E_GFXBLT_KEYNOT)
#define D_GFXFLAG_COLORCHANGE	(1 << (int)E_GFXBLT_COLORCHANGE)
#define D_GFXFLAG_CLIP			(1 << (int)E_GFXBLT_CLIP)
#define D_GFXFLAG_CFMT_ENA		(1 << (int)E_GFXBLT_CFMT_ENA)
#define D_GFXFLAG_KEYSDSEL		(1 << (int)E_GFXBLT_KEYSDSEL)
#define D_GFXFLAG_NONE			(0)
#define GFX_UNUSED_RET(X)
#define VERIFY(X)
#define UNUSEDGFX(X)
#define GFX_HW_FX_ENTRY

struct gfx_device {
	unsigned int irq_gfx0;
	unsigned int irq_gfx1;
	void __iomem *GFX0_DIST_BASE;
	void __iomem *GFX1_DIST_BASE;
	struct clk *infra_gfx;
} gfx_device;

struct _GFX_REGFILE {
	unsigned int au4RegFile[GREG_FILE_SIZE];
	MI_DIF_UNION_T *prRegFile;
} GFX_REGFILE;

#define GFX_BUFFER_ERROR_INTERNAL  -1
#define GFX_BUFFER_OK	0
#define GFX_STRETCH_SCALE	128
#if defined(CC_DEBUG)		// debug mode
#define GFX_DEBUG_MODE
#endif

#if defined(CC_MINI_DRIVER)
#define GFX_MINI_DRIVER
    //#define GFX_RISC_MODE
#endif

#define GFX_FONT_1BIT           0
#define GFX_FONT_2BIT           1
#define GFX_FONT_4BIT           2
#define GFX_FONT_8BIT           3

#define GFX_HAVE_SW_MOD         (1 << (int)E_GFX_SW_MOD)
#define GFX_HAVE_HW_8520_MOD    (1 << (int)E_GFX_HW_8520_MOD)
#define GFX_HAVE_FB_MOD         (1 << (int)E_GFX_FB_MOD)

#define GFX_RESET_ENGINE        0xC0
#define GFX_RESET_CMDQUE        0x30
#define GFX_RESET_BOTH          0xF0
#define GFX_RESET_POWERDOWN  0x10

#define GFX_IDX2DIR_LN_ST_BYTE_AL   1
#define GFX_IDX2DIR_MSB_LEFT        1


#define D_GFXFLAG_TRANSPARENT   (1 << (int)E_GFXBLT_TRANSPARENT)
#define D_GFXFLAG_KEYNOT        (1 << (int)E_GFXBLT_KEYNOT)
#define D_GFXFLAG_COLORCHANGE   (1 << (int)E_GFXBLT_COLORCHANGE)
#define D_GFXFLAG_CLIP          (1 << (int)E_GFXBLT_CLIP)
#define D_GFXFLAG_CFMT_ENA      (1 << (int)E_GFXBLT_CFMT_ENA)
#define D_GFXFLAG_KEYSDSEL      (1 << (int)E_GFXBLT_KEYSDSEL)
#define D_GFXFLAG_NONE          (0)


#define GFX_UNUSED_RET(X)
#define VERIFY(X)
#define UNUSEDGFX(X)
#define GFX_HW_FX_ENTRY


static const unsigned char _auColorModeShift[] = {
	2,
	1,			// CM_YCbCr_CLUT4
	0,			// CM_YCbCr_CLUT8
	0,			// CM_Reserved0
	1,			// CM_CbYCrY422_DIRECT16
	1,			// CM_YCbYCr422_DIRECT16
	2,			// CM_AYCbCr8888_DIRECT32
	0,			// CM_Reserved1
	2,			// CM_RGB_CLUT2
	1,			// CM_RGB_CLUT4
	0,			// CM_RGB_CLUT8
	1,			// CM_RGB565_DIRECT16
	1,			// CM_ARGB1555_DIRECT16
	1,			// CM_ARGB4444_DIRECT16
	2,			// CM_ARGB8888_DIRECT32
	0			// CM_Reserved2
};

enum EGFX_COLOR_MODE_T {
	CM_YCbCr_CLUT2 = 0,
	CM_YCbCr_CLUT4,
	CM_YCbCr_CLUT8,
	CM_Reserved0,
	CM_CbYCrY422_DIRECT16,
	CM_YCbYCr422_DIRECT16,
	CM_AYCbCr8888_DIRECT32,
	CM_Reserved1,
	CM_RGB_CLUT2,
	CM_RGB_CLUT4,
	CM_RGB_CLUT8,
	CM_RGB565_DIRECT16,
	CM_ARGB1555_DIRECT16,
	CM_ARGB4444_DIRECT16,
	CM_ARGB8888_DIRECT32,
	CM_Reserved2,
	CM_SC_SINGLE
};

/* GFX alpha composition mode */
enum EGFX_AC_MODE_T {
	E_AC_CLEAR = 0,
	E_AC_DST_IN,
	E_AC_DST_OUT,
	E_AC_DST_OVER,
	E_AC_SRC,
	E_AC_SRC_IN,
	E_AC_SRC_OUT,
	E_AC_SRC_OVER,
	E_AC_DST,
	E_AC_SRC_ATOP,
	E_AC_DST_ATOP,
	E_AC_XOR,
	E_AC_MAX
};

enum EGFX_BITBLT_OPT_T {
	E_BITBLT_SRC_MIRROR = 0,
	E_BITBLT_SRC_FLIP,
	E_BITBLT_DST_MIRROR,
	E_BITBLT_DST_FLIP,
	E_BITBLT_SRC_FLIPMIRROR,
	E_BITBLT_DST_FLIPMIRROR,
	E_BITBLT_NORMAL,
	E_BITBLT_MF_MAX
};

enum EGFX_BLT_OPT_T {
	E_GFXBLT_TRANSPARENT = 0,
	E_GFXBLT_KEYNOT,
	E_GFXBLT_COLORCHANGE,
	E_GFXBLT_CLIP,
	E_GFXBLT_CFMT_ENA,
	E_GFXBLT_KEYSDSEL
};

enum EGFX_VIDSTD_T {
	E_VSTD_BT601 = 0,
	E_VSTD_BT709
};

enum EGFX_VSYS_T {
	E_VSYS_VID = 0,
	E_VSYS_COMP
};

enum EGFX_SWAP_MODE_T {
	E_SWAP_0 = 0,
	E_SWAP_MERGETOP = 0,
	E_SWAP_1 = 1,
	E_SWAP_SWAP = 1,
	E_SWAP_2 = 2,
	E_SWAP_BLOCK = 2,
	E_SWAP_DEF = 2
};

enum EGFX_YCFMT_T {
	E_YCFMT_420MB = 0,
	E_YCFMT_420LINEAR,
	E_YCFMT_422LINEAR,
	E_YCFMT_RES_3
};

enum EGFX_BMP_COLOR_MODE_T {
	E_BMP_CM_1BIT = 0,
	E_BMP_CM_2BIT,
	E_BMP_CM_4BIT,
	E_BMP_CM_8BIT
};

enum EGFX_GRAD_MODE_T {
	E_GRAD_RESERVED = 0,
	E_GRAD_HOR,
	E_GRAD_VER,
	E_GRAD_BOTH
};

enum E_GFX_MODULE {
	E_GFX_SW_MOD = 0,
	E_GFX_HW_8520_MOD,
	E_GFX_FB_MOD,
	E_GFX_MODULE_LAST
};

enum EGFX_ROP_MODE_T {
	E_ROP_RESERVED0 = 0,
	E_ROP_RESERVED1,
	E_ROP_COLORIZE,
	E_ROP_RESERVED3,
	E_ROP_JAVA_XOR = E_ROP_RESERVED3,
	E_ROP_NOT_SRC,
	E_ROP_NOT_DST,
	E_ROP_SRC_XOR_DST,
	E_ROP_SRC_XNOR_DST,
	E_ROP_SRC_AND_DST,
	E_ROP_NOT_SRC_AND_DST,
	E_ROP_SRC_AND_NOT_DST,
	E_ROP_NOT_SRC_AND_NOT_DST,
	E_ROP_SRC_OR_DST,
	E_ROP_NOT_SRC_OR_DST,
	E_ROP_SRC_OR_NOT_DST,
	E_ROP_NOT_SRC_OR_NOT_DST,
	E_ROP_MAX
};

enum EGFX_IDX2DIR_OPT_T {
	E_IDX2DIR_LN_ST_BYTE_AL_OFF = 0,
	E_IDX2DIR_LN_ST_BYTE_AL_ON = 1,
	E_IDX2DIR_MSB_LEFT_OFF = 0,
	E_IDX2DIR_MSB_LEFT_ON = 1
};

enum GFX_PREMULTIPLIED_CNV_TYPE {
	E_NOT_PREMULTIPLIED_2_PREMULTIPLIED = 0,
	E_PREMULTIPLIED_2_NON_PREMULTIPLIED = 1
};

enum GFX_DECODE_TYPE {
	E_DECODE_FULL_PITCH = 1,
	E_DECODE_MASK = 1 << 1,
	E_DECODE_OFFSET = 1 << 2
};

int i4_gfx_resume(void);
int i4_gfx_suspend(void);

extern int Gfx_RegFileInit(void);
int Gfx_cmdq_init(void);
extern int GFX_Reset(unsigned long u4GfxHwId, unsigned int u4Reset);
extern int GFX_GetIdle(unsigned long u4GfxHwId);
extern void GFX_Wait(unsigned long u4GfxHwId);
extern int GFX_Flush(unsigned long u4GfxHwId);
extern void GFX_LockCmdque(unsigned long u4GfxHwId);
extern void GFX_UnlockCmdque(unsigned long u4GfxHwId);
extern int GFX_QueryHwIdle(unsigned long u4GfxHwId);
extern int GFX_SetDst(unsigned long u4GfxHwId, unsigned int *pu1Base,
	unsigned int u4ColorMode, unsigned int u4Pitch);
extern int GFX_SetSrc(unsigned long u4GfxHwId, unsigned int *pu1Base,
	unsigned int u4ColorMode, unsigned int u4Pitch);
extern int GFX_Set2ndSrc(unsigned long u4GfxHwId, unsigned int *pu1Base,
	unsigned int u4ColorMode, unsigned int u4Pitch);
extern int GFX_Set2ndSrcEnable(unsigned long u4GfxHwId,
	unsigned int u4ColorMode);
extern int GFX_SetCharSrcBase(unsigned long u4GfxHwId,
	unsigned int *pu1Base, unsigned int u4ColorMode,
	unsigned int u4Pitch);
extern int GFX_SetAlpha(unsigned long u4GfxHwId, unsigned int u4Alpha);
extern int GFX_SetColor(unsigned long u4GfxHwId, unsigned int u4Color);
extern void GFX_SetReqInterval(unsigned long u4GfxHwId,
	unsigned int u4ReqInterval);
extern int GFX_Fill(unsigned long u4GfxHwId, unsigned int u4X,
	unsigned int u4Y, unsigned int u4Width, unsigned int u4Height);
extern int GFX_Draw(unsigned long u4GfxHwId, unsigned int u4X,
	unsigned int u4Y, unsigned int u4Width, unsigned int u4Height);
extern int GFX_DrawCompose(unsigned long u4GfxHwId, unsigned int u4X,
	unsigned int u4Y, unsigned int u4Width, unsigned int u4Height,
	unsigned int u4Ar, unsigned int u4OpCode, unsigned int u4RectSrc);
extern int GFX_FillTriangle(unsigned long u4GfxHwId, unsigned int u4X1,
	unsigned int u4Y1, unsigned int u4X2, unsigned int u4Y2,
	unsigned int u4X3, unsigned int u4Y3);
extern int GFX_FillTriangleCompose(unsigned long u4GfxHwId,
	unsigned int u4X1, unsigned int u4Y1, unsigned int u4X2,
	unsigned int u4Y2, unsigned int u4X3, unsigned int u4Y3,
	unsigned int u4Ar, unsigned int u4OpCode, unsigned int u4RectSrc);
extern int GFX_FillChar(void *pvBase, unsigned int u1Char,
	unsigned int u4NumWrd);
extern int GFX_HLine(unsigned long u4GfxHwId, unsigned int u4X,
	unsigned int u4Y, unsigned int u4Width);
extern int GFX_VLine(unsigned long u4GfxHwId, unsigned int u4X,
	unsigned int u4Y, unsigned int u4Height);
extern int GFX_ObliqueLine(unsigned long u4GfxHwId,
	unsigned int u4SrcX, unsigned int u4SrcY,
	unsigned int u4DstX, unsigned int u4DstY);
extern int GFX_SetBltOpt(unsigned long u4GfxHwId,
	unsigned int u4Switch,
	unsigned int u4ColorMin, unsigned int u4ColorMax);
extern int GFX_BitBlt(unsigned long u4GfxHwId, unsigned int u4SrcX,
	unsigned int u4SrcY, unsigned int u4DstX, unsigned int u4DstY,
	unsigned int u4Width, unsigned int u4Height);
extern int GFX_BitBlt_NewMethod(unsigned long u4GfxHwId,
	unsigned int u4SrcX, unsigned int u4SrcY, unsigned int u4DstX,
	unsigned int u4DstY, unsigned int u4Width, unsigned int u4Height);
extern int GFX_Blend(unsigned long u4GfxHwId, unsigned int u4SrcX,
	unsigned int u4SrcY, unsigned int u4DstX, unsigned int u4DstY,
	unsigned int u4Width, unsigned int u4Height);
extern int GFX_SetBuf(unsigned long u4GfxHwId,
	unsigned int *pu4GfxWorkingBuf, unsigned int u4Size);
extern int GFX_ComposeLoop(unsigned long u4GfxHwId,
	unsigned int u4SrcX, unsigned int u4SrcY, unsigned int u4DstX,
	unsigned int u4DstY, unsigned int u4Width, unsigned int u4Height,
	unsigned int u4Ar, unsigned int u4OpCode, unsigned int u4RectSrc);
extern int GFX_NormalComposeLoop(unsigned long u4GfxHwId,
	unsigned int u4SrcX, unsigned int u4SrcY, unsigned int u4DstX,
	unsigned int u4DstY, unsigned int u4Width, unsigned int u4Height,
	unsigned int u4Ar, unsigned int u4OpCode, unsigned int u4RectSrc);
extern int GFX_2SrcBlendingBitblt(unsigned long u4GfxHwId,
	unsigned int u4SrcX, unsigned int u4SrcY, unsigned int u4DstX,
	unsigned int u4DstY, unsigned int u4Width, unsigned int u4Height,
	unsigned int u4Ar, unsigned int u4OpCode, unsigned int u4RectSrc,
	unsigned int u4Src2En);
extern int GFX_ComposeLoopEx(unsigned long u4GfxHwId,
	unsigned int u4SrcX, unsigned int u4SrcY, unsigned int u4DstX,
	unsigned int u4DstY, unsigned int u4Width, unsigned int u4Height,
	unsigned int u4AlComNormal, unsigned int u4Ar, unsigned int u4OpCode,
	unsigned int u4RectSrc);
extern int GFX_StretchRotate(unsigned long u4GfxHwId,
	unsigned int u4SrcX, unsigned int u4SrcY, unsigned int u4SrcW,
	unsigned int u4SrcH, unsigned int u4DstX, unsigned int u4DstY,
	unsigned int u4DstW, unsigned int u4DstH, unsigned int u4Is90CCW);
extern int GFX_StretchMFlip(unsigned long u4GfxHwId,
	unsigned int u4SrcX,
	unsigned int u4SrcY, unsigned int u4SrcW, unsigned int u4SrcH,
	unsigned int u4DstX, unsigned int u4DstY, unsigned int u4DstW,
	unsigned int u4DstH, unsigned int u4MFOp);
extern int GFX_AlphaComposeLoopRotate(unsigned long u4GfxHwId,
	unsigned int u4SrcX, unsigned int u4SrcY, unsigned int u4DstX,
	unsigned int u4DstY, unsigned int u4Width, unsigned int u4Height,
	unsigned int u4Ar, unsigned int u4OpCode, unsigned int u4RectSrc,
	unsigned int u4AlcomNormal, unsigned int u4Is90CCW);
extern int GFX_AlphaComposeLoopMFlip(unsigned long u4GfxHwId,
	unsigned int u4SrcX, unsigned int u4SrcY, unsigned int u4DstX,
	unsigned int u4DstY, unsigned int u4Width, unsigned int u4Height,
	unsigned int u4Ar, unsigned int u4OpCode, unsigned int u4RectSrc,
	unsigned int u4AlcomNormal, unsigned int u4MFOp);
extern int GFX_AlphaComposeLoopRotateMFlip(unsigned long u4GfxHwId,
	unsigned int u4SrcX, unsigned int u4SrcY, unsigned int u4DstX,
	unsigned int u4DstY, unsigned int u4Width, unsigned int u4Height,
	unsigned int u4Ar, unsigned int u4OpCode, unsigned int u4RectSrc,
	unsigned int u4AlcomNormal, unsigned int u4Is90CCW,
	unsigned int u4MFOp);
extern int GFX_Compose(unsigned long u4GfxHwId,
	unsigned int u4SrcX, unsigned int u4SrcY,
	unsigned int u4DstX, unsigned int u4DstY, unsigned int u4Width,
	unsigned int u4Height, unsigned int u4Ar, unsigned int u4Mode);
extern int GFX_AlphaComposePass(unsigned long u4GfxHwId,
	unsigned int u4SrcX, unsigned int u4SrcY,
	unsigned int u4DstX, unsigned int u4DstY, unsigned int u4Width,
	unsigned int u4Height, unsigned int u4Pass, unsigned int u4Param);
extern int GFX_NormalComposePass(unsigned long u4GfxHwId,
	unsigned int u4SrcX, unsigned int u4SrcY, unsigned int u4DstX,
	unsigned int u4DstY, unsigned int u4Width, unsigned int u4Height,
	unsigned int u4Pass, unsigned int u4Param);
extern int GFX_SetColCnvFmt(unsigned long u4GfxHwId,
	unsigned int u4YCFmt, unsigned int u4SwapMode,
	unsigned int u4VidStd, unsigned int u4VidSys);
extern int GFX_SetColCnvSrc(unsigned long u4GfxHwId,
	unsigned int *pu1SrcLuma, unsigned int u4SrcLumaPitch,
	unsigned int *pu1SrcChroma, unsigned int u4SrcChromaPitch,
	unsigned int u4FieldPic);
extern int GFX_ColConv(unsigned long u4GfxHwId, unsigned int u4X,
	unsigned int u4Y, unsigned int u4Width, unsigned int u4Height);
extern int GFX_StretchBlt(unsigned long u4GfxHwId,
	unsigned int u4SrcX,
	unsigned int u4SrcY, unsigned int u4SrcW, unsigned int u4SrcH,
	unsigned int u4DstX, unsigned int u4DstY, unsigned int u4DstW,
	unsigned int u4DstH);
extern int GFX_AlphaMapBitBlt(unsigned long u4GfxHwId,
	unsigned int u4SrcX, unsigned int u4SrcY,
	unsigned int u4DstX, unsigned int u4DstY,
	unsigned int u4Width, unsigned int u4Height);
extern int GFX_RopBitblt(unsigned long u4GfxHwId,
	unsigned int u4SrcX,
	unsigned int u4SrcY, unsigned int u4DstX, unsigned int u4DstY,
	unsigned int u4Width, unsigned int u4Height, unsigned int u4RopCode);

extern int GFX_SetHoriToVertLineOpt(unsigned long u4GfxHwId,
	unsigned int u4IsCounterClockWise);
extern int GFX_HoriToVertLine(unsigned long u4GfxHwId,
	unsigned int u4SrcX, unsigned int u4SrcY,
	unsigned int u4DstX, unsigned int u4DstY,
	unsigned int u4HoriLineWidth, int u4Is90CCW);
extern int GFX_RotateBmp(unsigned long u4GfxHwId,
	unsigned int *pu1SrcBase,
	unsigned int *pu1DstBase, unsigned int u4SrcX, unsigned int u4SrcY,
	unsigned int u4DstX, unsigned int u4DstY, unsigned int u4CM,
	unsigned int u4SrcPitch, unsigned int u4DstPitch,
	unsigned int u4Width,
	unsigned int u4Height, unsigned int u4Is90CCW);
extern int GFX_RotateMirrorFlipBmp(unsigned long u4GfxHwId,
	unsigned int *pu1SrcBase, unsigned int *pu1DstBase,
	unsigned int u4SrcX, unsigned int u4SrcY, unsigned int u4DstX,
	unsigned int u4DstY, unsigned int u4CM, unsigned int u4SrcPitch,
	unsigned int u4DstPitch, unsigned int u4Width,
	unsigned int u4Height, unsigned int u4Is90CCW,
	unsigned char u1AddOpt);
extern int GFX_RotateBmpSw(unsigned int *pu1SrcBase,
	unsigned int *pu1DstBase, unsigned int u4SrcX,
	unsigned int u4SrcY, unsigned int u4DstX, unsigned int u4DstY,
	unsigned int u4CM, unsigned int u4SrcPitch, unsigned int u4DstPitch,
	unsigned int u4Width, unsigned int u4Height, unsigned int u4Is90CCW);
extern int GFX_SetGradOpt(unsigned long u4GfxHwId,
	unsigned int u4IncX, unsigned int u4IncY,
	const unsigned int asu4DeltaX[4],
	const unsigned int asu4DeltaY[4]);
extern int GFX_GradFill(unsigned long u4GfxHwId, unsigned int u4X,
	unsigned int u4Y, unsigned int u4Width, unsigned int u4Height,
	unsigned int u4Mode);
int GFX_SetClipOpt(unsigned long u4GfxHwId, unsigned int u4ClipEnMask,
	unsigned int u4DstX, unsigned int u4DstY, unsigned int u4ClipTop,
	unsigned int u4ClipBot, unsigned int u4ClipLeft,
	unsigned int u4ClipRight);
int GFX_BurstEn(unsigned long u4GfxHwId, unsigned int u4BurstEn,
	unsigned int u4BurstMode);
int GFX_StretchComposeLoop(unsigned long u4GfxHwId,
	unsigned int u4SrcX,
	unsigned int u4SrcY, unsigned int u4SrcW, unsigned int u4SrcH,
	unsigned int u4DstX, unsigned int u4DstY, unsigned int u4DstW,
	unsigned int u4DstH, unsigned int u4AlComNormal,
	unsigned int u4Ar, unsigned int u4OpCode,
	unsigned int u4RectSrc);
int GFX_StretchRotateComposeLoop(unsigned long u4GfxHwId,
	unsigned int u4SrcX,
	unsigned int u4SrcY, unsigned int u4SrcW, unsigned int u4SrcH,
	unsigned int u4DstX, unsigned int u4DstY, unsigned int u4DstW,
	unsigned int u4DstH, unsigned int u4Is90CCW,
	unsigned int u4AlComNormal, unsigned int u4Ar,
	unsigned int u4OpCode, unsigned int u4RectSrc);
int GFX_StretchMFlipComposeLoop(unsigned long u4GfxHwId,
	unsigned int u4SrcX,
	unsigned int u4SrcY, unsigned int u4SrcW, unsigned int u4SrcH,
	unsigned int u4DstX, unsigned int u4DstY, unsigned int u4DstW,
	unsigned int u4DstH, unsigned int u4MFOp,
	unsigned int u4AlComNormal, unsigned int u4Ar,
	unsigned int u4OpCode, unsigned int u4RectSrc);
int GFX_StretchRotateMFlipComposeLoop(unsigned long u4GfxHwId,
	unsigned int u4SrcX, unsigned int u4SrcY, unsigned int u4SrcW,
	unsigned int u4SrcH, unsigned int u4DstX, unsigned int u4DstY,
	unsigned int u4DstW, unsigned int u4DstH, unsigned int u4Is90CCW,
	unsigned int u4MFOp, unsigned int u4AlComNormal,
	unsigned int u4Ar, unsigned int u4OpCode,
	unsigned int u4RectSrc);
int GFX_Reset_Engine(unsigned long u4GfxHwId);
int GFX_Reset_CmdQue(unsigned long u4GfxHwId);
void GFX_QueryCmdQueInfo(unsigned long u4GfxHwId);
int GFX_SetLegalAddress(unsigned long u4GfxHwId,
	unsigned int u4Start, unsigned int u4End);
unsigned int GFX_QueryFlushCount(unsigned long u4GfxHwId);
unsigned int GFX_QueryHwInterruptCount(unsigned long u4GfxHwId);
void GFX_SetCqCapacity(unsigned long u4GfxHwId, int i4Capacity);
int GFX_MMUCfgEx(unsigned long u4GfxHwId, unsigned int u4Enable,
	unsigned int u4SrcRead, unsigned int u4DstRead,
	unsigned int u4DstWrite);
int i4GfxSetMmuWaitMode(unsigned int u4Mode);
int i4GfxSetMmuGlbBypss(unsigned int u4Mode);
int GFX_SetMmuWaitMode(unsigned long u4GfxHwId, unsigned int u4Mode);
int i4GfxSetMmuSelfFire(unsigned long u4GfxHwId);
int GFX_SetPreColorize(unsigned long u4GfxHwId,
	unsigned int u4PreColorize,
	unsigned int u4ColorRepEn, unsigned int u4ColorRep);
int Gfx_Rotate90(unsigned long u4GfxHwId, unsigned int u4SrcX,
	unsigned int u4SrcY, unsigned int u4DstX, unsigned int u4DstY,
	unsigned int u4Width, unsigned int u4Height, unsigned int u4Is90CCW);
unsigned int _GfxGetColorMode(GFX_COLORMODE_T rMwColorMode);
int _GfxComposeLoop(unsigned long u4GfxHwId,
	GFX_ALPHA_COMPOSITION_T *prGfxCmd);
int GFX_SetStretchBltOpt(unsigned long u4GfxHwId, unsigned int u4SrcW,
	unsigned int u4SrcH, unsigned int u4DstW, unsigned int u4DstH);
int _GfxBitblt(unsigned long u4GfxHwId, GFX_BITBLT_T *prGfxCmd);
int _GfxFillRect(unsigned long u4GfxHwId, GFX_FILL_T *prGfxCmd);
int _GfxOpModeCheck(unsigned int u4Op, unsigned int u4Src,
	unsigned int u4Dst);
int GFX_SetRotateOpt(unsigned long u4GfxHwId, bool fgRotateEn,
	unsigned int u4Is90CCW);
int GFX_SetIdx2DirOpt(unsigned long u4GfxHwId,
	unsigned int *pu1PaletteBase, unsigned int u4MsbLeft,
	unsigned int u4StartByteAligned);
int GFX_SetMirrorFlipOpt(unsigned long u4GfxHwId, unsigned int u4op);
int GFX_SetI2DBltOpt(unsigned long u4GfxHwId,
	unsigned int u4SrcPitchEn,
	unsigned int *pv_pal_base, unsigned int u4MsbLeft,
	unsigned int u4StartByteAligned);
int GFX_SetComposeLoopOpt(unsigned long u4GfxHwId,
	unsigned int u4AlComNormal, unsigned int u4Ar,
	unsigned int u4OpCode,
	unsigned int u4RectSrc);
#endif				// GFX_IF_H
