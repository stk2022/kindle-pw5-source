#include <linux/delay.h>

#include "hwtcon_core.h"
#include "hwtcon_def.h"
#include "hwtcon_fb.h"
#include "hwtcon_debug.h"
#include "hwtcon_file.h"
#include "hwtcon_driver.h"
#include "hwtcon_hal.h"
#include "fiti_core.h"
#include "hwtcon_epd.h"
#include "hwtcon_mdp.h"
#include "hwtcon_pipeline_config.h"
#include "hwtcon_wf_lut_config.h"
#include "hwtcon_dpi_config.h"
#include "hwtcon_rect.h"
#include "mtk_imgrz_ext.h"
#include "hwtcon_lightbox.h"

extern u32 lut_frame_count[64];
extern bool lut_crc_all_zero[64];

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

	if (!hwtcon_debug_get_info()->sw_mitigation) {
		return res;
	}
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
			pixel_start = hwtcon_fb_info()->fb_buffer_va + top * stride + left + col;
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
			pixel_start = hwtcon_fb_info()->fb_buffer_va + (top + row) * stride + left;
			do_pixel_line((void*)pixel_start, &strength, &transition[scan_line], width);
			TCON_LOG("landscape transition neon [%d]=%d strength=%d", scan_line, transition[scan_line], strength);
		}
		kernel_neon_end();
#else
		for (row = scan_stride, scan_line = 0; row < height; row += scan_stride, scan_line++) {
			/* skip the first line on the screen */
			transition[scan_line] = 0;
			/* need to change to fb_buffer_va in the V2 driver */
			pixel_start = hwtcon_fb_info()->fb_buffer_va  + (top + row) * stride + left;
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
		down_param.src_info.dma_buf = hwtcon_fb_info()->fb_buffer_pa;

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

static struct swipe_info_struct hwtcon_fb_get_swipe_info(struct mxcfb_update_data *update_data)
{
	struct swipe_info_struct swipe_info = {0};

	swipe_info.enable = (update_data->flags & EPDC_FLAG_ENABLE_SWIPE) ? true : false;
	swipe_info.direction = update_data->swipe_data.direction;
	swipe_info.count = update_data->swipe_data.steps;

	return swipe_info;
}

void hwtcon_core_fiti_power_enable(bool enable)
{
	if (enable) {
		fiti_power_enable(true);
		return;
	}

	/* close fiti power
	 * check still remain task in task list.
	 * -> Yes: return directly
	 * -> No: close fiti power
	 */
	if (hwtcon_core_check_hwtcon_idle())
		fiti_power_enable(false);
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
	wf_lut_parse_wf_file(hwtcon_fb_info()->waveform_va);
	if (!fiti_pmic_judge_power_on_going())
		fiti_setting_get_from_waveform(hwtcon_fb_info()->waveform_va);
	else {
		TCON_ERR("Can't access fiti when fiti power state in POWER_ON_GOING");
		hwtcon_fb_info()->hwtcon_first_call = false;
	}

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
		hwtcon_core_put_task_callback(task);
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

		/* modify task state */
		task->state = dst_state;
		if (dst_lock)
			spin_unlock_irqrestore(&dst_task_list->lock, flags);
	}

	#if 0
	TCON_ERR("change task:0x%llx from:%d to %d",
		task->unique_id,
		task->state, dst_state);
	#endif

	wake_up(&hwtcon_fb_info()->task_state_wait_queue);
}

void hwtcon_core_put_task_with_lut_release(struct hwtcon_task *task)
{
	/* hwtcon_core_put_task */
	queue_work(hwtcon_fb_info()->wq_wf_lut_display_done,
		&task->work_display_done);
}

void hwtcon_core_put_task_callback(struct hwtcon_task *task)
{
	task->assign_lut = -1;
	/* hwtcon_core_put_task */
	queue_work(hwtcon_fb_info()->wq_wf_lut_display_done,
		&task->work_display_done);
}

void hwtcon_core_put_task(struct hwtcon_task *task)
{
	struct update_marker_struct *update_marker, *tmp;
	unsigned long flags;

	wake_up(&hwtcon_fb_info()->task_state_wait_queue);
	hwtcon_debug_record_printf(
		"task:0x%llx [%03d %03d %03d %03d] regal:%s swipe:[%d %d %d] mode:%02d->%s wf_cnt:%d ",
		task->unique_id,
		hwtcon_core_get_task_region(task).x,
		hwtcon_core_get_task_region(task).y,
		hwtcon_core_get_task_region(task).width,
		hwtcon_core_get_task_region(task).height,
		(task->regal_status == REGAL_STATUS_NON_REGAL) ? "False" : "True",
		task->swipe_info.enable,
		task->swipe_info.count,
		task->swipe_info.direction,
		task->update_data.waveform_mode,
		hwtcon_core_get_wf_mode_name(task->update_data.waveform_mode),
		wf_lut_get_waveform_len(hwtcon_core_read_temp_zone(),
			task->update_data.waveform_mode));

	hwtcon_debug_record_printf(
		"submit %03d trigger_mdp %03d mdp_done %03d trigger_pipeline %03d pipeline_done %03d wait_power_good %03d trigger_wf_lut %03d wf_lut_done total:%03d marker:%d",
		(task->time_submit == 0 || task->time_trigger_mdp == 0) ? -1 :
			hwtcon_hal_get_time_in_ms(task->time_submit, task->time_trigger_mdp),
		(task->time_trigger_mdp == 0 || task->time_mdp_done == 0) ? -1 :
			hwtcon_hal_get_time_in_ms(task->time_trigger_mdp, task->time_mdp_done),
		(task->time_mdp_done == 0 || task->time_trigger_pipeline == 0) ? -1 :
			hwtcon_hal_get_time_in_ms(task->time_mdp_done, task->time_trigger_pipeline),
		(task->time_trigger_pipeline == 0 || task->time_pipeline_done == 0) ? -1 :
			hwtcon_hal_get_time_in_ms(task->time_trigger_pipeline, task->time_pipeline_done),
		(task->time_pipeline_done == 0 || task->time_wait_fiti_power_good == 0) ? -1 :
			hwtcon_hal_get_time_in_ms(task->time_pipeline_done, task->time_wait_fiti_power_good),
		(task->time_wait_fiti_power_good == 0 || task->time_trigger_wf_lut == 0) ? -1 :
			hwtcon_hal_get_time_in_ms(task->time_wait_fiti_power_good, task->time_trigger_wf_lut),
		(task->time_trigger_wf_lut == 0 || task->time_wf_lut_done == 0) ? -1 :
			hwtcon_hal_get_time_in_ms(task->time_trigger_wf_lut, task->time_wf_lut_done),
		(task->time_submit == 0 || task->time_wf_lut_done == 0) ? -1 :
			hwtcon_hal_get_time_in_ms(task->time_submit, task->time_wf_lut_done),
		task->update_data.update_marker);

	hwtcon_debug_record_printf("\n");

	#ifndef MARKER_V2_ENABLE
	/* signal update_marker */
	list_for_each_entry_safe(update_marker, tmp,
		&task->marker_info_list.list, task_list) {
		unsigned long flags;
		bool release_marker = false;

		spin_lock_irqsave(&hwtcon_fb_info()->fb_global_marker_list.lock, flags);
		list_del_init(&update_marker->global_list);
		spin_unlock_irqrestore(&hwtcon_fb_info()->fb_global_marker_list.lock, flags);

		spin_lock_irqsave(&update_marker->marker_lock, flags);

		if (update_marker->waiters) {
			/* have waiters, free this completion on waiter thread */
			update_marker->need_release = true;
		} else {
			/* no waiters, free here */
			release_marker = true;
		}
		spin_unlock_irqrestore(&update_marker->marker_lock, flags);

		store_completed_markers(&hwtcon_fb_info()->marker_store ,update_marker->update_marker); /* store completed marker*/

		complete(&update_marker->submit_completion);
		complete(&update_marker->update_completion);

		if (release_marker)
			vfree(update_marker);
	}
	complete_markers_avail(&hwtcon_fb_info()->marker_store); /* Signal completed marker available */
	#else
	/* signal update_marker */
	list_for_each_entry_safe(update_marker, tmp,
		&hwtcon_fb_info()->fb_global_marker_list.list, global_list) {
		unsigned long flags;
		bool release_marker = false;
		bool signal_marker = false;

		spin_lock_irqsave(&update_marker->marker_lock, flags);
		if (task->assign_lut < MAX_LUT_REGION_COUNT)
			update_marker->lut_mask &= ~(1LL << task->assign_lut);
		signal_marker = ((update_marker->lut_mask == 0LL) &&
			(update_marker->marker_state == MARKER_STATE_LUT_ASSIGNED));
		spin_unlock_irqrestore(&update_marker->marker_lock, flags);

		if (signal_marker) {
			HWTCON_TIME end_time = timeofday_ms();

			spin_lock_irqsave(&hwtcon_fb_info()->fb_global_marker_list.lock, flags);
			list_del_init(&update_marker->global_list);
			spin_unlock_irqrestore(&hwtcon_fb_info()->fb_global_marker_list.lock, flags);

			spin_lock_irqsave(&update_marker->marker_lock, flags);
			if (update_marker->waiters) {
				/* have waiters, free this completion on waiter thread */
				update_marker->need_release = true;
			} else {
				/* no waiters, free here */
				release_marker = true;
			}
			spin_unlock_irqrestore(&update_marker->marker_lock, flags);
			TCON_LOG("[MARKER] update_completion: %d", update_marker->update_marker);
			TCON_EPDC("[%d] update end marker=%d, end time=%lld, time taken=%d ms",
					update_marker->update_marker,
					update_marker->update_marker,
					end_time,
					hwtcon_hal_get_time_in_ms(
						update_marker->start_time,
						end_time));

			store_completed_markers(&hwtcon_fb_info()->marker_store ,update_marker->update_marker); /* store completed marker*/

			complete(&update_marker->submit_completion);
			complete(&update_marker->update_completion);
		}

		if (release_marker)
			vfree(update_marker);
		complete_markers_avail(&hwtcon_fb_info()->marker_store); /* Signal completed marker available */
	}
	#endif
	spin_lock_irqsave(&hwtcon_fb_info()->g_update_order_lock, flags);
	hwtcon_fb_info()->g_update_cnt = hwtcon_fb_info()->g_update_cnt - 1;
	spin_unlock_irqrestore(&hwtcon_fb_info()->g_update_order_lock, flags);

	if (task) {
		del_timer(&task->mdp_debug_timer);
		vfree(task);
	}

}

static void hwtcon_core_handle_task_display_done(struct work_struct *work_item)
{
	struct hwtcon_task *task = container_of(work_item,
	       struct hwtcon_task,
	       work_display_done);

	/* remove task to free task list. */
	hwtcon_core_put_task(task);
}

void hwtcon_core_handle_mdp_debug_timer_cb(unsigned long param)
{
	wake_up(&hwtcon_fb_info()->pipeline_trigger_wait_queue);
}

void hwtcon_core_handle_lut_release_timeout_cb(unsigned long param)
{
	struct hwtcon_task *task = NULL;
	struct hwtcon_task *tmp = NULL;
	int lut_id = param;
	unsigned long flags;
	int release_task_found = false;

	/* search the release task */
	spin_lock_irqsave(&hwtcon_fb_info()->pipeline_done_task_list.lock, flags);
	list_for_each_entry_safe(task, tmp,
		&hwtcon_fb_info()->pipeline_done_task_list.list, list) {
		if (task->assign_lut == lut_id) {
			release_task_found = true;
			break;
		}
	}
	spin_unlock_irqrestore(&hwtcon_fb_info()->pipeline_done_task_list.lock, flags);


	if (!release_task_found)
		TCON_ERR("LUT[%d] release timeout", lut_id);
	else
		TCON_ERR("LUT[%d] timeout on task marker[%d] region[%d %d %d %d]",
			task->assign_lut,
			task->update_data.update_marker,
			hwtcon_core_get_task_region(task).x,
			hwtcon_core_get_task_region(task).y,
			hwtcon_core_get_task_region(task).width,
			hwtcon_core_get_task_region(task).height);
	hwtcon_driver_handle_released_lut_flag(1LL << lut_id);
}

static void print_task_top_10(char *task_name, struct hwtcon_task_list *task_list)
{
	struct hwtcon_task *task = NULL;
	struct hwtcon_task *tmp = NULL;
	int count = 0;
	unsigned long flags = 0;

	TCON_ERR("dump top 10 tasks in list:%s begin", task_name);

	spin_lock_irqsave(&task_list->lock, flags);
	list_for_each_entry_safe(task, tmp,
		&task_list->list, list) {
		if (count > 10)
			break;
		TCON_ERR("task[%d] region[%d %d %d %d] update_mode[%d] regal[%d %d] swipe[%d %d %d] wf[%d]",
			count++,
			task->update_data.update_region.left,
			task->update_data.update_region.top,
			task->update_data.update_region.width,
			task->update_data.update_region.height,
			task->update_data.update_mode,
			task->regal_status,
			task->regal_mode,
			task->swipe_info.enable,
			task->swipe_info.direction,
			task->swipe_info.count,
			task->update_data.waveform_mode);
	}
	spin_unlock_irqrestore(&task_list->lock, flags);

	TCON_ERR("dump top 10 tasks in list:%s end", task_name);

}

static struct hwtcon_task *hwtcon_core_get_task(
	struct mxcfb_update_data *update_data,
	bool create_marker)
{
	struct hwtcon_task *task = NULL;
	struct update_marker_struct *marker = NULL;
	unsigned long flags;

	task = vzalloc(sizeof(struct hwtcon_task));
	if (task == NULL) {
		static int count = 0;
		TCON_ERR("vmalloc task fail %d", count);
		if (count == 0) {
			count++;
			TCON_ERR("lut free 0x%016llx active 0x%016llx release 0x%016llx hardware active:0x%08x 0x%08x",
				hwtcon_fb_info()->lut_free,
				hwtcon_fb_info()->lut_active,
				hwtcon_core_get_released_lut(),
				pp_read(WF_LUT_EN_STA1_VA),
				pp_read(WF_LUT_EN_STA0_VA));
			TCON_ERR("pipeline_processing_task_list count:%d",
				hwtcon_core_get_task_count(&hwtcon_fb_info()->pipeline_processing_task_list.list));
			TCON_ERR("pipeline_done_task_list count:%d",
				hwtcon_core_get_task_count(&hwtcon_fb_info()->pipeline_done_task_list.list));
			TCON_ERR("wait_for_mdp_task_list count:%d",
				hwtcon_core_get_task_count(&hwtcon_fb_info()->wait_for_mdp_task_list.list));
			TCON_ERR("mdp_done_task_list count:%d",
				hwtcon_core_get_task_count(&hwtcon_fb_info()->mdp_done_task_list.list));
			TCON_ERR("collision_task_list count:%d",
				hwtcon_core_get_task_count(&hwtcon_fb_info()->collision_task_list.list));

			print_task_top_10("pipeline_processing_task_list", &hwtcon_fb_info()->pipeline_processing_task_list);
			print_task_top_10("pipeline_done_task_list", &hwtcon_fb_info()->pipeline_done_task_list);
			print_task_top_10("wait_for_mdp_task_list", &hwtcon_fb_info()->wait_for_mdp_task_list);
			print_task_top_10("mdp_done_task_list", &hwtcon_fb_info()->mdp_done_task_list);
			print_task_top_10("collision_task_list", &hwtcon_fb_info()->collision_task_list);
		}
		return NULL;
	}
	/* init task member */
	/*
		struct mxcfb_update_data update_data;

		u32 update_order;

		enum REGAL_STATUS_ENUM regal_status;
		enum REGAL_MODE_ENUM regal_mode;

		struct swipe_info_struct swipe_info;

		struct pipeline_info pipeline_info;
		enum HWTCON_TASK_STATE state;

		u64 lut_dependency;
		u32 assign_lut;

		struct list_head marker_info_list;
		struct list_head list;
	*/
	task->update_data = *update_data;

	/* assign task's update order */
	spin_lock_irqsave(&hwtcon_fb_info()->g_update_order_lock, flags);
	if (hwtcon_fb_info()->g_update_order == MAX_PIC_ORDER)
		TCON_ERR("g_update_order will be out range!!!");
	task->update_order = hwtcon_fb_info()->g_update_order++;
	hwtcon_fb_info()->g_update_cnt = hwtcon_fb_info()->g_update_cnt + 1;
	spin_unlock_irqrestore(&hwtcon_fb_info()->g_update_order_lock, flags);
	task->pic_order = task->update_order;

	/* regal setting */
	if (hwtcon_core_use_regal(update_data, &task->regal_mode))
		task->regal_status = REGAL_STATUS_REGAL;
	else
		task->regal_status = REGAL_STATUS_NON_REGAL;

	/* swipe setting */
	task->swipe_info = hwtcon_fb_get_swipe_info(&task->update_data);
	TCON_LOG("swipe info en=%d dir=%d cnt=%d\n",
		task->swipe_info.enable,
		task->swipe_info.direction,
		task->swipe_info.count);
	/* TODO: maybe need to change */
	if (task->swipe_info.enable) {
		task->regal_status = REGAL_STATUS_REGAL;
		task->regal_mode = REGAL_MODE_REGAL;
		task->update_data.waveform_mode = hwtcon_core_use_night_mode() ?
			WAVEFORM_MODE_GLKW16 : WAVEFORM_MODE_GLR16;
	}

	memset(&task->pipeline_info, 0, sizeof(task->pipeline_info));
	task->state = TASK_STATE_FREE;
	task->lut_dependency = 0LL;
	task->assign_lut = -1;
	INIT_LIST_HEAD(&task->list);
	spin_lock_init(&task->marker_info_list.lock);
	INIT_LIST_HEAD(&task->marker_info_list.list);

	if (create_marker) {
		/* allocate marker info */
		marker = hwtcon_core_alloc_update_marker();
		if (marker == NULL) {
			/* free task resource */
			vfree(task);
			return NULL;
		}

		marker->update_marker = update_data->update_marker;
		list_add_tail(&marker->task_list, &task->marker_info_list.list);
		spin_lock_irqsave(&hwtcon_fb_info()->fb_global_marker_list.lock, flags);
		list_add_tail(&marker->global_list, &hwtcon_fb_info()->fb_global_marker_list.list);
		spin_unlock_irqrestore(&hwtcon_fb_info()->fb_global_marker_list.lock, flags);
	}

	/* init task info */
	task->unique_id = sched_clock();
	task->time_submit = timeofday_ms();


	INIT_WORK(&task->work_written_done,
		hwtcon_core_handle_task_written_done);

	INIT_WORK(&task->work_display_done,
		hwtcon_core_handle_task_display_done);

	/* start timer for mdp merge simulate debug. */
	setup_timer(&task->mdp_debug_timer,
		hwtcon_core_handle_mdp_debug_timer_cb,
		0L);

	return task;
}

/* check pipeline & wf_lut idle */
bool hwtcon_core_check_hwtcon_idle(void)
{
	if (hwtcon_fb_info()->lut_active != 0LL)
		return false;

	if (!list_empty(&hwtcon_fb_info()->collision_task_list.list)){
		TCON_WARN("collision task(s) is pending but there is no active LUT");
		hwtcon_core_update_collision_list_on_release_lut(0LL);
		return false;
	}

	/* check if all task list Empty */
	if (!list_empty(&hwtcon_fb_info()->wait_for_mdp_task_list.list) ||
		!list_empty(&hwtcon_fb_info()->mdp_done_task_list.list) ||
		!list_empty(&hwtcon_fb_info()->pipeline_processing_task_list.list) ||
		!list_empty(&hwtcon_fb_info()->pipeline_done_task_list.list) ||
		!list_empty(&hwtcon_fb_info()->collision_task_list.list))
		return false;

	return true;
}

void hwtcon_core_handle_clock_disable(void)
{
	if (hwtcon_core_check_hwtcon_idle() == false)
		return;

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
	hwtcon_driver_enable_mmsys_power(false);
}

void hwtcon_core_handle_mmsys_power_down_cb(unsigned long param)
{
	/* hwtcon_core_handle_mmsys_power_down */
	queue_work(hwtcon_fb_info()->wq_power_down_mmsys,
		&hwtcon_fb_info()->wk_power_down_mmsys);
}

int hwtcon_core_wait_for_task_triggered(u32 update_marker)
{
#if 1
	struct update_marker_struct *marker, *tmp;
	bool found = false;
	unsigned long flags;

	/* search global marker list */
	spin_lock_irqsave(&hwtcon_fb_info()->fb_global_marker_list.lock, flags);
	list_for_each_entry_safe(marker, tmp,
		&hwtcon_fb_info()->fb_global_marker_list.list, global_list) {
		if (marker->update_marker == update_marker) {
			found = true;
			spin_lock_irqsave(&marker->marker_lock, flags);
			marker->waiters++;
			spin_unlock_irqrestore(&marker->marker_lock, flags);
			break;
		}
	}
	spin_unlock_irqrestore(&hwtcon_fb_info()->fb_global_marker_list.lock, flags);

	TCON_LOG("marker found, waiting for submission completion");

	if (found) {
		bool release_marker = false;

		if (wait_for_completion_timeout(
				&marker->submit_completion,
				msecs_to_jiffies(HWTCON_TASK_WAIT_MARKER_TIMEOUT_MS)) == 0) {
			TCON_ERR("wait marker[%d] submit timeout: mask[0x%016llx] state[%d]",
				marker->update_marker,
				marker->lut_mask,
				marker->marker_state);
		}
		spin_lock_irqsave(&marker->marker_lock, flags);
		if (marker->waiters > 0)
			marker->waiters--;
		release_marker = (marker->waiters == 0) && (marker->need_release);
		spin_unlock_irqrestore(&marker->marker_lock, flags);

		if (release_marker)
			vfree(marker);
	}
#endif
	return 0;
};


int hwtcon_core_wait_for_task_displayed(u32 update_marker)
{
#if 1
	struct update_marker_struct *marker, *tmp;
	bool found = false;
	unsigned long flags;

	/* search global marker list */
	spin_lock_irqsave(&hwtcon_fb_info()->fb_global_marker_list.lock, flags);
	list_for_each_entry_safe(marker, tmp,
		&hwtcon_fb_info()->fb_global_marker_list.list, global_list) {
		if (marker->update_marker == update_marker) {
			found = true;
			spin_lock_irqsave(&marker->marker_lock, flags);
			marker->waiters++;
			spin_unlock_irqrestore(&marker->marker_lock, flags);
			break;
		}
	}
	spin_unlock_irqrestore(&hwtcon_fb_info()->fb_global_marker_list.lock, flags);

	if (found) {
		bool release_marker = false;

		if (wait_for_completion_timeout(
				&marker->update_completion,
				msecs_to_jiffies(HWTCON_TASK_WAIT_MARKER_TIMEOUT_MS)) == 0) {
			TCON_ERR("wait marker[%d] update timeout: mask[0x%016llx] state[%d]",
				marker->update_marker,
				marker->lut_mask,
				marker->marker_state);
		}

		spin_lock_irqsave(&marker->marker_lock, flags);
		if (marker->waiters > 0)
			marker->waiters--;
		release_marker = (marker->waiters == 0) && (marker->need_release);
		spin_unlock_irqrestore(&marker->marker_lock, flags);

		if (release_marker)
			vfree(marker);
	}
#endif
	return 0;
}

int hwtcon_core_convert_temperature(int temp)
{
	return wf_lut_waveform_get_temperature_index(temp);
}

int hwtcon_core_read_temperature(void)
{
	if (hwtcon_debug_get_info()->fixed_temperature == TEMP_USE_AMBIENT)
		return hwtcon_fb_info()->temperature;

	/* used fixed temperature set by command */
	return hwtcon_debug_get_info()->fixed_temperature;
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

void hwtcon_core_insert_task_to_wait_for_mdp_task_list(
	struct hwtcon_task *insert_task,
	int *task_merged)
{
	unsigned long flags;
	struct hwtcon_task *task, *tmp = NULL;
	*task_merged = false;

	spin_lock_irqsave(&hwtcon_fb_info()->wait_for_mdp_task_list.lock, flags);

	/* reverse search wait_for_mdp_task list */
	list_for_each_entry_safe_reverse(task, tmp,
		&hwtcon_fb_info()->wait_for_mdp_task_list.list, list) {
		struct rect task_region = hwtcon_core_get_task_region(task);
		struct rect insert_task_region = hwtcon_core_get_task_region(insert_task);

		/* merge condition:
		 * 1. same update region
		 * 2. same regal setting (regal status & regal mode)
		 * 3. same swipe setting(enable & direction)
		 * 4. same waveform mode & update mode
		 */
		if ((task_region.x == insert_task_region.x) &&
			(task_region.y == insert_task_region.y) &&
			(task_region.width == insert_task_region.width) &&
			(task_region.height == insert_task_region.height) &&
			(task->update_data.update_mode == insert_task->update_data.update_mode) &&
			(task->regal_status == insert_task->regal_status) &&
			(task->update_data.waveform_mode == insert_task->update_data.waveform_mode) &&
			(task->regal_mode == insert_task->regal_mode) &&
			(task->swipe_info.enable == insert_task->swipe_info.enable) &&
			(task->swipe_info.direction == insert_task->swipe_info.direction) &&
			(task->swipe_info.count == insert_task->swipe_info.count)) {

			TCON_LOG("user insert scenario merge task in wait_for_mdp_task_list");
			*task_merged = true;

			TCON_LOG("dump merge marker begin");
			do {
				struct update_marker_struct *update_marker, *tmp_marker;

				list_for_each_entry_safe(update_marker, tmp_marker,
					&task->marker_info_list.list, task_list) {
					TCON_LOG("origin merge marker %d", update_marker->update_marker);
				}

				list_for_each_entry_safe(update_marker, tmp_marker,
					&insert_task->marker_info_list.list, task_list) {
					TCON_LOG("insert merge marker %d", update_marker->update_marker);
				}
			} while (0);
			TCON_LOG("dump merge marker end");

			task->update_order = MIN(insert_task->update_order,
				task->update_order);
			task->pic_order = MAX(insert_task->pic_order,
				task->pic_order);

			/* marker info: move insert_task marker info to task */
			list_splice_init(&insert_task->marker_info_list.list, &task->marker_info_list.list);

			list_del_init(&insert_task->list);
			hwtcon_core_put_task_callback(insert_task);
		} else
			break;
	}
	spin_unlock_irqrestore(&hwtcon_fb_info()->wait_for_mdp_task_list.lock, flags);

	/* don't need merge */
	if (*task_merged == false)
		hwtcon_core_change_task_state(insert_task, TASK_STATE_WAIT_MDP_HANDLE, false, true, INSERT_TO_TAIL);
}


int hwtcon_core_submit_task(struct mxcfb_update_data *update_data)
{
	struct hwtcon_task *task = NULL;
	int task_merged = false;

	hwtcon_core_load_init_setting_from_file();

	mutex_lock(&hwtcon_fb_info()->update_queue_mutex);

	task = hwtcon_core_get_task(update_data, true);
	if (task == NULL) {
		TCON_ERR("hwtcon_core_get_task fail");
		mutex_unlock(&hwtcon_fb_info()->update_queue_mutex);
		return HWTCON_STATUS_GET_TASK_FAIL;
	}

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
	TCON_EPDC("[%d] update start marker=%d, start time=%lld",
                task->update_data.update_marker,
                task->update_data.update_marker,
                task->time_submit);

	hwtcon_core_insert_task_to_wait_for_mdp_task_list(task, &task_merged);

	if (timer_pending(&hwtcon_fb_info()->mmsys_power_timer))
		del_timer(&hwtcon_fb_info()->mmsys_power_timer);

	/* enable fiti power */
	hwtcon_core_fiti_power_enable(true);

	wake_up_interruptible(&hwtcon_fb_info()->mdp_trigger_wait_queue);

	mutex_unlock(&hwtcon_fb_info()->update_queue_mutex);

	return 0;
}



/*
 * 1. remove task from trigger_task_list.
 * 2. add task to wf_lut_task_list.
 * 3. modify task state.
 */
void hwtcon_core_handle_task_written_done(
	struct work_struct *work_item)
{
	struct hwtcon_task *task = container_of(work_item,
			   struct hwtcon_task,
			   work_written_done);

	/* dump working buffer */
	if (hwtcon_debug_get_info()->enable_dump_next_buffer) {
		hwtcon_file_save_buffer(hwtcon_fb_info()->wb_buffer_va,
			hwtcon_fb_info()->wb_buffer_size, "/tmp/next_wb.bin");
		hwtcon_debug_get_info()->enable_dump_next_buffer = false;
	}

	if (hwtcon_debug_get_info()->collision_debug) {
		enum WF_SLOT_ENUM slot = wf_lut_get_waveform_mode_slot(
				task->update_data.waveform_mode,
				hwtcon_core_use_night_mode());

		TCON_ERR("simulate collision scenario, force sleep %d ms",
				hwtcon_debug_get_info()->collision_debug);
		msleep(hwtcon_debug_get_info()->collision_debug);
		/* wait fiti power good */
		fiti_wait_power_good();
		/* trigger WF_LUT */
		TS_WF_LUT_set_lut_info(NULL,
			hwtcon_core_get_task_region(task).x,
			hwtcon_core_get_task_region(task).y,
			hwtcon_core_get_task_region(task).width,
			hwtcon_core_get_task_region(task).height,
			task->assign_lut,
			slot);
		lut_frame_count[task->assign_lut] = 0;
		lut_crc_all_zero[task->assign_lut] = true;
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

struct rect hwtcon_core_rotate_region(
	const struct mxcfb_rect *src_region, u32 rotation)
{
	struct rect region = {0};

	switch (rotation) {
	case HWTCON_ROTATE_0:
		region.x = src_region->left;
		region.y = src_region->top;
		region.width = src_region->width;
		region.height = src_region->height;
		break;
	case HWTCON_ROTATE_270:
		region.x = src_region->top;
		region.y = hw_tcon_get_edp_height() -
			src_region->left - src_region->width;
		region.width = src_region->height;
		region.height = src_region->width;
		break;
	case HWTCON_ROTATE_180:
		region.x = hw_tcon_get_edp_width() -
			src_region->width - src_region->left;
		region.y = hw_tcon_get_edp_height() -
			src_region->height - src_region->top;
		region.width = src_region->width;
		region.height = src_region->height;
		break;
	case HWTCON_ROTATE_90:
		region.x = hw_tcon_get_edp_width() -
				src_region->top -
				src_region->height;
		region.y = src_region->left;
		region.width = src_region->height;
		region.height = src_region->width;
		break;
	default:
		TCON_ERR("invalid rotation:%d", rotation);
		WARN(1, "invalid rotation:%d", rotation);
		break;
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

	region = hwtcon_core_rotate_region(&buffer_region, hwtcon_fb_get_rotation());

	if (region.x < 0 ||
		region.y < 0 ||
		region.width <= 0 ||
		region.height <= 0) {
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
		*buffer_pa = hwtcon_fb_info()->img_buffer_pa;
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
			*buffer_pa = hwtcon_fb_info()->fb_buffer_pa;

		if (buffer_width)
			*buffer_width = hwtcon_fb_get_width();

		if (buffer_height)
			*buffer_height = hwtcon_fb_get_height();

	}
}

int hwtcon_core_wait_all_task_done(void)
{
	int status = 0;

	status = wait_event_timeout(
			hwtcon_fb_info()->task_state_wait_queue,
			(hwtcon_core_check_hwtcon_idle() == true),
			msecs_to_jiffies(HWTCON_TASK_WAIT_MARKER_TIMEOUT_MS));
	/* wait timeout */
	if (status == 0) {
		TCON_ERR("wait all task done timeout count[%d %d %d %d %d] pipeline status:0x%016llx",
			hwtcon_core_get_task_count(&hwtcon_fb_info()->wait_for_mdp_task_list.list),
			hwtcon_core_get_task_count(&hwtcon_fb_info()->mdp_done_task_list.list),
			hwtcon_core_get_task_count(&hwtcon_fb_info()->pipeline_processing_task_list.list),
			hwtcon_core_get_task_count(&hwtcon_fb_info()->pipeline_done_task_list.list),
			hwtcon_core_get_task_count(&hwtcon_fb_info()->collision_task_list.list),
			hwtcon_fb_info()->lut_active);
		return -1;
	}
	return 0;

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

void hwtcon_core_insert_task_to_collision_task_list(struct hwtcon_task *insert_task)
{
	unsigned long flags;
	bool find_a_merge = true;
	struct hwtcon_task *collision_task, *tmp = NULL;

	spin_lock_irqsave(&hwtcon_fb_info()->collision_task_list.lock, flags);
	/* merge */
	while (find_a_merge) {
		find_a_merge = false;
		list_for_each_entry_safe(collision_task, tmp,
			&hwtcon_fb_info()->collision_task_list.list, list) {
			struct rect task_region = hwtcon_core_get_task_region(insert_task);
			struct rect collision_task_region = hwtcon_core_get_task_region(collision_task);
			struct rect merge_region = {0};

			if (hwtcon_core_can_merge_collision_task_region(&task_region, &collision_task_region, &merge_region) &&
				(collision_task->update_data.update_mode == insert_task->update_data.update_mode) &&
				(collision_task->regal_status == insert_task->regal_status) &&
				(collision_task->regal_mode == insert_task->regal_mode) &&
				(collision_task->swipe_info.enable == insert_task->swipe_info.enable) &&
				(collision_task->swipe_info.direction == insert_task->swipe_info.direction) &&
				(collision_task->swipe_info.count == insert_task->swipe_info.count) &&
				(collision_task->lut_dependency == insert_task->lut_dependency)) {
				find_a_merge = true;
				/* merge task & collision_task to task */
				hwtcon_core_set_task_region(insert_task, merge_region);

				insert_task->update_order = MIN(insert_task->update_order,
					collision_task->update_order);
				insert_task->pic_order = MAX(insert_task->pic_order,
					collision_task->pic_order);

				if (insert_task->update_data.waveform_mode != collision_task->update_data.waveform_mode)
					insert_task->update_data.waveform_mode = WAVEFORM_MODE_AUTO;
				insert_task->lut_dependency |= collision_task->lut_dependency;

				/* marker info: move collision_task marker info to insert_task */
				list_splice_init(&collision_task->marker_info_list.list, &insert_task->marker_info_list.list);

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
	TCON_EPDC("insert task: 0x%llx [%d %d %d %d] to collision list lut_dependency:0x%016llx",
		insert_task->unique_id,
		hwtcon_core_get_task_region(insert_task).x,
		hwtcon_core_get_task_region(insert_task).y,
		hwtcon_core_get_task_region(insert_task).width,
		hwtcon_core_get_task_region(insert_task).height,
		insert_task->lut_dependency);
	/* add insert_task to collsion task list*/
	hwtcon_core_change_task_state(insert_task, TASK_STATE_COLLISION, false, false, INSERT_TO_TAIL);

	spin_unlock_irqrestore(&hwtcon_fb_info()->collision_task_list.lock, flags);

	/* check all the collision tasks against the current active LUTs */
	hwtcon_core_update_collision_list_on_release_lut(0LL);

}

void hwtcon_core_insert_normal_task_to_mdp_done_task_list(
	struct hwtcon_task *insert_task)
{
	unsigned long flags;
	struct hwtcon_task *task, *tmp = NULL;

	spin_lock_irqsave(&hwtcon_fb_info()->mdp_done_task_list.lock, flags);

	/* merge */
	list_for_each_entry_safe_reverse(task, tmp,
		&hwtcon_fb_info()->mdp_done_task_list.list, list) {
		struct rect task_region = hwtcon_core_get_task_region(task);
		struct rect insert_task_region = hwtcon_core_get_task_region(insert_task);
		struct rect merge_region = {0};

		/* merge condition:
		 * 1. same update mode
		 * 2. same regal setting (regal status & regal mode)
		 * 3. same swipe setting(enable & direction)
		 * 4. same waveform mode
		 */
		if (hwtcon_core_can_merge_trigger_task_region(&task_region, &insert_task_region, &merge_region) &&
			(task->update_data.update_mode == insert_task->update_data.update_mode) &&
			(task->regal_status == insert_task->regal_status) &&
			(task->update_data.waveform_mode == insert_task->update_data.waveform_mode) &&
			(task->regal_mode == insert_task->regal_mode) &&
			(task->swipe_info.enable == insert_task->swipe_info.enable) &&
			(task->swipe_info.direction == insert_task->swipe_info.direction) &&
			(task->swipe_info.count == insert_task->swipe_info.count)) {

			TCON_LOG("user insert scenario merge task in mdp_done_task list");
			TCON_LOG("merge region [%d %d %d %d] || [%d %d %d %d] = [%d %d %d %d]",
				insert_task_region.x,
				insert_task_region.y,
				insert_task_region.width,
				insert_task_region.height,
				task_region.x,
				task_region.y,
				task_region.width,
				task_region.height,
				merge_region.x,
				merge_region.y,
				merge_region.width,
				merge_region.height);

			TCON_LOG("dump merge marker begin");
			do {
				struct update_marker_struct *update_marker, *tmp_marker;

				list_for_each_entry_safe(update_marker, tmp_marker,
					&task->marker_info_list.list, task_list) {
					TCON_LOG("merge marker %d", update_marker->update_marker);
				}

				list_for_each_entry_safe(update_marker, tmp_marker,
					&insert_task->marker_info_list.list, task_list) {
					TCON_LOG("merge marker %d", update_marker->update_marker);
				}
			} while (0);
			TCON_LOG("dump merge marker end");

			/* merge task & collision_task to task */
			hwtcon_core_set_task_region(insert_task, merge_region);

			insert_task->update_order = MIN(insert_task->update_order,
				task->update_order);
			insert_task->pic_order = MAX(insert_task->pic_order,
				task->pic_order);

			insert_task->lut_dependency = 0LL;

			/* marker info: move task marker info to insert_task */
			list_splice_init(&task->marker_info_list.list, &insert_task->marker_info_list.list);

			list_del_init(&task->list);
			hwtcon_core_put_task_callback(task);
		} else
			break;
	}
	/* insert trigger task to mdp_done_task_list */
	hwtcon_core_change_task_state(insert_task, TASK_STAT_MDP_DONE, false, false, INSERT_TO_TAIL);
	spin_unlock_irqrestore(&hwtcon_fb_info()->mdp_done_task_list.lock, flags);
}


void hwtcon_core_insert_collision_task_to_mdp_done_task_list(
	struct hwtcon_task *insert_task)
{
	unsigned long flags;
	struct hwtcon_task *task, *tmp = NULL;
	bool inserted = false;

	spin_lock_irqsave(&hwtcon_fb_info()->mdp_done_task_list.lock, flags);

	/* merge */
	list_for_each_entry_safe(task, tmp,
		&hwtcon_fb_info()->mdp_done_task_list.list, list) {
		struct rect task_region = hwtcon_core_get_task_region(task);
		struct rect insert_task_region = hwtcon_core_get_task_region(insert_task);
		struct rect merge_region = {0};

		if (hwtcon_core_can_merge_trigger_task_region(&task_region, &insert_task_region, &merge_region) &&
			(task->update_data.update_mode == insert_task->update_data.update_mode) &&
			(task->regal_status == insert_task->regal_status) &&
			(task->regal_mode == insert_task->regal_mode) &&
			(task->swipe_info.enable == insert_task->swipe_info.enable) &&
			(task->swipe_info.direction == insert_task->swipe_info.direction) &&
			(task->swipe_info.count == insert_task->swipe_info.count)) {

			TCON_LOG("collsion scenario merge task in mdp_done_task list");
			TCON_LOG("merge region [%d %d %d %d] || [%d %d %d %d] = [%d %d %d %d]",
				insert_task_region.x,
				insert_task_region.y,
				insert_task_region.width,
				insert_task_region.height,
				task_region.x,
				task_region.y,
				task_region.width,
				task_region.height,
				merge_region.x,
				merge_region.y,
				merge_region.width,
				merge_region.height);

			TCON_LOG("dump merge marker begin");
			do {
				struct update_marker_struct *update_marker, *tmp_marker;

				list_for_each_entry_safe(update_marker, tmp_marker,
					&task->marker_info_list.list, task_list) {
					TCON_LOG("merge marker %d", update_marker->update_marker);
				}

				list_for_each_entry_safe(update_marker, tmp_marker,
					&insert_task->marker_info_list.list, task_list) {
					TCON_LOG("merge marker %d", update_marker->update_marker);
				}
			} while (0);
			TCON_LOG("dump merge marker end");

			/* merge task & collision_task to task */
			hwtcon_core_set_task_region(insert_task, merge_region);

			insert_task->update_order = MIN(insert_task->update_order,
				task->update_order);
			insert_task->pic_order = MAX(insert_task->pic_order,
				task->pic_order);

			if (insert_task->update_data.waveform_mode != task->update_data.waveform_mode)
				insert_task->update_data.waveform_mode = WAVEFORM_MODE_AUTO;
			insert_task->lut_dependency = 0LL;

			/* marker info: move task marker info to insert_task */
			list_splice_init(&task->marker_info_list.list, &insert_task->marker_info_list.list);

			list_del_init(&task->list);
			hwtcon_core_put_task_callback(task);
		} else if (insert_task->update_order > task->update_order) {
			/* can't merge &
			 * insert_task update_order > current task update order
			 * need to insert insert_task behind current task
			 */
			continue;
		} else {
			/* insert insert_task before current task */
			list_add_tail(&insert_task->list, &task->list);
			inserted = true;
			break;
		}
	}

	if (inserted == false) {
		/* mdp_done_task_list is empty.
		 * or all task in mdp_done_task_list's update_order is smaller than insert_task.
		 * add insert_task to the tail of mdp_done_task_list.
		 */
		hwtcon_core_change_task_state(insert_task, TASK_STAT_MDP_DONE, false, false, INSERT_TO_TAIL);
	}
	spin_unlock_irqrestore(&hwtcon_fb_info()->mdp_done_task_list.lock, flags);
}

void hwtcon_core_create_collision_task(struct hwtcon_task *task)
{
	struct hwtcon_task *collision_task = NULL;

	/* no collision */
	if (task->pipeline_info.collision_lut_0 == 0 &&
		task->pipeline_info.collision_lut_1 == 0)
		return;
	/* have collision
	 * create a new collision task
	 * 1. force new task waveform mode to partial
	 * 2. new task region is collision region
	 * 3. copy new task marker from task, delete current task marker info
	 * 4. new task->lut = -1
	 * 5. new task->lut_dependency
	 */
	collision_task = hwtcon_core_get_task(&task->update_data, false);
	if (collision_task == NULL) {
		TCON_ERR("create new task fail");
		dump_stack();
		return;
	}

	collision_task->update_order = task->update_order;
	collision_task->pic_order = task->pic_order;

	collision_task->update_data.update_mode = UPDATE_MODE_PARTIAL;
	collision_task->update_data.flags = 0;

	hwtcon_core_set_task_region(collision_task, task->pipeline_info.collision_region);

	collision_task->update_data.waveform_mode = WAVEFORM_MODE_AUTO;

	/* move task's marker to collision task */
	list_splice_init(&task->marker_info_list.list, &collision_task->marker_info_list.list);

	collision_task->assign_lut = -1;
	collision_task->lut_dependency = (u64)task->pipeline_info.collision_lut_1 << 32 |
		(u64)task->pipeline_info.collision_lut_0;

	TCON_LOG("create collision task[new] region[%d %d %d %d] lut_dependency:0x%016llx wf_mode:%s",
		collision_task->update_data.update_region.left,
		collision_task->update_data.update_region.top,
		collision_task->update_data.update_region.width,
		collision_task->update_data.update_region.height,
		collision_task->lut_dependency,
		hwtcon_core_get_wf_mode_name(collision_task->update_data.waveform_mode));
	hwtcon_core_insert_task_to_collision_task_list(collision_task);
	return;
}


int hwtcon_core_wait_all_wf_lut_release(void)
{
	int status = 0;

	/* wait all wf_lut release */
	status = wait_event_timeout(
			hwtcon_fb_info()->wf_lut_release_wait_queue,
			(hwtcon_fb_info()->lut_active == 0LL),
			msecs_to_jiffies(HWTCON_WAIT_WF_LUT_RELEASE_TIMEOUT));
	/* wait timeout */
	if (status == 0) {
		TCON_ERR("wait timeout, lut status: 0x%016llx 0x%016llx 0x%016llx",
			hwtcon_fb_info()->lut_free,
			hwtcon_core_get_released_lut(),
			hwtcon_fb_info()->lut_active);
		return -1;
	}
	return 0;
}

int hwtcon_core_wait_power_down(void)
{
	int status = 0;
	int timeout_ms = HWTCON_TASK_WAIT_MARKER_TIMEOUT_MS +
		hwtcon_fb_info()->power_down_delay_ms;

	status = wait_event_timeout(
			hwtcon_fb_info()->power_state_change_wait_queue,
			(hwtcon_fb_info()->mmsys_power_enable == false),
			msecs_to_jiffies(timeout_ms));
	if (status == 0) {
		TCON_ERR("wait power down timeout:%d timer:%d",
			hwtcon_fb_info()->mmsys_power_enable,
			timeout_ms);
		hwtcon_driver_enable_mmsys_power(false);
		return -1;
	}
	return 0;
}

bool hwtcon_core_use_night_mode(void)
{
	return (hwtcon_fb_get_grayscale() == GRAYSCALE_8BIT_INVERTED);
}

bool hwtcon_core_use_regal(struct mxcfb_update_data *update_data,
	enum REGAL_MODE_ENUM *regal_mode)
{
	switch (update_data->waveform_mode) {
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

void hwtcon_core_change_waveform_slot(struct hwtcon_task *task)
{
	int temp_zone = 0;
	int night_mode = 0;

	temp_zone = hwtcon_core_read_temp_zone();
	night_mode = hwtcon_core_use_night_mode();

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
			fiti_wait_power_good();
			fiti_set_night_mode(night_mode);
		}

		hwtcon_fb_info()->current_temp_zone = temp_zone;
		hwtcon_fb_info()->current_night_mode = night_mode;
	}
}

void hwtcon_core_pre_handle_info(struct hwtcon_task *task, struct pipeline_info *info)
{
	int i = 0;
	unsigned long flags;

	spin_lock_irqsave(&hwtcon_fb_info()->lut_pic_order_lock, flags);
	/* info->collision_lut_0 record lut 0 - lut 31  */
	for(i = 0; i < 32; i++) {
		if ((info->collision_lut_0 & BIT_MASK(i)) &&
				(task->update_order < hwtcon_fb_info()->lut_pic_order[i]))
			info->collision_lut_0 = info->collision_lut_0 & (~ BIT_MASK(i));
	}
	/* info->collision_lut_1 record lut 32 - lut 62  */
	for(i = 0; i < 31; i++) {
		if ((info->collision_lut_1 & BIT_MASK(i)) &&
				(task->update_order < hwtcon_fb_info()->lut_pic_order[i + 32]))
			info->collision_lut_1 = info->collision_lut_1 & (~ BIT_MASK(i));
	}
	spin_unlock_irqrestore(&hwtcon_fb_info()->lut_pic_order_lock, flags);
}

int hwtcon_core_trigger_pipeline(struct hwtcon_task *task)
{
	struct pipeline_info info = {0};
	int status = 0;
	unsigned long flags;
	struct update_marker_struct *update_marker, *tmp;

	hwtcon_driver_enable_mmsys_power(true);

	/* update waveform slot */
	hwtcon_core_change_waveform_slot(task);

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
		task->update_data.flags,hwtcon_fb_get_rotation());

	TCON_LOG("TRIGGER:task:0x%llx marker:%d time:%lld region[%d %d %d %d]",
		task->unique_id,
		task->update_data.update_marker,
		task->time_trigger_pipeline,
		hwtcon_core_get_task_region(task).x,
		hwtcon_core_get_task_region(task).y,
		hwtcon_core_get_task_region(task).width,
		hwtcon_core_get_task_region(task).height);
	/* trigger lut to pipeline. */
	TCON_LOG("%s update:%d->%s wf_mode:%d-%s temperature:%d-%d",
		hwtcon_core_use_night_mode() ? "Night Mode" : "Day Mode",
		task->update_data.update_mode,
		(task->update_data.update_mode == UPDATE_MODE_FULL) ?
			"FULL" : "PARTIAL",
		task->update_data.waveform_mode,
		hwtcon_core_get_wf_mode_name(task->update_data.waveform_mode),
		hwtcon_core_read_temperature(),
		hwtcon_core_read_temp_zone());

	if (task->swipe_info.enable == false)
		status = pipeline_handle_normal_update(task, &info);
	else
		status = pipeline_handle_swipe_update(task, &info);

	task->time_pipeline_done = timeofday_ms();

	if (status != 0) {
		TCON_ERR("trigger pipeline fail[%d]", status);
		spin_lock_irqsave(&hwtcon_fb_info()->pipeline_processing_task_list.lock, flags);
		list_del_init(&task->list);
		spin_unlock_irqrestore(&hwtcon_fb_info()->pipeline_processing_task_list.lock, flags);

		#ifdef MARKER_V2_ENABLE
		list_for_each_entry_safe(update_marker, tmp,
			&task->marker_info_list.list, task_list) {
			complete(&update_marker->submit_completion);
			TCON_LOG("[MARKER] submit_completion: %d", update_marker->update_marker);
			spin_lock_irqsave(&update_marker->marker_lock, flags);
			list_del_init(&update_marker->task_list);
			update_marker->lut_mask |= 0LL;
			update_marker->marker_state = MARKER_STATE_LUT_ASSIGNED;
			spin_unlock_irqrestore(&update_marker->marker_lock, flags);
		}
		#endif

		/* close mmsys & fiti power */
		hwtcon_core_handle_clock_disable();
		hwtcon_core_put_task_callback(task);
		return status;
	}

	hwtcon_core_pre_handle_info(task, &info);
	memcpy(&task->pipeline_info, &info, sizeof(info));

	if (info.update_void) {
		/* update_void == true: pipeline doesn't modify working buffer */
		if (info.collision_lut_0 == 0 &&
			info.collision_lut_1 == 0) {
			/* no collision
			 * move task to free task list
			 */
			HWTCON_TIME end_time = timeofday_ms();

			list_for_each_entry_safe(update_marker, tmp,
				&task->marker_info_list.list, task_list) {
				TCON_EPDC("[%d] Sending update. VOID update region top=%d, left=%d, width=%d, height=%d temp index: %d rotation=%d",
					update_marker->update_marker,
					hwtcon_core_get_task_user_region(task).y,
					hwtcon_core_get_task_user_region(task).x,
					hwtcon_core_get_task_user_region(task).width,
					hwtcon_core_get_task_user_region(task).height,
					hwtcon_core_read_temp_zone(),hwtcon_fb_get_rotation());
				TCON_LOG("task:0x%llx marker:%d update void with no collision",
					task->unique_id,
					update_marker->update_marker);
				TCON_EPDC("[%d] update end marker=%d, end time=%lld, time taken=%d ms",
					update_marker->update_marker,
					update_marker->update_marker,
					end_time,
					hwtcon_hal_get_time_in_ms(
						update_marker->start_time,
						end_time));

				//store_completed_markers(&hwtcon_fb_info()->marker_store ,update_marker->update_marker); /* store completed marker*/
			}

			//complete_markers_avail(&hwtcon_fb_info()->marker_store); /* Signal completed marker available */

			spin_lock_irqsave(&hwtcon_fb_info()->pipeline_processing_task_list.lock, flags);
			list_del_init(&task->list);
			spin_unlock_irqrestore(&hwtcon_fb_info()->pipeline_processing_task_list.lock, flags);

			#ifdef MARKER_V2_ENABLE
			list_for_each_entry_safe(update_marker, tmp,
				&task->marker_info_list.list, task_list) {
				complete(&update_marker->submit_completion);
				TCON_LOG("[MARKER] submit_completion: %d", update_marker->update_marker);
				spin_lock_irqsave(&update_marker->marker_lock, flags);
				list_del_init(&update_marker->task_list);
				update_marker->lut_mask |= 0LL;
				update_marker->marker_state = MARKER_STATE_LUT_ASSIGNED;
				spin_unlock_irqrestore(&update_marker->marker_lock, flags);
			}
			#endif

			/* close mmsys & fiti power */
			hwtcon_core_handle_clock_disable();
			hwtcon_core_put_task_callback(task);
			return 0;
		} else {
			/* have collision
			 * move task to collision task list
			 * 1. force task waveform mode to partial
			 * 2. task region is collision region
			 * 3. task->lut = -1
			 * 4. task->lut_dependency
			 * 5. move to collision task list
			 */
			task->update_data.update_mode = UPDATE_MODE_PARTIAL;
			task->update_data.waveform_mode = WAVEFORM_MODE_AUTO;
			task->update_data.flags = 0;
			hwtcon_core_set_task_region(task, info.collision_region);
			task->assign_lut = -1;
			task->lut_dependency = (u64)info.collision_lut_1 << 32 | (u64)info.collision_lut_0;

			#ifdef MARKER_V2_ENABLE
			list_for_each_entry_safe(update_marker, tmp,
				&task->marker_info_list.list, task_list) {
				spin_lock_irqsave(&update_marker->marker_lock, flags);
				update_marker->lut_mask |= 0LL;
				update_marker->marker_state = MARKER_STATE_COLLISION;
				spin_unlock_irqrestore(&update_marker->marker_lock, flags);
			}
			#endif

			TCON_LOG("create collision task[replace] region[%d %d %d %d] lut_dependency:0x%016llx wf_mode:%s",
				task->update_data.update_region.left,
				task->update_data.update_region.top,
				task->update_data.update_region.width,
				task->update_data.update_region.height,
				task->lut_dependency,
				hwtcon_core_get_wf_mode_name(task->update_data.waveform_mode));

			spin_lock_irqsave(&hwtcon_fb_info()->pipeline_processing_task_list.lock, flags);
			list_del_init(&task->list);
			spin_unlock_irqrestore(&hwtcon_fb_info()->pipeline_processing_task_list.lock, flags);
			hwtcon_core_insert_task_to_collision_task_list(task);
			return 0;
		}
	} else {
		/* update_void == false pipeline modify working buffer */
		WARN_ON(task->assign_lut > MAX_LUT_REGION_COUNT);
		if (info.collision_lut_0 == 0 &&
			info.collision_lut_1 == 0) {
			/* no collision
			 * signal update_marker
			 */
			#ifdef MARKER_V2_ENABLE
			list_for_each_entry_safe(update_marker, tmp,
				&task->marker_info_list.list, task_list) {
				complete(&update_marker->submit_completion);
				TCON_LOG("[MARKER] submit_completion: %d", update_marker->update_marker);
				spin_lock_irqsave(&update_marker->marker_lock, flags);
				list_del_init(&update_marker->task_list);
				update_marker->lut_mask |= (1LL << task->assign_lut);
				update_marker->marker_state = MARKER_STATE_LUT_ASSIGNED;
				spin_unlock_irqrestore(&update_marker->marker_lock, flags);
			}
			#else
			list_for_each_entry_safe(update_marker, tmp,
				&task->marker_info_list.list, task_list) {
				complete(&update_marker->submit_completion);
				TCON_LOG("[MARKER] submit_completion: %d", update_marker->update_marker);
			}
			#endif
		} else {
			/* have collision
			 * create a new collision task
			 */
			#ifdef MARKER_V2_ENABLE
			list_for_each_entry_safe(update_marker, tmp,
				&task->marker_info_list.list, task_list) {
				spin_lock_irqsave(&update_marker->marker_lock, flags);
				update_marker->lut_mask |= (1LL << task->assign_lut);
				update_marker->marker_state = MARKER_STATE_COLLISION;
				spin_unlock_irqrestore(&update_marker->marker_lock, flags);
			}
			#endif
			hwtcon_core_create_collision_task(task);
		}

		/* pipeline update working buffer
		 * move task to pipeline_done_task_list
		 * trigger WF_LUT to show update.
		 */
		hwtcon_core_change_task_state(task, TASK_STATE_PIPELINE_DONE, true, true, INSERT_TO_TAIL);

		/* auto waveform handle */
		hwtcon_core_handle_auto_waveform(task);

		list_for_each_entry_safe(update_marker, tmp,
				&task->marker_info_list.list, task_list) {
			TCON_EPDC("[%d] Sending update. waveform:%d (%s) mode:0x%d update region top=%d, left=%d, width=%d, height=%d temp index: %d rotation=%d",
				update_marker->update_marker,
				task->update_data.waveform_mode,
				hwtcon_core_get_wf_mode_name(task->update_data.waveform_mode),
				task->update_data.update_mode,
				hwtcon_core_get_task_user_region(task).y,
				hwtcon_core_get_task_user_region(task).x,
				hwtcon_core_get_task_user_region(task).width,
				hwtcon_core_get_task_user_region(task).height,
				hwtcon_core_read_temp_zone(),hwtcon_fb_get_rotation());

			TCON_EPDC("[%d] Sending update in LUT: %d",
					update_marker->update_marker,
					task->assign_lut);
		}

		TCON_LOG("trigger WF_LUT with lut:%d region[%d %d %d %d] wf_mode:%d lut status[0x%016llx 0x%016llx 0x%016llx]",
			task->assign_lut,
			hwtcon_core_get_task_region(task).x,
			hwtcon_core_get_task_region(task).y,
			hwtcon_core_get_task_region(task).width,
			hwtcon_core_get_task_region(task).height,
			task->update_data.waveform_mode,
			hwtcon_fb_info()->lut_free,
			hwtcon_core_get_released_lut(),
			hwtcon_fb_info()->lut_active);

		if (hwtcon_debug_get_info()->enable_dump_next_buffer ||
			hwtcon_debug_get_info()->collision_debug) {
			/* hwtcon_core_handle_task_written_done */
			queue_work(hwtcon_fb_info()->wq_pipeline_written_done,
				&task->work_written_done);
			if (hwtcon_debug_get_info()->enable_dump_next_buffer)
				msleep(500);
		}

		if (!hwtcon_debug_get_info()->collision_debug) {
			enum WF_SLOT_ENUM slot = wf_lut_get_waveform_mode_slot(
				task->update_data.waveform_mode,
				hwtcon_core_use_night_mode());

			task->time_wait_fiti_power_good = timeofday_ms();
			/* wait fiti power good */
			fiti_wait_power_good();
			task->time_trigger_wf_lut = timeofday_ms();
			/* trigger WF_LUT */
			TS_WF_LUT_set_lut_info(NULL,
					hwtcon_core_get_task_region(task).x,
					hwtcon_core_get_task_region(task).y,
					hwtcon_core_get_task_region(task).width,
					hwtcon_core_get_task_region(task).height,
					task->assign_lut,
					slot);
			lut_frame_count[task->assign_lut] = 0;
			lut_crc_all_zero[task->assign_lut] = true;
		}

	}

	return 0;
}


bool hwtcon_core_check_task_block(struct hwtcon_task *task,
		struct hwtcon_task *search_task)
{
#if 0
	if ((task->update_data.flags & EPDC_FLAG_ENABLE_INVERSION) !=
		(search_task->update_data.flags & EPDC_FLAG_ENABLE_INVERSION)) {
#else
	if (task->update_data.flags != search_task->update_data.flags) {
#endif
		struct rect task_region = hwtcon_core_get_task_region(task);
		struct rect sreach_task_region = hwtcon_core_get_task_region(search_task);

		if (hwtcon_rect_check_relationship(&task_region, &sreach_task_region, NULL) ==
			RECT_RELATION_CONTAIN)
			return true;
	}
	return false;
}

bool hwtcon_core_pre_check_mdp_trigger(struct hwtcon_task *task)
{
	/* make sure current task won't block by the following tasks
	 * all the tasks in mdp_done_task_list & pipeline_processing_task_list
	 */
	struct hwtcon_task *search_task = NULL;
	struct hwtcon_task *tmp;
	unsigned long flags;

	/* search the mdp_done_task_list */
	spin_lock_irqsave(&hwtcon_fb_info()->mdp_done_task_list.lock, flags);
	list_for_each_entry_safe(search_task, tmp,
		&hwtcon_fb_info()->mdp_done_task_list.list, list) {
		if (hwtcon_core_check_task_block(task, search_task)) {
			spin_unlock_irqrestore(&hwtcon_fb_info()->mdp_done_task_list.lock, flags);
			return false;
		}
	}
	spin_unlock_irqrestore(&hwtcon_fb_info()->mdp_done_task_list.lock, flags);


	/* search the pipeline_processing_task_list */
	spin_lock_irqsave(&hwtcon_fb_info()->pipeline_processing_task_list.lock, flags);
	list_for_each_entry_safe(search_task, tmp,
		&hwtcon_fb_info()->pipeline_processing_task_list.list, list) {
		if (hwtcon_core_check_task_block(task, search_task)) {
			spin_unlock_irqrestore(&hwtcon_fb_info()->pipeline_processing_task_list.lock, flags);
			return false;
		}
	}
	spin_unlock_irqrestore(&hwtcon_fb_info()->pipeline_processing_task_list.lock, flags);

	/* task is not block by any tasks
	 * can trigger mdp handle this task directly.
	 */
	return true;
}

void hwtcon_core_wait_for_mdp_trigger(struct hwtcon_task *task)
{
	DEFINE_WAIT_FUNC(wait, woken_wake_function);

	add_wait_queue(&hwtcon_fb_info()->task_state_wait_queue, &wait);

	while (!hwtcon_core_pre_check_mdp_trigger(task))
		wait_woken(&wait,
			TASK_INTERRUPTIBLE,
			MAX_SCHEDULE_TIMEOUT);

	remove_wait_queue(
		&hwtcon_fb_info()->task_state_wait_queue,
		&wait);
}


int hwtcon_core_dispatch_mdp(void *ignore)
{
	struct hwtcon_task *task = NULL;
	unsigned long flags;

	while (1) {
		wait_event_interruptible(hwtcon_fb_info()->mdp_trigger_wait_queue,
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

		TCON_LOG("wait for trigger Task [MARKER]:%d flags:0x%08x to MDP",
			task->update_data.update_marker,
			task->update_data.flags);

		hwtcon_core_wait_for_mdp_trigger(task);

		TCON_LOG("ready to trigger Task [MARKER]:%d flags:0x%08x to MDP",
			task->update_data.update_marker,
			task->update_data.flags);

		task->time_trigger_mdp = timeofday_ms();

		if (is_SW_mitigation_needed()) {
			hwtcon_pixel_pre_process(&task->update_data);
		}

		if (hwtcon_debug_get_info()->enable_dump_next_buffer)
			hwtcon_file_save_buffer(hwtcon_fb_info()->fb_buffer_va,
				hwtcon_fb_info()->fb_buffer_size,
				"/tmp/next_fb.bin");
		/* call MDP */
		mutex_lock(&hwtcon_fb_info()->image_buffer_access_mutex);

		#if 1
		hwtcon_mdp_convert(task);
		#else
		do {
			struct rect src_region = {0, 0, hw_tcon_get_edp_width(), hw_tcon_get_edp_height()};
			struct rect dst_region = {0, 0, hw_tcon_get_edp_width(), hw_tcon_get_edp_height()};

			hwtcon_mdp_copy_buffer_with_region(
				hwtcon_fb_info()->img_buffer_va,
				hw_tcon_get_edp_width(),
				&dst_region,
				hwtcon_fb_info()->fb_buffer_va,
				hw_tcon_get_edp_width(),
				&src_region);
		} while (0);
		#endif

		{
			HWTCON_TIME apply_lightbox_start = timeofday_ms();
			HWTCON_TIME apply_lightbox_end = 0;

			hwtcon_lightbox_apply_lightbox(task);
			apply_lightbox_end = timeofday_ms();
			TCON_LOG("Apply lightbox takes:%d ms", hwtcon_hal_get_time_in_ms(apply_lightbox_start, apply_lightbox_end));
		}

		if (hwtcon_debug_get_info()->enable_dump_next_buffer)
			hwtcon_file_save_buffer(hwtcon_fb_info()->img_buffer_va,
				hwtcon_fb_info()->img_buffer_size,
				"/tmp/next_img.bin");

		if (hwtcon_debug_get_info()->enable_dump_image_buffer) {
			int index = hwtcon_fb_info()->debug_img_buffer_counter++;
			HWTCON_TIME start = timeofday_ms();
			HWTCON_TIME end = 0;

			hwtcon_fb_info()->debug_img_buffer_counter %= MAX_DEBUG_IMAGE_BUFFER_COUNT;

			snprintf(hwtcon_fb_info()->debug_img_buffer_name[index], MAX_FILE_NAME_LEN,
				"/mnt/us/documents/dump/img_%04d_%03d_%03d_%03d_%03d.bin",
				task->update_data.update_marker,
				hwtcon_core_get_task_region(task).x,
				hwtcon_core_get_task_region(task).y,
				hwtcon_core_get_task_region(task).width,
				hwtcon_core_get_task_region(task).height);
			#if 0
			memcpy(hwtcon_fb_info()->debug_img_buffer_va[index],
				hwtcon_fb_info()->img_buffer_va,
				hwtcon_fb_info()->img_buffer_size);
			#else
			hwtcon_mdp_memcpy(hwtcon_fb_info()->debug_img_buffer_pa[index],
				hwtcon_fb_info()->img_buffer_pa);
			#endif

			end = timeofday_ms();
			TCON_ERR("dump buffer time:%d ms", hwtcon_hal_get_time_in_ms(start, end));
		}

		mutex_unlock(&hwtcon_fb_info()->image_buffer_access_mutex);

		task->time_mdp_done = timeofday_ms();
		spin_lock_irqsave(&hwtcon_fb_info()->wait_for_mdp_task_list.lock, flags);
		list_del_init(&task->list);
		spin_unlock_irqrestore(&hwtcon_fb_info()->wait_for_mdp_task_list.lock,
			flags);
		hwtcon_core_insert_normal_task_to_mdp_done_task_list(task);
		if (hwtcon_debug_get_info()->mdp_merge_debug) {
			/* start a timer for delay wakeup dispatch_pipeline thread
			 * hwtcon_core_handle_mdp_debug_timer_cb
			 */
			TCON_ERR("delay %d ms for simulate task merge in mdp_task_done list",
				hwtcon_debug_get_info()->mdp_merge_debug);
			mod_timer(&task->mdp_debug_timer,
				jiffies + msecs_to_jiffies(
					hwtcon_debug_get_info()->mdp_merge_debug));
		} else
			wake_up(&hwtcon_fb_info()->pipeline_trigger_wait_queue);

	}

	return 0;
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

		if (task == NULL) {
			spin_unlock_irqrestore(&hwtcon_fb_info()->mdp_done_task_list.lock, flags);
			wait_woken(&wait,
				TASK_INTERRUPTIBLE,
				MAX_SCHEDULE_TIMEOUT);
			continue;
		}

		hwtcon_core_change_task_state(task, TASK_STATE_PIPELINE_PROCESS, false, true, INSERT_TO_TAIL);
		spin_unlock_irqrestore(&hwtcon_fb_info()->mdp_done_task_list.lock, flags);

		/* trigger task to pipeline. */
		mutex_lock(&hwtcon_fb_info()->image_buffer_access_mutex);
		hwtcon_core_trigger_pipeline(task);
		mutex_unlock(&hwtcon_fb_info()->image_buffer_access_mutex);

	}

	remove_wait_queue(
		&hwtcon_fb_info()->pipeline_trigger_wait_queue,
		&wait);
	return 0;
}

void hwtcon_core_config_timing(struct cmdqRecStruct *pkt)
{
	/* config smi setting */
	rdma_config_smi_setting(NULL);

	wf_lut_config_context_init_for_pipeline();

	/* confit wf_lut */
	//wf_lut_dpi_enable(pkt);

	hwtcon_edp_pinmux_active();
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

void hwtcon_core_handle_release_lut(int lut_id)
{
	struct hwtcon_task *task = NULL;
	struct hwtcon_task *tmp;
	#ifndef MARKER_V2_ENABLE
	struct update_marker_struct *update_marker = NULL;
	struct update_marker_struct *tmp_marker;
	#endif

	unsigned long flags;
	bool release_task_found = false;

	if (lut_id < 0 || lut_id >= MAX_LUT_REGION_COUNT) {
		WARN(1, "invalid lut_id:%d", lut_id);
		return;
	}
	spin_lock_irqsave(&hwtcon_fb_info()->lut_pic_order_lock, flags);
	hwtcon_fb_info()->lut_pic_order[lut_id] = MIN_PIC_ORDER;
	spin_unlock_irqrestore(&hwtcon_fb_info()->lut_pic_order_lock, flags);

	/* cancel task lut release timer */
	del_timer(&hwtcon_fb_info()->timer_lut_release[lut_id]);

	/* search the release task */
	spin_lock_irqsave(&hwtcon_fb_info()->pipeline_done_task_list.lock, flags);
	list_for_each_entry_safe(task, tmp,
		&hwtcon_fb_info()->pipeline_done_task_list.list, list) {
		if (task->assign_lut == lut_id) {
			release_task_found = true;
			break;
		}
	}
	spin_unlock_irqrestore(&hwtcon_fb_info()->pipeline_done_task_list.lock, flags);

	/* For swipe case, no task will be found when LUT release,
	 * This is because swipe task will trigger many LUTs work at same time
	 * for swipe visual effect
	 */
	if (release_task_found == false)
		return;

	if (lut_crc_all_zero[lut_id])
		TCON_ERR("debug drop frame: lut[%d] crc all zero frame_count[%d] waveform[%d->%s]",
			lut_id, lut_frame_count[lut_id],
			task->update_data.waveform_mode,
			hwtcon_core_get_wf_mode_name(task->update_data.waveform_mode));
	lut_crc_all_zero[lut_id] = true;
	lut_frame_count[lut_id] = 0;

	hwtcon_core_dump_task_info(task);

	/* release task */
	task->time_wf_lut_done = timeofday_ms();

	#ifndef MARKER_V2_ENABLE
	list_for_each_entry_safe(update_marker, tmp_marker,
		&task->marker_info_list.list, task_list) {
		TCON_EPDC("[%d] update end marker=%d, end time=%lld, time taken=%d ms",
					update_marker->update_marker,
					update_marker->update_marker,
					task->time_wf_lut_done,
					hwtcon_hal_get_time_in_ms(
						task->time_trigger_pipeline,
						task->time_wf_lut_done));
	}
	#endif

	TCON_LOG("DONE:task:0x%llx marker[%d] time:%lld cost:%d ms",
		task->unique_id,
		task->update_data.update_marker,
		task->time_wf_lut_done,
		hwtcon_hal_get_time_in_ms(
			task->time_trigger_pipeline,
			task->time_wf_lut_done));
	spin_lock_irqsave(&hwtcon_fb_info()->pipeline_done_task_list.lock, flags);
	list_del_init(&task->list);
	spin_unlock_irqrestore(&hwtcon_fb_info()->pipeline_done_task_list.lock, flags);
	hwtcon_core_put_task_with_lut_release(task);
}

void hwtcon_core_update_collision_list_on_release_lut(u64 released_lut)
{
	unsigned long flags;
	struct hwtcon_task *task, *tmp;
	u64 active_lut;
	static bool warn_flag = false;

	spin_lock_irqsave(&hwtcon_fb_info()->collision_task_list.lock, flags);
	active_lut = hwtcon_fb_info()->lut_active;
	list_for_each_entry_safe(task, tmp,
		&hwtcon_fb_info()->collision_task_list.list, list) {
		if (released_lut != 0LL){
			task->lut_dependency &= ~released_lut;
		}
		if (task->lut_dependency == 0LL || (task->lut_dependency & active_lut) == 0LL) {
			TCON_EPDC("retrigger collision task:0x%llx [%d %d %d %d] wf:%s to mdp_done_list",
				task->unique_id,
				hwtcon_core_get_task_region(task).x,
				hwtcon_core_get_task_region(task).y,
				hwtcon_core_get_task_region(task).width,
				hwtcon_core_get_task_region(task).height,
				hwtcon_core_get_wf_mode_name(task->update_data.waveform_mode));
			list_del_init(&task->list);
			if (task->lut_dependency != 0LL && !warn_flag) {
				warn_flag = true;
				TCON_WARN("collision task:0x%llx is inserted after its LUT dependency has been released, lut_dependency[0x%016llx], active_lut[0x%016llx]",
						task->unique_id,
						task->lut_dependency,
						active_lut);
			}
			hwtcon_core_insert_collision_task_to_mdp_done_task_list(task);
		}
	}

	spin_unlock_irqrestore(&hwtcon_fb_info()->collision_task_list.lock,
		flags);
	wake_up(&hwtcon_fb_info()->pipeline_trigger_wait_queue);
}

int hwtcon_core_convert_bit_count_2_grey_level(u32 histogram)
{
	if ((histogram & ~HISTOGRAM_GREY_LEVEL_Y2) == 0)
		return 0;
	if ((histogram & ~HISTOGRAM_GREY_LEVEL_Y4) == 0)
		return 1;
	if ((histogram & ~HISTOGRAM_GREY_LEVEL_Y8) == 0)
		return 2;
	if ((histogram & ~HISTOGRAM_GREY_LEVEL_Y16) == 0)
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
	case WAVEFORM_MODE_DUNM:
		return "dunm";
	case WAVEFORM_MODE_P2SW:
		return "p2sw";
	case WAVEFORM_MODE_AUTO:
		return "auto";
	default:
		return "unknown_mode";
	}

	return "unknown_mode";
}

struct update_marker_struct *hwtcon_core_alloc_update_marker(void)
{
	struct update_marker_struct *marker = NULL;

	marker = vzalloc(sizeof(struct update_marker_struct));
	if (marker == NULL) {
		TCON_ERR("vmalloc update marker fail");
		return NULL;
	}

	/* init marker member	*/
	INIT_LIST_HEAD(&marker->global_list);
	INIT_LIST_HEAD(&marker->task_list);
	marker->update_marker = -1;
	init_completion(&marker->update_completion);
	init_completion(&marker->submit_completion);
	spin_lock_init(&marker->marker_lock);
	marker->lut_mask = 0LL;
	marker->marker_state = MARKER_STATE_LUT_NOT_ASSIGN;
	marker->start_time = timeofday_ms();
	marker->waiters = 0;
	marker->need_release = false;
	return marker;
}

void hwtcon_core_handle_auto_waveform(struct hwtcon_task *task)
{
	u32 next_grey = 0;
	u32 current_grey = 0;
	enum WAVEFORM_MODE_ENUM wf_mode;
	static const enum WAVEFORM_MODE_ENUM day_mode_wf_mode_table[5][5] = {
		{1, 2, 2, 2, 2},
		{1, 2, 2, 2, 2},
		{1, 2, 2, 2, 2},
		{1, 2, 2, 2, 2},
		{3, 3, 3, 3, 3},
	};
	static const enum WAVEFORM_MODE_ENUM night_mode_wf_mode_table[5][5] = {
		{1, 8, 8, 8, 8},
		{1, 8, 8, 8, 8},
		{1, 8, 8, 8, 8},
		{1, 8, 8, 8, 8},
		{9, 9, 9, 9, 9},
	};


	if (task->update_data.waveform_mode != WAVEFORM_MODE_AUTO)
		return;

	next_grey = hwtcon_core_convert_bit_count_2_grey_level(task->pipeline_info.next_histogram);
	current_grey = hwtcon_core_convert_bit_count_2_grey_level(task->pipeline_info.current_histogram);
	if (hwtcon_core_use_night_mode())
		wf_mode = night_mode_wf_mode_table[current_grey][next_grey];
	else
		wf_mode = day_mode_wf_mode_table[current_grey][next_grey];

	if (task->update_data.update_mode == UPDATE_MODE_PARTIAL) {
		if (wf_mode == WAVEFORM_MODE_GC16)
			wf_mode = WAVEFORM_MODE_GC16_PARTIAL;
		if (wf_mode == WAVEFORM_MODE_GCK16)
			wf_mode = WAVEFORM_MODE_GCK16_PARTIAL;
	}

	task->update_data.waveform_mode = wf_mode;

	TCON_EPDC("[%d] current_hist_stat = 0x%x[%d] next_hist_stat = 0x%x[%d] new waveform = 0x%x (%s)",
		task->update_data.update_marker,
		task->pipeline_info.current_histogram,
		current_grey,
		task->pipeline_info.next_histogram,
		next_grey,
		wf_mode,
		hwtcon_core_get_wf_mode_name(wf_mode));
	auto_waveform_replacement(&task->update_data);

}

u64 hwtcon_core_get_released_lut(void)
{
	u64 released = 0LL;

	released = ~(hwtcon_fb_info()->lut_free | hwtcon_fb_info()->lut_active) &
		LUT_BIT_ALL_SET;
	return released;
}

/* Note: this function only can call in dispatch pipeline thread. */
enum GET_LUT_STATUS_ENUM hwtcon_core_get_free_lut(	bool *need_do_clear,
	int *acquired_id)
{
	unsigned long flags;
	u64 free_lut = 0LL;
	u64 active_lut = 0LL;

	spin_lock_irqsave(&hwtcon_fb_info()->lut_free_lock, flags);
	free_lut = hwtcon_fb_info()->lut_free;
	spin_unlock_irqrestore(&hwtcon_fb_info()->lut_free_lock, flags);

	spin_lock_irqsave(&hwtcon_fb_info()->lut_active_lock, flags);
	active_lut = hwtcon_fb_info()->lut_active;
	spin_unlock_irqrestore(&hwtcon_fb_info()->lut_active_lock, flags);

	if (free_lut != 0LL) {
		/* free lut available */
		if (need_do_clear) {
			if (hwtcon_hal_bit_set_cnt(hwtcon_core_get_released_lut()) >
				MAX_RELEASED_LUT_COUNT)
				*need_do_clear = true;
			else
				*need_do_clear = false;
		}

		*acquired_id = hwtcon_hal_ffs(free_lut);
		if (*acquired_id < 0) {
			/* should not go here */
			TCON_ERR("calc ffs error: free_lut:0x%016llx id:%d",
				free_lut,
				*acquired_id);
			WARN(1, "calc ffs error");
			return GET_LUT_ERR;
		}

		spin_lock_irqsave(&hwtcon_fb_info()->lut_free_lock, flags);
		hwtcon_fb_info()->lut_free &= ~(1LL << *acquired_id);
		spin_unlock_irqrestore(&hwtcon_fb_info()->lut_free_lock, flags);

		spin_lock_irqsave(&hwtcon_fb_info()->lut_active_lock, flags);
		hwtcon_fb_info()->lut_active |= (1LL << *acquired_id);
		spin_unlock_irqrestore(&hwtcon_fb_info()->lut_active_lock, flags);

		TCON_LOG("alloc free lut:%d lut status: 0x%016llx 0x%016llx 0x%016llx",
			*acquired_id,
			hwtcon_fb_info()->lut_free,
			hwtcon_core_get_released_lut(),
			hwtcon_fb_info()->lut_active);

		return GET_LUT_OK;
	} else {
		/* free lut not available */
		if (active_lut == LUT_BIT_ALL_SET) {
			/* active lut all busy, need to wait WF_LUT release lut */
			int status = 0;

			status = wait_event_timeout(
				hwtcon_fb_info()->wf_lut_release_wait_queue,
				(hwtcon_fb_info()->lut_active != LUT_BIT_ALL_SET),
				msecs_to_jiffies(HWTCON_WAIT_WF_LUT_RELEASE_TIMEOUT));

			if (status == 0) {
				TCON_ERR("wait wf_lut release lut timeout");
				return GET_LUT_TIMEOUT;
			}
		}
		/* not all busy */
		if (pipeline_init_working_buffer() != 0)
			return GET_LUT_ERR;

		/* free lut not avilable need to retrigger get_free_lut */
		return GET_LUT_BUSY;
	}
}

void hwtcon_core_reset_pipeline(void)
{
	/* write 0 then 1 to reset
	 * 0: pipeline
	 * 1: image buffer rdma
	 * 2: working buffer rdma
	 * 3. working buffer wdma
	 * 4: wf_lut
	 * 5: dpi
	 * 6: tcon
	 * 7: main reset
	 */
	pp_write(NULL, MMSYS_SW1_RST_B, 0xF0);
	pp_write(NULL, MMSYS_SW1_RST_B, 0xFF);
}

void hwtcon_core_reset_mmsys(void)
{
	/* write 0 then 1 to reset
	 * 0: pipeline
	 * 1: image buffer rdma
	 * 2: working buffer rdma
	 * 3. working buffer wdma
	 * 4: wf_lut
	 * 5: dpi
	 * 6: tcon
	 * 7: main reset
	 */
	pp_write(NULL, MMSYS_SW1_RST_B, 0x00);
	pp_write(NULL, MMSYS_SW1_RST_B, 0xFF);
}
