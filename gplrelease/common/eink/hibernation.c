/*
 * Copyright 2017-2020 Amazon Technologies, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

/* #define DEBUG */
#include <common.h>
#include <malloc.h>
#ifdef CONFIG_EINK_DISPLAY
#include <lcd.h>
#include <mapmem.h>
#endif
#ifdef UFBL_FEATURE_IDME
#include <idme.h>
#endif
#ifdef CONFIG_FRONTLIGHT
#include <frontlight.h>
#endif

#include "miscdata.h"
#include "hibernation.h"
#ifdef CONFIG_EINK_DISPLAY
#include "display.h"
#endif

static struct hibernation_info_t hibernation_info;
static void* file_buffer = NULL;

#define CONFIG_HIBERNATION_CONFIG_MAX 4096
#define CONFIG_HIBERNATION_STATUS_MAX 512

/* Enable hibernation according to fos_flags */
/* #define HIBERNATION_WITH_FOS_FLAGS */
static char* get_hibernate_property(char *buffer, const char *key)
{
	char* itr = strstr(buffer, key);
	if (itr && itr[strlen(key)]=='=') {
		itr += strlen(key) + 1;
		return itr;
	}
	return NULL;
}

struct hibernation_info_t* get_hibernation_info(void)
{
#ifdef HIBERNATION_WITH_FOS_FLAGS
	char fos_flags_str[9] = "";
	int value = 0;
#endif

	if (!hibernation_info.available) {
		file_buffer = memalign(ARCH_DMA_MINALIGN, CONFIG_HIBERNATION_CONFIG_MAX+CONFIG_HIBERNATION_STATUS_MAX);
		memset(file_buffer, 0, CONFIG_HIBERNATION_CONFIG_MAX+CONFIG_HIBERNATION_STATUS_MAX);
#ifdef CONFIG_MISCDATA_FS
		unsigned long load_size;

		load_size = CONFIG_HIBERNATION_CONFIG_MAX;
		if (load_misc_file("config", file_buffer, load_size)) {
			printf("ERROR: Failed to load config file\n");
			goto done;
		}
#else
		if (misc_block_read(file_buffer, 0, CONFIG_HIBERNATION_CONFIG_MAX/512) <=0) {
			printf("ERROR: Failed to load config file\n");
			goto done;
		}
#endif

		hibernation_info.type = simple_strtol(get_hibernate_property(file_buffer, "type"), NULL, 10);
		hibernation_info.force_splash = simple_strtol(get_hibernate_property(file_buffer, "force_splash"), NULL, 10);
		hibernation_info.splash_middle = simple_strtol(get_hibernate_property(file_buffer, "splash_middle"), NULL, 10);
		hibernation_info.working_buffer_addr = simple_strtol(get_hibernate_property(file_buffer, "working_buffer_addr"), NULL, 10);
		hibernation_info.waveform_addr = simple_strtol(get_hibernate_property(file_buffer, "waveform_addr"), NULL, 10);
#ifndef CONFIG_MISCDATA_FS
		hibernation_info.waveform_offset = simple_strtol(get_hibernate_property(file_buffer, "waveform_offset"), NULL, 10);
		hibernation_info.waveform_size = simple_strtol(get_hibernate_property(file_buffer, "waveform_size"), NULL, 10);
		hibernation_info.splash_offset = simple_strtol(get_hibernate_property(file_buffer, "splash_offset"), NULL, 10);
		hibernation_info.splash_size = simple_strtol(get_hibernate_property(file_buffer, "splash_size"), NULL, 10);
		hibernation_info.working_buffer_offset = simple_strtol(get_hibernate_property(file_buffer, "working_buffer_offset"), NULL, 10);
		hibernation_info.working_buffer_size = simple_strtol(get_hibernate_property(file_buffer, "working_buffer_size"), NULL, 10);
#endif

#ifdef CONFIG_IN_HOUSE_HIBERNATION
		hibernation_info.setting = H_TYPE_TUXONICE;
#else
		hibernation_info.setting = H_TYPE_QUICKBOOT;
#endif

#ifdef HIBERNATION_WITH_FOS_FLAGS
#ifdef UFBL_FEATURE_IDME
		idme_get_var_external("fos_flags", fos_flags_str, sizeof(fos_flags_str));
#endif
		value = simple_strtol(fos_flags_str, NULL, 10) & BIT(0);
		if (!value)
			hibernation_info.setting = H_TYPE_NONE;
#endif

		hibernation_info.available = true;

		debug("hibernation_info\n");
		debug(" setting=%d\n",                hibernation_info.setting);
		debug(" type=%d\n",                   hibernation_info.type);
		debug(" force_splash=%d\n",           hibernation_info.force_splash);
		debug(" splash_middle=%d\n",          hibernation_info.splash_middle);
		debug(" working_buffer_addr=%lu\n",   hibernation_info.working_buffer_addr);
		debug(" waveform_addr=%lu\n",         hibernation_info.waveform_addr);
#ifndef CONFIG_MISCDATA_FS
		debug(" waveform_offset=%lu\n",       hibernation_info.waveform_offset);
		debug(" waveform_size=%lu\n",         hibernation_info.waveform_size);
		debug(" splash_offset=%lu\n",         hibernation_info.splash_offset);
		debug(" splash_size=%lu\n",           hibernation_info.splash_size);
		debug(" working_buffer_offset=%lu\n", hibernation_info.working_buffer_offset);
		debug(" working_buffer_size=%lu\n",   hibernation_info.working_buffer_size);
#endif
	}

done:
	return &hibernation_info;
}

static int is_hibernation(void)
{
	struct hibernation_info_t *info = get_hibernation_info();
	return (info->type == info->setting);
}

static int clear_hibernation(int resume_reason, const char* msg)
{
	int offset = CONFIG_HIBERNATION_CONFIG_MAX;
	int limit = CONFIG_HIBERNATION_STATUS_MAX - 4;
	struct hibernation_info_t* info = get_hibernation_info();

	if (file_buffer) {
		/* resume reason */
		offset += snprintf(file_buffer + offset, limit - offset, "resume_reason=0x%X\n", resume_reason);
		offset += snprintf(file_buffer + offset, limit - offset, "working_buffer_addr=%lu\n",   info->working_buffer_addr);
		offset += snprintf(file_buffer + offset, limit - offset, "waveform_addr=%lu\n",         info->waveform_addr);
		if (msg) {
			offset += snprintf(file_buffer + offset, limit - offset, "%s\n", msg);
		}
		offset += snprintf(file_buffer + offset, limit - offset, "\n\n\n\n");

		/* erase miscdata partition */
		memset(file_buffer, 0, CONFIG_HIBERNATION_CONFIG_MAX);
		if (misc_block_write(file_buffer, 0, (CONFIG_HIBERNATION_CONFIG_MAX+CONFIG_HIBERNATION_STATUS_MAX)/512) <=0) {
			printf("ERROR: Failed to erase config file\n\n");
		}
		free(file_buffer);
	}
	file_buffer = NULL;
	return 0;
}

static int init_hibernation(void)
{
#ifdef CONFIG_HIBERNATION_FALCON
	int *bios_addr = (int*)KLOWMEM_TO_PHYS(CONFIG_FALCON_BIOS_ADDR+0x30);
	*bios_addr = 0;
	if (get_hibernation_info()->setting == H_TYPE_QUICKBOOT) {
		return quickboot_load("quickboot");
	}
#endif
#ifdef CONFIG_TOI
	if (get_hibernation_info()->setting == H_TYPE_TUXONICE) {
		return 0;
	}
#endif
	return 0;
}

static int post_prepare_hibernation(void)
{
#ifdef CONFIG_HIBERNATION_FALCON
	if (get_hibernation_info()->setting == H_TYPE_QUICKBOOT) {
		return quickboot_init();
	}
#endif
#ifdef CONFIG_TOI
	if (get_hibernation_info()->setting == H_TYPE_TUXONICE) {
		return 0;
	}
#endif
	return 0;
}

static int is_hibernation_image_valid(void)
{
#ifdef CONFIG_HIBERNATION_FALCON
	if (get_hibernation_info()->setting == H_TYPE_QUICKBOOT) {
		return !quickboot_checkimg();
	}
#endif
#ifdef CONFIG_TOI
	if (get_hibernation_info()->setting == H_TYPE_TUXONICE) {
		return is_toi_image_exist();
	}
#endif
	return 0;
}

static int do_hibernation_resume(void)
{
#ifdef CONFIG_HIBERNATION_FALCON
	if (get_hibernation_info()->setting == H_TYPE_QUICKBOOT) {
		quickboot_resume();
	}
#endif
#ifdef CONFIG_TOI
	if (get_hibernation_info()->setting == H_TYPE_TUXONICE) {
		do_toi_resume();
	}
#endif
	return 0;
}

__weak int board_is_abnormal_reset(void)
{
	return 0;
}

__weak int board_is_rtc_wakeup(void)
{
	return 0;
}

__weak int board_is_usb_wakeup(void)
{
	return 0;
}

__weak int board_is_hall_wakeup(void)
{
	return 0;
}

__weak int board_is_usb_connected(void)
{
	return 0;
}

__weak int board_is_hall_closed(void)
{
	return 0;
}

int hibernation_resume(void)
{
#ifdef CONFIG_EINK_DISPLAY
	int splash_x, splash_y, splash_width, splash_height;
	int splash_available = 0;
#endif
	int need_splash = false;
	int is_rtc_wakeup, is_usb_wakeup, is_hall_wakeup;
	int is_usb_connected, is_hall_closed, force_splash;
	int resume_reason = 0;
	struct hibernation_info_t* info = get_hibernation_info();

	if (init_hibernation()) {
		printf("ERROR: Failed to init hibernation\n");
		return 1;
	}

	/* Do not continue if not from hibernation */
	if (!is_hibernation()) {
		printf("hibernation: Not from hibernation\n");
		goto normal_boot;
	}

	/* check abnormal reset */
	if (board_is_abnormal_reset()) {
		printf("hibernation: Abnormal shutdown, aborting hibernation\n");
		clear_hibernation(~0, "abnormal_reset");
		goto normal_boot;
	}

	is_rtc_wakeup = board_is_rtc_wakeup();
	is_usb_wakeup = board_is_usb_wakeup();
	is_hall_wakeup = board_is_hall_wakeup();
	is_hall_closed = board_is_hall_closed();
	is_usb_connected = board_is_usb_connected();
	force_splash = info->force_splash;

	resume_reason |= !!is_rtc_wakeup	<< 0;
	resume_reason |= !!is_usb_connected	<< 1;
	resume_reason |= !!is_hall_closed	<< 2;
	resume_reason |= !!is_usb_wakeup	<< 5;

	need_splash = !resume_reason || force_splash;

	/* Only need for debugging */
	resume_reason |= !!is_hall_wakeup	<< 4;
	resume_reason |= force_splash		<< 7;

	/* prepare display */
#ifdef CONFIG_EINK_DISPLAY
	if (eink_setup_buffer()) {
		printf("hibernation: failed to setup eink buffer, try again.\n");
		if (eink_setup_buffer()) {
			printf("hibernation: still failed to setup eink buffer, aborting hibernation.\n");
			clear_hibernation(resume_reason, "eink buffer setup error");
			goto normal_boot;
		}
	}
#endif
	if (need_splash) {
#ifdef CONFIG_FRONTLIGHT
		frontlight_enable(true, true);
#endif
#ifdef CONFIG_EINK_DISPLAY
		if (!eink_load_splash(info->splash_middle, &splash_x, &splash_y, &splash_width, &splash_height)) {
			splash_available = 1;
			eink_setup_hw();
		}
#endif
	}

	/* Save some information in misc partition */
	debug("resume reason: 0x%X\n", resume_reason);
	clear_hibernation(resume_reason, NULL);

	if (post_prepare_hibernation()) {
		printf("hibernation: post_prepare failed, goto normal boot\n");
		goto post_prepare;
	}

	/* check valid snapshot image */
	if (!is_hibernation_image_valid()) {
		printf("hibernation: No hibernation image found\n");
		goto post_prepare;
	}

	/* Check if splash needed */
	if (need_splash) {
		// only show splash image if hall is open and not rtc or usb wakeup
#ifdef CONFIG_EINK_DISPLAY
		if (splash_available) {
			puts("ss\n");
			eink_update_screen(splash_x, splash_y, splash_width, splash_height, false);
			puts("ds\n");
		}
#endif
	} else {
		puts("ns\n");
	}

	do_hibernation_resume();
	/* should not be here */

	printf("ERROR: do_hibernation_resume returns.\n");

normal_boot:
	post_prepare_hibernation();

post_prepare:
	return 0;
}
