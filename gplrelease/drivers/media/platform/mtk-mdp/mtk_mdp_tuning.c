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

#ifdef CONFIG_DEBUG_FS

#include <linux/dma-mapping.h>
#include "mtk_mdp_vpu.h"

#define MDP_TUNING_UNIT                       (512)

#define MDP_TUNING_ST_NONE                    (0)
#define MDP_TUNING_ST_INIT                    (1)
#define MDP_TUNING_ST_UPDATE                  (2)

/* 0 */
#define MDP_TUNING_OFFSET_START               (0)

/* 1 */
#define MDP_TUNING_OFFSET_FLAG_DS_SWREG \
	(MDP_TUNING_OFFSET_START + 1)
#define MDP_TUNING_OFFSET_FLAG_DS_INPUT \
	(MDP_TUNING_OFFSET_FLAG_DS_SWREG + 1)
#define MDP_TUNING_OFFSET_FLAG_DS_OUTPUT \
	(MDP_TUNING_OFFSET_FLAG_DS_INPUT + 1)
#define MDP_TUNING_OFFSET_FLAG_DC_SWREG \
	(MDP_TUNING_OFFSET_FLAG_DS_OUTPUT + 1)
#define MDP_TUNING_OFFSET_FLAG_DC_INPUT \
	(MDP_TUNING_OFFSET_FLAG_DC_SWREG + 1)
#define MDP_TUNING_OFFSET_FLAG_DC_OUTPUT \
	(MDP_TUNING_OFFSET_FLAG_DC_INPUT + 1)

/* 512 */
#define MDP_TUNING_OFFSET_ST_DS_SWREG \
	(MDP_TUNING_OFFSET_START + 512)
#define MDP_TUNING_OFFSET_ST_DS_INPUT \
	(MDP_TUNING_OFFSET_ST_DS_SWREG + 512)
#define MDP_TUNING_OFFSET_ST_DS_OUTPUT \
	(MDP_TUNING_OFFSET_ST_DS_INPUT + 512)
#define MDP_TUNING_OFFSET_ST_DC_SWREG \
	(MDP_TUNING_OFFSET_ST_DS_OUTPUT + 512)
#define MDP_TUNING_OFFSET_ST_DC_INPUT \
	(MDP_TUNING_OFFSET_ST_DC_SWREG + 512)
#define MDP_TUNING_OFFSET_ST_DC_OUTPUT \
	(MDP_TUNING_OFFSET_ST_DC_INPUT + 512)

/* (512 * 4 * 2) */
#define MDP_TUNING_OFFSET_DS_SWREG \
	(MDP_TUNING_OFFSET_START + (512 * 4 * 2))
#define MDP_TUNING_OFFSET_DS_INPUT \
	(MDP_TUNING_OFFSET_DS_SWREG + (512 * 4))
#define MDP_TUNING_OFFSET_DS_OUTPUT \
	(MDP_TUNING_OFFSET_DS_INPUT + (512 * 4))
#define MDP_TUNING_OFFSET_DC_SWREG \
	(MDP_TUNING_OFFSET_DS_OUTPUT + (512 * 4))
#define MDP_TUNING_OFFSET_DC_INPUT \
	(MDP_TUNING_OFFSET_DC_SWREG + (512 * 4))
#define MDP_TUNING_OFFSET_DC_OUTPUT \
	(MDP_TUNING_OFFSET_DC_INPUT + (512 * 4))
#define MDP_TUNING_OFFSET_TDSHP_HW_REG \
	(MDP_TUNING_OFFSET_DC_OUTPUT + (512 * 4))

#define MDP_TUNING_OFFSET_MAX    (200*1024)

static unsigned long g_mdp_tuning_buf_va;

static unsigned int mtk_mdp_tuning_get_offset(unsigned int module)
{
	unsigned int offset = 0xffffffff;

	switch (module) {
	case 0x110:
		offset = MDP_TUNING_OFFSET_DS_SWREG;
		break;
	case 0x111:
		offset = MDP_TUNING_OFFSET_DS_INPUT;
		break;
	case 0x112:
		offset = MDP_TUNING_OFFSET_DS_OUTPUT;
		break;
	case 0x120:
		offset = MDP_TUNING_OFFSET_DC_SWREG;
		break;
	case 0x121:
		offset = MDP_TUNING_OFFSET_DC_INPUT;
		break;
	case 0x122:
		offset = MDP_TUNING_OFFSET_DC_OUTPUT;
		break;
	default:
		break;
	}

	return offset;
}

static unsigned int mtk_mdp_tuning_get_flag(unsigned int module)
{
	unsigned int offset = 0xffffffff;

	switch (module) {
	case 0x110:
		offset = MDP_TUNING_OFFSET_FLAG_DS_SWREG;
		break;
	case 0x111:
		offset = MDP_TUNING_OFFSET_FLAG_DS_INPUT;
		break;
	case 0x112:
		offset = MDP_TUNING_OFFSET_FLAG_DS_OUTPUT;
		break;
	case 0x120:
		offset = MDP_TUNING_OFFSET_FLAG_DC_SWREG;
		break;
	case 0x121:
		offset = MDP_TUNING_OFFSET_FLAG_DC_INPUT;
		break;
	case 0x122:
		offset = MDP_TUNING_OFFSET_FLAG_DC_OUTPUT;
		break;
	default:
		break;
	}

	return offset;
}

static unsigned int mtk_mdp_tuning_get_st(unsigned int module)
{
	unsigned int offset = 0xffffffff;

	switch (module) {
	case 0x110:
		offset = MDP_TUNING_OFFSET_ST_DS_SWREG;
		break;
	case 0x111:
		offset = MDP_TUNING_OFFSET_ST_DS_INPUT;
		break;
	case 0x112:
		offset = MDP_TUNING_OFFSET_ST_DS_OUTPUT;
		break;
	case 0x120:
		offset = MDP_TUNING_OFFSET_ST_DC_SWREG;
		break;
	case 0x121:
		offset = MDP_TUNING_OFFSET_ST_DC_INPUT;
		break;
	case 0x122:
		offset = MDP_TUNING_OFFSET_ST_DC_OUTPUT;
		break;
	default:
		break;
	}

	return offset;
}

static void mtk_mdp_tuning_flag_set(unsigned int offset, unsigned char val)
{
	unsigned char *buf;

	buf = (unsigned char *)(g_mdp_tuning_buf_va + offset);
	*buf = val;
}

void mtk_mdp_tuning_read(unsigned long addr)
{
	unsigned int module;
	unsigned int field;
	unsigned int val;
	unsigned int *buf;

	module = (addr >> 16) & 0xffff;
	field = (addr & 0xffff);

	buf = (unsigned int *)
		(g_mdp_tuning_buf_va +
		mtk_mdp_tuning_get_offset(module) + field);
	val = *buf;

	pr_info("[tuning] read addr[0x%lx] buf[%p] val[0x%x]\n",
		addr, buf, val);

	pr_info("r:0x%08lx = 0x%08x\n", addr, val);
}
EXPORT_SYMBOL(mtk_mdp_tuning_read);

void mtk_mdp_tuning_write(unsigned long addr, unsigned int val)
{
	unsigned int module;
	unsigned int field;
	unsigned int *buf;

	module = (addr >> 16) & 0xffff;
	field = (addr & 0xffff);

	buf = (unsigned int *)
		(g_mdp_tuning_buf_va +
		mtk_mdp_tuning_get_offset(module) + field);
	*buf = val;

	mtk_mdp_tuning_flag_set(
		mtk_mdp_tuning_get_st(module) + (field / 4),
		MDP_TUNING_ST_UPDATE);

	mtk_mdp_tuning_flag_set(
		mtk_mdp_tuning_get_flag(module),
		MDP_TUNING_ST_UPDATE);

	pr_info("[tuning] write addr[0x%lx] buf[%p] val[0x%x]\n",
		addr, buf, val);
}
EXPORT_SYMBOL(mtk_mdp_tuning_write);

void mtk_mdp_vpu_handle_tuning_ack(struct mdp_ipi_comm_ack *msg)
{
	struct mtk_mdp_vpu *vpu = (struct mtk_mdp_vpu *)
					(unsigned long)msg->ap_inst;

	/* mapping tuning buffer address to kernel virtual address */
	if (msg->vpu_inst_addr != 0uLL) {
		vpu->tuning_buf_va = (uint64_t)(unsigned long)
			vpu_mapping_dm_addr(vpu->pdev, msg->vpu_inst_addr);
		vpu->tuning_buf_pa = __pa(vpu->tuning_buf_va);
		g_mdp_tuning_buf_va = vpu->tuning_buf_va;
		pr_info("[tuning] %s[%d] map va[0x%llx] pa[0x%llx]\n",
			__func__, __LINE__,
			vpu->tuning_buf_va, vpu->tuning_buf_pa);
	}
}

#endif
