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

#ifndef __HWTCON_CORE_H__
#define __HWTCON_CORE_H__

#include "hwtcon_ioctl_cmd.h"
#include "hwtcon_rect.h"
#include "hwtcon_def.h"

#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/irqreturn.h>
#include <linux/mailbox/mtk-cmdq-mailbox.h>
#include "cmdq_record.h"


enum HWTCON_TASK_STATE {
	TASK_STATE_FREE = 0,
	/* acqure task done, wait for mdp process */
	TASK_STATE_WAIT_MDP_HANDLE = 1,
	/* begin to trigger mdp. */
	TASK_STAT_MDP_DONE = 2,
	/* pipeline is processing */
	TASK_STATE_PIPELINE_PROCESS = 3,
	/* pipeline write done, wait wf_lut display done. */
	TASK_STATE_PIPELINE_DONE = 4,

	/* collision task */
	TASK_STATE_COLLISION = 5,
	/* wf_lut display done, OK to release task to free */
	/* same with TASK_STATE_FREE */
	TASK_STATE_DISPLAYED = 0,
};

/* backup reigser slot index */
enum HWTCON_TASK_SLOT_ENUM {
	SLOT_PIPELINE_ASSIGN_STATUS0 = 0,
	SLOT_PIPELINE_ASSIGN_STATUS1 = 1,
	SLOT_PIPELINE_CUR_HISTOGRAM = 2,
	SLOT_PIPELINE_NXT_HISTOGRAM = 3,
	SLOT_PIPELINE_CUR_GREY = 4,
	SLOT_PIPELINE_NXT_GREY = 5,
	SLOT_PIPELINE_TASK_LUT_ID = 6,
	SLOT_PIPELINE_AUTO_WF_MODE = 7,
	SLOT_PIPELINE_MODIFY_LUT_FLAG0 = 8,
	SLOT_PIPELINE_MODIFY_LUT_FLAG1 = 9,
	SLOT_PIPELINE_ASSIGNED_LUT_FLAG0 = 10,
	SLOT_PIPELINE_ASSIGNED_LUT_FLAG1 = 11,
	SLOT_PIPELINE_WF_LUT_SOF1 = 12,
	SLOT_PIPELINE_WF_LUT_SOF2 = 13,
	SLOT_PIPELINE_PIPELINE_SOF_COUNTER = 14,
	SLOT_PIPELINE_WDMA_EOF_COUNTER = 15,
	SLOT_PIPELINE_WF_LUT_EOF_COUNTER = 16,
	SLOT_PIPELINE_DEBUG_0 = 17,
	SLOT_PIPELINE_DEBUG_1 = 18,
	SLOT_PIPELINE_DEBUG_2 = 19,
	SLOT_PIPELINE_DEBUG_3 = 20,
	SLOT_PIPELINE_DEBUG_4 = 21,
	SLOT_PIPELINE_DEBUG_5 = 22,
	SLOT_PIPELINE_DEBUG_6 = 23,
	SLOT_PIPELINE_DEBUG_7 = 24,
	SLOT_PIPELINE_DEBUG_8 = 25,
	SLOT_PIPELINE_DEBUG_9 = 26,
	SLOT_MAX,
};

enum HWTCON_TASK_COLLSION_INFO_SLOT_ENUM {
	SLOT_COL_LUT0 = 0,
	SLOT_COL_LUT1 = 1,
	SLOT_COL_COUNT = 2,
	SLOT_COL_X = 3,
	SLOT_COL_Y = 4,
	SLOT_COL_WIDTH = 5,
	SLOT_COL_HEIGHT = 6,
	SLOT_COL_DEBUG0 = 7,
	SLOT_COL_DEBUG1 = 8,
	SLOT_COL_DEBUG2 = 9,
	SLOT_COL_DEBUG3 = 10,
	SLOT_COL_DEBUG4 = 11,
	SLOT_COL_DEBUG5 = 12,
	SLOT_COL_MAX,
};

#define HWTCON_WAIT_WF_LUT_RELEASE_TIMEOUT (5000) /* ms */
#define HWTCON_WAIT_DPI_IDLE_TIMEOUT 50 /* ms */
/* max lut collsion count
 * if larger than max count, wait wf_lut handle lut collision done.
 */
#define MAX_LUT_COLLISION_COUNT 64

#define MAX_MERGE_UPDATE_MARKER_NUMBER 100

/* GPT timer 13Mhz
*/
#define GPT_TIMER (0x10008068)

/* this value should between WB_Frame done and wf_lut Frame done */
//#define MODIFY_WF_MODE_POSITION (0x500)
#define MODIFY_WF_MODE_POSITION (0x400)
/* this value should after pipeline sof vcounter */
#define TRIGGER_PIPELINE_POSITION (0x10)

#define DEFAULT_MARKER_COUNT 2048

struct collision_info_struct {
	u64 collision_lut;
	struct rect collision_region;
};

struct update_marker_info {
	u32 buffer_count;
	int count;
	u32 *update_marker_arr;
};

struct hwtcon_task {
	/* passed from userspace. */
	struct mxcfb_update_data update_data;

	/* unique ID passed form userspace */
	struct update_marker_info marker_info;

	/* record collision info */
	struct collision_info_struct collision_info;
	enum HWTCON_TASK_STATE state;

	/* task is assign to which lut_id */
	int lut_id;

	/* task is auto trigger by collsion */
	bool is_collsion_auto_trigger;

	/* task depend on which lut release
	 * bit x = 1: task depend on LUT x release.
	 */
	u64 lut_dependency;

	/* record collision info
	 * collsion_LUT0 & collsion_LUT1 & rect
	*/
	dma_addr_t slot_collision_info;

	/* for backup register */
	dma_addr_t slot;

	struct list_head list;

	/* current task used temperature & temp zone */
	int used_temp;
	int used_temp_zone;

	/* unique ID */
	HWTCON_TIME unique_id;

	/* record time */
	HWTCON_TIME time_submit;
	HWTCON_TIME time_trigger_mdp;
	HWTCON_TIME time_mdp_done;
	HWTCON_TIME time_enable_power;
	HWTCON_TIME time_trigger_pipeline;
	HWTCON_TIME time_pipeline_done;
	HWTCON_TIME time_wf_lut_done;

	/* for work queue release task */
	struct work_struct work_written_done;
	struct work_struct work_display_done;
};

void hwtcon_core_config_timing(struct cmdqRecStruct *pkt);
int hwtcon_core_read_temperature(void);
int hwtcon_core_submit_task(struct mxcfb_update_data *update_data);
int hwtcon_core_wait_for_task_triggered(u32 update_marker);
int hwtcon_core_wait_for_task_displayed(u32 update_marker);
int hwtcon_core_get_wb_index(void);
u32 hwtcon_core_get_waveform_type(void);
bool hwtcon_core_have_collision(const struct rect *rect0,
	const struct rect *rect1);
int hwtcon_core_dispatch_pipeline(void *ignore);
int hwtcon_core_dispatch_mdp(void *ignore);
irqreturn_t hwtcon_core_wb_wdma_irq_handle(int irq, void *dev);
irqreturn_t hwtcon_core_wf_lut_dpi_irq_handle(int irq, void *dev);
irqreturn_t hwtcon_core_wf_lut_irq_handle(int irq, void *dev);
irqreturn_t hwtcon_core_wf_lut_end_irq_handle(int irq, void *dev);
irqreturn_t hwtcon_core_disp_rdma_irq_handle(int irq, void *dev);
irqreturn_t hwtcon_core_pipeline_irq_handle(int irq, void *dev);
void hwtcon_core_handle_mmsys_power_down_cb(unsigned long param);
struct rect hwtcon_core_get_task_region(const struct hwtcon_task *task);
struct rect hwtcon_core_get_mdp_region(
	const struct hwtcon_task *task);
void hwtcon_core_get_task_buffer_info(
	const struct hwtcon_task *task,
	u32 *buffer_pa, u32 *buffer_width, u32 *buffer_height);
void hwtcon_core_get_mdp_input_buffer_info(
	const struct hwtcon_task *task,
	u32 *buffer_pa, u32 *buffer_width, u32 *buffer_height);
int hwtcon_core_convert_temperature(int temp);
int hwtcon_core_read_temperature(void);
int hwtcon_core_read_temp_zone(void);
int hwtcon_core_load_init_setting_from_file(void);
int hwtcon_core_get_task_count(struct list_head *header);

int easy_mtk_mdp_func(
		u32 src_w, u32 src_h,
		u32 dst_w, u32 dst_h,
		u32 src_fmt, u32 dst_fmt,
		u32 src_pitch, u32 dst_pitch,
		dma_addr_t src_mva, dma_addr_t dst_mva,
		u32 crop_x, u32 crop_y,
		u32 crop_w, u32 crop_h,
		u8 dth_en, u32 dth_algo, u8 invert,
		u32 rotate, u32 gamma_flag);
void mdp_update_cmap_lut(u16 *gamma_lut, u32 len);
void hwtcon_core_handle_mmsys_power_down(
	struct work_struct *work_item);
void hwtcon_core_handle_clock_disable(
	struct work_struct *work_item);
void hwtcon_core_handle_task_written_done(
	struct work_struct *work_item);
void hwtcon_core_start_lut_assign_done_trigger_loop(void);
void hwtcon_core_start_lut_assign_done_trigger_loop(void);
void hwtcon_core_stop_lut_assign_done_trigger_loop(void);
void hwtcon_core_start_auto_collision_trigger_loop(void);
void hwtcon_core_stop_auto_collision_trigger_loop(void);
void hwtcon_core_compose_auto_trigger_command(struct cmdqRecStruct *pkt);
void hwtcon_core_compose_assign_task_lut_commmand(
	struct cmdqRecStruct *pkt,
	struct hwtcon_task *task);
bool hwtcon_core_check_task_full_collision(struct hwtcon_task *task,
	u64 *lut_dependency);
struct rect hwtcon_core_get_update_data_region(
	const struct mxcfb_update_data *update_data);




enum WAVEFORM_MODE_ENUM hwtcon_core_calc_wf_mode_from_histogram(
	int night_mode, int full_update);
void hwtcon_core_config_buffer_index(struct cmdqRecStruct *pkt);

char *hwtcon_core_get_wf_mode_name(enum WAVEFORM_MODE_ENUM mode);
bool hwtcon_core_use_night_mode(void);
int hwtcon_core_wait_all_task_done(void);
int hwtcon_core_wait_power_down(void);
void hwtcon_core_compose_histogram_command(
	struct cmdqRecStruct *pkt, struct hwtcon_task *task);

bool hwtcon_core_check_hwtcon_idle(void);


#endif /* __HWTCON_CORE_H__ */
