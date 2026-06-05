/*
 * Copyright 2017-2020 Amazon Technologies, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __EINK_MISC_H__
#define __EINK_MISC_H__

int misc_file_exist(const char *name);
int load_misc_file(const char *name, void *addr, unsigned long *len_read);
int misc_block_read(void *addr, unsigned long offset, unsigned long length);
int misc_block_write(void *addr, unsigned long offset, unsigned long length);
int misc_block_erase(void);
#endif
