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

#ifndef __HWTCON_DEF_H__
#define __HWTCON_DEF_H__
#include <linux/printk.h>
#include <linux/types.h>
#include "hwtcon_ioctl_cmd.h"

/* HWTCON error code */
enum HWTCON_STATUS {
	HWTCON_STATUS_OK = 0,
	HWTCON_STATUS_CREATE_FS_FAIL = -1,
	HWTCON_STATUS_FB_STRUCT_ALLOC_FAIL = -2,
	HWTCON_STATUS_FB_ALLOC_FAIL = -3,
	HWTCON_STATUS_INVALID_IOCTL_CMD = -4,
	HWTCON_STATUS_COPY_FROM_USER_FAIL = -5,
	HWTCON_STATUS_COPY_TO_USER_FAIL = -6,
	HWTCON_STATUS_INVALID_UPDATE_SCHEME = -7,
	HWTCON_STATUS_INVALID_WB_INDEX = -8,
	HWTCON_STATUS_INVALID_PARAM = -9,
	HWTCON_STATUS_GET_TASK_FAIL = -10,
	HWTCON_STATUS_REGION_NOT_MATCH = -11,
	HWTCON_STATUS_OPEN_FILE_FAIL = -12,
	HWTCON_STATUS_GET_RESOURCE_FAIL = -13,
	HWTCON_STATUS_OF_IOMAP_FAIL = -14,
	HWTCON_STATUS_CREAT_THREAD_FAIL = -15,
	HWTCON_STATUS_GET_IRQ_ID_FAIL = -16,
	HWTCON_STATUS_REGISTER_IRQ_FAIL = -17,
	HWTCON_STATUS_WAIT_TASK_STATE_TIMEOUT = -18,
	HWTCON_STATUS_PARSE_CLOCK_FAIL = -19,
	HWTCON_STATUS_ENABLE_CLOCK_FAIL = -20,
};

enum HISTOGRAM_GREY_LEVEL {
	HISTOGRAM_GREY_LEVEL_Y2 = 0x40000001,
	HISTOGRAM_GREY_LEVEL_Y4 = 0x1010101,
	HISTOGRAM_GREY_LEVEL_Y8 = 0x11111111,
	HISTOGRAM_GREY_LEVEL_Y16 = 0x55555555,
};

enum MDP_DITHER_ALGO {
	MDP_DITHER_ALGO_Y8_Y4_Q = 0x100,
	MDP_DITHER_ALGO_Y8_Y2_Q = 0x200,
	MDP_DITHER_ALGO_Y8_Y1_Q = 0x300,
	MDP_DITHER_ALGO_Y4_Y2_Q = 0x10200,
	MDP_DITHER_ALGO_Y4_Y1_Q = 0x10300,

	MDP_DITHER_ALGO_Y8_Y4_B = 0x101,
	MDP_DITHER_ALGO_Y8_Y2_B = 0x201,
	MDP_DITHER_ALGO_Y8_Y1_B = 0x301,
	MDP_DITHER_ALGO_Y4_Y2_B = 0x10201,
	MDP_DITHER_ALGO_Y4_Y1_B = 0x10301,

	MDP_DITHER_ALGO_Y8_Y4_S = 0x102,
	MDP_DITHER_ALGO_Y8_Y2_S = 0x202,
	MDP_DITHER_ALGO_Y8_Y1_S = 0x302,
	MDP_DITHER_ALGO_Y4_Y2_S = 0x10202,
	MDP_DITHER_ALGO_Y4_Y1_S = 0x10302,
};

#define HWTCON_DRIVER_NAME "hwtcon"
#define HWTCON_TASK_TIMEOUT_MS 10000
#define HWTCON_TASK_WAIT_MARKER_TIMEOUT_MS 20000
#define HWTCON_IRQ_CLEAR_TIMEOUT_MS 10
#define HWTCON_MAX_QUEUE_ITEM 1000

#define HWTCON_TIME unsigned long long

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) > (b) ? (b) : (a))

/* control log level */
bool hwtcon_get_log_level(void);
bool hwtcon_get_epdc_debug_level(void);


/* print log to /proc/hwtcon/error */
void hwtcon_debug_err_printf(const char *print_msg, ...);

#define TCON_ERR_SAVE(string, args...) do {\
	pr_notice("[HWTCON ERR]"string" @%s,%u\n", \
		##args, __func__, __LINE__); \
	hwtcon_debug_err_printf("[HWTCON ERR]"string" @%s,%u\n", \
		##args, __func__, __LINE__); \
	} while (0)

#define TCON_ERR(string, args...) \
	pr_notice("[HWTCON ERR]"string" @%s,%u\n", ##args, __func__, __LINE__)

#define TCON_WARN(string, args...) \
	pr_notice("[HWTCON WARN]"string" @%s,%u\n", ##args, __func__, __LINE__)

#define TCON_EPDC(string, args...) \
do { \
	if (hwtcon_get_epdc_debug_level()) \
		pr_notice("[HWTCON EPDC]"string"\n", ##args); \
} while (0)


#define TCON_LOG(string, args...) \
do { \
	if (hwtcon_get_log_level()) \
		pr_notice("[HWTCON LOG]"string"\n", ##args); \
} while (0)

#define EINK_DEFAULT_TEMPERATURE 25
#define EINK_DEFAULT_POWER_DOWN_TIME 0
#define EINK_NO_POWER_DOWN (-1)
#define MAX_LUT_REGION_COUNT 64

#define BIT_ENABLE(x) (1 << (x))
#define BIT_DISABLE(x) (0 << (x))
#define BIT_USE_AUTO_SOF(x) (0 << (x))
#define BIT_USE_SW_SOF(x) (1 << (x))

//#define SCREEN_WIDTH 1448
//#define SCREEN_HEIGHT 1072
#define FB_FRMAE_COUNT 1
#define FB_VIRTUAL_FRAME_COUNT 2


#endif /* __HWTCON_DEF_H__ */
