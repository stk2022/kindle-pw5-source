/*
 * Copyright (c) 2015-2016 MediaTek Inc.
 * Author: Qing Li <qing.li@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _MTK_MDP_TUNING_H_
#define _MTK_MDP_TUNING_H_

#ifdef CONFIG_DEBUG_FS
void mtk_mdp_tuning_read(unsigned long addr);
void mtk_mdp_tuning_write(unsigned long addr, unsigned int val);
void mtk_mdp_vpu_handle_tuning_ack(struct mdp_ipi_comm_ack *msg);
#endif

#endif /* _MTK_MDP_TUNING_H_ */
