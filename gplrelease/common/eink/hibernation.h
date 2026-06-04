/*
 * Copyright 2017-2020 Amazon Technologies, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __HIBERNATION_H__
#define __HIBERNATION_H__

#include <common.h>

enum hibernation_type {
	H_TYPE_QUICKBOOT = 1,
	H_TYPE_TUXONICE,
	H_TYPE_NONE,
	H_TYPE_MAX
};

struct hibernation_info_t {
	bool	available;
	int		setting;
	int		type;
	int		force_splash;
	int             splash_middle;
	unsigned long	working_buffer_addr;
	unsigned long	waveform_addr;
#ifndef CONFIG_MISCDATA_FS
	unsigned long	waveform_offset;
	unsigned long	waveform_size;
	unsigned long	splash_offset;
	unsigned long	splash_size;
	unsigned long	working_buffer_offset;
	unsigned long	working_buffer_size;
#endif
};

struct hibernation_info_t* get_hibernation_info(void);
int hibernation_resume(void);

#ifdef CONFIG_HIBERNATION_FALCON
int quickboot_load(const char* partition);
int quickboot_init(void);
int quickboot_checkimg(void);
int quickboot_resume(void);
#endif

#ifdef CONFIG_TOI
int is_toi_image_exist(void);
void do_toi_resume(void);
#endif

#endif
