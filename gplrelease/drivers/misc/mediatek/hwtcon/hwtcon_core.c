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

#include "hwtcon_core.h"
#include "hwtcon_def.h"
#include "hwtcon_fb.h"
#include "hwtcon_paper_top_config.h"
#include "hwtcon_pipeline_config.h"
#include "hwtcon_wdma_config.h"
#include "hwtcon_rdma_config.h"
#include "hwtcon_regal_config.h"
#include "hwtcon_wf_lut_config.h"
#include "hwtcon_dpi_config.h"
#include "hwtcon_tcon_config.h"
#include "hwtcon_wf_lut_rdma_config.h"
#include "hwtcon_debug.h"
#include "hwtcon_file.h"
#include "hwtcon_hal.h"
#include "fiti_core.h"
#include "hwtcon_epd.h"
#include "hwtcon_mdp.h"
#include "mtk_imgrz_ext.h"

#include <linux/list.h>
#include <linux/irqreturn.h>
#include <linux/videodev2.h>
#include <linux/delay.h>

#if defined(CONFIG_KERNEL_MODE_NEON)
#include <asm/neon.h>
extern void do_pixel_line(void *pixel_start, uint32_t *strength,  uint32_t *transition, uint32_t width);
#endif

#define show_resample
#define FIX_CURRENT_RESAMPLE

extern int bd71827_get_battery_soc(void);

/* Check battery and temperature
 * We only do SW mitigation with threshold 49% when
 *       ( Battery<=50% && 0=<Temperature<=15 ) or
 *       ( Battery<=20% && Temperature>15 )
 */
static int is_SW_mitigation_needed(void)
{
	int temp, batt_level;
	int res = 0;

	if (hwtcon_fb_info()->temperature != TEMP_USE_AMBIENT)
		temp = hwtcon_fb_info()->temperature;
	else
		temp = fiti_read_temperature(); /* fiti sensor temp */

	batt_level = bd71827_get_battery_soc();
	if(batt_level <= 50 && temp>=0 && temp <= 15)
		res = 1;
	else if (temp > 15 && batt_level<= 20)
		res =1;
	if(res)
		TCON_LOG("SW mitigation needed temp=%dC battery = %d",temp, batt_level);
	return res;
}

static int hwtcon_pixel_pre_process(struct mxcfb_update_data *upd_data)
{
	unsigned char *pixel_start, pixel0, pixel1;
	uint32_t top, left,width, height, stride, rotate;
	uint32_t row,col, scan_stride, norm_length;
	uint32_t transition[32], transition_total, strength, scan_line;
	uint32_t resample, count, bitmask, rescale_ratio;
	uint32_t s_avg, n_avg, avg;
	uint32_t temp_n = 1, temp_d = 1, temp_adjust = 0;
	uint32_t scaled_width, scaled_height;

        if (hwtcon_fb_info()->sw_algo.scan_lines == 0)
                return 0;

        TCON_LOG("%s: enter",__func__);

        top = upd_data->update_region.top;
        left = upd_data->update_region.left;
        width = upd_data->update_region.width;
        height = upd_data->update_region.height;
        stride = hwtcon_fb_get_virtual_width();
	rotate = hwtcon_fb_get_rotation();

        if (rotate & 1) {
                row = width;
                norm_length = height;
        }
        else {
                row = height; 
                norm_length = width;
        }

	/* make it a odd number to alternate even/odd line */
	scan_stride = ((row / hwtcon_fb_info()->sw_algo.scan_lines) & (-2)) + 1; 
        if (scan_stride < hwtcon_fb_info()->sw_algo.scan_lines)  /* skip short regions */
                return 0;

	if (((width << 1) < hwtcon_fb_get_width()) && ((height << 1) < hwtcon_fb_get_height()))
		return 0;

	strength = 0;
	if (rotate & 1) { /* portrait mode */
		for (col = 0, scan_line = 0; col < width - scan_stride; col += scan_stride, scan_line++) { 
			/* skip the first line on the screen */
			transition[scan_line] = 0;
			/* need to change to fb_buffer_va in the V2 driver */
			pixel_start = hwtcon_fb_info()->mdp_buffer_va + top * stride + left + col;
			for (row = 0; row < height; row += 2) {
				uint32_t diff, tran;
				pixel0 = pixel_start[row * stride];
				pixel1 = pixel_start[(row + 1) * stride];
				diff = abs((int32_t)pixel0 - (uint32_t)pixel1);
				strength += diff;
				tran = diff > ONE_GRAY_LEVEL ? 1 : 0;
				transition[scan_line] += tran;
			}
                        TCON_LOG("portrait transition[%d]=%d strength=%d", scan_line, transition[scan_line], strength);
                }
        }
	else {  /* landscape mode */
#if defined(CONFIG_KERNEL_MODE_NEON)
		if (width < 32 * 2) /* NEON works on 32 pixels in one shot, it's ok to skip lines less than 64 pixels wide */
			return 0;
		kernel_neon_begin();
		for (row = scan_stride, scan_line = 0; row < height; row += scan_stride, scan_line++) {
                        /* skip the first line on the screen */
			transition[scan_line] = 0;
			/* need to change to fb_buffer_va in the V2 driver */
			pixel_start = hwtcon_fb_info()->mdp_buffer_va + (top + row) * stride + left;
			do_pixel_line((void*)pixel_start, &strength, &transition[scan_line], width);
			TCON_LOG("landscape transition neon [%d]=%d strength=%d", scan_line, transition[scan_line], strength);
		}
		kernel_neon_end();
#else
		for (row = scan_stride, scan_line = 0; row < height; row += scan_stride, scan_line++) { 
			/* skip the first line on the screen */
			transition[scan_line] = 0;
			/* need to change to fb_buffer_va in the V2 driver */
			pixel_start = hwtcon_fb_info()->mdp_buffer_va  + (top + row) * stride + left;
			for (col = 0; col < width; col += 2){
				uint32_t diff, tran;
				pixel0 = pixel_start[col];
				pixel1 = pixel_start[col+1];
				diff = abs((int32_t)pixel0 - (int32_t)pixel1);
				strength += diff;
				tran = diff > ONE_GRAY_LEVEL ? 1 : 0;
				transition[scan_line] += tran;
			}
			TCON_LOG("landscape transition[%d]=%d strength=%d", scan_line, transition[scan_line], strength);
		}
#endif
	}

	transition_total = 0;
	count=0;bitmask=0;
	for (row = 0; row < scan_line; row++) {
		if ((transition[row] * 100) >= (hwtcon_fb_info()->sw_algo.pixel_thres * norm_length)){
			/* normalize to 50 in [0 15 0 15 ......] pattern */
			bitmask += 1 << row;
			count++;
		}
		transition_total += transition[row];
	}

	resample = 0;
	/* for low temperature, scale dowm pixel strength and count threshold  */
        if (hwtcon_fb_info()->temperature <= 5){
		temp_n = 3;
		temp_d = 4;
		temp_adjust = 2;
	}

	if (count >= (hwtcon_fb_info()->sw_algo.count_thres - temp_adjust))
		resample = 1;
	else if (count > (hwtcon_fb_info()->sw_algo.count_thres / 2)){
                col = 0b11;/* check if there is any 2 sequential lines */
                for (row = 0; row < hwtcon_fb_info()->sw_algo.scan_lines - 1; row++){
			if (((bitmask & col) >> row) == 0b11){
				resample =1;
				break;
			}
			col <<= 1;
		}
	}
	else if ((strength * 100) >= ((hwtcon_fb_info()->sw_algo.str_thres * norm_length * scan_line * 255 * temp_n) / temp_d))
                /* normalize to 50 in [0 255 0 255 ......] pattern */
                resample = 1;
	
	/* let's work out a resize factor */
	if (resample) {
		/* average strength and transition_total */
		s_avg = 500 * strength / 255 / scan_line / norm_length;
		if (s_avg > 100)
			s_avg = 100;

		n_avg = transition_total * 100 / scan_line / norm_length;
		TCON_LOG("%s: score n_avg= %d s_avg=%d", __func__, n_avg, s_avg);
		avg = (s_avg + n_avg) * hwtcon_fb_info()->sw_algo.scaled_factor; /* ??? normalized to [0 50] */
		if (avg > 50000)
			avg = 50000;
		rescale_ratio = 100 - avg / 1000;
		TCON_LOG("%s: resacle_ratio = %d\n", __func__, rescale_ratio);

		if (rescale_ratio >= 100)
			return 0;

		if (hwtcon_fb_info()->sw_algo.scaled_width == 0 && hwtcon_fb_info()->sw_algo.scaled_height == 0){
			if (rescale_ratio > 60) {
				scaled_width = (hwtcon_fb_get_virtual_width() * rescale_ratio / 100) & ((uint32_t)-2);
				scaled_height = (height * rescale_ratio / 100 ) & ((uint32_t)-2);
			}
			else {
				scaled_width = hwtcon_fb_get_virtual_width() >> 1;
				scaled_height = height >> 1;
			}
		}
		else {
			scaled_width = hwtcon_fb_info()->sw_algo.scaled_width;
			scaled_height = hwtcon_fb_info()->sw_algo.scaled_height;
		}

		/* for low temperature, scale dowm more because longer update time */
		if (hwtcon_fb_info()->temperature <= 5) { 
			scaled_width /= 2;
			scaled_width &= ((uint32_t)-2);
			scaled_height /= 2;
			scaled_height &= ((uint32_t)-2);
		}
		else if (((upd_data->waveform_mode == WAVEFORM_MODE_DU) || 
			(upd_data->waveform_mode == WAVEFORM_MODE_A2)) &&
			(rescale_ratio > 60)) {
				return 0;
		}
			
	}

	TCON_LOG("%s: scan_stride =%d  strength=%d transition=%d count=%d bitmask=0x%x\n", __func__,
		scan_stride, strength, transition_total, count,bitmask);

        if (resample && (scaled_width != 0) && (scaled_height != 0)){
		struct mtk_imgrz_scale_kapi_param down_param, up_param;
		
		down_param.src_info.buf_w = stride;
		down_param.src_info.buf_h = hwtcon_fb_get_height();
		down_param.src_info.pic_w = width;
		down_param.src_info.pic_h = height;
		down_param.src_info.x_offset = left;
		down_param.src_info.y_offset = top;
		down_param.src_info.dma_buf = hwtcon_fb_info()->mdp_buffer_pa; 

		down_param.dst_info.buf_w = down_param.src_info.buf_w;
                down_param.dst_info.buf_h = down_param.src_info.buf_h;
                down_param.dst_info.pic_w = scaled_width;
                down_param.dst_info.pic_h = scaled_height;
                down_param.dst_info.x_offset = 0;
                down_param.dst_info.y_offset = 0;
                down_param.dst_info.dma_buf = hwtcon_fb_info()->tmp_buffer_pa;

		/* scaling down */
                TCON_LOG("doing down sampling stuff -> [%d %d]\n", scaled_width, scaled_height);
		mtk_imgrz_scale(&down_param);
									
		/* scaling up */
		TCON_LOG( "doing up sampling stuff\n");
		up_param.src_info = down_param.dst_info;
		up_param.dst_info = down_param.src_info;
		mtk_imgrz_scale(&up_param);

#if defined(show_resample)
		printk(KERN_ERR"mxc_epdc_fb:SW mitigation count=%d strength=%d res %d %d %d\n",
			count, strength, rescale_ratio, scaled_width, scaled_height);
#endif

	}
	return 0;
}

static struct hwtcon_task_list *hwtcon_core_get_task_list_from_state(
	enum HWTCON_TASK_STATE state)
{
	switch (state) {
	case TASK_STATE_FREE:
		return &hwtcon_fb_info()->free_task_list;
	case TASK_STATE_WAIT_MDP_HANDLE:
		return &hwtcon_fb_info()->wait_for_mdp_task_list;
	case TASK_STAT_MDP_DONE:
		return &hwtcon_fb_info()->mdp_done_task_list;
	case TASK_STATE_PIPELINE_DONE:
		return &hwtcon_fb_info()->pipeline_done_task_list;
	case TASK_STATE_PIPELINE_PROCESS:
		return &hwtcon_fb_info()->pipeline_processing_task_list;
	case TASK_STATE_COLLISION:
		return &hwtcon_fb_info()->collision_task_list;
	default:
		TCON_ERR("invalid task state:%d", state);
		return NULL;
	}

	return NULL;
}

bool hwtcon_core_string_ends_with_gz(char *file_name)
{
	int str_len = strlen(file_name);

	if ((file_name[str_len - 3] == '.') &&
		(file_name[str_len - 2] == 'g') &&
		(file_name[str_len - 1] == 'z'))
		return true;
	return false;
}

static bool hwtcon_core_verify_unzip_buffer(void)
{
	/* this code is debug code, used to check decompress pass
	 * compare decompress buffer with golden buffer.
	 */
	char *file_name = hwtcon_debug_get_info()->golden_file_name;
	char *golden_buffer = NULL;
	int file_size = hwtcon_file_get_size(file_name);
	int i = 0;

	if (file_size == 0) {
		TCON_ERR("read file %s fail", file_name);
		return false;
	}
	golden_buffer = vmalloc(file_size);
	if (golden_buffer == NULL) {
		TCON_ERR("allocate golden buffer fail");
		return false;
	}

	hwtcon_file_read_buffer(file_name, golden_buffer, file_size);
	for (i = 0; i < file_size; i++)
		if (golden_buffer[i] != hwtcon_fb_info()->waveform_va[i])
			break;
	if (i == file_size)
		TCON_ERR("compare pass");
	else
		TCON_ERR("compare fail index:%d 0x%x - 0x%x",
			i,
			hwtcon_fb_info()->waveform_va[i],
			golden_buffer[i]);

	vfree(golden_buffer);

	return (i == file_size);
}

int hwtcon_core_load_init_setting_from_file(void)
{
	int wf_lut_file_size = 0;
	if (!hwtcon_fb_info()->hwtcon_first_call)
		return -1;

	/* load data only exec once */
	wf_lut_file_size = hwtcon_file_get_size(
		hwtcon_driver_get_wf_file_path());
	if (wf_lut_file_size == 0) {
		TCON_ERR("read file:%s fail", hwtcon_driver_get_wf_file_path());
		return -1;
	}
	if (hwtcon_core_string_ends_with_gz(
			hwtcon_driver_get_wf_file_path())) {
		/* wf_lut.gz file, need to decompress */
		char *zip_buffer = vmalloc(wf_lut_file_size);

		if (zip_buffer == NULL) {
			TCON_ERR("allocate unzip buffer fail");
			return -1;
		}

		hwtcon_file_read_buffer(hwtcon_driver_get_wf_file_path(),
			zip_buffer,
			wf_lut_file_size);

		hwtcon_file_unzip_buffer(zip_buffer,
			hwtcon_fb_info()->waveform_va,
			wf_lut_file_size,
			hwtcon_fb_info()->waveform_size);

		if (strlen(hwtcon_debug_get_info()->golden_file_name) != 0) {
			if (hwtcon_core_verify_unzip_buffer())
				TCON_ERR("compare golden pass");
			else
				TCON_ERR("compare golden fail");
		}
		vfree(zip_buffer);
	} else {
		TCON_LOG("read wf_lut.bin size:%d total buffer size:%d",
				wf_lut_file_size,
				hwtcon_fb_info()->waveform_size);
		hwtcon_file_read_buffer(hwtcon_driver_get_wf_file_path(),
			hwtcon_fb_info()->waveform_va,
			wf_lut_file_size);
	}

	hwtcon_fb_info()->hwtcon_first_call = false;
	fiti_setting_get_from_waveform(hwtcon_fb_info()->waveform_va);

	return 0;
}

int hwtcon_core_get_task_count(struct list_head *header)
{
	struct hwtcon_task *task, *tmp;
	int count = 0;

	list_for_each_entry_safe(task, tmp,
		header, list) {
		count++;
	}
	return count;
}

void hwtcon_core_dump_task_list(struct list_head *header)
{

	struct hwtcon_task *task, *tmp;
	int count = 0;

	list_for_each_entry_safe(task, tmp,
		header, list) {
		TCON_ERR("index:%d task:0x%llx", count++, task->unique_id);
	}
}

void hwtcon_core_realloc_marker_info(struct update_marker_info *marker_info, int buffer_count)
{
	int alloc_size = 0;
	u32 *alloc_buf = NULL;

	/* 256 align */
	buffer_count = ((buffer_count >> 8) + 1) << 8;
	alloc_size = buffer_count * sizeof(u32);

	if (buffer_count <= marker_info->buffer_count)
		return;

	TCON_ERR("realloc marker buffer size: %d", alloc_size);

	alloc_buf = vmalloc(alloc_size);
	if (alloc_buf == NULL) {
		TCON_ERR("alloc size:%d fail", alloc_size);
		WARN_ON(1);
		return;
	}

	memcpy(alloc_buf, marker_info->update_marker_arr, marker_info->count * sizeof(u32));
	marker_info->buffer_count = buffer_count;
	vfree(marker_info->update_marker_arr);
	marker_info->update_marker_arr = alloc_buf;
}


enum INSERT_LIST_ENUM {
	INSERT_TO_TAIL = 0,
	INSERT_TO_HEAD = 1,
};
static void hwtcon_core_change_task_state(struct hwtcon_task *task,
	enum HWTCON_TASK_STATE dst_state,
	bool src_lock,
	bool dst_lock,
	enum INSERT_LIST_ENUM insert_position)
{
	enum HWTCON_TASK_STATE src_state = task->state;

	/* which task_list is current task in ? */
	struct hwtcon_task_list *src_task_list =
		hwtcon_core_get_task_list_from_state(src_state);
	struct hwtcon_task_list *dst_task_list =
		hwtcon_core_get_task_list_from_state(dst_state);

	if (dst_state == TASK_STATE_FREE) {
		vfree(task);
		wake_up(&hwtcon_fb_info()->task_state_wait_queue);
		return;
	}

	if (!src_task_list || !dst_task_list) {
		TCON_ERR("change task:0x%llx id:0x%x state[%d]->[%d] fail",
			task->unique_id,
			task->update_data.update_marker,
			src_state,
			dst_state);
		dump_stack();
		return;
	}

	if (src_task_list != dst_task_list) {
		unsigned long flags;

		/* remove task from former task list. */
		if (src_lock)
			spin_lock_irqsave(&src_task_list->lock, flags);
		list_del_init(&task->list);
		if (src_lock)
			spin_unlock_irqrestore(&src_task_list->lock, flags);

		/* add task to current task list. */
		if (dst_lock)
			spin_lock_irqsave(&dst_task_list->lock, flags);
		if (insert_position == INSERT_TO_HEAD)
			list_add(&task->list, &dst_task_list->list);
		else
			list_add_tail(&task->list, &dst_task_list->list);
		if (dst_lock)
			spin_unlock_irqrestore(&dst_task_list->lock, flags);
	}

	#if 0
	TCON_ERR("change task:0x%llx from:%d to %d",
		task->unique_id,
		task->state, dst_state);
	#endif

	/* modify task state */
	task->state = dst_state;
	wake_up(&hwtcon_fb_info()->task_state_wait_queue);
}

void hwtcon_core_put_task_callback(struct hwtcon_task *task)
{
	/* hwtcon_core_put_task */
	queue_work(hwtcon_fb_info()->wq_wf_lut_display_done,
		&task->work_display_done);
}

void hwtcon_core_put_task(struct hwtcon_task *task)
{
	#if 1
	int i = 0;

	/* dump execute time to /proc */
	hwtcon_debug_record_printf(
		"task:0x%llx [%d %d %d %d] mode:%02d->%s wf_cnt:%d ",
		task->unique_id,
		hwtcon_core_get_task_region(task).x,
		hwtcon_core_get_task_region(task).y,
		hwtcon_core_get_task_region(task).width,
		hwtcon_core_get_task_region(task).height,
		task->update_data.waveform_mode,
		hwtcon_core_get_wf_mode_name(task->update_data.waveform_mode),
		wf_lut_get_waveform_len(task->used_temp_zone,
			task->update_data.waveform_mode));
	hwtcon_debug_record_printf(
		"submit %d trigger_mdp %d mdp_done %d power %d trigger_pipeline %d pipeline_done %d wf_lut_done total:%d marker:",
		hwtcon_hal_get_time_in_ms(task->time_submit, task->time_trigger_mdp),
		hwtcon_hal_get_time_in_ms(task->time_trigger_mdp, task->time_mdp_done),
		hwtcon_hal_get_time_in_ms(task->time_mdp_done, task->time_enable_power),
		hwtcon_hal_get_time_in_ms(task->time_enable_power, task->time_trigger_pipeline),
		hwtcon_hal_get_time_in_ms(task->time_trigger_pipeline, task->time_pipeline_done),
		hwtcon_hal_get_time_in_ms(task->time_pipeline_done, task->time_wf_lut_done),
		hwtcon_hal_get_time_in_ms(task->time_submit, task->time_wf_lut_done));
	for (i = 0; i < task->marker_info.count; i++)
		hwtcon_debug_record_printf(" %d", task->marker_info.update_marker_arr[i]);

	hwtcon_debug_record_printf("\n");
	#endif
	if (task->marker_info.update_marker_arr)
		vfree(task->marker_info.update_marker_arr);
	memset(&task->marker_info, 0, sizeof(task->marker_info));

	cmdqBackupFreeSlot(task->slot);
	cmdqBackupFreeSlot(task->slot_collision_info);

	#if 1
	hwtcon_core_change_task_state(task, TASK_STATE_FREE, false, true, INSERT_TO_TAIL);
	#else
	vfree(task);
	#endif
}


void hwtcon_core_handle_task_display_done(struct work_struct *work_item)
{
	struct hwtcon_task *task = container_of(work_item,
	       struct hwtcon_task,
	       work_display_done);

	/* remove task to free task list. */
	hwtcon_core_put_task(task);
}

/* find a free task from free task list,
 * and move task from free_task_list to  mdp_task_list.
 * if can't find free task list.
 * allocate one. and add task to mdp_task_list.
 */
static struct hwtcon_task *hwtcon_core_get_task(void)
{
	struct hwtcon_task *task = NULL;
	unsigned long flags;
	int i = 0;

	#if 0 /* do not use free task list */
	spin_lock_irqsave(&hwtcon_fb_info()->free_task_list.lock, flags);
	task = list_first_entry_or_null(&hwtcon_fb_info()->free_task_list.list,
		struct hwtcon_task, list);
	spin_unlock_irqrestore(&hwtcon_fb_info()->free_task_list.lock, flags);
	#endif

	if (task == NULL) {
		/* no free task, alloate one. */
		task = vzalloc(sizeof(struct hwtcon_task));
		if (task == NULL) {
			TCON_ERR("vmalloc task fail");
			return NULL;
		}
		/* add new task to free task list */
		task->state = TASK_STATE_FREE;
		INIT_LIST_HEAD(&task->list);

		INIT_WORK(&task->work_written_done,
			hwtcon_core_handle_task_written_done);

		INIT_WORK(&task->work_display_done,
			hwtcon_core_handle_task_display_done);
	} else {
		spin_lock_irqsave(&hwtcon_fb_info()->free_task_list.lock, flags);
		list_del_init(&task->list);
		spin_unlock_irqrestore(&hwtcon_fb_info()->free_task_list.lock, flags);
	}

	/* init task info */
	memset(&task->update_data, 0, sizeof(task->update_data));
	memset(&task->collision_info, 0, sizeof(task->collision_info));
	task->marker_info.buffer_count = DEFAULT_MARKER_COUNT;
	task->marker_info.count = 0;
	task->marker_info.update_marker_arr = vmalloc(DEFAULT_MARKER_COUNT * sizeof(u32));
	task->used_temp = 0;
	task->used_temp_zone = 0;

	/* allocate dma buffer for backup register */
	cmdqBackupAllocateSlot(&task->slot, SLOT_MAX);
	for (i = 0; i < SLOT_MAX; i++)
		cmdqBackupWriteSlot(task->slot, i, 0);

	/* allocate dma buffer for backup collision info */
	cmdqBackupAllocateSlot(&task->slot_collision_info, SLOT_COL_MAX);
	for (i = 0; i < SLOT_COL_MAX; i++)
		cmdqBackupWriteSlot(task->slot_collision_info, i, 0);

	task->unique_id = sched_clock();
	task->time_submit = timeofday_ms();
	task->time_trigger_mdp = 0LL;
	task->time_mdp_done = 0LL;
	task->time_enable_power = 0LL;
	task->time_trigger_pipeline = 0LL;
	task->time_pipeline_done = 0LL;
	task->time_wf_lut_done = 0LL;

	task->lut_id = -1;
	task->is_collsion_auto_trigger = false;
	task->lut_dependency = 0LL;

	return task;
}

bool hwtcon_core_check_hwtcon_idle(void)
{
	if (pipeline_get_lut_status() != 0LL)
		return false;

	/* check if all task list Empty */
	if (
		!list_empty(&hwtcon_fb_info()->pipeline_processing_task_list.list) ||
		!list_empty(&hwtcon_fb_info()->pipeline_done_task_list.list) ||
		!list_empty(&hwtcon_fb_info()->mdp_done_task_list.list) ||
		!list_empty(&hwtcon_fb_info()->collision_task_list.list)) {
		TCON_LOG("list not empty, cancel start timer");
		TCON_LOG("pipeline_processing_task_list count:%d",
			hwtcon_core_get_task_count(
			&hwtcon_fb_info()->pipeline_processing_task_list.list));
		TCON_LOG("pipeline_done_task_list count:%d",
			hwtcon_core_get_task_count(
			&hwtcon_fb_info()->pipeline_done_task_list.list));
		TCON_LOG("mdp_done_task_list count:%d",
			hwtcon_core_get_task_count(
			&hwtcon_fb_info()->mdp_done_task_list.list));
		TCON_LOG("collision_task_list count:%d",
			hwtcon_core_get_task_count(
			&hwtcon_fb_info()->collision_task_list.list));
		return false;
	}

	return true;
}

void hwtcon_core_handle_clock_disable(
	struct work_struct *work_item)
{
	struct cmdqRecStruct *pkt = NULL;

	if (hwtcon_core_check_hwtcon_idle() == false)
		return;

	/* wait for DPI Frame done */
	cmdqRecCreate(CMDQ_SCENARIO_HWTCON, &pkt);
	cmdqRecReset(pkt);
	cmdqRecClearEventToken(pkt, CMDQ_EVENT_DPI0_FRAME_DONE);
	cmdqRecWait(pkt, CMDQ_EVENT_DPI0_FRAME_DONE);
	cmdqRecFlush(pkt);
	cmdqRecDestroy(pkt);

	if (hwtcon_core_check_hwtcon_idle() == false)
		return;

	wake_up(&hwtcon_fb_info()->hwtcon_fb_flush_done_wq);

	if (hwtcon_fb_info()->power_down_delay_ms == EINK_NO_POWER_DOWN)
		return;

	TCON_LOG("start timer %d ms to power down mmsys",
		hwtcon_fb_info()->power_down_delay_ms);

	if (hwtcon_fb_info()->power_down_delay_ms)
		mod_timer(&hwtcon_fb_info()->mmsys_power_timer,
			jiffies + msecs_to_jiffies(
				hwtcon_fb_info()->power_down_delay_ms));
	else {
		/* hwtcon_core_handle_mmsys_power_down */
		queue_work(hwtcon_fb_info()->wq_power_down_mmsys,
			&hwtcon_fb_info()->wk_power_down_mmsys);
	}

}

void hwtcon_core_handle_mmsys_power_down(
	struct work_struct *work_item)
{
	hwtcon_driver_enable_mmsys_power(NULL, false);
}

void hwtcon_core_handle_mmsys_power_down_cb(unsigned long param)
{
	/* hwtcon_core_handle_mmsys_power_down */
	queue_work(hwtcon_fb_info()->wq_power_down_mmsys,
		&hwtcon_fb_info()->wk_power_down_mmsys);
}

/* check if the update marker belong to current task. */
static bool hwtcon_core_check_task_contains_update_marker(
	const struct hwtcon_task *task,
	u32 update_marker)
{
	int i = 0;

	for (i = 0; i < task->marker_info.count; i++)
		if (task->marker_info.update_marker_arr[i] == update_marker)
			return true;
	return false;
}

bool hwtcon_core_search_marker(u32 update_marker, enum HWTCON_TASK_STATE state)
{
	struct hwtcon_task *task, *tmp;
	unsigned long flags;
	struct hwtcon_task_list *task_list = hwtcon_core_get_task_list_from_state(state);
	bool task_found = false;

	if (task_list == NULL)
		return false;

	spin_lock_irqsave(&task_list->lock, flags);
	list_for_each_entry_safe(task, tmp,
		&task_list->list, list) {
		if (hwtcon_core_check_task_contains_update_marker(task,
			update_marker)) {
			task_found = true;
			break;
		}
	}
	spin_unlock_irqrestore(&task_list->lock,
			flags);

	return task_found;
}

struct hwtcon_task *hwtcon_core_search_task_from_update_marker(
	u32 update_marker)
{
	struct hwtcon_task *task, *tmp;
	unsigned long flags;

	/* wait_for_mdp_task_list */
	spin_lock_irqsave(&hwtcon_fb_info()->wait_for_mdp_task_list.lock, flags);
	list_for_each_entry_safe(task, tmp,
		&hwtcon_fb_info()->wait_for_mdp_task_list.list, list) {
		if (hwtcon_core_check_task_contains_update_marker(task,
			update_marker)) {
			spin_unlock_irqrestore(&hwtcon_fb_info()->wait_for_mdp_task_list.lock,
				flags);
			return task;
		}
	}
	spin_unlock_irqrestore(&hwtcon_fb_info()->wait_for_mdp_task_list.lock,
			flags);

	/* mdp_done_task_list */
	spin_lock_irqsave(&hwtcon_fb_info()->mdp_done_task_list.lock, flags);
	list_for_each_entry_safe(task, tmp,
		&hwtcon_fb_info()->mdp_done_task_list.list, list) {
		if (hwtcon_core_check_task_contains_update_marker(task,
			update_marker)) {
			spin_unlock_irqrestore(&hwtcon_fb_info()->mdp_done_task_list.lock,
				flags);
			return task;
		}
	}
	spin_unlock_irqrestore(&hwtcon_fb_info()->mdp_done_task_list.lock,
			flags);

	/* pipeline_processing_task_list */
	spin_lock_irqsave(&hwtcon_fb_info()->pipeline_processing_task_list.lock, flags);
	list_for_each_entry_safe(task, tmp,
		&hwtcon_fb_info()->pipeline_processing_task_list.list, list) {
		if (hwtcon_core_check_task_contains_update_marker(task,
			update_marker)) {
			spin_unlock_irqrestore(&hwtcon_fb_info()->pipeline_processing_task_list.lock,
				flags);
			return task;
		}
	}
	spin_unlock_irqrestore(&hwtcon_fb_info()->pipeline_processing_task_list.lock,
			flags);

	/* pipeline_done_task_list */
	spin_lock_irqsave(&hwtcon_fb_info()->pipeline_done_task_list.lock, flags);
	list_for_each_entry_safe(task, tmp,
		&hwtcon_fb_info()->pipeline_done_task_list.list, list) {
		if (hwtcon_core_check_task_contains_update_marker(task,
			update_marker)) {
			spin_unlock_irqrestore(&hwtcon_fb_info()->pipeline_done_task_list.lock,
				flags);
			return task;
		}
	}
	spin_unlock_irqrestore(&hwtcon_fb_info()->pipeline_done_task_list.lock,
			flags);

	/* collision_task_list */
	spin_lock_irqsave(&hwtcon_fb_info()->collision_task_list.lock, flags);
	list_for_each_entry_safe(task, tmp,
		&hwtcon_fb_info()->collision_task_list.list, list) {
		if (hwtcon_core_check_task_contains_update_marker(task,
			update_marker)) {
			spin_unlock_irqrestore(&hwtcon_fb_info()->collision_task_list.lock,
				flags);
			return task;
		}
	}
	spin_unlock_irqrestore(&hwtcon_fb_info()->collision_task_list.lock,
		flags);

	/* not find */
	TCON_WARN("can't find task:%d, maybe already free, ignore:%d",
		update_marker,
		hwtcon_fb_info()->ignore_request);

	return NULL;
}

int hwtcon_core_wait_for_task_triggered(u32 update_marker)
{
	int status = 0;

#if 0
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
#endif
	status = wait_event_timeout(
		hwtcon_fb_info()->task_state_wait_queue,
		(hwtcon_core_search_marker(update_marker, TASK_STATE_WAIT_MDP_HANDLE) == 0) &&
		(hwtcon_core_search_marker(update_marker, TASK_STAT_MDP_DONE) == 0) &&
		(hwtcon_core_search_marker(update_marker, TASK_STATE_PIPELINE_PROCESS) == 0) &&
		(hwtcon_core_search_marker(update_marker, TASK_STATE_COLLISION) == 0),
		msecs_to_jiffies(HWTCON_TASK_WAIT_MARKER_TIMEOUT_MS));

	if (status == 0) {
		TCON_ERR("wait marker:%d timeout.", update_marker);
		return -1;
	}
	return HWTCON_STATUS_OK;
}


int hwtcon_core_wait_for_task_displayed(u32 update_marker)
{
	int status = 0;

	status = wait_event_timeout(
		hwtcon_fb_info()->task_state_wait_queue,
		(hwtcon_core_search_marker(update_marker, TASK_STATE_WAIT_MDP_HANDLE) == 0) &&
		(hwtcon_core_search_marker(update_marker, TASK_STAT_MDP_DONE) == 0) &&
		(hwtcon_core_search_marker(update_marker, TASK_STATE_PIPELINE_PROCESS) == 0) &&
		(hwtcon_core_search_marker(update_marker, TASK_STATE_COLLISION) == 0) &&
		(hwtcon_core_search_marker(update_marker, TASK_STATE_PIPELINE_DONE) == 0),
		msecs_to_jiffies(HWTCON_TASK_WAIT_MARKER_TIMEOUT_MS));

	if (status == 0) {
		TCON_ERR("wait marker:0x%x timeout.", update_marker);
		return -1;
	}
	return HWTCON_STATUS_OK;
}

int hwtcon_core_convert_temperature(int temp)
{
	return wf_lut_waveform_get_temperature_index(temp);
}

int hwtcon_core_read_temperature(void)
{
	int value = hwtcon_fb_info()->temperature;

	if (value == TEMP_USE_AMBIENT) {
		/* temperature need to read from the temperature sensor */
		value = fiti_read_temperature();
	}

	return value;
}

int hwtcon_core_read_temp_zone(void)
{
	hwtcon_core_load_init_setting_from_file();
	return hwtcon_core_convert_temperature(
		hwtcon_core_read_temperature());
}

u32 hwtcon_core_get_waveform_type(void)
{
	return WAVEFORM_TYPE_5BIT;
}

int hwtcon_core_submit_task(struct mxcfb_update_data *update_data)
{
	struct hwtcon_task *task = NULL;

	task = hwtcon_core_get_task();
	if (task == NULL) {
		TCON_ERR("hwtcon_core_get_task fail");
		return HWTCON_STATUS_GET_TASK_FAIL;
	}

	mutex_lock(&hwtcon_fb_info()->update_queue_mutex);

	task->update_data = *update_data;
	task->marker_info.count = 1;
	task->marker_info.update_marker_arr[0] = update_data->update_marker;

	TCON_LOG("SUBMIT:task:0x%llx marker:%d wf_mode:%d(%s)",
		task->unique_id,
		task->update_data.update_marker,
		task->update_data.waveform_mode,
		hwtcon_core_get_wf_mode_name(update_data->waveform_mode));
	TCON_EPDC("[%d] Requested waveforms: mode: 0x%x (%s) __ BW: 0x%x (%s) __ Gray : 0x%x (%s)",
		task->update_data.update_marker,
		task->update_data.waveform_mode,
		hwtcon_core_get_wf_mode_name(task->update_data.waveform_mode),
		task->update_data.hist_bw_waveform_mode,
		hwtcon_core_get_wf_mode_name(task->update_data.hist_bw_waveform_mode),
		task->update_data.hist_gray_waveform_mode,
		hwtcon_core_get_wf_mode_name(task->update_data.hist_gray_waveform_mode));

	if (hwtcon_debug_get_info()->debug[0]) {
		static int count = 0;
		char buffer_name[100] = {0};

		snprintf(buffer_name, sizeof(buffer_name), "/tmp/image_%d.bin",
			count);
		count = (count + 1) % 10;
		hwtcon_file_save_buffer(hwtcon_fb_info()->fb_buffer_va,
			hwtcon_fb_info()->fb_buffer_size, buffer_name);
	}

	TCON_EPDC("[%d] update start marker=%d, start time=%lld",
                task->update_data.update_marker,
                task->update_data.update_marker,
                task->time_submit);

	hwtcon_core_change_task_state(task, TASK_STATE_WAIT_MDP_HANDLE, true, true, INSERT_TO_TAIL);
	if (timer_pending(&hwtcon_fb_info()->mmsys_power_timer))
		del_timer(&hwtcon_fb_info()->mmsys_power_timer);

	wake_up(&hwtcon_fb_info()->mdp_trigger_wait_queue);

	mutex_unlock(&hwtcon_fb_info()->update_queue_mutex);

	return 0;
}

int hwtcon_core_get_wb_index(void)
{
	return paper_get_write_buffer_index();
}

/*
 * 1. remove task from trigger_task_list.
 * 2. add task to wf_lut_task_list.
 * 3. modify task state.
 */
void hwtcon_core_handle_task_written_done(
	struct work_struct *work_item)
{
	/* dump working buffer */
	if (hwtcon_debug_get_info()->enable_dump_buffer) {
		u32 index = paper_get_write_buffer_index();

		WARN_ON(index > 1);
		hwtcon_file_save_buffer(hwtcon_fb_info()->wb_va[index],
			hwtcon_fb_info()->wb_size[index], "/tmp/wb.bin");
	}
}

void hwtcon_core_set_task_region(struct hwtcon_task *task, struct rect region)
{
	struct rect virtual_region = {0};

	switch (hwtcon_fb_get_rotation()) {
	case HWTCON_ROTATE_0:
		virtual_region.x = region.x;
		virtual_region.y = region.y;
		virtual_region.width = region.width;
		virtual_region.height = region.height;
		break;
	case HWTCON_ROTATE_90:
		virtual_region.x = region.y;
		virtual_region.y = hw_tcon_get_edp_width() -
				region.x -
				region.width;
		virtual_region.width = region.height;
		virtual_region.height = region.width;
		break;
	case HWTCON_ROTATE_180:
		virtual_region.x =  hw_tcon_get_edp_width() -
					region.width - region.x;
		virtual_region.y = hw_tcon_get_edp_height() -
					region.height - region.y;
		virtual_region.width = region.width;
		virtual_region.height = region.height;
		break;
	case HWTCON_ROTATE_270:
		virtual_region.x = hw_tcon_get_edp_height() -
					region.y - region.height;
		virtual_region.y = region.x;
		virtual_region.width = region.height;
		virtual_region.height = region.width;
		break;
	default:
		TCON_ERR("invalid rotation:%d", hwtcon_fb_get_rotation());
		return;
	}

	task->update_data.update_region.left = virtual_region.x;
	task->update_data.update_region.top = virtual_region.y;
	task->update_data.update_region.width = virtual_region.width;
	task->update_data.update_region.height = virtual_region.height;

}

struct rect hwtcon_core_get_task_user_region(
	const struct hwtcon_task *task)
{
	struct rect region = {0};
	const struct mxcfb_alt_buffer_data *buffer_data =
		&task->update_data.alt_buffer_data;

	if (task->update_data.flags & EPDC_FLAG_USE_ALT_BUFFER) {
		region.y = buffer_data->alt_update_region.top;
		region.x = buffer_data->alt_update_region.left;
		region.width = buffer_data->alt_update_region.width;
		region.height = buffer_data->alt_update_region.height;
	} else {
		region.y = task->update_data.update_region.top;
		region.x = task->update_data.update_region.left;
		region.width = task->update_data.update_region.width;
		region.height = task->update_data.update_region.height;
	}

	return region;
}

struct rect hwtcon_core_get_update_data_region(
	const struct mxcfb_update_data *update_data)
{
	struct rect region = {0};
	struct mxcfb_rect buffer_region = {0};
	const struct mxcfb_alt_buffer_data *buffer_data =
		&update_data->alt_buffer_data;

	if (update_data->flags & EPDC_FLAG_USE_ALT_BUFFER) {
		buffer_region.top = buffer_data->alt_update_region.top;
		buffer_region.left = buffer_data->alt_update_region.left;
		buffer_region.width = buffer_data->alt_update_region.width;
		buffer_region.height = buffer_data->alt_update_region.height;
	} else {
		buffer_region.top = update_data->update_region.top;
		buffer_region.left = update_data->update_region.left;
		buffer_region.width = update_data->update_region.width;
		buffer_region.height = update_data->update_region.height;
	}

	switch (hwtcon_fb_get_rotation()) {
	case HWTCON_ROTATE_0:
		region.x = buffer_region.left;
		region.y = buffer_region.top;
		region.width = buffer_region.width;
		region.height = buffer_region.height;
		break;
	case HWTCON_ROTATE_270:
		region.x = buffer_region.top;
		region.y = hw_tcon_get_edp_height() -
			buffer_region.left - buffer_region.width;
		region.width = buffer_region.height;
		region.height = buffer_region.width;
		break;
	case HWTCON_ROTATE_180:
		region.x = hw_tcon_get_edp_width() -
			buffer_region.width - buffer_region.left;
		region.y = hw_tcon_get_edp_height() -
			buffer_region.height - buffer_region.top;
		region.width = buffer_region.width;
		region.height = buffer_region.height;
		break;
	case HWTCON_ROTATE_90:
		region.x = hw_tcon_get_edp_width() -
				buffer_region.top -
				buffer_region.height;
		region.y = buffer_region.left;
		region.width = buffer_region.height;
		region.height = buffer_region.width;
		break;
	default:
		TCON_ERR("invalid rotation:%d", hwtcon_fb_get_rotation());
		WARN_ON(1);
		break;
	}

	if (region.x < 0 ||
		region.y < 0 ||
		region.width < 0 ||
		region.height < 0) {
		TCON_ERR("invalid buffer region[%d %d %d %d] rotation:%d region[%d %d %d %d] panel[%d %d] flag:0x%08x",
			buffer_region.left,
			buffer_region.top,
			buffer_region.width,
			buffer_region.height,
			hwtcon_fb_get_rotation(),
			region.x,
			region.y,
			region.width,
			region.height,
			hw_tcon_get_edp_width(),
			hw_tcon_get_edp_height(),
			update_data->flags);
		dump_stack();
	}

	return region;
}

struct rect hwtcon_core_get_task_region(
	const struct hwtcon_task *task)
{
	return hwtcon_core_get_update_data_region(&task->update_data);
}

struct rect hwtcon_core_get_mdp_region(
	const struct hwtcon_task *task)
{
	struct rect region = {0};
	const struct mxcfb_alt_buffer_data *buffer_data =
		&task->update_data.alt_buffer_data;

	if (task->update_data.flags & EPDC_FLAG_USE_ALT_BUFFER) {
		region.x =
			buffer_data->alt_update_region.left;
		region.y =
			buffer_data->alt_update_region.top;
		region.width =
			buffer_data->alt_update_region.width;
		region.height =
			buffer_data->alt_update_region.height;
	} else {
		region.x = task->update_data.update_region.left;
		region.y = task->update_data.update_region.top;
		region.width = task->update_data.update_region.width;
		region.height = task->update_data.update_region.height;
	}
	return region;
}


void hwtcon_core_get_task_buffer_info(
	const struct hwtcon_task *task,
	u32 *buffer_pa, u32 *buffer_width, u32 *buffer_height)
{
	if (buffer_pa)
		*buffer_pa = hwtcon_fb_info()->fb_buffer_pa;
	if (buffer_width)
		*buffer_width = hw_tcon_get_edp_width();
	if (buffer_height)
		*buffer_height = hw_tcon_get_edp_height();
}

void hwtcon_core_get_mdp_input_buffer_info(
	const struct hwtcon_task *task,
	u32 *buffer_pa, u32 *buffer_width, u32 *buffer_height)
{
	if (task->update_data.flags & EPDC_FLAG_USE_ALT_BUFFER) {
		if (buffer_pa)
			*buffer_pa =
				task->update_data.alt_buffer_data.phys_addr;
		if (buffer_width)
			*buffer_width =
				task->update_data.alt_buffer_data.width;
		if (buffer_height)
			*buffer_height =
				task->update_data.alt_buffer_data.height;
	} else {
		if (buffer_pa)
			*buffer_pa = hwtcon_fb_info()->mdp_buffer_pa;

		if (buffer_width) {
			*buffer_width = hwtcon_fb_get_width();
		}
		if (buffer_height)
                        *buffer_height = hwtcon_fb_get_height();
	}
}

/* check whether driver can trigger a new lut region to pipeline
 * 1. pipeline write working buffer done.
 * 2. lut collision count less than MAX_LUT_COLLISION_COUNT
 * 3. if need change image buffer address. all wf_lut must all released.
 */
static int hwtcon_core_check_pipeline_busy(
	const struct hwtcon_task *task)
{
	/* int wb_write_status = 0; */
	u32 collision_count = 0;
	struct rect collision_region = {0};
	struct rect task_region = {0};
	int i = 0;
	u32 predict_lut_count = 0;
	u64 hw_lut_status = 0LL;
	struct rect region = {0};

	task_region = hwtcon_core_get_task_region(task);

	/* check pipeline busy. */
	if (hwtcon_fb_info()->pipeline_busy) {
		/* pipeline busy writing working buffer */
		TCON_LOG("task 0x%llx pipeline busy", task->unique_id);
		return -1;
	}

	pipeline_get_collision_region(&collision_count, &collision_region);

	/* check pipeline collision count. */
	#if 0
	if (collision_count >= MAX_LUT_COLLISION_COUNT) {
		/* too many lut collision,
		 * not allow to trigger more lut to pipeline
		 */
		TCON_LOG("task 0x%llx collison_count:%d too many",
			task->unique_id,
			collision_count);
		return true;
	}
	#else
	/* calculate predict lut count */
	hw_lut_status = pipeline_get_lut_status();
	predict_lut_count = 0;
	for (i = 0; i < MAX_LUT_REGION_COUNT; i++) {
		if (hw_lut_status & BIT_ULL_MASK(i)) {
			predict_lut_count++;
			pipeline_get_unreleased_lut_info(NULL,
				i, NULL, NULL, &region);
			if (hwtcon_rect_have_collision(&region, &task_region))
				predict_lut_count++;
		}
	}
	predict_lut_count++;
	if (predict_lut_count >= 40) {
		TCON_LOG("task 0x%llx [%d %d %d %d] predict:%d too many",
			task->unique_id,
			task_region.x,
			task_region.y,
			task_region.width,
			task_region.height,
			predict_lut_count);
		return -2;
	}
	#endif

	/* need to change image buffer address */
	if (task->update_data.flags & EPDC_FLAG_USE_ALT_BUFFER) {
		u64 collision_lut = pipeline_get_collision_lut_status();
		u64 lut = pipeline_get_lut_status();
		int i = 0;
		struct rect lut_region = {0};
		struct rect task_region = {0};

		if (collision_lut != 0) {
			/* pipeline has lut collision, can't change
			 * image buffer for now, wait.
			 */
			return -3;
		}

		task_region = hwtcon_core_get_task_region(task);
		for (i = 0; i < 64; i++) {
			if (lut & BIT_MASK(i)) {
				/* search all unreleased lut in pipeline. */
				pipeline_get_unreleased_lut_info(NULL, i,
					NULL, NULL, &lut_region);
				if (hwtcon_rect_have_collision(&task_region,
					&lut_region)) {
					/* task region has collision with
					 * current lut region.,
					 * need to wait collision
					 * region update done.
					 */
					return -4;
				}
			}
		}
	}

	return 0;
}

static void hwtcon_core_config_paper_top_sof(struct cmdqRecStruct *pkt,
	enum MAIN_SOF_MODE_ENUM main_sof_mode,
	enum WF_LUT_SOF_SEL_ENUM wf_lut_sof_sel,
	enum LUT_MERGE_SOF_SEL_ENUM lut_merge_sof_sel,
	int lut_merge_sof_position,
	int wf_lut_sof_position,
	int pipeline_sof_position)
{
	int max_sof_position = lut_merge_sof_position > wf_lut_sof_position ?
		lut_merge_sof_position : wf_lut_sof_position;

	max_sof_position = max_sof_position > pipeline_sof_position ?
		max_sof_position : pipeline_sof_position;
	max_sof_position += 0x100;

	/* main sof select from mode. */
	paper_config_main_sof_mode(pkt, main_sof_mode);

	/* DPI VSYNC triggered by working buffer WDMA sof */
	paper_config_dpi_vsync_trigger_mode(pkt, DPI_VSYNC_SEL_AUTO);

	/* wf_lut sof position */
	paper_config_wf_lut_sof_position(pkt, wf_lut_sof_position);

	/* lut merge sof position. must ready before wf_lut work,
	 * so must before than wf_lut sof
	 */
	paper_config_lut_merge_sof_position(pkt, lut_merge_sof_position);

	/* pipeline sof position */
	paper_config_pipeline_sof_position(pkt, pipeline_sof_position);

	paper_config_main_sof_max_counter(pkt, max_sof_position);

	/* config hw sof select from HW or software  */
	/* SOF_SEL only need to update bit 6 / 7, others not care for now. */
	paper_config_sof_sel(pkt,
		IMG_RDMA_SOF_SEL_AUTO,
		WB_RDMA_SOF_SEL_AUTO,
		WB_WDMA_SOF_SEL_AUTO,
		PIPELINE_SOF_SEL_AUTO,
		wf_lut_sof_sel,
		lut_merge_sof_sel);
}

static void hwtcon_core_config_wb_wdma(struct cmdqRecStruct *pkt,
	int panel_width, int panel_height, u32 addr)
{

	wdma_reset_hw(pkt);
	wdma_config_color_format(pkt);
	//wdma_config_fifo(pkt);
	wdma_config_enable_ultra(pkt, true);
	wdma_config_enable_preultra(pkt, true);
	wdma_config_buffer_pitch(pkt, panel_width);
	wdma_config_buffer_size(pkt, panel_width, panel_height);
	wdma_config_crop_size(pkt, 0, 0, panel_width, panel_height);
	wdma_config_buffer_addr(pkt, addr);
	wdma_config_enable_interrupt(pkt, true);
	wdma_enable_hw(pkt);
}

int hwtcon_core_wait_all_task_done(void)
{
	int status = 0;

	status = wait_event_timeout(
			hwtcon_fb_info()->task_state_wait_queue,
			(list_empty(&hwtcon_fb_info()->wait_for_mdp_task_list.list) &&
			list_empty(&hwtcon_fb_info()->mdp_done_task_list.list) &&
			list_empty(&hwtcon_fb_info()->pipeline_processing_task_list.list) &&
			list_empty(&hwtcon_fb_info()->pipeline_done_task_list.list) &&
			//(pipeline_get_lut_status() == 0LL) &&
			list_empty(&hwtcon_fb_info()->collision_task_list.list)),
			msecs_to_jiffies(HWTCON_TASK_WAIT_MARKER_TIMEOUT_MS));
	/* wait timeout */
	if (status == 0) {
		TCON_ERR("wait all task done timeout count[%d %d %d %d %d] pipeline status:0x%016llx",
			hwtcon_core_get_task_count(&hwtcon_fb_info()->wait_for_mdp_task_list.list),
			hwtcon_core_get_task_count(&hwtcon_fb_info()->mdp_done_task_list.list),
			hwtcon_core_get_task_count(&hwtcon_fb_info()->pipeline_processing_task_list.list),
			hwtcon_core_get_task_count(&hwtcon_fb_info()->pipeline_done_task_list.list),
			hwtcon_core_get_task_count(&hwtcon_fb_info()->collision_task_list.list),
			pipeline_get_lut_status());
		return -1;
	}
	return 0;

}

enum WAVEFORM_MODE_ENUM hwtcon_core_update_collision_task_wf_mode(
	enum WAVEFORM_MODE_ENUM old_wf_mode)
{
	switch (old_wf_mode) {
	case WAVEFORM_MODE_GC16:
		return WAVEFORM_MODE_GC16_PARTIAL;
	case WAVEFORM_MODE_GCK16:
		return WAVEFORM_MODE_GCK16_PARTIAL;
	default:
		return old_wf_mode;
	}
	return old_wf_mode;
}

bool hwtcon_core_can_merge_trigger_task_region(const struct rect *rect1,
	const struct rect *rect2,
	struct rect *merge_region)
{
	if (hwtcon_rect_check_relationship(rect1, rect2, merge_region) == RECT_RELATION_CONTAIN)
		return true;

	return false;
}

bool hwtcon_core_can_merge_collision_task_region(const struct rect *rect1,
	const struct rect *rect2,
	struct rect *merge_region)
{

	if (hwtcon_rect_check_relationship(rect1, rect2, merge_region) == RECT_RELATION_INTERSECT ||
		hwtcon_rect_check_relationship(rect1, rect2, merge_region) == RECT_RELATION_CONTAIN)
		return true;

	return false;
}

void hwtcon_core_insert_task_to_collision_task_list(struct hwtcon_task *insert_task, bool lock)
{
	unsigned long flags;
	bool find_a_merge = true;
	struct hwtcon_task *collision_task, *tmp = NULL;

	if (lock)
		spin_lock_irqsave(&hwtcon_core_get_task_list_from_state(insert_task->state)->lock, flags);
	list_del_init(&insert_task->list);
	if (lock)
		spin_unlock_irqrestore(&hwtcon_core_get_task_list_from_state(insert_task->state)->lock, flags);

	spin_lock_irqsave(&hwtcon_fb_info()->collision_task_list.lock, flags);
	#if 1
	/* merge */
	while (find_a_merge) {
		find_a_merge = false;
		list_for_each_entry_safe(collision_task, tmp,
			&hwtcon_fb_info()->collision_task_list.list, list) {
			struct rect task_region = hwtcon_core_get_task_region(insert_task);
			struct rect collision_task_region = hwtcon_core_get_task_region(collision_task);
			struct rect merge_region = {0};

			if (hwtcon_core_can_merge_collision_task_region(&task_region, &collision_task_region, &merge_region)) {
				int i = 0;

				find_a_merge = true;
				/* merge task & collision_task to task */
				hwtcon_core_set_task_region(insert_task, merge_region);

				if (insert_task->update_data.waveform_mode != collision_task->update_data.waveform_mode)
					insert_task->update_data.waveform_mode = WAVEFORM_MODE_AUTO;
				insert_task->lut_dependency |= collision_task->lut_dependency;


				if (collision_task->marker_info.count + insert_task->marker_info.count >= insert_task->marker_info.buffer_count)
					hwtcon_core_realloc_marker_info(&insert_task->marker_info,
						collision_task->marker_info.count + insert_task->marker_info.count);

				for(i = 0; i < collision_task->marker_info.count; i++)
					insert_task->marker_info.update_marker_arr[insert_task->marker_info.count++] =
						collision_task->marker_info.update_marker_arr[i];

				TCON_LOG("merge collision region[%d %d %d %d]",
					hwtcon_core_get_task_region(collision_task).x,
					hwtcon_core_get_task_region(collision_task).y,
					hwtcon_core_get_task_region(collision_task).width,
					hwtcon_core_get_task_region(collision_task).height);

				list_del_init(&collision_task->list);
				hwtcon_core_put_task_callback(collision_task);
			}
		}
	}
	#endif
	TCON_EPDC("insert task: 0x%llx [%d %d %d %d] to collision list lut_dependency:0x%016llx",
		insert_task->unique_id,
		hwtcon_core_get_task_region(insert_task).x,
		hwtcon_core_get_task_region(insert_task).y,
		hwtcon_core_get_task_region(insert_task).width,
		hwtcon_core_get_task_region(insert_task).height,
		insert_task->lut_dependency);
	/* add task to collsion task list*/
	//list_add_tail(&insert_task->list, &hwtcon_fb_info()->collision_task_list.list);
	hwtcon_core_change_task_state(insert_task, TASK_STATE_COLLISION, lock, false, INSERT_TO_TAIL);

	spin_unlock_irqrestore(&hwtcon_fb_info()->collision_task_list.lock, flags);
}

void hwtcon_core_insert_task_to_mdp_done_task_list(struct hwtcon_task *insert_task, bool lock,
	enum INSERT_LIST_ENUM insert_type)
{
	unsigned long flags;
	struct hwtcon_task *task, *tmp = NULL;

	if (lock)
		spin_lock_irqsave(&hwtcon_core_get_task_list_from_state(insert_task->state)->lock, flags);
	list_del_init(&insert_task->list);
	if (lock)
		spin_unlock_irqrestore(&hwtcon_core_get_task_list_from_state(insert_task->state)->lock, flags);

	spin_lock_irqsave(&hwtcon_fb_info()->mdp_done_task_list.lock, flags);

	if (insert_type == INSERT_TO_TAIL) {
		#if 1
		/* merge */
		list_for_each_entry_safe_reverse(task, tmp,
			&hwtcon_fb_info()->mdp_done_task_list.list, list) {
			struct rect task_region = hwtcon_core_get_task_region(task);
			struct rect insert_task_region = hwtcon_core_get_task_region(insert_task);
			struct rect merge_region = {0};

			if (hwtcon_core_can_merge_trigger_task_region(&task_region, &insert_task_region, &merge_region)) {
				int i = 0;

				/* merge task & collision_task to task */
				hwtcon_core_set_task_region(insert_task, merge_region);

				if (insert_task->update_data.waveform_mode != task->update_data.waveform_mode)
					insert_task->update_data.waveform_mode = WAVEFORM_MODE_AUTO;
				insert_task->lut_dependency = 0LL;

				if (task->marker_info.count + insert_task->marker_info.count >= insert_task->marker_info.buffer_count)
					hwtcon_core_realloc_marker_info(&insert_task->marker_info,
						task->marker_info.count + insert_task->marker_info.count);
				for(i = 0; i < task->marker_info.count; i++)
					insert_task->marker_info.update_marker_arr[insert_task->marker_info.count++] =
						task->marker_info.update_marker_arr[i];
				list_del_init(&task->list);
				hwtcon_core_put_task_callback(task);
			} else
				break;
		}
		#endif
		/* insert trigger task to mdp_done_task_list */
		//list_add_tail(&insert_task->list, &hwtcon_fb_info()->mdp_done_task_list.list);
		hwtcon_core_change_task_state(insert_task, TASK_STAT_MDP_DONE, false, false, INSERT_TO_TAIL);
	}else {
		#if 1
		/* merge */
		list_for_each_entry_safe(task, tmp,
			&hwtcon_fb_info()->mdp_done_task_list.list, list) {
			struct rect task_region = hwtcon_core_get_task_region(task);
			struct rect insert_task_region = hwtcon_core_get_task_region(insert_task);
			struct rect merge_region = {0};

			if (hwtcon_core_can_merge_trigger_task_region(&task_region, &insert_task_region, &merge_region)) {
				int i = 0;

				if (task->marker_info.count + insert_task->marker_info.count >= insert_task->marker_info.buffer_count) {
					/* can't allocate buffer in interrupt context
					 * stop merge task.
					 */
					break;
				}

				/* merge task & collision_task to task */
				hwtcon_core_set_task_region(insert_task, merge_region);

				if (insert_task->update_data.waveform_mode != task->update_data.waveform_mode)
					insert_task->update_data.waveform_mode = WAVEFORM_MODE_AUTO;
				insert_task->lut_dependency = 0LL;


				#if 0
				if (task->marker_info.count + insert_task->marker_info.count >= insert_task->marker_info.buffer_count) {
					hwtcon_core_realloc_marker_info(&insert_task->marker_info,
						task->marker_info.count + insert_task->marker_info.count);
				}
				#endif
				for(i = 0; i < task->marker_info.count; i++)
					insert_task->marker_info.update_marker_arr[insert_task->marker_info.count++] =
						task->marker_info.update_marker_arr[i];
				list_del_init(&task->list);
				hwtcon_core_put_task_callback(task);
			} else
				break;
		}
		#endif
		/* insert trigger task to mdp_done_task_list */
		//list_add(&insert_task->list, &hwtcon_fb_info()->mdp_done_task_list.list);
		hwtcon_core_change_task_state(insert_task, TASK_STAT_MDP_DONE, false, false, INSERT_TO_HEAD);
	}
	spin_unlock_irqrestore(&hwtcon_fb_info()->mdp_done_task_list.lock, flags);
}

void hwtcon_core_create_collision_task(struct hwtcon_task *task)
{
	struct hwtcon_task *collision_task = NULL;
	int i = 0;

	/* no collision */
	if (task->collision_info.collision_region.height == 0 ||
		task->collision_info.collision_region.width == 0)
		return;
	/* have collision
	 * create a new collision task
	 * 1. force new task waveform mode to partial
	 * 2. new task region is collision region
	 * 3. copy new task marker from task, delete current task marker info
	 * 4. new task->lut = -1
	 * 5. new task->is_auto_trigger = true
	 * 6. new task->lut_dependency
	 */
	collision_task = hwtcon_core_get_task();
	if (collision_task == NULL) {
		TCON_ERR("create new task fail");
		dump_stack();
		return;
	}
	collision_task->update_data = task->update_data;
	collision_task->update_data.update_mode = UPDATE_MODE_PARTIAL;

	#if 1
	hwtcon_core_set_task_region(collision_task, task->collision_info.collision_region);
	#else
	collision_task->update_data.update_region.left =
		task->collision_info.collision_region.x;
	collision_task->update_data.update_region.top =
		task->collision_info.collision_region.y;
	collision_task->update_data.update_region.width =
		task->collision_info.collision_region.width;
	collision_task->update_data.update_region.height =
		task->collision_info.collision_region.height;
	#endif

	collision_task->update_data.waveform_mode =
		hwtcon_core_update_collision_task_wf_mode(task->update_data.waveform_mode);

	if (task->marker_info.count >= collision_task->marker_info.buffer_count)
		hwtcon_core_realloc_marker_info(&collision_task->marker_info,
			task->marker_info.count);
	collision_task->marker_info.count = task->marker_info.count;
	for (i = 0; i < task->marker_info.count; i++)
		collision_task->marker_info.update_marker_arr[i] =
			task->marker_info.update_marker_arr[i];
	task->marker_info.count = 0;

	collision_task->lut_id = -1;
	collision_task->is_collsion_auto_trigger = true;
	collision_task->lut_dependency = task->collision_info.collision_lut | (1LL << task->lut_id);

	TCON_LOG("create collision task region[%d %d %d %d] lut_dependency:0x%016llx wf_mode:%s",
		collision_task->update_data.update_region.left,
		collision_task->update_data.update_region.top,
		collision_task->update_data.update_region.width,
		collision_task->update_data.update_region.height,
		collision_task->lut_dependency,
		hwtcon_core_get_wf_mode_name(collision_task->update_data.waveform_mode));
	hwtcon_core_insert_task_to_collision_task_list(collision_task, false);
	return;
}

static int hwtcon_core_wait_all_wf_lut_release(void)
{
	int status = 0;

	/* wait all wf_lut release */
	status = wait_event_timeout(
			hwtcon_fb_info()->wf_lut_release_wait_queue,
			(pipeline_get_lut_status() == 0L),
			msecs_to_jiffies(HWTCON_WAIT_WF_LUT_RELEASE_TIMEOUT));
	/* wait timeout */
	if (status == 0) {
		TCON_ERR("wait timeout, lut status:0x%016llx",
			pipeline_get_lut_status());
		TCON_ERR("1400B100:0x%08x", pp_read_pa(0x1400B100));
		TCON_ERR("1400B104:0x%08x", pp_read_pa(0x1400B104));
		TCON_ERR("1400424C:0x%08x", pp_read_pa(0x1400424c));
		TCON_ERR("14004250:0x%08x", pp_read_pa(0x14004250));
		TCON_ERR("14004254:0x%08x", pp_read_pa(0x14004254));
		TCON_ERR("14004258:0x%08x", pp_read_pa(0x14004258));
		return -1;
	}

	TCON_LOG("wait all lut release done 0x%016llx",
		pipeline_get_lut_status());

	return 0;
}

int hwtcon_core_wait_power_down(void)
{
	int status = 0;
	int timeout_ms = HWTCON_TASK_WAIT_MARKER_TIMEOUT_MS +
		hwtcon_fb_info()->power_down_delay_ms;

	status = wait_event_timeout(
			hwtcon_fb_info()->power_state_change_wq,
			(hwtcon_fb_info()->mmsys_power_enable == false),
			msecs_to_jiffies(timeout_ms));
	if (status == 0) {
		TCON_ERR("wait power down timeout:%d timer:%d",
			hwtcon_fb_info()->mmsys_power_enable,
			timeout_ms);
		hwtcon_driver_enable_mmsys_power(NULL, false);
		return -1;
	}
	return 0;
}

bool hwtcon_core_use_night_mode(void)
{
	return (hwtcon_fb_get_grayscale() == GRAYSCALE_8BIT_INVERTED);
}

int hwtcon_core_get_waveform_mode_index(
	enum WAVEFORM_MODE_ENUM wf_mode,
	int night_mode)
{
	if (wf_mode == WAVEFORM_MODE_AUTO)
		return 0xF;

	if (!night_mode)	/* day mode */
		switch (wf_mode) {
		case WAVEFORM_MODE_INIT:
			return 0;
		case WAVEFORM_MODE_DU:
			return 1;
		case WAVEFORM_MODE_GC16:
			return 2;
		case WAVEFORM_MODE_GC16_PARTIAL:
			return 3;
		case WAVEFORM_MODE_GL16:
			return 4;
		case WAVEFORM_MODE_GLR16:
			return 5;
		case WAVEFORM_MODE_A2:
			return 6;
		default:
			TCON_ERR("invalid waveform mode:%d->%s for day mode",
				wf_mode,
				hwtcon_core_get_wf_mode_name(wf_mode));
			return 0;	/* force white screen */
		}
	else	/* night mode */
		switch (wf_mode) {
		case WAVEFORM_MODE_INIT:
			return 0;
		case WAVEFORM_MODE_DU:
			return 1;
		case WAVEFORM_MODE_GCK16:
			return 2;
		case WAVEFORM_MODE_GCK16_PARTIAL:
			return 3;
		case WAVEFORM_MODE_GLKW16:
			return 4;
		//case WAVEFORM_MODE_GLKW16:
		//	return 5;
		case WAVEFORM_MODE_A2:
			return 6;
		default:
			TCON_ERR("invalid waveform mode:%d->%s for night mode",
				wf_mode,
				hwtcon_core_get_wf_mode_name(wf_mode));
			return 0;	/* force white screen */
		}

	return 0;
}

int hwtcon_core_get_waveform_mode(
	int slot_number,
	int night_mode)
{
	if (!night_mode)	/* day mode */
		switch (slot_number) {
		case 0:
			return WAVEFORM_MODE_INIT;
		case 1:
			return WAVEFORM_MODE_DU;
		case 2:
			return WAVEFORM_MODE_GC16;
		case 3:
			return WAVEFORM_MODE_GC16_PARTIAL;
		case 4:
			return WAVEFORM_MODE_GL16;
		case 5:
			return WAVEFORM_MODE_GLR16;
		case 6:
			return WAVEFORM_MODE_A2;
		case 7:
			return WAVEFORM_MODE_AUTO;
		default:
			TCON_ERR("invalid waveform slot:%d night_mode:%d",
				slot_number,
				night_mode);
			return -1;
		}
	else	/* night mode */
		switch (slot_number) {
		case 0:
			return WAVEFORM_MODE_INIT;
		case 1:
			return WAVEFORM_MODE_DU;
		case 2:
			return WAVEFORM_MODE_GCK16;
		case 3:
			return WAVEFORM_MODE_GCK16_PARTIAL;
		case 4:
			return WAVEFORM_MODE_GLKW16;
		case 5:
			return WAVEFORM_MODE_GLKW16;
		case 6:
			return WAVEFORM_MODE_A2;
		default:
			TCON_ERR("invalid waveform slot:%d night_mode:%d",
				slot_number,
				night_mode);
			return -1;	/* force white screen */
		}

	return -1;
}

bool hwtcon_core_use_regal(struct hwtcon_task *task,
	enum REGAL_MODE_ENUM *regal_mode)
{
	switch (task->update_data.waveform_mode) {
	case WAVEFORM_MODE_REAGLD:
		*regal_mode = REGAL_MODE_REGAL_D;
		return true;
	case WAVEFORM_MODE_REAGL:
		*regal_mode = REGAL_MODE_REGAL;
		return true;
	case WAVEFORM_MODE_GLKW16:
		*regal_mode = REGAL_MODE_DRAK;
		return true;
	default:
		return false;
	}
	return false;
}

int hwtcon_core_get_task_assigned_info(struct hwtcon_task *task)
{
	int i = 0;
	u64 assigned_lut = pipeline_get_assigned_lut_status();
	u32 assign_lut_reg_dump[2] = {0};
	u32 priority;
	enum WAVEFORM_MODE_ENUM mode;
	struct rect region[MAX_LUT_REGION_COUNT] = {{0}};
	struct rect region_1;
	struct rect task_region = hwtcon_core_get_task_region(task);
	u32 dpi_vcounter[2] = {0};

	cmdqBackupReadSlot(task->slot, SLOT_PIPELINE_ASSIGN_STATUS0,
		&assign_lut_reg_dump[0]);
	cmdqBackupReadSlot(task->slot, SLOT_PIPELINE_ASSIGN_STATUS1,
		&assign_lut_reg_dump[1]);
	assigned_lut = (u64)assign_lut_reg_dump[0] |
		((u64)assign_lut_reg_dump[1] << 32);
	dpi_vcounter[0] = pp_read(hwtcon_driver_get_wf_lut_dpi_va() + 0x040);
	for (i = 0; i < MAX_LUT_REGION_COUNT; i++) {
		if (assigned_lut & BIT_ULL_MASK(i)) {
			/* lut i written done */
			pipeline_get_unreleased_lut_info(NULL, i,
				&priority,
				&mode,
				&region[i]);
			if (hwtcon_rect_compare(&region[i], &task_region)) {
				task->lut_id = i;
				return 0;
			}
		}
	}

	TCON_ERR("task:0x%llx [%d %d %d %d] waveform:%d assign lut not find 0x%016llx",
		task->unique_id,
		task_region.x,
		task_region.y,
		task_region.width,
		task_region.height,
		task->update_data.waveform_mode,
		pipeline_get_lut_status());
	TCON_ERR("dump all assigned lut assigned_lut:0x%016llx", assigned_lut);
	for (i = 0; i < MAX_LUT_REGION_COUNT; i++) {
		if (assigned_lut & BIT_ULL_MASK(i)) {
			/* lut i written done */
			pipeline_get_unreleased_lut_info(NULL, i,
				&priority,
				&mode,
				&region_1);
			TCON_ERR("assign lut:0x%llx id:%d mode:%d region[%d %d %d %d]",
				assigned_lut,
				i,
				mode,
				region_1.x, region_1.y,
				region_1.width, region_1.height);
		}
	}
	dpi_vcounter[1] = pp_read(hwtcon_driver_get_wf_lut_dpi_va() + 0x040);

	for (i = 0; i < MAX_LUT_REGION_COUNT; i++) {
		TCON_ERR("assign lut: id:%d region[%d %d %d %d]",
				i,
				region[i].x, region[i].y,
				region[i].width, region[i].height);
	}
	TCON_ERR("dump all assigned lut end vcounter:%d %d", dpi_vcounter[0], dpi_vcounter[1]);
	return -1;
}

s32 hwtcon_core_handle_pipeline_done(unsigned long data)
{
	unsigned long flags;
	enum WAVEFORM_MODE_ENUM wf_mode;
	struct hwtcon_task *task = (struct hwtcon_task *)data;
	u32 slot_reg[SLOT_MAX] = {0};
	u32 auto_slot_reg[SLOT_FB_MAX] = {0};
	u32 collision_slot_reg[SLOT_COL_MAX] = {0};
	u32 slot_index = 0;
	u32 read_buffer_index = 0;
	int i = 0;

	for (i = 0; i < SLOT_MAX; i++)
		cmdqBackupReadSlot(task->slot, i, &slot_reg[i]);
	for (i = 0; i < SLOT_FB_MAX; i++)
		cmdqBackupReadSlot(hwtcon_fb_info()->slot_auto_waveform_info, i, &auto_slot_reg[i]);
	for (i = 0; i < SLOT_COL_MAX; i++)
		cmdqBackupReadSlot(task->slot_collision_info, i, &collision_slot_reg[i]);

	task->collision_info.collision_lut = (u64)collision_slot_reg[SLOT_COL_LUT1] << 32 |
		collision_slot_reg[SLOT_COL_LUT0];
	task->collision_info.collision_region.x = collision_slot_reg[SLOT_COL_X];
	task->collision_info.collision_region.y = collision_slot_reg[SLOT_COL_Y];
	task->collision_info.collision_region.width = collision_slot_reg[SLOT_COL_WIDTH];
	task->collision_info.collision_region.height = collision_slot_reg[SLOT_COL_HEIGHT];

	cmdqBackupReadSlot(hwtcon_fb_info()->slot_read_buffer_index, 0, &read_buffer_index);

	TCON_LOG("SOF:%d %d index:%d vcounter[0x%08x 0x%08x 0x%08x] debug[0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x]",
		slot_reg[SLOT_PIPELINE_WF_LUT_SOF1],
		slot_reg[SLOT_PIPELINE_WF_LUT_SOF2],
		read_buffer_index,
		slot_reg[SLOT_PIPELINE_PIPELINE_SOF_COUNTER],
		slot_reg[SLOT_PIPELINE_WDMA_EOF_COUNTER],
		slot_reg[SLOT_PIPELINE_WF_LUT_EOF_COUNTER],
		slot_reg[SLOT_PIPELINE_DEBUG_0],
		slot_reg[SLOT_PIPELINE_DEBUG_1],
		slot_reg[SLOT_PIPELINE_DEBUG_2],
		slot_reg[SLOT_PIPELINE_DEBUG_3],
		slot_reg[SLOT_PIPELINE_DEBUG_4],
		slot_reg[SLOT_PIPELINE_DEBUG_5],
		slot_reg[SLOT_PIPELINE_DEBUG_6],
		slot_reg[SLOT_PIPELINE_DEBUG_7],
		slot_reg[SLOT_PIPELINE_DEBUG_8],
		slot_reg[SLOT_PIPELINE_DEBUG_9]);

	for (i = 0; i < SLOT_MAX; i++)
		TCON_LOG("gce read slot[%d] = 0x%08x",
			i, slot_reg[i]);
	TCON_LOG("COL info:count:%d [%d %d %d %d] col reg[0x%08x] [0x%08x] running_LUT:0x%08x 0x%08x debug[0x%08x][0x%08x][0x%08x][0x%08x][0x%08x][0x%08x]",
		collision_slot_reg[SLOT_COL_COUNT],
		collision_slot_reg[SLOT_COL_X],
		collision_slot_reg[SLOT_COL_Y],
		collision_slot_reg[SLOT_COL_WIDTH],
		collision_slot_reg[SLOT_COL_HEIGHT],
		collision_slot_reg[SLOT_COL_LUT0],
		collision_slot_reg[SLOT_COL_LUT1],
		pp_read(PIPELINT_LUT_STATUS1_VA),
		pp_read(PIPELINT_LUT_STATUS0_VA),
		collision_slot_reg[SLOT_COL_DEBUG0],
		collision_slot_reg[SLOT_COL_DEBUG1],
		collision_slot_reg[SLOT_COL_DEBUG2],
		collision_slot_reg[SLOT_COL_DEBUG3],
		collision_slot_reg[SLOT_COL_DEBUG4],
		collision_slot_reg[SLOT_COL_DEBUG5]);

	TCON_LOG("read buffer index:%d", read_buffer_index);
	/* read task assigned lut ID */
	task->lut_id = slot_reg[SLOT_PIPELINE_TASK_LUT_ID];
	//if (task->lut_id == 0xFFFFFFFF) {
	if (task->lut_id < 0 ||
		task->lut_id > 63) {
		TCON_ERR("task:0x%llx [%d %d %d %d] waveform:%d assign lut not find 0x%016llx lut_id:%d",
			task->unique_id,
			hwtcon_core_get_task_region(task).x,
			hwtcon_core_get_task_region(task).y,
			hwtcon_core_get_task_region(task).width,
			hwtcon_core_get_task_region(task).height,
			task->update_data.waveform_mode,
			pipeline_get_lut_status(),
			task->lut_id);
		for (i = 0; i < MAX_LUT_REGION_COUNT; i++) {
			if (pipeline_get_lut_status() & 1LL << i) {
				struct rect region = {0};

				pipeline_get_unreleased_lut_info(NULL, i, NULL, NULL, &region);
				TCON_ERR("lut %d region[%d %d %d %d]", i, region.x, region.y, region.width, region.height);
			}
		}

		for (i = 0; i < SLOT_MAX; i++)
			TCON_ERR("gce read slot[%d] = 0x%08x",
				i, slot_reg[i]);

		spin_lock_irqsave(&hwtcon_core_get_task_list_from_state(task->state)->lock, flags);
		list_del_init(&task->list);
		spin_unlock_irqrestore(&hwtcon_core_get_task_list_from_state(task->state)->lock, flags);
		hwtcon_core_put_task_callback(task);
		hwtcon_fb_info()->pipeline_busy = false;
		wake_up(&hwtcon_fb_info()->pipeline_trigger_wait_queue);
		BUG_ON(1);
		return -1;
	}

	pipeline_get_unreleased_lut_info(NULL,
		task->lut_id,
		NULL,
		&slot_index, NULL);
	slot_index &= GENMASK(2, 0);

	wf_mode = hwtcon_core_get_waveform_mode(slot_index,
		hwtcon_core_use_night_mode());

	TCON_EPDC("[%d] Sending update. waveform:%d (%s) mode:0x%d update region top=%d, left=%d, width=%d, height=%d temp index: %d rotation=%d",
		task->update_data.update_marker,
		wf_mode,
		hwtcon_core_get_wf_mode_name(wf_mode),
		task->update_data.update_mode,
		hwtcon_core_get_task_user_region(task).y,
		hwtcon_core_get_task_user_region(task).x,
		hwtcon_core_get_task_user_region(task).width,
		hwtcon_core_get_task_user_region(task).height,
		task->used_temp_zone, hwtcon_fb_get_rotation());
	#if 1
	if (task->update_data.waveform_mode == WAVEFORM_MODE_AUTO)
		TCON_EPDC("[%d] current_hist_stat = 0x%x[%d] next_hist_stat = 0x%x[%d] index[%d] new waveform = 0x%x (%s)",
			task->update_data.update_marker,
			auto_slot_reg[SLOT_FB_CUR_HISTOGRAM],
			auto_slot_reg[SLOT_FB_CUR_GREY],
			auto_slot_reg[SLOT_FB_NXT_HISTOGRAM],
			auto_slot_reg[SLOT_FB_NXT_GREY],
			auto_slot_reg[SLOT_FB_INDEX],
			wf_mode,
			hwtcon_core_get_wf_mode_name(wf_mode));
	#endif
	TCON_EPDC("[%d] Sending update in LUT: %d",
		task->update_data.update_marker,
		task->lut_id);

	TCON_LOG("WRITTEN:task:0x%llx marker[%d] LUT:%d slot:%d mode:%d(%s)",
		task->unique_id,
		task->update_data.update_marker,
		task->lut_id,
		slot_index,
		wf_mode,
		hwtcon_core_get_wf_mode_name(wf_mode));

	/* update lut info array */
	spin_lock_irqsave(&hwtcon_fb_info()->lut_info_lock, flags);
	hwtcon_fb_info()->lut_info[task->lut_id].busy = true;
	hwtcon_fb_info()->lut_info[task->lut_id].priority = 0;
	hwtcon_fb_info()->lut_info[task->lut_id].region =
		hwtcon_core_get_task_region(task);
	hwtcon_fb_info()->lut_info[task->lut_id].waveform_mode =
		task->update_data.waveform_mode;
	hwtcon_fb_info()->lut_info[task->lut_id].task = task;
	spin_unlock_irqrestore(&hwtcon_fb_info()->lut_info_lock, flags);

	hwtcon_core_change_task_state(task,
		TASK_STATE_PIPELINE_DONE, true, true, INSERT_TO_TAIL);

	task->time_pipeline_done = timeofday_ms();

	hwtcon_fb_info()->pipeline_busy = false;
	wake_up(&hwtcon_fb_info()->pipeline_trigger_wait_queue);

	/* hwtcon_core_handle_task_written_done */
	queue_work(hwtcon_fb_info()->wq_pipeline_written_done,
		&task->work_written_done);
	return 0;
}

void hwtcon_core_change_waveform_slot(struct hwtcon_task *task)
{
	int temp_zone = 0;
	int night_mode = 0;

	temp_zone = task->used_temp_zone;
	night_mode = (task->update_data.waveform_mode == WAVEFORM_MODE_GCK16 ||
		task->update_data.waveform_mode == WAVEFORM_MODE_GLKW16);

	if ((hwtcon_fb_info()->current_temp_zone != temp_zone) ||
		(hwtcon_fb_info()->current_night_mode != night_mode)) {
		TCON_LOG("change waveform slot temp_zone:%d night_mode:%d",
			temp_zone, night_mode);
		/* wait all wf_lut release */
		if (hwtcon_core_wait_all_wf_lut_release() != 0)
			return;
		wf_lut_waveform_slot_association(NULL, night_mode, temp_zone);
		if (hwtcon_fb_info()->current_night_mode != night_mode) {
			/* PMIC night mode enable */
			fiti_set_night_mode(night_mode);
		}

		hwtcon_fb_info()->current_temp_zone = temp_zone;
		hwtcon_fb_info()->current_night_mode = night_mode;
	}
}

/* config the timing of trigger pipeline start to work */
void hwtcon_core_wait_for_trigger(struct cmdqRecStruct *pkt, struct hwtcon_task *task)
{
	/* WF_LUT_DPI_STATUS: [12:0] vcounter, [16] dpi busy */
	CMDQ_VARIABLE lut_status0 = 0;
	CMDQ_VARIABLE lut_status1 = 0;
	CMDQ_VARIABLE condition = 0;
	CMDQ_VARIABLE vcounter = 0;

	cmdq_op_read_reg(pkt, PIPELINT_LUT_STATUS0, &lut_status0, 0xFFFFFFFF);
	cmdq_op_read_reg(pkt, PIPELINT_LUT_STATUS1, &lut_status1, 0xFFFFFFFF);
	cmdq_op_or(pkt, &lut_status0, lut_status0, lut_status1);

	/*
		if (lut_status0 == 0)
			wait vcounter >= TRIGGER_PIPELINE_POSITION || vcounter < hw_tcon_get_wf_lut_modify_vcounter
		else
			wait CMDQ_EVENT_WF_LUT_FRAME_DONE
	*/
	cmdqRecClearEventToken(pkt, CMDQ_EVENT_WF_LUT_FRAME_DONE);

	#if 1
	cmdq_op_while(pkt, 1, CMDQ_EQUAL, 1);

		cmdq_op_if(pkt, lut_status0, CMDQ_NOT_EQUAL, 0);
			cmdqRecWait(pkt, CMDQ_EVENT_WF_LUT_FRAME_DONE);
			cmdq_op_break(pkt);
		cmdq_op_end_if(pkt);
		/* lut_status0 == 0 */
		cmdq_op_read_reg(pkt, WF_LUT_DPI_STATUS, &vcounter, GENMASK(12, 0));
		cmdq_op_assign(pkt, &condition, 1);

		cmdq_op_if(pkt, vcounter, CMDQ_LESS_THAN, TRIGGER_PIPELINE_POSITION);
			cmdq_op_assign(pkt, &condition, 0);
		cmdq_op_end_if(pkt);

		cmdq_op_if(pkt, vcounter, CMDQ_GREATER_THAN_AND_EQUAL, hw_tcon_get_wf_lut_modify_vcounter());
			cmdq_op_assign(pkt, &condition, 0);
		cmdq_op_end_if(pkt);

		cmdq_op_if(pkt, condition, CMDQ_EQUAL, 1);
			cmdq_op_break(pkt);
		cmdq_op_end_if(pkt);

	cmdq_op_end_while(pkt);
	#else
	cmdq_op_if(pkt, lut_status0, CMDQ_EQUAL, 0);
		//cmdqRecPoll(pkt, WF_LUT_DPI_STATUS, TRIGGER_PIPELINE_POSITION, GENMASK(12, 0));
		cmdqRecPoll(pkt, WF_LUT_DPI_STATUS, hw_tcon_get_wf_lut_modify_vcounter(), GENMASK(12, 0));
	cmdq_op_else(pkt);
		cmdqRecWait(pkt, CMDQ_EVENT_WF_LUT_FRAME_DONE);
	cmdq_op_end_if(pkt);
	#endif
	return;
}

bool hwtcon_core_check_task_full_collision(struct hwtcon_task *task,
	u64 *lut_dependency)
{
	struct rects result = {0};
	struct rect task_region = hwtcon_core_get_task_region(task);
	struct rect lut_region = {0};
	int i = 0;

	result.count = 1;
	result.regions[0] = task_region;
	*lut_dependency = 0LL;

	for (i = 0; i < MAX_LUT_REGION_COUNT; i++) {
		if (pipeline_get_lut_status() & (1LL << i)) {
			/* LUT i is running */
			pipeline_get_unreleased_lut_info(NULL, i, NULL, NULL, &lut_region);
			if (hwtcon_rect_have_collision(&task_region, &lut_region))
				*lut_dependency |= 1LL << i;
			result = hwtcon_rect_minus_10(&result, &lut_region);
		}
	}

	/* if result.count == 0: fully collision
	 * else: not fully collision
	 */
	return (result.count == 0);
}


int hwtcon_core_trigger_pipeline(struct hwtcon_task *task)
{
	static int index;
	static struct cmdqRecStruct *pkt = NULL;
	struct rect task_region = hwtcon_core_get_task_region(task);
	struct update_lut_config lut_config = {0};
	enum REGAL_MODE_ENUM regal_mode = REGAL_MODE_REGAL;
	u32 waveform_index = 0;
	int i = 0;
	u64 lut_dependency = 0LL;
	struct rect region = {0, 0, hw_tcon_get_edp_width(), hw_tcon_get_edp_height()};

	task->used_temp = hwtcon_core_read_temperature();
	task->used_temp_zone = hwtcon_core_read_temp_zone();

	#if 1
	/* check whether the task is fully collision */
	if (hwtcon_core_check_task_full_collision(task, &lut_dependency)) {
		/* Task is fully contained in display area
		 * modify Task->lut_id = -1;
		 * modfiy Task->is_auto_trigger = true;
		 * modify Task->lut_dependency = collision_luts
		 * move Task to collision_task_list
		 */
		task->lut_id = -1;
		task->is_collsion_auto_trigger = true;
		task->lut_dependency = lut_dependency;
		TCON_LOG("create collision task region[%d %d %d %d] lut_dependency:0x%016llx",
			task->update_data.update_region.left,
			task->update_data.update_region.top,
			task->update_data.update_region.width,
			task->update_data.update_region.height,
			task->lut_dependency);
		hwtcon_core_insert_task_to_collision_task_list(task, false);

		return 0;
	}
	#endif

	#ifndef HWTCON_USE_CPU_CONFIG
	if (pkt == NULL)
		cmdqRecCreate(CMDQ_SCENARIO_HWTCON, &pkt);
	cmdqRecReset(pkt);
	cmdqRecBackupRegisterToSlot(pkt, task->slot, SLOT_PIPELINE_DEBUG_0, GPT_TIMER);
	#endif

	task->time_enable_power = timeofday_ms();


	hwtcon_fb_info()->pipeline_busy = true;

	/* enable power & clock */
	hwtcon_driver_enable_mmsys_power(task, true);

	hwtcon_core_change_waveform_slot(task);

	/* config the timing of trigger pipeline start to work */
	hwtcon_core_wait_for_trigger(pkt, task);
	cmdqRecBackupRegisterToSlot(pkt, task->slot, SLOT_PIPELINE_WF_LUT_EOF_COUNTER, WF_LUT_DPI_STATUS);

#if 1
	#if 1
	region = task_region;
	if (region.width < 16)
		region.width = 16;
	#endif
	pipeline_config_sw_image_size(pkt, region);
#endif
	/* clear all HWTCON event */
	for (i = 0; i < 16; i++)
		cmdqRecClearEventToken(pkt, i);
	cmdqRecClearEventToken(pkt, CMDQ_SYNC_HWTCON_WDMA_FRAME_DONE);

	/* config image buffer addr & size */
	if (task->update_data.flags & EPDC_FLAG_USE_ALT_BUFFER) {
		u32 buffer_pa = 0;

		hwtcon_core_get_task_buffer_info(task, &buffer_pa, NULL, NULL);
		paper_config_img_buffer_addr(pkt, buffer_pa);
		paper_config_img_buffer_size(pkt, IMG_BUFFER_SIZE_SEL_SINGLE);
	} else {
		u32 buffer_pa = 0;

		hwtcon_core_get_task_buffer_info(task, &buffer_pa, NULL, NULL);
		paper_config_img_buffer_addr(pkt, buffer_pa);
		paper_config_img_buffer_size(pkt, IMG_BUFFER_SIZE_SEL_PANEL);
	}
	#if 1
	/* use software lut merge region */
	paper_config_img_buffer_size(pkt, IMG_BUFFER_SIZE_SEL_PANEL);
	paper_config_image_buffer_pitch(pkt, BUF_PITCH_SEL_FROM_SW_CONFIG, hw_tcon_get_edp_width());
	#endif
	/* collision update mode. */
	if (task->update_data.flags & EPDC_FLAG_TEST_COLLISION)
		pipeline_config_collision_handle_method(pkt,
			LUT_COLLISION_HANDLE_DETECT_ONLY);
	else
		pipeline_config_collision_handle_method(pkt,
			LUT_COLLISION_HANDLE_NO_UPDATE);

	/* always config full update. */
	paper_config_update_mode(pkt, PAPER_UPDATE_MODE_FULL);
	paper_config_pre_buffer_region(pkt,
		PRE_BUF_UPDATE_MODE_ONLY_UPDATE_LUT_REGION);
	pp_write_mask(pkt, WF_LUT_CON, 0 << 19, BIT_MASK(19));

	/* regal setting  */
	if (hwtcon_core_use_regal(task, &regal_mode)) {
		u32 buffer_width = 0;
		u32 buffer_height = 0;

		hwtcon_core_get_task_buffer_info(task, NULL,
			&buffer_width, &buffer_height);
		TCON_LOG("regal enable, width:%d height:%d mode:%d",
			buffer_width, buffer_height, regal_mode);
		hwtcon_regal_config_regal_mode(pkt, regal_mode,
			buffer_width, buffer_height);

		paper_regal_enable(pkt, true);
	} else {
		TCON_LOG("regal disable");
		paper_regal_enable(pkt, false);
	}

	/* set histogram enable */
	paper_config_enable_histogram(pkt, true, false);

	/* modfiy waveform mode to hw index */
	waveform_index  =
		hwtcon_core_get_waveform_mode_index(
			task->update_data.waveform_mode,
			hwtcon_core_use_night_mode());

	task->time_trigger_pipeline = timeofday_ms();

	TCON_EPDC("[%d] waveform=0x%x (%s) mode=0x%x temp:%d update region top=%d, left=%d, width=%d, height=%d flags=0x%x rotation=%d",
		task->update_data.update_marker,
		task->update_data.waveform_mode,
		hwtcon_core_get_wf_mode_name(task->update_data.waveform_mode),
		task->update_data.update_mode,
		task->update_data.temp,
		hwtcon_core_get_task_user_region(task).y,
		hwtcon_core_get_task_user_region(task).x,
		hwtcon_core_get_task_user_region(task).width,
		hwtcon_core_get_task_user_region(task).height,
		task->update_data.flags, hwtcon_fb_get_rotation());

	TCON_LOG("TRIGGER:task:0x%llx marker:%d time:%lld region[%d %d %d %d]",
		task->unique_id,
		task->update_data.update_marker,
		task->time_trigger_pipeline,
		task_region.x,
		task_region.y,
		task_region.width,
		task_region.height);
	/* trigger lut to pipeline. */
	TCON_LOG("%s update:%d->%s wf_mode:%d-%s slot:%d temperature:%d-%d",
		hwtcon_core_use_night_mode() ? "Night Mode" : "Day Mode",
		task->update_data.update_mode,
		(task->update_data.update_mode == UPDATE_MODE_FULL) ?
			"FULL" : "PARTIAL",
		task->update_data.waveform_mode,
		hwtcon_core_get_wf_mode_name(task->update_data.waveform_mode),
		waveform_index,
		task->used_temp,
		task->used_temp_zone);

	memset(&lut_config, 0, sizeof(lut_config));
	lut_config.waveform_mode = 1 << 3 | waveform_index;
	lut_config.lut_region = task_region;
	lut_config.is_last_lut = true;
	paper_config_update_lut(pkt, &lut_config);
	if (task_region.width == 0 || task_region.height == 0) {
		TCON_ERR("trigger invalid region with [%d %d %d %d]",
			task_region.x,
			task_region.y,
			task_region.width,
			task_region.height);
	}
	cmdqRecClearEventToken(pkt, CMDQ_SYNC_HWTCON_WDMA_FRAME_DONE);

	#ifndef HWTCON_USE_CPU_CONFIG
	/* send region request need to before dpi sof */
	cmdq_op_write_reg(pkt, 0x10238060, 0, 0xFFFFFFFF);
	cmdqRecBackupRegisterToSlot(pkt, task->slot, SLOT_PIPELINE_WF_LUT_SOF1, 0x10238064);

	//cmdqRecWait(pkt, CMDQ_EVENT_PIPELINE_SOF);
	cmdqRecBackupRegisterToSlot(pkt, task->slot, SLOT_PIPELINE_PIPELINE_SOF_COUNTER, WF_LUT_DPI_STATUS);
	cmdqRecBackupRegisterToSlot(pkt, task->slot, SLOT_PIPELINE_DEBUG_8, GPT_TIMER);
	cmdqRecWait(pkt, CMDQ_SYNC_HWTCON_WDMA_FRAME_DONE);
	cmdqRecBackupRegisterToSlot(pkt, task->slot, SLOT_PIPELINE_DEBUG_9, GPT_TIMER);
	/* working buffer write done. */
	cmdqRecBackupRegisterToSlot(pkt, task->slot, SLOT_PIPELINE_WDMA_EOF_COUNTER, WF_LUT_DPI_STATUS);

	//hwtcon_core_compose_histogram_command(pkt, task);
	//cmdqRecBackupRegisterToSlot(pkt, task->slot, SLOT_PIPELINE_DEBUG_0, WF_LUT_DPI_STATUS);

	/* modify waveform mode need to before wf_lut sof */
	cmdq_op_write_reg(pkt, 0x10238060, 4, 0xFFFFFFFF);
	cmdqRecBackupRegisterToSlot(pkt, task->slot, SLOT_PIPELINE_WF_LUT_SOF2, 0x10238064);

	cmdqRecBackupRegisterToSlot(pkt, task->slot,
		SLOT_PIPELINE_ASSIGN_STATUS0,
		PIPELINE_ASSIGN_STATUS0);
	cmdqRecBackupRegisterToSlot(pkt, task->slot,
		SLOT_PIPELINE_ASSIGN_STATUS1,
		PIPELINE_ASSIGN_STATUS1);
	tcon_config_global_register(pkt);
	pp_write_mask(pkt, WF_LUT_DPI_OUTPUT_SETTING, 0 << 16, BIT_MASK(16));
	hwtcon_core_compose_assign_task_lut_commmand(pkt, task);

	if (hwtcon_core_use_regal(task, &regal_mode))
		paper_regal_enable(pkt, false);

	/* clear image buffer read region. If not clear, panel will flash */
	memset(&region, 0, sizeof(region));
	pipeline_config_sw_image_size(pkt, region);

	//hwtcon_core_config_buffer_index(pkt);
	cmdqRecBackupRegisterToSlot(pkt, task->slot, SLOT_PIPELINE_DEBUG_1, GPT_TIMER);

	cmdqRecFlush(pkt);
	hwtcon_core_handle_pipeline_done((unsigned long)(task));
	/* create new task to collision list if have collision */
	hwtcon_core_create_collision_task(task);
	cmdqRecReset(pkt);
	//cmdqRecDestroy(pkt);
	#endif

	hwtcon_debug_lut_info_printf(
		"id:%d region[%d %d %d %d] %s wf_mode[%d->%s,slot:%d]",
		index++,
		task_region.x,
		task_region.y,
		task_region.width,
		task_region.height,
		hwtcon_core_use_night_mode() ? "Night Mode" : "Day Mode",
		task->update_data.waveform_mode,
		hwtcon_core_get_wf_mode_name(task->update_data.waveform_mode),
		waveform_index);

	hwtcon_debug_lut_info_printf(
		"temp[%d-%d] update[%s]\n",
		task->used_temp,
		task->used_temp_zone,
		(task->update_data.update_mode == UPDATE_MODE_FULL) ?
			"FULL" : "PARTIAL");
	return 0;
}

int hwtcon_core_dispatch_mdp(void *ignore)
{
	struct hwtcon_task *task = NULL;
	unsigned long flags;

	while (1) {
		wait_event(hwtcon_fb_info()->mdp_trigger_wait_queue,
			!list_empty(
			&hwtcon_fb_info()->wait_for_mdp_task_list.list));
		spin_lock_irqsave(&hwtcon_fb_info()->wait_for_mdp_task_list.lock, flags);
		task = list_first_entry_or_null(
			&hwtcon_fb_info()->wait_for_mdp_task_list.list,
			struct hwtcon_task, list);
		spin_unlock_irqrestore(&hwtcon_fb_info()->wait_for_mdp_task_list.lock,
			flags);
		if (task == NULL)
			continue;

		task->time_trigger_mdp = timeofday_ms();

		if (is_SW_mitigation_needed()) {
			hwtcon_pixel_pre_process(&task->update_data);
		}

		if (hwtcon_debug_get_info()->enable_dump_buffer)
			hwtcon_file_save_buffer(hwtcon_fb_info()->mdp_buffer_va,
				hwtcon_fb_get_virtual_width() * hwtcon_fb_get_height(),
				"/tmp/mdp.bin");
		/* call MDP */
		if (hwtcon_mdp_need_use_mdp(&task->update_data)) {
			/* use MDP convert dither */
			//mutex_lock(&hwtcon_fb_info()->image_buffer_access_mutex);
			hwtcon_mdp_convert(task);
			//mutex_unlock(&hwtcon_fb_info()->image_buffer_access_mutex);
		}

		task->time_mdp_done = timeofday_ms();
		hwtcon_core_insert_task_to_mdp_done_task_list(task, true, INSERT_TO_TAIL);
		wake_up(&hwtcon_fb_info()->pipeline_trigger_wait_queue);
	}

	return 0;
}

u32 hwtcon_core_read_wf_frame_count(int wf_lut_id)
{
	unsigned long flags;
	u32 value;

	/*read wf_lut waveform mode in wf_lut irq */
	spin_lock_irqsave(&hwtcon_fb_info()->hwtcon_tcon_reg_lock,
			flags);
	pp_write_mask(NULL, 0x140041D4, wf_lut_id << 8 | 0x10 << 1 | 1 << 0,
		GENMASK(15, 8) | GENMASK(5, 1) | BIT_MASK(0));
	value = (pp_read(hwtcon_driver_get_wf_lut_va() + 0x1D8) &
		GENMASK(19, 8)) >> 8;
	spin_unlock_irqrestore(
			&hwtcon_fb_info()->hwtcon_tcon_reg_lock,
			flags);
	return value;
}

int hwtcon_core_dispatch_pipeline(void *ignore)
{
	struct hwtcon_task *task = NULL;
	unsigned long flags;

	DEFINE_WAIT_FUNC(wait, woken_wake_function);

	add_wait_queue(&hwtcon_fb_info()->pipeline_trigger_wait_queue, &wait);
	while (1) {
		/* find the first task */
		spin_lock_irqsave(&hwtcon_fb_info()->mdp_done_task_list.lock, flags);
		task = list_first_entry_or_null(
			&hwtcon_fb_info()->mdp_done_task_list.list,
			struct hwtcon_task, list);

		if (task == NULL || (hwtcon_core_check_pipeline_busy(task) != 0)) {
			spin_unlock_irqrestore(&hwtcon_fb_info()->mdp_done_task_list.lock, flags);
			wait_woken(&wait,
				TASK_INTERRUPTIBLE,
				MAX_SCHEDULE_TIMEOUT);
			continue;
		}

		hwtcon_core_change_task_state(task, TASK_STATE_PIPELINE_PROCESS, false, true, INSERT_TO_TAIL);
		spin_unlock_irqrestore(&hwtcon_fb_info()->mdp_done_task_list.lock, flags);

		/* trigger task to pipeline. */
		//mutex_lock(&hwtcon_fb_info()->image_buffer_access_mutex);
		hwtcon_core_trigger_pipeline(task);
		//mutex_unlock(&hwtcon_fb_info()->image_buffer_access_mutex);

	}

	remove_wait_queue(
		&hwtcon_fb_info()->pipeline_trigger_wait_queue,
		&wait);
	return 0;
}

void hwtcon_core_config_buffer_index(struct cmdqRecStruct *pkt)
{
	#ifndef HWTCON_AUTO_CHANGE_BUFFER_INDEX
	/* config buffer index */
	if (pkt == NULL) {
		u32 value = 0;

		cmdqBackupReadSlot(hwtcon_fb_info()->slot_read_buffer_index, 0, &value);
		if (value)
			paper_config_working_buffer_start_index(pkt,
				WB_READ_INDEX_ADDR1);
		else
			paper_config_working_buffer_start_index(pkt,
				WB_READ_INDEX_ADDR0);

		cmdqBackupWriteSlot(hwtcon_fb_info()->slot_read_buffer_index, 0, !value);
	} else {
		/* use gce config buffer index */
		CMDQ_VARIABLE index_value = 0;
		CMDQ_VARIABLE pa_addr = 0;

		cmdq_op_assign(pkt, &pa_addr, hwtcon_fb_info()->slot_read_buffer_index);
		cmdq_op_read_mem_with_cpr(pkt, pa_addr, &index_value, 0xFFFFFFFF);

		cmdq_op_if(pkt, index_value, CMDQ_EQUAL, 1);
			paper_config_working_buffer_start_index(pkt,
				WB_READ_INDEX_ADDR1);
			cmdq_op_assign(pkt, &index_value, 0);
		cmdq_op_else(pkt);
			paper_config_working_buffer_start_index(pkt,
				WB_READ_INDEX_ADDR0);
			cmdq_op_assign(pkt, &index_value, 1);
		cmdq_op_end_if(pkt);
		cmdq_op_backup_CPR(pkt, index_value, hwtcon_fb_info()->slot_read_buffer_index, 0);

	}
	#endif
}

void hwtcon_core_restore_buffer_index(struct cmdqRecStruct *pkt)
{
	u32 value = 0;

	cmdqBackupReadSlot(hwtcon_fb_info()->slot_read_buffer_index, 0, &value);
	//if (hwtcon_fb_info()->read_buffer_index)
	if (value)
		paper_config_working_buffer_start_index(pkt,
			WB_READ_INDEX_ADDR0);
	else
		paper_config_working_buffer_start_index(pkt,
			WB_READ_INDEX_ADDR1);
}

void hwtcon_core_config_timing(struct cmdqRecStruct *pkt)
{
	/* config edp pmic */
	fiti_power_enable(true);
	fiti_wait_power_good();

	/* config smi setting */
	rdma_config_smi_setting(NULL);

	/* confit wf_lut */
	wf_lut_config_context(pkt);
	wf_lut_dpi_enable(pkt);

	/* set vsync sof trigger mode. */
	#ifdef HWTCON_USE_AUTO_LUT_MERGE
	hwtcon_core_config_paper_top_sof(pkt,
		MAIN_SOF_MODE_IMG_LAST_UPDATE_AND_DPI_VSYNC,
		//MAIN_SOF_MODE_DPI_VSYNC,
		WF_LUT_SOF_SEL_SW,
		LUT_MERGE_SOF_SEL_AUTO,
		0x100, 0x100, 0x1000);
	#else
	hwtcon_core_config_paper_top_sof(pkt,
		//MAIN_SOF_MODE_IMG_LAST_UPDATE_AND_DPI_VSYNC,
		MAIN_SOF_MODE_DPI_VSYNC,
		WF_LUT_SOF_SEL_SW,
		LUT_MERGE_SOF_SEL_SW,
		0xFFFFFFFF, 0x100, 0x1000);
	#endif

	#if 1
	paper_config_dpi_vsync_trigger_mode(pkt, DPI_VSYNC_SEL_SW);
	#else
	paper_config_dpi_vsync_trigger_mode(pkt, DPI_VSYNC_SEL_AUTO);
	#endif

	/* config panel size. */
	paper_config_panel_size(pkt, hw_tcon_get_edp_width(),
		hw_tcon_get_edp_height());
	/* config img buffer addr */
	paper_config_img_buffer_addr(pkt,
		hwtcon_fb_info()->fb_buffer_pa);
	/* config working buffer addr */
	paper_config_working_buffer_addr(pkt,
		hwtcon_fb_info()->wb_pa[0],
		hwtcon_fb_info()->wb_pa[1]);
	/* bit 0 use auto calculate. */
	paper_config_dma_source_select(pkt, DMA_CONFIG_SOURCE_SEL_AUTO_CONFIG);
	paper_config_data_process_fifo(pkt, true, 0xFF, 0x10);

	/* paper_config_img_buffer_format(pkt, IMG_BUFFER_FORMAT_Y4_MODE2); */

	/* config img buffer RDMA */
	rdma_config_image_rdma(pkt);
	/* config working buffer RDMA */
	rdma_config_wb_rdma(pkt);

	/* for first buffer. config pipeline to
	 * READ buffer[1] WRITE buffer[0]
	 */
	hwtcon_core_restore_buffer_index(pkt);

	pipeline_config_enable_irq(pkt, PIPELINE_IRQ_ASSIGN_DONE);

	hwtcon_core_config_wb_wdma(pkt, hw_tcon_get_edp_width(),
		hw_tcon_get_edp_height(),
		hwtcon_fb_info()->wb_pa[0]);

	/* enable wb_wdma irq */
	wdma_config_enable_interrupt(pkt, true);
	tcon_setting(NULL);

	pp_write_mask(NULL, WF_LUT_DPI_OUTPUT_SETTING, 1 << 16, BIT_MASK(16));
	hwtcon_edp_pinmux_active();

}


void hwtcon_core_dump_lut_info(int lut_id)
{
	if (lut_id < 0 || lut_id >= 64) {
		TCON_ERR("invalid lut_id:%d", lut_id);
		return;
	}

	TCON_LOG("id:%d priority:%d wf_mode:%d region:[%d %d %d %d]",
		lut_id,
		hwtcon_fb_info()->lut_info[lut_id].priority,
		hwtcon_fb_info()->lut_info[lut_id].waveform_mode,
		hwtcon_fb_info()->lut_info[lut_id].region.x,
		hwtcon_fb_info()->lut_info[lut_id].region.y,
		hwtcon_fb_info()->lut_info[lut_id].region.width,
		hwtcon_fb_info()->lut_info[lut_id].region.height);
}

void hwtcon_core_dump_task_info(struct hwtcon_task *task)
{
	if (task == NULL)
		return;
	TCON_LOG("dump task:0x%llx begin", task->unique_id);

	TCON_LOG("state:%d region[%d %d %d %d]",
		task->state,
		hwtcon_core_get_task_region(task).x,
		hwtcon_core_get_task_region(task).y,
		hwtcon_core_get_task_region(task).width,
		hwtcon_core_get_task_region(task).height);
	TCON_LOG("dump task:0x%llx end", task->unique_id);
}

u64 hwtcon_core_read_released_wf_lut_id(void)
{
	u64 release_lut = 0LL;

	release_lut |= pp_read(hwtcon_driver_get_wf_lut_va() + 0x254);
	release_lut |= (u64)(pp_read(hwtcon_driver_get_wf_lut_va() + 0x258))
		<< 32;
	/* read release lut id in wf_lut irq */
	if (release_lut != 0)
		TCON_LOG("release lut flag:0x%016llx lut status:0x%016llx",
			release_lut,
			pipeline_get_lut_status());

	return release_lut;
}

void hwtcon_core_handle_release_lut(int lut_id)
{
	struct hwtcon_task *task = NULL;
	bool lut_busy = false;
	unsigned long flags;
	int i = 0;

	if (lut_id < 0 || lut_id >= 64) {
		TCON_ERR("invalid lut_id:%d", lut_id);
		WARN_ON(1);
		return;
	}

	lut_busy = hwtcon_fb_info()->lut_info[lut_id].busy;
	task = hwtcon_fb_info()->lut_info[lut_id].task;

	hwtcon_core_dump_lut_info(lut_id);

	if (lut_busy == false) {
		struct hwtcon_task *dump_task, *tmp;
		int count = 0;

		TCON_ERR("release lut:%d not match", lut_id);
		list_for_each_entry_safe(dump_task, tmp,
			&hwtcon_fb_info()->pipeline_done_task_list.list, list) {
			TCON_ERR("index:%d task:0x%llx lut_id:%d", count++, dump_task->unique_id, dump_task->lut_id);
		}

		BUG_ON(1);
		return;
	}

	if (task == NULL) {
		TCON_ERR("lut %d has no task", lut_id);
		WARN_ON(1);
		return;
	}
	hwtcon_core_dump_task_info(task);

	/* release lut_info */
	memset(&hwtcon_fb_info()->lut_info[lut_id], 0,
		sizeof(struct wf_lut_info));
	wake_up(&hwtcon_fb_info()->wf_lut_release_wait_queue);

	if (task->lut_id != lut_id) {
		struct hwtcon_task *dump_task, *tmp;
		int count = 0;

		TCON_ERR("task lut:%d and lut_id:%d not sync",
			task->lut_id,
			lut_id);
		list_for_each_entry_safe(dump_task, tmp,
			&hwtcon_fb_info()->pipeline_done_task_list.list, list) {
			TCON_ERR("index:%d task:0x%llx lut_id:%d", count++, dump_task->unique_id, dump_task->lut_id);
		}

		BUG_ON(1);
		return;
	}
	/* release task */
	task->lut_id = -1;
	task->time_wf_lut_done = timeofday_ms();

	for (i = 0; i < task->marker_info.count; i++) {
		TCON_EPDC("[%d] update end marker=%d, end time=%lld, time taken=%d ms",
			task->marker_info.update_marker_arr[i],
			task->marker_info.update_marker_arr[i],
			task->time_wf_lut_done,
			hwtcon_hal_get_time_in_ms(
				task->time_submit,
				task->time_wf_lut_done));
	}
	TCON_LOG("DONE:task:0x%llx marker[%d] time:%lld cost:%d ms",
		task->unique_id,
		task->update_data.update_marker,
		task->time_wf_lut_done,
		hwtcon_hal_get_time_in_ms(
			task->time_trigger_pipeline,
			task->time_wf_lut_done));
	spin_lock_irqsave(&hwtcon_core_get_task_list_from_state(task->state)->lock, flags);
	list_del_init(&task->list);
	spin_unlock_irqrestore(&hwtcon_core_get_task_list_from_state(task->state)->lock, flags);
	hwtcon_core_put_task_callback(task);
	wake_up(&hwtcon_fb_info()->pipeline_trigger_wait_queue);
}

void hwtcon_core_update_collision_list_on_release_lut(void)
{
	unsigned long flags;
	struct hwtcon_task *task, *tmp;
	u64 lut_flag = ((u64)pp_read(WF_LUT_RDMA1_DBG_VA) << 32) | pp_read(WF_LUT_RDMA0_DBG_VA);

	spin_lock_irqsave(&hwtcon_fb_info()->collision_task_list.lock, flags);
	list_for_each_entry_safe(task, tmp,
		&hwtcon_fb_info()->collision_task_list.list, list) {

		task->lut_dependency &= ~lut_flag;
		if (task->lut_dependency == 0LL) {
			TCON_EPDC("retrigger collision task:0x%llx [%d %d %d %d] wf:%s to mdp_done_list",
				task->unique_id,
				hwtcon_core_get_task_region(task).x,
				hwtcon_core_get_task_region(task).y,
				hwtcon_core_get_task_region(task).width,
				hwtcon_core_get_task_region(task).height,
				hwtcon_core_get_wf_mode_name(task->update_data.waveform_mode));
			hwtcon_core_insert_task_to_mdp_done_task_list(task, false, INSERT_TO_HEAD);
		}
	}

	spin_unlock_irqrestore(&hwtcon_fb_info()->collision_task_list.lock,
		flags);
	wake_up(&hwtcon_fb_info()->pipeline_trigger_wait_queue);
}


int hwtcon_core_handle_wf_lut_release(void)
{
	unsigned long flags;
	u64 hw_lut_status = hwtcon_core_read_released_wf_lut_id();
	int i = 0;

	spin_lock_irqsave(&hwtcon_fb_info()->lut_info_lock, flags);
	for (i = 0; i < MAX_LUT_REGION_COUNT; i++) {
		if ((hw_lut_status & BIT_ULL_MASK(i))) {
			/* release lut i*/
			TCON_LOG("wf_lut release lut:%d", i);
			hwtcon_core_handle_release_lut(i);
		}
	}
	spin_unlock_irqrestore(&hwtcon_fb_info()->lut_info_lock, flags);

	/* collision list on release lut */
	hwtcon_core_update_collision_list_on_release_lut();

	return 0;
}

u32 hwtcon_core_read_wf_lut_waveform_mode(void)
{
	/* read waveform mode from 0x41D8: 32bit, 4bit per lut mode.
	 * read 0x41D8 once, get 8 lut waveform mode.
	 * config 0x41D4[13, 8] can be configured from 8 to F.
	 * config 0x41D4[13, 8] = 8, 0x41D8: lut 0 ~ lut 7 waveform mode
	 * config 0x41D4[13, 8] = 9, 0x41D8: lut 8 ~ lut 15 waveform mode
	 * config 0x41D4[13, 8] = A, 0x41D8: lut 16 ~ lut 23 waveform mode
	 * ...
	 * config 0x41D4[13, 8] = F, 0x41D8: lut 56 ~ lut 63 waveform mode
	 */

	u32 waveform_mode = 0;
	unsigned long flags;

	/*read wf_lut waveform mode in wf_lut irq */
	spin_lock_irqsave(&hwtcon_fb_info()->hwtcon_tcon_reg_lock,
			flags);
	pp_write_mask(NULL, WF_LUT_DEBUG_MON_SEL,
		8 << 8 | 0 << 14 | 0x16 << 1 | 1 << 0,
		GENMASK(13, 8) | GENMASK(15, 14) |
		GENMASK(5, 1) | BIT_MASK(0));
	waveform_mode = pp_read((hwtcon_driver_get_wf_lut_va() + 0x1D8));
	spin_unlock_irqrestore(
			&hwtcon_fb_info()->hwtcon_tcon_reg_lock,
			flags);
	return waveform_mode;
}

int hwtcon_core_convert_bit_count_2_grey_level(u32 bit_count)
{
	if ((bit_count & ~HISTOGRAM_GREY_LEVEL_Y2) == 0)
		return 0;
	if ((bit_count & ~HISTOGRAM_GREY_LEVEL_Y4) == 0)
		return 1;
	if ((bit_count & ~HISTOGRAM_GREY_LEVEL_Y8) == 0)
		return 2;
	if ((bit_count & ~HISTOGRAM_GREY_LEVEL_Y16) == 0)
		return 3;

	return 4;
}

char *hwtcon_core_get_wf_mode_name(enum WAVEFORM_MODE_ENUM mode)
{
	switch (mode) {
	case WAVEFORM_MODE_INIT:
		return "init";
	case WAVEFORM_MODE_DU:
		return "du";
	case WAVEFORM_MODE_GC16:
		return "gc16";
	case WAVEFORM_MODE_GL16:
		return "gl16";
	case WAVEFORM_MODE_GLR16:
		return "glr16 (reagl)";
	case WAVEFORM_MODE_GLD16:
		return "gld16 (reagld)";
	case WAVEFORM_MODE_A2:
		return "a2";
	case WAVEFORM_MODE_DU4:
		return "du4";
	case WAVEFORM_MODE_GCK16:
		return "gck16";
	case WAVEFORM_MODE_GLKW16:
		return "glkw16";
	case WAVEFORM_MODE_GC16_PARTIAL:
		return "gc16_partial";
	case WAVEFORM_MODE_GCK16_PARTIAL:
		return "gck16_partial";
	case WAVEFORM_MODE_AUTO:
		return "auto";
	default:
		return "unknown_mode";
	}

	return "unknown_mode";
}

enum WAVEFORM_MODE_ENUM hwtcon_core_calc_wf_mode_from_histogram(
	int night_mode, int full_update)
{
	static const enum WAVEFORM_MODE_ENUM wf_table_day_full[5][5] = {
		{1, 4, 4, 4, 1},
		{1, 4, 4, 4, 1},
		{1, 4, 2, 2, 1},
		{1, 2, 2, 2, 1},
		{1, 1, 1, 1, 1},
	};
	static const enum WAVEFORM_MODE_ENUM wf_table_day_partial[5][5] = {
		{1, 10, 10, 10, 1},
		{1, 10, 10, 10, 1},
		{1, 10, 10, 10, 1},
		{1, 10, 10, 10, 1},
		{1, 1, 1, 1, 1},
	};
	static const enum WAVEFORM_MODE_ENUM wf_table_night_full[5][5] = {
		{1, 9, 9, 9, 1},
		{1, 9, 9, 9, 1},
		{1, 9, 8, 8, 1},
		{1, 8, 8, 8, 1},
		{1, 1, 1, 1, 1},
	};
	static const enum WAVEFORM_MODE_ENUM wf_table_night_partial[5][5] = {
		{1, 11, 11, 11, 1},
		{1, 11, 11, 11, 1},
		{1, 11, 11, 11, 1},
		{1, 11, 11, 11, 1},
		{1, 1, 1, 1, 1},
	};

	u32 cur_histogram = 0;
	u32 nxt_histogram = 0;
	int cur_grey_level = 0;
	int nxt_grey_level = 0;
	int slot = 0;
	enum WAVEFORM_MODE_ENUM wf_mode = 0;

	paper_get_histogram_info(&cur_histogram,
		&nxt_histogram,
		NULL,
		NULL);

	cur_grey_level = hwtcon_core_convert_bit_count_2_grey_level(
		cur_histogram);
	nxt_grey_level = hwtcon_core_convert_bit_count_2_grey_level(
		nxt_histogram);

	if (night_mode)
		if (full_update)
			wf_mode = wf_table_night_full[
				cur_grey_level][nxt_grey_level];
		else
			wf_mode = wf_table_night_partial[
				cur_grey_level][nxt_grey_level];
	else
		if (full_update)
			wf_mode = wf_table_day_full[
				cur_grey_level][nxt_grey_level];
		else
			wf_mode = wf_table_day_partial[
				cur_grey_level][nxt_grey_level];

	slot = hwtcon_core_get_waveform_mode_index(wf_mode, night_mode);

	TCON_EPDC("current_hist_stat = 0x%x next_hist_stat = 0x%x new waveform = 0x%x (%s)",
		cur_histogram,
		nxt_histogram,
		wf_mode,
		hwtcon_core_get_wf_mode_name(wf_mode));
	TCON_LOG("HISTO:cur[0x%08x->%d] nxt[0x%08x->%d] mode:%d->%s slot:%d",
		cur_histogram,
		cur_grey_level,
		nxt_histogram,
		nxt_grey_level,
		wf_mode,
		hwtcon_core_get_wf_mode_name(wf_mode),
		slot);

	return slot;
}

void cmdq_core_calculate_auto_wf_mode(
	struct cmdqRecStruct *pkt, struct hwtcon_task *task,
	CMDQ_VARIABLE *auto_wf_mode)
{
	CMDQ_VARIABLE y2_grey;
	CMDQ_VARIABLE y4_grey;
	CMDQ_VARIABLE y8_grey;
	CMDQ_VARIABLE y16_grey;
	CMDQ_VARIABLE histogram;
	CMDQ_VARIABLE result;
	CMDQ_VARIABLE current_grey;
	CMDQ_VARIABLE next_grey;

	CMDQ_VARIABLE auto_wf_slot;
	CMDQ_VARIABLE calculated_index;
	#if 1
	cmdq_op_init_variable(&histogram);
	cmdq_op_init_variable(&result);
	cmdq_op_init_variable(&current_grey);
	cmdq_op_init_variable(&next_grey);
	cmdq_op_init_variable(&y2_grey);
	cmdq_op_init_variable(&y4_grey);
	cmdq_op_init_variable(&y8_grey);
	cmdq_op_init_variable(&y16_grey);
	cmdq_op_init_variable(&auto_wf_slot);
	cmdq_op_init_variable(&calculated_index);
	cmdq_op_init_variable(auto_wf_mode);
	#endif

	cmdq_op_assign(pkt, &y2_grey, (u32)~HISTOGRAM_GREY_LEVEL_Y2);
	cmdq_op_assign(pkt, &y4_grey, (u32)~HISTOGRAM_GREY_LEVEL_Y4);
	cmdq_op_assign(pkt, &y8_grey, (u32)~HISTOGRAM_GREY_LEVEL_Y8);
	cmdq_op_assign(pkt, &y16_grey, (u32)~HISTOGRAM_GREY_LEVEL_Y16);

	if (task) {
		cmdqRecBackupRegisterToSlot(pkt, task->slot, SLOT_PIPELINE_CUR_HISTOGRAM, PAPER_TCTOP_HIST_STA0);
		cmdqRecBackupRegisterToSlot(pkt, task->slot, SLOT_PIPELINE_NXT_HISTOGRAM, PAPER_TCTOP_HIST_STA1);
	}

	/* calculate the cur gray level */
	cmdq_op_read_reg(pkt, PAPER_TCTOP_HIST_STA0, &histogram, 0xFFFFFFFF);

	cmdq_op_while(pkt, 1, CMDQ_EQUAL, 1);

		cmdq_op_and(pkt, &result, histogram, y2_grey);
		cmdq_op_if(pkt, result, CMDQ_EQUAL, 0);
			cmdq_op_assign(pkt, &current_grey, 0);
			cmdq_op_break(pkt);
		cmdq_op_end_if(pkt);

		cmdq_op_and(pkt, &result, histogram, y4_grey);
		cmdq_op_if(pkt, result, CMDQ_EQUAL, 0);
			cmdq_op_assign(pkt, &current_grey, 1);
			cmdq_op_break(pkt);
		cmdq_op_end_if(pkt);

		cmdq_op_and(pkt, &result, histogram, y8_grey);
		cmdq_op_if(pkt, result, CMDQ_EQUAL, 0);
			cmdq_op_assign(pkt, &current_grey, 2);
			cmdq_op_break(pkt);
		cmdq_op_end_if(pkt);

		cmdq_op_and(pkt, &result, histogram, y16_grey);
		cmdq_op_if(pkt, result, CMDQ_EQUAL, 0);
			cmdq_op_assign(pkt, &current_grey, 3);
			cmdq_op_break(pkt);
		cmdq_op_end_if(pkt);

		cmdq_op_assign(pkt, &current_grey, 4);
		cmdq_op_break(pkt);

	cmdq_op_end_while(pkt);

	/* calculate the nxt gray level */
	cmdq_op_read_reg(pkt, PAPER_TCTOP_HIST_STA1, &histogram, 0xFFFFFFFF);

	cmdq_op_while(pkt, 1, CMDQ_EQUAL, 1);

		cmdq_op_and(pkt, &result, histogram, y2_grey);
		cmdq_op_if(pkt, result, CMDQ_EQUAL, 0);
			cmdq_op_assign(pkt, &next_grey, 0);
			cmdq_op_break(pkt);
		cmdq_op_end_if(pkt);

		cmdq_op_and(pkt, &result, histogram, y4_grey);
		cmdq_op_if(pkt, result, CMDQ_EQUAL, 0);
			cmdq_op_assign(pkt, &next_grey, 1);
			cmdq_op_break(pkt);
		cmdq_op_end_if(pkt);

		cmdq_op_and(pkt, &result, histogram, y8_grey);
		cmdq_op_if(pkt, result, CMDQ_EQUAL, 0);
			cmdq_op_assign(pkt, &next_grey, 2);
			cmdq_op_break(pkt);
		cmdq_op_end_if(pkt);

		cmdq_op_and(pkt, &result, histogram, y16_grey);
		cmdq_op_if(pkt, result, CMDQ_EQUAL, 0);
			cmdq_op_assign(pkt, &next_grey, 3);
			cmdq_op_break(pkt);
		cmdq_op_end_if(pkt);

		cmdq_op_assign(pkt, &next_grey, 4);
		cmdq_op_break(pkt);

	cmdq_op_end_while(pkt);

	/* calculate index = current_grey * 5 + next_grey */

	cmdq_op_multiply(pkt, &calculated_index, current_grey, 5);
	cmdq_op_add(pkt, &calculated_index, calculated_index, next_grey);
	cmdq_op_multiply(pkt, &result, calculated_index, 4);
	cmdq_op_assign(pkt, &auto_wf_slot, hwtcon_fb_info()->slot_auto_waveform);
	cmdq_op_add(pkt, &result, result, auto_wf_slot);
	/* dram PA is store in result CPR, read result CPR register value */
	cmdq_op_read_mem_with_cpr(pkt, result, auto_wf_mode, 0xFFFFFFFF);

	if (task) {
		cmdqRecBackupRegisterToSlot(pkt, task->slot, SLOT_PIPELINE_CUR_HISTOGRAM, PAPER_TCTOP_HIST_STA0);
		cmdqRecBackupRegisterToSlot(pkt, task->slot, SLOT_PIPELINE_NXT_HISTOGRAM, PAPER_TCTOP_HIST_STA1);
		cmdq_op_backup_CPR(pkt, current_grey, task->slot, SLOT_PIPELINE_CUR_GREY);
		cmdq_op_backup_CPR(pkt, next_grey, task->slot, SLOT_PIPELINE_NXT_GREY);
		cmdq_op_backup_CPR(pkt, *auto_wf_mode, task->slot, SLOT_PIPELINE_AUTO_WF_MODE);
	} else {
		cmdqRecBackupRegisterToSlot(pkt, hwtcon_fb_info()->slot_auto_waveform_info, SLOT_FB_CUR_HISTOGRAM, PAPER_TCTOP_HIST_STA0);
		cmdqRecBackupRegisterToSlot(pkt, hwtcon_fb_info()->slot_auto_waveform_info, SLOT_FB_NXT_HISTOGRAM, PAPER_TCTOP_HIST_STA1);
		cmdq_op_backup_CPR(pkt, current_grey, hwtcon_fb_info()->slot_auto_waveform_info, SLOT_FB_CUR_GREY);
		cmdq_op_backup_CPR(pkt, next_grey, hwtcon_fb_info()->slot_auto_waveform_info, SLOT_FB_NXT_GREY);
		cmdq_op_backup_CPR(pkt, calculated_index, hwtcon_fb_info()->slot_auto_waveform_info, SLOT_FB_INDEX);
		cmdq_op_backup_CPR(pkt, *auto_wf_mode, hwtcon_fb_info()->slot_auto_waveform_info, SLOT_FB_AUTO_WF_MODE);
	}


	cmdq_op_or(pkt, auto_wf_mode, *auto_wf_mode, 8);

}

void cmdq_core_compose_modify_wf_mode_command(
	struct cmdqRecStruct *pkt,
	CMDQ_VARIABLE index,
	CMDQ_VARIABLE lut_id,
	CMDQ_VARIABLE modified_mode,
	CMDQ_VARIABLE *modified_lut_flag)
{
	CMDQ_VARIABLE result = 0;
	CMDQ_VARIABLE write_val = 0;
	CMDQ_VARIABLE lut_id_bit_mask = 0;
	CMDQ_VARIABLE lut_wf_mode = 0;
	CMDQ_VARIABLE lut_priority = 0;
	CMDQ_VARIABLE lut_region_x = 0;
	CMDQ_VARIABLE lut_region_y = 0;
	CMDQ_VARIABLE lut_region_width = 0;
	CMDQ_VARIABLE lut_region_height = 0;

	/* readback lut i waveform mode/priority/region */
	/*
		pp_write_mask(pkt, PIPELINE_LUT_INFO_CLT0,
			1 << 7 | index,
			BIT_MASK(7) | GENMASK(5, 0));

		config0 = pp_read(PIPELINE_LUT_INFO_STA0_VA);
		config1 = pp_read(PIPELINE_LUT_INFO_STA1_VA);
		config2 = pp_read(PIPELINE_LUT_INFO_STA2_VA);
	*/
	cmdq_op_left_shift(pkt, &write_val, 1, 7);
	cmdq_op_or(pkt, &write_val, write_val, lut_id);
	cmdq_op_write_reg(pkt, PIPELINE_LUT_INFO_CLT0, write_val, BIT_MASK(7) | GENMASK(5, 0));

	cmdq_op_read_reg(pkt, PIPELINE_LUT_INFO_STA2, &lut_wf_mode, GENMASK(3, 0));
	cmdq_op_read_reg(pkt, PIPELINE_LUT_INFO_STA2, &lut_priority, GENMASK(11, 5));
	cmdq_op_right_shift(pkt, &lut_priority, lut_priority, 5);
	cmdq_op_read_reg(pkt, PIPELINE_LUT_INFO_STA0, &lut_region_x, GENMASK(12, 0));
	cmdq_op_read_reg(pkt, PIPELINE_LUT_INFO_STA0, &lut_region_y, GENMASK(25, 13));
	cmdq_op_right_shift(pkt, &lut_region_y, lut_region_y, 13);
	cmdq_op_read_reg(pkt, PIPELINE_LUT_INFO_STA1, &lut_region_width, GENMASK(12, 0));
	cmdq_op_read_reg(pkt, PIPELINE_LUT_INFO_STA1, &lut_region_height, GENMASK(25, 13));
	cmdq_op_right_shift(pkt, &lut_region_height, lut_region_height, 13);

	cmdq_op_if(pkt, lut_wf_mode, CMDQ_EQUAL, 0xF);
		/* LUT lut_id need to change waveform mode */
		#if 1
		/*
			pp_write_mask(pkt, PIPELINE_LUT_INFO_CLT0,
				lut_id << 24 | priority << 16 | wf_mode << 12,
				GENMASK(29, 24) | GENMASK(22, 16) | GENMASK(15, 12));

			pp_write(pkt, PIPELINE_LUT_INFO_CLT1,
				region.y << 13 | region.x << 0);
			pp_write(pkt, PIPELINE_LUT_INFO_CLT2,
				region.height << 13 | region.width << 0);
		*/

		/* write region, priority, new auto waveform mode */
		cmdq_op_left_shift(pkt, &write_val, lut_id, 24);
		cmdq_op_left_shift(pkt, &result, lut_priority, 16);
		cmdq_op_or(pkt, &write_val, write_val, result);
		cmdq_op_left_shift(pkt, &result, modified_mode, 12);
		cmdq_op_or(pkt, &write_val, write_val, result);
		cmdq_op_write_reg(pkt, PIPELINE_LUT_INFO_CLT0,
			write_val,
			GENMASK(29, 24) | GENMASK(22, 16) | GENMASK(15, 12));
		cmdq_op_left_shift(pkt, &write_val, lut_region_y, 13);
		cmdq_op_or(pkt, &write_val, write_val, lut_region_x);
		cmdq_op_write_reg(pkt, PIPELINE_LUT_INFO_CLT1, write_val, 0xFFFFFFFF);
		cmdq_op_left_shift(pkt, &write_val, lut_region_height, 13);
		cmdq_op_or(pkt, &write_val, write_val, lut_region_width);
		cmdq_op_write_reg(pkt, PIPELINE_LUT_INFO_CLT2, write_val, 0xFFFFFFFF);

		/* trigger sw lut enable */
		pp_write_mask(pkt, PIPELINE_LUT_INFO_CLT0,
			1 << 31 | 1 << 30,
			GENMASK(31, 30));
		pp_write_mask(pkt, PIPELINE_LUT_INFO_CLT0,
			0 << 31 | 0 << 30,
			GENMASK(31, 30));
		#endif

		/* record modified lut flag  for debug */
		cmdq_op_left_shift(pkt, &lut_id_bit_mask, lut_id_bit_mask, index);
		cmdq_op_or(pkt, modified_lut_flag, *modified_lut_flag, lut_id_bit_mask);
	cmdq_op_end_if(pkt);
}


void hwtcon_core_compose_assign_task_lut_commmand(
	struct cmdqRecStruct *pkt,
	struct hwtcon_task *task)
{
	/* simulate code */
	/*
		for (i = 0; i < MAX_LUT_REGION_COUNT; i++) {
			if (assigned_lut & BIT_ULL_MASK(i)) {
				pipeline_get_unreleased_lut_info(NULL, i,
					&priority,
					&mode,
					&region[i]);
				if (hwtcon_rect_compare(&region[i], &task_region)) {
					task->lut_id = i;
					return 0;
				}
			}
		}
	*/
	int start_x = hwtcon_core_get_task_region(task).x;
	int start_y = hwtcon_core_get_task_region(task).y;
	int end_x = hwtcon_core_get_task_region(task).width + start_x;
	int end_y = hwtcon_core_get_task_region(task).height + start_y;
	CMDQ_VARIABLE task_lut_id = 0;
	CMDQ_VARIABLE lut_id = 0;
	CMDQ_VARIABLE assign_status0 = 0;
	CMDQ_VARIABLE assign_status1 = 0;
	CMDQ_VARIABLE col_lut_status0 = 0;
	CMDQ_VARIABLE col_lut_status1 = 0;

	CMDQ_VARIABLE index = 0;
	CMDQ_VARIABLE lut_status = 0;
	CMDQ_VARIABLE result = 0;
	CMDQ_VARIABLE lut_region_x = 0;
	CMDQ_VARIABLE lut_region_y = 0;
	CMDQ_VARIABLE lut_region_width = 0;
	CMDQ_VARIABLE lut_region_height = 0;

	cmdq_op_read_reg(pkt, PIPELINT_LUT_STATUS0, &assign_status0, 0xFFFFFFFF);
	cmdq_op_read_reg(pkt, PIPELINT_LUT_STATUS1, &assign_status1, 0xFFFFFFFF);

	/* task->lut_id default -1 */
	cmdq_op_assign(pkt, &task_lut_id, 0xFFFFFFFF);
	cmdq_op_assign(pkt, &lut_id, 0);

#if 1
	cmdq_op_while(pkt, lut_id, CMDQ_LESS_THAN, MAX_LUT_REGION_COUNT);

		cmdq_op_if(pkt, lut_id, CMDQ_LESS_THAN, 32);
			cmdq_op_add(pkt, &index, lut_id, 0);
			cmdq_op_add(pkt, &lut_status, assign_status0, 0);
		cmdq_op_else(pkt);
			cmdq_op_subtract(pkt, &index, lut_id, 32);
			cmdq_op_add(pkt, &lut_status, assign_status1, 0);
		cmdq_op_end_if(pkt);

		cmdq_op_left_shift(pkt, &index, 1, index);
		cmdq_op_and(pkt, &result, lut_status, index);

		cmdq_op_if(pkt, result, CMDQ_EQUAL, 0);
			cmdq_op_add(pkt, &lut_id, lut_id, 1);
			cmdq_op_continue(pkt);
		cmdq_op_end_if(pkt);

		// read lut_id region
		cmdq_op_left_shift(pkt, &result, 1, 7);
		cmdq_op_or(pkt, &result, result, lut_id);
		cmdq_op_write_reg(pkt, PIPELINE_LUT_INFO_CLT0, result, BIT_MASK(7) | GENMASK(5, 0));

		cmdq_op_read_reg(pkt, PIPELINE_LUT_INFO_STA0, &lut_region_x, GENMASK(12, 0));
		cmdq_op_read_reg(pkt, PIPELINE_LUT_INFO_STA0, &lut_region_y, GENMASK(25, 13));
		cmdq_op_right_shift(pkt, &lut_region_y, lut_region_y, 13);
		cmdq_op_read_reg(pkt, PIPELINE_LUT_INFO_STA1, &lut_region_width, GENMASK(12, 0));
		cmdq_op_read_reg(pkt, PIPELINE_LUT_INFO_STA1, &lut_region_height, GENMASK(25, 13));
		cmdq_op_right_shift(pkt, &lut_region_height, lut_region_height, 13);

		#if 0
		cmdq_op_if(pkt, lut_id, CMDQ_EQUAL, 0);
			cmdq_op_backup_CPR(pkt, lut_region_x, task->slot, SLOT_PIPELINE_DEBUG_4);
			cmdq_op_backup_CPR(pkt, lut_region_y, task->slot, SLOT_PIPELINE_DEBUG_5);
			cmdq_op_backup_CPR(pkt, lut_region_width, task->slot, SLOT_PIPELINE_DEBUG_6);
			cmdq_op_backup_CPR(pkt, lut_region_height, task->slot, SLOT_PIPELINE_DEBUG_7);
		cmdq_op_end_if(pkt);
		#endif

		cmdq_op_assign(pkt, &result, 1);

		cmdq_op_if(pkt, hwtcon_core_get_task_region(task).x, CMDQ_EQUAL, lut_region_x);
			cmdq_op_and(pkt, &result, result, 1);
		cmdq_op_else(pkt);
			cmdq_op_and(pkt, &result, result, 0);
		cmdq_op_end_if(pkt);

		cmdq_op_if(pkt, hwtcon_core_get_task_region(task).y, CMDQ_EQUAL, lut_region_y);
			cmdq_op_and(pkt, &result, result, 1);
		cmdq_op_else(pkt);
			cmdq_op_and(pkt, &result, result, 0);
		cmdq_op_end_if(pkt);

		cmdq_op_if(pkt, hwtcon_core_get_task_region(task).width, CMDQ_EQUAL, lut_region_width);
			cmdq_op_and(pkt, &result, result, 1);
		cmdq_op_else(pkt);
			cmdq_op_and(pkt, &result, result, 0);
		cmdq_op_end_if(pkt);

		cmdq_op_if(pkt, hwtcon_core_get_task_region(task).height, CMDQ_EQUAL, lut_region_height);
			cmdq_op_and(pkt, &result, result, 1);
		cmdq_op_else(pkt);
			cmdq_op_and(pkt, &result, result, 0);
		cmdq_op_end_if(pkt);

		cmdq_op_if(pkt, result, CMDQ_EQUAL, 1);
			/* find the lut id */
			cmdq_op_add(pkt, &task_lut_id, lut_id, 0);
			cmdq_op_break(pkt);
		cmdq_op_end_if(pkt);

		cmdq_op_add(pkt, &lut_id, lut_id, 1);
	cmdq_op_end_while(pkt);

	/* backup the task lut id */
	cmdq_op_backup_CPR(pkt, task_lut_id, task->slot, SLOT_PIPELINE_TASK_LUT_ID);

	/* backup collision region store in assign_status0 & assign_status1 */
	cmdq_op_read_reg(pkt, PIPELINE_COL_REGION_INFO0, &assign_status0, 0xFFFFFFFF);
	cmdq_op_read_reg(pkt, PIPELINE_COL_REGION_INFO1, &assign_status1, 0xFFFFFFFF);

	/* read collision number store in result */
	cmdq_op_assign(pkt, &index, GENMASK(31, 26));
	cmdq_op_and(pkt, &result, assign_status1, index);
	cmdq_op_right_shift(pkt, &result, result, 26);

	cmdq_op_if(pkt, result, CMDQ_EQUAL, 0);
		cmdq_op_assign(pkt, &lut_region_x, 0);
		cmdq_op_assign(pkt, &lut_region_y, 0);
		cmdq_op_assign(pkt, &lut_region_width, 0);
		cmdq_op_assign(pkt, &lut_region_height, 0);
	cmdq_op_else(pkt);
		/* read collision x, y store in lut_region_x & lut_region_y */
		cmdq_op_assign(pkt, &index, GENMASK(12, 0));
		cmdq_op_and(pkt, &lut_region_x, assign_status0, index);
		cmdq_op_assign(pkt, &index, GENMASK(25, 13));
		cmdq_op_and(pkt, &lut_region_y, assign_status0, index);
		cmdq_op_right_shift(pkt, &lut_region_y, lut_region_y, 13);
		/* read collision width, height store in lut_region_width & lut_region_height */
		cmdq_op_assign(pkt, &index, GENMASK(12, 0));
		cmdq_op_and(pkt, &lut_region_width, assign_status1, index);
		cmdq_op_assign(pkt, &index, GENMASK(25, 13));
		cmdq_op_and(pkt, &lut_region_height, assign_status1, index);
		cmdq_op_right_shift(pkt, &lut_region_height, lut_region_height, 13);
	cmdq_op_end_if(pkt);

	cmdq_op_backup_CPR(pkt, result, task->slot_collision_info, SLOT_COL_COUNT);
	cmdq_op_backup_CPR(pkt, lut_region_x, task->slot_collision_info, SLOT_COL_X);
	cmdq_op_backup_CPR(pkt, lut_region_y, task->slot_collision_info, SLOT_COL_Y);
	cmdq_op_backup_CPR(pkt, lut_region_width, task->slot_collision_info, SLOT_COL_WIDTH);
	cmdq_op_backup_CPR(pkt, lut_region_height, task->slot_collision_info, SLOT_COL_HEIGHT);

	cmdq_op_read_reg(pkt, PIPELINT_LUT_STATUS0, &assign_status0, 0xFFFFFFFF);
	cmdq_op_read_reg(pkt, PIPELINT_LUT_STATUS1, &assign_status1, 0xFFFFFFFF);
	cmdq_op_assign(pkt, &col_lut_status0, 0);
	cmdq_op_assign(pkt, &col_lut_status1, 0);
	/* search all 64 lut */
	cmdq_op_assign(pkt, &index, 0);
	cmdq_op_while(pkt, index, CMDQ_LESS_THAN, 64);
		cmdq_op_if(pkt, index, CMDQ_LESS_THAN, 32);
			cmdq_op_left_shift(pkt, &lut_id, 1, index);
			cmdq_op_and(pkt, &result, assign_status0, lut_id);
		cmdq_op_else(pkt);
			cmdq_op_subtract(pkt, &lut_id, index, 32);
			cmdq_op_left_shift(pkt, &lut_id, 1, lut_id);
			cmdq_op_and(pkt, &result, assign_status1, lut_id);
		cmdq_op_end_if(pkt);

		cmdq_op_if(pkt, result, CMDQ_EQUAL, 0);
		cmdq_op_add(pkt, &index, index, 1);
		cmdq_op_continue(pkt);
		cmdq_op_end_if(pkt);

		/* lut index is running, read lut info */
		/* read lut index region */
		cmdq_op_left_shift(pkt, &result, 1, 7);
		cmdq_op_or(pkt, &result, result, index);
		cmdq_op_write_reg(pkt, PIPELINE_LUT_INFO_CLT0, result, BIT_MASK(7) | GENMASK(5, 0));

		cmdq_op_read_reg(pkt, PIPELINE_LUT_INFO_STA0, &lut_region_x, GENMASK(12, 0));
		cmdq_op_read_reg(pkt, PIPELINE_LUT_INFO_STA0, &lut_region_y, GENMASK(25, 13));
		cmdq_op_right_shift(pkt, &lut_region_y, lut_region_y, 13);
		cmdq_op_read_reg(pkt, PIPELINE_LUT_INFO_STA1, &lut_region_width, GENMASK(12, 0));
		cmdq_op_read_reg(pkt, PIPELINE_LUT_INFO_STA1, &lut_region_height, GENMASK(25, 13));
		cmdq_op_right_shift(pkt, &lut_region_height, lut_region_height, 13);

		cmdq_op_add(pkt, &index, index, 1);

		/* check whether task region has collision with lut_region
		 * lut_region_x store start_x
		 * lut_region_y store start_y
		 * lut_region_width store end_x
		 * lut_region_height store end_y
		*/
		cmdq_op_add(pkt, &lut_region_width, lut_region_x, lut_region_width);
		cmdq_op_add(pkt, &lut_region_height, lut_region_y, lut_region_height);

		/* lut_region_x store max(lut_region_x, start_x) */
		cmdq_op_if(pkt, lut_region_x, CMDQ_LESS_THAN, start_x);
		cmdq_op_add(pkt, &lut_region_x, start_x, 0);
		cmdq_op_end_if(pkt);
		/* lut_region_y store max(lut_region_y, start_y) */
		cmdq_op_if(pkt, lut_region_y, CMDQ_LESS_THAN, start_y);
		cmdq_op_add(pkt, &lut_region_y, start_y, 0);
		cmdq_op_end_if(pkt);
		/* lut_region_width store min(lut_region_width, end_x) */
		cmdq_op_if(pkt, lut_region_width, CMDQ_GREATER_THAN, end_x);
		cmdq_op_add(pkt, &lut_region_width, end_x, 0);
		cmdq_op_end_if(pkt);
		/* lut_region_height store min(lut_region_height, end_y) */
		cmdq_op_if(pkt, lut_region_height, CMDQ_GREATER_THAN, end_y);
		cmdq_op_add(pkt, &lut_region_height, end_y, 0);
		cmdq_op_end_if(pkt);

		/* result store whether has collision: 0: has collision 1: no collision */
		cmdq_op_assign(pkt, &result, 0);
		cmdq_op_if(pkt, lut_region_width, CMDQ_LESS_THAN_AND_EQUAL ,lut_region_x);
		cmdq_op_or(pkt, &result, result, 1);
		cmdq_op_end_if(pkt);
		cmdq_op_if(pkt, lut_region_height, CMDQ_LESS_THAN_AND_EQUAL ,lut_region_y);
		cmdq_op_or(pkt, &result, result, 1);
		cmdq_op_end_if(pkt);

		cmdq_op_backup_CPR(pkt, lut_region_x, task->slot_collision_info, SLOT_COL_DEBUG1);
		cmdq_op_backup_CPR(pkt, lut_region_y, task->slot_collision_info, SLOT_COL_DEBUG2);
		cmdq_op_backup_CPR(pkt, lut_region_width, task->slot_collision_info, SLOT_COL_DEBUG3);
		cmdq_op_backup_CPR(pkt, lut_region_height, task->slot_collision_info, SLOT_COL_DEBUG4);
		cmdq_op_backup_CPR(pkt, result, task->slot_collision_info, SLOT_COL_DEBUG5);

		cmdq_op_if(pkt, result, CMDQ_NOT_EQUAL, 0);
			cmdq_op_continue(pkt);
		cmdq_op_end_if(pkt);
		/* assign lut bit to 1 */
		cmdq_op_if(pkt, index, CMDQ_LESS_THAN, 32);
			cmdq_op_or(pkt, &col_lut_status0, col_lut_status0, lut_id);
		cmdq_op_else(pkt);
			cmdq_op_or(pkt, &col_lut_status1, col_lut_status1, lut_id);
		cmdq_op_end_if(pkt);
	cmdq_op_end_while(pkt);

	cmdq_op_backup_CPR(pkt, col_lut_status0, task->slot_collision_info, SLOT_COL_LUT0);
	cmdq_op_backup_CPR(pkt, col_lut_status1, task->slot_collision_info, SLOT_COL_LUT1);
#endif

}


void hwtcon_core_compose_auto_trigger_command(struct cmdqRecStruct *pkt)
{
	/* Simulate code */
	/*
		only_collision = true;
		for (lut_id = 0; lut_id < MAX_LUT_REGION_COUNT; lut_id++) {
			if (pipeline_get_assigned_lut_status() & BIT_ULL_MASK(lut_id)) {
				pipeline_get_unreleased_lut_info(NULL,
					lut_id,
					NULL,
					&waveform_mode, &region);
				// trigger by sw
				if (waveform_mode & BIT_MASK(3)) {
					only_collision = false;
				}
			}
		}

		if (only_collision)
			hwtcon_core_config_buffer_index(NULL);
	*/

	CMDQ_VARIABLE only_collision = 0;
	CMDQ_VARIABLE lut_id = 0;
	CMDQ_VARIABLE assign_status0 = 0;
	CMDQ_VARIABLE assign_status1 = 0;


	CMDQ_VARIABLE index = 0;
	CMDQ_VARIABLE lut_status = 0;
	CMDQ_VARIABLE result = 0;
	CMDQ_VARIABLE wf_mode = 0;


	cmdq_op_read_reg(pkt, PIPELINE_ASSIGN_STATUS0, &assign_status0, 0xFFFFFFFF);
	cmdq_op_read_reg(pkt, PIPELINE_ASSIGN_STATUS1, &assign_status1, 0xFFFFFFFF);

	cmdq_op_assign(pkt, &only_collision, 1);
	cmdq_op_assign(pkt, &lut_id, 0);

#if 1
	cmdq_op_while(pkt, lut_id, CMDQ_LESS_THAN, MAX_LUT_REGION_COUNT);

		cmdq_op_if(pkt, lut_id, CMDQ_LESS_THAN, 32);
			cmdq_op_add(pkt, &index, lut_id, 0);
			cmdq_op_add(pkt, &lut_status, assign_status0, 0);
		cmdq_op_else(pkt);
			cmdq_op_subtract(pkt, &index, lut_id, 32);
			cmdq_op_add(pkt, &lut_status, assign_status1, 0);
		cmdq_op_end_if(pkt);

		cmdq_op_left_shift(pkt, &index, 1, index);
		cmdq_op_and(pkt, &result, lut_status, index);

		cmdq_op_if(pkt, result, CMDQ_EQUAL, 0);
			cmdq_op_add(pkt, &lut_id, lut_id, 1);
			cmdq_op_continue(pkt);
		cmdq_op_end_if(pkt);

		// read lut_id waveform
		cmdq_op_left_shift(pkt, &result, 1, 7);
		cmdq_op_or(pkt, &result, result, lut_id);
		cmdq_op_write_reg(pkt, PIPELINE_LUT_INFO_CLT0, result, BIT_MASK(7) | GENMASK(5, 0));

		cmdq_op_read_reg(pkt, PIPELINE_LUT_INFO_STA2, &wf_mode, GENMASK(3, 0));

		cmdq_op_and(pkt, &wf_mode, wf_mode, BIT_MASK(3));
		cmdq_op_if(pkt, wf_mode, CMDQ_NOT_EQUAL, 0);
			cmdq_op_assign(pkt, &only_collision, 0);
			cmdq_op_break(pkt);
		cmdq_op_end_if(pkt);

		cmdq_op_add(pkt, &lut_id, lut_id, 1);
	cmdq_op_end_while(pkt);

	cmdq_op_if(pkt, only_collision, CMDQ_EQUAL, 1);
		hwtcon_core_config_buffer_index(pkt);
		cmdq_op_add(pkt, &result, 1, ((1LL << 62) | 200));
	cmdq_op_end_if(pkt);
#endif

}

void hwtcon_core_compose_histogram_command(
	struct cmdqRecStruct *pkt, struct hwtcon_task *task)
{
	CMDQ_VARIABLE auto_wf_mode = 0;
	CMDQ_VARIABLE assign_lut0 = 0;
	CMDQ_VARIABLE assign_lut1 = 0;
	CMDQ_VARIABLE result = 0;
	CMDQ_VARIABLE write_val = 0;
	CMDQ_VARIABLE lut_id_bit_mask = 0;
	CMDQ_VARIABLE modified_lut_flag0 = 0;	/* for debug */
	CMDQ_VARIABLE modified_lut_flag1 = 0;	/* for debug */
	CMDQ_VARIABLE assigned_lut_flag0 = 0;	/* for debug */
	CMDQ_VARIABLE assigned_lut_flag1 = 0;	/* for debug */
	CMDQ_VARIABLE i = 0;
	CMDQ_VARIABLE lut_id = 0;

	cmdq_op_assign(pkt, &auto_wf_mode, 0);
	cmdq_op_assign(pkt, &assign_lut0, 0);
	cmdq_op_assign(pkt, &assign_lut1, 0);
	cmdq_op_assign(pkt, &result, 0);
	cmdq_op_assign(pkt, &write_val, 0);
	cmdq_op_assign(pkt, &lut_id_bit_mask, 0);
	cmdq_op_assign(pkt, &modified_lut_flag0, 0);
	cmdq_op_assign(pkt, &modified_lut_flag1, 0);
	cmdq_op_assign(pkt, &assigned_lut_flag0, 0);
	cmdq_op_assign(pkt, &assigned_lut_flag1, 0);
	cmdq_op_assign(pkt, &i, 0);
	cmdq_op_assign(pkt, &lut_id, 0);


	cmdq_core_calculate_auto_wf_mode(pkt, task, &auto_wf_mode);
	cmdq_op_read_reg(pkt, PIPELINE_ASSIGN_STATUS0, &assign_lut0, 0xFFFFFFFF);
	cmdq_op_read_reg(pkt, PIPELINE_ASSIGN_STATUS1, &assign_lut1, 0xFFFFFFFF);

	/* wait the right time to modify waveform mode */
	cmdqRecPoll(pkt, WF_LUT_DPI_STATUS, hw_tcon_get_wf_lut_modify_vcounter(), GENMASK(12, 0));

	/* LUT 0 ~ 31 */
	cmdq_op_assign(pkt, &i, 0);
	cmdq_op_while(pkt, i, CMDQ_LESS_THAN, 32);
		cmdq_op_add(pkt, &lut_id, i, 0);	/* lut_id = i */
		cmdq_op_left_shift(pkt, &lut_id_bit_mask, 1, i);
		cmdq_op_and(pkt, &result, assign_lut0, lut_id_bit_mask);
		cmdq_op_xor(pkt, &result, result, lut_id_bit_mask);
		cmdq_op_if(pkt, result, CMDQ_NOT_EQUAL, 0);
			cmdq_op_add(pkt, &i, i, 1);	/* i++ */
			cmdq_op_continue(pkt);
		cmdq_op_end_if(pkt);
		/* lut id is assigned */
		cmdq_op_or(pkt, &assigned_lut_flag0, assigned_lut_flag0, lut_id_bit_mask);
		cmdq_core_compose_modify_wf_mode_command(pkt, i, lut_id, auto_wf_mode, &modified_lut_flag0);
		cmdq_op_add(pkt, &i, i, 1);	/* i++ */
	cmdq_op_end_while(pkt);
#if 1
	/* LUT 32 ~ 63 */
	cmdq_op_assign(pkt, &i, 0);
	cmdq_op_while(pkt, i, CMDQ_LESS_THAN, 32);
		cmdq_op_add(pkt, &lut_id, i, 32);	/* lut_id = i + 32 */
		cmdq_op_left_shift(pkt, &lut_id_bit_mask, 1, i);
		cmdq_op_and(pkt, &result, assign_lut1, lut_id_bit_mask);
		cmdq_op_xor(pkt, &result, result, lut_id_bit_mask);
		cmdq_op_if(pkt, result, CMDQ_NOT_EQUAL, 0);
			cmdq_op_add(pkt, &i, i, 1);	/* i++ */
			cmdq_op_continue(pkt);
		cmdq_op_end_if(pkt);
		cmdq_op_or(pkt, &assigned_lut_flag1, assigned_lut_flag1, lut_id_bit_mask);
		cmdq_core_compose_modify_wf_mode_command(pkt, i, lut_id, auto_wf_mode, &modified_lut_flag1);
		cmdq_op_add(pkt, &i, i, 1);	/* i++ */
	cmdq_op_end_while(pkt);
#endif


	/* trigger WF_LUT shadow reg */
	pp_write_mask(pkt, WF_LUT_CON, 1 << 14, BIT_MASK(14));
	pp_write_mask(pkt, WF_LUT_CON, 0 << 14, BIT_MASK(14));


	/* record lut_flag for debug */
	if (task) {
		cmdq_op_backup_CPR(pkt, modified_lut_flag0, task->slot, SLOT_PIPELINE_MODIFY_LUT_FLAG0);
		cmdq_op_backup_CPR(pkt, modified_lut_flag1, task->slot, SLOT_PIPELINE_MODIFY_LUT_FLAG1);
		cmdq_op_backup_CPR(pkt, assigned_lut_flag0, task->slot, SLOT_PIPELINE_ASSIGNED_LUT_FLAG0);
		cmdq_op_backup_CPR(pkt, assigned_lut_flag1, task->slot, SLOT_PIPELINE_ASSIGNED_LUT_FLAG1);
	}
}


#ifndef HWTCON_USE_AUTO_LUT_MERGE
int hwtcon_core_software_trigger_lut_merge_sof(struct cmdqRecStruct *pkt)
{
	int count = 0;
	const int MAX_DELAY_COUNT = 10000;
	unsigned long flags;

	/* trigger lut merge sof */
	pp_write_mask(pkt, WF_LUT_TRIG, 1 << 3, BIT_MASK(3));
	pp_write_mask(pkt, WF_LUT_TRIG, 0 << 3, BIT_MASK(3));

	/* wait lut merge done */
	spin_lock_irqsave(&hwtcon_fb_info()->hwtcon_tcon_reg_lock,
			flags);
	pp_write_mask(pkt, WF_LUT_DEBUG_MON_SEL,
		0x10 << 8 | 0x16 << 1 | 1 << 0,
		GENMASK(15, 8) | GENMASK(5, 1) | BIT_MASK(0));
	while (((pp_read(hwtcon_driver_get_wf_lut_va() + 0x1D8)
		& BIT_MASK(0)) == 0) && count++ < MAX_DELAY_COUNT) {
		pp_write_mask(pkt,
			WF_LUT_DEBUG_MON_SEL,
			0x10 << 8 | 0x16 << 1 | 1 << 0,
			GENMASK(15, 8) | GENMASK(5, 1) | BIT_MASK(0));
	}
	spin_unlock_irqrestore(
			&hwtcon_fb_info()->hwtcon_tcon_reg_lock,
			flags);

	if (count == MAX_DELAY_COUNT) {
		TCON_ERR("wait timeout:41D4:0x%08x 41D8:0x%08x",
			pp_read(hwtcon_driver_get_wf_lut_va() + 0x1D4),
			pp_read(hwtcon_driver_get_wf_lut_va() + 0x1D8));
		WARN_ON(1);
		return -1;
	}

	return 0;
}
#endif

static struct cmdqRecStruct *g_lut_assign_done_handle;
static struct cmdqRecStruct *g_auto_collision_handle;
void hwtcon_core_start_lut_assign_done_trigger_loop(void)
{
	if (g_lut_assign_done_handle) {
		TCON_ERR("lut assign done trigger loop already started");
		return;
	}

	TCON_LOG("start trigger loop");

	//cmdqCoreClearEvent(CMDQ_EVENT_LUT_ASSIGN_DONE);
	cmdqRecCreate(CMDQ_SCENARIO_HWTCON_LUT_END_LOOP,
		&g_lut_assign_done_handle);
	cmdqRecReset(g_lut_assign_done_handle);
	cmdqRecClearEventToken(g_lut_assign_done_handle,
		CMDQ_EVENT_LUT_ASSIGN_DONE);
	cmdqRecWait(g_lut_assign_done_handle, CMDQ_EVENT_LUT_ASSIGN_DONE);

	/* trigger lut merge sof */
	pp_write_mask(g_lut_assign_done_handle,
		WF_LUT_TRIG, 1 << 3, BIT_MASK(3));
	pp_write_mask(g_lut_assign_done_handle,
		WF_LUT_TRIG, 0 << 3, BIT_MASK(3));
	cmdqRecStartLoop(g_lut_assign_done_handle);
}

void hwtcon_core_stop_lut_assign_done_trigger_loop(void)
{
	cmdqRecStopLoop(g_lut_assign_done_handle);
	cmdqRecDestroy(g_lut_assign_done_handle);
	TCON_LOG("stop trigger loop");
	g_lut_assign_done_handle = NULL;
}

void hwtcon_core_start_auto_collision_trigger_loop(void)
{
	if (g_auto_collision_handle) {
		TCON_ERR("lut assign done trigger loop already started");
		return;
	}

	TCON_LOG("start trigger loop");

	cmdqRecCreate(CMDQ_SCENARIO_HWTCON_AUTO_COLLISION_LOOP,
		&g_auto_collision_handle);
	cmdqRecReset(g_auto_collision_handle);
	cmdqRecClearEventToken(g_auto_collision_handle,
		CMDQ_EVENT_WB_WDMA_DONE);
	cmdqRecWait(g_auto_collision_handle, CMDQ_EVENT_WB_WDMA_DONE);
	//hwtcon_core_compose_auto_trigger_command(g_auto_collision_handle);
	hwtcon_core_compose_histogram_command(g_auto_collision_handle, NULL);
	hwtcon_core_config_buffer_index(g_auto_collision_handle);
	cmdqRecSetEventToken(g_auto_collision_handle, CMDQ_SYNC_HWTCON_WDMA_FRAME_DONE);
	cmdqRecStartLoop(g_auto_collision_handle);
}

void hwtcon_core_stop_auto_collision_trigger_loop(void)
{
	cmdqRecStopLoop(g_auto_collision_handle);
	cmdqRecDestroy(g_auto_collision_handle);
	TCON_LOG("stop trigger loop");
	g_auto_collision_handle = NULL;
}


irqreturn_t hwtcon_core_pipeline_irq_handle(int irq, void *dev)
{
	enum WAVEFORM_MODE_ENUM waveform_mode = 0;
	struct rect region = {0};
	int lut_id = 0;
	u64 lut_status = pipeline_get_assigned_lut_status();

#ifndef HWTCON_USE_AUTO_LUT_MERGE
	/* sw trigger lut merge sof */
	//hwtcon_core_software_trigger_lut_merge_sof(NULL);
#endif
	for (lut_id = 0; lut_id < MAX_LUT_REGION_COUNT; lut_id++) {
		if (lut_status & BIT_ULL_MASK(lut_id)) {
			pipeline_get_unreleased_lut_info(NULL,
				lut_id,
				NULL,
				&waveform_mode, &region);
			if (region.width == 0 || region.height == 0)
				TCON_ERR("invalid lut ID:%d mode:%d region[%d %d %d %d]",
					lut_id, waveform_mode, region.x, region.y, region.width, region.height);
		}
	}
	pipeline_config_clear_irq(NULL, PIPELINE_IRQ_ASSIGN_DONE);
	return IRQ_HANDLED;
}

irqreturn_t hwtcon_core_wb_wdma_irq_handle(int irq, void *dev)
{
	int lut_id = 0;
	u32 irq_status = wdma_config_get_irq_status();
	u64 lut_status = pipeline_get_assigned_lut_status();
	enum WAVEFORM_MODE_ENUM waveform_mode = 0;
	struct rect region = {0};
	bool only_collision = true;

	if (irq_status & BIT_MASK(1))
		TCON_ERR("WB WDMA under run");

	for (lut_id = 0; lut_id < MAX_LUT_REGION_COUNT; lut_id++) {
		if (lut_status & BIT_ULL_MASK(lut_id)) {
			pipeline_get_unreleased_lut_info(NULL,
				lut_id,
				NULL,
				&waveform_mode, &region);
			/* trigger by sw */
			if (waveform_mode & BIT_MASK(3)) {
				only_collision = false;
			}
		}
	}

	if (only_collision) {
		TCON_LOG("only_collision switch buffer index: dump all");
		for (lut_id = 0; lut_id < MAX_LUT_REGION_COUNT; lut_id++) {
			if (lut_status & BIT_ULL_MASK(lut_id)) {
				pipeline_get_unreleased_lut_info(NULL,
					lut_id,
					NULL,
					&waveform_mode, &region);
				TCON_LOG("lut id:%d waveform mode:%d region: [%d %d %d %d]",
					lut_id, waveform_mode, region.x, region.y, region.width, region.height);
			}
		}
		hwtcon_fb_info()->pipeline_busy = false;
		wake_up(&hwtcon_fb_info()->pipeline_trigger_wait_queue);
		//hwtcon_core_config_buffer_index(NULL);
	}
	pp_write_mask(NULL, WF_LUT_DPI_OUTPUT_SETTING, 0 << 16, BIT_MASK(16));


	/* clear irq status. */
	wdma_config_clear_irq_status(NULL);

	return IRQ_HANDLED;
}

irqreturn_t hwtcon_core_wf_lut_dpi_irq_handle(int irq, void *dev)
{
	if (hwtcon_debug_get_info()->enable_wf_lut_dpi_checksum)
		TCON_ERR("dpi:0x%08x wf_lut:0x%08x wf_mode:0x%08x lut cnt:%d D70[0x%08x 0x%08x 0x%08x 0x%08x]",
			wf_lut_dpi_get_checksum(NULL),
			wf_lut_get_wf_lut_output_checksum(),
			hwtcon_core_read_wf_lut_waveform_mode(),
			hwtcon_core_read_wf_frame_count(0),
			pp_read(WF_LUT_CHKSUM_0_VA),
			pp_read(WF_LUT_CHKSUM_1_VA),
			pp_read(WF_LUT_CHKSUM_2_VA),
			pp_read(WF_LUT_CHKSUM_3_VA));

	/* clear irq status. */
	wf_lut_dpi_clear_irq_status(NULL);
	/* wake_up(&hwtcon_fb_info()->hwtcon_irq_clear_wait_queue); */

	return IRQ_HANDLED;
}

#ifdef HWTCON_ENABLE_WF_LUT_IRQ
irqreturn_t hwtcon_core_wf_lut_irq_handle(int irq, void *dev)
{
	unsigned long flags;
	enum WAVEFORM_MODE_ENUM waveform_mode = 0;
	struct rect region = {0};
	int lut_id = 0;
	u64 lut_status = pipeline_get_assigned_lut_status();

	TCON_ERR("current assign lut:0x%016llx", lut_status);

	for (lut_id = 0; lut_id < MAX_LUT_REGION_COUNT; lut_id++) {
		if (lut_status & BIT_ULL_MASK(lut_id)) {
			pipeline_get_unreleased_lut_info(NULL,
				lut_id,
				NULL,
				&waveform_mode, &region);
			if (region.width == 0 || region.height == 0)
				TCON_ERR("invalid lut ID:%d mode:%d region[%d %d %d %d] assign status:0x%016llx",
					lut_id, waveform_mode, region.x, region.y, region.width, region.height, lut_status);
		}
	}

#if 0
	TCON_ERR("wf_lut irq:0x%08x checksum 0x%08x wf_mode:0x%08x lut cnt:%d D70[0x%08x 0x%08x 0x%08x 0x%08x]",
		wf_lut_get_irq_status(),
		wf_lut_get_wf_lut_output_checksum(),
		hwtcon_core_read_wf_lut_waveform_mode(),
		hwtcon_core_read_wf_frame_count(0),
		pp_read(WF_LUT_CHKSUM_0_VA),
		pp_read(WF_LUT_CHKSUM_1_VA),
		pp_read(WF_LUT_CHKSUM_2_VA),
		pp_read(WF_LUT_CHKSUM_3_VA));
#endif
	/*read wf_lut waveform mode in wf_lut irq */
	spin_lock_irqsave(&hwtcon_fb_info()->hwtcon_tcon_reg_lock,
			flags);
	pp_write_mask(NULL, 0x140041D4, 1 << 0, BIT_MASK(0));
	TCON_LOG("lut counter: 0x%08x assign lut status:0x%016llx",
		pp_read(hwtcon_driver_get_wf_lut_va() + 0x240),
		pipeline_get_lut_status());
	spin_unlock_irqrestore(
			&hwtcon_fb_info()->hwtcon_tcon_reg_lock,
			flags);

	/* clear irq status. */
	wf_lut_clear_irq_status(NULL);
	wake_up(&hwtcon_fb_info()->hwtcon_irq_clear_wait_queue);
	return IRQ_HANDLED;
}
#endif

irqreturn_t hwtcon_core_wf_lut_end_irq_handle(int irq, void *dev)
{
	hwtcon_core_handle_wf_lut_release();

#ifndef HWTCON_CLK_ALWAYS_ON
	if (hwtcon_core_check_hwtcon_idle()) {
		/* hwtcon_core_handle_clock_disable */
		queue_work(hwtcon_fb_info()->wq_disable_clk,
			&hwtcon_fb_info()->wk_disable_clk);
	}
#endif

	/* clear irq status. */
	wf_lut_clear_lut_end_irq_status(NULL);

	return IRQ_HANDLED;
}

irqreturn_t hwtcon_core_disp_rdma_irq_handle(int irq, void *dev)
{
	/* clear irq status. */
	wf_lut_clear_disp_rdma_irq_status(NULL);

	return IRQ_HANDLED;
}


