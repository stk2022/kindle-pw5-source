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

#ifndef __HWTCON_FB_H__
#define __HWTCON_FB_H__
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/pm_wakeup.h>
#include <linux/pm_qos.h>
#include <linux/fb.h>
#include "hwtcon_ioctl_cmd.h"
#include "hwtcon_def.h"
#include "hwtcon_rect.h"
#include "hwtcon_pipeline_config.h"

#define FB_W_ALIGN 16
/* read temperature from sensor every 1 mins */
#define SENSOR_READ_INTERVAL_MS 60000
#define SENSOR_READ_SHORT_INTERVAL_MS 1000
#define MAX_TEMP_READ 3

#define MAX_DEBUG_IMAGE_BUFFER_COUNT 10
#define MAX_FILE_NAME_LEN 100

#define ONE_GRAY_LEVEL 15
#define TRANSITION_THRESHOLD 49
#define STRENGTH_THREASHOLD 10
#define SCAN_LINE_TOTAL 10
#define SCAN_LINE_THRESHOLD 4
#define SCALED_FACTOR 375

enum LUT_RELEASE_ENUM {
	/* reserve for LUT 0 ~ 63 */
	LUT_RELEASE_TIME_START = 64,
	LUT_RELEASE_TIME_END = 65,
	LUT_RELEASE_MAX,
};

struct hwtcon_task_list {
	spinlock_t lock;
	struct list_head list;
};

struct wf_lut_info {
	bool busy;
	struct rect region;
	int priority;
	enum WAVEFORM_MODE_ENUM waveform_mode;
	struct hwtcon_task *task;
};

/* data structure for sw mitigation (in-rush current reduction) */
struct sw_mitigation {
        __u32 pixel_thres; /* percentage of pixel changed each line in % */
        __u32 count_thres; /* line exceed above threshold in %*/
        __u32 scaled_width;   /* resample width */
        __u32 scaled_height; /* resample height */
        __u32 scan_lines;     /* lines to be scanned */
        __u32 str_thres; /* average scanned line pixel value changed threshold in % */
        __u32 scaled_factor; /* used to calculate scaled width and height in %o*/
};
/* Store completed marker for MXCFB_WAIT_FOR_ANY_UPDATE_COMPLETE support */
struct completed_marker_store {
	__u32 marker[STORE_MARKER_NUM];
	struct mutex store_mutex;
	unsigned int num_marker;
	unsigned int  head;
	unsigned int  tail;
};

struct fb_private_info {
	struct regulator *regulator_vcore;
	struct pm_qos_request *vcore_req;


	/* frame buffer info: MDP's input */
	dma_addr_t fb_buffer_pa;
	char *fb_buffer_va;
	size_t fb_buffer_size;

	/* tmp buffer info*/
        dma_addr_t tmp_buffer_pa;
        char *tmp_buffer_va;
        size_t tmp_buffer_size;

	/* img buffer info: MDP's output, PIPELINE's input */
	dma_addr_t img_buffer_pa;
	char *img_buffer_va;
	size_t img_buffer_size;

	/* temp image buffer info: regal buffer */
	dma_addr_t temp_img_buffer_pa;
	char *temp_img_buffer_va;
	size_t temp_img_buffer_size;

	/* debug image buffer info:
	 * reserve 10 image buffers for buffer save.
	 * only for debug issue.
	 */
	dma_addr_t debug_img_buffer_pa[MAX_DEBUG_IMAGE_BUFFER_COUNT];
	char *debug_img_buffer_va[MAX_DEBUG_IMAGE_BUFFER_COUNT];
	size_t debug_img_buffer_size[MAX_DEBUG_IMAGE_BUFFER_COUNT];
	char *debug_img_buffer_name[MAX_DEBUG_IMAGE_BUFFER_COUNT];
	int debug_img_buffer_counter;
	bool debug_img_buffer_available;

	/* record LUT's picture order */
	u32 lut_pic_order[MAX_LUT_REGION_COUNT];
	spinlock_t lut_pic_order_lock;

	/* working buffer info */
	dma_addr_t wb_buffer_pa;
	char *wb_buffer_va;
	size_t wb_buffer_size;

	/* waveform buffer info */
	dma_addr_t waveform_pa;
	char *waveform_va;
	size_t waveform_size;

	/* LUT management */
	spinlock_t lut_free_lock;
	u64 lut_free;
	spinlock_t lut_active_lock;
	u64 lut_active;

	/* marker management: record every marker data in hwtcon. */
	struct hwtcon_task_list fb_global_marker_list;
	struct completion wb_frame_done_completion;
	struct completion wf_lut_release_completion;

	/* update sequence */
	spinlock_t g_update_order_lock;
	u32 g_update_order;
	u32 g_update_cnt;

	/* wake lock for pm_ops */
	struct wakeup_source wake_lock;

	/* Fake palette of 16 colors */
	u32 pseudo_palette[17];

	/* eink TEMPERATURE */
	int temperature;
	struct delayed_work read_sensor_work;

	/* first ioctl call */
	bool hwtcon_first_call;

	/*record whether power enabled */
	struct mutex mmsys_power_enable_lock;
	bool mmsys_power_enable;

	/* record whether clock enabled */
	spinlock_t hwtcon_clk_enable_lock;
	bool hwtcon_clk_enable;

	/* add a lock for wf_lut debug reg */
	spinlock_t hwtcon_tcon_reg_lock;

	/* for mmsys power close. */
	struct timer_list mmsys_power_timer;
	/* power down delay time.
	 * time elapse need to power down after HW frame done.
	 */
	int power_down_delay_ms;

	/* current temperature zone */
	int current_temp_zone;

	/* current night mode */
	int current_night_mode;

	/* pipeline hw busy */
	bool pipeline_busy;

	/* ignore update request. */
	bool ignore_request;

	/* blank state */
	int blank;

	/* night mode or not */
	bool enable_night_mode;

	/* add timer for each LUT update for error detect and recovery*/
	struct timer_list timer_lut_release[MAX_LUT_REGION_COUNT];

	enum hwtcon_update_scheme update_scheme;

	/* slot for GCE to backup register */
	cmdqBackupSlotHandle lut_release_slot;

	/* all task list in hwtcon driver. */
	/* free task list */
	struct hwtcon_task_list free_task_list;
	/* acquire task done, wait for mdp process  */
	struct hwtcon_task_list wait_for_mdp_task_list;
	/* mdp process done */
	struct hwtcon_task_list mdp_done_task_list;

	/* pipeilne is processing */
	struct hwtcon_task_list pipeline_processing_task_list;
	/* pipeline process done */
	struct hwtcon_task_list pipeline_done_task_list;
	/* collision task list */
	struct hwtcon_task_list collision_task_list;

	/* wait queue for power state change */
	wait_queue_head_t power_state_change_wait_queue;

	/* wait queue for wait wf_lut release */
	wait_queue_head_t wf_lut_release_wait_queue;

	/* wait queue for trigger MDP & collision detect */
	wait_queue_head_t mdp_trigger_wait_queue;
	wait_queue_head_t pipeline_trigger_wait_queue;

	/* wait for task state change to spefic state. */
	wait_queue_head_t task_state_wait_queue;

	/* work for power down mmsys */
	struct work_struct wk_power_down_mmsys;
	/* thread for power down mmsys domain */
	struct workqueue_struct *wq_power_down_mmsys;

	/* thread for handle pipeline work done */
	struct workqueue_struct *wq_pipeline_written_done;
	/* thread for handle wf_lut display done. */
	struct workqueue_struct *wq_wf_lut_display_done;

	struct mutex update_queue_mutex;
	struct mutex image_buffer_access_mutex;

	struct device *dev;

	struct sw_mitigation sw_algo;
	struct completed_marker_store marker_store;
	u32 update_flag_fast_mode; /*For convert large AUTO (GL16/DU to REAGL, in case not fast mode */
};

int hwtcon_fb_register_fb(struct platform_device *pdev);
int hwtcon_fb_unregister_fb(struct platform_device *pdev);
struct fb_private_info *hwtcon_fb_info(void);
u32 hwtcon_fb_get_rotation(void);
void hwtcon_fb_get_resolution(u32 *width, u32 *height);
u32 hwtcon_fb_get_grayscale(void);
void hwtcon_fb_flush_update(void);
int hwtcon_fb_check_update_data_invalid(
	struct mxcfb_update_data *update_data);
int hwtcon_fb_get_virtual_width(void);
int hwtcon_fb_get_width(void);
int hwtcon_fb_get_height(void);

int hwtcon_fb_ioctl_set_update_flags(struct fb_info *info, unsigned long arg);
int hwtcon_fb_ioctl_get_update_flags(struct fb_info *info, unsigned long arg);
int hwtcon_fb_ioctl_wait_for_any_update_complete(struct fb_info *info, unsigned long arg);
int marker_store_reset(struct completed_marker_store  *p_marker_store);
int marker_store_init(struct completed_marker_store  *p_marker_store);
int store_completed_markers(struct completed_marker_store  *p_marker_store, __u32 done_marker);
int complete_markers_avail(struct completed_marker_store  *p_marker_store);
int auto_waveform_replacement(struct mxcfb_update_data *upd_data);
int hwtcon_fb_alloc_debug_img_buffer(struct fb_private_info *private_info);

#endif /* __HWTCON_FB_H__ */
