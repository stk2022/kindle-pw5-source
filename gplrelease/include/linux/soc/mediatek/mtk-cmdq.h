/*
 * Copyright (c) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MTK_CMDQ_H__
#define __MTK_CMDQ_H__

#include <linux/mailbox_client.h>
#include <linux/mailbox/mtk-cmdq-mailbox.h>

/* display events in command queue(CMDQ) */
enum cmdq_event {
	/* HWTCON event */
	CMDQ_EVENT_DPI0_SOF = 0,
	CMDQ_EVENT_MAIN_SOF = 1,
	CMDQ_EVENT_PIPELINE_SOF = 2,
	CMDQ_EVENT_WB_WDMA_SOF = 3,
	CMDQ_EVENT_WF_LUT_SOF = 4,
	CMDQ_EVENT_LUT_MERGE_SOF = 5,
	CMDQ_EVENT_WF_LUT_DISP_RDMA_SOF = 6,
	CMDQ_EVENT_DPI0_FRAME_DONE = 7,
	CMDQ_EVENT_LUT_ASSIGN_DONE = 8,
	CMDQ_EVENT_PIPELINE_DONE = 9,
	CMDQ_EVENT_WB_WDMA_DONE = 10,
	CMDQ_EVENT_WF_LUT_FRAME_DONE = 11,
	CMDQ_EVENT_LUT_FRAME_DONE = 12,
	CMDQ_EVENT_DISP_RDMA0_FRAME_DONE = 13,
	CMDQ_EVENT_LUT_COL_EVENT = 14,
	CMDQ_EVENT_DPI_UPDATE_DONE = 15,
	CMDQ_EVENT_LUT_RELEASE_DONE = 16,
	CMDQ_EVENT_TCON_END = 17,


	/* MDP event */
	CMDQ_EVENT_MDP_RDMA0_SOF = 18,
	CMDQ_EVENT_MDP_RSZ0_SOF = 19,
	CMDQ_EVENT_MDP_TDSHP0_SOF = 20,
	CMDQ_EVENT_MDP_WROT0_SOF = 21,
	CMDQ_EVENT_MDP_GAMMA0_SOF = 22,
	CMDQ_EVENT_MDP_DITHER0_SOF = 23,
	CMDQ_EVENT_DISP_OVL0_2L_SOF = 24,
	CMDQ_EVENT_DISP_WDMA0_SOF = 25,
	CMDQ_EVENT_MDP_RDMA0_FRAME_DONE = 26,
	CMDQ_EVENT_MDP_RSZ0_FRAME_DONE = 27,
	CMDQ_EVENT_MDP_TDSHP0_FRAME_DONE = 28,
	CMDQ_EVENT_MDP_WROT0_WRITE_DONE = 29,
	CMDQ_EVENT_MDP_GAMMA0_FRAME_DONE = 30,
	CMDQ_EVENT_MDP_DITHER0_FRAME_DONE = 31,
	CMDQ_EVENT_MDP_OVL0_2L_FRAME_DONE = 32,
	CMDQ_EVENT_MDP_WDMA0_FRAME_DONE = 33,
	CMDQ_EVENT_JPGDEC_FRAME_DONE = 34,
	CMDQ_EVENT_PNG_FRAME_DONE = 35,
	CMDQ_EVENT_PNG_FRAME_DONE0 = 36,
	CMDQ_EVENT_PNG_FRAME_DONE1 = 37,
	CMDQ_EVENT_PNG_FRAME_DONE2 = 38,
	CMDQ_EVENT_IMGRSZ_FRAME_DONE = 39,
	CMDQ_EVENT_JPGDEC_BITS_FRAME_DONE = 40,
	CMDQ_EVENT_IMG_RESERVE_FRAME_DONE_0 = 41,
	CMDQ_EVENT_IMG_RESERVE_FRAME_DONE_1 = 42,
	CMDQ_EVENT_IMG_RESERVE_FRAME_DONE_2 = 43,
	CMDQ_EVENT_IMG_RESERVE_FRAME_DONE_3 = 44,
	CMDQ_EVENT_STREAM_DONE_0 = 45,
	CMDQ_EVENT_STREAM_DONE_1 = 46,
	CMDQ_EVENT_STREAM_DONE_2 = 47,
	CMDQ_EVENT_STREAM_DONE_3 = 48,
	CMDQ_EVENT_STREAM_DONE_4 = 49,
	CMDQ_EVENT_STREAM_DONE_5 = 50,
	CMDQ_EVENT_STREAM_DONE_6 = 51,
	CMDQ_EVENT_STREAM_DONE_7 = 52,
	CMDQ_EVENT_STREAM_DONE_8 = 53,
	CMDQ_EVENT_STREAM_DONE_9 = 54,
	CMDQ_EVENT_BUF_UNDERRUN_EVENT_0 = 55,
	CMDQ_EVENT_BUF_UNDERRUN_EVENT_1 = 56,
	CMDQ_EVENT_MDP_RDMA0_SW_RST_DONE = 57,
	CMDQ_EVENT_MDP_WROT0_SW_RST_DONE = 58,
	CMDQ_EVENT_DISP_OVL0_SW_RST_DONE = 59,
	CMDQ_EVENT_DISP_WDMA0_SW_RST_DONE = 60,

	/* Keep this at the end of HW events */
	CMDQ_MAX_HW_EVENT_COUNT = 61,

	CMDQ_SYNC_HWTCON_WDMA_FRAME_DONE = 400,

	/* SW Sync Tokens (Pre-defined) */
	/* Config thread notify trigger thread */
	CMDQ_SYNC_TOKEN_CONFIG_DIRTY = 401,
	/* Trigger thread notify config thread */
	CMDQ_SYNC_TOKEN_STREAM_EOF = 402,
	/* Block Trigger thread until the ESD check finishes. */
	CMDQ_SYNC_TOKEN_ESD_EOF = 403,
	/* check CABC setup finish */
	CMDQ_SYNC_TOKEN_CABC_EOF = 404,
	/* Block Trigger thread until the path freeze finishes */
	CMDQ_SYNC_TOKEN_FREEZE_EOF = 405,
	/* Pass-2 notifies VENC frame is ready to be encoded */
	CMDQ_SYNC_TOKEN_VENC_INPUT_READY = 406,
	/* VENC notifies Pass-2 encode done so next frame may start */
	CMDQ_SYNC_TOKEN_VENC_EOF = 407,

	/* Notify normal CMDQ there are some secure task done */
	CMDQ_SYNC_SECURE_THR_EOF = 408,
	/* Lock WSM resource */
	CMDQ_SYNC_SECURE_WSM_LOCK = 409,

	/* SW Sync Tokens (User-defined) */
	CMDQ_SYNC_TOKEN_USER_0 = 410,
	CMDQ_SYNC_TOKEN_USER_1 = 411,
	CMDQ_SYNC_TOKEN_POLL_MONITOR = 412,

	/* SW Sync Tokens (Pre-defined) */
	/* Config thread notify trigger thread for external display */
	CMDQ_SYNC_TOKEN_EXT_CONFIG_DIRTY = 415,
	/* Trigger thread notify config thread */
	CMDQ_SYNC_TOKEN_EXT_STREAM_EOF = 416,
	/* Check CABC setup finish */
	CMDQ_SYNC_TOKEN_EXT_CABC_EOF = 417,

	/* Secure video path notify SW token */
	CMDQ_SYNC_DISP_OVL0_2NONSEC_END = 420,
	CMDQ_SYNC_DISP_OVL1_2NONSEC_END = 421,
	CMDQ_SYNC_DISP_2LOVL0_2NONSEC_END = 422,
	CMDQ_SYNC_DISP_2LOVL1_2NONSEC_END = 423,
	CMDQ_SYNC_DISP_RDMA0_2NONSEC_END = 424,
	CMDQ_SYNC_DISP_RDMA1_2NONSEC_END = 425,
	CMDQ_SYNC_DISP_WDMA0_2NONSEC_END = 426,
	CMDQ_SYNC_DISP_WDMA1_2NONSEC_END = 427,
	CMDQ_SYNC_DISP_EXT_STREAM_EOF = 428,

	/**
	 * Event for CMDQ to block executing command when append command
	 * Plz sync CMDQ_SYNC_TOKEN_APPEND_THR(id) in cmdq_core source file.
	 */
	CMDQ_SYNC_TOKEN_APPEND_THR0 = 432,
	CMDQ_SYNC_TOKEN_APPEND_THR1 = 433,
	CMDQ_SYNC_TOKEN_APPEND_THR2 = 434,
	CMDQ_SYNC_TOKEN_APPEND_THR3 = 435,
	CMDQ_SYNC_TOKEN_APPEND_THR4 = 436,
	CMDQ_SYNC_TOKEN_APPEND_THR5 = 437,
	CMDQ_SYNC_TOKEN_APPEND_THR6 = 438,
	CMDQ_SYNC_TOKEN_APPEND_THR7 = 439,
	CMDQ_SYNC_TOKEN_APPEND_THR8 = 440,
	CMDQ_SYNC_TOKEN_APPEND_THR9 = 441,
	CMDQ_SYNC_TOKEN_APPEND_THR10 = 442,
	CMDQ_SYNC_TOKEN_APPEND_THR11 = 443,
	CMDQ_SYNC_TOKEN_APPEND_THR12 = 444,
	CMDQ_SYNC_TOKEN_APPEND_THR13 = 445,
	CMDQ_SYNC_TOKEN_APPEND_THR14 = 446,
	CMDQ_SYNC_TOKEN_APPEND_THR15 = 447,
	CMDQ_SYNC_TOKEN_APPEND_THR16 = 448,
	CMDQ_SYNC_TOKEN_APPEND_THR17 = 449,
	CMDQ_SYNC_TOKEN_APPEND_THR18 = 450,
	CMDQ_SYNC_TOKEN_APPEND_THR19 = 451,
	CMDQ_SYNC_TOKEN_APPEND_THR20 = 452,
	CMDQ_SYNC_TOKEN_APPEND_THR21 = 453,
	CMDQ_SYNC_TOKEN_APPEND_THR22 = 454,
	CMDQ_SYNC_TOKEN_APPEND_THR23 = 455,
	CMDQ_SYNC_TOKEN_APPEND_THR24 = 456,
	CMDQ_SYNC_TOKEN_APPEND_THR25 = 457,
	CMDQ_SYNC_TOKEN_APPEND_THR26 = 458,
	CMDQ_SYNC_TOKEN_APPEND_THR27 = 459,
	CMDQ_SYNC_TOKEN_APPEND_THR28 = 460,
	CMDQ_SYNC_TOKEN_APPEND_THR29 = 461,
	CMDQ_SYNC_TOKEN_APPEND_THR30 = 462,
	CMDQ_SYNC_TOKEN_APPEND_THR31 = 463,

	/* GPR access tokens (for HW register backup)
	 * There are 15 32-bit GPR, 3 GPR form a set
	 * (64-bit for address, 32-bit for value)
	 */
	CMDQ_SYNC_TOKEN_GPR_SET_0 = 470,
	CMDQ_SYNC_TOKEN_GPR_SET_1 = 471,
	CMDQ_SYNC_TOKEN_GPR_SET_2 = 472,
	CMDQ_SYNC_TOKEN_GPR_SET_3 = 473,
	CMDQ_SYNC_TOKEN_GPR_SET_4 = 474,

	/* Resource lock event to control resource in GCE thread */
	CMDQ_SYNC_RESOURCE_WROT0 = 480,
	CMDQ_SYNC_RESOURCE_WROT1 = 481,

	/**
	 * Event for CMDQ delay implement
	 * Plz sync CMDQ_SYNC_TOKEN_DELAY_THR(id) in cmdq_core source file.
	 */
	CMDQ_SYNC_TOKEN_TIMER = 485,
	CMDQ_SYNC_TOKEN_DELAY_SET0 = 486,
	CMDQ_SYNC_TOKEN_DELAY_SET1 = 487,
	CMDQ_SYNC_TOKEN_DELAY_SET2 = 488,

	/* GCE HW TPR Event*/
	CMDQ_EVENT_TIMER_00 = 962,
	CMDQ_EVENT_TIMER_01 = 963,
	CMDQ_EVENT_TIMER_02 = 964,
	CMDQ_EVENT_TIMER_03 = 965,
	CMDQ_EVENT_TIMER_04 = 966,
	/* 5: 1us */
	CMDQ_EVENT_TIMER_05 = 967,
	CMDQ_EVENT_TIMER_06 = 968,
	CMDQ_EVENT_TIMER_07 = 969,
	/* 8: 10us */
	CMDQ_EVENT_TIMER_08 = 970,
	CMDQ_EVENT_TIMER_09 = 971,
	CMDQ_EVENT_TIMER_10 = 972,
	/* 11: 100us */
	CMDQ_EVENT_TIMER_11 = 973,
	CMDQ_EVENT_TIMER_12 = 974,
	CMDQ_EVENT_TIMER_13 = 975,
	CMDQ_EVENT_TIMER_14 = 976,
	/* 15: 1ms */
	CMDQ_EVENT_TIMER_15 = 977,
	CMDQ_EVENT_TIMER_16 = 978,
	CMDQ_EVENT_TIMER_17 = 979,
	/* 18: 10ms */
	CMDQ_EVENT_TIMER_18 = 980,
	CMDQ_EVENT_TIMER_19 = 981,
	CMDQ_EVENT_TIMER_20 = 982,
	/* 21: 100ms */
	CMDQ_EVENT_TIMER_21 = 983,
	CMDQ_EVENT_TIMER_22 = 984,
	CMDQ_EVENT_TIMER_23 = 985,
	CMDQ_EVENT_TIMER_24 = 986,
	CMDQ_EVENT_TIMER_25 = 987,
	CMDQ_EVENT_TIMER_26 = 988,
	CMDQ_EVENT_TIMER_27 = 989,
	CMDQ_EVENT_TIMER_28 = 990,
	CMDQ_EVENT_TIMER_29 = 991,
	CMDQ_EVENT_TIMER_30 = 992,
	CMDQ_EVENT_TIMER_31 = 993,

	/* event id is 9 bit */
	CMDQ_SYNC_TOKEN_MAX = 0x3FF,
	CMDQ_MAX_EVENT = 0x3FF,
	CMDQ_SYNC_TOKEN_INVALID = -1,
};

/* General Purpose Register */
enum cmdq_gpr_reg {
	/* Value Reg, we use 32-bit */
	/* Address Reg, we use 64-bit */
	/* Note that R0-R15 and P0-P7 actullay share same memory */
	/* and R1 cannot be used. */

	CMDQ_DATA_REG_JPEG = 0x00,	/* R0 */
	CMDQ_DATA_REG_JPEG_DST = 0x11,	/* P1 */

	CMDQ_DATA_REG_PQ_COLOR = 0x04,	/* R4 */
	CMDQ_DATA_REG_PQ_COLOR_DST = 0x13,	/* P3 */

	CMDQ_DATA_REG_2D_SHARPNESS_0 = 0x05,	/* R5 */
	CMDQ_DATA_REG_2D_SHARPNESS_0_DST = 0x14,	/* P4 */

	CMDQ_DATA_REG_2D_SHARPNESS_1 = 0x0a,	/* R10 */
	CMDQ_DATA_REG_2D_SHARPNESS_1_DST = 0x16,	/* P6 */

	CMDQ_DATA_REG_DEBUG = 0x0b,	/* R11 */
	CMDQ_DATA_REG_DEBUG_DST = 0x17,	/* P7 */

	/* sentinel value for invalid register ID */
	CMDQ_DATA_REG_INVALID = -1,
};

struct cmdq_pkt;

struct cmdq_base {
	int	subsys;
	u32	base;
};

struct cmdq_client {
	struct mbox_client client;
	struct mbox_chan *chan;
};

/**
 * cmdq_register_device() - register device which needs CMDQ
 * @dev:	device for CMDQ to access its registers
 *
 * Return: cmdq_base pointer or NULL for failed
 */
struct cmdq_base *cmdq_register_device(struct device *dev);

/**
 * cmdq_mbox_create() - create CMDQ mailbox client and channel
 * @dev:	device of CMDQ mailbox client
 * @index:	index of CMDQ mailbox channel
 *
 * Return: CMDQ mailbox client pointer
 */
struct cmdq_client *cmdq_mbox_create(struct device *dev, int index);

/**
 * cmdq_mbox_destroy() - destroy CMDQ mailbox client and channel
 * @client:	the CMDQ mailbox client
 */
void cmdq_mbox_destroy(struct cmdq_client *client);

/**
 * cmdq_pkt_create() - create a CMDQ packet
 * @pkt_ptr:	CMDQ packet pointer to retrieve cmdq_pkt
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_create(struct cmdq_pkt **pkt_ptr);

/**
 * cmdq_pkt_destroy() - destroy the CMDQ packet
 * @pkt:	the CMDQ packet
 */
void cmdq_pkt_destroy(struct cmdq_pkt *pkt);

/**
 * cmdq_pkt_realloc_cmd_buffer() - reallocate command buffer for CMDQ packet
 * @pkt:	the CMDQ packet
 * @size:	the request size
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_realloc_cmd_buffer(struct cmdq_pkt *pkt, size_t size);

/**
 * cmdq_pkt_read() - append read command to the CMDQ packet
 * @pkt:	the CMDQ packet
 * @base:	the CMDQ base
 * @offset:	register offset from module base
 * @writeAddress:
 * @valueRegId:
 * @destRegId:
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_read(struct cmdq_pkt *pkt,
			struct cmdq_base *base, u32 offset, u32 writeAddress,
			enum cmdq_gpr_reg valueRegId,
			enum cmdq_gpr_reg destRegId);

/**
 * cmdq_pkt_write() - append write command to the CMDQ packet
 * @pkt:	the CMDQ packet
 * @value:	the specified target register value
 * @base:	the CMDQ base
 * @offset:	register offset from module base
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_write(struct cmdq_pkt *pkt, u32 value,
		   struct cmdq_base *base, u32 offset);

/**
 * cmdq_pkt_write_mask() - append write command with mask to the CMDQ packet
 * @pkt:	the CMDQ packet
 * @value:	the specified target register value
 * @base:	the CMDQ base
 * @offset:	register offset from module base
 * @mask:	the specified target register mask
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_write_mask(struct cmdq_pkt *pkt, u32 value,
			struct cmdq_base *base, u32 offset, u32 mask);

/**
 * cmdq_pkt_poll() - append polling command with mask to the CMDQ packet
 * @pkt:	the CMDQ packet
 * @value:	the specified target register value
 * @base:	the CMDQ base
 * @offset:	register offset from module base
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_poll(struct cmdq_pkt *pkt, u32 value,
			 struct cmdq_base *base, u32 offset);

/**
 * cmdq_pkt_poll_t() - append polling command with mask to the CMDQ packet
 * @pkt:	the CMDQ packet
 * @value:	the specified target register value
 * @base:	the CMDQ base
 * @offset:	register offset from module base
 * @mask:	the specified target register mask
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_poll_mask(struct cmdq_pkt *pkt, u32 value,
			struct cmdq_base *base, uint32_t offset, uint32_t mask);

/**
 * cmdq_pkt_wfe() - append wait for event command to the CMDQ packet
 * @pkt:	the CMDQ packet
 * @event:	the desired event type to "wait and CLEAR"
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_wfe(struct cmdq_pkt *pkt, enum cmdq_event event);

/**
 * cmdq_pkt_clear_event() - append clear event command to the CMDQ packet
 * @pkt:	the CMDQ packet
 * @event:	the desired event to be cleared
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_clear_event(struct cmdq_pkt *pkt, enum cmdq_event event);

/**
 * cmdq_pkt_flush() - trigger CMDQ to execute the CMDQ packet
 * @client:	the CMDQ mailbox client
 * @pkt:	the CMDQ packet
 *
 * Return: 0 for success; else the error code is returned
 *
 * Trigger CMDQ to execute the CMDQ packet. Note that this is a
 * synchronous flush function. When the function returned, the recorded
 * commands have been done.
 */
int cmdq_pkt_flush(struct cmdq_client *client, struct cmdq_pkt *pkt);

/**
 * cmdq_pkt_flush_async() - trigger CMDQ to asynchronously execute the CMDQ
 *                          packet and call back at the end of done packet
 * @client:	the CMDQ mailbox client
 * @pkt:	the CMDQ packet
 * @cb:		called at the end of done packet
 * @data:	this data will pass back to cb
 *
 * Return: 0 for success; else the error code is returned
 *
 * Trigger CMDQ to asynchronously execute the CMDQ packet and call back
 * at the end of done packet. Note that this is an ASYNC function. When the
 * function returned, it may or may not be finished.
 */
int cmdq_pkt_flush_async(struct cmdq_client *client, struct cmdq_pkt *pkt,
			 cmdq_async_flush_cb cb, void *data);

#ifdef CMDQ_MEMORY_JUMP
u64 *cmdq_pkt_get_va_by_offset(struct cmdq_pkt *pkt, size_t offset);
dma_addr_t cmdq_pkt_get_pa_by_offset(struct cmdq_pkt *pkt, u32 offset);
#endif

/* debug */
#if 0
extern const u32 cmdq_event_value_8173[CMDQ_MAX_EVENT];
extern const u32 cmdq_event_value_2712[CMDQ_MAX_EVENT];
#endif
extern const u32 cmdq_event_value_common[CMDQ_MAX_EVENT];
extern const u32 *cmdq_event_value;

u32 cmdq_subsys_id_to_base(int id);


struct cmdq_thread_task_info {
	dma_addr_t		pa_base;
	struct cmdq_pkt		*pkt;
	struct list_head	list_entry;
};

struct cmdq_timeout_info {
	u32 irq;
	u32 irq_en;
	dma_addr_t curr_pc;
	u32 *curr_pc_va;
	dma_addr_t end_addr;
	u32 task_num;
	struct cmdq_thread_task_info *timeout_task;
	struct list_head task_list;
};
#endif	/* __MTK_CMDQ_H__ */
