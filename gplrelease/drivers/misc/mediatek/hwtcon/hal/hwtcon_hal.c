/*****************************************************************************
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * Accelerometer Sensor Driver
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 *****************************************************************************/

#include "hwtcon_hal.h"
#include "hwtcon_def.h"
#include "hwtcon_driver.h"

#include <linux/dma-mapping.h>

int hwtcon_hal_get_time_in_ms(HWTCON_TIME start, HWTCON_TIME end)
{
	HWTCON_TIME duration = end - start;

	return duration;
}

HWTCON_TIME timeofday_ms(void)
{
	struct timeval tv;

	do_gettimeofday(&tv);
	return ((HWTCON_TIME) tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

int hwtcon_hal_get_gpt_time_in_unit(HWTCON_TIME start, HWTCON_TIME end)
{
	/* 13 unit = 1us */
	HWTCON_TIME duration = 0;

	if (start > end)
		end += 0x100000000;
	duration = end - start;
	return duration;
}

u32 *hwtcon_hal_convert_pa_2_va(u32 pa)
{
	if (pa >= hwtcon_driver_get_img_rdma_pa() &&
		pa < hwtcon_driver_get_img_rdma_pa() + 0x1000)
		return (u32 *)((pa - hwtcon_driver_get_img_rdma_pa()) +
			hwtcon_driver_get_img_rdma_va());
	else if (pa >= hwtcon_driver_get_wb_rdma_pa() &&
		pa < hwtcon_driver_get_wb_rdma_pa() + 0x1000)
		return (u32 *)((pa - hwtcon_driver_get_wb_rdma_pa()) +
			hwtcon_driver_get_wb_rdma_va());
	else if (pa >= hwtcon_driver_get_wb_wdma_pa() &&
		pa < hwtcon_driver_get_wb_wdma_pa() + 0x1000)
		return (u32 *)((pa - hwtcon_driver_get_wb_wdma_pa()) +
			hwtcon_driver_get_wb_wdma_va());
	else if (pa >= hwtcon_driver_get_pipeline_pa() &&
		pa < hwtcon_driver_get_pipeline_pa() + 0x1000)
		return (u32 *)((pa - hwtcon_driver_get_pipeline_pa()) +
			hwtcon_driver_get_pipeline_va());
	else if (pa >= hwtcon_driver_get_regal_pa() &&
		pa < hwtcon_driver_get_regal_pa() + 0x1000)
		return (u32 *)((pa - hwtcon_driver_get_regal_pa()) +
			hwtcon_driver_get_regal_va());
	else if (pa >= hwtcon_driver_get_paper_top_pa() &&
		pa < hwtcon_driver_get_paper_top_pa() + 0x1000)
		return (u32 *)((pa - hwtcon_driver_get_paper_top_pa()) +
			hwtcon_driver_get_paper_top_va());

	else if (pa >= hwtcon_driver_get_wf_lut_rdma_pa() &&
		pa < hwtcon_driver_get_wf_lut_rdma_pa() + 0x1000)
		return (u32 *)((pa - hwtcon_driver_get_wf_lut_rdma_pa()) +
			hwtcon_driver_get_wf_lut_rdma_va());
	else if (pa >= hwtcon_driver_get_wf_lut_dpi_pa() &&
		pa < hwtcon_driver_get_wf_lut_dpi_pa() + 0x1000)
		return (u32 *)((pa - hwtcon_driver_get_wf_lut_dpi_pa()) +
			hwtcon_driver_get_wf_lut_dpi_va());
	else if (pa >= hwtcon_driver_get_tcon_pa() &&
		pa < hwtcon_driver_get_tcon_pa() + 0x1000)
		return (u32 *)((pa - hwtcon_driver_get_tcon_pa()) +
			hwtcon_driver_get_tcon_va());
	else if (pa >= hwtcon_driver_get_mmsys_pa() &&
		pa < hwtcon_driver_get_mmsys_pa() + 0x1000)
		return (u32 *)((pa - hwtcon_driver_get_mmsys_pa()) +
			hwtcon_driver_get_mmsys_va());
	else if (pa >= hwtcon_driver_get_wf_lut_pa() &&
		pa < hwtcon_driver_get_wf_lut_pa() + 0x1000)
		return (u32 *)((pa - hwtcon_driver_get_wf_lut_pa()) +
			hwtcon_driver_get_wf_lut_va());

	return NULL;
}

u32 pp_read(void *va)
{
	return readl(va);
}

u32 pp_read_pa(u32 pa)
{
	u32 *va = ioremap(pa, sizeof(u32));
	u32 value = 0;

	if (pa >= 0x14007000 && pa < 0x14008000) {
		if (!hwtcon_driver_pipeline_clk_is_enable()) {
			TCON_ERR("TCON clock disable while access TCON");
			dump_stack();
		}
	}
	value = readl(va);

	iounmap(va);
	return value;
}

void pp_write(struct cmdqRecStruct *pkt, u32 pa, u32 value)
{
	if (pa >= 0x14007000 && pa < 0x14008000) {
		if (!hwtcon_driver_pipeline_clk_is_enable()) {
			TCON_ERR("TCON clock disable while access TCON");
			dump_stack();
		}
	}

	if (!pkt) {
		u32 *va = hwtcon_hal_convert_pa_2_va(pa);
		bool register_remap = false;

		if (va == NULL) {
			va = ioremap(pa, sizeof(u32));
			register_remap = true;
		}

		writel(value, va);

		if (register_remap)
			iounmap(va);
	} else {
		/* use gce */
		#if 0
		cmdq_pkt_assign_command(pkt, GCE_SPR0, pa);
		cmdq_pkt_store_value(pkt, GCE_SPR0, value, 0xFFFFFFFF);
		#else
		cmdqRecWrite(pkt, pa, value, 0xFFFFFFFF);
		#endif
	}
}

void pp_write_mask(struct cmdqRecStruct *pkt, u32 pa, u32 value, u32 mask)
{
	if (pa >= 0x14007000 && pa < 0x14008000) {
		if (!hwtcon_driver_pipeline_clk_is_enable()) {
			TCON_ERR("TCON clock disable while access TCON");
			dump_stack();
		}
	}

	if (!pkt) {
		/* only update mask bit = 1, for 0 case, do not update. */
		u32 *va = hwtcon_hal_convert_pa_2_va(pa);
		bool register_remap = false;
		u32 read_back = 0;

		if (va == NULL) {
			va = ioremap(pa, sizeof(u32));
			register_remap = true;
		}
		read_back = pp_read(va);
		if (register_remap)
			iounmap(va);
		pp_write(pkt, pa, (read_back & ~mask) | (value & mask));
	} else {
		/* use gce */
		#if 0
		cmdq_pkt_assign_command(pkt, GCE_SPR0, pa);
		cmdq_pkt_store_value(pkt, GCE_SPR0, value, mask);
		#else
		cmdqRecWrite(pkt, pa, value, mask);
		#endif
	}
}

void pp_poll(struct cmdqRecStruct *pkt, u32 pa, u32 value, u32 mask)
{
	/* TODO: */
}
