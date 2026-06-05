/*
 * Copyright (c) 2015-2016 MediaTek Inc.
 * Author: Houlong Wei <houlong.wei@mediatek.com>
 *         Ming Hsiu Tsai <minghsiu.tsai@mediatek.com>
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

#include <linux/dma-mapping.h>
#include "mtk_mdp_core.h"
#include "mtk_mdp_vpu.h"
#ifdef CONFIG_VIDEO_MEDIATEK_VCU
#include "mtk_mdp_vpu.h"
#else
#include "mtk_vpu.h"
#endif
#ifdef CONFIG_VIDEO_MEDIATEK_MDP_FRVC
#include "./fast-rvc/mtk_mdp_frvc.h"
#endif
#ifdef CONFIG_DEBUG_FS
#include "mtk_mdp_tuning.h"
#endif

#define FRVC_DEBUG 0 /* Only for FRVC debug */

static inline struct mtk_mdp_ctx *vpu_to_ctx(struct mtk_mdp_vpu *vpu)
{
	return container_of(vpu, struct mtk_mdp_ctx, vpu);
}

static void mtk_mdp_vpu_handle_init_ack(struct mdp_ipi_comm_ack *msg)
{
	struct mtk_mdp_vpu *vpu = (struct mtk_mdp_vpu *)
					(unsigned long)msg->ap_inst;
	struct mdp_cmdq_info *cmdq;

	/* mapping VPU address to kernel virtual address */
	vpu->vsi = (struct mdp_process_vsi *)
			vpu_mapping_dm_addr(vpu->pdev, msg->vpu_inst_addr);
	vpu->inst_addr = msg->vpu_inst_addr;

	/* mapping cmdq buffer address in VPU to kernel virtual address */
	cmdq = &vpu->vsi->cmdq;
	if (cmdq->vpu_buf_addr != 0uLL) {
		cmdq->ap_buf_addr = (uint64_t)(unsigned long)
			vpu_mapping_dm_addr(vpu->pdev,
				(unsigned long)cmdq->vpu_buf_addr);
		cmdq->ap_buf_pa = __pa(cmdq->ap_buf_addr);
	}
}

static void mtk_mdp_vpu_handle_pq_config_ack(struct mdp_pq_cfg_ack *msg)
{
	unsigned int msg_id = *(unsigned int *)msg;
	struct mtk_mdp_vpu *vpu = (struct mtk_mdp_vpu *)
						(unsigned long)msg->ap_inst;

	vpu->failure = msg->status;

	if (!vpu->failure) {
		vpu->pq_cfg_ack.get_cont_level = msg->get_cont_level;
		vpu->pq_cfg_ack.get_brit_level = msg->get_brit_level;
	} else {
		pr_info("[err]:msg 0x%x, failure:%d", msg_id, vpu->failure);
	}
}

#ifdef CONFIG_VIDEO_MEDIATEK_VCU
static int mtk_mdp_vpu_ipi_handler(void *data, unsigned int len, void *priv)
#else
static void mtk_mdp_vpu_ipi_handler(void *data, unsigned int len, void *priv)
#endif
{
	unsigned int msg_id = *(unsigned int *)data;
	struct mdp_ipi_comm_ack *msg = (struct mdp_ipi_comm_ack *)data;
	struct mtk_mdp_vpu *vpu = (struct mtk_mdp_vpu *)
					(unsigned long)msg->ap_inst;
	struct mtk_mdp_ctx *ctx;

	if (msg_id == VPU_MDP_PQ_CONFIG_ACK) {
		mtk_mdp_vpu_handle_pq_config_ack((struct mdp_pq_cfg_ack *)data);
		goto handle_done;
	}

	vpu->failure = msg->status;
	if (!vpu->failure) {
		switch (msg_id) {
		case VPU_MDP_INIT_ACK:
			mtk_mdp_vpu_handle_init_ack(data);
			break;
		case VPU_MDP_DEINIT_ACK:
		case VPU_MDP_PROCESS_ACK:
		case VPU_MDP_CMDQ_DONE_ACK:
			break;
#ifdef CONFIG_DEBUG_FS
		case VPU_MDP_TUNING_ACK:
			mtk_mdp_vpu_handle_tuning_ack(data);
			break;
#endif
		default:
			ctx = vpu_to_ctx(vpu);
			dev_err(&ctx->mdp_dev->pdev->dev,
				"handle unknown ipi msg:0x%x\n",
				msg_id);
			break;
		}
	} else {
		ctx = vpu_to_ctx(vpu);
		mtk_mdp_dbg(0, "[%d]:msg 0x%x, failure:%d", ctx->id,
			    msg_id, vpu->failure);
	}
handle_done:
#ifdef CONFIG_VIDEO_MEDIATEK_VCU
	return 0;
#endif
}

static enum ipi_id mdp_dev_id_2_ipi_id(int id)
{
	if (id == 1)
		return IPI_MDP_1;
	else if (id == 2)
		return IPI_MDP_2;
	else if (id == 3)
		return IPI_MDP_3;
	else
		return IPI_MDP;
}

int mtk_mdp_vpu_register(struct platform_device *pdev)
{
	struct mtk_mdp_dev *mdp = platform_get_drvdata(pdev);
	int err;
	const char *name;

	if (mdp->id == 1)
		name = "mdp_1_vpu";
	else if (mdp->id == 2)
		name = "mdp_2_vpu";
	else if (mdp->id == 3)
		name = "mdp_3_vpu";
	else
		name = "mdp_vpu";

	err = vpu_ipi_register(mdp->vpu_dev, mdp_dev_id_2_ipi_id(mdp->id),
			       mtk_mdp_vpu_ipi_handler, name, NULL);
	if (err)
		dev_err(&mdp->pdev->dev,
			"vpu_ipi_registration fail status=%d\n", err);

	return err;
}

static int mtk_mdp_vpu_send_msg(void *msg, int len, struct mtk_mdp_vpu *vpu,
				int id)
{
	struct mtk_mdp_ctx *ctx = vpu_to_ctx(vpu);
	int err;

	if (!vpu->pdev) {
		mtk_mdp_dbg(1, "[%d]:vpu pdev is NULL", ctx->id);
		return -EINVAL;
	}
	mutex_lock(&ctx->mdp_dev->vpulock);
	mtk_mdp_dbg(2, "id=0x%x, msg_len=%d", *(int *)msg, len);
#ifdef CONFIG_VIDEO_MEDIATEK_MDP_FRVC
	err = 0;

	if (*(uint32_t *)msg == (uint32_t)AP_MDP_INIT) {
		#if FRVC_DEBUG
		vpu->vsi = NULL;
		#else
		err = vpu_ipi_send(vpu->pdev, (enum ipi_id)id, msg, (u32)len);
		#endif
		if (vpu->vsi == NULL) {
			vpu->vsi = &vpu->vsi_frvc;
			mtk_mdp_dbg(0,
				"init vpu failed %d, use fast-rvc flow\n", err);
			err = 0;
			vpu->initialized = 0;
		} else
			vpu->initialized = 1;
	} else if (*(uint32_t *)msg == (uint32_t)AP_MDP_PROCESS) {
		if (vpu->initialized == 1) {
			/* normal flow, mtk mdp vpu is initialized */
			err = vpu_ipi_send(vpu->pdev,
				(enum ipi_id)id, msg, (u32)len);
		} else {
			/* vpu is not initialized, try to initialize it */
			#if FRVC_DEBUG
			err = 1;
			#else
			struct mdp_ipi_init init_msg;

			init_msg.msg_id = (uint32_t)AP_MDP_INIT;
			init_msg.ipi_id = (uint32_t)vpu->ipi_id;
			init_msg.ap_inst = (uint64_t)(unsigned long)vpu;
			err = vpu_ipi_send(vpu->pdev, (enum ipi_id)id,
				(void *)&init_msg,
				(u32)sizeof(init_msg));
			#endif

			if (err != 0 || vpu->failure != 0) {
				/* VPU is not running, go to fast rvc flow*/
				err = mtk_mdp_frvc_process(ctx);
			} else {
				#ifdef CONFIG_DEBUG_FS
				struct mdp_ipi_init tuning_msg;
				#endif

				vpu->initialized = 1;
				err = mtk_mdp_frvc_stop();

				#ifdef CONFIG_DEBUG_FS
				tuning_msg.msg_id = (uint32_t)AP_MDP_TUNING;
				tuning_msg.ipi_id = (uint32_t)vpu->ipi_id;
				tuning_msg.ap_inst =
						(uint64_t)(unsigned long)vpu;
				err = vpu_ipi_send(vpu->pdev, (enum ipi_id)id,
					(void *)&tuning_msg,
					(u32)sizeof(tuning_msg));
				if (!err && vpu->failure)
					pr_info("failed to send tuning msg\n");
				#endif

				vpu->vsi->src_config = vpu->vsi_frvc.src_config;
				vpu->vsi->src_buffer = vpu->vsi_frvc.src_buffer;
				vpu->vsi->dst_config = vpu->vsi_frvc.dst_config;
				vpu->vsi->dst_buffer = vpu->vsi_frvc.dst_buffer;
				vpu->vsi->misc = vpu->vsi_frvc.misc;
				vpu->vsi->pq = vpu->vsi_frvc.pq;
				((struct mdp_ipi_comm *)msg)->vpu_inst_addr =
					vpu->inst_addr;
				/* VPU is initialized, switch to normal flow */
				err = vpu_ipi_send(vpu->pdev,
					(enum ipi_id)id, msg, (u32)len);
			}
		}
	} else if (*(uint32_t *)msg == (uint32_t)AP_MDP_DEINIT) {
		if (vpu->initialized == 1)
			err = vpu_ipi_send(vpu->pdev,
				(enum ipi_id)id, msg, (u32)len);
		else {
			/* todo: mtk_mdp_frc_uninit */
			err = 0;
		}
	} else if (*(uint32_t *)msg == (uint32_t)AP_MDP_PQ_CONFIG) {
		if (vpu->initialized == 1)
			err = vpu_ipi_send(vpu->pdev,
				(enum ipi_id)id, msg, (u32)len);
		else {
			err = 0;
		}

	} else
		err = vpu_ipi_send(vpu->pdev, (enum ipi_id)id, msg, (u32)len);
#else
	err = vpu_ipi_send(vpu->pdev, (enum ipi_id)id, msg, (u32)len);
#endif
	if (err)
		dev_err(&ctx->mdp_dev->pdev->dev,
			"vpu_ipi_send fail status %d\n", err);

	mtk_mdp_dbg(2, "id=0x%x, return from vpu", id);
	mutex_unlock(&ctx->mdp_dev->vpulock);

	return err;
}

static int mtk_mdp_vpu_send_ap_ipi(struct mtk_mdp_vpu *vpu, uint32_t msg_id)
{
	int err;
	struct mdp_ipi_comm msg;

	msg.msg_id = msg_id;
	msg.ipi_id = (uint32_t)vpu->ipi_id;
	msg.vpu_inst_addr = vpu->inst_addr;
	msg.ap_inst = (uint64_t)(unsigned long)vpu;
	err = mtk_mdp_vpu_send_msg((void *)&msg, sizeof(msg), vpu, vpu->ipi_id);
	if (!err && vpu->failure)
		err = -EINVAL;

	return err;
}

int mtk_mdp_vpu_init(struct mtk_mdp_vpu *vpu)
{
	int err;
	struct mdp_ipi_init msg;
	struct mtk_mdp_ctx *ctx = vpu_to_ctx(vpu);

	vpu->pdev = ctx->mdp_dev->vpu_dev;
	vpu->ipi_id = (int32_t)mdp_dev_id_2_ipi_id(ctx->mdp_dev->id);

	msg.msg_id = AP_MDP_INIT;
	msg.ipi_id = (uint32_t)vpu->ipi_id;
	msg.ap_inst = (uint64_t)(unsigned long)vpu;
	err = mtk_mdp_vpu_send_msg((void *)&msg, sizeof(msg), vpu, vpu->ipi_id);
	if (!err && vpu->failure)
		err = -EINVAL;

#ifdef CONFIG_DEBUG_FS
#ifdef CONFIG_VIDEO_MEDIATEK_MDP_FRVC
	if (vpu->initialized != 0) {
		msg.msg_id = AP_MDP_TUNING;
		msg.ipi_id = (uint32_t)vpu->ipi_id;
		msg.ap_inst = (uint64_t)(unsigned long)vpu;
		err = mtk_mdp_vpu_send_msg(
			(void *)&msg, sizeof(msg), vpu, vpu->ipi_id);
		if (!err && vpu->failure)
			err = -EINVAL;
	}
#endif
#endif

	return err;
}

int mtk_mdp_vpu_deinit(struct mtk_mdp_vpu *vpu)
{
	return mtk_mdp_vpu_send_ap_ipi(vpu, AP_MDP_DEINIT);
}

static int mtk_mdp_cmdq_exec(struct mtk_mdp_ctx *ctx,
	struct mdp_cmdq_info *cmdq)
{
	struct cmdq_pkt *handle  = ctx->cmdq_handle;
	int err, request_size;
#ifdef CONFIG_VIDEO_MEDIATEK_MDP_VCU
	dma_addr_t dma_addr;
#endif

	mtk_mdp_dbg(2, "eng=%llx,addr=%llx(%llx),offset=%u,size=%u",
		cmdq->engine_flag, cmdq->ap_buf_addr, cmdq->ap_buf_pa,
		cmdq->cmd_offset, cmdq->cmd_size);

	/* copy cmd buffer */
	handle->cmd_buf_size = 0;
	if (cmdq->cmd_size % CMDQ_INST_SIZE)
		return -EINVAL;

	request_size = cmdq->cmd_size;
	if (unlikely(request_size > handle->buf_size)) {
		request_size = roundup(request_size, PAGE_SIZE);
		err = cmdq_pkt_realloc_cmd_buffer(handle, request_size);
		if (err < 0)
			return err;
	}

#if 0
	memcpy(handle->va_base,
		(void *)(unsigned long)cmdq->ap_buf_addr + cmdq->cmd_offset,
		cmdq->cmd_size);
#endif
	handle->cmd_buf_size = cmdq->cmd_size;

#ifdef CONFIG_VIDEO_MEDIATEK_MDP_VCU
	dma_addr = dma_map_single(&ctx->vpu.pdev->dev,
		(void *)(unsigned long)cmdq->ap_buf_addr,
		cmdq->buf_size, DMA_BIDIRECTIONAL);
	if (dma_mapping_error(&ctx->vpu.pdev->dev, dma_addr) != 0) {
		dev_notice(&ctx->mdp_dev->pdev->dev, "dma map failed\n");
		return -ENOMEM;
	}
#endif

	/* execute cmd */
	err = cmdq_pkt_flush(ctx->mdp_dev->cmdq_client, handle);
	if (unlikely(err < 0))
		dev_err(&ctx->mdp_dev->pdev->dev, "cmdq flush failed!!!\n");

#ifdef CONFIG_VIDEO_MEDIATEK_MDP_VCU
	dma_unmap_single(&ctx->vpu.pdev->dev,
		dma_addr, cmdq->buf_size, DMA_BIDIRECTIONAL);
#endif

	return err;
}

int mtk_mdp_send_pq_msg(struct mtk_mdp_vpu *vpu, uint32_t msg_id)
{
	int err;

	vpu->pq_cfg.msg_id = msg_id;
	vpu->pq_cfg.ipi_id = (uint32_t)vpu->ipi_id;
	vpu->pq_cfg.ap_inst = (uint64_t)(unsigned long)vpu;

	err = mtk_mdp_vpu_send_msg((void *)&vpu->pq_cfg,
				sizeof(vpu->pq_cfg), vpu, vpu->ipi_id);
	if (!err && vpu->failure)
		err = -EINVAL;

	return err;
}

int mtk_mdp_vpu_process(struct mtk_mdp_vpu *vpu)
{
	int err, use_cmdq;
	struct mtk_mdp_ctx *ctx;
	struct mdp_cmdq_info *cmdq;

	err = mtk_mdp_vpu_send_ap_ipi(vpu, (uint32_t)AP_MDP_PROCESS);

	use_cmdq = 0;
	cmdq = &vpu->vsi->cmdq;

	if (err == 0 && cmdq->ap_buf_addr != 0uLL && cmdq->cmd_size != 0u) {
		/* There are command in cmdq buffer, to use cmdq. */
		use_cmdq = 1;

#ifdef CONFIG_VIDEO_MEDIATEK_MDP_FRVC
		if (vpu->initialized == 0)
			use_cmdq = 0;
#endif
	}

	if (use_cmdq) {
		ctx = container_of(vpu, struct mtk_mdp_ctx, vpu);

		err = mtk_mdp_cmdq_exec(ctx, cmdq);
		if (unlikely(err < 0))
			dev_notice(&ctx->mdp_dev->pdev->dev,
				"cmdq execute failed!!!\n");

		/* notify VPU that cmdq instructions executed done */
		/* to do: add status in vpu->vsi->cmdq */
		err = mtk_mdp_vpu_send_ap_ipi(vpu, (uint32_t)AP_MDP_CMDQ_DONE);
	}

	return err;
}
