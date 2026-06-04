// SPDX-License-Identifier: GPL-2.0
/*
 * Boot args for MediaTek MT8512 SoC
 *
 * Copyright (C) 2019 MediaTek Inc.
 * Author: Mingming Lee <mingming.lee@mediatek.com>
 */

#include <common.h>
#include <asm/arch/boot_args.h>

DECLARE_GLOBAL_DATA_PTR;

BOOT_ARGUMENT_T g_boot_arg;

extern uint32_t BOOT_ARGUMENT_LOCATION;

const int efuse_len_info[] = {
	4,
	2,
	2,
	4,
	4,
	32,
	4,
	4,
	4,
	4,
	1,
	1,
	4,
	4,
	4,
	4,
	1,
	1,
	1,
	1,
	4,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	4,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	32,
	32,
	32,
	4,
	4,
	4,
	4,
	1,
	1,
	1,
	1,
	1,
	4,
	4, /*index 60*/
	4,
	1,
	16,
	24,
};
const int efuse_len_info_len = sizeof(efuse_len_info)/sizeof(efuse_len_info[0]);

void hex_dump(const char *prefix, unsigned char *buf, int len)
{
   int i;

   if (!buf || !len)
		return;

   printf("%s:\n", prefix);
   for (i = 0; i < len; i++) {
		if (i != 0 && !(i % 16))
			printf("\n");
		printf("%02x", *(buf + i));
	}
	printf("\n");
}

int efuse_read_index(unsigned int index, unsigned char *data)
{
	unsigned char i = 0;
	unsigned int efuse_data_offset = 0;

	if (index >= efuse_len_info_len) {
		return -ERANGE;
	}
	BOOT_ARGUMENT_T *g_boot_args = (BOOT_ARGUMENT_T *)BOOT_ARGUMENT_LOCATION;
	if (g_boot_args->magic_number_begin != BOOT_ARGUMENT_MAGIC
	   || g_boot_args->magic_number_end != BOOT_ARGUMENT_MAGIC) {
			pr_err("%s:boot arg magic error,plse check the flow.\n", __func__);
			return -1;
		}
    for (i = 0 ; i < index; i++)
		efuse_data_offset = efuse_data_offset + efuse_len_info[i];
    memcpy(data, g_boot_args->efuse_data + efuse_data_offset, efuse_len_info[index]);

	return 0;
}

int efuse_dump(unsigned int start, unsigned int count)
{
	unsigned char i = 0;
	unsigned char data[64];
	char msg[64];
	int ret;

	if (count == 0)
		count = efuse_len_info_len;

	if (start+count > efuse_len_info_len)
		count = efuse_len_info_len - start;

	for (i=0 ; i<count; i++) {
		ret = efuse_read_index(i+start, data);
		if (ret)
			return ret;
		snprintf(msg, sizeof(msg), "efuse index %d len %d", i+start, efuse_len_info[i+start]);
		hex_dump(msg, data, efuse_len_info[i+start]);
	}
	return 0;
}

int get_dramsize_from_boot_args(void)
{

	debug("BOOT_ARGUMENT_LOCATION =0x%x \n", BOOT_ARGUMENT_LOCATION);
	BOOT_ARGUMENT_T *g_boot_args = (BOOT_ARGUMENT_T *)BOOT_ARGUMENT_LOCATION;
	if (g_boot_args->magic_number_begin != BOOT_ARGUMENT_MAGIC
	   || g_boot_args->magic_number_end != BOOT_ARGUMENT_MAGIC) {
			pr_err("%s:boot arg magic error,plse check the flow.\n", __func__);
			return -1;
		}
	//hex_dump("BOOT_ARGUMENT_LOCATION hex:", (unsigned char *)BOOT_ARGUMENT_LOCATION, sizeof(BOOT_ARGUMENT_T));

	return g_boot_args->dram_size;
}

unsigned int get_wdt_status_from_boot_args(void)
{

        debug("BOOT_ARGUMENT_LOCATION =0x%x \n", BOOT_ARGUMENT_LOCATION);
        BOOT_ARGUMENT_T *g_boot_args = (BOOT_ARGUMENT_T *)BOOT_ARGUMENT_LOCATION;
        if (g_boot_args->magic_number_begin != BOOT_ARGUMENT_MAGIC
           || g_boot_args->magic_number_end != BOOT_ARGUMENT_MAGIC) {
                pr_err("%s:boot arg magic error, please check the flow.\n", __func__);
                return 0;
        }

        return g_boot_args->wdt_sta;
}

unsigned int get_rpmb_key_status_from_boot_args(void)
{
	BOOT_ARGUMENT_T *g_boot_args = (BOOT_ARGUMENT_T *)BOOT_ARGUMENT_LOCATION;

	return g_boot_args->rpmb_key_status;
}

unsigned int get_boot_reason_from_boot_args(void) {
	BOOT_ARGUMENT_T *g_boot_args = (BOOT_ARGUMENT_T *)BOOT_ARGUMENT_LOCATION;

	return g_boot_args->boot_reason;
}
