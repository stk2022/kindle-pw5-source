/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2008-2012 Travis Geiselbrecht
 */

#ifndef __AEE_TYPES
#define __AEE_TYPES
typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef unsigned long ulong;

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

typedef signed char        int8_t;
typedef short              int16_t;
typedef int                int32_t;
typedef long long          int64_t;

enum handler_return {
	INT_NO_RESCHEDULE = 0,
	INT_RESCHEDULE,
};

#define NULL ((void *)0)
#endif

#define PAGE_SIZE 4096
