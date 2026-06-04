/*
 * Copyright 2020 Amazon Technologies, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

/* #define DEBUG */
#include <common.h>
#include <fastboot.h>
#include <fastboot-internal.h>
#include <ufbl.h>
#ifdef UFBL_FEATURE_IDME
#include <idme.h>
#endif
#ifdef UFBL_FEATURE_SECURE_BOOT
#include <amzn_secure_boot.h>
#endif
#ifdef UFBL_FEATURE_UNLOCK
#include <amzn_unlock.h>
#endif
#ifdef UFBL_FEATURE_FASTBOOT_LOCKDOWN
#include <amzn_fastboot_lockdown.h>
#endif
#ifdef UFBL_FEATURE_ONETIME_UNLOCK
#include <amzn_onetime_unlock.h>
#endif

/* Functions need to implement for different boards */
__weak bool board_is_production_device(void)
{
	return false;
}

__weak bool board_is_secure_cpu(void)
{
	return false;
}

__weak int board_get_hw_serial(uint32_t *serial, int count)
{
	return 0;
}

__weak const unsigned char *board_get_unlock_key(unsigned int *key_len)
{
	*key_len = 0;
	return NULL;
}
__weak const unsigned char *board_get_onetime_unlock_root_pubkey(unsigned int *key_len)
{
	*key_len = 0;
	return NULL;
}

/* proto type */
int is_locked_production_device(void);

bool ufbl_is_secure_cpu(void)
{
	return board_is_secure_cpu();
}

bool ufbl_is_production_device(void)
{
#ifdef UFBL_FEATURE_SECURE_BOOT
	return (amzn_target_device_type() == AMZN_PRODUCTION_DEVICE);
#else
	return false;
#endif
}

bool ufbl_is_locked_production_device(void)
{
#ifdef UFBL_FEATURE_FASTBOOT_LOCKDOWN
	return is_locked_production_device();
#else
	return false;
#endif
}

void ufbl_lock_cli(void)
{
	while(ufbl_is_locked_production_device()) {
		printf("CLI locked, enter fastboot mode\n");
		run_command("fastboot", 0);
		run_command("boot", 0);
	}
}

/* Implement of UFBL functions */
#ifdef UFBL_FEATURE_SECURE_BOOT
int amzn_target_device_type(void)
{
	return board_is_production_device() ? AMZN_PRODUCTION_DEVICE : AMZN_ENGINEERING_DEVICE;
}
#endif

#ifdef UFBL_FEATURE_UNLOCK
#define UNLOCK_CODE_SERIAL_COUNT 4
int amzn_get_unlock_code(unsigned char *code, unsigned int *len)
{
	uint32_t serial[UNLOCK_CODE_SERIAL_COUNT] = {0};
	const int code_len = (UNLOCK_CODE_SERIAL_COUNT<<1) + 2 + 1; /* two bytes for "0x" and one byte for null character */
	int out_len = 0, i;

	if (!code || !len || *len < code_len)
		return -1;

	/* Use HW serial */
	if (board_get_hw_serial(serial, UNLOCK_CODE_SERIAL_COUNT)) {
		pr_err("%s: failed to get hw serial\n", __func__);
		return -1;
	}

	/* Add prefix first */
	out_len += sprintf((char*)code, "0x");
	for (i=0; i<UNLOCK_CODE_SERIAL_COUNT; i++) {
		out_len += sprintf((char*)code + out_len, "%08x", serial[i]);
	}
	*len = (unsigned)out_len;

	return 0;

}

const unsigned char *amzn_get_unlock_key(unsigned int *key_len)
{
	return board_get_unlock_key(key_len);
}
#endif

#ifdef UFBL_FEATURE_ONETIME_UNLOCK
#define ONETIME_UNLOCK_CODE_SERIAL_COUNT 2
#define RANDOM_BYTES_SIZE (ONETIME_UNLOCK_CODE_LEN + 3) / 4 * 3

static unsigned char one_tu_code[ONETIME_UNLOCK_CODE_LEN+1] = {0};

int amzn_get_one_tu_code(unsigned char *code, unsigned int *len)
{
	static unsigned char code_generated = 0;
	char entropy[ONETIME_UNLOCK_CODE_SERIAL_COUNT*2+2+1];
	uint8_t random_bytes[RANDOM_BYTES_SIZE] = {0};
	unsigned int out_len = sizeof(one_tu_code);
	uint32_t serial[ONETIME_UNLOCK_CODE_SERIAL_COUNT] = {0};
	int entropy_index = 0, i;

	if (!code || !len || *len < ONETIME_UNLOCK_CODE_LEN)
		return -1;

	/* If code is already generated skip generating new one */
	if (code_generated)
		goto skip;

	/* Use HUID serial and get_timer as entropy into PRNG */
	if (board_get_hw_serial(serial, ONETIME_UNLOCK_CODE_SERIAL_COUNT)) {
		pr_err("%s: failed to get hw serial\n", __func__);
		return -1;
	}

	/* Add timer first */
	entropy_index += sprintf(entropy, "%08lx", get_timer(0));
	for (i=0; i<ONETIME_UNLOCK_CODE_SERIAL_COUNT; i++) {
		entropy_index += sprintf(entropy + entropy_index, "%08x", serial[i]);
	}

	/* Generate random bytes */
	if (amzn_get_onetime_random_number((unsigned char*)entropy, sizeof(entropy), random_bytes,
				sizeof(random_bytes))) {
		pr_err("%s: random number generation failed\n", __func__);
		return -1;
	}

	/**
	 * amzn_get_onetime_random_number will return a binary string which
	 * cannot be returned via fastboot so use base64 to encode it.
	 */
	if (amzn_onetime_unlock_b64_encode(random_bytes, sizeof(random_bytes),
				one_tu_code, &out_len)) {
		pr_err("%s: onetime unlock code encode failed\n", __func__);
		return -1;
	}

	code_generated = 1;

skip:
	memcpy(code, one_tu_code, ONETIME_UNLOCK_CODE_LEN);
	*len = ONETIME_UNLOCK_CODE_LEN;
	return 0;

}

int amzn_get_onetime_unlock_root_pubkey(const unsigned char **key, unsigned int *key_len)
{
	*key = board_get_onetime_unlock_root_pubkey(key_len);
	return 0;
}
#endif
