/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _GFX_DRV_H_
#define _GFX_DRV_H_

#include "gfx_manager.h"
#ifdef MTK_FB_ION_SUPPORT
#include "ion_drv.h"
#endif

#define GFX_LOG_D(fmt, args...)   pr_info("[GFX]"fmt, ##args)
#define GFX_LOG_W(fmt, args...)   pr_info("[GFX]"fmt, ##args)
#define GFX_LOG_E(fmt, args...)   pr_info("[GFX]"fmt, ##args)
#define GFX_LOG_I(fmt, args...)   pr_info("[GFX]"fmt, ##args)

extern unsigned int gfx_dbg_level;
#define GFX_FUNC_LOG	(1 << 0)
#define GFX_FLOW_LOG	(1 << 1)
#define GFX_DRV_LOG		(1 << 2)
#define GFX_IF_LOG		(1 << 3)
#define GFX_CMDQ_LOG	(1 << 4)
#define GFX_LIST_LOG	(1 << 5)
#define GFX_ION_LOG		(1 << 6)
#define GFX_VYSNC_LOG	(1 << 8)

#define GFX_PRINTF(level, string, args...) do { \
		if (gfx_dbg_level & (level)) \
			GFX_LOG_I(string, args); \
	} while (0)

#define GFX_HW_INST_NUM	2

struct Gfx_buffer_list {
	List_Buffer_STATE list_buf_state;
	struct ion_handle *ion_src_handles;
	struct ion_handle *ion_dst_handles;
	GFX_CMD_CONFIG buffer_info;
	struct list_head list;
};

enum GFX_RET_CODE_T {
	GFX_RET_OK,
	GFX_RET_INV_ARG,
	GFX_RET_OUT_OF_MEM,
	GFX_RET_OUT_OF_LIST,
	GFX_RET_INV_LIST,
	GFX_RET_ERR_INTERNAL,
	GFX_RET_FENCE_FAIL,
	GFX_RET_UNDEF_ERR
};

struct GFX_DRV {
	struct task_struct *gfx_drv_task[GFX_HW_INST_NUM];
	wait_queue_head_t gfx_drv_wq[GFX_HW_INST_NUM];
	wait_queue_head_t gfx_cmd_wq[GFX_HW_INST_NUM];
	struct list_head GFX_Buffer_Head[GFX_HW_INST_NUM];
	struct ion_client *gfx_ion_client;
	struct mutex gfx_drv_lock[GFX_HW_INST_NUM];
	struct mutex gfx_cmdque_lock[GFX_HW_INST_NUM];
	struct mutex gfx_cmdlist_lock[GFX_HW_INST_NUM];
};

enum E_MI_GFX_ERR_CODE_T {
	E_GFX_OK = 0,
	E_GFX_INV_ARG,
	E_GFX_OUT_OF_MEM,
	E_GFX_UNINIT,
	E_GFX_UNDEF_ERR,
	E_GFX_WOULD_BLOCK,
	E_GFX_EMPTY_BUFFER
};

#ifdef MTK_FB_ION_SUPPORT
#define GFX_INVALID_ION_FD		(-1)
#define GFX_NO_ION_FD			((int)(~0U>>1))
#define GFX_ION_CLIENT_NAME		"GFX_ion_client"
void gfx_ion_init(void);
struct ion_handle *gfx_ion_import_handle(struct ion_client *client, int fd);
size_t gfx_ion_phys_mmu_addr(struct ion_client *client,
			      struct ion_handle *handle, unsigned int *mva);
void gfx_ion_free_handle(struct ion_client *client, struct ion_handle *handle);
#endif
int Gfx_SendCmdtoBufList(GFX_CMD_CONFIG *buffer_info);
int GFX_CmdBufHWConvert(unsigned int u4GfxHwId, Gfx_buffer_list *pBuffList);
int gfx_get_config_buffer_ion(Gfx_buffer_list *pBuffList);
void gfx_remove_config_buffer_list(unsigned int id);
void gfx_remove_user_config_buffer_list(unsigned int id, unsigned int user);
int gfx_drv_init(unsigned int irq0, unsigned int irq1);
int gfx_drv_uninit(void);
int Gfx_GetFreeCmdbuffer(GFX_CMD_CONFIG *buffer_info);
int Gfx_ReleaseCmdbuffer(GFX_CMD_CONFIG *buffer_info);
int Gfx_FlushCmdbuffer(GFX_CMD_CONFIG *buffer_info);
#endif
