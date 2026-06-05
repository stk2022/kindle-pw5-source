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
#include "hwtcon_driver.h"
#include "hwtcon_debug.h"
#include "hwtcon_def.h"
#include "hwtcon_fb.h"
#include "hwtcon_core.h"
#include "hwtcon_ioctl_cmd.h"
#include "hwtcon_hal.h"
#include "hwtcon_reg.h"
#include "hwtcon_dpi_config.h"
#include "hwtcon_tcon_config.h"
#include "mediatek/smi.h"
#include "include/mt-plat/mtk_devinfo.h"


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/dma-mapping.h>
#include <linux/of_address.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include "hwtcon_epd.h"
#include "fiti_core.h"
#include "hwtcon_wf_lut_config.h"
#include <linux/of.h>
#include <linux/of_device.h>

static struct hwtcon_device_info_struct g_hwtcon_device_info;

struct hwtcon_device_info_struct *hwtcon_device_info(void)
{
	return &g_hwtcon_device_info;
}

static char *hwtcon_driver_parse_reserved_buffer(struct platform_device *pdev,
	struct sg_table *sgt,
	u32 *pa,
	u32 *mva,
	u32 *size)
{
	struct device_node *np;
	struct resource res;
	char *map_va;

	np = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	if (np == NULL) {
		TCON_ERR("parse memory-region fail");
		return NULL;
	}

	if (of_address_to_resource(np, 0, &res)) {
		TCON_ERR("parse reserved buffer fail");
		return NULL;
	}

	*pa = (u32)res.start;
	*size = (u32)resource_size(&res);

	//map_va = (char *)phys_to_virt(*pa);
	map_va = ioremap_wc(*pa, *size);
	//map_va = of_iomap(np, 0);
	if (map_va == NULL) {
		TCON_ERR("map pa[0x%08x] to va fail", *pa);
		return NULL;
	}


	/* map pa to mva */
	if (sg_alloc_table(sgt, 1, GFP_KERNEL) != 0) {
		TCON_ERR("allocate sg table fail");
		iounmap(map_va);
		return NULL;
	}

	sg_dma_len(sgt->sgl) = *size;
	sg_set_page(sgt->sgl, phys_to_page(*pa), *size, 0);
	if (dma_map_sg_attrs(&pdev->dev, sgt->sgl, sgt->nents,
		DMA_BIDIRECTIONAL,
		DMA_ATTR_SKIP_CPU_SYNC) == 0) {
		TCON_ERR("dma_map_sg fail");
		iounmap(map_va);
		sg_free_table(sgt);
		return NULL;
	}

	*mva = sg_dma_address(sgt->sgl);

	return map_va;	
}

static int hwtcon_driver_parse_pa(struct device_node *node, int index,
	u32 *pa)
{
	struct resource res;

	if (of_address_to_resource(node, index, &res)) {
		TCON_ERR("parse index:%d fail", index);
		return HWTCON_STATUS_GET_RESOURCE_FAIL;
	}
	*pa = (u32)res.start;

	return 0;
}

struct device *hwtcon_driver_get_smi_device(struct platform_device *pdev)
{
	struct platform_device *larb_pdev;
	struct device_node *larb_node = NULL;

	/* smi larb */
	larb_node =  of_parse_phandle(pdev->dev.of_node, "mediatek,larb", 0);
	if (!larb_node) {
		TCON_ERR("get larb node fail");
		return NULL;
	}

	larb_pdev = of_find_device_by_node(larb_node);
	of_node_put(larb_node);
	if ((!larb_pdev) || (!larb_pdev->dev.driver)) {
		TCON_ERR("hwtcon_probe is earlier than SMI");
		return NULL;
	}

	return &larb_pdev->dev;
}

static int hwtcon_driver_init_device_info(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct device_node *epd_node = NULL;
	int status = 0;

	epd_node = of_get_child_by_name(node, "epd");
	if (epd_node)
		hwtcon_epd_init_device_info(epd_node);
	else
		TCON_ERR("epd node get fail!");
	of_node_put(epd_node);

	g_hwtcon_device_info.dev = &pdev->dev;
	g_hwtcon_device_info.larb_dev = hwtcon_driver_get_smi_device(pdev);

	#ifndef FPGA_EARLY_PORTING
	/* clock parse */
	g_hwtcon_device_info.clock_info.pipeline0 = devm_clk_get(&pdev->dev,
			"pipeline0");
	g_hwtcon_device_info.clock_info.pipeline1 = devm_clk_get(&pdev->dev,
			"pipeline1");
	g_hwtcon_device_info.clock_info.pipeline2 = devm_clk_get(&pdev->dev,
			"pipeline2");
	g_hwtcon_device_info.clock_info.pipeline3 = devm_clk_get(&pdev->dev,
			"pipeline3");
	g_hwtcon_device_info.clock_info.pipeline4 = devm_clk_get(&pdev->dev,
			"pipeline4");
	g_hwtcon_device_info.clock_info.pipeline5 = devm_clk_get(&pdev->dev,
			"pipeline5");
	g_hwtcon_device_info.clock_info.pipeline7 = devm_clk_get(&pdev->dev,
			"pipeline7");
	g_hwtcon_device_info.clock_info.dpi_tmp0 = devm_clk_get(&pdev->dev,
			"dpi_tmp0");
	g_hwtcon_device_info.clock_info.dpi_tmp1 = devm_clk_get(&pdev->dev,
			"dpi_tmp1");
	#endif

	/* mmsys  */
	status = hwtcon_driver_parse_pa(node, 0,
		&g_hwtcon_device_info.mmsys_reg_pa);
	if (status != 0) {
		TCON_ERR("parse mmsys pa fail");
		return status;
	}
	g_hwtcon_device_info.mmsys_reg_va = (char *)of_iomap(node, 0);
	if (g_hwtcon_device_info.mmsys_reg_va == NULL) {
		TCON_ERR("get mmsys va fail");
		return HWTCON_STATUS_OF_IOMAP_FAIL;
	}

	/* imgsys */
	status = hwtcon_driver_parse_pa(node, 1,
		&g_hwtcon_device_info.imgsys_reg_pa);
	if (status != 0) {
		TCON_ERR("parse imgsys pa fail");
		return status;
	}
	g_hwtcon_device_info.imgsys_reg_va = (char *)of_iomap(node, 1);
	if (g_hwtcon_device_info.imgsys_reg_va == NULL) {
		TCON_ERR("get imgsys va fail");
		return HWTCON_STATUS_OF_IOMAP_FAIL;
	}

	/* reserved buffer from dts */
	g_hwtcon_device_info.reserved_buf_ready = true;
	g_hwtcon_device_info.reserved_buf_va = hwtcon_driver_parse_reserved_buffer(pdev, 
		&g_hwtcon_device_info.reserved_sgt,
		&g_hwtcon_device_info.reserved_buf_pa,
		&g_hwtcon_device_info.reserved_buf_mva,
		&g_hwtcon_device_info.reserved_buf_size);
	if (g_hwtcon_device_info.reserved_buf_va == NULL) {
		TCON_ERR("parse reserved buffer fail");
		g_hwtcon_device_info.reserved_buf_ready = false;
	}

	if (g_hwtcon_device_info.reserved_buf_ready &&
		(g_hwtcon_device_info.reserved_buf_size < WAVEFORM_SIZE +
		hw_tcon_get_edp_width() * hw_tcon_get_edp_height() * 2)) {
		TCON_ERR("reserved buffer size[%d] small for res[%d %d]",
			g_hwtcon_device_info.reserved_buf_size,
			hw_tcon_get_edp_width(),
			hw_tcon_get_edp_height());
		g_hwtcon_device_info.reserved_buf_ready = false;
	}

	if (g_hwtcon_device_info.reserved_buf_ready)
		hwtcon_debug_err_printf("parse reserved buffer: pa[0x%08x] mva[0x%08x] va[%p] size[0x%08x]",
			g_hwtcon_device_info.reserved_buf_pa,
			g_hwtcon_device_info.reserved_buf_mva,
			g_hwtcon_device_info.reserved_buf_va,
			g_hwtcon_device_info.reserved_buf_size);

	g_hwtcon_device_info.irq_id[IRQ_PIPELINE_WB_FRAME_DONE] = platform_get_irq(pdev, 0);
	if (g_hwtcon_device_info.irq_id[IRQ_PIPELINE_WB_FRAME_DONE] < 0) {
		TCON_ERR("IRQ_PIPELINE_WB_FRAME_DONE get irq fail");
		return HWTCON_STATUS_GET_IRQ_ID_FAIL;
	}

	g_hwtcon_device_info.irq_id[IRQ_WF_LUT_FRAME_DONE] = platform_get_irq(pdev, 1);
	if (g_hwtcon_device_info.irq_id[IRQ_WF_LUT_FRAME_DONE] < 0) {
		TCON_ERR("IRQ_WF_LUT_FRAME_DONE get irq fail");
		return HWTCON_STATUS_GET_IRQ_ID_FAIL;
	}
	
	g_hwtcon_device_info.irq_id[IRQ_WF_LUT_RELEASE] = platform_get_irq(pdev, 2);
	if (g_hwtcon_device_info.irq_id[IRQ_WF_LUT_RELEASE] < 0) {
		TCON_ERR("IRQ_WF_LUT_RELEASE get irq fail");
		return HWTCON_STATUS_GET_IRQ_ID_FAIL;
	}

#if 0
	g_hwtcon_device_info.irq_id[IRQ_PIPELINE_DPI_UPDATE_DONE] = platform_get_irq(pdev, 3);
	if (g_hwtcon_device_info.irq_id[IRQ_PIPELINE_DPI_UPDATE_DONE] < 0) {
		TCON_ERR("IRQ_PIPELINE_DPI_UPDATE_DONE get irq fail");
		return HWTCON_STATUS_GET_IRQ_ID_FAIL;
	}
#endif

	g_hwtcon_device_info.irq_id[IRQ_WF_LUT_TCON_END] = platform_get_irq(pdev, 4);
	if (g_hwtcon_device_info.irq_id[IRQ_WF_LUT_TCON_END] < 0) {
		TCON_ERR("IRQ_WF_LUT_TCON_END get irq fail");
		return HWTCON_STATUS_GET_IRQ_ID_FAIL;
	}

	g_hwtcon_device_info.irq_id[IRQ_PIPELINE_PIXEL_LUT_COLLISION] = platform_get_irq(pdev, 5);
	if (g_hwtcon_device_info.irq_id[IRQ_PIPELINE_PIXEL_LUT_COLLISION] < 0) {
		TCON_ERR("IRQ_PIPELINE_PIXEL_LUT_COLLISION get irq fail");
		return HWTCON_STATUS_GET_IRQ_ID_FAIL;
	}

	TCON_LOG("mmsys: pa[0x%x] va[%p]",
		g_hwtcon_device_info.mmsys_reg_pa,
		g_hwtcon_device_info.mmsys_reg_va);

	TCON_LOG("imgsys: pa[0x%x] va[%p]",
		g_hwtcon_device_info.imgsys_reg_pa,
		g_hwtcon_device_info.imgsys_reg_va);

	hwtcon_debug_err_printf("mmsys: pa[0x%x] va[%p]",
		g_hwtcon_device_info.mmsys_reg_pa,
		g_hwtcon_device_info.mmsys_reg_va);

	hwtcon_debug_err_printf("imgsys: pa[0x%x] va[%p]",
		g_hwtcon_device_info.imgsys_reg_pa,
		g_hwtcon_device_info.imgsys_reg_va);

	return 0;
}


static int hwtcon_driver_destroy_device_info(struct platform_device *pdev)
{
#ifndef FPGA_EARLY_PORTING
	if (g_hwtcon_device_info.clock_info.pipeline0)
		devm_clk_put(&pdev->dev,
			g_hwtcon_device_info.clock_info.pipeline0);
	if (g_hwtcon_device_info.clock_info.pipeline1)
		devm_clk_put(&pdev->dev,
			g_hwtcon_device_info.clock_info.pipeline1);
	if (g_hwtcon_device_info.clock_info.pipeline2)
		devm_clk_put(&pdev->dev,
			g_hwtcon_device_info.clock_info.pipeline2);
	if (g_hwtcon_device_info.clock_info.pipeline3)
		devm_clk_put(&pdev->dev,
			g_hwtcon_device_info.clock_info.pipeline3);
	if (g_hwtcon_device_info.clock_info.pipeline4)
		devm_clk_put(&pdev->dev,
			g_hwtcon_device_info.clock_info.pipeline4);
	if (g_hwtcon_device_info.clock_info.pipeline5)
		devm_clk_put(&pdev->dev,
			g_hwtcon_device_info.clock_info.pipeline5);
	if (g_hwtcon_device_info.clock_info.pipeline7)
		devm_clk_put(&pdev->dev,
			g_hwtcon_device_info.clock_info.pipeline7);
	if (g_hwtcon_device_info.clock_info.dpi_tmp0)
		devm_clk_put(&pdev->dev,
			g_hwtcon_device_info.clock_info.dpi_tmp0);
	if (g_hwtcon_device_info.clock_info.dpi_tmp1)
		devm_clk_put(&pdev->dev,
			g_hwtcon_device_info.clock_info.dpi_tmp1);
#endif

	if (g_hwtcon_device_info.mmsys_reg_va)
		iounmap(g_hwtcon_device_info.mmsys_reg_va);
	if (g_hwtcon_device_info.imgsys_reg_va)
		iounmap(g_hwtcon_device_info.imgsys_reg_va);


	if (g_hwtcon_device_info.reserved_buf_va) {
		iounmap(g_hwtcon_device_info.reserved_buf_va);

		dma_unmap_sg_attrs(&pdev->dev,
			g_hwtcon_device_info.reserved_sgt.sgl,
			g_hwtcon_device_info.reserved_sgt.nents,
			DMA_BIDIRECTIONAL,
			DMA_ATTR_SKIP_CPU_SYNC);
		sg_free_table(&g_hwtcon_device_info.reserved_sgt);
	}


	return 0;
}

bool hwtcon_driver_check_clk_on(struct clk *clock)
{
	if (IS_ERR(clock)) {
		TCON_ERR("invalid clock");
		return false;
	}

	return __clk_is_enabled(clock);
}

unsigned long hwtcon_driver_get_mmsys_clock_rate(void)
{
	if (IS_ERR(hwtcon_device_info()->clock_info.pipeline0)) {
		TCON_ERR("parse mmsys clock fail");
		return 0UL;
	}

	if (!__clk_is_enabled(hwtcon_device_info()->clock_info.pipeline0)) {
		TCON_ERR("mmsys clock is not enable");
		return 0UL;
	}
	return clk_get_rate(hwtcon_device_info()->clock_info.pipeline0);
}

unsigned long hwtcon_driver_get_dpi_clock_rate(void)
{
	if (IS_ERR(hwtcon_device_info()->clock_info.dpi_tmp0)) {
		TCON_ERR("parse dpi clock fail");
		return 0UL;
	}

	if (!__clk_is_enabled(hwtcon_device_info()->clock_info.dpi_tmp0)) {
		TCON_ERR("dpi clock is not enable");
		return 0UL;
	}
	return clk_get_rate(hwtcon_device_info()->clock_info.dpi_tmp0);
}

int hwtcon_driver_enable_pipeline_clk(bool enable)
{
#ifndef FPGA_EARLY_PORTING
	if ((IS_ERR(g_hwtcon_device_info.clock_info.pipeline0)) ||
		(IS_ERR(g_hwtcon_device_info.clock_info.pipeline1)) ||
		(IS_ERR(g_hwtcon_device_info.clock_info.pipeline2)) ||
		(IS_ERR(g_hwtcon_device_info.clock_info.pipeline3)) ||
		(IS_ERR(g_hwtcon_device_info.clock_info.pipeline4)) ||
		(IS_ERR(g_hwtcon_device_info.clock_info.pipeline5)) ||
		(IS_ERR(g_hwtcon_device_info.clock_info.pipeline7))) {
		TCON_ERR("parse pipeline clk fail");
		return HWTCON_STATUS_PARSE_CLOCK_FAIL;
	}

	if (enable) {
		int status = 0;

		status |= clk_enable(
			g_hwtcon_device_info.clock_info.pipeline0);
		status |= clk_enable(
			g_hwtcon_device_info.clock_info.pipeline1);
		status |= clk_enable(
			g_hwtcon_device_info.clock_info.pipeline2);
		status |= clk_enable(
			g_hwtcon_device_info.clock_info.pipeline3);
		status |= clk_enable(
			g_hwtcon_device_info.clock_info.pipeline4);
		status |= clk_enable(
			g_hwtcon_device_info.clock_info.pipeline5);
		status |= clk_enable(
			g_hwtcon_device_info.clock_info.pipeline7);
		if (status != 0) {
			TCON_ERR("enable pipline clk fail");
			return HWTCON_STATUS_ENABLE_CLOCK_FAIL;
		}
	} else {
		clk_disable(
			g_hwtcon_device_info.clock_info.pipeline0);
		clk_disable(
			g_hwtcon_device_info.clock_info.pipeline1);
		clk_disable(
			g_hwtcon_device_info.clock_info.pipeline2);
		clk_disable(
			g_hwtcon_device_info.clock_info.pipeline3);
		clk_disable(
			g_hwtcon_device_info.clock_info.pipeline4);
		clk_disable(
			g_hwtcon_device_info.clock_info.pipeline5);
		clk_disable(
			g_hwtcon_device_info.clock_info.pipeline7);
	}
#endif

	return 0;
}

int hwtcon_driver_enable_dpi_clk(bool enable)
{
#ifndef FPGA_EARLY_PORTING
	if ((IS_ERR(g_hwtcon_device_info.clock_info.dpi_tmp0)) ||
		(IS_ERR(g_hwtcon_device_info.clock_info.dpi_tmp1))) {
		TCON_ERR("parse dpi clk fail");
		return HWTCON_STATUS_PARSE_CLOCK_FAIL;
	}
	if (enable) {
		int status = 0;

		status |= clk_enable(
			g_hwtcon_device_info.clock_info.dpi_tmp0);

		status |= clk_enable(
			g_hwtcon_device_info.clock_info.dpi_tmp1);
		if (status != 0) {
			TCON_ERR("enable dpi clk fail");
			return HWTCON_STATUS_ENABLE_CLOCK_FAIL;
		}
	} else {
		clk_disable(g_hwtcon_device_info.clock_info.dpi_tmp0);
		clk_disable(g_hwtcon_device_info.clock_info.dpi_tmp1);
	}
#endif
	return 0;
}

int hwtcon_driver_enable_smi_clk(bool enable)
{
	int status = 0;

	if (g_hwtcon_device_info.larb_dev == NULL) {
		TCON_ERR("get larb device fail");
		return HWTCON_STATUS_ENABLE_CLOCK_FAIL;
	}

	if (enable)
		status = mtk_smi_larb_get(g_hwtcon_device_info.larb_dev);
	else
		mtk_smi_larb_put(g_hwtcon_device_info.larb_dev);

	return status;
}

int hwtcon_driver_prepare_clk(void)
{
	int status = 0;

#ifndef FPGA_EARLY_PORTING
	if ((IS_ERR(g_hwtcon_device_info.clock_info.pipeline0)) ||
		(IS_ERR(g_hwtcon_device_info.clock_info.pipeline1)) ||
		(IS_ERR(g_hwtcon_device_info.clock_info.pipeline2)) ||
		(IS_ERR(g_hwtcon_device_info.clock_info.pipeline3)) ||
		(IS_ERR(g_hwtcon_device_info.clock_info.pipeline4)) ||
		(IS_ERR(g_hwtcon_device_info.clock_info.pipeline5)) ||
		(IS_ERR(g_hwtcon_device_info.clock_info.pipeline7)) ||
		(IS_ERR(g_hwtcon_device_info.clock_info.dpi_tmp0)) ||
		(IS_ERR(g_hwtcon_device_info.clock_info.dpi_tmp1))) {
		TCON_ERR("parse clk fail");
		return HWTCON_STATUS_PARSE_CLOCK_FAIL;
	}

	status |= clk_prepare(
				g_hwtcon_device_info.clock_info.pipeline0);
	status |= clk_prepare(
			g_hwtcon_device_info.clock_info.pipeline1);
	status |= clk_prepare(
			g_hwtcon_device_info.clock_info.pipeline2);
	status |= clk_prepare(
			g_hwtcon_device_info.clock_info.pipeline3);
	status |= clk_prepare(
			g_hwtcon_device_info.clock_info.pipeline4);
	status |= clk_prepare(
			g_hwtcon_device_info.clock_info.pipeline5);
	status |= clk_prepare(
			g_hwtcon_device_info.clock_info.pipeline7);

	status |= clk_prepare(
			g_hwtcon_device_info.clock_info.dpi_tmp0);
	status |= clk_set_rate(g_hwtcon_device_info.clock_info.dpi_tmp0,
		hw_tcon_get_edp_clk());

	status |= clk_prepare(
			g_hwtcon_device_info.clock_info.dpi_tmp1);

	if (status != 0)
		TCON_ERR("prepare clk fail");
#endif

	return status;
}

int hwtcon_driver_unprepare_clk(void)
{
#ifndef FPGA_EARLY_PORTING
	if ((IS_ERR(g_hwtcon_device_info.clock_info.pipeline0)) ||
		(IS_ERR(g_hwtcon_device_info.clock_info.pipeline1)) ||
		(IS_ERR(g_hwtcon_device_info.clock_info.pipeline2)) ||
		(IS_ERR(g_hwtcon_device_info.clock_info.pipeline3)) ||
		(IS_ERR(g_hwtcon_device_info.clock_info.pipeline4)) ||
		(IS_ERR(g_hwtcon_device_info.clock_info.pipeline5)) ||
		(IS_ERR(g_hwtcon_device_info.clock_info.pipeline7)) ||
		(IS_ERR(g_hwtcon_device_info.clock_info.dpi_tmp0)) ||
		(IS_ERR(g_hwtcon_device_info.clock_info.dpi_tmp1))) {
		TCON_ERR("parse clk fail");
		return HWTCON_STATUS_PARSE_CLOCK_FAIL;
	}

	clk_unprepare(
		g_hwtcon_device_info.clock_info.pipeline0);
	clk_unprepare(
		g_hwtcon_device_info.clock_info.pipeline1);
	clk_unprepare(
		g_hwtcon_device_info.clock_info.pipeline2);
	clk_unprepare(
		g_hwtcon_device_info.clock_info.pipeline3);
	clk_unprepare(
		g_hwtcon_device_info.clock_info.pipeline4);
	clk_unprepare(
		g_hwtcon_device_info.clock_info.pipeline5);
	clk_unprepare(
		g_hwtcon_device_info.clock_info.pipeline7);

	clk_unprepare(
		g_hwtcon_device_info.clock_info.dpi_tmp0);
	clk_unprepare(
		g_hwtcon_device_info.clock_info.dpi_tmp1);
#endif

	return 0;
}

/* control pipeline & dpi clock */
int hwtcon_driver_enable_clock(bool enable)
{
	unsigned long flags;

	if (enable) {
		/* enable HWTCON clock */
		spin_lock_irqsave(&hwtcon_fb_info()->hwtcon_clk_enable_lock,
			flags);
		if (!hwtcon_fb_info()->hwtcon_clk_enable) {
			hwtcon_fb_info()->hwtcon_clk_enable = true;
			hwtcon_driver_enable_pipeline_clk(true);
			hwtcon_driver_enable_dpi_clk(true);
		}
		spin_unlock_irqrestore(
			&hwtcon_fb_info()->hwtcon_clk_enable_lock,
			flags);
	} else {
		/* disable HWTCON clock */
		#ifndef HWTCON_CLK_ALWAYS_ON
		spin_lock_irqsave(&hwtcon_fb_info()->hwtcon_clk_enable_lock,
			flags);
		if (hwtcon_fb_info()->hwtcon_clk_enable) {
			hwtcon_fb_info()->hwtcon_clk_enable = false;
			hwtcon_driver_enable_pipeline_clk(false);
			hwtcon_driver_enable_dpi_clk(false);
		}
		spin_unlock_irqrestore(
			&hwtcon_fb_info()->hwtcon_clk_enable_lock,
			flags);
		#endif
	}
	return 0;
}

bool hwtcon_driver_pipeline_clk_is_enable(void)
{
#ifndef FPGA_EARLY_PORTING
	return hwtcon_driver_check_clk_on(
		g_hwtcon_device_info.clock_info.pipeline5);
#else
	return true;
#endif
}

static void hwtcon_driver_lock_wake_lock(bool lock)
{
    static bool is_locked;

    if (lock) {
        if (!is_locked) {
            __pm_stay_awake(&hwtcon_fb_info()->wake_lock);
            is_locked = true;
        } else  {
            /* should not reach here */
            TCON_ERR("try lock twice");
        }
    } else {
        if (is_locked) {
            __pm_relax(&hwtcon_fb_info()->wake_lock);
            is_locked = false;
        } else {
            /* should not reach here */
            TCON_ERR("try unlock twice");
        }
    }
}

void hwtcon_driver_force_enable_mmsys_domain(bool enable)
{
	hwtcon_driver_enable_smi_clk(enable);
}

/* enable MM domain power
 * prepare pipeline & dpi clock.
 * control smi clock.
 */
int hwtcon_driver_enable_mmsys_power(bool enable)
{
	int status = 0;

	if (enable) {
		mutex_lock(&hwtcon_fb_info()->mmsys_power_enable_lock);
		if (!hwtcon_fb_info()->mmsys_power_enable) {
			TCON_LOG("power on MMSYS domain");
			hwtcon_driver_lock_wake_lock(true);
			#ifndef FPGA_EARLY_PORTING
			/* request vcore voltage 0.7V */
			pm_qos_update_request(hwtcon_fb_info()->vcore_req, 2);
			hwtcon_driver_enable_smi_clk(true);
			status = hwtcon_driver_prepare_clk();
			/*enable clock */
			hwtcon_driver_enable_clock(true);
			#endif
			hwtcon_core_config_timing(NULL);
			#ifdef CMDQ_RELEASE_LUT
			hwtcon_driver_cmdq_lut_release_start_loop();
			#endif
			hwtcon_fb_info()->mmsys_power_enable = true;
			wake_up(&hwtcon_fb_info()->power_state_change_wait_queue);
		}
		mutex_unlock(&hwtcon_fb_info()->mmsys_power_enable_lock);
	} else {

		#ifndef HWTCON_CLK_ALWAYS_ON

		unsigned long flags;
		mutex_lock(&hwtcon_fb_info()->mmsys_power_enable_lock);
		/* double check hardware status */
		if (hwtcon_fb_info()->mmsys_power_enable && hwtcon_core_check_hwtcon_idle()) {
			TCON_LOG("power down MMSYS domain");
			#ifndef FPGA_EARLY_PORTING
			if (!hwtcon_debug_get_info()->fiti_power_always_on)
				hwtcon_core_fiti_power_enable(false);
			#endif
			TS_WF_LUT_disable_wf_lut();
			#ifndef FPGA_EARLY_PORTING
			hwtcon_edp_pinmux_inactive();
			hwtcon_driver_enable_clock(false);
			/* request vcore voltage 0.65V */
			pm_qos_update_request(hwtcon_fb_info()->vcore_req,
				PM_QOS_VCORE_OPP_DEFAULT_VALUE);
			status = hwtcon_driver_unprepare_clk();
			hwtcon_driver_enable_smi_clk(false);
			#endif
			//hwtcon_fb_info()->current_night_mode = -1;
			hwtcon_fb_info()->current_temp_zone = -1;
			spin_lock_irqsave(&hwtcon_fb_info()->g_update_order_lock, flags);
			if (hwtcon_fb_info()->g_update_cnt == 0)
				hwtcon_fb_info()->g_update_order = 0;
			spin_unlock_irqrestore(&hwtcon_fb_info()->g_update_order_lock, flags);
			#ifdef CMDQ_RELEASE_LUT
			hwtcon_driver_cmdq_lut_release_stop_loop();
			#endif
			hwtcon_fb_info()->mmsys_power_enable = false;
			wake_up(&hwtcon_fb_info()->power_state_change_wait_queue);
			hwtcon_driver_lock_wake_lock(false);
		} else {
			TCON_LOG("skip power down MMSYS domain[%d][%d]",
				hwtcon_fb_info()->mmsys_power_enable,
				hwtcon_core_check_hwtcon_idle());
			TCON_LOG("lut_active[0x%016llx 0x%016llx 0x%016llx] LUT reg[0x%08x 0x%08x] task count[%d] [%d] [%d] [%d] [%d]",
				hwtcon_fb_info()->lut_free,
				hwtcon_fb_info()->lut_active,
				hwtcon_core_get_released_lut(),
				pp_read(WF_LUT_EN_STA1_VA),
				pp_read(WF_LUT_EN_STA0_VA),
				hwtcon_core_get_task_count(&hwtcon_fb_info()->wait_for_mdp_task_list.list),
				hwtcon_core_get_task_count(&hwtcon_fb_info()->mdp_done_task_list.list),
				hwtcon_core_get_task_count(&hwtcon_fb_info()->pipeline_processing_task_list.list),
				hwtcon_core_get_task_count(&hwtcon_fb_info()->pipeline_done_task_list.list),
				hwtcon_core_get_task_count(&hwtcon_fb_info()->collision_task_list.list));
		}
		mutex_unlock(&hwtcon_fb_info()->mmsys_power_enable_lock);
		#endif
	}

	return status;
}

/* control log level */
bool hwtcon_get_log_level(void)
{
	return hwtcon_debug_get_info()->log_level;
}

bool hwtcon_get_epdc_debug_level(void)
{
	return hwtcon_debug_get_info()->epdc_debug;
}

/* PIPELINE write working buffer frame done */
irqreturn_t hwtcon_driver_handle_wb_frame_done(int irq,
	void *dev_id)
{
	complete(&hwtcon_fb_info()->wb_frame_done_completion);
	pipeline_config_clear_irq(NULL, IRQ_PIPELINE_WB_FRAME_DONE);

	return IRQ_HANDLED;
	/* return IRQ_WAKE_THREAD; */
}

u32 lut_frame_count[64];
bool lut_crc_all_zero[64];

irqreturn_t hwtcon_driver_handle_wf_lut_frame_done_thread(int irq,
	void *dev)
{
	/* HWTCON request vcore voltage: 0.7V
	 * MMSYS clock: 312 MHz
	 * DPI clock
	 */
	int deviation = 0;

	if (hwtcon_driver_get_dpi_clock_rate() > hw_tcon_get_edp_clk())
		deviation = hwtcon_driver_get_dpi_clock_rate() - hw_tcon_get_edp_clk();
	else
		deviation = hw_tcon_get_edp_clk() - hwtcon_driver_get_dpi_clock_rate();

	if ((regulator_get_voltage(hwtcon_fb_info()->regulator_vcore) != VCORE_VOLTAGE) ||
		(hwtcon_driver_get_mmsys_clock_rate() != MMSYS_CLK_RATE) ||
		(deviation >= hw_tcon_get_edp_clk() >> 10))
		TCON_ERR("invalid vcore:%d mmsys clock:%ld DPI clock:%lu golden %u",
			regulator_get_voltage(hwtcon_fb_info()->regulator_vcore),
			hwtcon_driver_get_mmsys_clock_rate(),
			hwtcon_driver_get_dpi_clock_rate(),
			hw_tcon_get_edp_clk());

	return IRQ_HANDLED;
}


/* wf_lut frame done irq handler */
irqreturn_t hwtcon_driver_handle_wf_lut_frame_done(
	int irq,
	void *dev)
{
	u32 wf_lut_irq_status = pp_read(WF_LUT_INTSTA_VA);
	int i = 0;

	/* WF_LUT_CRC_0_VA: lut 1 & LUT 0 input crc
	 * WF_LUT_CRC_1_VA: lut 3 & LUT 2 input crc
	 * ...
	 */
	TCON_LOG("DPI 0x%08x 0x%08x wb input: 0x%08x wf_lut output: 0x%08x",
		pp_read(WF_LUT_DPI_CHECKSUM_VA),
		pp_read(WF_LUT_DPI_STATUS_VA),
		pp_read(WF_LUT_CHKSUM_3_VA),
		pp_read(WF_LUT_CHKSUM_4_VA));

	for (i = 0; i < 63; i++) {
		u32 checksum = 0;

		if ((1LL << i) & hwtcon_fb_info()->lut_active) {
			lut_frame_count[i]++;

			pp_write(NULL, WF_LUT_CRC_ID_SEL_0, i);
			checksum = pp_read(WF_LUT_CRC_0_VA) & GENMASK(15, 0);
			if (checksum)
				lut_crc_all_zero[i] = false;
		}
	};

	if (wf_lut_irq_status & (BIT_MASK(2) | BIT_MASK(13) | GENMASK(8, 5))) {
		/* WF_LUT under flow */
		TCON_ERR("wf_lut under flow[0x%08x]!", wf_lut_irq_status);

		pp_write_mask(NULL, WF_LUT_INTSTA, 0x0, GENMASK(13, 2));
	}

	/* clear irq status. */
	wf_lut_clear_irq_status(NULL);
	return IRQ_WAKE_THREAD;
}

void hwtcon_driver_handle_released_lut_flag(u64 released_lut)
{
	u64 total_released_lut = 0LL;
	int i = 0;
	unsigned long flags;

	/* update active lut */
	spin_lock_irqsave(&hwtcon_fb_info()->lut_active_lock, flags);
	for (i = 0; i < MAX_LUT_REGION_COUNT; i++) {
		if (released_lut & (1LL << i))
			hwtcon_fb_info()->lut_active &= ~(1LL << i);
	}
	total_released_lut = ~(hwtcon_fb_info()->lut_active);
	spin_unlock_irqrestore(&hwtcon_fb_info()->lut_active_lock, flags);
	wake_up(&hwtcon_fb_info()->wf_lut_release_wait_queue);
	wake_up(&hwtcon_fb_info()->task_state_wait_queue);


	/* handle lut release */
	for (i = 0; i < MAX_LUT_REGION_COUNT; i++) {
		if (released_lut & (1LL << i))
			hwtcon_core_handle_release_lut(i);
	}

	hwtcon_core_update_collision_list_on_release_lut(total_released_lut);

	hwtcon_core_handle_clock_disable();

	return;
}

/* wf_lut LUT release irq handler */
irqreturn_t hwtcon_driver_handle_wf_lut_release(
	int irq,
	void *dev)
{
#ifndef CMDQ_RELEASE_LUT
	u64 released_lut = 0LL;
	u32 status_0 = 0;
	u32 status_1 = 0;

	status_0 = pp_read(WF_LUT_END_IRQ_STA0_VA);
	status_1 = pp_read(WF_LUT_END_IRQ_STA1_VA);

	released_lut = (u64)status_0;
	released_lut |= (u64)status_1 << 32;

	/* clear WF_LUT_END_IRQ_STA status */
	pp_write_mask(NULL, WF_LUT_END_IRQ_CLR0, status_0, 0xFFFFFFFF);
	pp_write_mask(NULL, WF_LUT_END_IRQ_CLR0, 0, 0xFFFFFFFF);
	pp_write_mask(NULL, WF_LUT_END_IRQ_CLR1, status_1, 0xFFFFFFFF);
	pp_write_mask(NULL, WF_LUT_END_IRQ_CLR1, 0, 0xFFFFFFFF);

	hwtcon_driver_handle_released_lut_flag(released_lut);

	TCON_LOG("release wf_lut 0x%016llx software[0x%016llx] hardware[0x%08x%08x]",
		released_lut,
		hwtcon_fb_info()->lut_active,
		pp_read(WF_LUT_EN_STA1_VA),
		pp_read(WF_LUT_EN_STA0_VA));
#endif
	/* clear irq status. */
	wf_lut_clear_lut_end_irq_status(NULL);

	return IRQ_HANDLED;
}

/* wf_lut LUT release all irq handler thread */
irqreturn_t hwtcon_driver_handle_tcon_end_thread(
	int irq,
	void *dev)
{
	static int count;

	TCON_ERR("IRQ debug %d", count++);

	do {
		u32 value0, value1;
		pp_write_mask(NULL, 0x1400700C, 0x2 << 20, GENMASK(23, 20));
		
		pp_write_mask(NULL, 0x1400D004, 5 << 16, GENMASK(19, 16));
		value0 = pp_read(hwtcon_device_info()->mmsys_reg_va + 0xD0C0);
		
		pp_write_mask(NULL, 0x1400D004, 1 << 16, GENMASK(19, 16));
		value1 = pp_read(hwtcon_device_info()->mmsys_reg_va + 0xD0C0);
		
		//TCON_LOG("data 0x%08x 0x%08x", value0, value1);
	} while (0);

	hwtcon_core_handle_clock_disable();

	/* clear irq status. */
	pipeline_config_clear_irq(NULL, IRQ_WF_LUT_TCON_END);

	return IRQ_HANDLED;
}

#if 0
/* wf_lut LUT release all irq handler thread */
irqreturn_t hwtcon_driver_handle_dpi_update_done_thread(
	int irq,
	void *dev)
{
	static int count;

	TCON_ERR("IRQ debug %d", count++);

	hwtcon_core_handle_clock_disable();

	/* clear irq status. */
	pipeline_config_clear_irq(NULL, IRQ_PIPELINE_DPI_UPDATE_DONE);

	return IRQ_HANDLED;
}
#endif


/* pixel lut collision irq handler */
irqreturn_t hwtcon_driver_handle_pixel_lut_collision(
	int irq,
	void *dev)
{
	/* clear irq status. */
	pipeline_config_clear_irq(NULL, IRQ_PIPELINE_PIXEL_LUT_COLLISION);

	WARN(1, "BUG Here Find a pixel lut collision");

	return IRQ_HANDLED;
}

static int hwtcon_driver_register_irq(struct platform_device *pdev)
{
	/* register wb_wdma irq */
	if (request_threaded_irq(
		g_hwtcon_device_info.irq_id[IRQ_PIPELINE_WB_FRAME_DONE],
		hwtcon_driver_handle_wb_frame_done,
		NULL,
		IRQF_TRIGGER_LOW | IRQF_ONESHOT,
		"hwtcon",
		&pdev->dev) != 0) {
		TCON_ERR("fail to register wb_wdma irq");
		return HWTCON_STATUS_REGISTER_IRQ_FAIL;
	} else
		TCON_LOG("register wb_wdma irq:%d success",
			g_hwtcon_device_info.irq_id[IRQ_PIPELINE_WB_FRAME_DONE]);

	#if 1
	/* register wf_lut frame done irq */
	if (request_threaded_irq(
		g_hwtcon_device_info.irq_id[IRQ_WF_LUT_FRAME_DONE],
		hwtcon_driver_handle_wf_lut_frame_done,
		hwtcon_driver_handle_wf_lut_frame_done_thread,
		IRQF_TRIGGER_LOW | IRQF_ONESHOT,
		"hwtcon",
		&pdev->dev) != 0) {
		TCON_ERR("fail to register wf_lut frame done irq");
		return HWTCON_STATUS_REGISTER_IRQ_FAIL;
	} else
		TCON_LOG("register wf_lut frame done irq:%d success",
			g_hwtcon_device_info.irq_id[IRQ_WF_LUT_FRAME_DONE]);
	#endif

#ifndef CMDQ_RELEASE_LUT
	/* register wf_lut LUT release irq */
	if (request_threaded_irq(
		g_hwtcon_device_info.irq_id[IRQ_WF_LUT_RELEASE],
		hwtcon_driver_handle_wf_lut_release,
		NULL,
		IRQF_TRIGGER_LOW | IRQF_ONESHOT,
		"hwtcon",
		&pdev->dev) != 0) {
		TCON_ERR("fail to register wf_lut LUT release irq");
		return HWTCON_STATUS_REGISTER_IRQ_FAIL;
	} else
		TCON_LOG("register wf_lut release irq:%d success",
			g_hwtcon_device_info.irq_id[IRQ_WF_LUT_RELEASE]);
#endif

#if 0
		/* register DPI update done irq */
		if (request_threaded_irq(
			g_hwtcon_device_info.irq_id[IRQ_PIPELINE_DPI_UPDATE_DONE],
			NULL,
			hwtcon_driver_handle_dpi_update_done_thread,
			IRQF_TRIGGER_LOW | IRQF_ONESHOT,
			"hwtcon",
			&pdev->dev) != 0) {
			TCON_ERR("fail to register DPI update done irq");
			return HWTCON_STATUS_REGISTER_IRQ_FAIL;
		} else
			TCON_LOG("register wf_lut release all irq:%d success",
				g_hwtcon_device_info.irq_id[IRQ_PIPELINE_DPI_UPDATE_DONE]);
#endif

#if 0
		/* register TCON end irq */
		if (request_threaded_irq(
			g_hwtcon_device_info.irq_id[IRQ_WF_LUT_TCON_END],
			hwtcon_driver_handle_tcon_end_thread,
			NULL,
			IRQF_TRIGGER_LOW | IRQF_ONESHOT,
			"hwtcon",
			&pdev->dev) != 0) {
			TCON_ERR("fail to register TCON end irq");
			return HWTCON_STATUS_REGISTER_IRQ_FAIL;
		} else
			TCON_LOG("register IRQ_WF_LUT_TCON_END irq:%d success",
				g_hwtcon_device_info.irq_id[IRQ_WF_LUT_TCON_END]);
#endif


	/* register pixel lut collision irq */
	if (request_threaded_irq(
		g_hwtcon_device_info.irq_id[IRQ_PIPELINE_PIXEL_LUT_COLLISION],
		hwtcon_driver_handle_pixel_lut_collision,
		NULL,
		IRQF_TRIGGER_LOW | IRQF_ONESHOT,
		"hwtcon",
		&pdev->dev) != 0) {
		TCON_ERR("fail to register pixel lut collision irq");
		return HWTCON_STATUS_REGISTER_IRQ_FAIL;
	} else
		TCON_LOG("register pixel lut collision irq:%d success",
			g_hwtcon_device_info.irq_id[IRQ_PIPELINE_PIXEL_LUT_COLLISION]);

	return 0;
}

static int hwtcon_driver_unregister_irq(struct platform_device *pdev)
{
	free_irq(g_hwtcon_device_info.irq_id[IRQ_PIPELINE_WB_FRAME_DONE], &pdev->dev);
	free_irq(g_hwtcon_device_info.irq_id[IRQ_WF_LUT_FRAME_DONE], &pdev->dev);
	#ifndef CMDQ_RELEASE_LUT
	free_irq(g_hwtcon_device_info.irq_id[IRQ_WF_LUT_RELEASE], &pdev->dev);
	#endif
	#if 0
	free_irq(g_hwtcon_device_info.irq_id[IRQ_PIPELINE_DPI_UPDATE_DONE], &pdev->dev);
	free_irq(g_hwtcon_device_info.irq_id[IRQ_WF_LUT_TCON_END], &pdev->dev);
	#endif
	free_irq(g_hwtcon_device_info.irq_id[IRQ_PIPELINE_PIXEL_LUT_COLLISION], &pdev->dev);

	return 0;
}

s32 hwtcon_driver_cmdq_timeout_dump(u64 engineFlag, int level)
{
	TCON_ERR("dump reg begin");
	/*
	 * 0x14004000 = 0x3E: all frame show done.
	 */
	TCON_ERR("14004000: 0x%08x",
		pp_read(g_hwtcon_device_info.mmsys_reg_va + 0x0000));
	TCON_ERR("regal status:0x%08x func:0x%x",
		pp_read(PAPER_TCTOP_REGAL_CFG_VA),
		pp_read(g_hwtcon_device_info.mmsys_reg_va + 0xC000));
	TCON_ERR("DPI enable:%d irq status: 0x%08x clock[%d] power[%d]",
		pp_read(g_hwtcon_device_info.mmsys_reg_va + 0x6000),
		pp_read(PAPER_TCTOP_UPD_CFG3_VA),
		hwtcon_driver_check_clk_on(hwtcon_device_info()->clock_info.pipeline0),
		hwtcon_fb_info()->mmsys_power_enable);
	TCON_ERR("WDMA Event:%d", cmdqCoreGetEvent(CMDQ_EVENT_WB_WDMA_DONE));
	hwtcon_core_reset_mmsys();
	TCON_ERR("dump reg end");
	return 0;
}

s32 hwtcon_driver_lut_release_callback(unsigned long data)
{
	int i = 0;
	u32 value[LUT_RELEASE_MAX] = {0};
	u64 released_lut = 0LL;

	for(i = 0; i < LUT_RELEASE_MAX; i++) {
		cmdqBackupReadSlot(hwtcon_fb_info()->lut_release_slot, i, &value[i]);
		if (value[i] && i < 64) {
			released_lut |= 1LL << i;
			cmdqBackupWriteSlot(hwtcon_fb_info()->lut_release_slot, i, 0);
		}
	}
	if (released_lut != 0) {
		TCON_LOG("release wf_lut[0x%016llx] total time:%d unit",
			released_lut,
			hwtcon_hal_get_gpt_time_in_unit(
				value[LUT_RELEASE_TIME_START],
				value[LUT_RELEASE_TIME_END]));
		
		hwtcon_driver_handle_released_lut_flag(released_lut);
	}

	return 0;
}

static struct cmdqRecStruct *g_lut_release_handle;
void hwtcon_driver_cmdq_lut_release_start_loop(void)
{
	CMDQ_VARIABLE reg0 = 0;
	CMDQ_VARIABLE reg1 = 0;
	CMDQ_VARIABLE index = 0;
	CMDQ_VARIABLE result = 0;
	CMDQ_VARIABLE bit_mask = 0;
	CMDQ_VARIABLE dram_pa = 0;

	if (g_lut_release_handle) {
		TCON_ERR("lut_release_loop already started");
		return;
	}
	TCON_LOG("start trigger loop");

	cmdqRecCreate(CMDQ_SCENARIO_HWTCON_AUTO_COLLISION_LOOP,
		&g_lut_release_handle);
	cmdqRecReset(g_lut_release_handle);
#if 0
	cmdqRecClearEventToken(g_lut_release_handle, CMDQ_EVENT_LUT_FRAME_DONE);
	cmdqRecWait(g_lut_release_handle, CMDQ_EVENT_LUT_FRAME_DONE);
#else
	cmdqRecClearEventToken(g_lut_release_handle, CMDQ_EVENT_DPI0_FRAME_DONE);
	cmdqRecWait(g_lut_release_handle, CMDQ_EVENT_DPI0_FRAME_DONE);
#endif
	cmdqRecBackupRegisterToSlot(g_lut_release_handle, hwtcon_fb_info()->lut_release_slot, LUT_RELEASE_TIME_START, 0x10008068);

	/* reg0 */
	cmdq_op_read_reg(g_lut_release_handle, WF_LUT_END_IRQ_STA0, &reg0, 0xFFFFFFFF);
	cmdq_op_assign(g_lut_release_handle, &index, 0);
	cmdq_op_assign(g_lut_release_handle, &bit_mask, 0);
	cmdq_op_assign(g_lut_release_handle, &dram_pa, hwtcon_fb_info()->lut_release_slot);

	cmdq_op_while(g_lut_release_handle, index, CMDQ_LESS_THAN, 32);
		cmdq_op_assign(g_lut_release_handle, &bit_mask, 1);
		cmdq_op_left_shift(g_lut_release_handle, &bit_mask, bit_mask, index);
		cmdq_op_and(g_lut_release_handle, &result, reg0, bit_mask);
		cmdq_op_if(g_lut_release_handle, result, CMDQ_NOT_EQUAL, 0);
			/* write value to slot slot + index * 4 */
			cmdq_op_left_shift(g_lut_release_handle, &result, index, 2);
			cmdq_op_add(g_lut_release_handle, &result, dram_pa, result);
			cmdq_op_write_mem_with_cpr(g_lut_release_handle, result, 1);
		cmdq_op_end_if(g_lut_release_handle);

		cmdq_op_add(g_lut_release_handle, &index, index, 1);
	cmdq_op_end_while(g_lut_release_handle);

	
	/* reg1 */
	cmdq_op_read_reg(g_lut_release_handle, WF_LUT_END_IRQ_STA1, &reg1, 0xFFFFFFFF);
	cmdq_op_assign(g_lut_release_handle, &index, 0);
	cmdq_op_assign(g_lut_release_handle, &bit_mask, 0);
	cmdq_op_assign(g_lut_release_handle, &dram_pa, hwtcon_fb_info()->lut_release_slot);

	cmdq_op_while(g_lut_release_handle, index, CMDQ_LESS_THAN, 31);
		cmdq_op_assign(g_lut_release_handle, &bit_mask, 1);
		cmdq_op_left_shift(g_lut_release_handle, &bit_mask, bit_mask, index);
		cmdq_op_and(g_lut_release_handle, &result, reg1, bit_mask);
		cmdq_op_if(g_lut_release_handle, result, CMDQ_NOT_EQUAL, 0);
			/* write value to slot slot + (index + 32) * 4 */
			cmdq_op_add(g_lut_release_handle, &result, index, 32);
			cmdq_op_left_shift(g_lut_release_handle, &result, result, 2);
			cmdq_op_add(g_lut_release_handle, &result, dram_pa, result);
			cmdq_op_write_mem_with_cpr(g_lut_release_handle, result, 1);
		cmdq_op_end_if(g_lut_release_handle);

		cmdq_op_add(g_lut_release_handle, &index, index, 1);
	cmdq_op_end_while(g_lut_release_handle);

#if 1
	/* clear WF_LUT_END_IRQ_STA0 & WF_LUT_END_IRQ_STA1 */
	cmdq_op_write_reg(g_lut_release_handle, WF_LUT_END_IRQ_CLR0, reg0, 0xFFFFFFFF);
	cmdq_op_write_reg(g_lut_release_handle, WF_LUT_END_IRQ_CLR0, 0, 0xFFFFFFFF);
	cmdq_op_write_reg(g_lut_release_handle, WF_LUT_END_IRQ_CLR1, reg1, 0xFFFFFFFF);
	cmdq_op_write_reg(g_lut_release_handle, WF_LUT_END_IRQ_CLR1, 0, 0xFFFFFFFF);
#endif

	cmdqRecBackupRegisterToSlot(g_lut_release_handle, hwtcon_fb_info()->lut_release_slot, LUT_RELEASE_TIME_END, 0x10008068);
	cmdqRecStartLoopWithCallback(g_lut_release_handle, hwtcon_driver_lut_release_callback, 0);
}

void hwtcon_driver_cmdq_lut_release_stop_loop(void)
{
	if (g_lut_release_handle == NULL) {
		TCON_ERR("lut_release_loop already stoped");
		return;
	}
	TCON_LOG("stop trigger loop");

	cmdqRecStopLoop(g_lut_release_handle);
	cmdqRecDestroy(g_lut_release_handle);
	g_lut_release_handle = NULL;
}

static int hwtcon_suspend(struct device *pDevice)
{
	/* disable VDD */
	TCON_LOG("enter hwtcon suspend");
	edp_vdd_disable();
	return 0;
}

static int hwtcon_resume(struct device *pDevice)
{
	/* enable VDD */
	TCON_LOG("enter hwtcon resume");
	edp_vdd_enable();
	return 0;
}


static int hwtcon_probe(struct platform_device *pdev)
{
	int status = 0;

	TCON_ERR("enter hwtcon probe");

	status = hwtcon_driver_init_device_info(pdev);
	if (status != 0)
		return status;

	status = hwtcon_fb_register_fb(pdev);
	if (status != 0)
		return status;

	pm_runtime_enable(&pdev->dev);

	hwtcon_edp_pinmux_control(pdev);

	status = hwtcon_driver_register_irq(pdev);
	if (status != 0)
		return status;

#if IS_MODULE(CONFIG_MTK_HWTCON)
	/* load waveform while init the driver as module */
	hwtcon_core_load_init_setting_from_file();
#endif

	return status;
}

static int hwtcon_remove(struct platform_device *pdev)
{
	TCON_LOG("enter hwtcon remove");
	hwtcon_core_wait_power_down();
	hwtcon_driver_unregister_irq(pdev);
	hwtcon_driver_destroy_device_info(pdev);
	hwtcon_fb_unregister_fb(pdev);
	hwtcon_edp_pinmux_release();
	pm_runtime_disable(&pdev->dev);
	return 0;
}

static const struct of_device_id hwtcon_of_ids[] = {
	{.compatible = "mediatek,hwtcon_v2",},
	{}
};

static const struct dev_pm_ops hwtcon_pm_ops = {
    .suspend = hwtcon_suspend,
    .resume = hwtcon_resume,
    .freeze = NULL,
    .thaw = NULL,
    .poweroff = NULL,
    .restore = NULL,
    .restore_noirq = NULL,
};


static struct platform_driver hwtcon_driver = {
	.probe = hwtcon_probe,
	.remove = hwtcon_remove,
	.driver = {
		.name = HWTCON_DRIVER_NAME,
		.of_match_table = hwtcon_of_ids,
		.pm = &hwtcon_pm_ops,
	}
};

static int __init hwtcon_init(void)
{
	int status = 0;
	enum HW_VERSION_ENUM hw_version = get_devinfo_with_index(65);

	TCON_LOG("enter %s", __func__);

	if (hw_version != HW_VERSION_MT8113) {
		TCON_ERR("try to insmod MT8113 hwtcon driver on other IC:0x%08x",
			hw_version);
		return -ENODEV;
	}

	status = platform_driver_register(&hwtcon_driver);
	if (status != 0) {
		TCON_ERR("register hwtcon platform driver fail:%d", status);
		return status;
	}

	/* create debug proc node */
	status = hwtcon_debug_create_procfs();
	if (status != 0) {
		TCON_ERR("create procfs fail:%d", status);
		return status;
	}

	/* register cmdq callback function */
	cmdqCoreRegisterCB(CMDQ_GROUP_HWTCON,
		NULL, hwtcon_driver_cmdq_timeout_dump,
		NULL, NULL);

	return status;
}

static void __exit hwtcon_exit(void)
{
	TCON_LOG("leave hwcon_exit");
	/* destroy debug proc node */
	hwtcon_debug_destroy_procfs();
	platform_driver_unregister(&hwtcon_driver);
}

static char waveform_path[NAME_MAX] = "/data/init_bin/wf_lut.gz";
char *hwtcon_driver_get_wf_file_path(void)
{
	return waveform_path;
}

void hwtcon_driver_set_wf_file_path(char *file_path)
{
	int copy_length = strlen(file_path) + 1;

	if (copy_length > sizeof(waveform_path)) {
		TCON_ERR("file path too long:%d", copy_length);
		return;
	}
	memcpy(waveform_path, file_path, copy_length);
}

module_param_string(waveform_path, waveform_path, sizeof(waveform_path), 0444);
module_param_cb(enable_debug_img_buffer, &param_ops_bool_enable_only, &g_hwtcon_device_info.param_enable_debug_img_buffer, 0644);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Hardware TCON");
late_initcall_sync(hwtcon_init);
module_exit(hwtcon_exit);
