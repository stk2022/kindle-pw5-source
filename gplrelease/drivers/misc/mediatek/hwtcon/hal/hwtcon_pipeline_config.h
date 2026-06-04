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

#ifndef __HWTCON_PIPELINE_CONFIG_H__
#define __HWTCON_PIPELINE_CONFIG_H__

#include <linux/types.h>
#include <linux/mailbox/mtk-cmdq-mailbox.h>
#include "hwtcon_reg.h"
#include "hwtcon_def.h"
#include "hwtcon_rect.h"
#include "cmdq_record.h"


enum GET_LUT_INFO_TYPE_ENUM {
	/* paper top trigger LUT info.
	 * MUST Read before trigger pipeline work.
	 */
	GET_LUT_INFO_TYPE_TRIGGER = 0,
	/* current triggered img buffer LUT info. */
	GET_LUT_INFO_TYPE_IMG = 1,
	/* total triggered img buffer LUT info before WF_LUT done. */
	GET_LUT_INFO_TYPE_TOTAL = 2,
};

enum LUT_COLLISION_HANDLE_ENUM {
	/* handle colloison */
	LUT_COLLISION_HANDLE_NORMAL = 0 << 2 | 0 << 3,
	/* detect LUT collision, handle the LUT .
	 * but when release lut, will not auto update collision region
	 */
	LUT_COLLISION_HANDLE_NO_UPDATE = 1 << 3 | 0 << 2,
	/* only detect LUT collision, will not handle the lut region,
	 * need to use software clear collision status.
	 */
	LUT_COLLISION_HANDLE_DETECT_ONLY = 0 << 3 | 1 << 2,
};

enum PIPELINE_IRQ_ENUM {
	PIPELINE_IRQ_ARBIT_DONE = 0,
	PIPELINE_IRQ_LUT_FULL = 1,
	PIPELINE_IRQ_ASSIGN_DONE = 2,
	PIPELINE_IRQ_OCCURS_COLLISION = 3,
	PIPELINE_IRQ_COLLISION_UPDATE_DONE = 4,
};

/* sw clear lut status */
void pipeline_config_clear_lut_status(struct cmdqRecStruct *pkt, u32 lut_id);


/* sw clear collision status */
void pipeline_config_clear_collision_status(struct cmdqRecStruct *pkt,
	u32 lut_id);


/* enable sw config access image read buffer size. default use HW calculate. */
void pipeline_config_sw_image_size(struct cmdqRecStruct *pkt,
	struct rect region);


/* SW release LUT. normlly should release be WF_LUT */
void pipeline_config_release_lut(struct cmdqRecStruct *pkt, u32 lut_index);

/* when occur collision, How to handle */
void pipeline_config_collision_handle_method(struct cmdqRecStruct *pkt,
	enum LUT_COLLISION_HANDLE_ENUM type);

u64 pipeline_get_lut_status(void);
u64 pipeline_get_assigned_lut_status(void);
u64 pipeline_get_collision_lut_status(void);
void pipeline_get_collision_region(u32 *collision_count,
	struct rect *region);


/* set LUT ID usage limit, when set lut x limit,
 * HW will not assign region to this lut x
 */
void pipeline_config_limit_hw_lut_usage(struct cmdqRecStruct *pkt,
	u32 lut_id);

/* enable pipeline IRQ */
void pipeline_config_enable_irq(struct cmdqRecStruct *pkt,
	enum PIPELINE_IRQ_ENUM irq_type);

/* clear IRQ flag */
void pipeline_config_clear_irq(struct cmdqRecStruct *pkt,
	enum PIPELINE_IRQ_ENUM irq_type);

/* read irq flag */
u32 pipeline_read_irq_flag(struct cmdqRecStruct *pkt);

/* sw config pipeline img buffer upd lut info */
void pipeline_config_sw_upd_lut(struct cmdqRecStruct *pkt,
	bool enable_sw_config,
	int lut_id, int priority, struct rect region);

/* get LUT usage and current assign LUT. collision info */
void pipeline_print_lut_usage_status(void);


void pipeline_get_upd_status(u32 *collision_update_num,
	u32 *image_update_num,
	u32 *lut_update_num,
	u32 *lut_use_num);

void pipeline_get_unreleased_lut_info(struct cmdqRecStruct *pkt,
	int index, u32 *priority,
	enum WAVEFORM_MODE_ENUM *waveform_mode,
	struct rect *region);

/* sw config pipeline LUT info */
void pipeline_config_sw_lut_info(struct cmdqRecStruct *pkt,
	int lut_id,
	int priority,
	enum WAVEFORM_MODE_ENUM wf_mode,
	struct rect region);



#endif /* __HWTCON_PIPELINE_CONFIG_H__ */
