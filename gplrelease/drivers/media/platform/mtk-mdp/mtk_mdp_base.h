/*
 * Copyright (C) 2020 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __MTK_MDP_BASE_H__
#define __MTK_MDP_BASE_H__

#include <linux/io.h>
#include <linux/videodev2.h>

extern int dump_data_enable;
extern int log_level;
extern u32 mdp_test;
#define MDP_LOG_LEVEL_DEBUG 2
#define MDP_LOG_LEVEL_INFO 1
#define MDP_LOG_LEVEL_ERR 0

#define MDP_REG_MASK(addr, value, mask) \
do { \
	MDP_LOG_DEBUG("write: addr:0x%p val:0x%08x mask:0x%08x\n", \
		      addr, value, mask); \
	__raw_writel((((value) & (mask)) | \
		     (__raw_readl((unsigned long *)(addr)) & ~(mask))), \
		     (void __force __iomem *)((addr))); \
	/* add memory barrier */ \
	mb();  \
} while (0)

#define MDP_REG_SET(addr, value) MDP_REG_MASK((addr), (value), (0xffffffff))

#define MDP_LOG_DEBUG(fmt, args...) \
	do { \
		if (log_level >= MDP_LOG_LEVEL_DEBUG) \
			pr_info("[MDP/DEBUG] "fmt, ##args); \
	} while (0)

#define MDP_LOG_INFO(fmt, args...) \
	do { \
		if (log_level >= MDP_LOG_LEVEL_INFO) \
			pr_info("[MDP/INFO] "fmt, ##args); \
	} while (0)

#define MDP_LOG_ERR(fmt, args...) \
	do { \
		if (log_level >= MDP_LOG_LEVEL_ERR) \
			pr_info("[MDP/ERR] "fmt, ##args); \
	} while (0)

char *fmt_2_string(int fmt);
int mdp_debugfs_init(struct device *dev);

#define DDPMSG(fmt, args...) pr_info("[MDP/MSG] "fmt, ##args)
#define DISP_REG_GET(arg) (*(unsigned int *)(arg))
#define DISP_CPU_REG_SET(arg, val) ((*(unsigned int *)(arg)) = (val))
int write_file(char *path, char *buffer, unsigned int length);
void read_file(char *path, char *buffer, u32 length);

void __iomem *mdp_init_comp(const char *compat, u32 *phy_base);

#endif
