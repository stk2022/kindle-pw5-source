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
#include <linux/pm_wakeup.h>
#include "hwtcon_ioctl_cmd.h"
#include "hwtcon_def.h"
#include "hwtcon_rect.h"

/* read temperature from sensor every 1 mins */
#define SENSOR_READ_INTERVAL_MS 60000

#define AUTO_WAVEFORM_TABLE_CNT 25
#define FB_W_ALIGN 16

#define ONE_GRAY_LEVEL 15 
#define TRANSITION_THRESHOLD 49
#define STRENGTH_THREASHOLD 10
#define SCAN_LINE_TOTAL 10
#define SCAN_LINE_THRESHOLD 4
#define SCALED_FACTOR 375


enum HWTCON_AUTO_WF_INFO_ENUM {
	SLOT_FB_CUR_HISTOGRAM = 0,
	SLOT_FB_NXT_HISTOGRAM = 1,
	SLOT_FB_CUR_GREY = 2,
	SLOT_FB_NXT_GREY = 3,
	SLOT_FB_INDEX = 4,
	SLOT_FB_AUTO_WF_MODE = 5,
	SLOT_FB_MAX,
};

enum HWTCON_ROTATE_ENUM {
	HWTCON_ROTATE_0 = 0,	/* 0 degree rotate */
	HWTCON_ROTATE_90 = 1,	/* 90 degree rotate */
	HWTCON_ROTATE_180 = 2,	/*180 degree rotate */
	HWTCON_ROTATE_270 = 3,	/* 270 degree rotate */
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

struct fb_private_info {
	/* frame buffer info */
	dma_addr_t fb_buffer_pa;
	char *fb_buffer_va;
	size_t fb_buffer_size;

	/* mdp buffer info*/
	dma_addr_t mdp_buffer_pa;
	char *mdp_buffer_va;
	size_t mdp_buffer_size;

	/* tmp buffer info*/
        dma_addr_t tmp_buffer_pa;
        char *tmp_buffer_va;
        size_t tmp_buffer_size;

	/* working buffer info */
	dma_addr_t wb_pa[2];
	char *wb_va[2];
	size_t wb_size[2];

	dma_addr_t waveform_pa;
	char *waveform_va;
	size_t waveform_size;

	/* swdata test */
	dma_addr_t swdata_pa;
	char *swdata_va;
	size_t swdata_size;

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

	/* record lut status */
	spinlock_t lut_info_lock;
	struct wf_lut_info lut_info[MAX_LUT_REGION_COUNT];

	/* pipeline write buffer index 0 / 1 */
	bool read_buffer_index;
	dma_addr_t slot_read_buffer_index;

	/* day mode auto wavefrom table slot */
	dma_addr_t slot_auto_waveform;

	/* waveform_info slot */
	dma_addr_t slot_auto_waveform_info;

	enum hwtcon_update_scheme update_scheme;

	/* wake lock for pm_ops */
	struct wakeup_source wake_lock;

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
	wait_queue_head_t power_state_change_wq;

	/* wait queue for wait wf_lut release */
	wait_queue_head_t wf_lut_release_wait_queue;

	/* wait queue for trigger MDP & collision detect */
	wait_queue_head_t mdp_trigger_wait_queue;
	wait_queue_head_t pipeline_trigger_wait_queue;

	/* wait for task state change to spefic state. */
	wait_queue_head_t task_state_wait_queue;

	/* wait for irq status clear */
	wait_queue_head_t hwtcon_irq_clear_wait_queue;

	/* wait for fb flush done queue */
	wait_queue_head_t hwtcon_fb_flush_done_wq;

	/* work for power down mmsys */
	struct work_struct wk_power_down_mmsys;
	/* thread for power down mmsys domain */
	struct workqueue_struct *wq_power_down_mmsys;

	/* work for disable mmsys clock */
	struct work_struct wk_disable_clk;
	/* thread for disable mmsys clock */
	struct workqueue_struct *wq_disable_clk;

	/* thread for handle pipeline work done */
	struct workqueue_struct *wq_pipeline_written_done;
	/* thread for handle wf_lut display done. */
	struct workqueue_struct *wq_wf_lut_display_done;

	struct mutex update_queue_mutex;
	struct mutex image_buffer_access_mutex;

	struct device *dev;
	
	struct sw_mitigation sw_algo;
};

int hwtcon_fb_register_fb(struct platform_device *pdev);
int hwtcon_fb_unregister_fb(struct platform_device *pdev);
struct fb_private_info *hwtcon_fb_info(void);
u32 hwtcon_fb_get_rotation(void);
void hwtcon_fb_get_resolution(u32 *width, u32 *height);
u32 hwtcon_fb_get_grayscale(void);
void hwtcon_fb_flush_update(void);
int hwtcon_fb_get_virtual_width(void);
int hwtcon_fb_get_width(void);
int hwtcon_fb_get_height(void);

#endif /* __HWTCON_FB_H__ */
