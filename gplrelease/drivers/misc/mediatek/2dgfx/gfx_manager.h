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


#ifndef _GFX_MANAGER_H_
#define _GFX_MANAGER_H_

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/atomic.h>
#include <linux/miscdevice.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/cdev.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/clk.h>

#define GFX_SESSION_DEVICE	"mtk_2dgfx_mgr"

enum GFX_COLORMODE_T {
	GFX_COLORMODE_AYCbCr_CLUT2	=	0,
	GFX_COLORMODE_AYCbCr_CLUT4	=	1,
	GFX_COLORMODE_AYCbCr_CLUT8	=	2,
	GFX_COLORMODE_CbYCrY_16		=	3,
	GFX_COLORMODE_YCbYCr_16		=	4,
	GFX_COLORMODE_AYCbCr_D8888	=	5,
	GFX_COLORMODE_ARGB_CLUT2	=	6,
	GFX_COLORMODE_ARGB_CLUT4	=	7,
	GFX_COLORMODE_ARGB_CLUT8	=	8,
	GFX_COLORMODE_RGB_D565		=	9,
	GFX_COLORMODE_ARGB_D1555	=	10,
	GFX_COLORMODE_ARGB_D4444	=	11,
	GFX_COLORMODE_ARGB_D8888	=	12,
	GFX_COLORMDOE_YUV_420_BLK	=	13,
	GFX_COLORMODE_YUV_420_RS	=	14,
	GFX_COLORMDOE_YUV_422_BLK	=	15,
	GFX_COLORMODE_YUV_422_RS	=	16,
	GFX_COLORMDOE_YUV_444_BLK	=	17,
	GFX_COLORMODE_YUV_444_RS	=	18
};

enum GFX_PD_RULE_T {
	GFX_CLEAR	=	0,
	GFX_DST_IN,
	GFX_DST_OUT,
	GFX_DST_OVER,
	GFX_SRC,
	GFX_SRC_IN,
	GFX_SRC_OUT,
	GFX_SRC_OVER,
	GFX_DST,
	GFX_SRC_ATOP,
	GFX_DST_ATOP,
	GFX_XOR,
	GFX_NONE,
	GFX_ADD,
	GFX_RULE_MAX
};

enum GFX_BUFF_CMD_TYPE_T {
	GFX_BUFF_TYPE_DRAW_OPERATION,
	GFX_BUFF_TYPE_FILL_RECT,
	GFX_BUFF_TYPE_BITBLT,
	GFX_BUFF_TYPE_ALPHA_COMPOSITION,
	GFX_BUFF_TYPE_DRAW_LINE_H,
	GFX_BUFF_TYPE_DRAW_LINE_V,
	GFX_BUFF_TYPE_ALPHA_BLENDING,
	GFX_BUFF_TYPE_GET_CMDBUFFER,
	GFX_BUFF_TYPE_RELEASE_CMDBUFFER,
	GFX_BUFF_TYPE_FLUSH,
	GFX_BUFF_TYPE_STOP,
	GFX_BUFF_TYPE_MAX
};

enum List_Buffer_STATE {
	list_new,
	list_configed,
	list_updated,
	list_useless
};

struct GFX_FILL_T {
	void *pv_dst;
	void *pv_dst2;
	unsigned int i4_dst_x;
	unsigned int i4_dst_y;
	unsigned int ui4_dst_pitch;
	unsigned int ui4_dst_pitch2;
	GFX_COLORMODE_T e_dst_cm;
	unsigned int ui4_width;
	unsigned int ui4_height;
	unsigned int ui4_color;
};

struct GFX_BITBLT_T {
	void *pv_src;
	unsigned int i4_src_x;
	unsigned int i4_src_y;
	unsigned int ui4_src_pitch;
	GFX_COLORMODE_T e_src_cm;
	void *pv_dst;
	int i4_dst_x;
	int i4_dst_y;
	unsigned int ui4_dst_pitch;
	GFX_COLORMODE_T e_dst_cm;
	unsigned int ui4_width;
	unsigned int ui4_height;
	unsigned char ui4_alpha;
};

struct GFX_ALPHA_COMPOSITION_T {
	void *pv_src;
	unsigned int i4_src_x;
	unsigned int i4_src_y;
	unsigned int ui4_src_pitch;
	GFX_COLORMODE_T e_src_cm;
	void *pv_dst;
	unsigned int i4_dst_x;
	unsigned int i4_dst_y;
	unsigned int ui4_dst_pitch;
	GFX_COLORMODE_T e_dst_cm;
	unsigned int ui4_width;
	unsigned int ui4_height;
	unsigned char ui4_alpha;
	GFX_PD_RULE_T e_rule;
};

struct GFX_CMDBUFFER_T {
	unsigned int t_user;
	unsigned int u4GfxTicketId;
	unsigned int u4Priority;
};

enum GFX_CMD_TYPE {
	GFX_SET_TYPE_FILL_RECT,
	GFX_SET_TYPE_BITBLIT,
	GFX_SET_TYPE_ALPHA_COMPOSE,
};

struct GFX_CMD_T {
	GFX_CMD_TYPE gfx_cmd_type;
	GFX_FILL_T rFillCmd;
	GFX_BITBLT_T rBitbltCmd;
	GFX_ALPHA_COMPOSITION_T rAlphaComposeCmd;
};

enum GFX_SYNC_TYPE {
	GFX_CMD_ASYNC,
	GFX_CMD_SYNC,
};

struct GFX_ION {
	int ion_src_fd;
	int src_phy_addr;
	int ion_dst_fd;
	int dst_phy_addr;
};

struct GFX_CMD_CONFIG {
	GFX_CMDBUFFER_T gfx_id;
	GFX_ION gfx_ion;
	GFX_CMD_T gfx_cmd;
	GFX_SYNC_TYPE gfx_sync;
};
#endif
