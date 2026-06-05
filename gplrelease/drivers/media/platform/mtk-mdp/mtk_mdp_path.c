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


#include <linux/interrupt.h>
#include <linux/types.h>
#include "mtk_mdp_rdma.h"
#include "mtk_mdp_wrot.h"
#include "mtk_mdp_dither.h"
#include "mtk_mdp_mutex.h"
#include "mtk_mdp_path.h"
#include "mtk_mdp_core.h"
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include "mtk_mdp_base.h"

#define TILE_SIZE			512

struct mtk_mdp_dev *g_mdp_dev;

static irqreturn_t mdp_irq_handler(int irq, void *priv);

static int wrot_irq;

static wait_queue_head_t mtk_mdp_wq;
static u32 mtk_mdp_wakeup_status;
static struct mutex mtk_mdp_mutex;

static wait_queue_head_t mdp_sync_wq;
static atomic_t mdp_sync_work;

void mtk_mdp_sfd_init(struct mtk_mdp_dev *mdp_dev)
{
	int ret;
	struct device_node *node;
	struct platform_device *pdev = NULL;

	g_mdp_dev = mdp_dev;
	mtk_mdp_dbg(3, "mdp_dev=%p", g_mdp_dev);

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8512-mdp-wrot");
	if (node) {
		wrot_irq = of_irq_get(node, 0);
		pdev = of_find_device_by_node(node);

		pr_info("wrot irq %d, %s %s\n",
			wrot_irq, node->full_name,
			pdev ? dev_name(&pdev->dev) : NULL);
		if (!pdev) {
			pr_info("Waiting for device %s\n", node->full_name);
			//return -EPROBE_DEFER;
		}
	} else {
		pr_info("can not find out wrot\n");
		return;
	}

	//irq_set_status_flags(wrot_irq, IRQ_NOAUTOEN);
	ret = devm_request_irq(&pdev->dev, wrot_irq, mdp_irq_handler,
			       IRQF_TRIGGER_NONE,
			       dev_name(&pdev->dev), mdp_dev);
	if (ret) {
		mtk_mdp_err("Failed to request mdp_irq %d (%d)\n",
			wrot_irq, ret);
	//	return -EINVAL;
	}

	config_init();
	mdp_mutex_init();
	rdma_init();
	gamma_init();
	dither_init();
	wrot_init();

	mutex_init(&mtk_mdp_mutex);

	init_waitqueue_head(&mtk_mdp_wq);
	mtk_mdp_wakeup_status = 0;

	init_waitqueue_head(&mdp_sync_wq);
	atomic_set(&mdp_sync_work, 0);
}

void mtk_mdp_sfd_deinit(void)
{
	g_mdp_dev = NULL;
}

void mdp_enable_irq(void)
{
	MDP_LOG_INFO("%s\n", __func__);

	wrot_enable_irq();
}

void mdp_clear_irq(void)
{
	MDP_LOG_INFO("%s\n", __func__);

	wrot_clear_irq();

	MDP_LOG_INFO("%s done\n", __func__);
}

void power_on_mdp(void)
{
	/* enable wrot irq*/
	//enable_irq(wrot_irq);

	mtk_mdp_clock_on(g_mdp_dev);

	mdp_enable_irq();
}

void power_off_mdp(void)
{
	MDP_LOG_INFO("%s\n", __func__);

	/* disable wrot irq*/
	//disable_irq(wrot_irq);

	mtk_mdp_clock_off(g_mdp_dev);
}

static irqreturn_t mdp_irq_handler(int irq, void *priv)
{
	MDP_LOG_INFO("%s\n", __func__);

	wake_up(&mtk_mdp_wq);
	mtk_mdp_wakeup_status = 1;

	mdp_clear_irq();

	rdma_stop();
	gamma_stop();
	dither_stop();
	wrot_stop();

	//power_off_mdp();

	MDP_LOG_INFO("%s done\n", __func__);

	return IRQ_HANDLED;
}

int mdp_get_sync_work_nr(void)
{
	return atomic_read(&mdp_sync_work);
}

int mdp_wait_sync_work(void)
{
	int ret;

	MDP_LOG_INFO("%s waiting sync work\n", __func__);

	ret = wait_event_interruptible_timeout(mdp_sync_wq,
					      (atomic_read(&mdp_sync_work) ==
					       0),
					      HZ);

	MDP_LOG_INFO("%s wait sync work done\n", __func__);

	return ret;
}

void mdp_update_cmap_lut(u16 *gamma_lut, u32 len)
{
	mutex_lock(&mtk_mdp_mutex);

	gamma_update_cmap_lut(gamma_lut, len);

	mutex_unlock(&mtk_mdp_mutex);
}
EXPORT_SYMBOL(mdp_update_cmap_lut);

void mdp_sync_config(struct mtk_mdp_frame_config *config)
{
	int ret;

	ret = mdp_wait_latency_work();
	if (!ret) {
		int cnt = mdp_get_latency_work_nr();

		pr_info("mdp latency work nr %d\n", cnt);
	}

	mutex_lock(&mtk_mdp_mutex);

	atomic_inc(&mdp_sync_work);
	gamma_gen_table(config->gamma_flag);
	mdp_tile_config(config);
	atomic_dec(&mdp_sync_work);
	wake_up(&mdp_sync_wq);

	mutex_unlock(&mtk_mdp_mutex);
}

void mdp_tile_config(struct mtk_mdp_frame_config *config)
{
	u32 i, j;
	u32 h_step, v_step;
	u32 dst_width = config->dst_width;
	u32 dst_height = config->dst_height;
	u32 dst_pitch = config->dst_pitch;
	dma_addr_t dst_addr = config->dst_addr;
	u32 crop_x = config->crop_x;
	u32 crop_y = config->crop_y;
	u32 crop_w = config->crop_w;
	u32 crop_h = config->crop_h;
	u32 rotate = config->rotate;
	u32 dst_h_offset = 0;
	u32 dst_v_offset = 0;
	u32 org_crop_x = config->crop_x;
	u32 org_crop_y = config->crop_y;
	u32 org_crop_w = config->crop_w;
	u32 org_crop_h = config->crop_h;

	config->src_width = crop_w;
	config->src_height = crop_h;

	if ((rotate == 0) ||
	    (rotate == 180) ||
	    ((crop_w <= TILE_SIZE) && (crop_h <= TILE_SIZE)) ||
	    mdp_test) {
		config_path(config);
		return;
	}

	h_step = (org_crop_w + TILE_SIZE - 1)/TILE_SIZE;
	v_step = (org_crop_h + TILE_SIZE - 1)/TILE_SIZE;
	/* v_step = 1; */

	/* v tile */
	for (j = 0; j < v_step; j++) {
		if ((j < (v_step -1)) && (TILE_SIZE < org_crop_h))
			crop_h = TILE_SIZE;
		else if (j * TILE_SIZE < org_crop_h)
			crop_h = org_crop_h - j * TILE_SIZE;

		crop_y = org_crop_y + j * TILE_SIZE;

		if (rotate == 90) {
			if (org_crop_h > ((j + 1) * TILE_SIZE))
				dst_h_offset = org_crop_h - (j + 1) * TILE_SIZE;
			else
				dst_h_offset = 0;
		} else {
			dst_h_offset = j * TILE_SIZE;
		}
		/*
		 * crop_h = org_crop_h;
		 * crop_y = org_crop_y;
		 * dst_h_offset = 0;
		 */
		/* h tile */
		for (i = 0; i < h_step; i++) {
			if ((i < (h_step -1)) && (TILE_SIZE < org_crop_w))
				crop_w = TILE_SIZE;
			else if (i * TILE_SIZE < org_crop_w)
				crop_w = org_crop_w - i * TILE_SIZE;

			crop_x = org_crop_x + i * TILE_SIZE;

			if (rotate == 90) {
				dst_v_offset = i * TILE_SIZE * dst_pitch;
			} else {
				/* 270 */
				if (org_crop_w >= (i * TILE_SIZE))
					dst_v_offset = (org_crop_w -
						        i * TILE_SIZE - 1) *
						        dst_pitch;
				else
					dst_v_offset = 0;
			}

			dst_width = crop_w;
			dst_height = crop_h;

			config->dst_addr = dst_addr + dst_v_offset +
					   dst_h_offset;

			MDP_LOG_DEBUG("[%d %d] offset 0x%x dst_addr 0x%x\n",
				      j, i, dst_h_offset, config->dst_addr);

			config->src_width = crop_w;
			config->src_height = crop_h;

			config->crop_w = crop_w;
			config->crop_h = crop_h;
			config->crop_x = crop_x;
			config->crop_y = crop_y;

			config->dst_width = dst_width;
			config->dst_height= dst_height;

			config_path(config);
		}
	}
}

void config_path(struct mtk_mdp_frame_config *config)
{
	u32 src_width = config->src_width;
	u32 src_height = config->src_height;
	u32 src_pitch = config->src_pitch;
	u32 src_fmt = config->src_fmt;
	dma_addr_t src_addr = config->src_addr;
	u32 dst_width = config->dst_width;
	u32 dst_height = config->dst_height;
	u32 dst_pitch = config->dst_pitch;
	u32 dst_fmt = config->dst_fmt;
	dma_addr_t dst_addr = config->dst_addr;
	u32 crop_x = config->crop_x;
	u32 crop_y = config->crop_y;
	u32 crop_w = config->crop_w;
	u32 crop_h = config->crop_h;
	u32 rotate = config->rotate;
	u32 invert = config->invert;
	u32 dth_en = config->dth_en;
	u32 dth_algo = config->dth_algo;
	u32 gamma_flag = config->gamma_flag;
	u32 crop_type = 1;
	int ret;

	MDP_LOG_INFO("%s 1\n", __func__);

	MDP_LOG_INFO("%s 2\n", __func__);

	power_on_mdp();

	reset_mutex();
	connect_path();

	rdma_reset();
	gamma_reset();
	dither_reset();
	wrot_reset();

	rdma_start();
	gamma_start();
	dither_start();
	wrot_start();

	config_mutex();
	rdma_config(src_width, src_height, src_pitch, src_fmt, src_addr,
		    crop_type, crop_x, crop_y, crop_w, crop_h);
	gamma_config(src_width, src_height, invert, gamma_flag);
	dither_config(src_width, src_height, dth_en, dth_algo,
		      src_fmt, dst_fmt);

	wrot_config(dst_width, dst_height, dst_pitch, dst_fmt, dst_addr,
		    rotate, crop_type, crop_x, crop_y, crop_w, crop_h);

	trigger_path();

	MDP_LOG_INFO("%s done 1\n", __func__);

	MDP_LOG_INFO("%s done 2\n", __func__);

	ret = wait_event_interruptible_timeout(mtk_mdp_wq,
					       mtk_mdp_wakeup_status, HZ/10);
	mtk_mdp_wakeup_status = 0;

	power_off_mdp();
	//disable_irq(wrot_irq);

	MDP_LOG_INFO("%s done ret %d HZ %d %d\n", __func__, ret, HZ, HZ/10);
}

