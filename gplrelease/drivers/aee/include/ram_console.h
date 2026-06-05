/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#ifndef __RAM_CONSOLE_H__
#define __RAM_CONSOLE_H__

#include "dev/ram_console_def.h"
#include "include/ram_console_common.h"

#define SZLOG 8192
int SLOG(const char *fmt, ...);

#define aee_pr_crit(fmt, ...)          \
	do {                               \
		const char *tmp = fmt;         \
		SLOG(tmp, ##__VA_ARGS__);      \
		pr_info(tmp, ##__VA_ARGS__);    \
	} while (0)

#define LOGD(fmt, ...)              \
	SLOG(fmt, ##__VA_ARGS__)

#if WITH_KERNEL_VM
	#define PA_TO_VA(pa) paddr_to_kvaddr(pa)
#else
	#define PA_TO_VA(pa) pa
#endif

#endif // #ifndef __RAM_CONSOLE_H__
