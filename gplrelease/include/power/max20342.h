// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Amazon LLC.
 * Author: Chih Chieh Chou <chihcho@amazon.com>
 */

#ifndef __MAX20342_H__
#define __MAX20342_H__


extern void max20342_initialize(void);
extern void max20342_fastboot_initialize(void);
extern void max20342_enable_shutdown_mode(void);
extern void max20342_swc_auto(void);
extern void max20342_swc_usb(void);
extern void max20342_handle_interrupts(void);

#endif	/* __MAX20342_H__ */
