/*
 * Copyright (C) 2015 MediaTek Inc.
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

#include "cmdq_record.h"
#include "cmdq_helper_ext.h"


void cmdq_unit_test(int test_id)
{
	struct cmdqRecStruct *handle = NULL;

	CMDQ_ERR("enter cmdq unitTest\n");
	cmdqRecCreate(CMDQ_SCENARIO_PIPELINE, &handle);
	cmdqRecReset(handle);
	cmdqRecWrite(handle, 0x1400D000, 0xffffffff, ~0x0);
	cmdqRecFlush(handle);
	cmdqRecDestroy(handle);
	CMDQ_ERR("leave cmdq unitTest\n");
}
