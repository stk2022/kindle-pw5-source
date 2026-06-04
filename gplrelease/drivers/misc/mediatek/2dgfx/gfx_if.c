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


#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/miscdevice.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/sched.h>

#include "gfx_if.h"
#include "gfx_dif.h"
#include "gfx_hw.h"
#include "gfx_cmdque.h"
#include "gfx_drv.h"

#ifndef NULL
#define NULL    0
#endif
#define GFX_PHYSICAL_SAFE(_addr_)	((uintptr_t)_addr_)
#define GFX_PHYSICAL(_addr_)		(GFX_PHYSICAL_SAFE(_addr_))
#define GFX_PAL_PHYSICAL(_addr_)	(GFX_PHYSICAL_SAFE(_addr_))


#ifdef GFX_RISC_MODE
#define GFX_CMDENQ(u4GfxHwId, XREG) \
(GFX_RiscPushBack(u4GfxHwId, (int)(XREG), REGFILE(u4GfxHwId, XREG)))
#else
#define GFX_CMDENQ(u4GfxHwId, XREG) \
(GFX_CmdQuePushBack(u4GfxHwId, (int)(XREG), REGFILE(u4GfxHwId, XREG)))
#endif
#define SetRotateWr(u4GfxHwId, fgRotateEn) \
	(REG(u4GfxHwId, fg_DST_WR_ROTATE) = fgRotateEn)
#define SetRotateRd(u4GfxHwId, fgRotateEn) \
	(REG(u4GfxHwId, fg_DST_RD_ROTATE) = fgRotateEn)
#define SetRotate SetRotateWr

#define ResetMirrorFlipFlag(u4GfxHwId) { \
	REG(u4GfxHwId, fg_DSTPITCH_DEC) = 0; \
	REG(u4GfxHwId, fg_DST_MIRR_OR)  = 0; \
	REG(u4GfxHwId, fg_SRCPITCH_DEC) = 0; \
	REG(u4GfxHwId, fg_SRC_MIRR_OR)  = 0; \
	GFX_CMDENQ(u4GfxHwId, GREG_BITBLT_MODE); \
}
#define ResetRotateFlag(u4GfxHwId) { \
	SetRotate(u4GfxHwId, 0); \
	SetRotateRd(u4GfxHwId, 0); \
	REG(u4GfxHwId, fg_DSTPITCH_DEC) = 0; \
	GFX_CMDENQ(u4GfxHwId, GREG_BITBLT_MODE); \
}
#define ResetRotateMirrorFlipFlag(u4GfxHwId) { \
	REG(u4GfxHwId, fg_DSTPITCH_DEC) = 0; \
	REG(u4GfxHwId, fg_DST_MIRR_OR)  = 0; \
	REG(u4GfxHwId, fg_SRCPITCH_DEC) = 0; \
	REG(u4GfxHwId, fg_SRC_MIRR_OR)  = 0; \
	SetRotate(u4GfxHwId, 0); \
	SetRotateRd(u4GfxHwId, 0); \
	GFX_CMDENQ(u4GfxHwId, GREG_BITBLT_MODE); \
}
#define ResetI2DFlag(u4GfxHwId) { \
	REG(u4GfxHwId, fg_IDX2DIR_EN)    = 0; \
	REG(u4GfxHwId, fg_CPT_DRAM_EN)   = 0; \
	REG(u4GfxHwId, fg_USE_256CPT)    = 0; \
	REG(u4GfxHwId, fg_256CPT_FIRE)   = 0; \
	REG(u4GfxHwId, fg_LN_ST_BYTE_AL) = 0; \
	REG(u4GfxHwId, fg_MSB_LEFT)      = 0; \
	REG(u4GfxHwId, fg_SRC_PITCH_ENA) = 0; \
	GFX_CMDENQ(u4GfxHwId, GREG_IDX2DIR); \
	GFX_CMDENQ(u4GfxHwId, GREG_G_256CPT); \
}
#define ResetI2DMirrorFlipFlag(u4GfxHwId) { \
	ResetI2DFlag(u4GfxHwId); \
	ResetMirrorFlipFlag(u4GfxHwId); \
}
#define ResetI2DRotateMirrorFlipFlag(u4GfxHwId) { \
	ResetI2DFlag(u4GfxHwId); \
	ResetRotateMirrorFlipFlag(u4GfxHwId); \
}
#define GFX_LOG_2048_HAL(X) { \
	if (X > 2047) \
		GFX_LOG_E("h>2048\n"); \
}
#define VA_MSB_REG(u4GfxHwId, fg_MSB, u4Val) \
(REG(u4GfxHwId, fg_MSB) = (GFX_PHYSICAL(u4Val) >> 30))
#define GFX_VA_MSB_CMDENQ(u4GfxHwId) GFX_CMDENQ(u4GfxHwId, GREG_VA_MSB)
#define VA_PAL_MSB_REG(u4GfxHwId, fg_MSB, u4Val) \
(REG(u4GfxHwId, fg_MSB) = (GFX_PAL_PHYSICAL(u4Val) >> 30))

unsigned int *_pu1DstBase[GFX_HAL_HW_INST_NUM];
unsigned int *_pu1SrcBase[GFX_HAL_HW_INST_NUM];
unsigned int *_pu12ndSrcBase[GFX_HAL_HW_INST_NUM];
unsigned int _u4DstPitch[GFX_HAL_HW_INST_NUM];
unsigned int _u4SrcPitch[GFX_HAL_HW_INST_NUM];
unsigned int _u42ndSrcPitch[GFX_HAL_HW_INST_NUM];
unsigned int _u4DstCM[GFX_HAL_HW_INST_NUM];
unsigned int _u4SrcCM[GFX_HAL_HW_INST_NUM];
unsigned int _u42ndSrcCM[GFX_HAL_HW_INST_NUM];
unsigned int _u4DstShift[GFX_HAL_HW_INST_NUM];
unsigned int _u4SrcShift[GFX_HAL_HW_INST_NUM];
unsigned int _u42ndSrcShift[GFX_HAL_HW_INST_NUM];
static GFX_REGFILE GfxRegFile[GFX_HAL_HW_INST_NUM];
static unsigned int _u4CharCM[GFX_HAL_HW_INST_NUM];

#define  REGFILE(u4GfxHwId, XREG) \
(GfxRegFile[u4GfxHwId].au4RegFile[XREG])
#define REG(u4GfxHwId, XREG) \
(GfxRegFile[u4GfxHwId].prRegFile->rField.XREG)

int i4_gfx_suspend(void)
{
	int i4Ret;

	i4Ret = 0;

	//CKGEN_MskWrite(0x300, 0, CKEN_CFG0_GRAPH_CKEN_MASK);
	return i4Ret;
}

int i4_gfx_resume(void)
{
	int i4Ret;
	int i4;

	GFX_HW_FX_ENTRY;

	for (i4 = 0; i4 < GFX_HAL_HW_INST_NUM; i4++) {
		i4Ret = GFX_Reset(i4, GFX_RESET_BOTH);
		VERIFY(i4Ret == E_GFX_OK);
	}

	i4Ret = Gfx_RegFileInit();

	for (i4 = 0; i4 < GFX_HAL_HW_INST_NUM; i4++) {
		GFX_UNUSED_RET(x_memset(GfxRegFile[i4].au4RegFile,
			0, sizeof(GfxRegFile[i4].au4RegFile)))
		REG(i4, fg_A_SEL) = 3;
		REG(i4, fg_VB_SEL) = 0;
		REG(i4, fg_UG_SEL) = 1;
		REG(i4, fg_YR_SEL) = 2;
		VA_MSB_REG(i4, fg_LEGAL_AD_END_H, 0xC0000000);
		VA_MSB_REG(i4, fg_BPCOMP_AD_END_H, 0xC0000000);
	}

	for (i4 = 0; i4 < GFX_HAL_HW_INST_NUM; i4++)
		GFX_BurstEn(i4, 1, 2);

	GFX_CmdQueUninit();

	for (i4 = 0; i4 < GFX_HAL_HW_INST_NUM; i4++) {
		GFX_CmdQueInit(i4);
		i4Ret = GFX_CmdQueReset(i4);
		VA_MSB_REG(i4, fg_DRAMQ_BSAD_H,
			Gfx_CmdQueVar[i4].gfx_cmdq_bus_addr);
		GFX_VA_MSB_CMDENQ(i4);
		GFX_LOG_D("STR cmdq_bus=%x, 0x20b8 1=%x\n",
			Gfx_CmdQueVar[i4].gfx_cmdq_bus_addr,
			GFX_READ32(i4, 0xB8));
		GFX_Flush(0);
		GFX_LOG_D("STR cmdq_bus=%x, 0x20b8 2=%x\n",
			Gfx_CmdQueVar[i4].gfx_cmdq_bus_addr,
			GFX_READ32(i4, 0xB8));
	}

	GFX_Flush(0);
	return i4Ret;
}

int Gfx_RegFileInit(void)
{
	int i4;

	for (i4 = 0; i4 < GFX_HAL_HW_INST_NUM; i4++)
		GfxRegFile[i4].prRegFile =
		(MI_DIF_UNION_T *) (&REGFILE(i4, 0));

	return E_GFX_OK;
}

int Gfx_cmdq_init(void)
{
	int i4;

	for (i4 = 0; i4 < GFX_HAL_HW_INST_NUM; i4++) {
		VA_MSB_REG(i4, fg_DRAMQ_BSAD_H, GFX_GetCmdQBusaddr(i4));
		VA_MSB_REG(i4, fg_LEGAL_AD_END_H, 0xC0000000);
		GFX_VA_MSB_CMDENQ(i4);
		GFX_LOG_D("cmdq_bus=%x, 0x20b8 1=%x\n",
			Gfx_CmdQueVar[i4].gfx_cmdq_bus_addr,
		GFX_READ32(i4, 0xB8));
		GFX_Flush(i4);
		GFX_LOG_D("cmdq_bus=%x, 0x20b8 2=%x\n",
			Gfx_CmdQueVar[i4].gfx_cmdq_bus_addr,
		GFX_READ32(i4, 0xB8));
	}

	return E_GFX_OK;
}

int GFX_Reset(unsigned long u4GfxHwId, unsigned int u4Reset)
{
	return GFX_DifReset(u4GfxHwId, u4Reset);
}

int GFX_GetIdle(unsigned long u4GfxHwId)
{
	return GFX_DifGetIdle(u4GfxHwId);
}

void GFX_Wait(unsigned long u4GfxHwId)
{
	pfnGFX_DifWait(u4GfxHwId);
	//GFX_LockCmdque();
	//GFX_UnlockCmdque();
	//GFX_Reset(GFX_RESET_BOTH);
	//GFX_DifSetMode(1);
}

int GFX_Flush(unsigned long u4GfxHwId)
{
	int i4Ret = 0;

	GFX_HW_FX_ENTRY;
	i4Ret = GFX_CmdQueAction(u4GfxHwId);

	return i4Ret;
}

void GFX_LockCmdque(unsigned long u4GfxHwId)
{

	while (1) {
		if (GFX_HwGetIdle(u4GfxHwId))
			break;
	}
}

void GFX_UnlockCmdque(unsigned long u4GfxHwId)
{
	Gfx_CmdQueVar[u4GfxHwId].gfx_irq_done = true;
}

int GFX_QueryHwIdle(unsigned long u4GfxHwId)
{
	return GFX_HwGetIdle(u4GfxHwId);
}

int GFX_SetDst(unsigned long u4GfxHwId, unsigned int *pu1Base,
	unsigned int u4ColorMode, unsigned int u4Pitch)
{
	GFX_HW_FX_ENTRY;

	if ((uintptr_t) u4Pitch & 0xF)
		return -(int)E_GFX_INV_ARG;

	VERIFY(pu1Base != 0);
	_pu1DstBase[u4GfxHwId] = pu1Base;
	_u4DstCM[u4GfxHwId] = u4ColorMode;
	_u4DstPitch[u4GfxHwId] = u4Pitch;
	_u4DstShift[u4GfxHwId] = _auColorModeShift[u4ColorMode];

	REG(u4GfxHwId, fg_CM) = u4ColorMode;
	REG(u4GfxHwId, fg_OSD_WIDTH) = (u4Pitch >> 4);

	return GFX_CMDENQ(u4GfxHwId, GREG_S_OSD_WIDTH);
}

int GFX_Set2ndSrcEnable(unsigned long u4GfxHwId, unsigned int u4ColorMode)
{
	GFX_HW_FX_ENTRY;

	REG(u4GfxHwId, fg_SRC2_BSAD_ENA) = 1;
	REG(u4GfxHwId, fg_SRC2_CM) = u4ColorMode;

	return GFX_CMDENQ(u4GfxHwId, GREG_SRCCBCR_PITCH);
}

int GFX_SetSrc(unsigned long u4GfxHwId, unsigned int *pu1Base,
	unsigned int u4ColorMode, unsigned int u4Pitch)
{
	GFX_HW_FX_ENTRY;

	if ((uintptr_t) u4Pitch & 0xF)
		return -(int)E_GFX_INV_ARG;

	_pu1SrcBase[u4GfxHwId] = pu1Base;
	_u4SrcCM[u4GfxHwId] = u4ColorMode;
	_u4SrcPitch[u4GfxHwId] = u4Pitch;
	_u4SrcShift[u4GfxHwId] = _auColorModeShift[u4ColorMode];

#ifdef GFX_RISC_MODE
	GFX_WRITE32(u4GfxHwId, GFX_REG_DRAMQ_LEN,
	    (GFX_READ32(u4GfxHwId, GFX_REG_DRAMQ_LEN) | 0x10000000));
#endif
	REG(u4GfxHwId, fg_SRC_CM) = u4ColorMode;
	REG(u4GfxHwId, fg_SRC_PITCH) = (u4Pitch >> 4);

	return GFX_CMDENQ(u4GfxHwId, GREG_S_OSD_WIDTH);
}

int GFX_Set2ndSrc(unsigned long u4GfxHwId, unsigned int *pu1Base,
	unsigned int u4ColorMode, unsigned int u4Pitch)
{
	GFX_HW_FX_ENTRY;

	// check 128 bits (16 bytes) alignment
	if ((uintptr_t) u4Pitch & 0xF)
		return -(int)E_GFX_INV_ARG;

	_pu12ndSrcBase[u4GfxHwId] = pu1Base;
	_u42ndSrcCM[u4GfxHwId] = u4ColorMode;
	_u42ndSrcPitch[u4GfxHwId] = u4Pitch;
	_u42ndSrcShift[u4GfxHwId] = _auColorModeShift[u4ColorMode];

	REG(u4GfxHwId, fg_SRC2_CM) = u4ColorMode;
	REG(u4GfxHwId, fg_SRCCBCR_PITCH) = (u4Pitch >> 4);

	return GFX_CMDENQ(u4GfxHwId, GREG_SRCCBCR_PITCH);
}

int GFX_SetCharSrcBase(unsigned long u4GfxHwId, unsigned int *pu1Base,
	unsigned int u4ColorMode, unsigned int u4Pitch)
{
	GFX_HW_FX_ENTRY;

	// check 128 bits (16 bytes) alignment
	if ((uintptr_t) u4Pitch & 0xF)
		return -(int)E_GFX_INV_ARG;

	_pu1SrcBase[u4GfxHwId] = pu1Base;
	_u4CharCM[u4GfxHwId] = u4ColorMode;
	_u4SrcPitch[u4GfxHwId] = u4Pitch;
	_u4SrcShift[u4GfxHwId] = _auColorModeShift[u4ColorMode];

	REG(u4GfxHwId, fg_SRC_PITCH) = (u4Pitch >> 4);

	return GFX_CMDENQ(u4GfxHwId, GREG_S_OSD_WIDTH);
}

int GFX_SetAlpha(unsigned long u4GfxHwId, unsigned int u4Alpha)
{
	GFX_HW_FX_ENTRY;
	REG(u4GfxHwId, fg_ALPHA_VALUE) = u4Alpha;
	return GFX_CMDENQ(u4GfxHwId, GREG_BITBLT_MODE);
}


int GFX_SetColor(unsigned long u4GfxHwId, unsigned int u4Color)
{
	GFX_HW_FX_ENTRY;

	REG(u4GfxHwId, fg_RECT_COLOR) = u4Color;
	return GFX_CMDENQ(u4GfxHwId, GREG_RECT_COLOR);
}

void GFX_SetReqInterval(unsigned long u4GfxHwId, unsigned int u4ReqInterval)
{
	GFX_HwSetReqInterval(u4GfxHwId, u4ReqInterval);
}

int GFX_Fill(unsigned long u4GfxHwId, unsigned int u4X, unsigned int u4Y,
	unsigned int u4Width, unsigned int u4Height)
{
	unsigned int u4DstAddr;

	GFX_HW_FX_ENTRY;

	//destination base address
	u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] + ((
		u4Y * _u4DstPitch[u4GfxHwId]) +
		(u4X << _u4DstShift[u4GfxHwId])));

	REG(u4GfxHwId, fg_DST_BSAD) = GFX_PHYSICAL(u4DstAddr);
	REG(u4GfxHwId, fg_OP_MODE) = (unsigned int)OP_RECT_FILL;
	REG(u4GfxHwId, fg_SRC_WIDTH) = u4Width;

	u4Height -= 1;
	REG(u4GfxHwId, fg_SRC_HEIGHT) = u4Height & 0x007FF;
	REG(u4GfxHwId, fg_SRC_CM_2) = (u4Height >> 11) & 0x0F;
	REG(u4GfxHwId, fg_SRC_HEIGHT_15) = (u4Height >> 15) & 0x01;
	REG(u4GfxHwId, fg_SRC_HEIGHT_19_16) = (u4Height >> 16) & 0x0F;
	REG(u4GfxHwId, fg_FIRE) = GFX_ENGINE_FIRE;

	VA_MSB_REG(u4GfxHwId, fg_DST_BSAD_H, GFX_PHYSICAL(u4DstAddr));
	GFX_VA_MSB_CMDENQ(u4GfxHwId);

	if (GFX_CMDENQ(u4GfxHwId, GREG_DST_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE) |
	    GFX_CMDENQ(u4GfxHwId, GREG_G_PGIG) |
	    GFX_CMDENQ(u4GfxHwId, GREG_G_MODE))
		return -(int)E_GFX_OUT_OF_MEM;

	REG(u4GfxHwId, fg_SRC_CM_2) = 0;
	REG(u4GfxHwId, fg_SRC_HEIGHT_15) = 0;
	GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE);
	REG(u4GfxHwId, fg_SRC_HEIGHT_19_16) = 0;
	GFX_CMDENQ(u4GfxHwId, GREG_G_PGIG);

	return (int)E_GFX_OK;
}

int GFX_Draw(unsigned long u4GfxHwId, unsigned int u4X, unsigned int u4Y,
	unsigned int u4Width, unsigned int u4Height)
{
	unsigned int u4DstAddr;

	GFX_HW_FX_ENTRY;

	if (u4Height <= 2048) {
		u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
			(u4Y * _u4DstPitch[u4GfxHwId]) +
			(u4X << _u4DstShift[u4GfxHwId]));

		REG(u4GfxHwId, fg_DST_BSAD) = GFX_PHYSICAL(u4DstAddr);
		REG(u4GfxHwId, fg_OP_MODE) = (unsigned int)OP_RECT_FILL;
		REG(u4GfxHwId, fg_SRC_WIDTH) = u4Width;
		REG(u4GfxHwId, fg_SRC_HEIGHT) = 0;
		REG(u4GfxHwId, fg_FIRE) = GFX_ENGINE_FIRE;
		VA_MSB_REG(u4GfxHwId, fg_DST_BSAD_H,
			GFX_PHYSICAL(u4DstAddr));
		GFX_VA_MSB_CMDENQ(u4GfxHwId);

		if (GFX_CMDENQ(u4GfxHwId, GREG_DST_BSAD) |
		    GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE) |
		    GFX_CMDENQ(u4GfxHwId, GREG_G_MODE))
			return -(int)E_GFX_OUT_OF_MEM;

		REG(u4GfxHwId, fg_OP_MODE) = (unsigned int)OP_RECT_FILL;
		REG(u4GfxHwId, fg_SRC_WIDTH) = 1;
		REG(u4GfxHwId, fg_SRC_HEIGHT) = (u4Height - 1);
		REG(u4GfxHwId, fg_FIRE) = GFX_ENGINE_FIRE;
		VA_MSB_REG(u4GfxHwId, fg_DST_BSAD_H,
			GFX_PHYSICAL(u4DstAddr));
		GFX_VA_MSB_CMDENQ(u4GfxHwId);

		if (GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE) |
			GFX_CMDENQ(u4GfxHwId, GREG_G_MODE))
			return -(int)E_GFX_OUT_OF_MEM;

		u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
		     (u4Y + u4Height - 1) * _u4DstPitch[u4GfxHwId] +
		     (u4X << _u4DstShift[u4GfxHwId]));

		REG(u4GfxHwId, fg_DST_BSAD) = GFX_PHYSICAL(u4DstAddr);
		REG(u4GfxHwId, fg_OP_MODE) = (unsigned int)OP_RECT_FILL;
		REG(u4GfxHwId, fg_SRC_WIDTH) = u4Width;
		REG(u4GfxHwId, fg_SRC_HEIGHT) = 0;
		REG(u4GfxHwId, fg_FIRE) = GFX_ENGINE_FIRE;
		VA_MSB_REG(u4GfxHwId, fg_DST_BSAD_H,
			GFX_PHYSICAL(u4DstAddr));
		GFX_VA_MSB_CMDENQ(u4GfxHwId);

		if (GFX_CMDENQ(u4GfxHwId, GREG_DST_BSAD) |
		    GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE) |
		    GFX_CMDENQ(u4GfxHwId, GREG_G_MODE))
			return -(int)E_GFX_OUT_OF_MEM;

		u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
			u4Y * _u4DstPitch[u4GfxHwId] +
			((u4X + u4Width - 1) << _u4DstShift[u4GfxHwId]));

		REG(u4GfxHwId, fg_DST_BSAD) = GFX_PHYSICAL(u4DstAddr);
		REG(u4GfxHwId, fg_OP_MODE) = (unsigned int)OP_RECT_FILL;
		REG(u4GfxHwId, fg_SRC_WIDTH) = 1;
		REG(u4GfxHwId, fg_SRC_HEIGHT) = (u4Height - 1);
		REG(u4GfxHwId, fg_FIRE) = GFX_ENGINE_FIRE;

		VA_MSB_REG(u4GfxHwId, fg_DST_BSAD_H,
			GFX_PHYSICAL(u4DstAddr));
		GFX_VA_MSB_CMDENQ(u4GfxHwId);

		if (GFX_CMDENQ(u4GfxHwId, GREG_DST_BSAD) |
		    GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE) |
		    GFX_CMDENQ(u4GfxHwId, GREG_G_MODE))
			return -(int)E_GFX_OUT_OF_MEM;

		return (int)E_GFX_OK;
	} else
		return (int)E_GFX_INV_ARG;
}

int GFX_DrawCompose(unsigned long u4GfxHwId, unsigned int u4X,
	unsigned int u4Y, unsigned int u4Width, unsigned int u4Height,
	unsigned int u4Ar, unsigned int u4OpCode, unsigned int u4RectSrc)
{
	unsigned int u4DstAddr;

	if ((u4OpCode == (unsigned int)E_AC_CLEAR) ||
		(u4OpCode == (unsigned int)E_AC_SRC)) {
		unsigned int u4RectColor = REG(u4GfxHwId, fg_RECT_COLOR);

		GFX_SetColor(u4GfxHwId, 0x0);
		GFX_Draw(u4GfxHwId, u4X, u4Y, u4Width, u4Height);
		GFX_SetColor(u4GfxHwId, u4RectColor);
		return (int)E_GFX_OK;
	}

	REG(u4GfxHwId, fg_SRC2_BSAD_ENA) = 0;
	if (GFX_CMDENQ(u4GfxHwId, GREG_SRCCBCR_PITCH))
		return -(int)E_GFX_OUT_OF_MEM;

	u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
	     (u4Y * _u4DstPitch[u4GfxHwId]) +
	     (u4X << _u4DstShift[u4GfxHwId]));

	REG(u4GfxHwId, fg_DST_BSAD) = GFX_PHYSICAL(u4DstAddr);
	REG(u4GfxHwId, fg_OP_MODE) = (unsigned int)OP_LOOP_ALPAH_COMPOS;
	REG(u4GfxHwId, fg_SRC_WIDTH) = u4Width;
	REG(u4GfxHwId, fg_SRC_HEIGHT) = 0;
	REG(u4GfxHwId, fg_ALCOM_AR) = u4Ar;
	REG(u4GfxHwId, fg_ALCOM_OPCODE) = u4OpCode;
	REG(u4GfxHwId, fg_ALCOM_RECT_SRC) = u4RectSrc;
	REG(u4GfxHwId, fg_ALCOM_NORMAL) = (u4Ar == 255);
	REG(u4GfxHwId, fg_FIRE) = GFX_ENGINE_FIRE;

	VA_MSB_REG(u4GfxHwId, fg_DST_BSAD_H, GFX_PHYSICAL(u4DstAddr));
	GFX_VA_MSB_CMDENQ(u4GfxHwId);

	if (GFX_CMDENQ(u4GfxHwId, GREG_DST_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE) |
	    GFX_CMDENQ(u4GfxHwId, GREG_ALCOM_LOOP) |
	    GFX_CMDENQ(u4GfxHwId, GREG_G_MODE))
		return -(int)E_GFX_OUT_OF_MEM;

	REG(u4GfxHwId, fg_OP_MODE) = (unsigned int)OP_LOOP_ALPAH_COMPOS;
	REG(u4GfxHwId, fg_SRC_WIDTH) = 1;
	REG(u4GfxHwId, fg_SRC_HEIGHT) = (u4Height - 1);
	REG(u4GfxHwId, fg_FIRE) = GFX_ENGINE_FIRE;
	VA_MSB_REG(u4GfxHwId, fg_DST_BSAD_H, GFX_PHYSICAL(u4DstAddr));
	GFX_VA_MSB_CMDENQ(u4GfxHwId);

	if (GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE) |
		GFX_CMDENQ(u4GfxHwId, GREG_G_MODE))
		return -(int)E_GFX_OUT_OF_MEM;

	u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
		(u4Y + u4Height - 1) * _u4DstPitch[u4GfxHwId] +
		(u4X << _u4DstShift[u4GfxHwId]));

	REG(u4GfxHwId, fg_DST_BSAD) = GFX_PHYSICAL(u4DstAddr);
	REG(u4GfxHwId, fg_OP_MODE) = (unsigned int)OP_LOOP_ALPAH_COMPOS;
	REG(u4GfxHwId, fg_SRC_WIDTH) = u4Width;
	REG(u4GfxHwId, fg_SRC_HEIGHT) = 0;
	REG(u4GfxHwId, fg_FIRE) = GFX_ENGINE_FIRE;
	VA_MSB_REG(u4GfxHwId, fg_DST_BSAD_H, GFX_PHYSICAL(u4DstAddr));
	GFX_VA_MSB_CMDENQ(u4GfxHwId);

	if (GFX_CMDENQ(u4GfxHwId, GREG_DST_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE) |
	    GFX_CMDENQ(u4GfxHwId, GREG_G_MODE))
		return -(int)E_GFX_OUT_OF_MEM;

	u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
		u4Y * _u4DstPitch[u4GfxHwId] +
		((u4X + u4Width - 1) << _u4DstShift[u4GfxHwId]));

	REG(u4GfxHwId, fg_DST_BSAD) = GFX_PHYSICAL(u4DstAddr);
	REG(u4GfxHwId, fg_OP_MODE) = (unsigned int)OP_LOOP_ALPAH_COMPOS;
	REG(u4GfxHwId, fg_SRC_WIDTH) = 1;
	REG(u4GfxHwId, fg_SRC_HEIGHT) = (u4Height - 1);
	REG(u4GfxHwId, fg_FIRE) = GFX_ENGINE_FIRE;

	VA_MSB_REG(u4GfxHwId, fg_DST_BSAD_H, GFX_PHYSICAL(u4DstAddr));
	GFX_VA_MSB_CMDENQ(u4GfxHwId);

	if (GFX_CMDENQ(u4GfxHwId, GREG_DST_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE) |
	    GFX_CMDENQ(u4GfxHwId, GREG_G_MODE))
		return -(int)E_GFX_OUT_OF_MEM;

	return (int)E_GFX_OK;
}

int GFX_FillTriangle(unsigned long u4GfxHwId, unsigned int u4X1,
	unsigned int u4Y1, unsigned int u4X2, unsigned int u4Y2,
	unsigned int u4X3, unsigned int u4Y3)
{
	unsigned int u4DstAddr;
	unsigned int u4Width = 2, u4Index = 0;
	unsigned int u4X = u4X1, u4Y = u4Y1;

	GFX_HW_FX_ENTRY;

	u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
	     (u4Y * _u4DstPitch[u4GfxHwId]) +
	     (u4X << _u4DstShift[u4GfxHwId]));

	REG(u4GfxHwId, fg_DST_BSAD) = GFX_PHYSICAL(u4DstAddr);
	REG(u4GfxHwId, fg_OP_MODE) = (unsigned int)OP_RECT_FILL;
	REG(u4GfxHwId, fg_SRC_WIDTH) = u4Width;
	REG(u4GfxHwId, fg_SRC_HEIGHT) = 0;
	REG(u4GfxHwId, fg_FIRE) = GFX_ENGINE_FIRE;
	VA_MSB_REG(u4GfxHwId, fg_DST_BSAD_H, GFX_PHYSICAL(u4DstAddr));
	GFX_VA_MSB_CMDENQ(u4GfxHwId);

	if (GFX_CMDENQ(u4GfxHwId, GREG_DST_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE) |
	    GFX_CMDENQ(u4GfxHwId, GREG_G_MODE))
		return -(int)E_GFX_OUT_OF_MEM;

	u4Y++;

	for (u4Index = 1; u4Index < 256; u4Index++, u4Y++) {
		if (u4Index < 128)
			u4Width += 2;
		else if (u4Index == 192)
			u4Width--;
		else
			u4Width -= 2;

		u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
		(u4Y * _u4DstPitch[u4GfxHwId]) +
		(u4X << _u4DstShift[u4GfxHwId]));

		REG(u4GfxHwId, fg_DST_BSAD) = GFX_PHYSICAL(u4DstAddr);
		REG(u4GfxHwId, fg_OP_MODE) = (unsigned int)OP_RECT_FILL;
		REG(u4GfxHwId, fg_SRC_WIDTH) = u4Width;
		REG(u4GfxHwId, fg_FIRE) = GFX_ENGINE_FIRE;

		if (GFX_CMDENQ(u4GfxHwId, GREG_DST_BSAD) |
		    GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE) |
		    GFX_CMDENQ(u4GfxHwId, GREG_G_MODE))
			return -(int)E_GFX_OUT_OF_MEM;
	}

	return (int)E_GFX_OK;
}

int GFX_FillTriangleCompose(unsigned long u4GfxHwId,
	unsigned int u4X1, unsigned int u4Y1, unsigned int u4X2,
	unsigned int u4Y2, unsigned int u4X3, unsigned int u4Y3,
	unsigned int u4Ar, unsigned int u4OpCode, unsigned int u4RectSrc)
{
	unsigned int u4DstAddr;
	unsigned int u4Width = 2, u4Index = 0;
	unsigned int u4X = u4X1, u4Y = u4Y1;

	if ((u4OpCode == (unsigned int)E_AC_CLEAR) ||
		(u4OpCode == (unsigned int)E_AC_SRC)) {
		unsigned int u4RectColor =
			REG(u4GfxHwId, fg_RECT_COLOR);

		GFX_SetColor(u4GfxHwId, 0x0);
		GFX_FillTriangle(u4GfxHwId, u4X1, u4Y1,
			u4X2, u4Y2, u4X3, u4Y3);

		GFX_SetColor(u4GfxHwId, u4RectColor);
		return (int)E_GFX_OK;
	}

	REG(u4GfxHwId, fg_SRC2_BSAD_ENA) = 0;
	if (GFX_CMDENQ(u4GfxHwId, GREG_SRCCBCR_PITCH))
		return -(int)E_GFX_OUT_OF_MEM;

	u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
		(u4Y * _u4DstPitch[u4GfxHwId]) +
		(u4X << _u4DstShift[u4GfxHwId]));

	REG(u4GfxHwId, fg_DST_BSAD) = GFX_PHYSICAL(u4DstAddr);
	REG(u4GfxHwId, fg_OP_MODE) =
		(unsigned int)OP_LOOP_ALPAH_COMPOS;
	REG(u4GfxHwId, fg_SRC_WIDTH) = u4Width;
	REG(u4GfxHwId, fg_SRC_HEIGHT) = 0;
	REG(u4GfxHwId, fg_ALCOM_AR) = u4Ar;
	REG(u4GfxHwId, fg_ALCOM_OPCODE) = u4OpCode;
	REG(u4GfxHwId, fg_ALCOM_RECT_SRC) = u4RectSrc;
	REG(u4GfxHwId, fg_ALCOM_NORMAL) = (u4Ar == 255);
	REG(u4GfxHwId, fg_FIRE) = GFX_ENGINE_FIRE;

	VA_MSB_REG(u4GfxHwId, fg_DST_BSAD_H,
		GFX_PHYSICAL(u4DstAddr));
	GFX_VA_MSB_CMDENQ(u4GfxHwId);

	if (GFX_CMDENQ(u4GfxHwId, GREG_DST_BSAD) |
		GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE) |
		GFX_CMDENQ(u4GfxHwId, GREG_ALCOM_LOOP) |
		GFX_CMDENQ(u4GfxHwId, GREG_G_MODE))
		return -(int)E_GFX_OUT_OF_MEM;

	u4Y++;

	for (u4Index = 1; u4Index < 256; u4Index++, u4Y++) {
		if (u4Index < 128)
			u4Width += 2;
		else if (u4Index == 192)
			u4Width--;
		else
			u4Width -= 2;

		u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
			(u4Y * _u4DstPitch[u4GfxHwId]) +
			(u4X << _u4DstShift[u4GfxHwId]));

		REG(u4GfxHwId, fg_DST_BSAD) = GFX_PHYSICAL(u4DstAddr);
		REG(u4GfxHwId, fg_OP_MODE) =
			(unsigned int)OP_LOOP_ALPAH_COMPOS;
		REG(u4GfxHwId, fg_SRC_WIDTH) = u4Width;
		REG(u4GfxHwId, fg_FIRE) = GFX_ENGINE_FIRE;

		VA_MSB_REG(u4GfxHwId, fg_DST_BSAD_H,
			GFX_PHYSICAL(u4DstAddr));
		GFX_VA_MSB_CMDENQ(u4GfxHwId);

		if (GFX_CMDENQ(u4GfxHwId, GREG_DST_BSAD) |
		    GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE) |
		    GFX_CMDENQ(u4GfxHwId, GREG_G_MODE))
			return -(int)E_GFX_OUT_OF_MEM;
	}

	return (int)E_GFX_OK;
}

int GFX_HLine(unsigned long u4GfxHwId, unsigned int u4X,
	unsigned int u4Y, unsigned int u4Width)
{
	unsigned int u4DstAddr;

	GFX_HW_FX_ENTRY;
	u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
		(u4Y * _u4DstPitch[u4GfxHwId]) +
		(u4X << _u4DstShift[u4GfxHwId]));

	REG(u4GfxHwId, fg_DST_BSAD) = GFX_PHYSICAL(u4DstAddr);
	REG(u4GfxHwId, fg_OP_MODE) = (unsigned int)OP_DRAW_HLINE;
	REG(u4GfxHwId, fg_SRC_WIDTH) = u4Width;
	REG(u4GfxHwId, fg_SRC_HEIGHT) = 0;
	REG(u4GfxHwId, fg_FIRE) = GFX_ENGINE_FIRE;

	VA_MSB_REG(u4GfxHwId, fg_DST_BSAD_H,
		GFX_PHYSICAL(u4DstAddr));
	GFX_VA_MSB_CMDENQ(u4GfxHwId);

	if (GFX_CMDENQ(u4GfxHwId, GREG_DST_BSAD) |
		GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE) |
		GFX_CMDENQ(u4GfxHwId, GREG_G_MODE))
		return -(int)E_GFX_OUT_OF_MEM;

	return (int)E_GFX_OK;
}

int GFX_VLine(unsigned long u4GfxHwId, unsigned int u4X,
	unsigned int u4Y, unsigned int u4Height)
{
	unsigned int u4DstAddr;

	GFX_HW_FX_ENTRY;

	u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
		(u4Y * _u4DstPitch[u4GfxHwId]) +
		(u4X << _u4DstShift[u4GfxHwId]));

	REG(u4GfxHwId, fg_DST_BSAD) = GFX_PHYSICAL(u4DstAddr);
	REG(u4GfxHwId, fg_OP_MODE) = (unsigned int)OP_DRAW_VLINE;
	REG(u4GfxHwId, fg_SRC_WIDTH) = 1;
	REG(u4GfxHwId, fg_SRC_HEIGHT) = (u4Height - 1);
	REG(u4GfxHwId, fg_FIRE) = GFX_ENGINE_FIRE;

	VA_MSB_REG(u4GfxHwId, fg_DST_BSAD_H, GFX_PHYSICAL(u4DstAddr));
	GFX_VA_MSB_CMDENQ(u4GfxHwId);

	if (GFX_CMDENQ(u4GfxHwId, GREG_DST_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE) |
	    GFX_CMDENQ(u4GfxHwId, GREG_G_MODE))
		return -(int)E_GFX_OUT_OF_MEM;

	return (int)E_GFX_OK;
}

int GFX_ObliqueLine(unsigned long u4GfxHwId, unsigned int u4SrcX,
	unsigned int u4SrcY, unsigned int u4DstX, unsigned int u4DstY)
{
	unsigned int u4DstAddr;

	GFX_HW_FX_ENTRY;
	u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
			(u4SrcY * _u4DstPitch[u4GfxHwId]) +
			(u4SrcX << _u4DstShift[u4GfxHwId]));

	REG(u4GfxHwId, fg_DST_BSAD) = GFX_PHYSICAL(u4DstAddr);
	REG(u4GfxHwId, fg_OP_MODE) = (unsigned int)OP_OBLIQUE_LINE;
	REG(u4GfxHwId, fg_SRCX) = u4SrcX;
	REG(u4GfxHwId, fg_SRCY) = u4SrcY;
	REG(u4GfxHwId, fg_DSTX) = u4DstX;
	REG(u4GfxHwId, fg_DSTY) = u4DstY;
	REG(u4GfxHwId, fg_SRC_WIDTH) = 1;
	REG(u4GfxHwId, fg_FIRE) = GFX_ENGINE_FIRE;

	VA_MSB_REG(u4GfxHwId, fg_DST_BSAD_H, GFX_PHYSICAL(u4DstAddr));
	GFX_VA_MSB_CMDENQ(u4GfxHwId);

	if (GFX_CMDENQ(u4GfxHwId, GREG_DST_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_XY) |
	    GFX_CMDENQ(u4GfxHwId, GREG_DST_XY) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE) |
	    GFX_CMDENQ(u4GfxHwId, GREG_G_MODE))
		return -(int)E_GFX_OUT_OF_MEM;

	return (int)E_GFX_OK;
}

int GFX_SetBltOpt(unsigned long u4GfxHwId, unsigned int u4Switch,
	unsigned int u4ColorMin, unsigned int u4ColorMax)
{
	GFX_HW_FX_ENTRY;

	if ((u4Switch & D_GFXFLAG_TRANSPARENT) &&
		(u4Switch & D_GFXFLAG_COLORCHANGE))
		return -(int)E_GFX_INV_ARG;

	REG(u4GfxHwId, fg_TRANS_ENA) =
		(u4Switch & D_GFXFLAG_TRANSPARENT) ? 1 : 0;
	REG(u4GfxHwId, fg_KEYNOT_ENA) =
		(u4Switch & D_GFXFLAG_KEYNOT) ? 1 : 0;
	REG(u4GfxHwId, fg_COLCHG_ENA) =
		(u4Switch & D_GFXFLAG_COLORCHANGE) ? 1 : 0;
	REG(u4GfxHwId, fg_KEYSDSEL) =
		(u4Switch & D_GFXFLAG_KEYSDSEL) ? 1 : 0;
	REG(u4GfxHwId, fg_COLOR_KEY_MIN) = u4ColorMin;
	REG(u4GfxHwId, fg_COLOR_KEY_MAX) = u4ColorMax;
	REG(u4GfxHwId, fg_CFMT_ENA) = 0;

	if (GFX_CMDENQ(u4GfxHwId, GREG_KEY_DATA0) |
	    GFX_CMDENQ(u4GfxHwId, GREG_KEY_DATA1) |
	    GFX_CMDENQ(u4GfxHwId, GREG_BITBLT_MODE))
		return -(int)E_GFX_OUT_OF_MEM;

	return (int)E_GFX_OK;
}

int GFX_BitBlt(unsigned long u4GfxHwId, unsigned int u4SrcX,
	unsigned int u4SrcY, unsigned int u4DstX, unsigned int u4DstY,
	unsigned int u4Width, unsigned int u4Height)
{
	unsigned int u4SrcAddr, u4DstAddr;

	GFX_HW_FX_ENTRY;
	//destination base address
	u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
		(u4DstY * _u4DstPitch[u4GfxHwId]) +
		(u4DstX << _u4DstShift[u4GfxHwId]));

	REG(u4GfxHwId, fg_DST_BSAD) = GFX_PHYSICAL(u4DstAddr);
	//source base address
	u4SrcAddr = ((uintptr_t) _pu1SrcBase[u4GfxHwId] +
		(u4SrcY * _u4SrcPitch[u4GfxHwId]) +
		(u4SrcX << _u4SrcShift[u4GfxHwId]));

	REG(u4GfxHwId, fg_SRC_BSAD) = GFX_PHYSICAL(u4SrcAddr);
	REG(u4GfxHwId, fg_OP_MODE) = (unsigned int)OP_BITBLT;
	REG(u4GfxHwId, fg_CFMT_ENA) = (unsigned int)(
		_u4DstCM[u4GfxHwId] != _u4SrcCM[u4GfxHwId]);
	REG(u4GfxHwId, fg_SRC_WIDTH) = u4Width;

	u4Height -= 1;
	REG(u4GfxHwId, fg_SRC_HEIGHT) = u4Height & 0x007FF;
	REG(u4GfxHwId, fg_SRC_HEIGHT_15) = (u4Height >> 15) & 0x01;
	REG(u4GfxHwId, fg_SRC_HEIGHT_19_16) = (u4Height >> 16) & 0x0F;
	REG(u4GfxHwId, fg_FIRE) = GFX_ENGINE_FIRE;

	GFX_LOG_2048_HAL(u4Height);
	VA_MSB_REG(u4GfxHwId, fg_DST_BSAD_H, GFX_PHYSICAL(u4DstAddr));
	VA_MSB_REG(u4GfxHwId, fg_SRC_BSAD_H, GFX_PHYSICAL(u4SrcAddr));
	GFX_VA_MSB_CMDENQ(u4GfxHwId);

	if (GFX_CMDENQ(u4GfxHwId, GREG_DST_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE) |
	    GFX_CMDENQ(u4GfxHwId, GREG_BITBLT_MODE) |
	    GFX_CMDENQ(u4GfxHwId, GREG_DRAMQ_LEN) |
	    GFX_CMDENQ(u4GfxHwId, GREG_G_MODE))
		return -(int)E_GFX_OUT_OF_MEM;

	REG(u4GfxHwId, fg_SRC_CM_2) = 0;
	REG(u4GfxHwId, fg_SRC_HEIGHT_15) = 0;
	GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE);

	return (int)E_GFX_OK;
}

int GFX_BitBlt_NewMethod(unsigned long u4GfxHwId,
	unsigned int u4SrcX, unsigned int u4SrcY, unsigned int u4DstX,
	unsigned int u4DstY, unsigned int u4Width, unsigned int u4Height)
{
	unsigned int u4DstAddr, u4SrcAddr;
	unsigned int u4BppWidth;

	GFX_HW_FX_ENTRY;

	u4BppWidth = u4Width << _u4DstShift[u4GfxHwId];
	//destination base address
	u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
		(u4DstY * _u4DstPitch[u4GfxHwId]) +
		(u4DstX << _u4DstShift[u4GfxHwId]));

	REG(u4GfxHwId, fg_DST_BSAD) = GFX_PHYSICAL(u4DstAddr);
	//source base address
	u4SrcAddr = ((uintptr_t) _pu1SrcBase[u4GfxHwId] +
		(u4SrcY * _u4SrcPitch[u4GfxHwId]) +
		(u4SrcX << _u4SrcShift[u4GfxHwId]));

	REG(u4GfxHwId, fg_SRC_BSAD) = GFX_PHYSICAL(u4SrcAddr);
	REG(u4GfxHwId, fg_OP_MODE) = (unsigned int)OP_BITBLT;
	REG(u4GfxHwId, fg_CFMT_ENA) = (unsigned int)(
		_u4DstCM[u4GfxHwId] != _u4SrcCM[u4GfxHwId]);
	REG(u4GfxHwId, fg_SRC_WIDTH) = u4Width;

	u4Height -= 1;
	REG(u4GfxHwId, fg_SRC_HEIGHT) = u4Height & 0x007FF;
	REG(u4GfxHwId, fg_SRC_CM_2) = (u4Height >> 11) & 0x0F;
	REG(u4GfxHwId, fg_SRC_HEIGHT_15) = (u4Height >> 15) & 0x01;
	REG(u4GfxHwId, fg_SRC_HEIGHT_19_16) = (u4Height >> 16) & 0x0F;

	if (u4BppWidth > 32) {
		REG(u4GfxHwId, fg_BARREL_ENA) = 1;
		REG(u4GfxHwId, fg_PASS_ALU_ENA) = 1;
	} else {
		REG(u4GfxHwId, fg_BARREL_ENA) = 0;
		REG(u4GfxHwId, fg_PASS_ALU_ENA) = 0;
	}
	REG(u4GfxHwId, fg_FIRE) = GFX_ENGINE_FIRE;

	GFX_LOG_2048_HAL(u4Height);
	VA_MSB_REG(u4GfxHwId, fg_DST_BSAD_H, GFX_PHYSICAL(u4DstAddr));
	VA_MSB_REG(u4GfxHwId, fg_SRC_BSAD_H, GFX_PHYSICAL(u4SrcAddr));
	GFX_VA_MSB_CMDENQ(u4GfxHwId);

	if (GFX_CMDENQ(u4GfxHwId, GREG_DST_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE) |
	    GFX_CMDENQ(u4GfxHwId, GREG_BITBLT_MODE) |
	    GFX_CMDENQ(u4GfxHwId, GREG_G_MODE))
		return -(int)E_GFX_OUT_OF_MEM;

	REG(u4GfxHwId, fg_SRC_CM_2) = 0;
	REG(u4GfxHwId, fg_SRC_HEIGHT_15) = 0;
	GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE);
	REG(u4GfxHwId, fg_BARREL_ENA) = 0;
	REG(u4GfxHwId, fg_PASS_ALU_ENA) = 0;
	GFX_CMDENQ(u4GfxHwId, GREG_BITBLT_MODE);

	return (int)E_GFX_OK;
}

int GFX_RopBitblt(unsigned long u4GfxHwId, unsigned int u4SrcX,
	unsigned int u4SrcY, unsigned int u4DstX, unsigned int u4DstY,
	unsigned int u4Width, unsigned int u4Height,
	unsigned int u4RopCode)
{
	unsigned int u4SrcAddr, u4DstAddr;

	GFX_HW_FX_ENTRY;
	//destination base address
	u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
		(u4DstY * _u4DstPitch[u4GfxHwId]) +
		(u4DstX << _u4DstShift[u4GfxHwId]));

	REG(u4GfxHwId, fg_DST_BSAD) = GFX_PHYSICAL(u4DstAddr);

	//source base address
	u4SrcAddr = ((uintptr_t) _pu1SrcBase[u4GfxHwId] +
		(u4SrcY * _u4SrcPitch[u4GfxHwId]) +
		(u4SrcX << _u4SrcShift[u4GfxHwId]));

	REG(u4GfxHwId, fg_SRC_BSAD) = GFX_PHYSICAL(u4SrcAddr);
	REG(u4GfxHwId, fg_SRCALPHA_CHECK) = 1;
	REG(u4GfxHwId, fg_OP_MODE) = (unsigned int)OP_ROP_BITBLT;
	REG(u4GfxHwId, fg_CFMT_ENA) = (unsigned int)(
		_u4DstCM[u4GfxHwId] != _u4SrcCM[u4GfxHwId]);
	REG(u4GfxHwId, fg_SRC_WIDTH) = u4Width;
	REG(u4GfxHwId, fg_SRC_HEIGHT) = (u4Height - 1);
	REG(u4GfxHwId, fg_ROP_OPCODE) = u4RopCode;
	REG(u4GfxHwId, fg_NO_WR) = GFX_DISABLE;
	REG(u4GfxHwId, fg_FIRE) = GFX_ENGINE_FIRE;

	VA_MSB_REG(u4GfxHwId, fg_DST_BSAD_H, GFX_PHYSICAL(u4DstAddr));
	VA_MSB_REG(u4GfxHwId, fg_SRC_BSAD_H, GFX_PHYSICAL(u4SrcAddr));

	GFX_VA_MSB_CMDENQ(u4GfxHwId);

	if (GFX_CMDENQ(u4GfxHwId, GREG_DST_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE) |
	    GFX_CMDENQ(u4GfxHwId, GREG_BITBLT_MODE) |
	    GFX_CMDENQ(u4GfxHwId, GREG_ROP) |
	    GFX_CMDENQ(u4GfxHwId, GREG_G_MODE))
		return -(int)E_GFX_OUT_OF_MEM;

	return (int)E_GFX_OK;
}

int GFX_SetHoriToVertLineOpt(unsigned long u4GfxHwId,
	unsigned int u4IsCounterClockWise)
{
	REG(u4GfxHwId, fg_DSTPITCH_DEC) = u4IsCounterClockWise;
	return GFX_CMDENQ(u4GfxHwId, GREG_BITBLT_MODE);
}

int GFX_HoriToVertLine(unsigned long u4GfxHwId, unsigned int u4SrcX,
	unsigned int u4SrcY, unsigned int u4DstX, unsigned int u4DstY,
	unsigned int u4HoriLineWidth, int u4Is90CCW)
{
	unsigned int u4SrcAddr, u4DstAddr;

	GFX_HW_FX_ENTRY;
	VERIFY(u4HoriLineWidth != 0);

	u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
		(u4DstY * _u4DstPitch[u4GfxHwId]) +
		(u4DstX << _u4DstShift[u4GfxHwId]));
	REG(u4GfxHwId, fg_DST_BSAD) = GFX_PHYSICAL(u4DstAddr);

	/* source base address */
	u4SrcAddr = ((uintptr_t) _pu1SrcBase[u4GfxHwId] +
		(u4SrcY * _u4SrcPitch[u4GfxHwId]) +
		(u4SrcX << _u4SrcShift[u4GfxHwId]));

	REG(u4GfxHwId, fg_SRC_BSAD) = GFX_PHYSICAL(u4SrcAddr);
	REG(u4GfxHwId, fg_OP_MODE) = (unsigned int)OP_H2V_LINE;
	REG(u4GfxHwId, fg_SRC_WIDTH) = u4HoriLineWidth;
	REG(u4GfxHwId, fg_SRC_HEIGHT) = 0;
	REG(u4GfxHwId, fg_STR_DST_WIDTH) = 1;
	REG(u4GfxHwId, fg_STR_DST_HEIGHT) = (u4HoriLineWidth - 1);
	REG(u4GfxHwId, fg_DSTPITCH_DEC) = u4Is90CCW;
	REG(u4GfxHwId, fg_FIRE) = GFX_ENGINE_FIRE;
	VA_MSB_REG(u4GfxHwId, fg_DST_BSAD_H, GFX_PHYSICAL(u4DstAddr));
	VA_MSB_REG(u4GfxHwId, fg_SRC_BSAD_H, GFX_PHYSICAL(u4SrcAddr));
	GFX_VA_MSB_CMDENQ(u4GfxHwId);

	if (GFX_CMDENQ(u4GfxHwId, GREG_S_OSD_WIDTH) |
	    GFX_CMDENQ(u4GfxHwId, GREG_DST_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE) |
	    GFX_CMDENQ(u4GfxHwId, GREG_STR_DST_SIZE) |
	    GFX_CMDENQ(u4GfxHwId, GREG_BITBLT_MODE) |
	    GFX_CMDENQ(u4GfxHwId, GREG_G_MODE))
		return -(int)E_GFX_OUT_OF_MEM;

	REG(u4GfxHwId, fg_DSTPITCH_DEC) = 0;
	GFX_CMDENQ(u4GfxHwId, GREG_BITBLT_MODE);
	return (int)E_GFX_OK;
}

int GFX_RotateBmp(unsigned long u4GfxHwId, unsigned int *pu1SrcBase,
	unsigned int *pu1DstBase, unsigned int u4SrcX, unsigned int u4SrcY,
	unsigned int u4DstX, unsigned int u4DstY, unsigned int u4CM,
	unsigned int u4SrcPitch, unsigned int u4DstPitch, unsigned int u4Width,
	unsigned int u4Height, unsigned int u4Is90CCW)
{
	int i4Ret;
	unsigned int i, u4Dx, u4Dy;
	unsigned int u4Sx = u4SrcX, u4Sy = u4SrcY;
	unsigned int u4WidthStep = 2047;
	unsigned int u4WidthFlush;
	{
		i4Ret = GFX_SetSrc(u4GfxHwId, pu1SrcBase, u4CM, u4SrcPitch);

		if (i4Ret != (int)E_GFX_OK)
			return -(int)E_GFX_INV_ARG;

		i4Ret = GFX_SetDst(u4GfxHwId, pu1DstBase, u4CM, u4DstPitch);
		if (i4Ret != (int)E_GFX_OK)
			return -(int)E_GFX_INV_ARG;

		i4Ret = GFX_SetHoriToVertLineOpt(u4GfxHwId, u4Is90CCW);
		if (i4Ret != (int)E_GFX_OK)
			return -(int)E_GFX_INV_ARG;

		if (u4Is90CCW == (unsigned int)true) {
			u4Dx = u4DstX;
			u4Dy = u4DstY;
			while (u4Width != 0) {
				u4WidthFlush = (u4Width > u4WidthStep) ?
					u4WidthStep : u4Width;

				GFX_LOG_D("[gfx] _GfxRotate90 w %d,90=%d\n",
					  u4WidthFlush, u4Is90CCW);

				for (i = 0; i < u4Height; i++) {
				i4Ret = GFX_HoriToVertLine(u4GfxHwId, u4Sx,
				u4Sy++, u4Dx++, u4Dy, u4WidthFlush, u4Is90CCW);
				if (i4Ret != (int)E_GFX_OK)
					return -(int)E_GFX_INV_ARG;
				}
				u4Sx += u4WidthFlush;
				u4Sy = u4SrcY;
				u4Dx = u4DstX;
				u4Dy -= u4WidthFlush;
				u4Width -= u4WidthFlush;
			}
		} else {
			u4Dx = u4DstX;	//+(u4Height-1);
			u4Dy = u4DstY;	//-(u4Width-1);
			while (u4Width != 0) {
				u4WidthFlush = (u4Width > u4WidthStep) ?
					u4WidthStep : u4Width;

				GFX_LOG_D("[gfx] _GfxRotate90 width=%d,90=%d\n",
					  u4WidthFlush, u4Is90CCW);

				for (i = 0; i < u4Height; i++) {
				i4Ret = GFX_HoriToVertLine(u4GfxHwId,
					u4Sx, u4Sy++, u4Dx--, u4Dy,
					u4WidthFlush, u4Is90CCW);
				if (i4Ret != (int)E_GFX_OK)
					return -(int)E_GFX_INV_ARG;
				}
				u4Sy = u4SrcY;
				u4Dx = u4DstX;
				u4Sx += u4WidthFlush;
				u4Dy += u4WidthFlush;
				u4Width -= u4WidthFlush;
			}
		}

		i4Ret = GFX_Flush(u4GfxHwId);

		REG(u4GfxHwId, fg_DSTPITCH_DEC) = 0;
		GFX_CMDENQ(u4GfxHwId, GREG_BITBLT_MODE);
		if (i4Ret != (int)E_GFX_OK)
			return -(int)E_GFX_INV_ARG;
	}

	return (int)E_GFX_OK;
}

unsigned int Gfx_BytePerPixel(unsigned int u4ColorMode)
{
	switch (u4ColorMode) {
	case CM_YCbCr_CLUT8:
	case CM_RGB_CLUT8:
		return 1;

	case CM_CbYCrY422_DIRECT16:
	case CM_YCbYCr422_DIRECT16:
	case CM_RGB565_DIRECT16:
	case CM_ARGB1555_DIRECT16:
	case CM_ARGB4444_DIRECT16:
		return 2;

	case CM_AYCbCr8888_DIRECT32:
	case CM_ARGB8888_DIRECT32:
	default:
		return 4;
	}
}

int GFX_RotateMirrorFlipBmp(unsigned long u4GfxHwId,
	unsigned int *pu1SrcBase,
	unsigned int *pu1DstBase, unsigned int u4SrcX, unsigned int u4SrcY,
	unsigned int u4DstX, unsigned int u4DstY, unsigned int u4CM,
	unsigned int u4SrcPitch, unsigned int u4DstPitch, unsigned int u4Width,
	unsigned int u4Height, unsigned int u4Is90CCW, unsigned char u1AddOpt)
{
	int i4Ret;
	unsigned int i, u4Dx, u4Dy;
	unsigned int u4Sx = u4SrcX, u4Sy = u4SrcY;
	unsigned int u4WidthStep = 100;
	unsigned int u4WidthFlush;

	{
		i4Ret = GFX_SetSrc(u4GfxHwId, pu1SrcBase, u4CM, u4SrcPitch);
		if (i4Ret != (int)E_GFX_OK)
			return -(int)E_GFX_INV_ARG;

		i4Ret = GFX_SetDst(u4GfxHwId, pu1DstBase, u4CM, u4DstPitch);
		if (i4Ret != (int)E_GFX_OK)
			return -(int)E_GFX_INV_ARG;

		i4Ret = GFX_SetHoriToVertLineOpt(u4GfxHwId, u4Is90CCW);
		if (i4Ret != (int)E_GFX_OK)
			return -(int)E_GFX_INV_ARG;

		if (u4Is90CCW == (unsigned int)true) {
			u4Dx = u4DstX;
			u4Dy = u4DstY;

			while (u4Width != 0) {

				u4WidthFlush = (u4Width > u4WidthStep) ?
					u4WidthStep : u4Width;
				for (i = 0; i < u4Height; i++) {
				i4Ret = GFX_HoriToVertLine(u4GfxHwId, u4Sx,
					u4Sy++, u4Dx++, u4Dy,
					u4WidthFlush, u4Is90CCW);
					if (i4Ret != (int)E_GFX_OK)
						return -(int)E_GFX_INV_ARG;
					GFX_Flush(u4GfxHwId);
				}
				u4Sx += u4WidthFlush;
				u4Sy = u4SrcY;
				u4Dx = u4DstX;
				u4Dy -= u4WidthFlush;
				u4Width -= u4WidthFlush;
			}
		} else {
			u4Dx = u4DstX;	//+(u4Height-1);
			u4Dy = u4DstY;	//-(u4Width-1);
			while (u4Width != 0) {
				u4WidthFlush = (u4Width > u4WidthStep) ?
					u4WidthStep : u4Width;
				for (i = 0; i < u4Height; i++) {
				i4Ret = GFX_HoriToVertLine(u4GfxHwId, u4Sx,
					u4Sy++, u4Dx--, u4Dy, u4WidthFlush,
					u4Is90CCW);
				if (i4Ret != (int)E_GFX_OK)
					return -(int)E_GFX_INV_ARG;
				GFX_Flush(u4GfxHwId);
				}
				u4Sy = u4SrcY;
				u4Dx = u4DstX;
				u4Sx += u4WidthFlush;
				u4Dy += u4WidthFlush;
				u4Width -= u4WidthFlush;
			}
		}

		i4Ret = GFX_Flush(u4GfxHwId);

		REG(u4GfxHwId, fg_DSTPITCH_DEC) = 0;
		GFX_CMDENQ(u4GfxHwId, GREG_BITBLT_MODE);
		if (i4Ret != (int)E_GFX_OK)
			return -(int)E_GFX_INV_ARG;
	}

	return (int)E_GFX_OK;
}

int GFX_RotateBmpSw(unsigned int *pu1SrcBase,
	unsigned int *pu1DstBase, unsigned int u4SrcX, unsigned int u4SrcY,
	unsigned int u4DstX, unsigned int u4DstY, unsigned int u4CM,
	unsigned int u4SrcPitch, unsigned int u4DstPitch, unsigned int u4Width,
	unsigned int u4Height, unsigned int u4Is90CCW)
{

	unsigned int u4Sx, u4Sy;
	unsigned int u4CMByte;
	unsigned int i, j, m, n;
	unsigned short *pu2Src, *pu2Dst;
	unsigned int *pu4Src, *pu4Dst;
	unsigned int *pu1Src, *pu1Dst;
	unsigned int SumSrcPitch;
	unsigned int u4SrcOsdWidth;
	unsigned int u4DstOsdPitch;

	pu4Src = (unsigned int *)pu1SrcBase;
	pu4Dst = (unsigned int *)pu1DstBase;
	pu2Src = (unsigned short *)pu1SrcBase;
	pu2Dst = (unsigned short *)pu1DstBase;
	pu1Src = (unsigned int *)pu1SrcBase;
	pu1Dst = (unsigned int *)pu1DstBase;

	u4Sx = u4SrcX;
	u4Sy = u4SrcY;
	u4CMByte = Gfx_BytePerPixel(u4CM);

	//define over
	{
		if (u4CMByte == 4) {
			u4SrcOsdWidth = u4SrcPitch / 4;
			u4DstOsdPitch = u4DstPitch / 4;
			//ccw
			if (u4Is90CCW == (unsigned int)true) {
				for (j = u4Sy, m = u4DstX; j < u4Height + u4Sy;
					j++, m++) {
				SumSrcPitch = j * u4SrcOsdWidth;
				for (i = u4Sx, n = u4DstY; i < u4Width + u4Sx;
				i++, n--) {
					pu4Dst[m + n * u4DstOsdPitch] =
					    pu4Src[i + SumSrcPitch];
				}
				}
			} else {
				for (j = u4Sy, m = u4DstX + u4Height - 1;
					j < u4Height + u4Sy;
					j++, m--) {
				SumSrcPitch = j * u4SrcOsdWidth;
				for (i = u4Sx, n = u4DstY; i < u4Width + u4Sx;
				i++, n++) {
					pu4Dst[m + n * u4DstOsdPitch] =
					    pu4Src[i + SumSrcPitch];
				}
				}
			}
		} else if (u4CMByte == 2) {
			u4SrcOsdWidth = u4SrcPitch / 2;
			u4DstOsdPitch = u4DstPitch / 2;
			//ccw
			if (u4Is90CCW == (unsigned int)true) {
			for (j = u4Sy, m = u4DstX; j < u4Height + u4Sy;
			j++, m++) {
				SumSrcPitch = j * u4SrcOsdWidth;
				for (i = u4Sx, n = u4DstY; i < u4Width + u4Sx;
				i++, n--) {
					pu2Dst[m + n * u4DstOsdPitch] =
					    pu2Src[i + SumSrcPitch];
				}
				}
			} else {
			for (j = u4Sy, m = u4DstX + u4Height - 1;
			j < u4Height + u4Sy;
			     j++, m--) {
				SumSrcPitch = j * u4SrcOsdWidth;
				for (i = u4Sx, n = u4DstY; i < u4Width + u4Sx;
				i++, n++) {
					pu2Dst[m + n * u4DstOsdPitch] =
					    pu2Src[i + SumSrcPitch];
				}
				}
			}
		} else if (u4CMByte == 1) {
			u4SrcOsdWidth = u4SrcPitch;
			u4DstOsdPitch = u4DstPitch;
			//ccw
			if (u4Is90CCW == (unsigned int)true) {
			for (j = u4Sy, m = u4DstX; j < u4Height + u4Sy;
				j++, m++) {
				SumSrcPitch = j * u4SrcOsdWidth;
				for (i = u4Sx, n = u4DstY; i < u4Width + u4Sx;
				i++, n--) {
					pu1Dst[m + n * u4DstOsdPitch] =
					    pu1Src[i + SumSrcPitch];
				}
				}
			} else {
			for (j = u4Sy, m = u4DstX + u4Height - 1;
			j < u4Height + u4Sy; j++, m--) {
				SumSrcPitch = j * u4SrcOsdWidth;
				for (i = u4Sx, n = u4DstY; i < u4Width + u4Sx;
				i++, n++) {
					pu1Dst[m + n * u4DstOsdPitch] =
					    pu1Src[i + SumSrcPitch];
				}
			}
			}
		}
	}
	return (int)E_GFX_OK;
}

int GFX_Blend(unsigned long u4GfxHwId, unsigned int u4SrcX,
	unsigned int u4SrcY, unsigned int u4DstX, unsigned int u4DstY,
	unsigned int u4Width, unsigned int u4Height)
{
	unsigned int u4SrcAddr, u4DstAddr;

	GFX_HW_FX_ENTRY;
#ifndef GFX_SUPPORT_DIFF_CM
	if (_u4SrcCM[u4GfxHwId] != _u4DstCM[u4GfxHwId])
		return -(int)E_GFX_INV_ARG;
#endif
	u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
	(u4DstY * _u4DstPitch[u4GfxHwId]) +
	(u4DstX << _u4DstShift[u4GfxHwId]));

	REG(u4GfxHwId, fg_DST_BSAD) = GFX_PHYSICAL(u4DstAddr);

	u4SrcAddr = ((uintptr_t) _pu1SrcBase[u4GfxHwId] +
	(u4SrcY * _u4SrcPitch[u4GfxHwId]) +
	(u4SrcX << _u4SrcShift[u4GfxHwId]));

	REG(u4GfxHwId, fg_SRC_BSAD) = GFX_PHYSICAL(u4SrcAddr);

	REG(u4GfxHwId, fg_OP_MODE) = (unsigned int)OP_ALPHA_BITBLT;
	REG(u4GfxHwId, fg_SRC_WIDTH) = u4Width;

	u4Height -= 1;
	REG(u4GfxHwId, fg_SRC_HEIGHT) = u4Height & 0x007FF;
	REG(u4GfxHwId, fg_SRC_CM_2) = (u4Height >> 11) & 0x0F;
	REG(u4GfxHwId, fg_SRC_HEIGHT_15) = (u4Height >> 15) & 0x01;
	REG(u4GfxHwId, fg_SRC_HEIGHT_19_16) = (u4Height >> 16) & 0x0F;
	GFX_LOG_2048_HAL(u4Height);

	REG(u4GfxHwId, fg_FIRE) = GFX_ENGINE_FIRE;

	VA_MSB_REG(u4GfxHwId, fg_DST_BSAD_H, GFX_PHYSICAL(u4DstAddr));
	VA_MSB_REG(u4GfxHwId, fg_SRC_BSAD_H, GFX_PHYSICAL(u4SrcAddr));

	GFX_VA_MSB_CMDENQ(u4GfxHwId);

	if (GFX_CMDENQ(u4GfxHwId, GREG_DST_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE) |
	    GFX_CMDENQ(u4GfxHwId, GREG_BITBLT_MODE) |
	    GFX_CMDENQ(u4GfxHwId, GREG_G_MODE))
		return -(int)E_GFX_OUT_OF_MEM;

	REG(u4GfxHwId, fg_SRC_CM_2) = 0;
	REG(u4GfxHwId, fg_SRC_HEIGHT_15) = 0;
	GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE);

	return (int)E_GFX_OK;
}

int GFX_NormalComposePass(unsigned long u4GfxHwId,
	unsigned int u4SrcX, unsigned int u4SrcY, unsigned int u4DstX,
	unsigned int u4DstY, unsigned int u4Width, unsigned int u4Height,
	unsigned int u4Pass, unsigned int u4Param)
{
	unsigned int u4SrcAddr, u4DstAddr;

	GFX_HW_FX_ENTRY;

	u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
		(u4DstY * _u4DstPitch[u4GfxHwId]) +
		(u4DstX << _u4DstShift[u4GfxHwId]));
	REG(u4GfxHwId, fg_DST_BSAD) = GFX_PHYSICAL(u4DstAddr);
	u4SrcAddr = ((uintptr_t) _pu1SrcBase[u4GfxHwId] +
	(u4SrcY * _u4SrcPitch[u4GfxHwId]) +
	(u4SrcX << _u4DstShift[u4GfxHwId]));

	REG(u4GfxHwId, fg_SRC_BSAD) = GFX_PHYSICAL(u4SrcAddr);
	REG(u4GfxHwId, fg_ALCOM_PASS) = u4Pass;
	REG(u4GfxHwId, fg_ALPHA_VALUE) = u4Param;
	REG(u4GfxHwId, fg_OSD_WIDTH) = (_u4DstPitch[u4GfxHwId] >> 4);
	REG(u4GfxHwId, fg_SRC_PITCH) = (_u4SrcPitch[u4GfxHwId] >> 4);

	REG(u4GfxHwId, fg_OP_MODE) =
		(unsigned int)OP_ALPHA_COMPOS_BITBLT;
	REG(u4GfxHwId, fg_SRC_WIDTH) = u4Width;
	REG(u4GfxHwId, fg_SRC_HEIGHT) = (u4Height - 1);
	REG(u4GfxHwId, fg_ALCOM_NORMAL) = 1;
	REG(u4GfxHwId, fg_FIRE) = GFX_ENGINE_FIRE;

	VA_MSB_REG(u4GfxHwId, fg_DST_BSAD_H, u4DstAddr);
	VA_MSB_REG(u4GfxHwId, fg_SRC_BSAD_H, u4SrcAddr);
	GFX_VA_MSB_CMDENQ(u4GfxHwId);

	if (GFX_CMDENQ(u4GfxHwId, GREG_DST_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE) |
	    GFX_CMDENQ(u4GfxHwId, GREG_BITBLT_MODE) |
	    GFX_CMDENQ(u4GfxHwId, GREG_S_OSD_WIDTH) |
	    GFX_CMDENQ(u4GfxHwId, GREG_G_MODE))
		return -(int)E_GFX_OUT_OF_MEM;

	return (int)E_GFX_OK;
}

int GFX_AlphaComposePass(unsigned long u4GfxHwId,
	unsigned int u4SrcX, unsigned int u4SrcY, unsigned int u4DstX,
	unsigned int u4DstY, unsigned int u4Width, nsigned int u4Height,
	unsigned int u4Pass, unsigned int u4Param)
{
	unsigned int u4SrcAddr, u4DstAddr;

	GFX_HW_FX_ENTRY;

	u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
		(u4DstY * _u4DstPitch[u4GfxHwId]) +
		(u4DstX << _u4DstShift[u4GfxHwId]));
	REG(u4GfxHwId, fg_DST_BSAD) = GFX_PHYSICAL(u4DstAddr);

	u4SrcAddr = ((uintptr_t) _pu1SrcBase[u4GfxHwId] +
		(u4SrcY * _u4SrcPitch[u4GfxHwId]) +
		(u4SrcX << _u4DstShift[u4GfxHwId]));

	REG(u4GfxHwId, fg_SRC_BSAD) = GFX_PHYSICAL(u4SrcAddr);
	REG(u4GfxHwId, fg_ALCOM_PASS) = u4Pass;
	REG(u4GfxHwId, fg_ALPHA_VALUE) = u4Param;
	REG(u4GfxHwId, fg_OSD_WIDTH) = (_u4DstPitch[u4GfxHwId] >> 4);
	REG(u4GfxHwId, fg_SRC_PITCH) = (_u4SrcPitch[u4GfxHwId] >> 4);

	REG(u4GfxHwId, fg_OP_MODE) =
		(unsigned int)OP_ALPHA_COMPOS_BITBLT;
	REG(u4GfxHwId, fg_SRC_WIDTH) = u4Width;
	REG(u4GfxHwId, fg_SRC_HEIGHT) = (u4Height - 1);
	REG(u4GfxHwId, fg_ALCOM_NORMAL) = 0;
	REG(u4GfxHwId, fg_FIRE) = GFX_ENGINE_FIRE;
	VA_MSB_REG(u4GfxHwId, fg_DST_BSAD_H, GFX_PHYSICAL(u4DstAddr));
	VA_MSB_REG(u4GfxHwId, fg_SRC_BSAD_H, GFX_PHYSICAL(u4SrcAddr));

	GFX_VA_MSB_CMDENQ(u4GfxHwId);

	if (GFX_CMDENQ(u4GfxHwId, GREG_DST_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE) |
	    GFX_CMDENQ(u4GfxHwId, GREG_BITBLT_MODE) |
	    GFX_CMDENQ(u4GfxHwId, GREG_S_OSD_WIDTH) |
	    GFX_CMDENQ(u4GfxHwId, GREG_G_MODE))
		return -(int)E_GFX_OUT_OF_MEM;

	return (int)E_GFX_OK;
}

int GFX_SetColCnvFmt(unsigned long u4GfxHwId, unsigned int u4YCFmt,
	unsigned int u4SwapMode, unsigned int u4VidStd,
	unsigned int u4VidSys)
{
	REG(u4GfxHwId, fg_YC_FMT) = u4YCFmt;
	REG(u4GfxHwId, fg_VSTD) = u4VidStd;
	REG(u4GfxHwId, fg_VSYS) = u4VidSys;
	REG(u4GfxHwId, fg_SWAP_NEW_MODE) = 0x1;
	REG(u4GfxHwId, fg_SWAP_MODE) = u4SwapMode;
	REG(u4GfxHwId, fg_VSCLIP) = GFX_ENABLE;
	REG(u4GfxHwId, fg_ALPHA_VALUE) = 0xFF;

	return (int)E_GFX_OK;
}

int GFX_SetColCnvSrc(unsigned long u4GfxHwId,
	unsigned int *pu1SrcLuma,
	unsigned int u4SrcLumaPitch, unsigned int *pu1SrcChroma,
	unsigned int u4SrcChromaPitch, unsigned int u4FieldPic)
{

	REG(u4GfxHwId, fg_SRC_BSAD) = (uintptr_t) pu1SrcLuma;
	REG(u4GfxHwId, fg_SRCCBCR_BSAD) = (uintptr_t) pu1SrcChroma;
	REG(u4GfxHwId, fg_SRC_PITCH) = u4SrcLumaPitch;
	REG(u4GfxHwId, fg_SRCCBCR_PITCH) = u4SrcChromaPitch;
	REG(u4GfxHwId, fg_FLD_PIC) = u4FieldPic;
	VA_MSB_REG(u4GfxHwId, fg_SRC_BSAD_H, pu1SrcLuma);
	VA_MSB_REG(u4GfxHwId, fg_SRCCBCR_BSAD_H, pu1SrcChroma);
	GFX_VA_MSB_CMDENQ(u4GfxHwId);

	if (GFX_CMDENQ(u4GfxHwId, GREG_SRC_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_S_OSD_WIDTH) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRCCBCR_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRCCBCR_PITCH) |
	    GFX_CMDENQ(u4GfxHwId, GREG_BITBLT_MODE))
		return -(int)E_GFX_OUT_OF_MEM;

	return (int)E_GFX_OK;
}

int GFX_SetColCnvFmtEx(unsigned long u4GfxHwId, unsigned int u4YCFmt,
	unsigned int u4SwapMode, unsigned int u4VidStd, unsigned int u4VidSys,
	unsigned int u4VSCLIP)
{
	REG(u4GfxHwId, fg_YC_FMT) = u4YCFmt;
	REG(u4GfxHwId, fg_VSTD) = u4VidStd;
	REG(u4GfxHwId, fg_VSYS) = u4VidSys;
	REG(u4GfxHwId, fg_SWAP_MODE) = u4SwapMode;
	REG(u4GfxHwId, fg_VSCLIP) = u4VSCLIP;
	REG(u4GfxHwId, fg_ALPHA_VALUE) = 0xFF;

	return (int)E_GFX_OK;
}

int GFX_SetColCnvSrcEx(unsigned long u4GfxHwId,
	unsigned int *pu1SrcLuma,
	unsigned int u4SrcLumaPitch, unsigned int *pu1SrcChroma,
	unsigned int u4SrcChromaPitch, unsigned int u4FieldPic)
{
	REG(u4GfxHwId, fg_SRC_BSAD) = (uintptr_t) pu1SrcLuma;
	REG(u4GfxHwId, fg_SRCCBCR_BSAD) = (uintptr_t) pu1SrcChroma;
	REG(u4GfxHwId, fg_SRC_PITCH) = (u4SrcLumaPitch >> 4);
	REG(u4GfxHwId, fg_SRCCBCR_PITCH) = (u4SrcChromaPitch >> 4);
	REG(u4GfxHwId, fg_FLD_PIC) = u4FieldPic;
	VA_MSB_REG(u4GfxHwId, fg_SRC_BSAD_H, pu1SrcLuma);
	VA_MSB_REG(u4GfxHwId, fg_SRCCBCR_BSAD_H, pu1SrcChroma);
	GFX_VA_MSB_CMDENQ(u4GfxHwId);

	if (GFX_CMDENQ(u4GfxHwId, GREG_SRC_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_S_OSD_WIDTH) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRCCBCR_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRCCBCR_PITCH) |
	    GFX_CMDENQ(u4GfxHwId, GREG_BITBLT_MODE))
		return -(int)E_GFX_OUT_OF_MEM;

	return (int)E_GFX_OK;
}

int GFX_ColConv(unsigned long u4GfxHwId, unsigned int u4X,
	unsigned int u4Y,
	unsigned int u4Width, unsigned int u4Height)
{
	unsigned int u4DstAddr;

	u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
		(u4Y * _u4DstPitch[u4GfxHwId]) +
		(u4X << _u4DstShift[u4GfxHwId]));
	REG(u4GfxHwId, fg_DST_BSAD) = GFX_PHYSICAL(u4DstAddr);
	REG(u4GfxHwId, fg_OP_MODE) = (unsigned int)OP_YCRCB_RGB_CNV;
	REG(u4GfxHwId, fg_SRC_WIDTH) = u4Width;
	REG(u4GfxHwId, fg_SRC_HEIGHT) = (u4Height - 1);
	REG(u4GfxHwId, fg_FIRE) = GFX_ENGINE_FIRE;
	VA_MSB_REG(u4GfxHwId, fg_DST_BSAD_H, GFX_PHYSICAL(u4DstAddr));

	GFX_VA_MSB_CMDENQ(u4GfxHwId);

	if (GFX_CMDENQ(u4GfxHwId, GREG_DST_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE) |
	    GFX_CMDENQ(u4GfxHwId, GREG_BITBLT_MODE) |
	    GFX_CMDENQ(u4GfxHwId, GREG_G_MODE))
		return -(int)E_GFX_OUT_OF_MEM;

	return (int)E_GFX_OK;
}

int GFX_StretchBlt(unsigned long u4GfxHwId, unsigned int u4SrcX,
	unsigned int u4SrcY, unsigned int u4SrcW, unsigned int u4SrcH,
	unsigned int u4DstX, unsigned int u4DstY, unsigned int u4DstW,
	unsigned int u4DstH)
{

	unsigned int u4MnV = 0;
	unsigned int u4MnH = 0;
	unsigned int u4SrcAddr, u4DstAddr;

	GFX_HW_FX_ENTRY;

#ifndef GFX_SUPPORT_DIFF_CM
	if (_u4DstShift[u4GfxHwId] != _u4SrcShift[u4GfxHwId])
		return -(int)E_GFX_INV_ARG;
#endif

	u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
		(u4DstY * _u4DstPitch[u4GfxHwId]) +
		(u4DstX << _u4DstShift[u4GfxHwId]));
	REG(u4GfxHwId, fg_DST_BSAD) = GFX_PHYSICAL(u4DstAddr);

	u4SrcAddr = ((uintptr_t) _pu1SrcBase[u4GfxHwId] +
		(u4SrcY * _u4SrcPitch[u4GfxHwId]) +
		(u4SrcX << _u4SrcShift[u4GfxHwId]));
	REG(u4GfxHwId, fg_SRC_BSAD) = GFX_PHYSICAL(u4SrcAddr);

	if ((u4DstW == u4SrcW) || (u4DstW == 1)) {
		u4MnH = 0x00010000;
		u4SrcW = u4DstW;
	} else if (u4SrcW > u4DstW) {
		u4MnH = (u4SrcW << 16) / (u4DstW - 1);
		if (((u4SrcW << 16) % (u4DstW - 1)) == 0)
			u4MnH--;
	} else {

		unsigned int u4Step;

		u4Step = (u4SrcW << 16) / (u4DstW);
		if ((u4Step * (u4DstW - 1)) < ((u4SrcW - 1) << 16)) {
			u4MnH = (u4SrcW << 16) / (u4DstW - 1);
			if (u4MnH == 0x10000)
				u4MnH--;
		} else
			u4MnH = u4Step;
	}

	if ((u4DstH == u4SrcH) || (u4DstH == 1)) {
		u4MnV = 0x00010000;
		u4SrcH = u4DstH;
	} else {
		u4MnV = (u4SrcH << 16) / (u4DstH - 1);
		if (u4MnV == 0x00010000)
			u4MnV = (u4SrcH << 16) / (u4DstH);
	}

	REG(u4GfxHwId, fg_OP_MODE) = (unsigned int)OP_STRETCH_BITBLT;
	REG(u4GfxHwId, fg_SRC_WIDTH) = u4SrcW;

	u4SrcH -= 1;
	REG(u4GfxHwId, fg_SRC_HEIGHT) = u4SrcH & 0x007FF;
	REG(u4GfxHwId, fg_SRC_CM_2) = (u4SrcH >> 11) & 0x0F;
	REG(u4GfxHwId, fg_SRC_HEIGHT_15) = (u4SrcH >> 15) & 0x01;
	REG(u4GfxHwId, fg_SRC_HEIGHT_19_16) = (u4SrcH >> 16) & 0x0F;

	REG(u4GfxHwId, fg_STR_DST_WIDTH) = u4DstW;
	REG(u4GfxHwId, fg_STR_DST_HEIGHT) = (u4DstH - 1);
	REG(u4GfxHwId, fg_STR_BLT_H) = u4MnH;
	REG(u4GfxHwId, fg_STR_BLT_V) = u4MnV;
	REG(u4GfxHwId, fg_FIRE) = GFX_ENGINE_FIRE;

	VA_MSB_REG(u4GfxHwId, fg_DST_BSAD_H, GFX_PHYSICAL(u4DstAddr));
	VA_MSB_REG(u4GfxHwId, fg_SRC_BSAD_H, GFX_PHYSICAL(u4SrcAddr));

	GFX_VA_MSB_CMDENQ(u4GfxHwId);

	if (GFX_CMDENQ(u4GfxHwId, GREG_DST_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE) |
	    GFX_CMDENQ(u4GfxHwId, GREG_G_PGIG) |
	    GFX_CMDENQ(u4GfxHwId, GREG_STR_BLT_H) |
	    GFX_CMDENQ(u4GfxHwId, GREG_STR_BLT_V) |
	    GFX_CMDENQ(u4GfxHwId, GREG_STR_DST_SIZE) |
	    GFX_CMDENQ(u4GfxHwId, GREG_G_MODE))
		return -(int)E_GFX_OUT_OF_MEM;

	REG(u4GfxHwId, fg_SRC_CM_2) = 0;
	REG(u4GfxHwId, fg_SRC_HEIGHT_15) = 0;
	GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE);
	REG(u4GfxHwId, fg_SRC_HEIGHT_19_16) = 0;
	GFX_CMDENQ(u4GfxHwId, GREG_G_PGIG);

	REG(u4GfxHwId, fg_STR_DST_WIDTH) = 0;
	REG(u4GfxHwId, fg_STR_DST_HEIGHT) = 0;
	REG(u4GfxHwId, fg_STR_BLT_H) = 0;
	REG(u4GfxHwId, fg_STR_BLT_V) = 0;
	GFX_CMDENQ(u4GfxHwId, GREG_STR_BLT_H);
	GFX_CMDENQ(u4GfxHwId, GREG_STR_BLT_V);
	GFX_CMDENQ(u4GfxHwId, GREG_STR_DST_SIZE);

	return (int)E_GFX_OK;
}

int GFX_AlphaMapBitBlt(unsigned long u4GfxHwId, unsigned int u4SrcX,
	unsigned int u4SrcY, unsigned int u4DstX, unsigned int u4DstY,
	unsigned int u4Width, unsigned int u4Height)
{
	unsigned int u4SrcAddr, u4DstAddr;

	if (_u4SrcCM[u4GfxHwId] != (unsigned int)CM_RGB_CLUT8)
		return -(int)E_GFX_INV_ARG;

	if ((_u4DstCM[u4GfxHwId] != (unsigned int)CM_ARGB8888_DIRECT32) &&
	    (_u4DstCM[u4GfxHwId] != (unsigned int)CM_ARGB4444_DIRECT16) &&
	    (_u4DstCM[u4GfxHwId] != (unsigned int)CM_ARGB1555_DIRECT16))
		return -(int)E_GFX_INV_ARG;

	u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
		(u4DstY * _u4DstPitch[u4GfxHwId]) +
		(u4DstX << _u4DstShift[u4GfxHwId]));

	REG(u4GfxHwId, fg_DST_BSAD) = GFX_PHYSICAL(u4DstAddr);
	u4SrcAddr = ((uintptr_t) _pu1SrcBase[u4GfxHwId] +
		(u4SrcY * _u4SrcPitch[u4GfxHwId]) +
		(u4SrcX << _u4SrcShift[u4GfxHwId]));

	REG(u4GfxHwId, fg_SRC_BSAD) = GFX_PHYSICAL(u4SrcAddr);
	REG(u4GfxHwId, fg_OP_MODE) = (unsigned int)OP_ALPHA_MAP_BITBLT;
	REG(u4GfxHwId, fg_SRC_WIDTH) = u4Width;
	REG(u4GfxHwId, fg_SRC_HEIGHT) = (u4Height - 1);
	REG(u4GfxHwId, fg_FIRE) = GFX_ENGINE_FIRE;

	VA_MSB_REG(u4GfxHwId, fg_DST_BSAD_H, GFX_PHYSICAL(u4DstAddr));
	VA_MSB_REG(u4GfxHwId, fg_SRC_BSAD_H, GFX_PHYSICAL(u4SrcAddr));

	GFX_VA_MSB_CMDENQ(u4GfxHwId);

	if (GFX_CMDENQ(u4GfxHwId, GREG_DST_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE) |
	    GFX_CMDENQ(u4GfxHwId, GREG_G_MODE))
		return -(int)E_GFX_OUT_OF_MEM;

	return (int)E_GFX_OK;
}

int GFX_Reset_Engine(unsigned long u4GfxHwId)
{
	return GFX_HwResetEngine(u4GfxHwId);
}

int GFX_Reset_CmdQue(unsigned long u4GfxHwId)
{
	return GFX_HwResetCmdQue(u4GfxHwId);
}

void GFX_QueryCmdQueInfo(unsigned long u4GfxHwId)
{
	GFX_CmdQueDbgInfo(u4GfxHwId);
}

unsigned int GFX_QueryFlushCount(unsigned long u4GfxHwId)
{
	return _GFX_GetFlushCount(u4GfxHwId);
}

unsigned int GFX_QueryHwInterruptCount(unsigned long u4GfxHwId)
{
	return _GFX_GetInterruptCount(u4GfxHwId);
}

void GFX_SetCqCapacity(unsigned long u4GfxHwId, int i4Capacity)
{
	GFX_CmdQueSetCqCapacity(u4GfxHwId, i4Capacity);
}

int GFX_SetLegalAddress(unsigned long u4GfxHwId, unsigned int u4Start,
	unsigned int u4End)
{
	REG(u4GfxHwId, fg_WR_PROT_EN) = GFX_ENABLE;
	REG(u4GfxHwId, fg_LEGAL_AD_START) = GFX_PHYSICAL_SAFE(u4Start);
	REG(u4GfxHwId, fg_LEGAL_AD_END) = GFX_PHYSICAL_SAFE(u4End);

	VA_MSB_REG(u4GfxHwId, fg_LEGAL_AD_START_H, u4Start);
	VA_MSB_REG(u4GfxHwId, fg_LEGAL_AD_END_H, u4End);
	GFX_VA_MSB_CMDENQ(u4GfxHwId);

	if (GFX_CMDENQ(u4GfxHwId, GREG_LEGAL_START_ADDR) |
	    GFX_CMDENQ(u4GfxHwId, GREG_LEGAL_END_ADDR))
		return -(int)E_GFX_INV_ARG;

	return (int)E_GFX_OK;
}

int GFX_ComposeLoop(unsigned long u4GfxHwId, unsigned int u4SrcX,
	unsigned int u4SrcY, unsigned int u4DstX, unsigned int u4DstY,
	unsigned int u4Width, unsigned int u4Height, unsigned int u4Ar,
	unsigned int u4OpCode, unsigned int u4RectSrc)
{
	unsigned int i4Ret;

	i4Ret = GFX_ComposeLoopEx(u4GfxHwId,
		u4SrcX, u4SrcY, u4DstX, u4DstY, u4Width, u4Height,
		0, u4Ar, u4OpCode, u4RectSrc);

	return i4Ret;
}

int GFX_ComposeLoopEx(unsigned long u4GfxHwId, unsigned int u4SrcX,
	unsigned int u4SrcY, unsigned int u4DstX, unsigned int u4DstY,
	unsigned int u4Width, unsigned int u4Height,
	unsigned int u4AlComNormal, unsigned int u4Ar,
	unsigned int u4OpCode, unsigned int u4RectSrc)
{
	unsigned int u4SrcAddr, u4DstAddr;

	REG(u4GfxHwId, fg_DIFF_CM) = (
		(_u4DstCM[u4GfxHwId] != _u4SrcCM[u4GfxHwId] ? 1 : 0));
	if (u4AlComNormal)
		u4Ar = 255;

	REG(u4GfxHwId, fg_SRC2_BSAD_ENA) = 0;
	if (GFX_CMDENQ(u4GfxHwId, GREG_SRCCBCR_PITCH))
		return -(int)E_GFX_OUT_OF_MEM;

	u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
		(u4DstY * _u4DstPitch[u4GfxHwId]) +
		(u4DstX << _u4DstShift[u4GfxHwId]));

	REG(u4GfxHwId, fg_DST_BSAD) = GFX_PHYSICAL(u4DstAddr);
	u4SrcAddr = ((uintptr_t) _pu1SrcBase[u4GfxHwId] +
		(u4SrcY * _u4SrcPitch[u4GfxHwId]) +
		(u4SrcX << _u4SrcShift[u4GfxHwId]));

	REG(u4GfxHwId, fg_SRC_BSAD) = GFX_PHYSICAL(u4SrcAddr);
	REG(u4GfxHwId, fg_OP_MODE) = OP_LOOP_ALPAH_COMPOS;
	REG(u4GfxHwId, fg_SRC_WIDTH) = u4Width;
	// set src height> 2048
	u4Height -= 1;
	REG(u4GfxHwId, fg_SRC_HEIGHT) = u4Height & 0x007FF;
	REG(u4GfxHwId, fg_SRC_CM_2) = (u4Height >> 11) & 0x0F;
	REG(u4GfxHwId, fg_SRC_HEIGHT_15) = (u4Height >> 15) & 0x01;
	REG(u4GfxHwId, fg_SRC_HEIGHT_19_16) = (u4Height >> 16) & 0x0F;

	REG(u4GfxHwId, fg_ALCOM_AR) = u4Ar;
	REG(u4GfxHwId, fg_ALCOM_OPCODE) = u4OpCode;
	REG(u4GfxHwId, fg_ALCOM_RECT_SRC) = u4RectSrc;
	REG(u4GfxHwId, fg_ALCOM_NORMAL) = u4AlComNormal;
	REG(u4GfxHwId, fg_FIRE) = GFX_ENGINE_FIRE;
	VA_MSB_REG(u4GfxHwId, fg_DST_BSAD_H, GFX_PHYSICAL(u4DstAddr));
	VA_MSB_REG(u4GfxHwId, fg_SRC_BSAD_H, GFX_PHYSICAL(u4SrcAddr));

	GFX_VA_MSB_CMDENQ(u4GfxHwId);

	if (GFX_CMDENQ(u4GfxHwId, GREG_BITBLT_MODE) |
	    GFX_CMDENQ(u4GfxHwId, GREG_DST_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE) |
	    GFX_CMDENQ(u4GfxHwId, GREG_ALCOM_LOOP) |
	    GFX_CMDENQ(u4GfxHwId, GREG_G_MODE))
		return -(int)E_GFX_OUT_OF_MEM;

	return (int)E_GFX_OK;
}

int GFX_SetPreMultiply(unsigned long u4GfxHwId,
	unsigned int u4PremultSrcRdEn,
	unsigned int u4PremultDstWrEn, unsigned int u4PremultDstRdEn)
{
	REG(u4GfxHwId, fg_PREMULT_SRCRD_ENA) = u4PremultSrcRdEn;
	REG(u4GfxHwId, fg_PREMULT_DSTWR_ENA) = u4PremultDstWrEn;
	REG(u4GfxHwId, fg_PREMULT_DSTRD_ENA) = u4PremultDstRdEn;
	REG(u4GfxHwId, fg_SRC_OVERFLOW_ENA) = ((u4PremultSrcRdEn == 1) ||
		(u4PremultDstRdEn == 1));
	GFX_CMDENQ(u4GfxHwId, GREG_ALCOM_LOOP);

	return (int)E_GFX_OK;
}

int GFX_2SrcBlendingBitblt(unsigned long u4GfxHwId,
	unsigned int u4SrcX, unsigned int u4SrcY, unsigned int u4DstX,
	unsigned int u4DstY, unsigned int u4Width, unsigned int u4Height,
	unsigned int u4Ar, unsigned int u4OpCode, unsigned int u4RectSrc,
	unsigned int u4Src2En)
{
	unsigned int u4SrcAddr, u4DstAddr, u4Src2Addr;

#ifndef GFX_SUPPORT_DIFF_CM
	if (_u4SrcCM[u4GfxHwId] != _u4DstCM[u4GfxHwId])
		return -(int)E_GFX_INV_ARG;
#endif

	if (u4OpCode == (unsigned int)E_AC_CLEAR) {
		unsigned int u4RectColor = REG(u4GfxHwId, fg_RECT_COLOR);

		GFX_SetColor(u4GfxHwId, 0x0);
		GFX_Fill(u4GfxHwId, u4DstX, u4DstY, u4Width, u4Height);

		// restore original value
		GFX_SetColor(u4GfxHwId, u4RectColor);
		return (int)E_GFX_OK;
	}
	u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
		(u4DstY * _u4DstPitch[u4GfxHwId]) +
		(u4DstX << _u4DstShift[u4GfxHwId]));
	REG(u4GfxHwId, fg_DST_BSAD) = GFX_PHYSICAL(u4DstAddr);

	u4SrcAddr = ((uintptr_t) _pu1SrcBase[u4GfxHwId] +
		(u4SrcY * _u4SrcPitch[u4GfxHwId]) +
		(u4SrcX << _u4SrcShift[u4GfxHwId]));
	REG(u4GfxHwId, fg_SRC_BSAD) = GFX_PHYSICAL(u4SrcAddr);
	REG(u4GfxHwId, fg_SRC2_BSAD_ENA) = u4Src2En;
	u4Src2Addr = ((uintptr_t) _pu12ndSrcBase[u4GfxHwId] +
		(u4SrcY * _u42ndSrcPitch[u4GfxHwId]) +
		(u4SrcX << _u42ndSrcShift[u4GfxHwId]));
	REG(u4GfxHwId, fg_SRCCBCR_BSAD) = GFX_PHYSICAL(u4Src2Addr);
	REG(u4GfxHwId, fg_OP_MODE) = (unsigned int)OP_LOOP_ALPAH_COMPOS;
	REG(u4GfxHwId, fg_SRC_WIDTH) = u4Width;
	REG(u4GfxHwId, fg_SRC_HEIGHT) = (u4Height - 1);
	REG(u4GfxHwId, fg_ALCOM_AR) = u4Ar;
	REG(u4GfxHwId, fg_ALCOM_OPCODE) = u4OpCode;
	REG(u4GfxHwId, fg_ALCOM_RECT_SRC) = u4RectSrc;
	REG(u4GfxHwId, fg_ALCOM_NORMAL) = 0;
	REG(u4GfxHwId, fg_FIRE) = GFX_ENGINE_FIRE;

	VA_MSB_REG(u4GfxHwId, fg_DST_BSAD_H, GFX_PHYSICAL(u4DstAddr));
	VA_MSB_REG(u4GfxHwId, fg_SRC_BSAD_H, GFX_PHYSICAL(u4SrcAddr));
	VA_MSB_REG(u4GfxHwId, fg_SRCCBCR_BSAD_H,
		GFX_PHYSICAL(u4Src2Addr));
	GFX_VA_MSB_CMDENQ(u4GfxHwId);

	if (GFX_CMDENQ(u4GfxHwId, GREG_DST_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRCCBCR_PITCH) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRCCBCR_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_ALCOM_LOOP) |
	    GFX_CMDENQ(u4GfxHwId, GREG_G_MODE))
		return -(int)E_GFX_OUT_OF_MEM;

	GFX_Flush(u4GfxHwId);
	REG(u4GfxHwId, fg_SRC2_BSAD_ENA) = 0;
	REG(u4GfxHwId, fg_SRCCBCR_BSAD) = 0;
	GFX_CMDENQ(u4GfxHwId, GREG_SRCCBCR_PITCH);

	return (int)E_GFX_OK;
}

int GFX_BurstEn(unsigned long u4GfxHwId, unsigned int u4BurstEn,
	unsigned int u4BurstMode)
{
	GFX_HW_FX_ENTRY;

	if (u4BurstEn) {
		REG(u4GfxHwId, fg_BURST_EN) = 1;
		REG(u4GfxHwId, fg_BURST_MODE) = u4BurstMode;
	} else {
		REG(u4GfxHwId, fg_BURST_EN) = 0;
		REG(u4GfxHwId, fg_BURST_MODE) = 0;
	}

	return (int)E_GFX_OK;
}

int GFX_SetHighPriority(unsigned long u4GfxHwId,
	unsigned int u4DynaHiPri)
{
	GFX_HW_FX_ENTRY;

	if (u4DynaHiPri)
		REG(u4GfxHwId, fg_STATIC_HIGH_PRIORITY) = 0;
	else
		REG(u4GfxHwId, fg_STATIC_HIGH_PRIORITY) = 1;
	return (int)E_GFX_OK;
}

int GFX_SetStretchBltOpt(unsigned long u4GfxHwId,
	unsigned int u4SrcW,
	unsigned int u4SrcH, unsigned int u4DstW, unsigned int u4DstH)
{
	unsigned int u4MnV, u4MnH;

	GFX_HW_FX_ENTRY;

#ifndef GFX_SUPPORT_DIFF_CM
	if (_u4DstShift[u4GfxHwId] != _u4SrcShift[u4GfxHwId])
		return -(int)E_GFX_INV_ARG;
#endif

	if ((u4DstW == u4SrcW) || (u4DstW == 1)) {
		u4MnH = 0x00010000;
		u4SrcW = u4DstW;
	} else if (u4SrcW > u4DstW) {
		u4MnH = (u4SrcW << 16) / (u4DstW - 1);
		if (((u4SrcW << 16) % (u4DstW - 1)) == 0)
			u4MnH--;
	} else {

		unsigned int u4Step;

		u4Step = (u4SrcW << 16) / (u4DstW);
		if ((u4Step * (u4DstW - 1)) < ((u4SrcW - 1) << 16)) {
			u4MnH = (u4SrcW << 16) / (u4DstW - 1);
			if (u4MnH == 0x10000)
				u4MnH--;
		} else
			u4MnH = u4Step;
	}

	if ((u4DstH == u4SrcH) || (u4DstH == 1)) {
		u4MnV = 0x00010000;
		u4SrcH = u4DstH;
	} else {
		u4MnV = (u4SrcH << 16) / (u4DstH - 1);
		if (u4MnV == 0x00010000)
			u4MnV = (u4SrcH << 16) / (u4DstH);
	}

	REG(u4GfxHwId, fg_SRC_WIDTH) = u4SrcW;
	REG(u4GfxHwId, fg_STR_DST_WIDTH) = u4DstW;
	REG(u4GfxHwId, fg_SRC_HEIGHT) = (u4SrcH - 1);
	REG(u4GfxHwId, fg_STR_DST_HEIGHT) = (u4DstH - 1);
	REG(u4GfxHwId, fg_STR_BLT_H) = u4MnH;
	REG(u4GfxHwId, fg_STR_BLT_V) = u4MnV;

	if (GFX_CMDENQ(u4GfxHwId, GREG_STR_BLT_H) |
		GFX_CMDENQ(u4GfxHwId, GREG_STR_BLT_V) |
		GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE) |
	    GFX_CMDENQ(u4GfxHwId, GREG_STR_DST_SIZE))
		return -(int)E_GFX_OUT_OF_MEM;

	return (int)E_GFX_OK;
}

int GFX_SetComposeLoopOpt(unsigned long u4GfxHwId,
	unsigned int u4AlComNormal, unsigned int u4Ar,
	unsigned int u4OpCode, unsigned int u4RectSrc)
{

	if (u4AlComNormal)
		u4Ar = 255;

	REG(u4GfxHwId, fg_SRC2_BSAD_ENA) = 0;
	if (GFX_CMDENQ(u4GfxHwId, GREG_SRCCBCR_PITCH))
		return -(int)E_GFX_OUT_OF_MEM;

	REG(u4GfxHwId, fg_ALCOM_AR) = u4Ar;
	REG(u4GfxHwId, fg_ALCOM_OPCODE) = u4OpCode;
	REG(u4GfxHwId, fg_ALCOM_RECT_SRC) = u4RectSrc;
	REG(u4GfxHwId, fg_ALCOM_NORMAL) = u4AlComNormal;

	if (GFX_CMDENQ(u4GfxHwId, GREG_ALCOM_LOOP))
		return -(int)E_GFX_OUT_OF_MEM;

	return (int)E_GFX_OK;
}

int GFX_SetRotateOpt(unsigned long u4GfxHwId, bool fgRotateEn,
	unsigned int u4Is90CCW)
{
	GFX_HW_FX_ENTRY;

	REG(u4GfxHwId, fg_DSTPITCH_DEC) = u4Is90CCW;
	SetRotate(u4GfxHwId, fgRotateEn);
	if (GFX_CMDENQ(u4GfxHwId, GREG_BITBLT_MODE))
		return -(int)E_GFX_OUT_OF_MEM;

	return (int)E_GFX_OK;
}

int GFX_SetMirrorFlipOpt(unsigned long u4GfxHwId, unsigned int u4op)
{
	GFX_HW_FX_ENTRY;
	//destination base address
	switch (u4op) {
	case E_BITBLT_SRC_MIRROR:
		REG(u4GfxHwId, fg_DSTPITCH_DEC) = 0;
		REG(u4GfxHwId, fg_DST_MIRR_OR) = 0;
		REG(u4GfxHwId, fg_SRCPITCH_DEC) = 0;
		REG(u4GfxHwId, fg_SRC_MIRR_OR) = 1;
		break;
	case E_BITBLT_SRC_FLIP:
		REG(u4GfxHwId, fg_DSTPITCH_DEC) = 0;
		REG(u4GfxHwId, fg_DST_MIRR_OR) = 0;
		REG(u4GfxHwId, fg_SRCPITCH_DEC) = 1;
		REG(u4GfxHwId, fg_SRC_MIRR_OR) = 0;
		break;
	case E_BITBLT_DST_MIRROR:
		REG(u4GfxHwId, fg_DSTPITCH_DEC) = 0;
		REG(u4GfxHwId, fg_DST_MIRR_OR) = 1;
		REG(u4GfxHwId, fg_SRCPITCH_DEC) = 0;
		REG(u4GfxHwId, fg_SRC_MIRR_OR) = 0;
		break;
	case E_BITBLT_DST_FLIP:
		REG(u4GfxHwId, fg_DSTPITCH_DEC) = 1;
		REG(u4GfxHwId, fg_DST_MIRR_OR) = 0;
		REG(u4GfxHwId, fg_SRCPITCH_DEC) = 0;
		REG(u4GfxHwId, fg_SRC_MIRR_OR) = 0;
		break;
	case E_BITBLT_SRC_FLIPMIRROR:
		REG(u4GfxHwId, fg_DSTPITCH_DEC) = 0;
		REG(u4GfxHwId, fg_DST_MIRR_OR) = 0;
		REG(u4GfxHwId, fg_SRCPITCH_DEC) = 1;
		REG(u4GfxHwId, fg_SRC_MIRR_OR) = 1;
		break;
	case E_BITBLT_DST_FLIPMIRROR:
		REG(u4GfxHwId, fg_DSTPITCH_DEC) = 1;
		REG(u4GfxHwId, fg_DST_MIRR_OR) = 1;
		REG(u4GfxHwId, fg_SRCPITCH_DEC) = 0;
		REG(u4GfxHwId, fg_SRC_MIRR_OR) = 0;
		break;

	default:
		return -1;
	}

	if (GFX_CMDENQ(u4GfxHwId, GREG_DST_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE) |
	    GFX_CMDENQ(u4GfxHwId, GREG_BITBLT_MODE))
		return -(int)E_GFX_OUT_OF_MEM;

	return (int)E_GFX_OK;
}

int GFX_StretchComposeLoop(unsigned long u4GfxHwId,
	unsigned int u4SrcX,
	unsigned int u4SrcY, unsigned int u4SrcW, unsigned int u4SrcH,
	unsigned int u4DstX, unsigned int u4DstY, unsigned int u4DstW,
	unsigned int u4DstH, unsigned int u4AlComNormal, unsigned int u4Ar,
	unsigned int u4OpCode, unsigned int u4RectSrc)
{
	unsigned int u4SrcAddr, u4DstAddr;

#ifndef GFX_SUPPORT_DIFF_CM
	if (_u4SrcCM[u4GfxHwId] != _u4DstCM[u4GfxHwId])
		return -(int)E_GFX_INV_ARG;
	if (_u4DstShift[u4GfxHwId] != _u4SrcShift[u4GfxHwId])
		return -(int)E_GFX_INV_ARG;
#endif

	u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
		(u4DstY * _u4DstPitch[u4GfxHwId]) +
		(u4DstX << _u4DstShift[u4GfxHwId]));
	u4SrcAddr = ((uintptr_t) _pu1SrcBase[u4GfxHwId] +
		(u4SrcY * _u4SrcPitch[u4GfxHwId]) +
		(u4SrcX << _u4SrcShift[u4GfxHwId]));

	REG(u4GfxHwId, fg_DST_BSAD) = GFX_PHYSICAL(u4DstAddr);
	REG(u4GfxHwId, fg_SRC_BSAD) = GFX_PHYSICAL(u4SrcAddr);
	REG(u4GfxHwId, fg_SRC_WIDTH) = u4SrcW;
	REG(u4GfxHwId, fg_SRC_HEIGHT) = (u4SrcH - 1);

	if (GFX_CMDENQ(u4GfxHwId, GREG_DST_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE))
		return -(int)E_GFX_OUT_OF_MEM;

	GFX_SetStretchBltOpt(u4GfxHwId, u4SrcW, u4SrcH, u4DstW, u4DstH);
	GFX_SetComposeLoopOpt(u4GfxHwId, u4AlComNormal,
		u4Ar, u4OpCode, u4RectSrc);

	REG(u4GfxHwId, fg_OP_MODE) =
		(unsigned int)OP_STRETCH_LOOP_ALPHA_COMPOS;
	REG(u4GfxHwId, fg_FIRE) = GFX_ENGINE_FIRE;

	VA_MSB_REG(u4GfxHwId, fg_DST_BSAD_H, GFX_PHYSICAL(u4DstAddr));
	VA_MSB_REG(u4GfxHwId, fg_SRC_BSAD_H, GFX_PHYSICAL(u4SrcAddr));
	GFX_VA_MSB_CMDENQ(u4GfxHwId);

	if (GFX_CMDENQ(u4GfxHwId, GREG_G_MODE))
		return -(int)E_GFX_OUT_OF_MEM;

	return (int)E_GFX_OK;
}

int GFX_StretchRotateComposeLoop(unsigned long u4GfxHwId,
	unsigned int u4SrcX, unsigned int u4SrcY, unsigned int u4SrcW,
	unsigned int u4SrcH, unsigned int u4DstX, unsigned int u4DstY,
	unsigned int u4DstW, unsigned int u4DstH, unsigned int u4Is90CCW,
	unsigned int u4AlComNormal, unsigned int u4Ar,
	unsigned int u4OpCode, unsigned int u4RectSrc)
{
	unsigned int u4SrcAddr, u4DstAddr;

#ifndef GFX_SUPPORT_DIFF_CM
	if (_u4SrcCM[u4GfxHwId] != _u4DstCM[u4GfxHwId])
		return -(int)E_GFX_INV_ARG;
	if (_u4DstShift[u4GfxHwId] != _u4SrcShift[u4GfxHwId])
		return -(int)E_GFX_INV_ARG;
#endif

	u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
		(u4DstY * _u4DstPitch[u4GfxHwId]) +
		(u4DstX << _u4DstShift[u4GfxHwId]));
	u4SrcAddr = ((uintptr_t) _pu1SrcBase[u4GfxHwId] +
		(u4SrcY * _u4SrcPitch[u4GfxHwId]) +
		(u4SrcX << _u4SrcShift[u4GfxHwId]));

	REG(u4GfxHwId, fg_DST_BSAD) = GFX_PHYSICAL(u4DstAddr);
	REG(u4GfxHwId, fg_SRC_BSAD) = GFX_PHYSICAL(u4SrcAddr);
	REG(u4GfxHwId, fg_SRC_WIDTH) = u4SrcW;
	REG(u4GfxHwId, fg_SRC_HEIGHT) = (u4SrcH - 1);

	if (GFX_CMDENQ(u4GfxHwId, GREG_DST_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE))
		return -(int)E_GFX_OUT_OF_MEM;

	GFX_SetStretchBltOpt(u4GfxHwId, u4SrcW, u4SrcH, u4DstW, u4DstH);
	GFX_SetRotateOpt(u4GfxHwId, false, u4Is90CCW);
	GFX_SetComposeLoopOpt(u4GfxHwId, u4AlComNormal,
		u4Ar, u4OpCode, u4RectSrc);

	REG(u4GfxHwId, fg_OP_MODE) =
		(unsigned int)OP_STRETCH_ROTATE_LOOP_ALPHA_COMPOS;
	REG(u4GfxHwId, fg_FIRE) = GFX_ENGINE_FIRE;

	VA_MSB_REG(u4GfxHwId, fg_DST_BSAD_H, GFX_PHYSICAL(u4DstAddr));
	VA_MSB_REG(u4GfxHwId, fg_SRC_BSAD_H, GFX_PHYSICAL(u4SrcAddr));
	GFX_VA_MSB_CMDENQ(u4GfxHwId);

	if (GFX_CMDENQ(u4GfxHwId, GREG_G_MODE))
		return -(int)E_GFX_OUT_OF_MEM;

	GFX_Flush(u4GfxHwId);
	SetRotate(u4GfxHwId, 0);

	REG(u4GfxHwId, fg_DSTPITCH_DEC) = 0;
	GFX_CMDENQ(u4GfxHwId, GREG_BITBLT_MODE);
	return (int)E_GFX_OK;
}

int GFX_StretchMFlipComposeLoop(unsigned long u4GfxHwId,
	unsigned int u4SrcX, unsigned int u4SrcY, unsigned int u4SrcW,
	unsigned int u4SrcH, unsigned int u4DstX, unsigned int u4DstY,
	unsigned int u4DstW, unsigned int u4DstH, unsigned int u4MFOp,
	unsigned int u4AlComNormal, unsigned int u4Ar,
	unsigned int u4OpCode, unsigned int u4RectSrc)
{
	unsigned int u4SrcAddr, u4DstAddr;

#ifndef GFX_SUPPORT_DIFF_CM
	if (_u4SrcCM[u4GfxHwId] != _u4DstCM[u4GfxHwId])
		return -(int)E_GFX_INV_ARG;
	if (_u4DstShift[u4GfxHwId] != _u4SrcShift[u4GfxHwId])
		return -(int)E_GFX_INV_ARG;
#endif
	switch (u4MFOp) {
	case E_BITBLT_SRC_MIRROR:
		u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
			     (u4DstY * _u4DstPitch[u4GfxHwId]) +
			     (u4DstX << _u4DstShift[u4GfxHwId]));
		//source base address
		u4SrcAddr = ((uintptr_t) _pu1SrcBase[u4GfxHwId] +
			     (u4SrcY * _u4SrcPitch[u4GfxHwId]) +
			     ((u4SrcX + u4SrcW - 1) << _u4SrcShift[u4GfxHwId]));
		break;
	case E_BITBLT_SRC_FLIP:
		u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
			     (u4DstY * _u4DstPitch[u4GfxHwId]) +
			     (u4DstX << _u4DstShift[u4GfxHwId]));
		//source base address
		u4SrcAddr = ((uintptr_t) _pu1SrcBase[u4GfxHwId] +
			     ((u4SrcY + u4SrcH - 1) * _u4SrcPitch[u4GfxHwId]) +
			     (u4SrcX << _u4SrcShift[u4GfxHwId]));
		break;
	case E_BITBLT_SRC_FLIPMIRROR:
		u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
			     (u4DstY * _u4DstPitch[u4GfxHwId]) +
			     (u4DstX << _u4DstShift[u4GfxHwId]));
		//source base address
		u4SrcAddr = ((uintptr_t) _pu1SrcBase[u4GfxHwId] +
			     ((u4SrcY + u4SrcH - 1) * _u4SrcPitch[u4GfxHwId]) +
			     ((u4SrcX + u4SrcW - 1) << _u4SrcShift[u4GfxHwId]));
		break;
	default:
		return E_GFX_INV_ARG;
	}

	REG(u4GfxHwId, fg_DST_BSAD) = GFX_PHYSICAL(u4DstAddr);
	REG(u4GfxHwId, fg_SRC_BSAD) = GFX_PHYSICAL(u4SrcAddr);
	REG(u4GfxHwId, fg_SRC_WIDTH) = u4SrcW;
	REG(u4GfxHwId, fg_SRC_HEIGHT) = (u4SrcH - 1);

	if (GFX_CMDENQ(u4GfxHwId, GREG_DST_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE))
		return -(int)E_GFX_OUT_OF_MEM;
	// Stretch part
	GFX_SetStretchBltOpt(u4GfxHwId, u4SrcW, u4SrcH, u4DstW, u4DstH);
	GFX_SetMirrorFlipOpt(u4GfxHwId, u4MFOp);
	GFX_SetComposeLoopOpt(u4GfxHwId, u4AlComNormal,
		u4Ar, u4OpCode, u4RectSrc);

	REG(u4GfxHwId, fg_OP_MODE) =
		(unsigned int)OP_STRETCH_LOOP_ALPHA_COMPOS;
	REG(u4GfxHwId, fg_FIRE) = GFX_ENGINE_FIRE;
	VA_MSB_REG(u4GfxHwId, fg_DST_BSAD_H, GFX_PHYSICAL(u4DstAddr));
	VA_MSB_REG(u4GfxHwId, fg_SRC_BSAD_H, GFX_PHYSICAL(u4SrcAddr));
	GFX_VA_MSB_CMDENQ(u4GfxHwId);

	if (GFX_CMDENQ(u4GfxHwId, GREG_G_MODE))
		return -(int)E_GFX_OUT_OF_MEM;

	GFX_Flush(u4GfxHwId);
	ResetMirrorFlipFlag(u4GfxHwId);

	return (int)E_GFX_OK;
}

int GFX_StretchRotateMFlipComposeLoop(unsigned long u4GfxHwId,
	unsigned int u4SrcX, unsigned int u4SrcY, unsigned int u4SrcW,
	unsigned int u4SrcH, unsigned int u4DstX, unsigned int u4DstY,
	unsigned int u4DstW, unsigned int u4DstH, unsigned int u4Is90CCW,
	unsigned int u4MFOp, unsigned int u4AlComNormal,
	unsigned int u4Ar, unsigned int u4OpCode,
	unsigned int u4RectSrc)
{
	unsigned int u4SrcAddr, u4DstAddr;

#ifndef GFX_SUPPORT_DIFF_CM
	if (_u4SrcCM[u4GfxHwId] != _u4DstCM[u4GfxHwId])
		return -(int)E_GFX_INV_ARG;
	if (_u4DstShift[u4GfxHwId] != _u4SrcShift[u4GfxHwId])
		return -(int)E_GFX_INV_ARG;
#endif

	switch (u4MFOp) {
	case E_BITBLT_SRC_MIRROR:
		u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
			     (u4DstY * _u4DstPitch[u4GfxHwId]) +
			     (u4DstX << _u4DstShift[u4GfxHwId]));
		//source base address
		u4SrcAddr = ((uintptr_t) _pu1SrcBase[u4GfxHwId] +
			     (u4SrcY * _u4SrcPitch[u4GfxHwId]) +
			     ((u4SrcX + u4SrcW - 1) << _u4SrcShift[u4GfxHwId]));
		break;
	case E_BITBLT_SRC_FLIP:
		u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
			     (u4DstY * _u4DstPitch[u4GfxHwId]) +
			     (u4DstX << _u4DstShift[u4GfxHwId]));
		//source base address
		u4SrcAddr = ((uintptr_t) _pu1SrcBase[u4GfxHwId] +
			     ((u4SrcY + u4SrcH - 1) * _u4SrcPitch[u4GfxHwId]) +
			     (u4SrcX << _u4SrcShift[u4GfxHwId]));
		break;
	case E_BITBLT_SRC_FLIPMIRROR:
		u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
			     (u4DstY * _u4DstPitch[u4GfxHwId]) +
			     (u4DstX << _u4DstShift[u4GfxHwId]));
		//source base address
		u4SrcAddr = ((uintptr_t) _pu1SrcBase[u4GfxHwId] +
			     ((u4SrcY + u4SrcH - 1) * _u4SrcPitch[u4GfxHwId]) +
			     ((u4SrcX + u4SrcW - 1) << _u4SrcShift[u4GfxHwId]));
		break;
	default:
		return E_GFX_INV_ARG;
	}

	REG(u4GfxHwId, fg_DST_BSAD) = GFX_PHYSICAL(u4DstAddr);
	REG(u4GfxHwId, fg_SRC_BSAD) = GFX_PHYSICAL(u4SrcAddr);
	REG(u4GfxHwId, fg_SRC_WIDTH) = u4SrcW;
	REG(u4GfxHwId, fg_SRC_HEIGHT) = (u4SrcH - 1);

	if (GFX_CMDENQ(u4GfxHwId, GREG_DST_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE))
		return -(int)E_GFX_OUT_OF_MEM;

	GFX_SetMirrorFlipOpt(u4GfxHwId, u4MFOp);
	GFX_SetStretchBltOpt(u4GfxHwId, u4SrcW, u4SrcH, u4DstW, u4DstH);
	GFX_SetRotateOpt(u4GfxHwId, true, u4Is90CCW);
	GFX_SetComposeLoopOpt(u4GfxHwId, u4AlComNormal,
		u4Ar, u4OpCode, u4RectSrc);

	REG(u4GfxHwId, fg_OP_MODE) =
		(unsigned int)OP_STRETCH_ROTATE_LOOP_ALPHA_COMPOS;
	REG(u4GfxHwId, fg_FIRE) = GFX_ENGINE_FIRE;
	VA_MSB_REG(u4GfxHwId, fg_DST_BSAD_H, GFX_PHYSICAL(u4DstAddr));
	VA_MSB_REG(u4GfxHwId, fg_SRC_BSAD_H, GFX_PHYSICAL(u4SrcAddr));
	GFX_VA_MSB_CMDENQ(u4GfxHwId);

	if (GFX_CMDENQ(u4GfxHwId, GREG_G_MODE))
		return -(int)E_GFX_OUT_OF_MEM;

	GFX_Flush(u4GfxHwId);
	ResetRotateMirrorFlipFlag(u4GfxHwId);

	return (int)E_GFX_OK;
}

int GFX_StretchRotate(unsigned long u4GfxHwId, unsigned int u4SrcX,
	unsigned int u4SrcY, unsigned int u4SrcW, unsigned int u4SrcH,
	unsigned int u4DstX, unsigned int u4DstY, unsigned int u4DstW,
	unsigned int u4DstH, unsigned int u4Is90CCW)
{
	unsigned int u4SrcAddr, u4DstAddr;

#ifndef GFX_SUPPORT_DIFF_CM
	if (_u4SrcCM[u4GfxHwId] != _u4DstCM[u4GfxHwId])
		return -(int)E_GFX_INV_ARG;
	if (_u4DstShift[u4GfxHwId] != _u4SrcShift[u4GfxHwId])
		return -(int)E_GFX_INV_ARG;
#endif

	u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
		(u4DstY * _u4DstPitch[u4GfxHwId]) +
		(u4DstX << _u4DstShift[u4GfxHwId]));
	u4SrcAddr = ((uintptr_t) _pu1SrcBase[u4GfxHwId] +
		(u4SrcY * _u4SrcPitch[u4GfxHwId]) +
		(u4SrcX << _u4SrcShift[u4GfxHwId]));

	REG(u4GfxHwId, fg_DST_BSAD) = GFX_PHYSICAL(u4DstAddr);
	REG(u4GfxHwId, fg_SRC_BSAD) = GFX_PHYSICAL(u4SrcAddr);
	REG(u4GfxHwId, fg_SRC_WIDTH) = u4SrcW;
	REG(u4GfxHwId, fg_SRC_HEIGHT) = (u4SrcH - 1);

	if (GFX_CMDENQ(u4GfxHwId, GREG_DST_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE))
		return -(int)E_GFX_OUT_OF_MEM;

	GFX_SetStretchBltOpt(u4GfxHwId, u4SrcW, u4SrcH, u4DstW, u4DstH);
	GFX_SetRotateOpt(u4GfxHwId, true, u4Is90CCW);

	// Fire...
	REG(u4GfxHwId, fg_OP_MODE) = (unsigned int)OP_STRETCH_BITBLT;
	REG(u4GfxHwId, fg_FIRE) = GFX_ENGINE_FIRE;

	VA_MSB_REG(u4GfxHwId, fg_DST_BSAD_H, GFX_PHYSICAL(u4DstAddr));
	VA_MSB_REG(u4GfxHwId, fg_SRC_BSAD_H, GFX_PHYSICAL(u4SrcAddr));
	GFX_VA_MSB_CMDENQ(u4GfxHwId);

	if (GFX_CMDENQ(u4GfxHwId, GREG_G_MODE))
		return -(int)E_GFX_OUT_OF_MEM;

	SetRotate(u4GfxHwId, 0);
	REG(u4GfxHwId, fg_DSTPITCH_DEC) = 0;

	return (int)E_GFX_OK;
}

int GFX_StretchMFlip(unsigned long u4GfxHwId, unsigned int u4SrcX,
	unsigned int u4SrcY, unsigned int u4SrcW, unsigned int u4SrcH,
	unsigned int u4DstX, unsigned int u4DstY, unsigned int u4DstW,
	unsigned int u4DstH, unsigned int u4MFOp)
{
	unsigned int u4SrcAddr, u4DstAddr;

#ifndef GFX_SUPPORT_DIFF_CM
	if (_u4SrcCM[u4GfxHwId] != _u4DstCM[u4GfxHwId])
		return -(int)E_GFX_INV_ARG;
	if (_u4DstShift[u4GfxHwId] != _u4SrcShift[u4GfxHwId])
		return -(int)E_GFX_INV_ARG;
#endif

	switch (u4MFOp) {
	case E_BITBLT_SRC_MIRROR:
		u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
			     (u4DstY * _u4DstPitch[u4GfxHwId]) +
			     (u4DstX << _u4DstShift[u4GfxHwId]));
		//source base address
		u4SrcAddr = ((uintptr_t) _pu1SrcBase[u4GfxHwId] +
			     (u4SrcY * _u4SrcPitch[u4GfxHwId]) +
			     ((u4SrcX + u4SrcW - 1) << _u4SrcShift[u4GfxHwId]));
		break;
	case E_BITBLT_SRC_FLIP:
		u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
			     (u4DstY * _u4DstPitch[u4GfxHwId]) +
			     (u4DstX << _u4DstShift[u4GfxHwId]));
		//source base address
		u4SrcAddr = ((uintptr_t) _pu1SrcBase[u4GfxHwId] +
			     ((u4SrcY + u4SrcH - 1) * _u4SrcPitch[u4GfxHwId]) +
			     (u4SrcX << _u4SrcShift[u4GfxHwId]));
		break;
	case E_BITBLT_SRC_FLIPMIRROR:
		u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
			     (u4DstY * _u4DstPitch[u4GfxHwId]) +
			     (u4DstX << _u4DstShift[u4GfxHwId]));
		//source base address
		u4SrcAddr = ((uintptr_t) _pu1SrcBase[u4GfxHwId] +
			     ((u4SrcY + u4SrcH - 1) * _u4SrcPitch[u4GfxHwId]) +
			     ((u4SrcX + u4SrcW - 1) << _u4SrcShift[u4GfxHwId]));
		break;
	case E_BITBLT_DST_MIRROR:
		u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
			     (u4DstY * _u4DstPitch[u4GfxHwId]) +
			     ((u4DstX + u4DstW - 1) << _u4DstShift[u4GfxHwId]));
		//source base address
		u4SrcAddr = ((uintptr_t) _pu1SrcBase[u4GfxHwId] +
			     (u4SrcY * _u4SrcPitch[u4GfxHwId]) +
			     (u4SrcX << _u4SrcShift[u4GfxHwId]));
		break;
	case E_BITBLT_DST_FLIP:
		u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
			     ((u4DstY + u4DstH - 1) * _u4DstPitch[u4GfxHwId]) +
			     (u4DstX << _u4DstShift[u4GfxHwId]));
		//source base address
		u4SrcAddr = ((uintptr_t) _pu1SrcBase[u4GfxHwId] +
			     (u4SrcY * _u4SrcPitch[u4GfxHwId]) +
			     (u4SrcX << _u4SrcShift[u4GfxHwId]));
		break;

	case E_BITBLT_DST_FLIPMIRROR:
		u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
			     ((u4DstY + u4DstH - 1) * _u4DstPitch[u4GfxHwId]) +
			     ((u4DstX + u4DstW - 1) << _u4DstShift[u4GfxHwId]));
		//source base address
		u4SrcAddr = ((uintptr_t) _pu1SrcBase[u4GfxHwId] +
			     (u4SrcY * _u4SrcPitch[u4GfxHwId]) +
			     (u4SrcX << _u4SrcShift[u4GfxHwId]));
		break;
	default:
		return E_GFX_INV_ARG;
	}

	REG(u4GfxHwId, fg_DST_BSAD) = GFX_PHYSICAL(u4DstAddr);
	REG(u4GfxHwId, fg_SRC_BSAD) = GFX_PHYSICAL(u4SrcAddr);
	REG(u4GfxHwId, fg_SRC_WIDTH) = u4SrcW;
	REG(u4GfxHwId, fg_SRC_HEIGHT) = (u4SrcH - 1);

	if (GFX_CMDENQ(u4GfxHwId, GREG_DST_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE))
		return -(int)E_GFX_OUT_OF_MEM;

	GFX_SetStretchBltOpt(u4GfxHwId, u4SrcW, u4SrcH, u4DstW, u4DstH);
	GFX_SetMirrorFlipOpt(u4GfxHwId, u4MFOp);

	REG(u4GfxHwId, fg_OP_MODE) = (unsigned int)OP_STRETCH_BITBLT;
	REG(u4GfxHwId, fg_FIRE) = GFX_ENGINE_FIRE;
	VA_MSB_REG(u4GfxHwId, fg_DST_BSAD_H, GFX_PHYSICAL(u4DstAddr));
	VA_MSB_REG(u4GfxHwId, fg_SRC_BSAD_H, GFX_PHYSICAL(u4SrcAddr));
	GFX_VA_MSB_CMDENQ(u4GfxHwId);

	if (GFX_CMDENQ(u4GfxHwId, GREG_G_MODE))
		return -(int)E_GFX_OUT_OF_MEM;

	GFX_Flush(u4GfxHwId);
	ResetMirrorFlipFlag(u4GfxHwId);

	return (int)E_GFX_OK;
}

int GFX_SetPreColorize(unsigned long u4GfxHwId,
	unsigned int u4PreColorize,
	unsigned int u4ColorRepEn, unsigned int u4ColorRep)
{
	GFX_HW_FX_ENTRY;

	REG(u4GfxHwId, fg_COLORIZE_REP) = u4ColorRepEn;
	REG(u4GfxHwId, fg_PRE_COLORIZE) = u4PreColorize;
	GFX_CMDENQ(u4GfxHwId, GREG_ROP);
	REG(u4GfxHwId, fg_JAVA_XOR_COLOR) = u4ColorRep;
	return GFX_CMDENQ(u4GfxHwId, GREG_XOR_COLOR);

}

int GFX_PreColorizeComposeLoop(unsigned long u4GfxHwId,
	unsigned int u4SrcX, unsigned int u4SrcY, unsigned int u4DstX,
	unsigned int u4DstY, unsigned int u4Width, unsigned int u4Height,
	unsigned int u4Ar, unsigned int u4OpCode, unsigned int u4RectSrc)
{
	int i4MhpPatch = GFX_ENABLE;
	unsigned int u4SrcAddr, u4DstAddr;
	unsigned int u4PreColorize, u4ColorRepEn, u4ColorRep;

	u4PreColorize = (u4OpCode & 0x00010000)>>16;
	u4ColorRepEn = (u4OpCode & 0x00020000)>>17;
	u4ColorRep = (u4OpCode & 0xFF000000);
	u4OpCode = (u4OpCode & 0xFFFF);
	REG(u4GfxHwId, fg_COLORIZE_REP)    = u4ColorRepEn;
	REG(u4GfxHwId, fg_PRE_COLORIZE)    = u4PreColorize;
	GFX_CMDENQ(u4GfxHwId, GREG_ROP);
	REG(u4GfxHwId, fg_JAVA_XOR_COLOR)  = u4ColorRep;
	GFX_CMDENQ(u4GfxHwId, GREG_XOR_COLOR);

#ifndef GFX_SUPPORT_DIFF_CM
	if (_u4SrcCM[u4GfxHwId] != _u4DstCM[u4GfxHwId])
		return -(int)E_GFX_INV_ARG;
#endif
	if ((_u4SrcCM[u4GfxHwId] == (unsigned int)CM_ARGB8888_DIRECT32) &&
	    (_u4SrcCM[u4GfxHwId] == (unsigned int)CM_ARGB8888_DIRECT32]) &&
	    (i4MhpPatch == GFX_ENABLE)) {
		if (u4OpCode == (unsigned int)E_AC_CLEAR) {
			unsigned int u4RectColor =
				REG(u4GfxHwId, fg_RECT_COLOR);

			GFX_SetColor(u4GfxHwId, 0x0);
			GFX_Fill(u4GfxHwId, u4DstX, u4DstY, u4Width, u4Height);
			GFX_SetColor(u4GfxHwId, u4RectColor);
			return (int)E_GFX_OK;
		} else if (u4OpCode == (unsigned int)E_AC_SRC) {
			if (u4Ar == 0) {
				unsigned int u4RectColor =
					REG(u4GfxHwId, fg_RECT_COLOR);

				GFX_SetColor(u4GfxHwId, 0x0);
				GFX_Fill(u4GfxHwId, u4DstX,
					u4DstY, u4Width, u4Height);

				GFX_SetColor(u4GfxHwId, u4RectColor);
				return (int)E_GFX_OK;
			} else if (u4Ar == 255) {
				unsigned int u4RectColor =
					REG(u4GfxHwId, fg_RECT_COLOR);

				GFX_SetColor(u4GfxHwId, 0x0);
				GFX_SetBltOpt(u4GfxHwId,
					(unsigned int)D_GFXFLAG_NONE,
					0x00000000, 0xffffffff);

				GFX_SetAlpha(u4GfxHwId, u4Ar);

				GFX_BitBlt(u4GfxHwId, u4SrcX, u4SrcY,
					u4DstX, u4DstY, u4Width, u4Height);
				// restore original value
				GFX_SetColor(u4GfxHwId, u4RectColor);
				return (int)E_GFX_OK;
			}
		} else if ((u4OpCode == (unsigned int)E_AC_SRC_OVER) &&
			(u4Ar == 0)) {
			unsigned int u4RectColor =
				REG(u4GfxHwId, fg_RECT_COLOR);
			unsigned int u4MinColor =
				REG(u4GfxHwId, fg_COLOR_KEY_MIN);
			unsigned int u4MaxColor =
				REG(u4GfxHwId, fg_COLOR_KEY_MAX);
			unsigned int u4Switch = 0x0;

			if (REG(u4GfxHwId, fg_TRANS_ENA))
				u4Switch = (u4Switch | D_GFXFLAG_TRANSPARENT);

			if (REG(u4GfxHwId, fg_KEYNOT_ENA))
				u4Switch = (u4Switch | D_GFXFLAG_KEYNOT);

			if (REG(u4GfxHwId, fg_COLCHG_ENA))
				u4Switch = (u4Switch | D_GFXFLAG_COLORCHANGE);

			if (REG(u4GfxHwId, fg_KEYSDSEL))
				u4Switch = (u4Switch | D_GFXFLAG_KEYSDSEL);

			GFX_SetColor(u4GfxHwId, 0x0);
			GFX_SetBltOpt(u4GfxHwId, D_GFXFLAG_COLORCHANGE,
				0x00000000, 0x00FFFFFF);

			REG(u4GfxHwId, fg_OSD_WIDTH) =
				_u4DstPitch[u4GfxHwId] >> 4;
			REG(u4GfxHwId, fg_SRC_PITCH) =
				_u4DstPitch[u4GfxHwId] >> 4;

			u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
			     (u4DstY * _u4DstPitch[u4GfxHwId]) +
			     (u4DstX << _u4DstShift[u4GfxHwId]));
			u4SrcAddr = ((uintptr_t) _pu1SrcBase[u4GfxHwId] +
			     (u4SrcY * _u4SrcPitch[u4GfxHwId]) +
			     (u4SrcX << _u4SrcShift[u4GfxHwId]));
			REG(u4GfxHwId, fg_DST_BSAD) = GFX_PHYSICAL(u4DstAddr);
			REG(u4GfxHwId, fg_SRC_BSAD) = GFX_PHYSICAL(u4SrcAddr);

			REG(u4GfxHwId, fg_OP_MODE) = (unsigned int)OP_BITBLT;
			REG(u4GfxHwId, fg_CFMT_ENA) =
			    ((unsigned int)(_u4DstCM[u4GfxHwId] !=
			    _u4SrcCM[u4GfxHwId]));
			REG(u4GfxHwId, fg_SRC_WIDTH) = u4Width;
			REG(u4GfxHwId, fg_SRC_HEIGHT) = (u4Height - 1);
			REG(u4GfxHwId, fg_FIRE) = GFX_ENGINE_FIRE;
			VA_MSB_REG(u4GfxHwId, fg_DST_BSAD_H,
				GFX_PHYSICAL(u4DstAddr));
			VA_MSB_REG(u4GfxHwId, fg_SRC_BSAD_H,
				GFX_PHYSICAL(u4SrcAddr));

			GFX_VA_MSB_CMDENQ(u4GfxHwId);

			if (GFX_CMDENQ(u4GfxHwId, GREG_S_OSD_WIDTH) |
			    GFX_CMDENQ(u4GfxHwId, GREG_DST_BSAD) |
			    GFX_CMDENQ(u4GfxHwId, GREG_SRC_BSAD) |
			    GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE) |
			    GFX_CMDENQ(u4GfxHwId, GREG_BITBLT_MODE) |
			    GFX_CMDENQ(u4GfxHwId, GREG_G_MODE))
				return -(int)E_GFX_OUT_OF_MEM;
			// restore original value
			REG(u4GfxHwId, fg_OSD_WIDTH) =
				_u4DstPitch[u4GfxHwId] >> 4;
			REG(u4GfxHwId, fg_SRC_PITCH) =
				_u4SrcPitch[u4GfxHwId] >> 4;
			u4SrcAddr = ((uintptr_t) _pu1SrcBase[u4GfxHwId] +
			     (u4SrcY * _u4SrcPitch[u4GfxHwId]) +
			     (u4SrcX << _u4SrcShift[u4GfxHwId]));
			REG(u4GfxHwId, fg_SRC_BSAD) = GFX_PHYSICAL(u4SrcAddr);
			GFX_SetColor(u4GfxHwId, u4RectColor);
			GFX_SetBltOpt(u4GfxHwId, u4Switch,
				u4MinColor, u4MaxColor);
			if (GFX_CMDENQ(u4GfxHwId, GREG_S_OSD_WIDTH) |
			    GFX_CMDENQ(u4GfxHwId, GREG_SRC_BSAD) |
			    GFX_CMDENQ(u4GfxHwId, GREG_BITBLT_MODE))
				return -(int)E_GFX_OUT_OF_MEM;
			return (int)E_GFX_OK;
		}
	}

	REG(u4GfxHwId, fg_SRC2_BSAD_ENA) = 0;
	if (GFX_CMDENQ(u4GfxHwId, GREG_SRCCBCR_PITCH))
		return -(int)E_GFX_OUT_OF_MEM;
	// normal compose in LOOP mode

	//destination base address
	u4DstAddr = ((uintptr_t) _pu1DstBase[u4GfxHwId] +
		(u4DstY * _u4DstPitch[u4GfxHwId]) +
		(u4DstX << _u4DstShift[u4GfxHwId]));
	REG(u4GfxHwId, fg_DST_BSAD) = GFX_PHYSICAL(u4DstAddr);

	//source base address
	u4SrcAddr = ((uintptr_t) _pu1SrcBase[u4GfxHwId] +
		(u4SrcY * _u4SrcPitch[u4GfxHwId]) +
		(u4SrcX << _u4SrcShift[u4GfxHwId]));
	REG(u4GfxHwId, fg_SRC_BSAD) = GFX_PHYSICAL(u4SrcAddr);


	REG(u4GfxHwId, fg_OP_MODE) = (unsigned int)OP_LOOP_ALPAH_COMPOS;
	REG(u4GfxHwId, fg_SRC_WIDTH) = u4Width;
	REG(u4GfxHwId, fg_SRC_HEIGHT) = (u4Height - 1);
	REG(u4GfxHwId, fg_ALCOM_AR) = u4Ar;
	REG(u4GfxHwId, fg_ALCOM_OPCODE) = u4OpCode;
	REG(u4GfxHwId, fg_ALCOM_RECT_SRC) = u4RectSrc;
	REG(u4GfxHwId, fg_ALCOM_NORMAL) = 0;
	REG(u4GfxHwId, fg_FIRE) = GFX_ENGINE_FIRE;
	VA_MSB_REG(u4GfxHwId, fg_DST_BSAD_H, GFX_PHYSICAL(u4DstAddr));
	VA_MSB_REG(u4GfxHwId, fg_SRC_BSAD_H, GFX_PHYSICAL(u4SrcAddr));

	GFX_VA_MSB_CMDENQ(u4GfxHwId);

	if (GFX_CMDENQ(u4GfxHwId, GREG_DST_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_BSAD) |
	    GFX_CMDENQ(u4GfxHwId, GREG_SRC_SIZE) |
	    GFX_CMDENQ(u4GfxHwId, GREG_ALCOM_LOOP) |
	    GFX_CMDENQ(u4GfxHwId, GREG_G_MODE))
		return -(int)E_GFX_OUT_OF_MEM;

	return (int)E_GFX_OK;
}

int Gfx_Rotate90(unsigned long u4GfxHwId, unsigned int u4SrcX,
	unsigned int u4SrcY, unsigned int u4DstX, unsigned int u4DstY,
	unsigned int u4Width, unsigned int u4Height,
	unsigned int u4Is90CCW)
{
	GFX_SetRotateOpt(u4GfxHwId, 1, u4Is90CCW);

	GFX_SetBltOpt(u4GfxHwId, (unsigned int)D_GFXFLAG_NONE,
		0x00000000, 0xffffffff);

	GFX_BitBlt(u4GfxHwId, u4SrcX, u4SrcY, u4DstX,
		u4DstY, u4Width, u4Height);
	GFX_Flush(u4GfxHwId);

	ResetRotateFlag(u4GfxHwId);

	return (int)E_GFX_OK;
}

int _GfxOpModeCheck(unsigned int u4Op, unsigned int u4Src,
	unsigned int u4Dst)
{
	// rule:1 - do NOT support clut2 and clut4      (SRC)
	if ((u4Src == (unsigned int)GFX_COLORMODE_AYCbCr_CLUT2) ||
	    (u4Src == (unsigned int)GFX_COLORMODE_AYCbCr_CLUT4) ||
	    (u4Src == (unsigned int)GFX_COLORMODE_ARGB_CLUT2) ||
	    (u4Src == (unsigned int)GFX_COLORMODE_ARGB_CLUT4))
		return (int)-E_GFX_INV_ARG;

	// rule:1 - do NOT support clut2 and clut4      (DST)
	if ((u4Dst == (unsigned int)GFX_COLORMODE_AYCbCr_CLUT2) ||
	    (u4Dst == (unsigned int)GFX_COLORMODE_AYCbCr_CLUT4) ||
	    (u4Dst == (unsigned int)GFX_COLORMODE_ARGB_CLUT2) ||
	    (u4Dst == (unsigned int)GFX_COLORMODE_ARGB_CLUT4))
		return (int)-E_GFX_INV_ARG;

	// rule:2 - do NOT support YCbYCr or CbYCrY mode    (SRC, DST)
	if ((u4Src == (unsigned int)GFX_COLORMODE_CbYCrY_16) ||
	    (u4Src == (unsigned int)GFX_COLORMODE_YCbYCr_16) ||
	    (u4Dst == (unsigned int)GFX_COLORMODE_CbYCrY_16) ||
	    (u4Dst == (unsigned int)GFX_COLORMODE_YCbYCr_16))
		return (int)-E_GFX_INV_ARG;

	// rule:3 - normal bitblt modes
	if (u4Op == (unsigned int)OP_BITBLT) {
		if (((u4Src == (unsigned int)GFX_COLORMODE_RGB_D565) ||
		     (u4Src == (unsigned int)GFX_COLORMODE_ARGB_D1555) ||
		     (u4Src == (unsigned int)GFX_COLORMODE_ARGB_D4444) ||
		     (u4Src == (unsigned int)GFX_COLORMODE_ARGB_D8888) ||
		     (u4Src == (unsigned int)GFX_COLORMODE_AYCbCr_D8888)) &&
		    ((u4Dst == (unsigned int)GFX_COLORMODE_RGB_D565) ||
		     (u4Dst == (unsigned int)GFX_COLORMODE_ARGB_D1555) ||
		     (u4Dst == (unsigned int)GFX_COLORMODE_ARGB_D4444) ||
		     (u4Dst == (unsigned int)GFX_COLORMODE_ARGB_D8888) ||
		     (u4Src == (unsigned int)GFX_COLORMODE_AYCbCr_D8888)))
			return (int)E_GFX_OK;

		if ((u4Src == u4Dst) &&
		    ((u4Dst == (unsigned int)GFX_COLORMODE_AYCbCr_CLUT8) ||
		     (u4Dst == (unsigned int)GFX_COLORMODE_ARGB_CLUT8)))
			return (int)E_GFX_OK;

		return (int)-E_GFX_INV_ARG;
	}
	// rule:4 - the mode without source
	if ((u4Op == (unsigned int)OP_RECT_FILL) ||
	    (u4Op == (unsigned int)OP_DRAW_HLINE) ||
	    (u4Op == (unsigned int)OP_DRAW_VLINE) ||
	    (u4Op == (unsigned int)OP_GRAD_FILL)) {
		if ((u4Dst == (unsigned int)GFX_COLORMODE_AYCbCr_CLUT8) ||
		    (u4Dst == (unsigned int)GFX_COLORMODE_ARGB_CLUT8) ||
		    (u4Dst == (unsigned int)GFX_COLORMODE_RGB_D565) ||
		    (u4Dst == (unsigned int)GFX_COLORMODE_ARGB_D1555) ||
		    (u4Dst == (unsigned int)GFX_COLORMODE_ARGB_D4444) ||
		    (u4Dst == (unsigned int)GFX_COLORMODE_ARGB_D8888) ||
		    (u4Dst == (unsigned int)GFX_COLORMODE_AYCbCr_D8888)) {
			if ((u4Op == (unsigned int)OP_GRAD_FILL) &&
		    ((u4Dst == (unsigned int)GFX_COLORMODE_AYCbCr_CLUT8) ||
		     (u4Dst == (unsigned int)GFX_COLORMODE_ARGB_CLUT8)))
				return (int)-E_GFX_INV_ARG;
			return (int)E_GFX_OK;
		} else if ((u4Dst == (unsigned int)GFX_COLORMDOE_YUV_420_BLK) ||
			 (u4Dst == (unsigned int)GFX_COLORMODE_YUV_420_RS) ||
			 (u4Dst == (unsigned int)GFX_COLORMDOE_YUV_422_BLK) ||
			 (u4Dst == (unsigned int)GFX_COLORMODE_YUV_422_RS)) {
			if (u4Op == (unsigned int)OP_RECT_FILL)
				return (int)E_GFX_OK;
		}
		return (int)-E_GFX_INV_ARG;
	}
	// rule:5 - dma mode
	if (u4Op == (unsigned int)OP_DMA) {
		if ((u4Dst == (unsigned int)GFX_COLORMODE_AYCbCr_D8888) ||
		    (u4Dst == (unsigned int)GFX_COLORMODE_ARGB_D8888))
			return (int)E_GFX_OK;

		return (int)-E_GFX_INV_ARG;
	}
	// rule:6 - alphamap mode
	if (u4Op == (unsigned int)OP_ALPHA_MAP_BITBLT) {
		if (((u4Src == (unsigned int)GFX_COLORMODE_AYCbCr_CLUT8) ||
		     (u4Src == (unsigned int)GFX_COLORMODE_ARGB_CLUT8)) &&
		    ((u4Dst == (unsigned int)GFX_COLORMODE_ARGB_D8888) ||
		     (u4Dst == (unsigned int)GFX_COLORMODE_ARGB_D4444) ||
		     (u4Dst == (unsigned int)GFX_COLORMODE_ARGB_D1555)))
			return (int)E_GFX_OK;

		return (int)-E_GFX_INV_ARG;
	}
	// rule:7 - all other mode
	if (u4Src == u4Dst) {
		if ((u4Dst == (unsigned int)GFX_COLORMODE_RGB_D565) ||
		    (u4Dst == (unsigned int)GFX_COLORMODE_ARGB_D1555) ||
		    (u4Dst == (unsigned int)GFX_COLORMODE_ARGB_D4444) ||
		    (u4Dst == (unsigned int)GFX_COLORMODE_ARGB_D8888) ||
		    (u4Dst == (unsigned int)GFX_COLORMODE_AYCbCr_D8888))
			return (int)E_GFX_OK;

		if ((u4Dst == (unsigned int)GFX_COLORMODE_AYCbCr_CLUT8) ||
		    (u4Dst == (unsigned int)GFX_COLORMODE_ARGB_CLUT8)) {
			if ((u4Op == (unsigned int)OP_STRETCH_BITBLT) ||
			    (u4Op == (unsigned int)OP_BITBLT))
			return (int)E_GFX_OK;
		}
	}
	// rule:8 - ybr to rgb mode
	if ((u4Op == (unsigned int)OP_YCRCB_RGB_CNV) ||
		(u4Op == (unsigned int)OP_IDX2DIR_BITBLT) ||
		(u4Op == (unsigned int)OP_IDX2DIR_LOOP_ALPHA_COMPOS)) {
		if (((u4Src == (unsigned int)GFX_COLORMODE_AYCbCr_CLUT8)
		     || (u4Src == (unsigned int)GFX_COLORMODE_ARGB_CLUT8)
		     || (u4Src == (unsigned int)GFX_COLORMDOE_YUV_420_BLK)
		     || (u4Src == (unsigned int)GFX_COLORMODE_YUV_420_RS))
		    && ((u4Dst == (unsigned int)GFX_COLORMODE_RGB_D565)
			|| (u4Dst == (unsigned int)GFX_COLORMODE_ARGB_D1555)
			|| (u4Dst == (unsigned int)GFX_COLORMODE_ARGB_D4444)
			|| (u4Dst == (unsigned int)GFX_COLORMODE_ARGB_D8888)))
			return (int)E_GFX_OK;

		return (int)-E_GFX_INV_ARG;
	}
	// rule:9 - bpcompress only support 32bpp
	if (u4Op == OP_BPCOMP) {
		if ((u4Src == (unsigned int)GFX_COLORMODE_ARGB_D8888)
		    && (u4Dst == (unsigned int)GFX_COLORMODE_ARGB_D8888))
			return (int)E_GFX_OK;
	}

	GFX_LOG_E("[GFX]not support CM(src:%d,dst:%d) in %d op.\n",
		u4Src, u4Dst, u4Op);

	return (int)-E_GFX_INV_ARG;
}

unsigned int _GfxGetColorMode(GFX_COLORMODE_T rMwColorMode)
{
	switch (rMwColorMode) {
	case GFX_COLORMODE_AYCbCr_CLUT8:
		return (unsigned int)CM_YCbCr_CLUT8;

	case GFX_COLORMODE_CbYCrY_16:
		return (unsigned int)CM_CbYCrY422_DIRECT16;

	case GFX_COLORMODE_YCbYCr_16:
		return (unsigned int)CM_YCbYCr422_DIRECT16;

	case GFX_COLORMODE_AYCbCr_D8888:
		return (unsigned int)CM_AYCbCr8888_DIRECT32;

	case GFX_COLORMODE_ARGB_CLUT8:
		return (unsigned int)CM_RGB_CLUT8;

	case GFX_COLORMODE_RGB_D565:
		return (unsigned int)CM_RGB565_DIRECT16;

	case GFX_COLORMODE_ARGB_D1555:
		return (unsigned int)CM_ARGB1555_DIRECT16;

	case GFX_COLORMODE_ARGB_D4444:
		return (unsigned int)CM_ARGB4444_DIRECT16;

	case GFX_COLORMODE_ARGB_D8888:
		return (unsigned int)CM_ARGB8888_DIRECT32;

	default:
		return (unsigned int)CM_ARGB8888_DIRECT32;
	}
}

int _GfxFillRect(unsigned long u4GfxHwId, GFX_FILL_T *prGfxCmd)
{
	unsigned int ux, uy;
	unsigned int ui4ColorY = 0, ui4ColorCbCr = 0;
	unsigned int ui4WidthAlign = 0, ui4HeightAlign = 0;
	GFX_FILL_T *prFill = (GFX_FILL_T *) prGfxCmd;

	if ((void *)(prFill) == NULL)
		return -E_GFX_INV_ARG;

	if (((int)(prFill->i4_src_x) < 0) ||
		((int)(prFill->i4_src_y) < 0) ||
		((int)(prFill->ui4_width) <= 0) ||
		((int)(prFill->ui4_height) <= 0))
		return -E_GFX_INV_ARG;

	if (((int)(prFill->i4_dst_x) < 0) ||
		((int)(prFill->i4_dst_y) < 0) ||
		((int)(prFill->ui4_width) <= 0) ||
		((int)(prFill->ui4_height) <= 0))
		return -E_GFX_INV_ARG;

	if (_GfxOpModeCheck((unsigned int)((uintptr_t) OP_RECT_FILL),
	    (unsigned int)prFill->e_dst_cm,
	    (unsigned int)prFill->e_dst_cm))
		return -E_GFX_INV_ARG;

	ux = prFill->i4_dst_x;
	uy = prFill->i4_dst_y;


	if (prFill->e_dst_cm <= GFX_COLORMODE_ARGB_D8888) {
		GFX_SetDst(u4GfxHwId, (unsigned int *)prFill->pv_dst,
	   _GfxGetColorMode(prFill->e_dst_cm), prFill->ui4_dst_pitch);
		GFX_SetColor(u4GfxHwId, prFill->ui4_color);
		GFX_Fill(u4GfxHwId, ux, uy, prFill->ui4_width,
			prFill->ui4_height);
		GFX_SetDst(u4GfxHwId, (unsigned int *)prFill->pv_dst,
	   _GfxGetColorMode(prFill->e_dst_cm), prFill->ui4_dst_pitch);

		GFX_SetColor(u4GfxHwId, prFill->ui4_color);

		GFX_Fill(u4GfxHwId, ux, uy, prFill->ui4_width,
			prFill->ui4_height);
	} else if ((prFill->e_dst_cm >= GFX_COLORMDOE_YUV_420_BLK) &&
		   (prFill->e_dst_cm <= GFX_COLORMODE_YUV_422_RS)) {
		if ((ux || uy) != 0)
			return -E_GFX_INV_ARG;
		ui4ColorY = (((prFill->ui4_color) >> 16) & 0xff);
		ui4ColorCbCr = (prFill->ui4_color & 0xFFFF);
		ui4ColorCbCr |= (ui4ColorCbCr << 16);
		ui4WidthAlign = (prFill->ui4_width + 15) & (~15);
		ui4HeightAlign = (prFill->ui4_height + 31) & (~31);
		ui4HeightAlign = ((ui4WidthAlign * ui4HeightAlign +
			2047) & (~2047)) / (prFill->ui4_dst_pitch);
		//fill Y
		GFX_SetDst(u4GfxHwId, (unsigned int *)prFill->pv_dst,
			   CM_YCbCr_CLUT8, prFill->ui4_dst_pitch);

		GFX_SetColor(u4GfxHwId, ui4ColorY);

		GFX_Fill(u4GfxHwId, ux, uy, prFill->ui4_dst_pitch,
			ui4HeightAlign);

		//fill CbCr
		GFX_SetDst(u4GfxHwId, (unsigned int *)prFill->pv_dst2,
			   CM_ARGB4444_DIRECT16, prFill->ui4_dst_pitch);

		GFX_SetColor(u4GfxHwId, ui4ColorCbCr);

		//the height is diff to 422 and 420
		if ((prFill->e_dst_cm) <= GFX_COLORMODE_YUV_420_RS)
			ui4HeightAlign = ui4HeightAlign / 2;

		GFX_Fill(u4GfxHwId, ux, uy, prFill->ui4_dst_pitch / 2,
			ui4HeightAlign);

	} else
		return -E_GFX_INV_ARG;

	return E_GFX_OK;
}

int _GfxBitblt(unsigned long u4GfxHwId, GFX_BITBLT_T *prGfxCmd)
{
	unsigned int usx, udx, usy, udy;
	GFX_BITBLT_T *prBitblt = (GFX_BITBLT_T *) prGfxCmd;
	void *pv_src = NULL;
	void *pv_dst = NULL;

	if ((void *)(prBitblt) == NULL)
		return -E_GFX_INV_ARG;

	pv_dst = prBitblt->pv_dst;
	pv_src = prBitblt->pv_src;


	if (((int)(prBitblt->i4_src_x) < 0) ||
		((int)(prBitblt->i4_src_y) < 0) ||
		((int)(prBitblt->ui4_width) <= 0) ||
		((int)(prBitblt->ui4_height) <= 0))
		return -E_GFX_INV_ARG;

	if (((int)(prBitblt->i4_dst_x) < 0) ||
		((int)(prBitblt->i4_dst_y) < 0) ||
		((int)(prBitblt->ui4_width) <= 0) ||
		((int)(prBitblt->ui4_height) <= 0))
		return -E_GFX_INV_ARG;

	if (_GfxOpModeCheck((unsigned int)OP_BITBLT,
		(unsigned int)prBitblt->e_src_cm,
		(unsigned int)prBitblt->e_dst_cm))
		return -E_GFX_INV_ARG;

	usx = prBitblt->i4_src_x;
	usy = prBitblt->i4_src_y;
	udx = prBitblt->i4_dst_x;
	udy = prBitblt->i4_dst_y;


	GFX_SetSrc(u4GfxHwId, (unsigned int *)pv_src,
	_GfxGetColorMode(prBitblt->e_src_cm), prBitblt->ui4_src_pitch);
	GFX_SetDst(u4GfxHwId, (unsigned int *)pv_dst,
	_GfxGetColorMode(prBitblt->e_dst_cm), prBitblt->ui4_dst_pitch);

	GFX_SetBltOpt(u4GfxHwId, (unsigned int)D_GFXFLAG_NONE,
		0x00000000, 0xffffffff);
	if (_GfxGetColorMode(prBitblt->e_src_cm) ==
		CM_ARGB1555_DIRECT16)
		GFX_SetAlpha(u4GfxHwId, 0);
	else
		GFX_SetAlpha(u4GfxHwId, prBitblt->ui4_alpha);

	if (prBitblt->e_src_cm != prBitblt->e_dst_cm)
		GFX_BitBlt(u4GfxHwId, usx, usy, udx, udy,
		prBitblt->ui4_width, prBitblt->ui4_height);
	else
		GFX_BitBlt_NewMethod(u4GfxHwId, usx, usy, udx, udy,
			prBitblt->ui4_width, prBitblt->ui4_height);

	return E_GFX_OK;
}

int _GfxComposeLoop(unsigned long u4GfxHwId, GFX_ALPHA_COMPOSITION_T *prGfxCmd)
{
	unsigned int usx, usy, udx, udy;
	GFX_ALPHA_COMPOSITION_T *prBitblt =
		(GFX_ALPHA_COMPOSITION_T *) prGfxCmd;

	if ((void *)(prBitblt) == NULL)
		return -E_GFX_INV_ARG;

	if (((int)(prBitblt->i4_src_x) < 0) ||
		((int)(prBitblt->i4_src_y) < 0) ||
		((int)(prBitblt->ui4_width) <= 0) ||
		((int)(prBitblt->ui4_height) <= 0))
		return -E_GFX_INV_ARG;

	if (((int)(prBitblt->i4_dst_x) < 0) ||
		((int)(prBitblt->i4_dst_y) < 0) ||
		((int)(prBitblt->ui4_width) <= 0) ||
		((int)(prBitblt->ui4_height) <= 0))
		return -E_GFX_INV_ARG;

	if (_GfxOpModeCheck((unsigned int)OP_ALPHA_COMPOS_BITBLT,
		(unsigned int)prBitblt->e_src_cm,
		(unsigned int)prBitblt->e_dst_cm))
		return -E_GFX_INV_ARG;

	usx = prBitblt->i4_src_x;
	usy = prBitblt->i4_src_y;
	udx = prBitblt->i4_dst_x;
	udy = prBitblt->i4_dst_y;

	GFX_SetSrc(u4GfxHwId, (unsigned int *)prBitblt->pv_src,
	   _GfxGetColorMode(prBitblt->e_src_cm), prBitblt->ui4_src_pitch);

	GFX_SetDst(u4GfxHwId, (unsigned int *)prBitblt->pv_dst,
	   _GfxGetColorMode(prBitblt->e_dst_cm), prBitblt->ui4_dst_pitch);

	GFX_SetAlpha(u4GfxHwId, prBitblt->ui4_alpha);

	GFX_ComposeLoop(u4GfxHwId, usx, usy, udx, udy, prBitblt->ui4_width,
		prBitblt->ui4_height, prBitblt->ui4_alpha,
		(unsigned int)prBitblt->e_rule, 0);

	return E_GFX_OK;
}
