#ifndef __CMDQ_H__
#define __CMDQ_H__

#include "hwtcon_def.h"
/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2018 MediaTek Inc.
 * Author: Jiaguang Zhang <jiaguang.zhang@mediatek.com>
 */

/*
** cmdq error status.
*/
enum CMDQ_ERR_ENUM {
	CMDQ_OK = 0,
	CMDQ_ALLOC_MEM_FAIL = -1,
	CMDQ_INVALID_PARAM = -2,
	CMDQ_CMD_FINALIZED = -3,
	CMDQ_CMD_BUF_TOO_SMALL = -3,
	CMDQ_EVENT_NUM_ERR = -4,
	CMDQ_TASK_EXEC_TIMEOUT = -5,
};

/** @ingroup IP_group_gce_external_enum
 * @brief gce operation code enum.
 */
enum cmdq_code {
	CMDQ_CODE_MASK = 0x02,
	CMDQ_CODE_POLL = 0x08,
	CMDQ_CODE_JUMP = 0x10,
	CMDQ_CODE_WFE = 0x20,
	CMDQ_CODE_READ_S = 0x80,
	CMDQ_CODE_WRITE_S = 0x90,
	CMDQ_CODE_WRITE_S_W_MASK = 0x91,
	CMDQ_CODE_LOGIC = 0xa0,
	CMDQ_CODE_JUMP_C_RELATIVE = 0xb1,
	CMDQ_CODE_EOC = 0x40,
};

/** @ingroup IP_group_gce_external_enum
 * @brief gce logic enum.
 */
enum CMDQ_LOGIC_ENUM {
	CMDQ_LOGIC_ASSIGN = 0,
	CMDQ_LOGIC_ADD = 1,
	CMDQ_LOGIC_SUBTRACT = 2,
	CMDQ_LOGIC_MULTIPLY = 3,
	CMDQ_LOGIC_XOR = 8,
	CMDQ_LOGIC_NOT = 9,
	CMDQ_LOGIC_OR = 10,
	CMDQ_LOGIC_AND = 11,
	CMDQ_LOGIC_LEFT_SHIFT = 12,
	CMDQ_LOGIC_RIGHT_SHIFT = 13
};

/** @ingroup IP_group_gce_external_def
 * @brief gce instruction size definition.
 * @{
 */
#define CMDQ_INST_SIZE		8
#define CMDQ_IMMEDIATE_VALUE			0
#define CMDQ_REG_TYPE				1
#define CMDQ_SYNC_TOKEN_UPDATE			0x68
#define CMDQ_SUBSYS_SHIFT			16
#define CMDQ_OP_CODE_SHIFT			24
#define CMDQ_EOC_IRQ_EN				BIT(0)

#define CMDQ_EOC_CMD				((u64)((CMDQ_CODE_EOC \
						<< CMDQ_OP_CODE_SHIFT)) << 32 \
						| CMDQ_EOC_IRQ_EN)

/** get the argument b from 32bits value */
#define CMDQ_GET_ARG_B(arg)			(((arg) & GENMASK(31, 16)) \
						>> 16)
/** get the argument c from 32bits value */
#define CMDQ_GET_ARG_C(arg)			((arg) & GENMASK(15, 0))



/** conbine the argument b and c to a 32bits balue */
#define CMDQ_GET_32B_VALUE(arg_b, arg_c)	((u32)((arg_b) << 16) | (arg_c))
/** get the register index prefix from type */
#define CMDQ_REG_IDX_PREFIX(type)		((type) ? "" : "Reg Index ")
/** get operand index or value */
#define CMDQ_OPERAND_GET_IDX_VALUE(operand)	((operand)->reg ? \
						(operand)->idx : \
						(operand)->value)

/** @ingroup IP_group_gce_internal_struct
* @brief cmdq 64bits instruction structure.
*/
struct cmdq_instruction {
	s16 arg_c:16;
	s16 arg_b:16;
	s16 arg_a:16;
	u8 s_op:5;
	u8 arg_c_type:1;
	u8 arg_b_type:1;
	u8 arg_a_type:1;
	u8 op:8;
};


struct cmdq_pkt {
	/** virtual base address */
	void			*va_base;
	/** command occupied size */
	size_t			cmd_buf_size;
	/** real buffer size */
	size_t			buf_size;
};

#define GCE_SPR0		0
#define GCE_SPR1		1
#define GCE_SPR2		2
#define GCE_SPR3		3

#endif /* endof __CMDQ_H__ */
