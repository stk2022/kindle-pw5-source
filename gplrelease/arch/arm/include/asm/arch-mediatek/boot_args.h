/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef __BOOT_ARGS_H
#define __BOOT_ARGS_H

typedef struct {
	unsigned int magic_number_begin;
	unsigned int dram_size;
	unsigned char efuse_data[512];
	unsigned int wdt_sta;
	unsigned int rpmb_key_status;
	unsigned int boot_reason;
	unsigned int reserved[16];
	unsigned int magic_number_end;
} BOOT_ARGUMENT_T;

#define BOOT_ARGUMENT_MAGIC 0x504c504c

int get_dramsize_from_boot_args(void);
void hex_dump(const char *prefix, unsigned char *buf, int len);
int efuse_dump(unsigned int start, unsigned int count);
int efuse_read_index(unsigned int index, unsigned char *data);
unsigned int get_rpmb_key_status_from_boot_args(void);
unsigned int get_boot_reason_from_boot_args(void);

#endif	/* __BOOT_ARGS_H */
