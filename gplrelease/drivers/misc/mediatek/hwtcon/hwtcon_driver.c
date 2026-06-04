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
#include "hwtcon_paper_top_config.h"
#include "hwtcon_dpi_config.h"
#include "include/mt-plat/mtk_devinfo.h"

#include "mediatek/smi.h"

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

struct hwtcon_hw_info {
	char *va;
	u32 pa;
	u32 irq_id[10];
};

struct hwtcon_clock_info {
	struct clk *pipeline0;
	struct clk *pipeline1;
	struct clk *pipeline2;
	struct clk *pipeline3;
	struct clk *pipeline4;
	struct clk *pipeline5;
	struct clk *pipeline7;
	struct clk *dpi_tmp0;
	struct clk *dpi_tmp1;
};

struct hwtcon_device_info {
	struct device *dev;
	struct device *larb_dev;
	struct hwtcon_hw_info img_rdma;
	struct hwtcon_hw_info wb_rdma;
	struct hwtcon_hw_info wb_wdma;
	struct hwtcon_hw_info pipeline;
	struct hwtcon_hw_info regal;
	struct hwtcon_hw_info paper_top;
	struct hwtcon_hw_info wf_lut_rdma;
	struct hwtcon_hw_info wf_lut_dpi;
	struct hwtcon_hw_info tcon;
	struct hwtcon_hw_info mmsys;
	struct hwtcon_hw_info wf_lut;
	struct hwtcon_clock_info clock_info;
};

static struct hwtcon_device_info g_hwtcon_device_info;

char *hwtcon_driver_get_img_rdma_va(void)
{
	return g_hwtcon_device_info.img_rdma.va;
}

u32 hwtcon_driver_get_img_rdma_pa(void)
{
	return g_hwtcon_device_info.img_rdma.pa;
}

char *hwtcon_driver_get_wb_rdma_va(void)
{
	return g_hwtcon_device_info.wb_rdma.va;
}

u32 hwtcon_driver_get_wb_rdma_pa(void)
{
	return g_hwtcon_device_info.wb_rdma.pa;
}

char *hwtcon_driver_get_wb_wdma_va(void)
{
	return g_hwtcon_device_info.wb_wdma.va;
}

u32 hwtcon_driver_get_wb_wdma_pa(void)
{
	return g_hwtcon_device_info.wb_wdma.pa;
}

u32 hwtcon_driver_get_wb_wdma_irq_id(void)
{
	return g_hwtcon_device_info.wb_wdma.irq_id[0];
}

char *hwtcon_driver_get_pipeline_va(void)
{
	return g_hwtcon_device_info.pipeline.va;
}

u32 hwtcon_driver_get_pipeline_pa(void)
{
	return g_hwtcon_device_info.pipeline.pa;
}

u32 hwtcon_driver_get_pipeline_irq_id(void)
{
	return g_hwtcon_device_info.pipeline.irq_id[0];
}


char *hwtcon_driver_get_regal_va(void)
{
	return g_hwtcon_device_info.regal.va;
}

u32 hwtcon_driver_get_regal_pa(void)
{
	return g_hwtcon_device_info.regal.pa;
}

char *hwtcon_driver_get_paper_top_va(void)
{
	return g_hwtcon_device_info.paper_top.va;
}

u32 hwtcon_driver_get_paper_top_pa(void)
{
	return g_hwtcon_device_info.paper_top.pa;
}

char *hwtcon_driver_get_wf_lut_rdma_va(void)
{
	return g_hwtcon_device_info.wf_lut_rdma.va;
}

u32 hwtcon_driver_get_wf_lut_rdma_pa(void)
{
	return g_hwtcon_device_info.wf_lut_rdma.pa;
}

char *hwtcon_driver_get_wf_lut_dpi_va(void)
{
	return g_hwtcon_device_info.wf_lut_dpi.va;
}

u32 hwtcon_driver_get_wf_lut_dpi_pa(void)
{
	return g_hwtcon_device_info.wf_lut_dpi.pa;
}

u32 hwtcon_driver_get_wf_lut_dpi_irq_id(void)
{
	return g_hwtcon_device_info.wf_lut_dpi.irq_id[0];
}


char *hwtcon_driver_get_tcon_va(void)
{
	return g_hwtcon_device_info.tcon.va;
}

u32 hwtcon_driver_get_tcon_pa(void)
{
	return g_hwtcon_device_info.tcon.pa;
}

u32 hwtcon_driver_get_tcon_irq_id(void)
{
	return g_hwtcon_device_info.tcon.irq_id[0];
}


char *hwtcon_driver_get_mmsys_va(void)
{
	return g_hwtcon_device_info.mmsys.va;
}

u32 hwtcon_driver_get_mmsys_pa(void)
{
	return g_hwtcon_device_info.mmsys.pa;
}

char *hwtcon_driver_get_wf_lut_va(void)
{
	return g_hwtcon_device_info.wf_lut.va;
}

u32 hwtcon_driver_get_wf_lut_pa(void)
{
	return g_hwtcon_device_info.wf_lut.pa;
}

u32 hwtcon_driver_get_wf_lut_irq_id(void)
{
	return g_hwtcon_device_info.wf_lut.irq_id[0];
}

u32 hwtcon_driver_get_wf_lut_end_irq_id(void)
{
	return g_hwtcon_device_info.wf_lut.irq_id[1];
}

u32 hwtcon_driver_get_disp_rdma_irq_id(void)
{
	return g_hwtcon_device_info.wf_lut.irq_id[2];
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
	g_hwtcon_device_info.dev = &pdev->dev;
	g_hwtcon_device_info.larb_dev = hwtcon_driver_get_smi_device(pdev);

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

	/* img_rdma */
	status = hwtcon_driver_parse_pa(node, 0,
		&g_hwtcon_device_info.img_rdma.pa);
	if (status != 0) {
		TCON_ERR("parse img_rdma pa fail");
		return status;
	}
	g_hwtcon_device_info.img_rdma.va = (char *)of_iomap(node, 0);
	if (g_hwtcon_device_info.img_rdma.va == NULL) {
		TCON_ERR("img_rdma of_iomap fail");
		return HWTCON_STATUS_OF_IOMAP_FAIL;
	}

	/* wb_rdma */
	status = hwtcon_driver_parse_pa(node, 1,
		&g_hwtcon_device_info.wb_rdma.pa);
	if (status != 0) {
		TCON_ERR("parse wb_rdma pa fail");
		return status;
	}
	g_hwtcon_device_info.wb_rdma.va = (char *)of_iomap(node, 1);
	if (g_hwtcon_device_info.wb_rdma.va == NULL) {
		TCON_ERR("wb_rdma of_iomap fail");
		return HWTCON_STATUS_OF_IOMAP_FAIL;
	}

	/* wb_wdma */
	status = hwtcon_driver_parse_pa(node, 2,
		&g_hwtcon_device_info.wb_wdma.pa);
	if (status != 0) {
		TCON_ERR("parse wb_wdma pa fail");
		return status;
	}
	g_hwtcon_device_info.wb_wdma.va = (char *)of_iomap(node, 2);
	if (g_hwtcon_device_info.wb_wdma.va == NULL) {
		TCON_ERR("wb_wdma of_iomap fail");
		return HWTCON_STATUS_OF_IOMAP_FAIL;
	}
	g_hwtcon_device_info.wb_wdma.irq_id[0] = platform_get_irq(pdev, 0);
	if (g_hwtcon_device_info.wb_wdma.irq_id[0] < 0) {
		TCON_ERR("wb_wdma get irq fail");
		return HWTCON_STATUS_GET_IRQ_ID_FAIL;
	}

	/* pipeline */
	status = hwtcon_driver_parse_pa(node, 3,
		&g_hwtcon_device_info.pipeline.pa);
	if (status != 0) {
		TCON_ERR("parse pipeline pa fail");
		return status;
	}
	g_hwtcon_device_info.pipeline.va = (char *)of_iomap(node, 3);
	if (g_hwtcon_device_info.pipeline.va == NULL) {
		TCON_ERR("pipeline of_iomap fail");
		return HWTCON_STATUS_OF_IOMAP_FAIL;
	}

	g_hwtcon_device_info.pipeline.irq_id[0] = platform_get_irq(pdev, 6);
	if (g_hwtcon_device_info.pipeline.irq_id[0] < 0) {
		TCON_ERR("pipeline get irq fail");
		return HWTCON_STATUS_GET_IRQ_ID_FAIL;
	}

	/* regal */
	status = hwtcon_driver_parse_pa(node, 4,
		&g_hwtcon_device_info.regal.pa);
	if (status != 0) {
		TCON_ERR("parse regal pa fail");
		return status;
	}
	g_hwtcon_device_info.regal.va = (char *)of_iomap(node, 4);
	if (g_hwtcon_device_info.regal.va == NULL) {
		TCON_ERR("regal of_iomap fail");
		return HWTCON_STATUS_OF_IOMAP_FAIL;
	}

	/* paper_top */
	status = hwtcon_driver_parse_pa(node, 5,
		&g_hwtcon_device_info.paper_top.pa);
	if (status != 0) {
		TCON_ERR("parse paper_top pa fail");
		return status;
	}
	g_hwtcon_device_info.paper_top.va = (char *)of_iomap(node, 5);
	if (g_hwtcon_device_info.paper_top.va == NULL) {
		TCON_ERR("paper_top of_iomap fail");
		return HWTCON_STATUS_OF_IOMAP_FAIL;
	}

	/* wf_lut_rdma */
	status = hwtcon_driver_parse_pa(node, 6,
		&g_hwtcon_device_info.wf_lut_rdma.pa);
	if (status != 0) {
		TCON_ERR("parse wf_lut_rdma pa fail");
		return status;
	}
	g_hwtcon_device_info.wf_lut_rdma.va = (char *)of_iomap(node, 6);
	if (g_hwtcon_device_info.wf_lut_rdma.va == NULL) {
		TCON_ERR("paper_top of_iomap fail");
		return HWTCON_STATUS_OF_IOMAP_FAIL;
	}

	/* wf_lut_dpi */
	status = hwtcon_driver_parse_pa(node, 7,
		&g_hwtcon_device_info.wf_lut_dpi.pa);
	if (status != 0) {
		TCON_ERR("parse wf_lut_dpi pa fail");
		return status;
	}
	g_hwtcon_device_info.wf_lut_dpi.va = (char *)of_iomap(node, 7);
	if (g_hwtcon_device_info.wf_lut_dpi.va == NULL) {
		TCON_ERR("paper_top of_iomap fail");
		return HWTCON_STATUS_OF_IOMAP_FAIL;
	}

	g_hwtcon_device_info.wf_lut_dpi.irq_id[0] = platform_get_irq(pdev, 1);
	if (g_hwtcon_device_info.wf_lut_dpi.irq_id[0] < 0) {
		TCON_ERR("wf_lut_dpi get irq fail");
		return HWTCON_STATUS_GET_IRQ_ID_FAIL;
	}

	/* tcon */
	status = hwtcon_driver_parse_pa(node, 8,
		&g_hwtcon_device_info.tcon.pa);
	if (status != 0) {
		TCON_ERR("parse tcon pa fail");
		return status;
	}
	g_hwtcon_device_info.tcon.va = (char *)of_iomap(node, 8);
	if (g_hwtcon_device_info.tcon.va == NULL) {
		TCON_ERR("paper_top of_iomap fail");
		return HWTCON_STATUS_OF_IOMAP_FAIL;
	}

	g_hwtcon_device_info.tcon.irq_id[0] = platform_get_irq(pdev, 2);
	if (g_hwtcon_device_info.tcon.irq_id[0] < 0) {
		TCON_ERR("tcon get irq fail");
		return HWTCON_STATUS_GET_IRQ_ID_FAIL;
	}


	/* mmsys */
	status = hwtcon_driver_parse_pa(node, 9,
		&g_hwtcon_device_info.mmsys.pa);
	if (status != 0) {
		TCON_ERR("parse mmsys pa fail");
		return status;
	}
	g_hwtcon_device_info.mmsys.va = (char *)of_iomap(node, 9);
	if (g_hwtcon_device_info.mmsys.va == NULL) {
		TCON_ERR("paper_top of_iomap fail");
		return HWTCON_STATUS_OF_IOMAP_FAIL;
	}

	/* wf_lut top */
	status = hwtcon_driver_parse_pa(node, 10,
		&g_hwtcon_device_info.wf_lut.pa);
	if (status != 0) {
		TCON_ERR("parse wf_lut pa fail");
		return status;
	}
	g_hwtcon_device_info.wf_lut.va = (char *)of_iomap(node, 10);
	if (g_hwtcon_device_info.wf_lut.va == NULL) {
		TCON_ERR("paper_top of_iomap fail");
		return HWTCON_STATUS_OF_IOMAP_FAIL;
	}

	g_hwtcon_device_info.wf_lut.irq_id[1] = platform_get_irq(pdev, 3);
	if (g_hwtcon_device_info.wf_lut.irq_id[1] < 0) {
		TCON_ERR("wf_lut get lut end irq fail");
		return HWTCON_STATUS_GET_IRQ_ID_FAIL;
	}

	g_hwtcon_device_info.wf_lut.irq_id[0] = platform_get_irq(pdev, 4);
	if (g_hwtcon_device_info.wf_lut.irq_id[0] < 0) {
		TCON_ERR("wf_lut get irq fail");
		return HWTCON_STATUS_GET_IRQ_ID_FAIL;
	}

	g_hwtcon_device_info.wf_lut.irq_id[2] = platform_get_irq(pdev, 5);
	if (g_hwtcon_device_info.wf_lut.irq_id[2] < 0) {
		TCON_ERR("wf_lut get irq fail");
		return HWTCON_STATUS_GET_IRQ_ID_FAIL;
	}

	TCON_LOG("img_rdma: pa[0x%x] va[%p]",
		g_hwtcon_device_info.img_rdma.pa,
		g_hwtcon_device_info.img_rdma.va);
	TCON_LOG("wb_rdma: pa[0x%x] va[%p]",
		g_hwtcon_device_info.wb_rdma.pa,
		g_hwtcon_device_info.wb_rdma.va);
	TCON_LOG("wb_wdma: pa[0x%x] va[%p]",
		g_hwtcon_device_info.wb_wdma.pa,
		g_hwtcon_device_info.wb_wdma.va);
	TCON_LOG("pipeline: pa[0x%x] va[%p]",
		g_hwtcon_device_info.pipeline.pa,
		g_hwtcon_device_info.pipeline.va);
	TCON_LOG("regal: pa[0x%x] va[%p]",
		g_hwtcon_device_info.regal.pa,
		g_hwtcon_device_info.regal.va);
	TCON_LOG("paper_top: pa[0x%x] va[%p]",
		g_hwtcon_device_info.paper_top.pa,
		g_hwtcon_device_info.paper_top.va);
	TCON_LOG("wf_lut_rdma: pa[0x%x] va[%p]",
		g_hwtcon_device_info.wf_lut_rdma.pa,
		g_hwtcon_device_info.wf_lut_rdma.va);
	TCON_LOG("wf_lut_dpi: pa[0x%x] va[%p]",
		g_hwtcon_device_info.wf_lut_dpi.pa,
		g_hwtcon_device_info.wf_lut_dpi.va);
	TCON_LOG("tcon: pa[0x%x] va[%p]",
		g_hwtcon_device_info.tcon.pa,
		g_hwtcon_device_info.tcon.va);
	TCON_LOG("mmsys: pa[0x%x] va[%p]",
		g_hwtcon_device_info.mmsys.pa,
		g_hwtcon_device_info.mmsys.va);
	TCON_LOG("wf_lut: pa[0x%x] va[%p]",
		g_hwtcon_device_info.wf_lut.pa,
		g_hwtcon_device_info.wf_lut.va);

	hwtcon_debug_err_printf("img_rdma: pa[0x%x] va[%p]\n",
		g_hwtcon_device_info.img_rdma.pa,
		g_hwtcon_device_info.img_rdma.va);
	hwtcon_debug_err_printf("wb_rdma: pa[0x%x] va[%p]\n",
		g_hwtcon_device_info.wb_rdma.pa,
		g_hwtcon_device_info.wb_rdma.va);
	hwtcon_debug_err_printf("wb_wdma: pa[0x%x] va[%p]\n",
		g_hwtcon_device_info.wb_wdma.pa,
		g_hwtcon_device_info.wb_wdma.va);
	hwtcon_debug_err_printf("pipeline: pa[0x%x] va[%p]\n",
		g_hwtcon_device_info.pipeline.pa,
		g_hwtcon_device_info.pipeline.va);
	hwtcon_debug_err_printf("regal: pa[0x%x] va[%p]\n",
		g_hwtcon_device_info.regal.pa,
		g_hwtcon_device_info.regal.va);
	hwtcon_debug_err_printf("paper_top: pa[0x%x] va[%p]\n",
		g_hwtcon_device_info.paper_top.pa,
		g_hwtcon_device_info.paper_top.va);
	hwtcon_debug_err_printf("wf_lut_rdma: pa[0x%x] va[%p]\n",
		g_hwtcon_device_info.wf_lut_rdma.pa,
		g_hwtcon_device_info.wf_lut_rdma.va);
	hwtcon_debug_err_printf("wf_lut_dpi: pa[0x%x] va[%p]\n",
		g_hwtcon_device_info.wf_lut_dpi.pa,
		g_hwtcon_device_info.wf_lut_dpi.va);
	hwtcon_debug_err_printf("tcon: pa[0x%x] va[%p]\n",
		g_hwtcon_device_info.tcon.pa,
		g_hwtcon_device_info.tcon.va);
	hwtcon_debug_err_printf("mmsys: pa[0x%x] va[%p]\n",
		g_hwtcon_device_info.mmsys.pa,
		g_hwtcon_device_info.mmsys.va);
	hwtcon_debug_err_printf("wf_lut: pa[0x%x] va[%p]\n",
		g_hwtcon_device_info.wf_lut.pa,
		g_hwtcon_device_info.wf_lut.va);

	return 0;
}


static int hwtcon_driver_destroy_device_info(struct platform_device *pdev)
{
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

	if (g_hwtcon_device_info.img_rdma.va)
		iounmap(g_hwtcon_device_info.img_rdma.va);
	if (g_hwtcon_device_info.wb_rdma.va)
		iounmap(g_hwtcon_device_info.wb_rdma.va);
	if (g_hwtcon_device_info.wb_wdma.va)
		iounmap(g_hwtcon_device_info.wb_wdma.va);
	if (g_hwtcon_device_info.pipeline.va)
		iounmap(g_hwtcon_device_info.pipeline.va);
	if (g_hwtcon_device_info.regal.va)
		iounmap(g_hwtcon_device_info.regal.va);
	if (g_hwtcon_device_info.paper_top.va)
		iounmap(g_hwtcon_device_info.paper_top.va);
	if (g_hwtcon_device_info.wf_lut_rdma.va)
		iounmap(g_hwtcon_device_info.wf_lut_rdma.va);
	if (g_hwtcon_device_info.wf_lut_dpi.va)
		iounmap(g_hwtcon_device_info.wf_lut_dpi.va);
	if (g_hwtcon_device_info.tcon.va)
		iounmap(g_hwtcon_device_info.tcon.va);
	if (g_hwtcon_device_info.mmsys.va)
		iounmap(g_hwtcon_device_info.mmsys.va);
	if (g_hwtcon_device_info.wf_lut.va)
		iounmap(g_hwtcon_device_info.wf_lut.va);

	return 0;
}

bool hwtcon_driver_check_clk_on(struct clk *clock)
{
	if (IS_ERR(g_hwtcon_device_info.clock_info.pipeline0)) {
		TCON_ERR("invalid clock");
		return false;
	}

	return __clk_is_enabled(clock);
}

int hwtcon_driver_enable_pipeline_clk(bool enable)
{
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

	return 0;
}

int hwtcon_driver_enable_dpi_clk(bool enable)
{
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
	return status;
}

int hwtcon_driver_unprepare_clk(void)
{
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
	return hwtcon_driver_check_clk_on(
		g_hwtcon_device_info.clock_info.pipeline5);
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
int hwtcon_driver_enable_mmsys_power(struct hwtcon_task *task, bool enable)
{
	int status = 0;

	hwtcon_core_load_init_setting_from_file();

	if (enable) {
		mutex_lock(&hwtcon_fb_info()->mmsys_power_enable_lock);
		if (!hwtcon_fb_info()->mmsys_power_enable) {
			TCON_LOG("power on MMSYS domain");
			hwtcon_driver_lock_wake_lock(true);
			hwtcon_driver_enable_smi_clk(true);
			status = hwtcon_driver_prepare_clk();
			/*enable clock */
			hwtcon_driver_enable_clock(true);
			hwtcon_core_config_timing(NULL);
			hwtcon_core_start_lut_assign_done_trigger_loop();
			hwtcon_core_start_auto_collision_trigger_loop();
			hwtcon_fb_info()->mmsys_power_enable = true;
			wake_up(&hwtcon_fb_info()->power_state_change_wq);
		}
		mutex_unlock(&hwtcon_fb_info()->mmsys_power_enable_lock);
	} else {
		#ifndef HWTCON_CLK_ALWAYS_ON
		mutex_lock(&hwtcon_fb_info()->mmsys_power_enable_lock);
		/* double check hardware status */
		if (hwtcon_fb_info()->mmsys_power_enable && hwtcon_core_check_hwtcon_idle()) {
			TCON_LOG("power down MMSYS domain");
			pp_write(NULL, TCON_GR1, 0x00000000);
			pp_write(NULL, TCON_GR0, 0x00000000);
			wf_lut_dpi_disable(NULL);
			hwtcon_edp_pinmux_inactive();
			if (!hwtcon_debug_get_info()->fiti_power_always_on)
				fiti_power_enable(false);
			hwtcon_core_stop_lut_assign_done_trigger_loop();
			hwtcon_core_stop_auto_collision_trigger_loop();
			hwtcon_driver_enable_clock(false);
			status = hwtcon_driver_unprepare_clk();
			hwtcon_driver_enable_smi_clk(false);
			hwtcon_fb_info()->current_night_mode = -1;
			hwtcon_fb_info()->current_temp_zone = -1;
			hwtcon_fb_info()->mmsys_power_enable = false;
			wake_up(&hwtcon_fb_info()->power_state_change_wq);
			hwtcon_driver_lock_wake_lock(false);
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
static int hwtcon_driver_register_irq(struct platform_device *pdev)
{
	#if 0
	/* register pipeline irq */
	if (devm_request_irq(&pdev->dev, hwtcon_driver_get_pipeline_irq_id(),
		hwtcon_core_pipeline_irq_handle, 0, "hwtcon", NULL) < 0) {
		TCON_ERR("fail to register pipeline irq");
		return HWTCON_STATUS_REGISTER_IRQ_FAIL;
	}
	#endif

	/* register wb_wdma irq */
	if (devm_request_irq(&pdev->dev, hwtcon_driver_get_wb_wdma_irq_id(),
		hwtcon_core_wb_wdma_irq_handle, 0, "hwtcon", NULL) < 0) {
		TCON_ERR("fail to register wb_wdma irq");
		return HWTCON_STATUS_REGISTER_IRQ_FAIL;
	}

	/* register dpi irq*/
	if (devm_request_irq(&pdev->dev, hwtcon_driver_get_wf_lut_dpi_irq_id(),
		hwtcon_core_wf_lut_dpi_irq_handle, 0, "dpi", NULL) < 0) {
		TCON_ERR("fail to register wf_lut_dpi irq");
		return HWTCON_STATUS_REGISTER_IRQ_FAIL;
	}

	#ifdef HWTCON_ENABLE_WF_LUT_IRQ
	if (devm_request_irq(&pdev->dev, hwtcon_driver_get_wf_lut_irq_id(),
		hwtcon_core_wf_lut_irq_handle, 0, "wflut", NULL) < 0) {
		TCON_ERR("fail to register wf_lut irq");
		return HWTCON_STATUS_REGISTER_IRQ_FAIL;
	}
	#endif

	if (devm_request_irq(&pdev->dev, hwtcon_driver_get_wf_lut_end_irq_id(),
		hwtcon_core_wf_lut_end_irq_handle, 0, "wflutend", NULL) < 0) {
		TCON_ERR("fail to register wf_lut_end irq");
		return HWTCON_STATUS_REGISTER_IRQ_FAIL;
	}

	#ifdef HWTCON_EANBLE_DISP_RDMA_IRQ
	if (devm_request_irq(&pdev->dev, hwtcon_driver_get_disp_rdma_irq_id(),
		hwtcon_core_disp_rdma_irq_handle, 0, "disprdma", NULL) < 0) {
		TCON_ERR("fail to register disp_rdma irq");
		return HWTCON_STATUS_REGISTER_IRQ_FAIL;
	}
	#endif

	TCON_LOG("register wb_wdma irq:%d success",
		hwtcon_driver_get_wb_wdma_irq_id());
	TCON_LOG("register wf_lut_dpi irq:%d success",
		hwtcon_driver_get_wf_lut_dpi_irq_id());
	TCON_LOG("register wf_lut irq:%d success",
		hwtcon_driver_get_wf_lut_irq_id());

	return 0;
}

static int hwtcon_driver_unregister_irq(struct platform_device *pdev)
{
	#if 0
	/* register pipeline irq */
	if (devm_request_irq(&pdev->dev, hwtcon_driver_get_pipeline_irq_id(),
		NULL, 0, "hwtcon", NULL) < 0) {
		TCON_ERR("fail to register pipeline irq");
		return HWTCON_STATUS_REGISTER_IRQ_FAIL;
	}
	#endif

	/* register wb_wdma irq */
	devm_free_irq(&pdev->dev, hwtcon_driver_get_wb_wdma_irq_id(), NULL);

	/* register dpi irq*/
	devm_free_irq(&pdev->dev, hwtcon_driver_get_wf_lut_dpi_irq_id(), NULL);

	#ifdef HWTCON_ENABLE_WF_LUT_IRQ
	devm_free_irq(&pdev->dev, hwtcon_driver_get_wf_lut_irq_id(), NULL);
	#endif

	devm_free_irq(&pdev->dev, hwtcon_driver_get_wf_lut_end_irq_id(), NULL);

	#ifdef HWTCON_EANBLE_DISP_RDMA_IRQ
	devm_free_irq(&pdev->dev, hwtcon_driver_get_disp_rdma_irq_id(), NULL);
	#endif

	return 0;
}

s32 hwtcon_driver_cmdq_timeout_dump(u64 engineFlag, int level)
{
	TCON_ERR("dump reg begin");
	TCON_ERR("B100: 0x%08x B104: 0x%08x",
		pp_read(PIPELINT_LUT_STATUS0_VA),
		pp_read(PIPELINT_LUT_STATUS1_VA));
	/*
	 * 0x14004000 = 0x3E: all frame show done.
	 */
	TCON_ERR("14004000: 0x%08x",
		pp_read(hwtcon_driver_get_wf_lut_va() + 0x00));
	TCON_ERR("regal status:0x%08x func:0x%x",
		pp_read(PAPER_TCTOP_REGAL_CFG_VA),
		pp_read(hwtcon_driver_get_regal_va()));
	TCON_ERR("DPI enable:%d", pp_read(hwtcon_driver_get_wf_lut_dpi_va() + 0x000));
	TCON_ERR("WDMA Event:%d", cmdqCoreGetEvent(CMDQ_EVENT_WB_WDMA_DONE));
	TCON_ERR("dump reg end");
	return 0;
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

	TCON_LOG("enter hwtcon probe");

	status = hwtcon_driver_init_device_info(pdev);
	if (status != 0)
		return status;

	pm_runtime_enable(&pdev->dev);

	hwtcon_edp_pinmux_control(pdev);

	status = hwtcon_fb_register_fb(pdev);
	if (status != 0)
		return status;

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
	hwtcon_driver_unregister_irq(pdev);
	hwtcon_fb_unregister_fb(pdev);
	hwtcon_edp_pinmux_release();
	pm_runtime_disable(&pdev->dev);
	hwtcon_driver_destroy_device_info(pdev);
	return 0;
}

static const struct of_device_id hwtcon_of_ids[] = {
	{.compatible = "mediatek,hwtcon",},
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
	if (hw_version == HW_VERSION_MT8113) {
		TCON_ERR("try to insmod MT8110 hwtcon driver on other IC:0x%08x",
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
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Hardware TCON");
late_initcall_sync(hwtcon_init);
module_exit(hwtcon_exit);
