// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek clock driver for MT8110 SoC
 *
 * Copyright (C) 2019 BayLibre, SAS
 * Author: Jiaguang Zhang <jiaguang.zhang@mediatek.com>
 */

#include <common.h>
#include <dm.h>
#include <asm/io.h>
#include <stdarg.h>
#include <linux/kernel.h>
#include "hwtcon.h"
#include "cmdq.h"

static void cmdq_pkt_instr_encoder(struct cmdq_pkt *pkt, s16 arg_c, s16 arg_b,
				   s16 arg_a, u8 s_op, u8 arg_c_type,
				   u8 arg_b_type, u8 arg_a_type, u8 op)
{
	struct cmdq_instruction *cmdq_inst;

	cmdq_inst = pkt->va_base + pkt->cmd_buf_size;
	cmdq_inst->op = op;
	cmdq_inst->arg_a_type = arg_a_type;
	cmdq_inst->arg_b_type = arg_b_type;
	cmdq_inst->arg_c_type = arg_c_type;
	cmdq_inst->s_op = s_op;
	cmdq_inst->arg_a = arg_a;
	cmdq_inst->arg_b = arg_b;
	cmdq_inst->arg_c = arg_c;
	pkt->cmd_buf_size += CMDQ_INST_SIZE;
}

static bool cmdq_pkt_is_finalized(struct cmdq_pkt *pkt)
{
	u64 *expect_eoc;

	if (pkt->cmd_buf_size < CMDQ_INST_SIZE << 1)
		return false;

	expect_eoc = pkt->va_base + pkt->cmd_buf_size - (CMDQ_INST_SIZE << 1);
	if (*expect_eoc == CMDQ_EOC_CMD)
		return true;

	return false;
}

static int cmdq_pkt_append_command(struct cmdq_pkt *pkt, s16 arg_c, s16 arg_b,
				   s16 arg_a, u8 s_op, u8 arg_c_type,
				   u8 arg_b_type, u8 arg_a_type,
				   enum cmdq_code code)
{
	int err;

	if (!pkt)
		return CMDQ_INVALID_PARAM;
	if (cmdq_pkt_is_finalized(pkt))
		return CMDQ_CMD_FINALIZED;

	if (pkt->cmd_buf_size + CMDQ_INST_SIZE > pkt->buf_size) {
		TCON_ERR("buf_size:%d too small, try to enlarge buf_size", pkt->buf_size);
		return CMDQ_CMD_BUF_TOO_SMALL;
	}
	cmdq_pkt_instr_encoder(pkt, arg_c, arg_b, arg_a, s_op, arg_c_type,
			       arg_b_type, arg_a_type, code);

	return 0;
}


int cmdq_pkt_assign_command(struct cmdq_pkt *pkt, u16 reg_idx, int value)
{
	return cmdq_pkt_append_command(pkt, CMDQ_GET_ARG_C(value),
						   CMDQ_GET_ARG_B(value), reg_idx,
						   CMDQ_LOGIC_ASSIGN, CMDQ_IMMEDIATE_VALUE,
						   CMDQ_IMMEDIATE_VALUE, CMDQ_REG_TYPE,
						   CMDQ_CODE_LOGIC);
}


int cmdq_pkt_store_value(struct cmdq_pkt *pkt, u16 indirect_dst_reg_idx,
			 u32 value, u32 mask)
{
	int err = 0;
	enum cmdq_code op = CMDQ_CODE_WRITE_S;

	if (mask != 0xffffffff) {
		err = cmdq_pkt_append_command(pkt, CMDQ_GET_ARG_C(~mask),
					      CMDQ_GET_ARG_B(~mask), 0, 0, 0, 0,
					      0, CMDQ_CODE_MASK);
		if (err != 0)
			return err;

		op = CMDQ_CODE_WRITE_S_W_MASK;
	}

	return cmdq_pkt_append_command(pkt, CMDQ_GET_ARG_C(value),
				       CMDQ_GET_ARG_B(value),
				       indirect_dst_reg_idx, 0,
				       CMDQ_IMMEDIATE_VALUE,
				       CMDQ_IMMEDIATE_VALUE, CMDQ_REG_TYPE, op);
}

