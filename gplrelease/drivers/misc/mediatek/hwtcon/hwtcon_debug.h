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

#ifndef __HWTCON_DEBUG_H__
#define __HWTCON_DEBUG_H__
#include "hwtcon_def.h"

struct hwtcon_debug_info {
	bool log_level;	/* enable TCON_LOG ? */
	bool epdc_debug; /* enable epdc debug */
	int fixed_temperature; /* fixed temperature instead of read from sesor */
	int debug[10]; /* only for debug use. */
	int enable_wf_lut_dpi_checksum;
	int enable_wf_lut_checksum;
	bool enable_dump_buffer;
	bool fiti_power_always_on;
	char golden_file_name[100];
	unsigned char *debug_va;
};

struct hwtcon_debug_info *hwtcon_debug_get_info(void);

void hwtcon_debug_err_printf(const char *print_msg, ...);
void hwtcon_debug_fiti_printf(const char *print_msg, ...);
void hwtcon_debug_lut_info_printf(const char *print_msg, ...);
void hwtcon_debug_record_printf(const char *print_msg, ...);

int hwtcon_debug_create_procfs(void);
int hwtcon_debug_destroy_procfs(void);

#endif /* __HWTCON_DEBUG_H__ */
