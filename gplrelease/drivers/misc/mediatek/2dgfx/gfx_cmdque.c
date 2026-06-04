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

#include <linux/slab.h>
#include "gfx_if.h"
#include "gfx_cmdque.h"
#include "gfx_dif.h"
#include "gfx_hw.h"
#include "gfx_drv.h"

#ifdef GFX_RISC_MODE
#define D_GFX_CQ_HW_INT_MODE    1
#else
#define D_GFX_CQ_HW_INT_MODE    0
#endif
#define E_GFX_CQ_ALIGN          16
#define GFX_CMQ_MIN_SIZE        2
#define GFX_CMD_MARGIN          2
#define GFX_CMD_MAX_FIRE       10
#define GFX_ONE_CMD_SIZE        8

static GFX_CMDQUE_T _rGfxCmdQue[GFX_HAL_HW_INST_NUM];
GFX_CMDQUE_VAR Gfx_CmdQueVar[GFX_HAL_HW_INST_NUM];

void _Gfx_CmdQueVarInt(unsigned int i4)
{

	Gfx_CmdQueVar[i4].u4GfxHwFlushCount = 0;
	Gfx_CmdQueVar[i4].u4GfxSwFlushCount = 0;
	Gfx_CmdQueVar[i4].u4GfxSwIntCount = 0;
#ifdef GFX_PERFORMANCE_TEST
	Gfx_CmdQueVar[i4].i4GfxCqCapacity = (int)EGFX_CPT_128KB;
#else
	Gfx_CmdQueVar[i4].i4GfxCqCapacity = (int)EGFX_CPT_32KB;
#endif
	Gfx_CmdQueVar[i4].pu8GfxCmdqueBuf = NULL;
	Gfx_CmdQueVar[i4]._i4GfxCmdqueBufExist = false;
	Gfx_CmdQueVar[i4].gfx_cmdq_bus_addr = 0;
	Gfx_CmdQueVar[i4].pu8GfxCmdQueTopAddr = NULL;
	Gfx_CmdQueVar[i4].gfx_irq_done = false;
}

unsigned int _GFX_GetFlushCount(unsigned long u4GfxHwId)
{
	return Gfx_CmdQueVar[u4GfxHwId].u4GfxHwFlushCount;
}

unsigned int GFX_GetCmdQBusaddr(unsigned long u4GfxHwId)
{
	return Gfx_CmdQueVar[u4GfxHwId].gfx_cmdq_bus_addr;
}

int GFX_CmdQueInit(unsigned long i4)
{
	int i4Ret = E_GFX_OK;
	unsigned int u4GfxRegBase;

	_Gfx_CmdQueVarInt(i4);

	if (GFX_DifGetData(i4)->u4GfxMode == (unsigned int)E_GFX_SW_MOD) {
		i4Ret = GFX_DifGetRegBase(i4, &u4GfxRegBase);
		VERIFY(i4Ret == (int)E_GFX_OK);
		Gfx_CmdQueVar[i4].prRegBase = (MI_DIF_UNION_T *)
			(uintptr_t) u4GfxRegBase;
	}

	if (Gfx_CmdQueVar[i4]._i4GfxCmdqueBufExist == (int)false) {
		unsigned int u4Size, u4Size1, u4Align;

		u4Size = (unsigned int)(Gfx_CmdQueVar[i4].i4GfxCqCapacity *
			GFX_ONE_CMD_SIZE);
		u4Align = (unsigned int)(E_GFX_CQ_ALIGN);
		u4Size1 = u4Size + u4Align;
		Gfx_CmdQueVar[i4].pu8GfxCmdqueBuf =
			kzalloc(u4Size1, GFP_KERNEL);

		if (((uintptr_t) Gfx_CmdQueVar[i4].pu8GfxCmdqueBuf &
			0xf) != 0) {
			Gfx_CmdQueVar[i4].pu8GfxCmdqueBuf =
			(unsigned long long *) (((uintptr_t)
			Gfx_CmdQueVar[i4].pu8GfxCmdqueBuf + 0xf) & (~0xf));
		}

		memset((void *)Gfx_CmdQueVar[i4].pu8GfxCmdqueBuf, 0, u4Size);
		GFX_LOG_D("GFX_CmdQueInit\n");

		GFX_LOG_D("\n[GFX] Gfx_CmdQueVar[i4].pu8GfxCmdqueBuf= 0x%p\n",
			  Gfx_CmdQueVar[i4].pu8GfxCmdqueBuf);
		VERIFY(Gfx_CmdQueVar[i4].pu8GfxCmdqueBuf != NULL);

		Gfx_CmdQueVar[i4]._i4GfxCmdqueBufExist = (int)true;

	}

	i4Ret = GFX_CmdQueReset(i4);
	return i4Ret;
}

int GFX_CmdQueUninit(void)
{
	int u4GfxHwId	= 0;
	int i4Ret		= E_GFX_OK;
	unsigned int u4GfxRegBase;

	for (u4GfxHwId = 0; u4GfxHwId < GFX_HAL_HW_INST_NUM; u4GfxHwId++) {
		if (GFX_DifGetData(u4GfxHwId)->u4GfxMode ==
				(unsigned int)E_GFX_SW_MOD) {

			i4Ret = GFX_DifGetRegBase(u4GfxHwId, &u4GfxRegBase);
			VERIFY(i4Ret == (int)E_GFX_OK);

			Gfx_CmdQueVar[u4GfxHwId].prRegBase =
			    (MI_DIF_UNION_T *)(uintptr_t) u4GfxRegBase;
		}

		if (Gfx_CmdQueVar[u4GfxHwId]._i4GfxCmdqueBufExist ==
			(int)true) {
			kfree((void *)Gfx_CmdQueVar[u4GfxHwId].pu8GfxCmdqueBuf);
			Gfx_CmdQueVar[u4GfxHwId]._i4GfxCmdqueBufExist =
				(int)false;
		}
	}

	return i4Ret;
}



int GFX_CmdQueReset(unsigned long u4GfxHwId)
{
	_rGfxCmdQue[u4GfxHwId].i4QueCapacity =
		Gfx_CmdQueVar[u4GfxHwId].i4GfxCqCapacity;
	_rGfxCmdQue[u4GfxHwId].i4QueSize = 0;
	_rGfxCmdQue[u4GfxHwId].i4Idle = 1;
	_rGfxCmdQue[u4GfxHwId].i4ReadIndex = 0;
	_rGfxCmdQue[u4GfxHwId].i4WriteIndex = 0;
	_rGfxCmdQue[u4GfxHwId].bNeedFlushAll = false;

	_rGfxCmdQue[u4GfxHwId].i4ShortCmdque =
	    (Gfx_CmdQueVar[u4GfxHwId].i4GfxCqCapacity < (int)EGFX_CPT_32KB);

	switch (Gfx_CmdQueVar[u4GfxHwId].i4GfxCqCapacity) {
	case (int)EGFX_CPT_256KB:
		_rGfxCmdQue[u4GfxHwId].i4CqSizeCfg = (int)EGFX_CQCFG_256KB;
		break;
	case (int)EGFX_CPT_128KB:
		_rGfxCmdQue[u4GfxHwId].i4CqSizeCfg = (int)EGFX_CQCFG_128KB;
		break;
	case (int)EGFX_CPT_64KB:
		_rGfxCmdQue[u4GfxHwId].i4CqSizeCfg = (int)EGFX_CQCFG_64KB;
		break;
	case (int)EGFX_CPT_32KB:
		_rGfxCmdQue[u4GfxHwId].i4CqSizeCfg = (int)EGFX_CQCFG_32KB;
		break;
	case (int)EGFX_CPT_16KB:
		_rGfxCmdQue[u4GfxHwId].i4CqSizeCfg = (int)EGFX_CQCFG_16KB;
		break;
	case (int)EGFX_CPT_8KB:
		_rGfxCmdQue[u4GfxHwId].i4CqSizeCfg = (int)EGFX_CQCFG_8KB;
		break;
	case (int)EGFX_CPT_4KB:
		_rGfxCmdQue[u4GfxHwId].i4CqSizeCfg = (int)EGFX_CQCFG_4KB;
		break;
	case (int)EGFX_CPT_2KB:
		_rGfxCmdQue[u4GfxHwId].i4CqSizeCfg = (int)EGFX_CQCFG_2KB;
		break;
	default:
		break;
	}

	_rGfxCmdQue[u4GfxHwId].pu8QueTop =
		(Gfx_CmdQueVar[u4GfxHwId].pu8GfxCmdqueBuf);

	if (GFX_DifGetData(u4GfxHwId)->u4GfxMode ==
			(unsigned int)E_GFX_HW_8520_MOD) {
		unsigned int u4Value, u4Value1, u4Value2;

		u4Value = GFX_READ32(u4GfxHwId, GFX_REG_G_CONFIG);
		u4Value = (u4Value) & (~GREG_MSK_EN_DRAMQ);
		GFX_WRITE32(u4GfxHwId, GFX_REG_G_CONFIG, u4Value);

		u4Value = GFX_READ32(u4GfxHwId, GFX_REG_G_CONFIG);
		u4Value = (u4Value) & (~GREG_MSK_DRAMQ_MODE);
		GFX_WRITE32(u4GfxHwId, GFX_REG_G_CONFIG, u4Value);
		mb();	 /* memory barrier */

		u4Value1 = (unsigned int)(__virt_to_bus((uintptr_t)
		_rGfxCmdQue[u4GfxHwId].pu8QueTop));

		Gfx_CmdQueVar[u4GfxHwId].gfx_cmdq_bus_addr = u4Value1;

		u4Value = (unsigned int)((uintptr_t)
		_rGfxCmdQue[u4GfxHwId].i4CqSizeCfg << GREG_SHT_CYC_SIZE) |
		(__virt_to_bus(((uintptr_t)
		_rGfxCmdQue[u4GfxHwId].pu8QueTop)) & 0x3FFFFFFF);

		GFX_WRITE32(u4GfxHwId, GFX_REG_DRAMQ_STAD, u4Value);
		mb();	 /* memory barrier */

		u4Value2 = u4Value1 & 0xC0000000;
		u4Value2 = u4Value2 | GFX_READ32(u4GfxHwId, GFX_REG_VA_MSB);
		GFX_WRITE32(u4GfxHwId, GFX_REG_VA_MSB, u4Value2);
		mb();	 /* memory barrier */
		u4Value = GFX_READ32(u4GfxHwId, GFX_REG_G_CONFIG);
		u4Value = (u4Value) | ((unsigned int)
		_rGfxCmdQue[u4GfxHwId].i4ShortCmdque << GREG_SHT_SRAM_LP);
		GFX_WRITE32(u4GfxHwId, GFX_REG_G_CONFIG, u4Value);
		mb();	 /* memory barrier */


#ifdef GFX_RISC_MODE
		u4Value = GFX_READ32(u4GfxHwId, GFX_REG_G_CONFIG);
		u4Value = (u4Value) & (~GREG_MSK_EN_DRAMQ);
		GFX_WRITE32(u4GfxHwId, GFX_REG_G_CONFIG, u4Value);
#else
		u4Value = GFX_READ32(u4GfxHwId, GFX_REG_G_CONFIG);
		u4Value = (u4Value) | (GREG_MSK_EN_DRAMQ);
		GFX_WRITE32(u4GfxHwId, GFX_REG_G_CONFIG, u4Value);
#endif
		mb();	 /* memory barrier */

	}
	return (int)E_GFX_OK;
}

int GFX_CmdQuePushBack(unsigned long u4GfxHwId,
	unsigned int u4Reg, unsigned int u4Val)
{
	int i4Ret = (int)E_GFX_OK;

	ASSERT(_rGfxCmdQue[u4GfxHwId].pu8QueTop != NULL);
	ASSERT(_rGfxCmdQue[u4GfxHwId].i4QueCapacity != 0);

	if ((_rGfxCmdQue[u4GfxHwId].i4QueSize + GFX_CMD_MARGIN) >=
	    _rGfxCmdQue[u4GfxHwId].i4QueCapacity) {
		i4Ret = GFX_CmdQueAction(u4GfxHwId);
		if (i4Ret)
			return i4Ret;
	}
	u4Reg = ((u4GfxHwId ? GFX_ADDR_1 : GFX_ADDR_0) |
		((u4Reg << 2) & 0xfff));

	_rGfxCmdQue[u4GfxHwId].i4QueSize++;

	_rGfxCmdQue[u4GfxHwId].pu8QueTop[
		_rGfxCmdQue[u4GfxHwId].i4WriteIndex++] =
		((((long)(u4Reg)) << 32) | u4Val);

	if (_rGfxCmdQue[u4GfxHwId].i4WriteIndex
			>= _rGfxCmdQue[u4GfxHwId].i4QueCapacity)
		_rGfxCmdQue[u4GfxHwId].i4WriteIndex = 0;

	return i4Ret;
}

void GFX_CmdQueDbgInfo(unsigned long u4GfxHwId)
{
	GFX_LOG_D("\ngfx cmdq dump - begin\n");

	GFX_LOG_D("\t_rGfxCmdQue.i4QueCapacity = %d\n",
		_rGfxCmdQue[u4GfxHwId].i4QueCapacity);
	GFX_LOG_D("\t_rGfxCmdQue.i4QueSize = %d\n",
		_rGfxCmdQue[u4GfxHwId].i4QueSize);
	GFX_LOG_D("\t_rGfxCmdQue.i4PrevIndex = 0x%08x\n",
		_rGfxCmdQue[u4GfxHwId].i4PrevIndex);
	GFX_LOG_D("\t_rGfxCmdQue.i4ReadIndex = 0x%08x\n",
		_rGfxCmdQue[u4GfxHwId].i4ReadIndex);
	GFX_LOG_D("\t_rGfxCmdQue.i4WriteIndex = 0x%08x\n",
		_rGfxCmdQue[u4GfxHwId].i4WriteIndex);
	GFX_LOG_D("\t_rGfxCmdQue.i4Idle = %d\n",
		_rGfxCmdQue[u4GfxHwId].i4Idle);
	GFX_LOG_D("\t_rGfxCmdQue.pu8QueTop = 0x%p\n",
		_rGfxCmdQue[u4GfxHwId].pu8QueTop);

	GFX_LOG_D("gfx cmdq dump - end\n\n");
}

void GFX_CmdQueSetCqCapacity(unsigned long u4GfxHwId, int i4Capacity)
{
	Gfx_CmdQueVar[u4GfxHwId].i4GfxCqCapacity = i4Capacity;
}

#ifdef GFX_QUE_REG
static unsigned int gui4QueueSize;
static unsigned int gui4PreExt;
#endif

int GFX_CmdQueCheckSize(unsigned long u4GfxHwId)
{
	int i4Ret = E_GFX_OK;

	if (_rGfxCmdQue[u4GfxHwId].i4QueSize & 1)
		i4Ret = GFX_CmdQuePushBack(u4GfxHwId, (int)(GREG_DUMMY), 0);

	return i4Ret;
}

int GFX_CmdQueAction(unsigned long u4GfxHwId)
{
	unsigned int u4CmdQueLen;
	int i4Ret = E_GFX_OK;

#ifndef GFX_RISC_MODE

	if (!GFX_DifGetIdle(u4GfxHwId))
		GFX_Wait();
	GFX_DifSetIdle(u4GfxHwId, false);

	if (_rGfxCmdQue[u4GfxHwId].i4QueSize < GFX_CMQ_MIN_SIZE) {
		int i4Ret;
		int i4Count;
		int i4Times;

		i4Times = ((int)GFX_CMQ_MIN_SIZE -
			_rGfxCmdQue[u4GfxHwId].i4QueSize);

		for (i4Count = 0; i4Count < i4Times; i4Count++) {
			i4Ret = GFX_CmdQuePushBack(u4GfxHwId,
				(int)GREG_DUMMY, 0);
			if (i4Ret)
				return i4Ret;
		}
	}
	if (_rGfxCmdQue[u4GfxHwId].i4QueSize & 1) {
		i4Ret = GFX_CmdQuePushBack(u4GfxHwId,
			(int)(GREG_DUMMY), 0);
		if (i4Ret)
			return i4Ret;
	}

	if (GFX_DifGetData(u4GfxHwId)->u4GfxMode ==
		(unsigned int)E_GFX_HW_8520_MOD) {
#ifdef GFX_QUE_REG
		unsigned int ui4i = 0;

		for (ui4i = 0; ui4i < 80; ui4i++)
			Gfx_CmdQueVar[u4GfxHwId].GfxReg[ui4i] =
			GFX_READ32(4 * ui4i);
#endif
		++Gfx_CmdQueVar[u4GfxHwId].u4GfxHwFlushCount;
		UNUSEDGFX(Gfx_CmdQueVar[u4GfxHwId].u4GfxHwFlushCount);

		Gfx_CmdQueVar[u4GfxHwId].gfx_irq_done = false;
		u4CmdQueLen =
		    (unsigned int)(_rGfxCmdQue[u4GfxHwId].i4QueSize *
				   GFX_ONE_CMD_SIZE) | 0x10000000;

		GFX_WRITE32(u4GfxHwId, GFX_REG_DRAMQ_LEN, u4CmdQueLen);

		GFX_LockCmdque(u4GfxHwId);
		_rGfxCmdQue[u4GfxHwId].i4PrevIndex
			= _rGfxCmdQue[u4GfxHwId].i4ReadIndex;
		_rGfxCmdQue[u4GfxHwId].i4ReadIndex
			= _rGfxCmdQue[u4GfxHwId].i4WriteIndex;
		_rGfxCmdQue[u4GfxHwId].i4QueSize = 0;
		_rGfxCmdQue[u4GfxHwId].bNeedFlushAll = false;

	}
#endif

	return i4Ret;
}

#ifdef GFX_QUE_REG
void Gfx_PrintCmdQueue(void)
{
	unsigned int *pui4_data;
	unsigned int *pui4_data_old;
	unsigned int ui4_idx;

	pui4_data = (unsigned int *)
		(&_rGfxCmdQue.pu8QueTop[_rGfxCmdQue.i4PrevIndex]);
	pui4_data_old = (unsigned int *)
		(&_rGfxCmdQue.pu8QueTop[gui4PreExt]);
	GFX_LOG_D("\n\n************* Queue info excuted **********\n");
	GFX_LOG_D("Old_PrevReadIdx = 0x%x,PrevReadIdx = 0x%x",
		gui4PreExt, _rGfxCmdQue.i4PrevIndex);
	for (ui4_idx = 0;
		ui4_idx < _rGfxCmdQue.i4PrevIndex - gui4PreExt; ui4_idx++) {
		GFX_LOG_D("[GFX] 0x%x | ", pui4_data_old[ui4_idx * 2]);
		GFX_LOG_D("0x%x\n", pui4_data_old[ui4_idx * 2 + 1]);

	}

	GFX_LOG_D("\n\n************* Queue info **********\n");
	GFX_LOG_D("PrevReadIdx = 0x%x, ReadIdx = 0x%x, WriteIdx=0x%x\n ",
		_rGfxCmdQue.i4PrevIndex, _rGfxCmdQue.i4ReadIndex,
		_rGfxCmdQue.i4WriteIndex);
	for (ui4_idx = 0; ui4_idx < gui4QueueSize; ui4_idx++) {
		GFX_LOG_D("[GFX] 0x%x | ", pui4_data[ui4_idx * 2]);
		GFX_LOG_D("0x%x\n", pui4_data[ui4_idx * 2 + 1]);
	}

}
#endif

int GFX_RiscPushBack(unsigned long u4GfxHwId, unsigned int u4Reg,
							unsigned int u4Val)
{
	int i4Ret = (int)E_GFX_OK;
	unsigned int u4RegOffset;

	u4RegOffset = ((u4Reg << 2) & 0xfff);

	if (GFX_DifGetData(u4GfxHwId)->u4GfxMode ==
		(unsigned int)E_GFX_SW_MOD) {
		GFX_CmdQuePushBack(u4GfxHwId, u4Reg, u4Val);
	} else if ((u4RegOffset == (unsigned int)GFX_REG_G_MODE)
		   && ((u4Val & GREG_MSK_FIRE) == GREG_MSK_FIRE)) {

		GFX_LockCmdque(u4GfxHwId);
		Gfx_CmdQueVar[u4GfxHwId].gfx_irq_done = true;
		GFX_WRITE32(u4GfxHwId, u4RegOffset, u4Val);
		GFX_UnlockCmdque(u4GfxHwId);
	} else
		GFX_WRITE32(u4GfxHwId, u4RegOffset, u4Val);
	return i4Ret;
}

void GFX_CmdQueBackup(void)
{
	Gfx_CmdQueVar[0].pu8GfxCmdQueTopAddr = _rGfxCmdQue[0].pu8QueTop;
}

void GFX_CmdQueRestore(void)
{
	unsigned int u4Value;

	u4Value = GFX_READ32(0, GFX_REG_G_CONFIG);
	u4Value |= 0xF0000000;
	GFX_WRITE32(0, GFX_REG_G_CONFIG, u4Value);
	mb();	 /* memory barrier */
	u4Value &= 0x0FFFFFFF;
	GFX_WRITE32(0, GFX_REG_G_CONFIG, u4Value);
	mb();	 /* memory barrier */

	_rGfxCmdQue[0].pu8QueTop = Gfx_CmdQueVar[0].pu8GfxCmdQueTopAddr;
	_rGfxCmdQue[0].i4QueSize = 0;
	_rGfxCmdQue[0].i4ReadIndex = 0;
	_rGfxCmdQue[0].i4WriteIndex = 0;
	_rGfxCmdQue[0].bNeedFlushAll = false;

	u4Value = (unsigned int)((uintptr_t)
		_rGfxCmdQue[0].i4CqSizeCfg << GREG_SHT_CYC_SIZE) |
	    __virt_to_bus(((uintptr_t) _rGfxCmdQue[0].pu8QueTop));

	GFX_WRITE32(0, GFX_REG_DRAMQ_STAD, u4Value);
}
