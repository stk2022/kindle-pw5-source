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

#ifndef GFX_CMDQUE_H
#define GFX_CMDQUE_H


#define PAGE_OFFSEY 0
#define __bus_to_virt(X) (X + PAGE_OFFSEY)
#define __virt_to_bus(X) (X - PAGE_OFFSEY)

#ifndef ASSERT
#define ASSERT(expr)\
	do {\
		if (expr)\
			break;\
		pr_info("gfx ASSERT FAILED %s, %d\n", __FILE__, __LINE__);\
		WARN_ON(1);\
	} while (0)
#endif


enum EGFX_CMDQUE_CAPACITY {
	EGFX_CPT_256KB = 32768,
	EGFX_CPT_128KB = 16384,
	EGFX_CPT_64KB = 8192,
	EGFX_CPT_32KB = 4096,
	EGFX_CPT_16KB = 2048,
	EGFX_CPT_8KB = 1024,
	EGFX_CPT_4KB = 512,
	EGFX_CPT_2KB = 256,
	EGFX_CQCFG_256KB = 0x3,
	EGFX_CQCFG_128KB = 0x2,
	EGFX_CQCFG_64KB = 0x1,
	EGFX_CQCFG_32KB = 0x0,
	EGFX_CQCFG_16KB = 0x3,
	EGFX_CQCFG_8KB = 0x2,
	EGFX_CQCFG_4KB = 0x1,
	EGFX_CQCFG_2KB = 0x0,
};

struct GFX_CMDQUE_T {
	int i4QueCapacity;
	int i4QueSize;
	int i4PrevIndex;
	int i4ReadIndex;
	int i4WriteIndex;
	int i4Idle;
	int i4ShortCmdque;
	int i4CqSizeCfg;
	unsigned int bNeedFlushAll;
	unsigned long long *pu8QueTop;
	unsigned long cmdq_mva;
} GFX_CMDQUE_T;

struct GFX_CMDQUE_VAR {
	unsigned int u4GfxHwFlushCount;
	int i4GfxCqCapacity;
	unsigned long u4GfxSwFlushCount;
	unsigned long u4GfxSwIntCount;
	unsigned long long *pu8GfxCmdqueBuf;
	int _i4GfxCmdqueBufExist;
#ifdef GFX_QUE_REG
	unsigned long GfxReg[80];
#endif
	unsigned int gfx_cmdq_bus_addr;
	unsigned long long *pu8GfxCmdQueTopAddr;
	bool gfx_irq_done;

} GFX_CMDQUE_VAR;

extern GFX_CMDQUE_VAR Gfx_CmdQueVar[GFX_HAL_HW_INST_NUM];

unsigned int _GFX_GetFlushCount(unsigned long u4GfxHwId);
unsigned int GFX_GetCmdQBusaddr(unsigned long u4GfxHwId);
int GFX_CmdQueInit(unsigned long i4);
int GFX_CmdQueUninit(void);
int GFX_CmdQueReset(unsigned long u4GfxHwId);
int GFX_CmdQueAction(unsigned long u4GfxHwId);
int GFX_RiscPushBack(unsigned long u4GfxHwId,
	unsigned int u4Reg, unsigned int u4Val);
int GFX_CmdQuePushBack(unsigned long u4GfxHwId,
	unsigned int u4Reg, unsigned int u4Val);
void GFX_CmdQueSetCqCapacity(unsigned long u4GfxHwId, int i4Capacity);
void GFX_CmdQueDbgInfo(unsigned long u4GfxHwId);
#endif
