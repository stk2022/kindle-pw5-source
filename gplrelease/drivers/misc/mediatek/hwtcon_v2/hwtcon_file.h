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

#ifndef __HWTCON_FILE_H__
#define __HWTCON_FILE_H__

#include <linux/fs.h>
#include <linux/file.h>
#include <linux/uaccess.h>
#include <linux/types.h>

enum FS_OPEN_MODE_ENUM {
	FS_MODE_READ_ONLY = O_RDONLY,
	FS_MODE_RW = O_RDWR | O_CREAT | O_TRUNC,
	FS_MODE_RW_APPEND = O_RDWR | O_CREAT | O_APPEND,
};

struct fs_struct {
	mm_segment_t fs;
	struct file *fp;
	int (*fs_create)(struct fs_struct *file, const char *fileName,
		enum FS_OPEN_MODE_ENUM open_mode);
	int (*fs_read)(struct fs_struct *file, char *buffer, u32 size);
	void (*fs_write)(struct fs_struct *file, char *buffer, u32 size);
	int (*fs_get_size)(struct fs_struct *file, const char *fileName);
	void (*fs_close)(struct fs_struct *file);
};

void init_fs_struct(struct fs_struct *file);
int hwtcon_file_save_buffer(char *buffer, u32 size, const char *file_name);
int hwtcon_file_save_buffer_append(char *buffer, u32 size,
		const char *file_name);
int hwtcon_file_printf(const char *file_name, char *fmt, ...);
int hwtcon_file_read_buffer(char *file_name, char *buffer, int buffer_size);
int hwtcon_file_get_size(char *file_name);
/* unzip data */
int hwtcon_file_unzip_buffer(char *zip_buffer,
	char *unzip_buffer,
	int zip_length,
	int unzip_max_length);

#endif
