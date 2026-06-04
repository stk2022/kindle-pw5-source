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

#ifndef GFX_HW_H
#define GFX_HW_H

#include <linux/err.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include "mt-plat/sync_write.h"
#include "gfx_if.h"

#ifdef GFX_PERFORMANCE_TEST
#define GFX_PERFORMANCE_TEST_SUPPORT 1
#else
#define GFX_PERFORMANCE_TEST_SUPPORT 0
#endif

#define GFX_HAL_HW_INST_NUM	 2
#define GFX_ADDR_0				(0x1400a000)
#define GFX_ADDR_1				(0x1400b000)
#define GFX_REG_G_CONFIG        (0x00)
#define GFX_REG_G_STATUS        (0x04)
#define GFX_REG_DRAMQ_STAD      (0x08)
#define GFX_REG_DRAMQ_LEN       (0x0C)
#define GFX_REG_G_MODE          (0x10)
#define GFX_REG_SRC_BSAD        (0x18)
#define GFX_REG_DST_BSAD        (0x1C)
#define GFX_REG_SRC_SIZE        (0x28)
#define GFX_REG_S_OSD_WIDTH     (0x2C)
#define GFX_REG_LEGAL_START     (0x90)
#define GFX_REG_LEGAL_END       (0x94)
#define GFX_REG_VA_MSB			(0xB8)
#define GFX_REG_0x40E0          (0xE0)
#define GFX_REG_0x40E4          (0xE4)
#define GFX_REG_0x40EC          (0xEC)
#define GFX_REG_0x40F0          (0xF0)
#define GFX_REG_WAIT_MMU        (0xFC)
#define GFX_REG_ROP             (0xA0)
#define GFX_REG_IDX2DIR         (0xA4)
#define GFX_REG_MMU_SF          (0x104)
#define GFX_REG_G_256CPT        (0x108)
#define GFX_REG_G_PGIG          (0x124)
#define GFX_REG_SRC_RING        (0x130)
#define GFX_REG_AXI_READ        (0x14c)
#define GFX_REG_AVI_WRITE       (0x154)


//------------------------------------------------
// GFX HW registers' shift
#define GREG_SHT_G_RST          30
#define GREG_SHT_CQ_RST         28
#define GREG_SHT_SHORT_CMQ      25
#define GREG_SHT_DYN_HIGH       24
#define GREG_SHT_MMU_CMQ        23
#define GREG_SHT_ENG_LP         17
#define GREG_SHT_SRAM_LP        16
#define GREG_SHT_SDFIFO_THRS     9
#define GREG_SHT_DRAMQ_MODE      8
#define GREG_SHT_REQ_INTVAL      6
#define GREG_SHT_CMDFIFO_THRS    4
#define GREG_SHT_POST_THRS       2
#define GREG_SHT_INT_MASK        1
#define GREG_SHT_EN_DRAMQ        0
//   [G_STATUS] - 0x4004
#define GREG_SHT_VERSION_ID      8
#define GREG_SHT_IDLE            0
//   [DRAMQ_STAD] - 0x4008
#define GREG_SHT_CYC_SIZE       30
#define GREG_SHT_DRAMQ_BSAD      0

//   [G_MODE] - 0x400C
#define GREG_SHT_SW_MODE             28

//   [G_MODE] - 0x4010
#define GREG_SHT_CM                  0
#define GREG_SHT_OP_MODE             4
#define GREG_SHT_FIRE               11
#define GREG_SHT_BURST_EN           12
#define GREG_SHT_BURST_MODE         13
#define GREG_SHT_SRCQ_FIRE          12
#define GREG_SHT_DSTQ_FIRE          13

//   [SRC_BSAD] - 0x4018
#define GREG_SHT_SRC_BSAD            0
#define GREG_SHT_CHAR_CM            30
//   [DST_BSAD] - 0x401C
#define GREG_SHT_DST_BSAD            0
#define GREG_SHT_DST_BSAD_WR        31
//   [SRC_SIZE] - 0x4028
#define GREG_SHT_SRC_WIDTH           0
#define GREG_SHT_RL_DEC             15
#define GREG_SHT_SRC_HEIGHT         16
#define GREG_SHT_SRC_CM             27
//   [S_OSD_WIDTH] - 0x402C
#define GREG_SHT_OSD_WIDTH           0
#define GREG_SHT_SRC_PITCH          16
//   [ROP] - 0x40A0
#define GREG_SHT_CMP_FLAG        9
//   [IDX2DIR] - 0x40A4
#define GREG_SHT_PAL_BSAD            0
#define GREG_SHT_MSB_LEFT           30
#define GREG_SHT_LN_ST_BYTE_AL      31
// [MMU Self Fire] - 0x40fc
#define GREG_SHT_CMQ_SELF_FIRE_CYC  0
#define GREG_SHT_CMQ_BYPASS_MMU     27
#define GREG_SHT_SELF_FIRE_EN       28
#define GREG_SHT_ORG_FIRE_EN        29
#define GREG_SHT_CMQ_SELF_FIRE_EN   30
#define GREG_SHT_HW_SELF_FIRE_EN    31

//   [G_256CPT] - 0x4108
#define GREG_SHT_256CPT_FIRE         0
#define GREG_SHT_USE_256CPT          1
#define GREG_SHT_CPT_DRAM_EN         2
#define GREG_SHT_IDX2DIR_EN          7
//   [G_PGIG] - 0x4124
#define GREG_SHT_PGIG_RLE_EN         0
#define GREG_SHT_PGIG_DIR_EN         1
#define GREG_SHT_HDMV_CPT_NUM        8
#define GREG_SHT_YR_SEL             16
#define GREG_SHT_UG_SEL             18
#define GREG_SHT_VB_SEL             20
#define GREG_SHT_A_SEL              22
#define GREG_SHT_SRCQ_RING_EN       24
#define GREG_SHT_DSTQ_RING_EN       25
//   [SRC_RING] - 0x4130
#define GREG_SHT_SRC_END_BSAD        0
#define GREG_SHT_SRC_SIZE_EN        30
#define GREG_SHT_SRC_BSAD_WR        31
//   [SRC_RING] - 0x4134
#define GREG_SHT_SRCQ_LEN             0
#define GREG_SHT_WDLE                1
#define GREG_SHT_DST_HEIGHT_RING    16

//------------------------------------------------
// GFX HW registers' mask
//   [G_CONFIG] - 0x4000
#define GREG_MSK_G_RST          (0x3 << GREG_SHT_G_RST)
#define GREG_MSK_CQ_RST         (0x3 << GREG_SHT_CQ_RST)
#define GREG_MSK_SHORT_CMQ      (0x1 << GREG_SHT_SHORT_CMQ)
#define GREG_MSK_DYN_HIGH       (0x1 << GREG_SHT_DYN_HIGH)
#define GREG_MSK_MMU_CMQ        (0x1 << GREG_SHT_MMU_CMQ)
#define GREG_MSK_ENG_LP         (0x1 << GREG_SHT_ENG_LP)
#define GREG_MSK_SRAM_LP		(0x1 << GREG_SHT_SRAM_LP)
#define GREG_MSK_SDFIFO_THRS    (0x3 << GREG_SHT_SDFIFO_THRS)
#define GREG_MSK_DRAMQ_MODE     (0x1 << GREG_SHT_DRAMQ_MODE)
#define GREG_MSK_REQ_INTVAL     (0x3 << GREG_SHT_REQ_INTVAL)
#define GREG_MSK_CMDFIFO_THRS   (0x3 << GREG_SHT_CMDFIFO_THRS)
#define GREG_MSK_POST_THRS      (0x3 << GREG_SHT_POST_THRS)
#define GREG_MSK_INT_MASK       (0x1 << GREG_SHT_INT_MASK)
#define GREG_MSK_EN_DRAMQ       (0x1 << GREG_SHT_EN_DRAMQ)
//   [G_STATUS] - 0x4004
#define GREG_MSK_VERSION_ID     (0xFF << GREG_SHT_VERSION_ID)
#define GREG_MSK_IDLE           (0x1 << GREG_SHT_IDLE)
//   [DRAMQ_STAD] - 0x4008
#define GREG_MSK_CYC_SIZE       (0x3 << GREG_SHT_CYC_SIZE)
#define GREG_MSK_DRAMQ_BSAD     (0xFFFFFFF << GREG_SHT_DRAMQ_BSAD)
//   [SRC_BSAD] - 0x4018
#define GREG_MSK_SRC_BSAD		(0x3FFFFFFF << GREG_SHT_SRC_BSAD)
#define GREG_MSK_CHAR_CM		(0x3 << GREG_SHT_CHAR_CM)

//   [G_MODE] - 0x4010
#define GREG_MSK_CM				(0xF << GREG_SHT_CM)
#define GREG_MSK_OP_MODE		(0x1F << GREG_SHT_OP_MODE)
#define GREG_MSK_FIRE			(0x1 << GREG_SHT_FIRE)
#define GREG_MSK_BURST_EN		(0x1 << GREG_SHT_BURST_EN)
#define GREG_MSK_BURST_MODE		(0x3 << GREG_SHT_BURST_MODE)
#define GREG_MSK_SRCQ_FIRE		(0x1 << GREG_SHT_SRCQ_FIRE)
#define GREG_MSK_DSTQ_FIRE		(0x1 << GREG_SHT_DSTQ_FIRE)

//   [DST_BSAD] - 0x401C
#define GREG_MSK_DST_BSAD		(0x3FFFFFFF << GREG_SHT_DST_BSAD)
#define GREG_MSK_DST_BSAD_WR	(0x1 << GREG_SHT_DST_BSAD_WR)
//   [SRC_SIZE] - 0x4028
#define GREG_MSK_SRC_WIDTH		(0x7FFF << GREG_SHT_SRC_WIDTH)
#define GREG_MSK_RL_DEC			(0x1 << GREG_SHT_RL_DEC)
#define GREG_MSK_SRC_HEIGHT		(0x7FFF << GREG_SHT_SRC_HEIGHT)
#define GREG_MSK_SRC_CM			(0xF << GREG_SHT_SRC_CM)

//   [S_OSD_WIDTH] - 0x402C
#define GREG_MSK_OSD_WIDTH		(0xFFFF << GREG_SHT_OSD_WIDTH)
#define GREG_MSK_SRC_PITCH		(0xFFFF << GREG_SHT_SRC_PITCH)
//   [ROP] - 0x40A0
#define GREG_MSK_CMP_FLAG		(0x1 << GREG_SHT_CMP_FLAG)
//   [IDX2DIR] - 0x40A4
#define GREG_MSK_PAL_BSAD		(0x3FFFFFFF << GREG_SHT_PAL_BSAD)
#define GREG_MSK_MSB_LEFT		(0x1 << GREG_SHT_MSB_LEFT)
#define GREG_MSK_LN_ST_BYTE_AL	(0x1 << GREG_SHT_LN_ST_BYTE_AL)
//   [G_256CPT] - 0x4108
#define GREG_MSK_256CPT_FIRE	(0x1 << GREG_SHT_256CPT_FIRE)
#define GREG_MSK_USE_256CPT		(0x1 << GREG_SHT_USE_256CPT)
#define GREG_MSK_CPT_DRAM_EN	(0x1 << GREG_SHT_CPT_DRAM_EN)
#define GREG_MSK_IDX2DIR_EN		(0x1 << GREG_SHT_IDX2DIR_EN)
//   [G_PGIG] - 0x4124
#define GREG_MSK_PGIG_RLE_EN	(0x1 << GREG_SHT_PGIG_RLE_EN)
#define GREG_MSK_PGIG_DIR_EN	(0x1 << GREG_SHT_PGIG_DIR_EN)
#define GREG_MSK_HDMV_CPT_NUM	(0xFF << GREG_SHT_HDMV_CPT_NUM)
#define GREG_MSK_YR_SEL			(0x3 << GREG_SHT_YR_SEL)
#define GREG_MSK_UG_SEL			(0x3 << GREG_SHT_UG_SEL)
#define GREG_MSK_VB_SEL			(0x3 << GREG_SHT_VB_SEL)
#define GREG_MSK_A_SEL			(0x3 << GREG_SHT_A_SEL)

#define GREG_MSK_SRCQ_RING_EN	(0x1 << GREG_SHT_SRCQ_RING_EN)
#define GREG_MSK_DSTQ_RING_EN	(0x1 << GREG_SHT_DSTQ_RING_EN)
//   [SRC_RING] - 0x4130
#define GREG_MSK_SRC_END_BSAD	(0x3FFFFFFF << GREG_SHT_SRC_END_BSAD)
#define GREG_MSK_SRC_SIZE_EN	(0x1 << GREG_SHT_SRC_SIZE_EN)
#define GREG_MSK_SRC_BSAD_WR	(0x1 << GREG_SHT_SRC_BSAD_WR)
//   [SRC_RING] - 0x4134
#define GREG_MSK_SRCQ_LEN		(0xFFFF << GREG_SHT_SRCQ_LEN)
#define GREG_MSK_DST_HEIGHT_RING (0x7FF << GREG_SHT_DST_HEIGHT_RING)
#define GREG_MSK_WDLE			(0x1 << GREG_SHT_WDLE)
#define GFX_G_RST_READY			(0x80000000)
#define GFX_CQ_RST_READY		(0x20000000)
#define GIC_GFX0_IRQ_ID    206
#define GIC_GFX1_IRQ_ID    205
#define D_GFX_CRASH_DEAD_LINE   (10)

enum _ENUM_GFX_CORE_STATUS_T {
	EGFX_IDLE = 0,
	EGFX_BUSY,
	EGFX_SEMIIDLE,
	EGFX_CRASHED,
	EGFX_IRQFAIL,
	EGFX_UNKNOWN
} ENUM_GFX_CORE_STATUS_T;

struct GFX_HW {
	unsigned int u4GfxHwIntCount[GFX_HAL_HW_INST_NUM];
	unsigned int i4GfxSetupISR[GFX_HAL_HW_INST_NUM];
	unsigned int gu4GfxCheckSum[GFX_HAL_HW_INST_NUM];
	unsigned int gu4DSTFIFO_IN_CHS[GFX_HAL_HW_INST_NUM];
	unsigned int gu4DSTFIFO_OUT_CHS[GFX_HAL_HW_INST_NUM];
	unsigned int gu4SRCFIFO_IN_CHS[GFX_HAL_HW_INST_NUM];
	unsigned int gu4SRCFIFO_OUT_CHS[GFX_HAL_HW_INST_NUM];
	bool fgGfxInt[GFX_HAL_HW_INST_NUM];
} GFX_HW;

extern struct gfx_device *gGfxDev;

#define Gfx_ReadReg(u4Addr)			readl_relaxed(u4Addr)
#define Gfx_WriteReg(arg, val)		writel_relaxed(val, arg)
#define GFX_READ32(_u4GfxHwId_, _offset_) \
Gfx_ReadReg(((_u4GfxHwId_ ?  gGfxDev->GFX1_DIST_BASE : \
gGfxDev->GFX0_DIST_BASE) + (_offset_)))

#ifdef GFX_RISC_MODE
#define GFX_WRITE32(_u4GfxHwId_, _offset_, _val_) \
Gfx_WriteReg(((_u4GfxHwId_ ?  gGfxDev->GFX1_DIST_BASE :\
gGfxDev->GFX0_DIST_BASE) + (_offset_)), (_val_))
#else
#define GFX_WRITE32(_u4GfxHwId_, _offset_, _val_) \
Gfx_WriteReg(((_u4GfxHwId_ ?  gGfxDev->GFX1_DIST_BASE :\
gGfxDev->GFX0_DIST_BASE) + (_offset_)), (_val_))
#endif

extern int GFX_HwInit(unsigned long u4GfxHwId);
extern int GFX_HwIrqInit(unsigned int irq_gfx0, unsigned int irq_gfx1,
	unsigned long u4GfxHwId);
extern int GFX_HwUninit(void);
extern int GFX_HwInit2(void);
extern int GFX_HwGetRegBase(unsigned long u4GfxHwId,
	unsigned int **ppu4RegBase);
extern void GFX_HwISR(unsigned long u4GfxHwId);
extern void GFX_HwWait(void);
extern int GFX_HwReset(unsigned long u4GfxHwId, unsigned int u4Reset);
extern int GFX_HwGetIdle(unsigned long u4GfxHwId);
extern int GFX_HwAction(unsigned long u4GfxHwId);
extern void GFX_HwSetRiscMode(unsigned long u4GfxHwId);
extern void GFX_HwSetCmdQMode(unsigned long u4GfxHwId);
extern void GFX_HwSetReqInterval(unsigned long u4GfxHwId,
	unsigned int u4ReqInterval);
extern unsigned int _GFX_GetInterruptCount(unsigned long u4GfxHwId);
extern int GFX_HwGetMemCompareResult(unsigned long u4GfxHwId);
extern int i4GfxHwProc(unsigned long u4HwInstId);
extern int i4GfxHwWait(unsigned long u4HwInstId);
extern int GFX_HwResetEngine(unsigned long u4GfxHwId);
extern int GFX_HwResetCmdQue(unsigned long u4GfxHwId);
int GFX_HwSetHighPriorityMode(unsigned long u4GfxHwId,
	unsigned int u4HighPriority);

#endif
