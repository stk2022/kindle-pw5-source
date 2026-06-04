/*
 * Copyright 2020 Amazon Technologies, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __DISPLAY_H__
#define __DISPLAY_H__

int eink_setup_buffer(void);
int eink_setup_hw(void);
int eink_load_splash(int middle, int *x, int *y, int *width, int *height);
int eink_load_critical_battery(int *x, int *y, int *width, int *height);
int eink_update_screen(int x, int y, int width, int height, bool wait);
int eink_update_screen_raw(const char *name);
int eink_update_screen_mode0(void);

#endif
