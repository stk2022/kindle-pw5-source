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

#include "gfx_dif.h"
#include "gfx_hw.h"
#include "gfx_cmdque.h"
#include "gfx_if.h"
#include "gfx_drv.h"

static MI_DIF_T _rDifData[GFX_HAL_HW_INST_NUM] = {
	{(unsigned int)(GFX_HAVE_SW_MOD + GFX_HAVE_HW_8520_MOD),
	(unsigned int)E_GFX_HW_8520_MOD,
	0,
	(unsigned int *)NULL,
	(unsigned int)true,
	(bool) false},
	{(unsigned int)(GFX_HAVE_SW_MOD + GFX_HAVE_HW_8520_MOD),
	(unsigned int)E_GFX_HW_8520_MOD,
	0,
	(unsigned int *)NULL,
	(unsigned int)true,
	(bool) false}
};

int pfnGFX_DifAction(unsigned long u4GfxHwId)
{
	if (_rDifData[u4GfxHwId].u4GfxMode != (unsigned int)E_GFX_SW_MOD)
		GFX_HwAction(u4GfxHwId);
	return 1;
}


int pfnGFX_DifGetInternalIdle(unsigned long u4GfxHwId)
{
	return 1;
}


void pfnGFX_DifWait(unsigned long u4GfxHwId)
{

}

void GFX_DifSetIdle(unsigned long u4GfxHwId, int i4Idle)
{
	_rDifData[u4GfxHwId].i4DifIdle = i4Idle;
}


int GFX_DifGetIdle(unsigned long u4GfxHwId)
{
	return _rDifData[u4GfxHwId].i4DifIdle;
}

MI_DIF_T *GFX_DifGetData(unsigned long u4GfxHwId)
{
	return &_rDifData[u4GfxHwId];
}

int GFX_DifIrqInit(unsigned int irq_gfx0, unsigned int irq_gfx1,
	unsigned long u4GfxHwId)
{
	int i4Ret;

	i4Ret = GFX_HwIrqInit(irq_gfx0, irq_gfx1, u4GfxHwId);

	return i4Ret;

}

int GFX_DifInit(unsigned long u4GfxHwId)
{
	int i4Ret;

	GFX_DifSetIdle(u4GfxHwId, true);

	if (MA_DIF_HAVE_GFX_HW8520(u4GfxHwId)) {
		i4Ret = GFX_HwInit(u4GfxHwId);
		if (i4Ret == (int)E_GFX_OK) {
			MA_DIF_GFX_HW8520_MOD_OK(u4GfxHwId);
			GFX_DifSetMode(u4GfxHwId,
				(unsigned int)E_GFX_HW_8520_MOD);
		}
	}
	MA_DIF_GFX_FB_MOD_OK(u4GfxHwId);

	return (int)E_GFX_OK;
}

int GFX_DifUninit(void)
{
	return (int)E_GFX_OK;
}

int GFX_DifSetRegBase(unsigned long u4GfxHwId, unsigned int *pu4Base)
{
	if (pu4Base != NULL) {
		_rDifData[u4GfxHwId].pu4CrBase = pu4Base;
		return (int)E_GFX_OK;
	}

	return -(int)E_GFX_INV_ARG;
}

int GFX_DifGetRegBase(unsigned long u4GfxHwId, unsigned int *pu4Base)
{
	if (pu4Base != NULL) {
		pu4Base = (_rDifData[u4GfxHwId].pu4CrBase);
		return (int)E_GFX_OK;
	}

	return -(int)E_GFX_INV_ARG;
}

int GFX_DifReset(unsigned long u4GfxHwId, unsigned int u4Reset)
{
	if (MA_DIF_HAVE_GFX_HW8520(u4GfxHwId))
		GFX_HwReset(u4GfxHwId, u4Reset);
	else
		GFX_UNUSED_RET(memset(_rDifData[u4GfxHwId].pu4CrBase, 0,
				      (sizeof(unsigned int) * GREG_FILE_SIZE)))
	return (int)E_GFX_OK;
}

void GFX_DifSetMode(unsigned long u4GfxHwId, unsigned int u4GfxMode)
{
	int i4Ret;
	unsigned int u4GfxRegBase;
	unsigned int *pu4GfxRegBase = &u4GfxRegBase;

	if (u4GfxMode == (unsigned int)E_GFX_HW_8520_MOD) {
		i4Ret = GFX_HwGetRegBase(u4GfxHwId, &pu4GfxRegBase);
		VERIFY(i4Ret == (int)E_GFX_OK);

		i4Ret = GFX_DifSetRegBase(u4GfxHwId, pu4GfxRegBase);
		VERIFY(i4Ret == (int)E_GFX_OK);
	}

	_rDifData[u4GfxHwId].u4GfxMode = u4GfxMode;
	i4Ret = GFX_CmdQueInit(u4GfxHwId);
	VERIFY(i4Ret == (int)E_GFX_OK);
}

int GFX_DifSetCR(unsigned long u4GfxHwId, unsigned int u4CrName,
	unsigned int u4Val)
{
	if (_rDifData[u4GfxHwId].pu4CrBase != NULL) {
		_rDifData[u4GfxHwId].pu4CrBase[u4CrName] = u4Val;
		return (int)E_GFX_OK;
	}

	return -(int)E_GFX_INV_ARG;
}

int GFX_DifGetCR(unsigned long u4GfxHwId, unsigned int u4CrName,
	unsigned int *pu4Val)
{
	if (pu4Val && _rDifData[u4GfxHwId].pu4CrBase) {
		*pu4Val = _rDifData[u4GfxHwId].pu4CrBase[u4CrName];
		return (int)E_GFX_OK;
	}

	return -(int)E_GFX_INV_ARG;
}
