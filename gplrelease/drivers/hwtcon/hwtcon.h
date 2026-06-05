#ifndef __HWTCON_H__
#define __HWTCON_H__

/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2018 MediaTek Inc.
 * Author: Jiaguang Zhang <jiaguang.zhang@mediatek.com>
 */

#if 0
#define WF_LUT_TABLE_ADDR	0x50000000
#define IMG_ADDR			0x51000000
#define WB_ADDR0			0x52000000
#define WB_ADDR1			0x53000000
#endif

struct hwtcon_buffer_info {
	unsigned int wf_file_buffer;
	unsigned int image_buffer;
	unsigned int wb_buffer_0;
	unsigned int wb_buffer_1;
};

extern struct hwtcon_buffer_info g_buffer_info;

#define MMSYS_CG_CON0	(0x14000100)
#define IMGSYS_CG_CON0	(0x15000100)


#endif /* endof __HWTCON_H__ */
