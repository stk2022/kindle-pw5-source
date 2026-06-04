/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2008-2012 Travis Geiselbrecht
 */
#ifndef __DEBUG_H
#define __DEBUG_H

#if !defined(LK_DEBUGLEVEL)
#define LK_DEBUGLEVEL 0
#endif

/* debug levels */
#define CRITICAL 0
#define ALWAYS 0
#define INFO 1
#define SPEW 2

#endif
