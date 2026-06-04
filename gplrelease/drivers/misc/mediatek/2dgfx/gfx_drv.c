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

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/miscdevice.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include "gfx_drv.h"
#include "gfx_manager.h"
#include "gfx_if.h"
#include "gfx_cmdque.h"
#include "gfx_dif.h"

#define GFX_RESET_BOTH          0xF0
struct GFX_DRV gfx;
atomic_t gfx_drv_event[GFX_HW_INST_NUM] = {ATOMIC_INIT(0)};
atomic_t gfx_cmd_event[GFX_HW_INST_NUM] = {ATOMIC_INIT(0)};
unsigned int gfx_hw_idx[GFX_HW_INST_NUM] = { 0, 1 };
unsigned int gfx_dbg_level;

#ifdef MTK_FB_ION_SUPPORT
void gfx_ion_init(void)
{
	if (!gfx->gfx_ion_client && g_ion_device) {
		gfx->gfx_ion_client =
			ion_client_create(g_ion_device, GFX_ION_CLIENT_NAME);
		GFX_LOG_E("create ion client 0x%p\n", gfx.gfx_ion_client);
	}
	if (!gfx->gfx_ion_client)
		GFX_LOG_E("create ion client failed!\n");
}

struct ion_handle *gfx_ion_import_handle(struct ion_client *client, int fd)
{
	struct ion_handle *handle = NULL;
	struct ion_mm_data mm_data;

	if (fd == GFX_NO_ION_FD) {
		GFX_LOG_E("NO NEED ion support\n");
		return handle;
	}

	if (!client) {
		GFX_LOG_E("invalid ion client!\n");
		return handle;
	}

	handle = ion_import_dma_buf(client, fd);
	if (!handle) {
		GFX_LOG_E("import ion handle failed!\n");
		return handle;
	}

	mm_data.mm_cmd = ION_MM_CONFIG_BUFFER;
	mm_data.config_buffer_param.kernel_handle = handle;
	mm_data.config_buffer_param.module_id = 0;
	mm_data.config_buffer_param.security = 0;
	mm_data.config_buffer_param.coherent = 0;

	if (ion_kernel_ioctl(client, ION_CMD_MULTIMEDIA,
		(unsigned long)&mm_data)) {
		GFX_LOG_E("configure ion buffer failed!\n");
		return NULL;
	}

	GFX_LOG_E("import ion handle fd=%d,hnd=0x%p\n", fd, handle);
	return handle;
}

void gfx_ion_free_handle(struct ion_client *client, struct ion_handle *handle)
{
	if (!client) {
		GFX_LOG_E("invalid ion client!\n");
		return;
	}
	if (!handle) {
		GFX_LOG_E("invalid ion hanlde!\n");
		return;
	}

	ion_free(client, handle);
	GFX_LOG_E("free ion handle 0x%p\n", handle);
}

size_t gfx_ion_phys_mmu_addr(struct ion_client *client,
	struct ion_handle *handle, unsigned int *mva)
{
	size_t size;

	if (!client) {
		GFX_LOG_E("invalid ion client!\n");
		return 0;
	}
	if (!handle) {
		GFX_LOG_E("invalid ion hanlde!\n");
		return 0;
	}
	ion_phys(client, handle, (ion_phys_addr_t *) mva, &size);

	GFX_LOG_E("alloc mmu addr handle = 0x%p,mva = 0x%08x\n",
		handle, (unsigned int)*mva);

	return size;
}

int gfx_get_config_buffer_ion(Gfx_buffer_list *pBuffList)
{
	int ret = GFX_RET_OK;
	struct ion_handle *gfx_srcbuf_ion_handle = NULL;
	struct ion_handle *gfx_dstbuf_ion_handle = NULL;
	unsigned int src_mva;
	unsigned int dst_mva;

	if (pBuffList->buffer_info->gfx_ion.ion_src_fd > 0) {
		gfx_srcbuf_ion_handle = gfx_ion_import_handle(
			struct ion_client *client, int fd)(gfx.gfx_ion_client,
			pBuffList->buffer_info->gfx_ion.ion_src_fd);

		if (gfx_srcbuf_ion_handle == NULL) {
			GFX_LOG_E("gfx_cmd_config get ion handle err\n");
			ret = -GFX_RET_INV_ARG;
			return ret;
		}

		gfx_ion_phys_mmu_addr(gfx.gfx_ion_client,
			gfx_srcbuf_ion_handle, &src_mva);

		if (src_mva == 0) {
			GFX_LOG_E("gfx_cmd_config ion_phys err\n");
			ret = -GFX_RET_INV_ARG;
			return ret;
		}
	} else {
		if (pBuffList->buffer_info->gfx_ion.src_phy_addr < 0) {
			GFX_LOG_E("gfx_cmd_config src_phy_addr err\n");
			ret = -GFX_RET_INV_ARG;
			return ret;
		}
		src_mva = pBuffList->buffer_info->gfx_ion.src_phy_addr;
	}

	if (pBuffList->buffer_info->gfx_ion.ion_dst_fd > 0) {
		gfx_dstbuf_ion_handle =
			gfx_ion_import_handle(gfx.gfx_ion_client,
				pBuffList->buffer_info->gfx_ion.ion_dst_fd);
		if (gfx_dstbuf_ion_handle == NULL) {
			GFX_LOG_E("gfx_cmd_config get ion handle err\n");
			ret = -GFX_RET_INV_ARG;
			return ret;
		}

		gfx_ion_phys_mmu_addr(gfx.gfx_ion_client,
			gfx_dstbuf_ion_handle, &dst_mva);

		if (dst_mva == 0) {
			GFX_LOG_E("gfx_cmd_config ion_phys err\n");
			ret = -GFX_RET_INV_ARG;
			return ret;
		}
	} else {
			if (pBuffList->buffer_info->gfx_ion.dst_phy_addr < 0) {
				GFX_LOG_E("gfx_cmd_config src_phy_addr err\n");
				ret = -GFX_RET_INV_ARG;
				return ret;
			}

			dst_mva = pBuffList->buffer_info->gfx_ion.dst_phy_addr;
	}

	pBuffList->ion_src_handles = gfx_srcbuf_ion_handle;
	pBuffList->ion_dst_handles = gfx_dstbuf_ion_handle;

	switch (pBuffList->buffer_info->gfx_cmd.gfx_cmd_type) {
	case GFX_BUFF_TYPE_ALPHA_COMPOSITION:
		pBuffList->buffer_info->gfx_cmd->rAlphaComposeCmd.pv_src =
			src_mva;
		pBuffList->buffer_info->gfx_cmd->rAlphaComposeCmd.pv_dst =
			dst_mva;
		break;
	case GFX_BUFF_TYPE_FILL_RECT:
		pBuffList->buffer_info->gfx_cmd->rFillCmd.pv_dst =
			dst_mva;
		break;
	case GFX_BUFF_TYPE_BITBLT:
		pBuffList->buffer_info->gfx_cmd->rBitbltCmd.pv_src =
			src_mva;
		pBuffList->buffer_info->gfx_cmd->rBitbltCmd.pv_dst =
			dst_mva;
		break;
	}

	return ret;
}
#endif

void gfx_remove_config_buffer_list(unsigned int id)
{
	int i = id;

	mutex_lock(&(gfx.gfx_cmdlist_lock[i]));

	if (!list_empty(&(gfx.GFX_Buffer_Head[i]))) {
		Gfx_buffer_list *pBuffList = NULL;

		list_for_each_entry(pBuffList, &(gfx.GFX_Buffer_Head[i]),
			list) {
		if (pBuffList->list_buf_state != list_new) {
#ifdef MTK_FB_ION_SUPPORT
		if (pBuffList->ion_src_handles != NULL)
			gfx_ion_free_handle(gfx.gfx_ion_client,
				pBuffList->ion_src_handles);

		if (pBuffList->ion_dst_handles != NULL)
			gfx_ion_free_handle(gfx.gfx_ion_client,
				pBuffList->ion_dst_handles);
#endif
				list_del_init(&(pBuffList->list));
				vfree(pBuffList);
		}
		}
	}

	mutex_unlock(&(gfx.gfx_cmdlist_lock[i]));

}

void gfx_remove_user_config_buffer_list(unsigned int id, unsigned int user)
{
	mutex_lock(&(gfx.gfx_cmdlist_lock[id]));

	if (!list_empty(&(gfx.GFX_Buffer_Head[id]))) {
		Gfx_buffer_list *pBuffList = NULL;

		list_for_each_entry(pBuffList,
			&(gfx.GFX_Buffer_Head[id]), list) {
			if (pBuffList->buffer_info.gfx_id.t_user == user) {
#ifdef MTK_FB_ION_SUPPORT
			if (pBuffList->ion_src_handles != NULL)
				gfx_ion_free_handle(gfx.gfx_ion_client,
						pBuffList->ion_src_handles);

			if (pBuffList->ion_dst_handles != NULL)
				gfx_ion_free_handle(gfx.gfx_ion_client,
						pBuffList->ion_dst_handles);
#endif
				list_del_init(&(pBuffList->list));
				vfree(pBuffList);
		}
		}
	}

	mutex_unlock(&(gfx.gfx_cmdlist_lock[id]));
}

int Gfx_GetFreeCmdbuffer(GFX_CMD_CONFIG *buffer_info)
{
	int ret = GFX_RET_OK;
	Gfx_buffer_list *pBuffList = NULL;
	int i = 0;
	int list_count[GFX_HW_INST_NUM] = { 0, 0 };

	for (i = 0; i <= GFX_HW_INST_NUM; i++) {
		if (list_empty(&(gfx.GFX_Buffer_Head[i]))) {
			buffer_info->gfx_id.u4GfxTicketId = i;
		} else {
			list_for_each_entry(pBuffList,
				&(gfx.GFX_Buffer_Head[i]), list)
			list_count[i]++;
		}
	}

	if (list_count[0] >= list_count[1])
		buffer_info->gfx_id.u4GfxTicketId = 1;
	else
		buffer_info->gfx_id.u4GfxTicketId = 0;

	return ret;
}

int Gfx_ReleaseCmdbuffer(GFX_CMD_CONFIG *buffer_info)
{
	int ret = GFX_RET_OK;
	unsigned int id = buffer_info->gfx_id.u4GfxTicketId;
	unsigned int user = buffer_info->gfx_id.t_user;

	gfx_remove_user_config_buffer_list(id, user);

	return ret;
}

int Gfx_FlushCmdbuffer(GFX_CMD_CONFIG *buffer_info)
{
	int ret = GFX_RET_OK;
	unsigned int id = buffer_info->gfx_id.u4GfxTicketId;

	if (id <= GFX_HW_INST_NUM) {
		mutex_lock(&(gfx.gfx_drv_lock[id]));
		atomic_set(&gfx_drv_event[id], 1);
		wake_up_interruptible(&gfx.gfx_drv_wq[id]);
		mutex_unlock(&(gfx.gfx_drv_lock[id]));
	} else
		ret = -GFX_RET_INV_ARG;

	return ret;
}

int Gfx_SendCmdtoBufList(GFX_CMD_CONFIG *buffer_info)
{
	int ret = GFX_RET_OK;
	Gfx_buffer_list *pBuffList = NULL;
	int i = buffer_info->gfx_id.u4GfxTicketId;

	if (i >= GFX_HW_INST_NUM) {
		ret = -GFX_RET_INV_ARG;
		GFX_LOG_E("hw_id_error\n");
		return ret;
	}

	pBuffList = vmalloc(sizeof(Gfx_buffer_list));

	if (!pBuffList) {
		ret = -GFX_RET_INV_ARG;
		GFX_LOG_E("could not allocate buffer_list\n");
		return ret;
	}

	memcpy(&pBuffList->buffer_info,
		buffer_info, sizeof(GFX_CMD_CONFIG));
	pBuffList->list_buf_state = list_new;
	INIT_LIST_HEAD(&pBuffList->list);
	mutex_lock(&(gfx.gfx_cmdlist_lock[i]));
	list_add_tail(&pBuffList->list, &(gfx.GFX_Buffer_Head[i]));
	mutex_unlock(&(gfx.gfx_cmdlist_lock[i]));

	if (buffer_info->gfx_sync == GFX_CMD_SYNC) {
		mutex_lock(&(gfx.gfx_drv_lock[i]));
		atomic_set(&gfx_drv_event[i], 1);
		wake_up_interruptible(&gfx.gfx_drv_wq[i]);
		mutex_unlock(&(gfx.gfx_drv_lock[i]));
		wait_event_interruptible(gfx.gfx_cmd_wq[i],
			atomic_read(&gfx_cmd_event[i]));
		atomic_set(&gfx_cmd_event[i], 0);
	}

	return ret;
}

int GFX_CmdBufHWConvert(unsigned int u4GfxHwId, Gfx_buffer_list *pBuffList)
{
	int ret = GFX_RET_OK;

#ifdef MTK_FB_ION_SUPPORT
	if (gfx_get_config_buffer_ion(pBuffList) < 0)
		return GFX_RET_INV_ARG;
#endif

	switch (pBuffList->buffer_info.gfx_cmd.gfx_cmd_type) {
	case GFX_SET_TYPE_ALPHA_COMPOSE:
		ret = _GfxComposeLoop(u4GfxHwId,
			&pBuffList->buffer_info.gfx_cmd.rAlphaComposeCmd);
		break;
	case GFX_SET_TYPE_FILL_RECT:
		ret = _GfxFillRect(u4GfxHwId,
			&pBuffList->buffer_info.gfx_cmd.rFillCmd);
		break;
	case GFX_SET_TYPE_BITBLIT:
		ret = _GfxBitblt(u4GfxHwId,
			&pBuffList->buffer_info.gfx_cmd.rBitbltCmd);
		break;
	default:
		GFX_LOG_D("[GFX] Invalid graphic command type: %d\n",
			pBuffList->buffer_info.gfx_cmd.gfx_cmd_type);
		break;
	}

	return ret;
}

int Gfx_Drv_Main(void *data)
{
	unsigned int hw_id = *(unsigned int *)data;
	int wait_ret = 0;
	unsigned int cmd_count = 0;
	Gfx_buffer_list *pBuffList = NULL;

	while (1) {
		wait_ret =
			wait_event_interruptible(gfx.gfx_drv_wq[hw_id],
				atomic_read(&gfx_drv_event[hw_id]));
				atomic_set(&gfx_drv_event[hw_id], 0);

			mutex_lock(&(gfx.gfx_drv_lock[hw_id]));
			if (!list_empty(&(gfx.GFX_Buffer_Head[hw_id]))) {

				list_for_each_entry(pBuffList,
					&(gfx.GFX_Buffer_Head[hw_id]), list) {

				if (pBuffList->list_buf_state == list_new) {
				GFX_CmdBufHWConvert(hw_id, pBuffList);
				pBuffList->list_buf_state = list_configed;
				cmd_count++;

				if (pBuffList->buffer_info.gfx_sync ==
					GFX_CMD_SYNC) {
					GFX_Flush(hw_id);
					gfx_remove_config_buffer_list(hw_id);
					cmd_count = 0;
					atomic_set(&gfx_cmd_event[hw_id], 1);
					wake_up_interruptible(&
						gfx.gfx_cmd_wq[hw_id]);
				}
				}
			}
		}

		if (cmd_count > 0) {
			cmd_count = 0;
			GFX_Flush(hw_id);
			gfx_remove_config_buffer_list(hw_id);
		}

		mutex_unlock(&(gfx.gfx_drv_lock[hw_id]));

		if (kthread_should_stop())
			break;
	}

	return 0;
}

int gfx_drv_init(unsigned int irq0, unsigned int irq1)
{
	int ret = 0;
	unsigned long i4 = 0;

	ret = Gfx_RegFileInit();

	for (i4 = 0; i4 < GFX_HW_INST_NUM; i4++) {
		ret = GFX_Reset(i4, GFX_RESET_BOTH);
		GFX_DifIrqInit(irq0, irq1, i4);
		ret = GFX_DifInit(i4);
		mutex_init(&gfx.gfx_drv_lock[i4]);
		mutex_init(&gfx.gfx_cmdlist_lock[i4]);
	}

	Gfx_cmdq_init();

#ifdef MTK_FB_ION_SUPPORT
	gfx_ion_init();
#endif
	init_waitqueue_head(&gfx.gfx_drv_wq[0]);

	if (!gfx.gfx_drv_task[0]) {
		gfx.gfx_drv_task[0] = kthread_create(Gfx_Drv_Main,
			(void *)&(gfx_hw_idx[0]), "Gfx_Drv0_Main");
		wake_up_process(gfx.gfx_drv_task[0]);
		GFX_LOG_D("kthread_create Gfx_Drv_Main 0\n");
	}

	init_waitqueue_head(&gfx.gfx_drv_wq[1]);

	if (!gfx.gfx_drv_task[1]) {
		gfx.gfx_drv_task[1] = kthread_create(Gfx_Drv_Main,
			(void *)&(gfx_hw_idx[1]), "Gfx_Drv1_Main");
		wake_up_process(gfx.gfx_drv_task[1]);
		GFX_LOG_D("kthread_create Gfx_Drv_Main 1\n");
	}

	return ret;
}

int gfx_drv_uninit(void)
{
	GFX_CmdQueUninit();
	GFX_DifUninit();

	return E_GFX_OK;
}


