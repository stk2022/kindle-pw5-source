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

#include "hwtcon_pipeline_config.h"
#include "hwtcon_hal.h"
#include "hwtcon_def.h"


/* sw clear lut status */
void pipeline_config_clear_lut_status(struct cmdqRecStruct *pkt, u32 lut_id)
{
	if (lut_id >= MAX_LUT_REGION_COUNT) {
		TCON_ERR("invalid lut_id:%d", lut_id);
		return;
	}

	if ((lut_id >= 0) && (lut_id <= 31))
		pp_write_mask(pkt, PIPELINE_LUT_CLR0,
			1 << lut_id,
			BIT_MASK(lut_id));
	else if ((lut_id >= 32) && (lut_id <= 63))
		pp_write_mask(pkt, PIPELINE_LUT_CLR1,
			1 << (lut_id - 32),
			BIT_MASK(lut_id - 32));
	else {
		TCON_ERR("invalid lut_id:%d", lut_id);
		return;
	}
}


/* sw clear collision status */
void pipeline_config_clear_collision_status(struct cmdqRecStruct *pkt,
	u32 lut_id)
{
	if (lut_id >= MAX_LUT_REGION_COUNT) {
		TCON_ERR("invalid lut_id:%d", lut_id);
		return;
	}

	if ((lut_id >= 0) && (lut_id <= 31))
		pp_write_mask(pkt, PIPELINE_COL_CLR0,
			1 << lut_id,
			BIT_MASK(lut_id));
	else if ((lut_id >= 32) && (lut_id <= 63))
		pp_write_mask(pkt, PIPELINE_COL_CLR1,
			1 << (lut_id - 32),
			BIT_MASK(lut_id - 32));
	else {
		TCON_ERR("invalid lut_id:%d", lut_id);
		return;
	}
}

/* enable sw config access image read buffer size. default use HW calculate. */
void pipeline_config_sw_image_size(struct cmdqRecStruct *pkt,
	struct rect region)
{
	pp_write(pkt, PIPELINE_IMG_ACCESS_CFG0,
		region.y << 13 | region.x << 0);
	pp_write(pkt, PIPELINE_IMG_ACCESS_CFG1,
		1 << 31 | region.height << 13 | region.width << 0);
}


/* SW release LUT. normlly should release be WF_LUT */
void pipeline_config_release_lut(struct cmdqRecStruct *pkt, u32 lut_index)
{
	if ((lut_index >= 0) && (lut_index <= 31))
		pp_write_mask(pkt, PIPELINE_COL_UPD_CFG1,
			1 << lut_index,
			BIT_MASK(lut_index));
	else if ((lut_index >= 32) && (lut_index <= 63))
		pp_write_mask(pkt, PIPELINE_COL_UPD_CFG2,
			1 << (lut_index - 32),
			BIT_MASK(lut_index - 32));
	else {
		TCON_ERR("invalid lut index:%d", lut_index);
		return;
	}
	/* BIT28 enable lut release.
	 * BIT29 trigger lut release, write 1 then 0 to take effect.
	 */
	pp_write_mask(pkt, PIPELINE_COL_UPD_CFG0,
		1 << 28 | 1 << 29,
		GENMASK(29, 28));
	pp_write_mask(pkt, PIPELINE_COL_UPD_CFG0,
		1 << 28 | 0 << 29,
		GENMASK(29, 28));
}

/* when occur collision, How to handle */
void pipeline_config_collision_handle_method(struct cmdqRecStruct *pkt,
	enum LUT_COLLISION_HANDLE_ENUM type)
{
	pp_write_mask(pkt, PIPELINE_COL_UPD_CFG0, type, GENMASK(3, 2));
}

/* set LUT ID usage limit, when set lut x limit,
 * HW will not assign region to this lut x
 */
void pipeline_config_limit_hw_lut_usage(struct cmdqRecStruct *pkt,
	u32 lut_id)
{
	if ((lut_id >= 0) && (lut_id <= 31))
		pp_write_mask(pkt, PIPELINE_LUT_USE_CFG0,
			1 << lut_id,
			BIT_MASK(lut_id));
	else if ((lut_id >= 32) && (lut_id <= 63))
		pp_write_mask(pkt, PIPELINE_LUT_USE_CFG1,
			1 << (lut_id - 32),
			BIT_MASK(lut_id - 32));
	else
		TCON_ERR("invalid lut_id:%d", lut_id);
}


/* enable pipeline IRQ */
void pipeline_config_enable_irq(struct cmdqRecStruct *pkt,
	enum PIPELINE_IRQ_ENUM irq_type)
{
	switch (irq_type) {
	case PIPELINE_IRQ_ARBIT_DONE:
		pp_write_mask(pkt, PIPELINE_LUT_ARBIT_CFG,
			1 << 4,
			BIT_MASK(4));
		break;
	case PIPELINE_IRQ_LUT_FULL:
		pp_write_mask(pkt, PIPELINE_LUT_ARBIT_CFG,
			1 << 1,
			BIT_MASK(1));
		break;
	case PIPELINE_IRQ_ASSIGN_DONE:
		pp_write_mask(pkt, PIPELINE_LUT_ASSIGN_CFG,
			1 << 0,
			BIT_MASK(0));
		break;
	case PIPELINE_IRQ_OCCURS_COLLISION:
		pp_write_mask(pkt, PIPELINE_COL_UPD_CFG0,
			1 << 1,
			BIT_MASK(1));
		break;
	case PIPELINE_IRQ_COLLISION_UPDATE_DONE:
		pp_write_mask(pkt, PIPELINE_COL_UPD_CFG0,
			1 << 5,
			BIT_MASK(5));
		break;
	default:
		TCON_ERR("invalid irq type:%d", irq_type);
	}
}

/* clear IRQ flag */
void pipeline_config_clear_irq(struct cmdqRecStruct *pkt,
	enum PIPELINE_IRQ_ENUM irq_type)
{
	switch (irq_type) {
	case PIPELINE_IRQ_ARBIT_DONE:
		pp_write_mask(pkt, PIPELINE_LUT_ARBIT_CFG,
			1 << 5,
			BIT_MASK(5));
		pp_write_mask(pkt, PIPELINE_LUT_ARBIT_CFG,
			0 << 5,
			BIT_MASK(5));
		break;
	case PIPELINE_IRQ_LUT_FULL:
		pp_write_mask(pkt, PIPELINE_LUT_ARBIT_CFG,
			1 << 0,
			BIT_MASK(0));
		pp_write_mask(pkt, PIPELINE_LUT_ARBIT_CFG,
			0 << 0,
			BIT_MASK(0));
		break;
	case PIPELINE_IRQ_ASSIGN_DONE:
		pp_write_mask(pkt, PIPELINE_LUT_ASSIGN_CFG,
			1 << 1,
			BIT_MASK(1));
		pp_write_mask(pkt, PIPELINE_LUT_ASSIGN_CFG,
			0 << 1,
			BIT_MASK(1));
		break;
	case PIPELINE_IRQ_OCCURS_COLLISION:
		pp_write_mask(pkt, PIPELINE_COL_UPD_CFG0,
			1 << 0,
			BIT_MASK(0));
		pp_write_mask(pkt, PIPELINE_COL_UPD_CFG0,
			0 << 0,
			BIT_MASK(0));
		break;
	case PIPELINE_IRQ_COLLISION_UPDATE_DONE:
		pp_write_mask(pkt, PIPELINE_COL_UPD_CFG0,
			1 << 4,
			BIT_MASK(4));
		pp_write_mask(pkt, PIPELINE_COL_UPD_CFG0,
			0 << 4,
			BIT_MASK(4));
		break;
	default:
		TCON_ERR("invalid irq type:%d", irq_type);
	}

}

/* sw config pipeline img buffer upd lut info */
void pipeline_config_sw_upd_lut_info(struct cmdqRecStruct *pkt,
	bool enable_sw_config,
	int lut_id, int priority,
	struct rect region)
{
	if (enable_sw_config) {
		pp_write_mask(pkt, PIPELINE_UPD_INFO_CLT1,
			region.y << 13 | region.x,
			GENMASK(25, 0));
		pp_write_mask(pkt, PIPELINE_UPD_INFO_CLT2,
			region.height << 13 | region.width,
			GENMASK(25, 0));
		/* bit 30 write 1 then 0 to trigger UPD LUT */
		pp_write_mask(pkt, PIPELINE_UPD_INFO_CLT0,
			1 << 31 | 1 << 30 | lut_id << 24 | priority << 16,
			GENMASK(22, 16) | GENMASK(31, 24));
		pp_write_mask(pkt, PIPELINE_UPD_INFO_CLT0,
			1 << 31 | 0 << 30 | lut_id << 24 | priority << 16,
			GENMASK(22, 16) | GENMASK(31, 24));
	} else {
		pp_write(pkt, PIPELINE_UPD_INFO_CLT0, 0);
		pp_write(pkt, PIPELINE_UPD_INFO_CLT1, 0);
		pp_write(pkt, PIPELINE_UPD_INFO_CLT2, 0);
	}
}

/* sw config pipeline LUT info */
void pipeline_config_sw_lut_info(struct cmdqRecStruct *pkt,
	int lut_id,
	int priority,
	enum WAVEFORM_MODE_ENUM wf_mode,
	struct rect region)
{
	pp_write_mask(pkt, PIPELINE_LUT_INFO_CLT0,
		lut_id << 24 | priority << 16 | wf_mode << 12,
		GENMASK(29, 24) | GENMASK(22, 16) | GENMASK(15, 12));
	pp_write(pkt, PIPELINE_LUT_INFO_CLT1,
		region.y << 13 | region.x << 0);
	pp_write(pkt, PIPELINE_LUT_INFO_CLT2,
		region.height << 13 | region.width << 0);
	/* trigger sw lut enable */
	pp_write_mask(pkt, PIPELINE_LUT_INFO_CLT0,
		1 << 31 | 1 << 30,
		GENMASK(31, 30));
	pp_write_mask(pkt, PIPELINE_LUT_INFO_CLT0,
		0 << 31 | 0 << 30,
		GENMASK(31, 30));
}

/* sw config pipeline lut arbit */
void pipeline_config_sw_lut_arbit(struct cmdqRecStruct *pkt,
	bool enable_sw_config)
{
	if (enable_sw_config) {
		pp_write_mask(pkt, PIPELINE_LUT_ARBIT_CFG,
			enable_sw_config << 31 | 1 << 30,
			GENMASK(31, 30));
		pp_write_mask(pkt, PIPELINE_LUT_ARBIT_CFG,
			enable_sw_config << 31 | 0 << 30,
			GENMASK(31, 30));
	} else {
		pp_write(pkt, PIPELINE_LUT_ARBIT_CFG, 0);
	}
}

/* sw config pipeline lut assign mode */
void pipeline_config_sw_lut_assign(struct cmdqRecStruct *pkt,
	bool enable_sw_config)
{
	if (enable_sw_config) {
		pp_write_mask(pkt, PIPELINE_LUT_ASSIGN_CFG,
			enable_sw_config << 31 | 1 << 30,
			GENMASK(31, 30));
		pp_write_mask(pkt, PIPELINE_LUT_ASSIGN_CFG,
			enable_sw_config << 31 | 0 << 30,
			GENMASK(31, 30));
	} else {
		pp_write(pkt, PIPELINE_LUT_ASSIGN_CFG, 0);
	}
}

/* sw config pipeline lut collision */
void pipeline_config_sw_lut_collision(struct cmdqRecStruct *pkt,
	bool enable_sw_config)
{
	if (enable_sw_config) {
		pp_write_mask(pkt, PIPELINE_COL_UPD_CFG0,
			enable_sw_config << 31 | 1 << 30,
			GENMASK(31, 30));
		pp_write_mask(pkt, PIPELINE_COL_UPD_CFG0,
			enable_sw_config << 31 | 0 << 30,
			GENMASK(31, 30));
	} else {
		pp_write(pkt, PIPELINE_COL_UPD_CFG0, 0);
	}
}

/* read irq flag */
u32 pipeline_read_irq_flag(struct cmdqRecStruct *pkt)
{
	return 0;
}

void pipeline_get_unreleased_lut_info(struct cmdqRecStruct *pkt,
	int index, u32 *priority,
	enum WAVEFORM_MODE_ENUM *waveform_mode,
	struct rect *region)
{
	u32 config0 = 0;
	u32 config1 = 0;
	u32 config2 = 0;

	pp_write_mask(pkt, PIPELINE_LUT_INFO_CLT0,
		1 << 7 | index,
		BIT_MASK(7) | GENMASK(5, 0));

	config0 = pp_read(PIPELINE_LUT_INFO_STA0_VA);
	config1 = pp_read(PIPELINE_LUT_INFO_STA1_VA);
	config2 = pp_read(PIPELINE_LUT_INFO_STA2_VA);

	if (priority)
		*priority = (config2 >> 5) & GENMASK(6, 0);
	if (waveform_mode)
		*waveform_mode = (config2 >> 0) & GENMASK(3, 0);

	if (region) {
		region->x = (config0 >> 0) & GENMASK(12, 0);
		region->y = (config0 >> 13) & GENMASK(12, 0);
		region->width = (config1 >> 0) & GENMASK(12, 0);
		region->height = (config1 >> 13) & GENMASK(12, 0);
	}

}


void pipeline_get_assigned_lut_info(struct cmdqRecStruct *pkt,
	int index,
	struct rect *region)
{
	u32 config0 = 0;
	u32 config1 = 0;

	pp_write_mask(pkt, PIPELINE_UPD_INFO_CLT0,
		1 << 7 | index,
		BIT_MASK(7) | GENMASK(5, 0));

	config0 = pp_read(PIPELINE_UPD_INFO0_VA);
	config1 = pp_read(PIPELINE_UPD_INFO1_VA);

	if (region) {
		region->x = (config0 >> 0) & GENMASK(12, 0);
		region->y = (config0 >> 13) & GENMASK(12, 0);
		region->width = (config1 >> 0) & GENMASK(12, 0);
		region->height = (config1 >> 13) & GENMASK(12, 0);
	}
}

u64 pipeline_get_lut_status(void)
{
	/* 64 bit represent for 64 lut.
	 * bit x = 1 means lut x is used.
	 */

	u32 readback0 = pp_read(PIPELINT_LUT_STATUS0_VA);
	u32 readback1 = pp_read(PIPELINT_LUT_STATUS1_VA);

	return ((u64)readback1 << 32 | readback0);
}

u64 pipeline_get_assigned_lut_status(void)
{
	u32 readback0 = pp_read(PIPELINE_ASSIGN_STATUS0_VA);
	u32 readback1 = pp_read(PIPELINE_ASSIGN_STATUS1_VA);

	return ((u64)readback1 << 32 | readback0);
}

u64 pipeline_get_collision_lut_status(void)
{
	/* 64 bit represent for 64 lut.
	 * bit x = 1 means lut x has collision.
	 */
	u32 readback0 = pp_read(PIPELINE_COL_STATUS0_VA);
	u32 readback1 = pp_read(PIPELINE_COL_STATUS1_VA);

	return ((u64)readback1 << 32 | readback0);
}

void pipeline_get_collision_region(u32 *collision_count,
	struct rect *region)
{
	u32 read_back0 = 0;
	u32 read_back1 = 0;
	u32 count = 0;

	read_back0 = pp_read(PIPELINE_COL_REGION_INFO0_VA);
	read_back1 = pp_read(PIPELINE_COL_REGION_INFO1_VA);
	count = (read_back1 & GENMASK(31, 26)) >> 26;

	if (region) {
		if (count != 0) {
			region->y = (read_back0 & GENMASK(25, 13)) >> 13;
			region->x = (read_back0 & GENMASK(12, 0)) >> 0;
			region->height = (read_back1 & GENMASK(25, 13)) >> 13;
			region->width = (read_back1 & GENMASK(12, 0)) >> 0;
		} else
			memset(region, 0, sizeof(struct rect));
	}
	if (collision_count)
		*collision_count = count;
}

void pipeline_get_upd_status(u32 *collision_update_num,
	u32 *image_update_num,
	u32 *lut_update_num,
	u32 *lut_use_num)
{

	u32 readback = pp_read(PIPELINE_UPD_STATUS_VA);

	if (collision_update_num)
		*collision_update_num = (readback & GENMASK(30, 24)) >> 24;
	if (image_update_num)
		*image_update_num = (readback & GENMASK(22, 16)) >> 16;
	if (lut_update_num)
		*lut_update_num = (readback & GENMASK(14, 8)) >> 8;
	if (lut_use_num)
		*lut_use_num = (readback & GENMASK(6, 0)) >> 0;
}

void pipeline_get_upd_read_region(struct rect *region)
{
	u32 readback0 = pp_read(PIPELINE_UPD_INFO0_VA);
	u32 readback1 = pp_read(PIPELINE_UPD_INFO1_VA);

	if (region) {
		region->x = (readback0 & GENMASK(25, 13)) >> 13;
		region->y = (readback0 & GENMASK(12, 0)) >> 0;
		region->width = (readback1 & GENMASK(25, 13)) >> 13;
		region->height = (readback1 & GENMASK(12, 0)) >> 0;
	}
}

void pipeline_get_upd_pixel_region(struct rect *region)
{
	u32 readback0 = pp_read(PIPELINE_PXL_UPD_RGN0_VA);
	u32 readback1 = pp_read(PIPELINE_PXL_UPD_RGN1_VA);

	if (region) {
		region->x = (readback0 & GENMASK(25, 13)) >> 13;
		region->y = (readback0 & GENMASK(12, 0)) >> 0;
		region->width = (readback1 & GENMASK(25, 13)) >> 13;
		region->height = (readback1 & GENMASK(12, 0)) >> 0;
	}
}

void pipeline_get_pixel_collision_region(struct rect *region)
{
	u32 readback0 = pp_read(PIPELINE_PXL_COL_RGN0_VA);
	u32 readback1 = pp_read(PIPELINE_PXL_COL_RGN1_VA);

	if (region) {
		region->x = (readback0 & GENMASK(25, 13)) >> 13;
		region->y = (readback0 & GENMASK(12, 0)) >> 0;
		region->width = (readback1 & GENMASK(25, 13)) >> 13;
		region->height = (readback1 & GENMASK(12, 0)) >> 0;
	}
}

/* get LUT usage and current assign LUT. collision info */
void pipeline_print_lut_usage_status(void)
{
	u64 readback = 0;
	struct rect region = {0};
	u32 collision_count = 0;
	int total_lut_count = 0;
	int i = 0;
	u32 collision_update_num;
	u32 image_update_num;
	u32 lut_update_num;
	u32 lut_use_num;

	readback = pipeline_get_lut_status();


	for (i = 0; i < 64; i++) {
		u32 priority = 0;
		enum WAVEFORM_MODE_ENUM wf_mode = 0;

		if (readback & ((u64)1 << i)) {
			pipeline_get_unreleased_lut_info(NULL, i,
				&priority, &wf_mode, &region);
			TCON_ERR("LUT[%d] wf_mode:%d priority:%d",
				i,
				wf_mode,
				priority);
			TCON_ERR("rect[%d %d %d %d]",
				region.x,
				region.y,
				region.width,
				region.height);
			total_lut_count++;
		}
	}
	TCON_ERR("total LUT:0x%016llx count:%d",
		readback, total_lut_count);

	readback = pipeline_get_assigned_lut_status();
	TCON_ERR("assigned LUT: 0x%016llx", readback);

	readback = pipeline_get_collision_lut_status();
	TCON_ERR("collision LUT: 0x%016llx", readback);

	pipeline_get_collision_region(&collision_count, &region);
	TCON_ERR("collision region: number:%d rect[%d %d %d %d]",
		collision_count,
		region.x,
		region.y,
		region.width,
		region.height);

	pipeline_get_upd_read_region(&region);
	TCON_ERR("upd_read_region:[%d %d %d %d]",
		region.x,
		region.y,
		region.width,
		region.height);

	pipeline_get_upd_pixel_region(&region);
	TCON_ERR("upd_pixel_region:[%d %d %d %d]",
		region.x,
		region.y,
		region.width,
		region.height);

	pipeline_get_pixel_collision_region(&region);
	TCON_ERR("pixel_collision_region:[%d %d %d %d]",
		region.x, region.y,
		region.width,
		region.height);

	pipeline_get_upd_status(&collision_update_num,
		&image_update_num,
		&lut_update_num,
		&lut_use_num);
	TCON_ERR("upd collision_update_num:%d image_update_num:%d",
		collision_update_num,
		image_update_num);
	TCON_ERR("upd lut_update_num:%d lut_use_num:%d",
		lut_update_num,
		lut_use_num);
}
