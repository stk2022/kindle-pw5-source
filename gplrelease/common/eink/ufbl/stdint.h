/*
 * Copyright (C) 2020 Amazon.com Inc. or its affiliates.  All Rights Reserved.
 */

#ifndef __STDINT_H__
#define __STDINT_H__
#if defined(__STRICT_ANSI__)
#include <linux/posix_types.h>
#include <asm/types.h>

typedef __kernel_loff_t		loff_t;
typedef		__u64		uint64_t;
#endif

#endif
