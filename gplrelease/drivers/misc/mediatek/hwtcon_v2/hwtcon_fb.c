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
#include "hwtcon_lightbox.h"
#include "fiti_core.h"
#include "hwtcon_wf_lut_config.h"

#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/pm_qos.h>
#include <linux/device.h>
#include "mtk_mdp_path.h"
#include "mtk_vcu.h"

#include "hwtcon_epd.h"
#include "hwtcon_driver.h"

extern int bd71828_get_batt_temperature(void);

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
		TCON_LOG("fb_var info changed");
		/* wait all task release */
		hwtcon_fb_flush_update();
		if(var->grayscale != info->var.grayscale) { /*reset marker store if greyscale changed*/
			TCON_LOG("grayscale %d -> %d",  info->var.grayscale, var->grayscale);
		}

		TCON_LOG("fb_var info changed done");
	}

	return 0;
}

void hwtcon_fb_flush_update(void)
{
	mutex_lock(&hwtcon_fb_info()->update_queue_mutex);

	/* hwtcon busy,need to wait it finish update*/
	hwtcon_core_wait_all_task_done();

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
		/* Only flush the pending updates in current HWTCON list */
		hwtcon_fb_flush_update();
		break;
    case FB_BLANK_POWERDOWN:
		hwtcon_fb_flush_update();
		if (hwtcon_fb_info()->mmsys_power_enable == true &&
			hwtcon_fb_info()->power_down_delay_ms == EINK_DEFAULT_POWER_DOWN_TIME)
		{
			/* no power off mode, force to power off here*/
			hwtcon_driver_enable_mmsys_power(false);
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
	bool full_update = (update_data->update_mode == UPDATE_MODE_FULL);

	if (hwtcon_core_use_night_mode()) {
		if (wf_mode == WAVEFORM_MODE_GC16)
			wf_mode = WAVEFORM_MODE_GCK16;
		if (wf_mode == WAVEFORM_MODE_GLR16)
			wf_mode = WAVEFORM_MODE_GLKW16;

		if ((wf_mode == WAVEFORM_MODE_DU) &&
			(wf_lut_get_wf_info()->mode_version == WF_MODE_VERSION_TL))
			wf_mode = WAVEFORM_MODE_DUNM;
	}

	/* update partial update mode */
	if (wf_mode == WAVEFORM_MODE_GC16 && !full_update)
		wf_mode = WAVEFORM_MODE_GC16_PARTIAL;
	if (wf_mode == WAVEFORM_MODE_GCK16 && !full_update)
		wf_mode = WAVEFORM_MODE_GCK16_PARTIAL;

	switch (wf_mode) {
	case WAVEFORM_MODE_INIT:
	case WAVEFORM_MODE_DU:
	case WAVEFORM_MODE_GC16:
	case WAVEFORM_MODE_GL16:
	case WAVEFORM_MODE_GLR16:
	case WAVEFORM_MODE_GLD16:
	case WAVEFORM_MODE_A2:
	case WAVEFORM_MODE_DU4:
	case WAVEFORM_MODE_GCK16:
	case WAVEFORM_MODE_GLKW16:
	case WAVEFORM_MODE_GC16_PARTIAL:
	case WAVEFORM_MODE_GCK16_PARTIAL:
	case WAVEFORM_MODE_DUNM:
	case WAVEFORM_MODE_P2SW:
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

int hwtcon_fb_check_update_data_invalid(
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

	if (update_data->flags & EPDC_FLAG_ENABLE_SWIPE) {
		if (wf_lut_get_wf_info()->mode_version != WF_MODE_VERSION_TL) {
			TCON_LOG("wavefrom file not support swipe");
			update_data->flags &= ~EPDC_FLAG_ENABLE_SWIPE;
		}

		if ((update_data->swipe_data.direction >= SWIPE_MAX) ||
			(update_data->swipe_data.steps == 0) ||
			(update_data->swipe_data.steps > MAX_SWIPE_COUNT)) {
			TCON_ERR("invalid swipe setting: flags[0x%08x] dir[%d] steps[%d]",
				update_data->flags,
				update_data->swipe_data.direction,
				update_data->swipe_data.steps);
			return HWTCON_STATUS_INVALID_PARAM;
		}
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

static int hwtcon_fb_ioctl_wait_for_task_triggered(struct fb_info *info, void *arg)
{
	u32 update_marker = 0;
	int ret;

	if (copy_from_user(&update_marker, arg, sizeof(u32)) != 0) {
		TCON_ERR("copy_from_user fail");
		return HWTCON_STATUS_COPY_FROM_USER_FAIL;
	}
	 /*unlock to allow other concurrent IOCTLS */
	unlock_fb_info(info); /*it is locked before this function is called, see fbmem.c function do_fb_ioctl case default. */
	ret =  hwtcon_core_wait_for_task_triggered(update_marker);
	lock_fb_info(info); /*re-lock it before exit, it will unlock in do_fb_ioctl case default */
	return ret;
}

static int hwtcon_fb_ioctl_wait_for_task_displayed(struct fb_info *info, void *arg)
{
	u32 update_marker = 0;
	int ret;

	if (copy_from_user(&update_marker, arg, sizeof(u32)) != 0) {
		TCON_ERR("copy_from_user fail");
		return HWTCON_STATUS_COPY_FROM_USER_FAIL;
	}
	 /*unlock to allow other concurrent IOCTLS */
	unlock_fb_info(info);  /*it is locked before this function is called, see fbmem.c function do_fb_ioctl case default. */
	ret = hwtcon_core_wait_for_task_displayed(update_marker);
	lock_fb_info(info); /*re-lock it before exit, it will unlock in do_fb_ioctl case default */
	return ret;
}

static int hwtcon_fb_ioctl_get_working_buffer(void *arg)
{
	if (copy_to_user(arg, hwtcon_fb_info()->wb_buffer_va,
		hwtcon_fb_info()->wb_buffer_size) != 0) {
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
#ifndef FPGA_EARLY_PORTING
	/* TODO */
	info.vcom_value = fiti_read_vcom();
#endif
	snprintf(info.wf_file_name,	sizeof(info.wf_file_name), "%s",
		wf_lut_get_wf_info()->wf_file_name);

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
	case MXCFB_SET_HALFTONE:
		return hwtcon_lightbox_ioctl_set_lightbox_ctrl((void *)arg);
	case MXCFB_SET_UPDATE_SCHEME:
		return hwtcon_fb_ioctl_set_update_scheme((void *)arg);
	case MXCFB_WAIT_FOR_UPDATE_SUBMISSION:
		return hwtcon_fb_ioctl_wait_for_task_triggered(info, (void *)arg);
	case MXCFB_WAIT_FOR_UPDATE_COMPLETE:
		return hwtcon_fb_ioctl_wait_for_task_displayed(info, (void *)arg);
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
	case MXCFB_WAIT_FOR_ANY_UPDATE_COMPLETE:
		return hwtcon_fb_ioctl_wait_for_any_update_complete(info, arg);
	case MXCFB_SET_UPDATE_FLAGS:
		return hwtcon_fb_ioctl_set_update_flags(info, arg);
	 case MXCFB_GET_UPDATE_FLAGS:
		return hwtcon_fb_ioctl_get_update_flags(info, arg);
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
		((struct fb_private_info *)(info->par))->fb_buffer_va,
		((struct fb_private_info *)(info->par))->fb_buffer_pa,
		((struct fb_private_info *)(info->par))->fb_buffer_size,
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
	/* color map */
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
	unsigned int interval = SENSOR_READ_SHORT_INTERVAL_MS;
	int temperature;
	bool valid = false;
	static int count = 0;

	if (!fiti_pmic_judge_power_on_going()) {
		/* Do not use fiti when fiti is in POWER_ON_GOING state */
		temperature = fiti_read_temperature();
		TCON_LOG("read fiti temperature[%d]", temperature);

		if (temperature == 0) {
			count++;
			if (count >= MAX_TEMP_READ) {
				TCON_ERR("sensor temperature is 0 in %d consecutive reads!", count);
				/* read the backup temp sensor and take the average of that temperature and
				the previous FITI temperature*/
				temperature = bd71828_get_batt_temperature();
				TCON_LOG("read battery temperature[%d]", temperature);
				if (temperature != -255) {
					hwtcon_fb_info()->temperature = (temperature + hwtcon_fb_info()->temperature) / 2;
					valid = true;
				}
				else {
					TCON_ERR("error in reading battery temperature!");
				}
			}
		}
		else {
			valid = true;
		}

		if (valid) {
			TCON_LOG("read sensor temperature[%d]", temperature);
			count = 0;
			interval = SENSOR_READ_INTERVAL_MS;
		}
		else {
			TCON_ERR("fail to read sensor temperature");
		}
	}
	schedule_delayed_work(&hwtcon_fb_info()->read_sensor_work, msecs_to_jiffies(interval));
}

static int hwtcon_fb_init_fb_info(struct fb_info *info)
{
	info->fbops = &hwtcon_fb_ops;
	info->flags = FBINFO_FLAG_DEFAULT;
	info->screen_buffer = ((struct fb_private_info *)
		(info->par))->fb_buffer_va;
	info->screen_size = ((struct fb_private_info *)
		(info->par))->fb_buffer_size;
	info->pseudo_palette = ((struct fb_private_info *)
		(info->par))->pseudo_palette;

	/* fb_fix_screeninfo */
	strncpy(info->fix.id, HWTCON_DRIVER_NAME, sizeof(info->fix.id));
	info->fix.smem_start = ((struct fb_private_info *)
		(info->par))->fb_buffer_pa;
	info->fix.smem_len = ((struct fb_private_info *)
		(info->par))->fb_buffer_size;
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

int hwtcon_fb_alloc_debug_img_buffer(struct fb_private_info *private_info)
{
	int i = 0;
	int buffer_size = hw_tcon_get_edp_width() * hw_tcon_get_edp_height();

	if (private_info->debug_img_buffer_available)
		return HWTCON_STATUS_OK;

	TCON_WARN("Enabling debug img buffer, %d MB additional memory will be used.",
			(MAX_DEBUG_IMAGE_BUFFER_COUNT * buffer_size) >> 20);
	for (i = 0; i < MAX_DEBUG_IMAGE_BUFFER_COUNT; i++) {
		private_info->debug_img_buffer_size[i] = buffer_size;
		private_info->debug_img_buffer_va[i] = (char *)dma_alloc_coherent(private_info->dev,
				private_info->debug_img_buffer_size[i],
				&private_info->debug_img_buffer_pa[i],
				GFP_KERNEL);
		if (private_info->debug_img_buffer_va[i] == NULL) {
			TCON_ERR("allocate debug image buffer fail w:%d h:%d size:%d",
					hw_tcon_get_edp_width(),
					hw_tcon_get_edp_height(),
					private_info->debug_img_buffer_size[i]);
			return HWTCON_STATUS_FB_ALLOC_FAIL;
		}
		private_info->debug_img_buffer_name[i] = kzalloc(MAX_FILE_NAME_LEN, GFP_KERNEL);
		if (private_info->debug_img_buffer_name[i] == NULL) {
			TCON_ERR("allocate buffer name fail");
			return HWTCON_STATUS_FB_ALLOC_FAIL;
		}
	}
	private_info->debug_img_buffer_available = true;
	return HWTCON_STATUS_OK;
}

static int hwtcon_fb_init_private_fb_info(
	struct fb_private_info *private_info, struct platform_device *pdev)
{
	unsigned long flags;
	int i = 0;

	private_info->regulator_vcore = regulator_get(&pdev->dev, "vcore");
	if (private_info->regulator_vcore == NULL) {
		TCON_ERR("get vcore regulator fail");
	}

	private_info->vcore_req = kzalloc(sizeof(struct pm_qos_request), GFP_KERNEL);
	/* request vcore voltage 0.65V */
	pm_qos_add_request(private_info->vcore_req,
		PM_QOS_VCORE_OPP,
		PM_QOS_VCORE_OPP_DEFAULT_VALUE);

	private_info->dev = &pdev->dev;
	/* set private_info as dev->data */
	dev_set_drvdata(&pdev->dev, private_info);

	/* allocate frame buffer */
	private_info->fb_buffer_size = max((ALIGN(hw_tcon_get_edp_width(), FB_W_ALIGN) * hw_tcon_get_edp_height()),
		(hw_tcon_get_edp_width() * ALIGN(hw_tcon_get_edp_height(), FB_W_ALIGN))) * FB_VIRTUAL_FRAME_COUNT;
	private_info->fb_buffer_va = (char *)dma_alloc_coherent(&pdev->dev,
		private_info->fb_buffer_size,
		&private_info->fb_buffer_pa,
		GFP_KERNEL);
	if (private_info->fb_buffer_va == NULL) {
		TCON_ERR("allocate fb buffer fail w:%d h:%d size:%d",
			hw_tcon_get_edp_width(),
			hw_tcon_get_edp_height(),
			private_info->fb_buffer_size);
		return HWTCON_STATUS_FB_ALLOC_FAIL;
	}

	/* allocate temp buffer */
	private_info->tmp_buffer_size = private_info->fb_buffer_size;
	private_info->tmp_buffer_va = (char *)dma_alloc_coherent(&pdev->dev,
		private_info->tmp_buffer_size,
		&private_info->tmp_buffer_pa,
		GFP_KERNEL);
	if (private_info->tmp_buffer_va == NULL) {
                TCON_ERR("allocate tmp buffer fail");
                return HWTCON_STATUS_FB_ALLOC_FAIL;
        }

	/* allocate image buffer */
	private_info->img_buffer_size =
		hw_tcon_get_edp_width() * hw_tcon_get_edp_height();
	private_info->img_buffer_va = (char *)dma_alloc_coherent(&pdev->dev,
		private_info->img_buffer_size,
		&private_info->img_buffer_pa,
		GFP_KERNEL);
	if (private_info->img_buffer_va == NULL) {
		TCON_ERR("allocate image buffer fail w:%d h:%d size:%d",
			hw_tcon_get_edp_width(),
			hw_tcon_get_edp_height(),
			private_info->img_buffer_size);
		return HWTCON_STATUS_FB_ALLOC_FAIL;
	}

	/* allocate regal buffer */
	private_info->temp_img_buffer_size =
		hw_tcon_get_edp_width() * hw_tcon_get_edp_height();
	private_info->temp_img_buffer_va = (char *)dma_alloc_coherent(&pdev->dev,
		private_info->temp_img_buffer_size,
		&private_info->temp_img_buffer_pa,
		GFP_KERNEL);
	if (private_info->temp_img_buffer_va == NULL) {
		TCON_ERR("allocate temp image buffer fail w:%d h:%d size:%d",
			hw_tcon_get_edp_width(),
			hw_tcon_get_edp_height(),
			private_info->temp_img_buffer_size);
		return HWTCON_STATUS_FB_ALLOC_FAIL;
	}

	/* allocate debug image buffer info */
	private_info->debug_img_buffer_counter = 0;
	private_info->debug_img_buffer_available = false;
	if (hwtcon_device_info()->param_enable_debug_img_buffer) {
		if (hwtcon_fb_alloc_debug_img_buffer(private_info)) {
			return HWTCON_STATUS_FB_ALLOC_FAIL;
		}
	}


	if (hwtcon_device_info()->reserved_buf_ready) {
		/* use reserved buffer */
		private_info->waveform_va = hwtcon_device_info()->reserved_buf_va;
		private_info->waveform_pa = hwtcon_device_info()->reserved_buf_mva;
		private_info->waveform_size = WAVEFORM_SIZE;

		private_info->wb_buffer_va = hwtcon_device_info()->reserved_buf_va + WAVEFORM_SIZE;
		private_info->wb_buffer_pa = hwtcon_device_info()->reserved_buf_mva + WAVEFORM_SIZE;
		private_info->wb_buffer_size =
			hw_tcon_get_edp_width() *
			hw_tcon_get_edp_height() * 2;
	} else {
		/* use dma alloc buffer */
		/* allocate working buffer */
		private_info->wb_buffer_size =
			hw_tcon_get_edp_width() *
			hw_tcon_get_edp_height() * 2;
		private_info->wb_buffer_va = (char *)dma_alloc_coherent(&pdev->dev,
			private_info->wb_buffer_size,
			&private_info->wb_buffer_pa,
			GFP_KERNEL);
		if (private_info->wb_buffer_va == NULL) {
			TCON_ERR("allocate working buffer fail w:%d h:%d size:%d",
				hw_tcon_get_edp_width(),
				hw_tcon_get_edp_height(),
				private_info->wb_buffer_size);
			return HWTCON_STATUS_FB_ALLOC_FAIL;
		}


		/* allocate waveform buffer */
		private_info->waveform_size = WAVEFORM_SIZE;
		private_info->waveform_va = (char *)dma_alloc_coherent(&pdev->dev,
				private_info->waveform_size,
				&private_info->waveform_pa,
				GFP_KERNEL);
		if (private_info->waveform_va == NULL) {
			TCON_ERR("allocate waveform buffer fail size:%d",
				private_info->waveform_size);
			return HWTCON_STATUS_FB_ALLOC_FAIL;
		}
	}

	hwtcon_debug_err_printf("fb buffer va: %p pa:0x%08x size:0x%x\n",
			private_info->fb_buffer_va,
			private_info->fb_buffer_pa,
			private_info->fb_buffer_size);
	hwtcon_debug_err_printf("img buffer va: %p pa:0x%08x size:0x%x\n",
		private_info->img_buffer_va,
		private_info->img_buffer_pa,
		private_info->img_buffer_size);
	hwtcon_debug_err_printf("temp img buffer va: %p pa:0x%08x size:0x%x\n",
		private_info->temp_img_buffer_va,
		private_info->temp_img_buffer_pa,
		private_info->temp_img_buffer_size);
	hwtcon_debug_err_printf("wb va:%p pa:0x%08x size:0x%x\n",
		private_info->wb_buffer_va,
		private_info->wb_buffer_pa,
		private_info->wb_buffer_size);
	hwtcon_debug_err_printf("waveform buffer va:%p pa:0x%08x size:0x%x\n",
		private_info->waveform_va,
		private_info->waveform_pa,
		private_info->waveform_size);

	memset(private_info->pseudo_palette, 0,
		sizeof(private_info->pseudo_palette));

	private_info->temperature = EINK_DEFAULT_TEMPERATURE;
	private_info->pipeline_busy = false;
	private_info->hwtcon_first_call = true;

	INIT_DELAYED_WORK(&private_info->read_sensor_work,
		hwtcon_fb_read_temperature_from_sensor);

	schedule_delayed_work(&private_info->read_sensor_work,
			msecs_to_jiffies(0));

	for (i = 0; i < MAX_LUT_REGION_COUNT; i++) {
		hwtcon_fb_info()->lut_pic_order[i] = MIN_PIC_ORDER;
	}
	spin_lock_init(&private_info->lut_pic_order_lock);

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
	/* init timer for LUT release */
	for (i = 0; i < MAX_LUT_REGION_COUNT; i++)
		setup_timer(&private_info->timer_lut_release[i],
			hwtcon_core_handle_lut_release_timeout_cb,
			(unsigned long)i);


	/* allocate dma buffer for lut_release_slot */
	if (cmdqBackupAllocateSlot(&private_info->lut_release_slot, LUT_RELEASE_MAX) != 0) {
		TCON_ERR("allocate lut_release_slot fail");
	}
	for (i = 0; i < LUT_RELEASE_MAX; i++)
		cmdqBackupWriteSlot(private_info->lut_release_slot, i, 0);

	/* init sw mitigation data */
	private_info->sw_algo.scan_lines = SCAN_LINE_TOTAL;
	private_info->sw_algo.count_thres = SCAN_LINE_THRESHOLD;
	private_info->sw_algo.pixel_thres = TRANSITION_THRESHOLD;
	private_info->sw_algo.str_thres = STRENGTH_THREASHOLD;
	private_info->sw_algo.scaled_width = 0;
	private_info->sw_algo.scaled_height = 0;
	private_info->sw_algo.scaled_factor = SCALED_FACTOR;

	/*init mark_store for MXCFB_WAIT_FOR_ANY_UPDATE_COMPLETE support*/
	marker_store_init(&private_info->marker_store);
	private_info->update_flag_fast_mode = 0;

	init_completion(&private_info->wb_frame_done_completion);

	spin_lock_init(&private_info->lut_free_lock);
	spin_lock_init(&private_info->lut_active_lock);
	spin_lock_init(&private_info->g_update_order_lock);

	wakeup_source_init(&private_info->wake_lock, "hwtcon_wakelock");

	spin_lock_irqsave(&private_info->g_update_order_lock, flags);
	private_info->g_update_order = 0;
	private_info->g_update_cnt = 0;
	spin_unlock_irqrestore(&private_info->g_update_order_lock, flags);

	spin_lock_irqsave(&private_info->lut_free_lock, flags);
	private_info->lut_free = LUT_BIT_ALL_SET;
	spin_unlock_irqrestore(&private_info->lut_free_lock, flags);

	spin_lock_irqsave(&private_info->lut_active_lock, flags);
	private_info->lut_active = 0LL;
	spin_unlock_irqrestore(&private_info->lut_active_lock, flags);

	/* init task list. */
	spin_lock_init(&private_info->fb_global_marker_list.lock);
	INIT_LIST_HEAD(&private_info->fb_global_marker_list.list);

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
	init_waitqueue_head(&private_info->power_state_change_wait_queue);
	init_waitqueue_head(&private_info->wf_lut_release_wait_queue);
	init_waitqueue_head(&private_info->mdp_trigger_wait_queue);
	init_waitqueue_head(&private_info->pipeline_trigger_wait_queue);
	init_waitqueue_head(&private_info->task_state_wait_queue);

	/* init blank */
	private_info->blank = FB_BLANK_UNBLANK;

	/* init update queue */
	mutex_init(&private_info->update_queue_mutex);
	mutex_init(&private_info->image_buffer_access_mutex);

	/* create work queue. */
	private_info->wq_power_down_mmsys =
		create_singlethread_workqueue("power_down_mmsys_domain");
	private_info->wq_pipeline_written_done =
		create_singlethread_workqueue("handle_pieline_written_done");
	private_info->wq_wf_lut_display_done =
		create_singlethread_workqueue("handle_wf_lut_display_done");

	/* init work */
	INIT_WORK(&hwtcon_fb_info()->wk_power_down_mmsys,
		hwtcon_core_handle_mmsys_power_down);

	return 0;
}

static int hwtcon_fb_release_private_fb_info(
	struct fb_private_info *private_info, struct platform_device *pdev)
{
	int i = 0;

	if (private_info->vcore_req) {
		pm_qos_remove_request(private_info->vcore_req);
		kfree(private_info->vcore_req);
		private_info->vcore_req = NULL;
	}

	if (private_info->regulator_vcore) {
		regulator_put(private_info->regulator_vcore);
		private_info->regulator_vcore = NULL;
	}

	if (private_info->fb_buffer_va != NULL)
		dma_free_coherent(&pdev->dev, private_info->fb_buffer_size,
			private_info->fb_buffer_va,
			private_info->fb_buffer_pa);

	if (private_info->img_buffer_va != NULL)
		dma_free_coherent(&pdev->dev, private_info->img_buffer_size,
			private_info->img_buffer_va,
			private_info->img_buffer_pa);

	if (private_info->temp_img_buffer_va != NULL)
		dma_free_coherent(&pdev->dev, private_info->temp_img_buffer_size,
			private_info->temp_img_buffer_va,
			private_info->temp_img_buffer_pa);

	/* release debug image buffer info */
	if (private_info->debug_img_buffer_available) {
		for (i = 0; i < MAX_DEBUG_IMAGE_BUFFER_COUNT; i++) {
			if (private_info->debug_img_buffer_va[i] != NULL)
				dma_free_coherent(&pdev->dev, private_info->debug_img_buffer_size[i],
						private_info->debug_img_buffer_va[i],
						private_info->debug_img_buffer_pa[i]);

			if (private_info->debug_img_buffer_name[i] != NULL)
				kfree(private_info->debug_img_buffer_name[i]);
		}
		private_info->debug_img_buffer_available = false;
	}

	if (!hwtcon_device_info()->reserved_buf_ready) {
		if (private_info->wb_buffer_va != NULL)
			dma_free_coherent(&pdev->dev,
				private_info->wb_buffer_size,
				private_info->wb_buffer_va,
				private_info->wb_buffer_pa);

		if (private_info->waveform_va != NULL)
			dma_free_coherent(&pdev->dev,
				private_info->waveform_size,
				private_info->waveform_va,
				private_info->waveform_pa);
	}

	for (i = 0; i < MAX_LUT_REGION_COUNT; i++)
		del_timer(&hwtcon_fb_info()->timer_lut_release[i]);

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
