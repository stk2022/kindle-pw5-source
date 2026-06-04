/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __MTK_SWITCH_CHARGING_H
#define __MTK_SWITCH_CHARGING_H

/*****************************************************************************
 *  Switch Charging State
 ****************************************************************************/

struct switch_charging_alg_data {
	int state;
	bool disable_charging;
	struct mutex ichg_aicr_lock;
	u32 total_charging_time;
	u32 pre_cc_charging_time;
	u32 cc_charging_time;
	u32 cv_charging_time;
	u32 full_charging_time;
	struct timespec charging_begin_time;
};

#endif /* __MTK_SWITCH_CHARGING_H */
