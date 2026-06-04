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


#ifndef __MDP_MUTEX_H__
#define __MDP_MUTEX_H__

extern void __iomem *mdp_mutex_base;
extern void __iomem *mdp_config_base;

#define MDP_MUTEX_BASE mdp_mutex_base

#define MDP_MUTEX_INTEN (MDP_MUTEX_BASE + 0x0)
#define MDP_MUTEX_INTSTA (MDP_MUTEX_BASE + 0x4)
#define MDP_MUTEX0_EN (MDP_MUTEX_BASE + 0x20)
#define MDP_MUTEX0_RST (MDP_MUTEX_BASE + 0x28)
#define MDP_MUTEX0_CTL (MDP_MUTEX_BASE + 0x2c)
#define MDP_MUTEX0_MOD (MDP_MUTEX_BASE + 0x30)

#define MDP_CONFIG_BASE mdp_config_base
#define MDP_CONFIG_RDMA0_MOUT_EN (MDP_CONFIG_BASE + 0xF08)
#define MDP_CONFIG_RSZ0_MOUT_EN (MDP_CONFIG_BASE + 0xF10)
#define MDP_CONFIG_GAMMA0_MOUT_EN (MDP_CONFIG_BASE + 0xF1C)
#define MDP_CONFIG_TDSHP0_SEL_IN (MDP_CONFIG_BASE + 0xF2C)
#define MDP_CONFIG_WROT0_SEL_IN (MDP_CONFIG_BASE + 0xF38)
#define MDP_CONFIG_DITHER0_SEL_IN (MDP_CONFIG_BASE + 0xF50)

void mdp_mutex_init(void);
void config_init(void);
void reset_mutex(void);
void config_mutex(void);
void connect_path(void);
void trigger_path(void);

#endif
