/*
 * Copyright 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */

#define SBIOS_CALL_MAGIC_NUMBER 0x35350909

enum sbios_call_cmd {
	SBIOS_CALL_GET_CSD,
	SBIOS_CALL_GET_EXT_CSD,
	SBIOS_CALL_SAVE_REGS,
	SBIOS_CALL_RESTORE_REGS,
	SBIOS_CALL_SUSPEND,
	SBIOS_CALL_RESUME,
};

struct sbios_call_data {
	u32 magic;
	enum sbios_call_cmd cmd;
	void *data;
	u32 size;
};
