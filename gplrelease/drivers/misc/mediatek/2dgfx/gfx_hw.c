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
#include <linux/wait.h>
#include <linux/sched.h>

#include "gfx_hw.h"
#include "gfx_cmdque.h"
#include "gfx_if.h"
#include "gfx_dif.h"
#include "gfx_drv.h"

struct GFX_HW gfx_hw;


int i4GfxHwProc(unsigned long u4HwInstId)
{
	return 0;
}

int i4GfxHwWait(unsigned long u4HwInstId)
{
	return 0;
}

void GFX_HalSetNotify(unsigned long u4HwInstId)
{
}

static irqreturn_t _GfxHwRealIsr0(void)
{
	unsigned int u4GfxHwId = 0;

	GFX_HwISR(u4GfxHwId);
	GFX_UnlockCmdque(u4GfxHwId);

	return IRQ_HANDLED;

}

static irqreturn_t _GfxHwRealIsr1(void)
{
	unsigned int u4GfxHwId = 1;

	GFX_HwISR(u4GfxHwId);
	GFX_UnlockCmdque(u4GfxHwId);

	return IRQ_HANDLED;

}

unsigned int _GFX_GetInterruptCount(unsigned long u4GfxHwId)
{
	return gfx_hw.u4GfxHwIntCount[u4GfxHwId];
}

void _GFX_DecInterruptCount(unsigned long u4GfxHwId)
{
	if (gfx_hw.u4GfxHwIntCount[u4GfxHwId])
		gfx_hw.u4GfxHwIntCount[u4GfxHwId]--;
}

int GFX_HwCrashDetect(unsigned long u4GfxHwId)
{
	return (int)EGFX_SEMIIDLE;
}

int GFX_HwIrqInit(unsigned int irq_gfx0, unsigned int irq_gfx1,
	unsigned long u4GfxHwId)
{

	if (gfx_hw.i4GfxSetupISR[u4GfxHwId] == GFX_DISABLE) {
		if (u4GfxHwId == 0) {
		if (request_irq(irq_gfx0, (irq_handler_t)
			_GfxHwRealIsr0, IRQF_TRIGGER_HIGH, "gfx0", NULL)) {
			GFX_LOG_I("request GFX%ld IRQ failed\n", u4GfxHwId);
			return -EGFX_IRQFAIL;
		}
		} else {
		if (request_irq(irq_gfx1, (irq_handler_t)
			_GfxHwRealIsr1, IRQF_TRIGGER_HIGH, "gfx1", NULL)) {
			GFX_LOG_I("request GFX%ld IRQ failed\n", u4GfxHwId);
			return -EGFX_IRQFAIL;
		}
		}
		gfx_hw.i4GfxSetupISR[u4GfxHwId] = GFX_ENABLE;
	}

	return (int)E_GFX_OK;
}

int GFX_HwInit(unsigned long u4GfxHwId)
{
	gfx_hw.gu4GfxCheckSum[u4GfxHwId] = 0;
	gfx_hw.gu4DSTFIFO_IN_CHS[u4GfxHwId] = 0;
	gfx_hw.gu4DSTFIFO_OUT_CHS[u4GfxHwId] = 0;
	gfx_hw.gu4SRCFIFO_IN_CHS[u4GfxHwId] = 0;
	gfx_hw.gu4SRCFIFO_OUT_CHS[u4GfxHwId] = 0;
	gfx_hw.fgGfxInt[u4GfxHwId] = false;
	gfx_hw.u4GfxHwIntCount[u4GfxHwId] = 0;

	return (int)E_GFX_OK;
}

int GFX_HwUninit(void)
{
	return (int)E_GFX_OK;
}

int GFX_HwGetRegBase(unsigned long u4GfxHwId, unsigned int **ppu4RegBase)
{
	if (ppu4RegBase != NULL) {
		*ppu4RegBase = (unsigned int *)(uintptr_t)
			(u4GfxHwId ? GFX_ADDR_1 : GFX_ADDR_0);
		return (int)E_GFX_OK;
	}

	return -(int)E_GFX_INV_ARG;
}

void GFX_HwISR(unsigned long u4GfxHwId)
{
	GFX_DifSetIdle(u4GfxHwId, true);
}

int GFX_HwReset(unsigned long u4GfxHwId, unsigned int u4Reset)
{
	unsigned long u4Value;
	unsigned long u4ResHi = 0x3;
	unsigned long u4ResLo = 0x0;

	GFX_WRITE32(u4GfxHwId, GFX_REG_G_CONFIG, 0xA0000000);
	mb();	/* memory barrier */
	GFX_WRITE32(u4GfxHwId, GFX_REG_G_MODE, 0xC00001F0);
	mb();	/* memory barrier */

	u4Value = GFX_READ32(u4GfxHwId, GFX_REG_G_CONFIG);
	u4Value = (u4Value & (~(GREG_MSK_G_RST | GREG_MSK_CQ_RST)));
	u4Value = (u4Value) | (u4ResHi << GREG_SHT_G_RST) |
		(u4ResHi << GREG_SHT_CQ_RST);
	GFX_WRITE32(u4GfxHwId, GFX_REG_G_CONFIG, u4Value);
	u4Value = (u4Value & (~(GREG_MSK_G_RST | GREG_MSK_CQ_RST)));
	u4Value = (u4Value) | (u4ResLo << GREG_SHT_G_RST) |
		(u4ResLo << GREG_SHT_CQ_RST);
	GFX_WRITE32(u4GfxHwId, GFX_REG_G_CONFIG, u4Value);

	for (;;) {
		u4Value = GFX_READ32(u4GfxHwId, GFX_REG_G_CONFIG);

		if (((u4Value & GREG_MSK_G_RST) == GFX_G_RST_READY) &&
		    ((u4Value & GREG_MSK_CQ_RST) == GFX_CQ_RST_READY))
			break;
	}
	u4Value = (u4Value) & (~(GREG_MSK_SDFIFO_THRS));
	u4Value = (u4Value) | (0x1 << GREG_SHT_SDFIFO_THRS);
	u4Value = (u4Value) & (~(GREG_MSK_CMDFIFO_THRS));
	u4Value = (u4Value) | (0x1 << GREG_SHT_CMDFIFO_THRS);
	u4Value = (u4Value) & (~(GREG_MSK_POST_THRS));
	u4Value = (u4Value) | (0x3 << GREG_SHT_POST_THRS);
	u4Value = (u4Value) & (~(GREG_MSK_REQ_INTVAL));
	u4Value = (u4Value) | (0x0 << GREG_SHT_REQ_INTVAL);
	u4Value = (u4Value) & (~(GREG_MSK_ENG_LP));
	u4Value = (u4Value) | (0x1 << GREG_SHT_ENG_LP);
	u4Value = (u4Value | GREG_MSK_INT_MASK);
	GFX_WRITE32(u4GfxHwId, GFX_REG_G_CONFIG, u4Value);
	GFX_WRITE32(u4GfxHwId, GFX_REG_AXI_READ, ((GFX_READ32(u4GfxHwId,
		GFX_REG_AXI_READ)) & 0xfffff0fe) | 0x00000C01));
	GFX_WRITE32(u4GfxHwId, GFX_REG_AVI_WRITE, (((GFX_READ32(u4GfxHwId,
		GFX_REG_AVI_WRITE)) & 0xffff0ff7) | 0x0000c008));

	return (int)E_GFX_OK;
}

void GFX_SetFifo(unsigned long u4GfxHwId, unsigned int ui4_sdfifo,
	unsigned int ui4_cmdfifo, unsigned int ui4_postthrs)
{
	unsigned int u4Value;

	u4Value = GFX_READ32(u4GfxHwId, GFX_REG_G_CONFIG);
	u4Value = (u4Value) & (~(GREG_MSK_SDFIFO_THRS));
	u4Value = (u4Value) | (ui4_sdfifo << GREG_SHT_SDFIFO_THRS);
	u4Value = (u4Value) & (~(GREG_MSK_CMDFIFO_THRS));
	u4Value = (u4Value) | (ui4_cmdfifo << GREG_SHT_CMDFIFO_THRS);
	u4Value = (u4Value) & (~(GREG_MSK_POST_THRS));
	u4Value = (u4Value) | (ui4_postthrs << GREG_SHT_POST_THRS);
	GFX_WRITE32(u4GfxHwId, GFX_REG_G_CONFIG, u4Value);
}

int GFX_HwGetIdle(unsigned long u4GfxHwId)
{
	unsigned int u4Value;

	u4Value = GFX_READ32(u4GfxHwId, GFX_REG_G_STATUS);
	u4Value = ((u4Value & GREG_MSK_IDLE) >> GREG_SHT_IDLE);

	return (int)(u4Value);
}

int GFX_HwGetHwVersion(unsigned long u4GfxHwId, unsigned int *pu4HwVersion)
{
	unsigned int u4Value;

	if (pu4HwVersion == NULL)
		return -(int)E_GFX_INV_ARG;

	u4Value = GFX_READ32(u4GfxHwId, GFX_REG_G_STATUS);
	u4Value = ((u4Value & GREG_MSK_VERSION_ID) >> GREG_SHT_VERSION_ID);

	*pu4HwVersion = u4Value;

	return (int)E_GFX_OK;
}

void GFX_HwEnableLowPowerMode(unsigned long u4GfxHwId)
{
	unsigned int u4Value;

	u4Value = GFX_READ32(u4GfxHwId, GFX_REG_G_CONFIG);
	u4Value = (u4Value) | (GREG_MSK_ENG_LP);
	GFX_WRITE32(u4GfxHwId, GFX_REG_G_CONFIG, u4Value);
}

void GFX_HwDisableLowPowerMode(unsigned long u4GfxHwId)
{
	unsigned int u4Value;

	u4Value = GFX_READ32(u4GfxHwId, GFX_REG_G_CONFIG);
	u4Value = (u4Value) & (~GREG_MSK_ENG_LP);
	GFX_WRITE32(u4GfxHwId, GFX_REG_G_CONFIG, u4Value);
}

int GFX_HwAction(unsigned long u4GfxHwId)
{
	GFX_DifSetIdle(u4GfxHwId, false);

	return (int)E_GFX_OK;
}

void GFX_HwWait(void)
{

}

void GFX_HwSetRiscMode(unsigned long u4GfxHwId)
{
	unsigned int u4Value;

	u4Value = GFX_READ32(u4GfxHwId, GFX_REG_G_CONFIG);
	u4Value = (u4Value) & (~GREG_MSK_EN_DRAMQ);
	GFX_WRITE32(u4GfxHwId, GFX_REG_G_CONFIG, u4Value);
}

void GFX_HwSetCmdQMode(unsigned long u4GfxHwId)
{
	unsigned int u4Value;

	u4Value = GFX_READ32(u4GfxHwId, GFX_REG_G_CONFIG);
	u4Value = (u4Value) | (GREG_MSK_EN_DRAMQ);
	GFX_WRITE32(u4GfxHwId, GFX_REG_G_CONFIG, u4Value);
}

void GFX_HwSetSDFifoThreshold(unsigned long u4GfxHwId, unsigned int u4Value)
{
	unsigned int u4RegVal;

	u4RegVal = GFX_READ32(u4GfxHwId, GFX_REG_G_CONFIG);
	u4RegVal = (u4RegVal) & (~GREG_MSK_SDFIFO_THRS);
	u4RegVal = (u4RegVal) | (u4Value << GREG_SHT_SDFIFO_THRS);
	GFX_WRITE32(u4GfxHwId, GFX_REG_G_CONFIG, u4RegVal);
}

void GFX_HwSetCMDFifoThreshold(unsigned long u4GfxHwId, unsigned int u4Value)
{
	unsigned int u4RegVal;

	u4RegVal = GFX_READ32(u4GfxHwId, GFX_REG_G_CONFIG);
	u4RegVal = (u4RegVal) & (~GREG_MSK_CMDFIFO_THRS);
	u4RegVal = (u4RegVal) | (u4Value << GREG_SHT_CMDFIFO_THRS);
	GFX_WRITE32(u4GfxHwId, GFX_REG_G_CONFIG, u4RegVal);
}

void GFX_HwSetPOSTFifoThreshold(unsigned long u4GfxHwId,
	unsigned int u4Value)
{
	unsigned int u4RegVal;

	u4RegVal = GFX_READ32(u4GfxHwId, GFX_REG_G_CONFIG);
	u4RegVal = (u4RegVal) & (~GREG_MSK_POST_THRS);
	u4RegVal = (u4RegVal) | (u4Value << GREG_SHT_POST_THRS);
	GFX_WRITE32(u4GfxHwId, GFX_REG_G_CONFIG, u4RegVal);
}

void GFX_HwDbgInfo(unsigned long u4GfxHwId)
{
	GFX_LOG_D("\ngfx hw dump - begin\n");

	GFX_LOG_D("\tflush count = %u\n", _GFX_GetFlushCount(u4GfxHwId));
	GFX_LOG_D("\tinterrupt count = %u\n",
		_GFX_GetInterruptCount(u4GfxHwId));
	GFX_LOG_D("\thw fire count = %u\n",
		(GFX_READ32(u4GfxHwId, GFX_REG_0x40E0) & 0x0000ffff));
	GFX_LOG_D("\thw cmdq index = %u\n",
		  ((GFX_READ32(u4GfxHwId, GFX_REG_0x40E4) & 0x3fff0000) >> 16));
	GFX_LOG_D("\thw read  count of dram word = %u\n",
		GFX_READ32(u4GfxHwId, GFX_REG_0x40EC));
	GFX_LOG_D("\thw write count of dram word = %u\n",
		GFX_READ32(u4GfxHwId, GFX_REG_0x40F0));
	GFX_LOG_D("gfx hw dump - end\n\n");
}

int GFX_HwResetEngine(unsigned long u4GfxHwId)
{
	unsigned int u4Value;
	unsigned int u4ResHi = 0x3;
	unsigned int u4ResLo = 0x0;

	u4Value = GFX_READ32(u4GfxHwId, GFX_REG_G_CONFIG);

	u4Value = (u4Value) & (~(GREG_MSK_G_RST));
	u4Value = (u4Value) | (u4ResHi << GREG_SHT_G_RST);
	GFX_WRITE32(u4GfxHwId, GFX_REG_G_CONFIG, u4Value);
	mb();	/* memory barrier */

	u4Value = (u4Value) & (~(GREG_MSK_G_RST));
	u4Value = (u4Value) | (u4ResLo << GREG_SHT_G_RST);
	GFX_WRITE32(u4GfxHwId, GFX_REG_G_CONFIG, u4Value);
	mb();	 /* memory barrier */

	for (;;) {
		u4Value = GFX_READ32(u4GfxHwId, GFX_REG_G_CONFIG);

		if ((u4Value & GREG_MSK_G_RST) == GFX_G_RST_READY)
			break;
	}

	return (int)E_GFX_OK;
}

int GFX_HwResetCmdQue(unsigned long u4GfxHwId)
{
	unsigned int u4Value;
	unsigned long u4ResHi = 0x3;
	unsigned long u4ResLo = 0x0;

	u4Value = GFX_READ32(u4GfxHwId, GFX_REG_G_CONFIG);
	u4Value = (u4Value) & (~(GREG_MSK_CQ_RST));
	u4Value = (u4Value) | (u4ResHi << GREG_SHT_CQ_RST);
	GFX_WRITE32(u4GfxHwId, GFX_REG_G_CONFIG, u4Value);
	mb();	 /* memory barrier */

	u4Value = (u4Value) & (~(GREG_MSK_CQ_RST));
	u4Value = (u4Value) | (u4ResLo << GREG_SHT_CQ_RST);
	GFX_WRITE32(u4GfxHwId, GFX_REG_G_CONFIG, u4Value);
	mb();	 /* memory barrier */

	for (;;) {
		u4Value = GFX_READ32(u4GfxHwId, GFX_REG_G_CONFIG);

		if ((u4Value & GREG_MSK_CQ_RST) == GFX_CQ_RST_READY)
			break;
	}

	return (int)E_GFX_OK;
}

int GFX_HwGetMemCompareResult(unsigned long u4GfxHwId)
{
	unsigned int u4Value;

	u4Value = GFX_READ32(u4GfxHwId, GFX_REG_ROP);
	u4Value = (u4Value & GREG_MSK_CMP_FLAG) >> GREG_SHT_CMP_FLAG;

	return (int)u4Value;
}

void GFX_HwSetReqInterval(unsigned long u4GfxHwId, unsigned int u4ReqInterval)
{
	unsigned int u4Value;

	u4Value = GFX_READ32(u4GfxHwId, GFX_REG_G_CONFIG);

	u4Value = (u4Value) & (~(GREG_MSK_REQ_INTVAL));
	u4Value = (u4Value) | (u4ReqInterval << GREG_SHT_REQ_INTVAL);

	GFX_WRITE32(u4GfxHwId, GFX_REG_G_CONFIG, u4Value);
}

int GFX_HwSetHighPriorityMode(unsigned long u4GfxHwId,
	unsigned int u4HighPriority)
{
	unsigned int u4Val = 0;
	const unsigned int u4Bit[2] = { 5, 0 };

	u4GfxHwId %= 2;
	if (u4HighPriority)
		u4Val |= 1 << u4Bit[u4GfxHwId];
	else
		u4Val &= ~(1 << u4Bit[u4GfxHwId]);

	return (int)E_GFX_OK;
}
