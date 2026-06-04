/*
 * Copyright (C) 2020 MediaTek Inc.
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

#ifndef __MDP_BASE_H__
#define __MDP_BASE_H__

#define MDP_HW_CHECK


static u32 mdp_engine_port[ENGBASE_COUNT] = {
	0,	/*ENGBASE_IMGSYS_CONFIG,*/
	0,	/*ENGBASE_MMSYS_MUTEX,*/
	0,	/*ENGBASE_MDP_RDMA0,*/
	0,	/*ENGBASE_MDP_WROT0,*/
	0,	/*ENGBASE_MDP_GAMMA,*/
	0,	/*ENGBASE_MDP_DITHER,*/
	0,	/*ENGBASE_MDP_RSZ0,*/
	0,	/*ENGBASE_MDP_TDSHP0,*/
};

static u32 mdp_base[ENGBASE_COUNT] = {
	[ENGBASE_IMGSYS_CONFIG] = 0x15000000,
	[ENGBASE_MMSYS_MUTEX] = 0x15001000,
	[ENGBASE_MDP_RDMA0] = 0x15007000,
	[ENGBASE_MDP_WROT0] = 0x1500a000,
	[ENGBASE_MDP_GAMMA] = 0x1500b000,
	[ENGBASE_MDP_DITHER] = 0x1500c000,
	[ENGBASE_MDP_RSZ0] = 0x15008000,
	[ENGBASE_MDP_TDSHP0] = 0x15009000,
};

#endif
