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


#ifndef __MTK_MDP_PATH_H__
#define __MTK_MDP_PATH_H__

#include "mtk_mdp_core.h"
#include "mtk_mdp_gamma.h"

struct mtk_mdp_frame_config {
	u32 src_width;
	u32 src_height;
	u32 src_pitch;
	u32 src_fmt;
	dma_addr_t src_addr;
	u32 dst_width;
	u32 dst_height;
	u32 dst_pitch;
	u32 dst_fmt;
	u32 crop_x;
	u32 crop_y;
	u32 crop_w;
	u32 crop_h;
	dma_addr_t dst_addr;
	u32 rotate;
	u32 invert;
	u32 dth_en;
	u32 dth_algo;
	u32 gamma_flag;
};

int mdp_get_sync_work_nr(void);
int mdp_wait_sync_work(void);
void config_path(struct mtk_mdp_frame_config *config);
void mdp_tile_config(struct mtk_mdp_frame_config *config);
void mdp_sync_config(struct mtk_mdp_frame_config *config);
void mdp_update_cmap_lut(u16 *gamma_lut, u32 len);

void mtk_mdp_sfd_init(struct mtk_mdp_dev *mdp_dev);
void mtk_mdp_sfd_deinit(void);

#endif
