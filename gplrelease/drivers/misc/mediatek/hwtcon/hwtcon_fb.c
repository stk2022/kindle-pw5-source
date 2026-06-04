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

#include "hwtcon_fb.h"
#include "hwtcon_def.h"
#include "hwtcon_debug.h"
#include "hwtcon_core.h"
#include "fiti_core.h"
#include "hwtcon_wf_lut_config.h"

#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include "hwtcon_epd.h"
#include "hwtcon_driver.h"

static struct fb_info *g_framebuffer_info;

static int hwtcon_fb_open(struct fb_info *info, int user)
{
	return 0;
}

static int hwtcon_fb_release(struct fb_info *info, int user)
{
	return 0;
}

/* no use */
static int hwtcon_fb_setcolreg(unsigned int regno, unsigned int red,
	unsigned int green,
	unsigned int blue, unsigned int transp, struct fb_info *info)
{
	return 0;
}

/* no use */
static int hwtcon_fb_pan_display_proxy(struct fb_var_screeninfo *var,
	struct fb_info *info)
{
	return 0;
}

/* no use */
static int hwtcon_fb_set_par(struct fb_info *fbi)
{
	return 0;
}

static int hwtcon_fb_check_rotate(struct fb_var_screeninfo *var, struct fb_info *info)
{
	var->rotate = var->rotate % 4;
	switch (var->rotate) {
	case HWTCON_ROTATE_0:
	case HWTCON_ROTATE_180:
		var->xres = hw_tcon_get_display_width();
		var->yres = hw_tcon_get_display_height();
		var->width = hw_tcon_get_edp_area_width();
		var->height = hw_tcon_get_edp_area_height();
		break;
	case HWTCON_ROTATE_90:
	case HWTCON_ROTATE_270:
		var->xres = hw_tcon_get_display_height();
		var->yres = hw_tcon_get_display_width();
		var->width = hw_tcon_get_edp_area_height();
		var->height = hw_tcon_get_edp_area_width();
		break;
	default:
		TCON_ERR("invalid rotate:%d check fail",
			var->rotate);
		return -1;
	}

	var->xres_virtual = ALIGN(var->xres, FB_W_ALIGN);
	var->yres_virtual = var->yres * FB_VIRTUAL_FRAME_COUNT;
	info->fix.line_length = var->xres_virtual;

	return 0;
}

int hwtcon_fb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	if (hwtcon_fb_check_rotate(var, info)) {
		return -1;
	}

	if (memcmp(var, &info->var, sizeof(struct fb_var_screeninfo)) != 0) {
		int status = 0;

		TCON_LOG("fb_var info changed");
		hwtcon_fb_info()->ignore_request = true;
		/* wait all task release */
		status = hwtcon_core_wait_all_task_done();
		hwtcon_fb_info()->ignore_request = false;
		return status;
	}

	return 0;
}

void hwtcon_fb_flush_update(void)
{
	int ret;

	mutex_lock(&hwtcon_fb_info()->update_queue_mutex);

	/* hwtcon busy,need to wait it finish update*/
	if (!hwtcon_core_check_hwtcon_idle()) {
		ret  = wait_event_timeout(
			hwtcon_fb_info()->hwtcon_fb_flush_done_wq,
			(hwtcon_core_check_hwtcon_idle() == true),
			msecs_to_jiffies(20000));
		if (!ret) {
			TCON_ERR("wait for flush update finish timeout");
		}
	}

	mutex_unlock(&hwtcon_fb_info()->update_queue_mutex);
}

static int hwtcon_fb_blank(int blank_mode, struct fb_info *info)
{
	TCON_LOG("fb blank mode:%d", blank_mode);
	if (hwtcon_fb_info()->blank == blank_mode) {
		return 0;
	}

    switch (blank_mode) {
	case FB_BLANK_UNBLANK:
		/* resume*/
		hwtcon_fb_info()->blank = blank_mode;
		break;
    case FB_BLANK_NORMAL:
    case FB_BLANK_VSYNC_SUSPEND:
    case FB_BLANK_HSYNC_SUSPEND:
		hwtcon_fb_flush_update();
		break;
    case FB_BLANK_POWERDOWN:
		hwtcon_fb_flush_update();
		if (hwtcon_fb_info()->mmsys_power_enable == true &&
			hwtcon_fb_info()->power_down_delay_ms == EINK_NO_POWER_DOWN)
		{
			/* no power off mode, force to power off here*/
			hwtcon_driver_enable_mmsys_power(NULL, false);
		} else if (hwtcon_fb_info()->mmsys_power_enable == true) {
		    /* power on, but need wait for power off finished.
		       if timeout force to power off here */
			hwtcon_core_wait_power_down();
		}
		hwtcon_fb_info()->blank = blank_mode;
		/* suspend */
		break;
    default:
		TCON_ERR("invalid blank_mode:%d", blank_mode);
        return -EINVAL;
    }

    return 0;
}

static int hwtcon_core_update_task_wf_mode(
	struct mxcfb_update_data *update_data)
{
	enum WAVEFORM_MODE_ENUM wf_mode = update_data->waveform_mode;
	bool full_update = update_data->update_mode;

	if (hwtcon_core_use_night_mode()) {
		if (wf_mode == WAVEFORM_MODE_GC16)
			wf_mode = WAVEFORM_MODE_GCK16;
		if (wf_mode == WAVEFORM_MODE_GLR16)
			wf_mode = WAVEFORM_MODE_GLKW16;
	}

	switch (wf_mode) {
	case WAVEFORM_MODE_GC16:
		if (!full_update)
			wf_mode = WAVEFORM_MODE_GC16_PARTIAL;
		break;
	case WAVEFORM_MODE_GCK16:
		if (!full_update)
			wf_mode = WAVEFORM_MODE_GCK16_PARTIAL;
		break;
	case WAVEFORM_MODE_INIT:
	case WAVEFORM_MODE_DU:
	case WAVEFORM_MODE_GL16:
	case WAVEFORM_MODE_GLR16:
	case WAVEFORM_MODE_GLD16:
	case WAVEFORM_MODE_A2:
	case WAVEFORM_MODE_DU4:
	case WAVEFORM_MODE_GLKW16:
		/* bypass check */
		#if 0
		if (!full_update) {
			TCON_ERR("waveform mode:%d->%s doesn't support partial",
				wf_mode,
				hwtcon_core_get_wf_mode_name(wf_mode));
			return -1;
		}
		#endif
		break;
	case WAVEFORM_MODE_AUTO:
		break;
	default:
		TCON_ERR("invalid waveform mode:%d->%s",
			wf_mode,
			hwtcon_core_get_wf_mode_name(wf_mode));
		return -1;
	}
	update_data->waveform_mode = wf_mode;
	return 0;
}

int hwtcon_fb_get_width(void) 
{
	return g_framebuffer_info->var.xres;
}

int hwtcon_fb_get_height(void)
{
	return g_framebuffer_info->var.yres;
}

int hwtcon_fb_get_virtual_width(void)
{
	return g_framebuffer_info->var.xres_virtual;
}

static int hwtcon_fb_check_update_data_invalid(
	struct mxcfb_update_data *update_data)
{
	struct rect panel_region = {0, 0,
		hw_tcon_get_edp_width(),
		hw_tcon_get_edp_height()};
	struct rect task_region = {0};

	task_region = hwtcon_core_get_update_data_region(update_data);

	if (update_data->update_mode != UPDATE_MODE_FULL &&
		update_data->update_mode != UPDATE_MODE_PARTIAL) {
		TCON_ERR("invalid update_mode:%d", update_data->update_mode);
		return HWTCON_STATUS_INVALID_PARAM;
	}

	if (!hwtcon_rect_contain_00(&panel_region, &task_region)) {
		TCON_ERR("invalid region[%d %d %d %d] panel[%d %d] rot:%d",
			task_region.x,
			task_region.y,
			task_region.width,
			task_region.height,
			hw_tcon_get_edp_width(),
			hw_tcon_get_edp_height(),
			hwtcon_fb_get_rotation());
		return HWTCON_STATUS_INVALID_PARAM;
	}

	if (hwtcon_core_update_task_wf_mode(update_data) != 0) {
		TCON_ERR("invalid waveform_mode:%d update_mode:%d",
			update_data->waveform_mode,
			update_data->update_mode);
		return HWTCON_STATUS_INVALID_PARAM;
	}

	return 0;
}

static int hwtcon_fb_ioctl_set_temperature(void *arg)
{
	int value = 0;

	if (copy_from_user(&value, arg, sizeof(value)) != 0) {
		TCON_ERR("copy_from_user fail");
		return HWTCON_STATUS_COPY_FROM_USER_FAIL;
	}

	TCON_LOG("set temperature:%d", value);
	hwtcon_fb_info()->temperature = value;

	return 0;
}

static int hwtcon_fb_ioctl_get_temperature(void *arg)
{
	int value = hwtcon_core_read_temperature();

	if (copy_to_user((void *)arg, &value, sizeof(value)) != 0) {
		TCON_ERR("copy_to_user fail");
		return HWTCON_STATUS_COPY_TO_USER_FAIL;
	}

	return 0;
}

static int hwtcon_fb_ioctl_send_update(void *arg)
{
	int status = 0;
	struct mxcfb_update_data update_data;

	if (hwtcon_fb_info()->ignore_request || hwtcon_fb_info()->blank != FB_BLANK_UNBLANK) {
		TCON_LOG("ignore request");
		return 0;
	}

	memset(&update_data, 0, sizeof(update_data));
	if (copy_from_user(&update_data, (void *)arg,
		sizeof(update_data)) != 0) {
		TCON_ERR("copy_from_user fail");
		return HWTCON_STATUS_COPY_FROM_USER_FAIL;
	}

	status = hwtcon_fb_check_update_data_invalid(&update_data);
	if (status != 0)
		return status;

	status = hwtcon_core_submit_task(&update_data);

	return status;
}

static int hwtcon_fb_ioctl_set_update_scheme(void *arg)
{
	u32 scheme = 0;

	if (copy_from_user(&scheme, arg, sizeof(scheme)) != 0) {
		TCON_ERR("copy_from_user fail");
		return HWTCON_STATUS_COPY_FROM_USER_FAIL;
	}

	/* check param valid. */
	if ((scheme != UPDATE_SCHEME_QUEUE) && 
		(scheme != UPDATE_SCHEME_QUEUE_AND_MERGE)) {
		TCON_ERR("invalid param: %d", scheme);
		return HWTCON_STATUS_INVALID_UPDATE_SCHEME;
	}

	hwtcon_fb_info()->update_scheme = scheme;
	return 0;
}

static int hwtcon_fb_ioctl_wait_for_task_triggered(void *arg)
{
	u32 update_marker = 0;

	if (copy_from_user(&update_marker, arg, sizeof(u32)) != 0) {
		TCON_ERR("copy_from_user fail");
		return HWTCON_STATUS_COPY_FROM_USER_FAIL;
	}

	return hwtcon_core_wait_for_task_triggered(update_marker);
}

static int hwtcon_fb_ioctl_wait_for_task_displayed(void *arg)
{
	u32 update_marker = 0;

	if (copy_from_user(&update_marker, arg, sizeof(u32)) != 0) {
		TCON_ERR("copy_from_user fail");
		return HWTCON_STATUS_COPY_FROM_USER_FAIL;
	}

	return hwtcon_core_wait_for_task_displayed(update_marker);
}

static int hwtcon_fb_ioctl_get_working_buffer(void *arg)
{
	int index = 0;

	index = hwtcon_core_get_wb_index();
	if (index < 0 || index > 1) {
		TCON_ERR("invalid index:%d", index);
		return HWTCON_STATUS_INVALID_WB_INDEX;
	}

	if (copy_to_user(arg, hwtcon_fb_info()->wb_va[index],
		hwtcon_fb_info()->wb_size[index]) != 0) {
		TCON_ERR("copy_to_user fail");
		return HWTCON_STATUS_COPY_TO_USER_FAIL;
	}
	return 0;
}

static int hwtcon_fb_ioctl_get_waveform_type(void *arg)
{
	u32 wf_type = 0;

	wf_type = hwtcon_core_get_waveform_type();

	if (copy_to_user(arg, &wf_type, sizeof(wf_type)) != 0) {
		TCON_ERR("copy to user fail");
		return HWTCON_STATUS_COPY_TO_USER_FAIL;
	}

	return 0;
}

static int hwtcon_fb_ioctl_get_material_type(void *arg)
{
	u32 material_type = 0;

	material_type = hw_tcon_get_edp_material_type();

	if (copy_to_user(arg, &material_type, sizeof(material_type)) != 0) {
		TCON_ERR("copy to user fail");
		return HWTCON_STATUS_COPY_TO_USER_FAIL;
	}

	return 0;
}


static int hwtcon_fb_ioctl_set_night_mode(void *arg)
{
	struct mxcfb_nightmode_ctrl night_mode_info;

	if (copy_from_user(&night_mode_info, arg,
		sizeof(night_mode_info)) != 0) {
		TCON_ERR("copy from user fail");
		return HWTCON_STATUS_COPY_FROM_USER_FAIL;
	}
	/* hwtcon_fb_info()->enable_night_mode = !night_mode_info.disable; */
	return 0;
}

static int hwtcon_fb_ioctl_set_power_down_delay_time(void *arg)
{
	int delay_time = 0;

	if (copy_from_user(&delay_time, arg, sizeof(delay_time)) != 0) {
		TCON_ERR("copy from user fail");
		return HWTCON_STATUS_COPY_FROM_USER_FAIL;
	}

	/* check param valid */
	if (delay_time < 0) {
		TCON_ERR("invalid delay time:%d", delay_time);
		return HWTCON_STATUS_INVALID_PARAM;
	}

	hwtcon_fb_info()->power_down_delay_ms = delay_time;
	return 0;
}

static int hwtcon_fb_ioctl_get_power_down_delay_time(void *arg)
{
	int delay_time = hwtcon_fb_info()->power_down_delay_ms;

	if (copy_to_user(arg, &delay_time, sizeof(delay_time)) != 0) {
		TCON_ERR("invalid copy to user");
		return HWTCON_STATUS_COPY_TO_USER_FAIL;
	}

	return 0;
}

static int hwtcon_fb_ioctl_set_pause(void *arg)
{
	hwtcon_fb_info()->ignore_request = true;
	return 0;
}

static int hwtcon_fb_ioctl_set_resume(void *arg)
{
	hwtcon_fb_info()->ignore_request = false;
	return 0;
}

static int hwtcon_fb_ioctl_get_pause(void *arg)
{
	u32 ignore_request = hwtcon_fb_info()->ignore_request;

	if (copy_to_user(arg, &ignore_request, sizeof(ignore_request)) != 0) {
		TCON_ERR("copy to user fail");
		return HWTCON_STATUS_COPY_TO_USER_FAIL;
	}

	return 0;
}

static int hwtcon_fb_ioctl_get_panel_info(void *arg)
{
	struct mxcfb_panel_info info;

	memset(&info, 0, sizeof(info));
	info.temp = hwtcon_core_read_temperature();
	info.temp_zone = hwtcon_core_read_temp_zone();
	info.vcom_value = fiti_read_vcom();
	snprintf(info.wf_file_name,	sizeof(info.wf_file_name), "%s",
		wf_lut_waveform_get_name());

	if (copy_to_user(arg, &info, sizeof(info)) != 0) {
		TCON_ERR("copy to user fail");
		return HWTCON_STATUS_COPY_TO_USER_FAIL;
	}
	return 0;
}

static int hwtcon_fb_ioctl(struct fb_info *info, unsigned int cmd,
	unsigned long arg)
{
	switch (cmd) {
	case MXCFB_SET_WAVEFORM_MODES:
		break;
	case MXCFB_SET_TEMPERATURE:
		return hwtcon_fb_ioctl_set_temperature((void *)arg);
	case MXCFB_GET_TEMPERATURE:
		return hwtcon_fb_ioctl_get_temperature((void *)arg);
	case MXCFB_SEND_UPDATE:
		return hwtcon_fb_ioctl_send_update((void *)arg);
	case MXCFB_SET_UPDATE_SCHEME:
		return hwtcon_fb_ioctl_set_update_scheme((void *)arg);
	case MXCFB_WAIT_FOR_UPDATE_SUBMISSION:
		return hwtcon_fb_ioctl_wait_for_task_triggered((void *)arg);
	case MXCFB_WAIT_FOR_UPDATE_COMPLETE:
		return hwtcon_fb_ioctl_wait_for_task_displayed((void *)arg);
	case MXCFB_GET_WORK_BUFFER:
		return hwtcon_fb_ioctl_get_working_buffer((void *)arg);
	case MXCFB_GET_WAVEFORM_TYPE:
		return hwtcon_fb_ioctl_get_waveform_type((void *)arg);
	case MXCFB_GET_MATERIAL_TYPE:
		return hwtcon_fb_ioctl_get_material_type((void *)arg);
	case MXCFB_SET_NIGHTMODE:
		return hwtcon_fb_ioctl_set_night_mode((void *)arg);
	case MXCFB_SET_PWRDOWN_DELAY:
		return hwtcon_fb_ioctl_set_power_down_delay_time((void *)arg);
	case MXCFB_GET_PWRDOWN_DELAY:
		return hwtcon_fb_ioctl_get_power_down_delay_time((void *)arg);
	case MXCFB_SET_PAUSE:
		return hwtcon_fb_ioctl_set_pause((void *)arg);
	case MXCFB_SET_RESUME:
		return hwtcon_fb_ioctl_set_resume((void *)arg);
	case MXCFB_GET_PAUSE:
		return hwtcon_fb_ioctl_get_pause((void *)arg);
	case MXCFB_GET_PANEL_INFO:
		return hwtcon_fb_ioctl_get_panel_info((void *)arg);
	case MXCFB_SET_AUTO_UPDATE_MODE:
		return 0;
	default:
		TCON_ERR("err cmd:0x%08x dir:0x%x type:0x%x nr:0x%x size:0x%x",
			cmd,
			_IOC_DIR(cmd),
			_IOC_TYPE(cmd),
			_IOC_NR(cmd),
			_IOC_SIZE(cmd));
		return HWTCON_STATUS_INVALID_IOCTL_CMD;
	}

	TCON_ERR("ioctl cmd:0x%x not implement", cmd);
	return 0;
}

#ifdef CONFIG_COMPAT
static int hwtcon_fb_compat_ioctl(struct fb_info *info, unsigned int cmd,
	unsigned long arg)
{
	return hwtcon_fb_ioctl(info, cmd, arg);
}
#endif


/* frame buffer already have mmap implement,
 * this function can be removed.
 */
static int hwtcon_fb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	int status = 0;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	/* vma->vm_pgoff << PAGE_SHIFT */

	#if 0
	status = remap_pfn_range(vma,
		vma->vm_start,	/* target userspace va address */
		#if 1
		((struct fb_private_info *)(info->par))->fb_buffer_pa >>
			PAGE_SHIFT,	/* kernel pa address */
		#else
		vma->vm_pgoff,
		#endif
		(vma->vm_end - vma->vm_start),	/* mapped size */
		vma->vm_page_prot);
	if (status != 0) {
		TCON_ERR("remap_pfn_range fail");
		return status;
	}
	vma->vm_ops = &hwtcon_remap_vm_ops;
	hwtcon_vma_open(vma);
	#else
	status = dma_mmap_attrs(((struct fb_private_info *)(info->par))->dev,
		vma,
		((struct fb_private_info *)(info->par))->mdp_buffer_va,
		((struct fb_private_info *)(info->par))->mdp_buffer_pa,
		((struct fb_private_info *)(info->par))->mdp_buffer_size,
		0);
	if (status != 0)
		TCON_ERR("mmap fail: status:%d size:%d",
			status,
			((struct fb_private_info *)
			(info->par))->fb_buffer_size);
	#endif

	return status;
}

static int hwtcon_fb_setcmap(struct fb_cmap *cmap, struct fb_info *info)
{
	mdp_update_cmap_lut(cmap->red, cmap->len);

	return 0;
}

static struct fb_ops hwtcon_fb_ops = {
	.owner = THIS_MODULE,
	.fb_open = hwtcon_fb_open,
	.fb_release = hwtcon_fb_release,
	.fb_setcolreg = hwtcon_fb_setcolreg,
	.fb_pan_display = hwtcon_fb_pan_display_proxy,
	.fb_check_var = hwtcon_fb_check_var,
	.fb_blank = hwtcon_fb_blank,
	#if 0
	.fb_fillrect = cfb_fillrect,
	.fb_copyarea = cfb_copyarea,
	.fb_imageblit = cfb_imageblit,
	#endif
	.fb_set_par = hwtcon_fb_set_par,
	.fb_ioctl = hwtcon_fb_ioctl,
#ifdef CONFIG_COMPAT
	.fb_compat_ioctl = hwtcon_fb_compat_ioctl,
#endif
	.fb_mmap = hwtcon_fb_mmap,
	.fb_setcmap = hwtcon_fb_setcmap,

};

static struct task_struct *pThread1;
static struct task_struct *pThread2;
static int hwtcon_fb_kthread_create(void)
{
	struct sched_param param = {2};

	if (pThread1 == NULL) {
		pThread1 = kthread_run(hwtcon_core_dispatch_pipeline, NULL,
			"dispatch_pipeline");
		if (IS_ERR(pThread1)) {
			TCON_ERR("create thread dispatch_pipeline failed");
			return HWTCON_STATUS_CREAT_THREAD_FAIL;
		}

		/* adjust thread priority. */
		sched_setscheduler(pThread1, SCHED_RR, &param);
	}

	if (pThread2 == NULL) {
		pThread2 = kthread_run(hwtcon_core_dispatch_mdp, NULL,
			"dispatch_mdp");
		if (IS_ERR(pThread2)) {
			TCON_ERR("create thread hwtcon_dispatch_mdp failed");
			return HWTCON_STATUS_CREAT_THREAD_FAIL;
		}
		/* adjust thread priority. */
		sched_setscheduler(pThread2, SCHED_RR, &param);
	}
	#if 0
	/* hwtcon_core_start_lut_assign_done_trigger_loop(); */
	#endif

	return 0;
}

static int hwtcon_fb_kthread_destroy(void)
{
#if 0
	if (pThread1)
		kthread_stop(pThread1);
	if (pThread2)
		kthread_stop(pThread2);
#endif
	return 0;
}

void hwtcon_fb_read_temperature_from_sensor(struct work_struct *workItem)
{
	hwtcon_fb_info()->temperature = fiti_read_temperature();
	TCON_LOG("read sensor temperature[%d]",
		hwtcon_fb_info()->temperature);
	schedule_delayed_work(&hwtcon_fb_info()->read_sensor_work,
			msecs_to_jiffies(SENSOR_READ_INTERVAL_MS));
}

static int hwtcon_fb_init_fb_info(struct fb_info *info)
{
	info->fbops = &hwtcon_fb_ops;
	info->flags = FBINFO_FLAG_DEFAULT;
	info->screen_buffer = ((struct fb_private_info *)
		(info->par))->mdp_buffer_va;
	info->screen_size = ((struct fb_private_info *)
		(info->par))->fb_buffer_size;
	info->pseudo_palette = ((struct fb_private_info *)
		(info->par))->pseudo_palette;

	/* fb_fix_screeninfo */
	strncpy(info->fix.id, HWTCON_DRIVER_NAME, sizeof(info->fix.id));
	info->fix.smem_start = ((struct fb_private_info *)
		(info->par))->mdp_buffer_pa;
	info->fix.smem_len = ((struct fb_private_info *)
		(info->par))->mdp_buffer_size;
	info->fix.type = FB_TYPE_PACKED_PIXELS;
	info->fix.visual = FB_VISUAL_MONO10;
	info->fix.xpanstep = 0;
	info->fix.ypanstep = 0;
	info->fix.ywrapstep = 0;
	info->fix.line_length =
		hw_tcon_get_edp_width();	/* 1 byte/pixel */
	info->fix.accel = FB_ACCEL_NONE;

	/* fb_var_screeninfo */
	info->var.rotate = hw_tcon_get_edp_rotate();
	hwtcon_fb_check_rotate(&info->var, info);
	info->var.xoffset = 0;
	info->var.yoffset = 0;

	info->var.bits_per_pixel = 8;
	info->var.grayscale = 1; /* 0 = color, 1 = grayscale, */
	info->var.nonstd = 1; /* non standard pixel format */
	info->var.activate = FB_ACTIVATE_NOW;

	return 0;
}

/* #define HWTCON_WF_LUT_DEBUG 1 */
#ifdef HWTCON_WF_LUT_DEBUG	/* only for debug */
__aligned(256) unsigned char SLT_WF_MODE_ADDR[] = {
	#include "wf_mode.bin"
};
#endif

static int hwtcon_fb_init_private_fb_info(
	struct fb_private_info *private_info, struct platform_device *pdev)
{
	int i = 0;
	static const int wf_table_partial[5][5] = {
		{1, 3, 3, 3, 3},
		{1, 3, 3, 3, 3},
		{1, 3, 3, 3, 3},
		{1, 3, 3, 3, 3},
		{3, 3, 3, 3, 3},
	};

	private_info->dev = &pdev->dev;
	/* set private_info as dev->data */
	dev_set_drvdata(&pdev->dev, private_info);

	/* allocate frame buffer */
	private_info->fb_buffer_size =
		hw_tcon_get_edp_width()*
		hw_tcon_get_edp_height() * FB_FRMAE_COUNT;
	private_info->fb_buffer_va = (char *)dma_alloc_coherent(&pdev->dev,
		private_info->fb_buffer_size,
		&private_info->fb_buffer_pa,
		GFP_KERNEL);
	if (private_info->fb_buffer_va == NULL) {
		TCON_ERR("allocate fb buffer fail");
		return HWTCON_STATUS_FB_ALLOC_FAIL;
	}

	/* allocate mdp buffer */
	private_info->mdp_buffer_size = max((ALIGN(hw_tcon_get_edp_width(), FB_W_ALIGN) * hw_tcon_get_edp_height()), 
		(hw_tcon_get_edp_width() * ALIGN(hw_tcon_get_edp_height(), FB_W_ALIGN))) * FB_VIRTUAL_FRAME_COUNT;	
	private_info->mdp_buffer_va = (char *)dma_alloc_coherent(&pdev->dev,
		private_info->mdp_buffer_size,
		&private_info->mdp_buffer_pa,
		GFP_KERNEL);
	if (private_info->mdp_buffer_va == NULL) {
		TCON_ERR("allocate mdp buffer fail");
		return HWTCON_STATUS_FB_ALLOC_FAIL;
	}

	/* allocate temp buffer */
	private_info->tmp_buffer_size = private_info->mdp_buffer_size;
	private_info->tmp_buffer_va = (char *)dma_alloc_coherent(&pdev->dev,
		private_info->tmp_buffer_size,
		&private_info->tmp_buffer_pa,
		GFP_KERNEL);
	if (private_info->tmp_buffer_va == NULL) {
                TCON_ERR("allocate tmp buffer fail");
                return HWTCON_STATUS_FB_ALLOC_FAIL;
        }

	/* allocate working buffer */
	for (i = 0; i < ARRAY_SIZE(private_info->wb_pa); i++) {
		private_info->wb_size[i] =
			hw_tcon_get_edp_width() *
			hw_tcon_get_edp_height() * 2;
		private_info->wb_va[i] = (char *)dma_alloc_coherent(&pdev->dev,
			private_info->wb_size[i],
			&private_info->wb_pa[i],
			GFP_KERNEL);
		if (private_info->wb_va[i] == NULL) {
			TCON_ERR("allocate working buffer fail");
			return HWTCON_STATUS_FB_ALLOC_FAIL;
		}
		memset(private_info->wb_va[i], 0xff, private_info->wb_size[i]);
	}

	/* allocate waveform buffer */
	private_info->waveform_size = WAVEFORM_SIZE;
	private_info->waveform_va = (char *)dma_alloc_coherent(&pdev->dev,
			private_info->waveform_size,
			&private_info->waveform_pa,
			GFP_KERNEL);
	if (private_info->waveform_va == NULL) {
		TCON_ERR("allocate waveform buffer fail");
		return HWTCON_STATUS_FB_ALLOC_FAIL;
	}

#ifdef HWTCON_WF_LUT_DEBUG
	if (sizeof(SLT_WF_MODE_ADDR) <= private_info->waveform_size)
		memcpy(private_info->waveform_va,
			SLT_WF_MODE_ADDR,
			sizeof(SLT_WF_MODE_ADDR));
	else
		hwtcon_debug_err_printf("wf_size too large\n");
#endif

	hwtcon_debug_err_printf("mdp buffer va: %p pa:0x%08x size:0x%x\n",
			private_info->mdp_buffer_va,
			private_info->mdp_buffer_pa,
			private_info->mdp_buffer_size);
	hwtcon_debug_err_printf("img buffer va: %p pa:0x%08x size:0x%x\n",
		private_info->fb_buffer_va,
		private_info->fb_buffer_pa,
		private_info->fb_buffer_size);
	hwtcon_debug_err_printf("wb[0] va:%p pa:0x%08x size:0x%x\n",
		private_info->wb_va[0],
		private_info->wb_pa[0],
		private_info->wb_size[0]);
	hwtcon_debug_err_printf("wb[1] va:%p pa:0x%08x size:0x%x\n",
		private_info->wb_va[1],
		private_info->wb_pa[1],
		private_info->wb_size[1]);
	hwtcon_debug_err_printf("waveform buffer va:%p pa:0x%08x size:0x%x\n",
		private_info->waveform_va,
		private_info->waveform_pa,
		private_info->waveform_size);

	memset(private_info->pseudo_palette, 0,
		sizeof(private_info->pseudo_palette));

	spin_lock_init(&private_info->lut_info_lock);
	memset(private_info->lut_info, 0, sizeof(private_info->lut_info));

	cmdqBackupAllocateSlot(&private_info->slot_auto_waveform, AUTO_WAVEFORM_TABLE_CNT);

	for (i = 0; i < AUTO_WAVEFORM_TABLE_CNT; i++)
		cmdqBackupWriteSlot(private_info->slot_auto_waveform, i, wf_table_partial[i / 5][i % 5]);

	cmdqBackupAllocateSlot(&private_info->slot_auto_waveform_info, SLOT_FB_MAX);

	private_info->temperature = TEMP_USE_AMBIENT;
	private_info->pipeline_busy = false;
	private_info->hwtcon_first_call = true;
	private_info->read_buffer_index = true;
	cmdqBackupAllocateSlot(&private_info->slot_read_buffer_index, 1);
	cmdqBackupWriteSlot(private_info->slot_read_buffer_index, 0, 1);

	wakeup_source_init(&private_info->wake_lock, "hwtcon_wakelock");

	INIT_DELAYED_WORK(&private_info->read_sensor_work,
		hwtcon_fb_read_temperature_from_sensor);

	schedule_delayed_work(&private_info->read_sensor_work,
			msecs_to_jiffies(0));

	private_info->mmsys_power_enable = false;
	mutex_init(&private_info->mmsys_power_enable_lock);
	private_info->hwtcon_clk_enable = false;
	spin_lock_init(&private_info->hwtcon_clk_enable_lock);

	spin_lock_init(&private_info->hwtcon_tcon_reg_lock);

	/* start timer for mmsys power down timeout. */
	setup_timer(&private_info->mmsys_power_timer,
		hwtcon_core_handle_mmsys_power_down_cb,
		0L);

	private_info->update_scheme = UPDATE_SCHEME_QUEUE;
	private_info->power_down_delay_ms = EINK_DEFAULT_POWER_DOWN_TIME;
	private_info->ignore_request = false;
	private_info->enable_night_mode = false;
	private_info->current_temp_zone = -1;
	private_info->current_night_mode = -1;

	/* init sw mitigation data */
	private_info->sw_algo.scan_lines = SCAN_LINE_TOTAL;
	private_info->sw_algo.count_thres = SCAN_LINE_THRESHOLD;
	private_info->sw_algo.pixel_thres = TRANSITION_THRESHOLD;
	private_info->sw_algo.str_thres = STRENGTH_THREASHOLD;
	private_info->sw_algo.scaled_width = 0;
	private_info->sw_algo.scaled_height = 0;
	private_info->sw_algo.scaled_factor = SCALED_FACTOR;

	/* init task list. */
	spin_lock_init(&private_info->free_task_list.lock);
	INIT_LIST_HEAD(&private_info->free_task_list.list);

	spin_lock_init(&private_info->wait_for_mdp_task_list.lock);
	INIT_LIST_HEAD(&private_info->wait_for_mdp_task_list.list);

	spin_lock_init(&private_info->mdp_done_task_list.lock);
	INIT_LIST_HEAD(&private_info->mdp_done_task_list.list);

	spin_lock_init(&private_info->pipeline_processing_task_list.lock);
	INIT_LIST_HEAD(&private_info->pipeline_processing_task_list.list);

	spin_lock_init(&private_info->pipeline_done_task_list.lock);
	INIT_LIST_HEAD(&private_info->pipeline_done_task_list.list);

	spin_lock_init(&private_info->collision_task_list.lock);
	INIT_LIST_HEAD(&private_info->collision_task_list.list);

	/* init wait queue */
	init_waitqueue_head(&private_info->power_state_change_wq);
	init_waitqueue_head(&private_info->wf_lut_release_wait_queue);
	init_waitqueue_head(&private_info->mdp_trigger_wait_queue);
	init_waitqueue_head(&private_info->pipeline_trigger_wait_queue);
	init_waitqueue_head(&private_info->task_state_wait_queue);
	init_waitqueue_head(&private_info->hwtcon_irq_clear_wait_queue);
	init_waitqueue_head(&private_info->hwtcon_fb_flush_done_wq);

	/* init blank */
	private_info->blank = FB_BLANK_UNBLANK;

	/* init update queue */
	mutex_init(&private_info->update_queue_mutex);
	mutex_init(&private_info->image_buffer_access_mutex);

	/* create work queue. */
	private_info->wq_disable_clk =
		create_singlethread_workqueue("disable hwtcon clock");
	private_info->wq_power_down_mmsys =
		create_singlethread_workqueue("power_down_mmsys_domain");
	private_info->wq_pipeline_written_done =
		create_singlethread_workqueue("handle_pieline_written_done");
	private_info->wq_wf_lut_display_done =
		create_singlethread_workqueue("handle_wf_lut_display_done");

	/* init work */
	INIT_WORK(&hwtcon_fb_info()->wk_power_down_mmsys,
		hwtcon_core_handle_mmsys_power_down);

	INIT_WORK(&hwtcon_fb_info()->wk_disable_clk,
		hwtcon_core_handle_clock_disable);

	return 0;
}

static int hwtcon_fb_release_private_fb_info(
	struct fb_private_info *private_info, struct platform_device *pdev)
{
	int i = 0;

	if (private_info->fb_buffer_va != NULL)
		dma_free_coherent(&pdev->dev, private_info->fb_buffer_size,
			private_info->fb_buffer_va,
			private_info->fb_buffer_pa);

	if (private_info->mdp_buffer_va != NULL)
		dma_free_coherent(&pdev->dev, private_info->mdp_buffer_size,
			private_info->mdp_buffer_va,
			private_info->mdp_buffer_pa);

	for (i = 0; i < ARRAY_SIZE(private_info->wb_pa); i++)
		if (private_info->wb_va[i] != NULL)
			dma_free_coherent(&pdev->dev,
				private_info->wb_size[i],
				private_info->wb_va[i],
				private_info->wb_pa[i]);

	if (private_info->waveform_va != NULL)
		dma_free_coherent(&pdev->dev,
			private_info->waveform_size,
			private_info->waveform_va,
			private_info->waveform_pa);
	cmdqBackupFreeSlot(private_info->slot_auto_waveform);
	cmdqBackupFreeSlot(private_info->slot_auto_waveform_info);
	cmdqBackupFreeSlot(private_info->slot_read_buffer_index);

	if (private_info->wq_disable_clk)
		destroy_workqueue(private_info->wq_disable_clk);

	if (private_info->wq_pipeline_written_done)
		destroy_workqueue(private_info->wq_pipeline_written_done);

	if (private_info->wq_power_down_mmsys)
		destroy_workqueue(private_info->wq_power_down_mmsys);

	if (private_info->wq_wf_lut_display_done)
		destroy_workqueue(private_info->wq_wf_lut_display_done);

	if (delayed_work_pending(&private_info->read_sensor_work))
		cancel_delayed_work(&private_info->read_sensor_work);

	return 0;
}

struct fb_private_info *hwtcon_fb_info(void)
{
	return (struct fb_private_info *)g_framebuffer_info->par;
}

u32 hwtcon_fb_get_rotation(void)
{
	return g_framebuffer_info->var.rotate;
}

void hwtcon_fb_get_resolution(u32 *width, u32 *height)
{
	if (width)
		*width = g_framebuffer_info->var.xres;
	if (height)
		*height = g_framebuffer_info->var.yres;
}

u32 hwtcon_fb_get_grayscale(void)
{
	return g_framebuffer_info->var.grayscale;
}

int hwtcon_fb_register_fb(struct platform_device *pdev)
{
	int status = 0;

	/* allocate fb_info & fb_private_info */
	g_framebuffer_info = framebuffer_alloc(sizeof(struct fb_private_info),
		&pdev->dev);
	if (g_framebuffer_info == NULL) {
		TCON_ERR("framebuffer_alloc fail");
		return HWTCON_STATUS_FB_STRUCT_ALLOC_FAIL;
	}

	status = hwtcon_fb_init_private_fb_info(
		(struct fb_private_info *)g_framebuffer_info->par, pdev);
	if (status != 0)
		return status;

	status = hwtcon_fb_init_fb_info(g_framebuffer_info);
	if (status != 0)
		return status;

	/* register frame buffer */
	status = register_framebuffer(g_framebuffer_info);
	if (status != 0) {
		TCON_ERR("register frame buffer fail:%d", status);
		return status;
	}

	status = hwtcon_fb_kthread_create();

	return status;
}

int hwtcon_fb_unregister_fb(struct platform_device *pdev)
{
	if (g_framebuffer_info == NULL)
		return 0;

	hwtcon_fb_kthread_destroy();

	/* unregister frame buffer */
	unregister_framebuffer(g_framebuffer_info);

	hwtcon_fb_release_private_fb_info(
		(struct fb_private_info *)g_framebuffer_info->par, pdev);

	/* release fb_info & fb_private_info */
	framebuffer_release(g_framebuffer_info);
	return 0;
}

