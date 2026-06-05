/*
 * Copyright 2020 Amazon Technologies, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __EINK_UFBL_H__
#define __EINK_UFBL_H__

bool ufbl_is_secure_cpu(void);
bool ufbl_is_production_device(void);
bool ufbl_is_locked_production_device(void);

void ufbl_lock_cli(void);

int ufbl_fastboot_getvar(char *cmd_parameter, char *var_parameter, char *response);
int ufbl_fastboot_handle_command(const char *cmd_string, char *response);
int ufbl_fastboot_handle_flash(const char *cmd_parameter, void *download_buffer,
		u32 download_bytes, char *response);
#endif
