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

#ifndef __HWTCON_DRIVER_H__
#define __HWTCON_DRIVER_H__
#include <linux/types.h>

#include "hwtcon_core.h"

enum HW_VERSION_ENUM {
	HW_VERSION_MT8110_1 = 0xCA00,
	HW_VERSION_MT8110_2 = 0xCA01,
	HW_VERSION_MT8113 = 0xCA02,
};

char *hwtcon_driver_get_img_rdma_va(void);
u32 hwtcon_driver_get_img_rdma_pa(void);
char *hwtcon_driver_get_wb_rdma_va(void);
u32 hwtcon_driver_get_wb_rdma_pa(void);
char *hwtcon_driver_get_wb_wdma_va(void);
u32 hwtcon_driver_get_wb_wdma_pa(void);
char *hwtcon_driver_get_pipeline_va(void);
u32 hwtcon_driver_get_pipeline_pa(void);
char *hwtcon_driver_get_regal_va(void);
u32 hwtcon_driver_get_regal_pa(void);
char *hwtcon_driver_get_paper_top_va(void);
u32 hwtcon_driver_get_paper_top_pa(void);
char *hwtcon_driver_get_wf_lut_rdma_va(void);
u32 hwtcon_driver_get_wf_lut_rdma_pa(void);
char *hwtcon_driver_get_wf_lut_dpi_va(void);
u32 hwtcon_driver_get_wf_lut_dpi_pa(void);
char *hwtcon_driver_get_tcon_va(void);
u32 hwtcon_driver_get_tcon_pa(void);
char *hwtcon_driver_get_mmsys_va(void);
u32 hwtcon_driver_get_mmsys_pa(void);
char *hwtcon_driver_get_wf_lut_va(void);
u32 hwtcon_driver_get_wf_lut_pa(void);
u32 hwtcon_driver_get_wf_lut_dpi_irq_id(void);
u32 hwtcon_driver_get_tcon_irq_id(void);
u32 hwtcon_driver_get_wf_lut_irq_id(void);
u32 hwtcon_driver_get_wf_lut_end_irq_id(void);
/* control pipeline & dpi clock */
int hwtcon_driver_enable_clock(bool enable);
int hwtcon_driver_enable_smi_clk(bool enable);
int hwtcon_driver_prepare_clk(void);
int hwtcon_driver_unprepare_clk(void);
int hwtcon_driver_enable_mmsys_power(struct hwtcon_task *task, bool enable);
int hwtcon_driver_enable_pipeline_clk(bool enable);
int hwtcon_driver_enable_dpi_clk(bool enable);
void hwtcon_driver_force_enable_mmsys_domain(bool enable);
u32 hwtcon_driver_get_epd_index(void);
char *hwtcon_driver_get_wf_file_path(void);
void hwtcon_driver_set_wf_file_path(char *file_path);
bool hwtcon_driver_pipeline_clk_is_enable(void);

#endif /* __HWTCON_DRIVER_H__ */
