/*
 * Copyright 2020 Amazon Technologies, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

/* #define DEBUG */
#include <common.h>
#include <fastboot.h>
#include <fastboot-internal.h>
#ifdef UFBL_FEATURE_IDME
#include <idme.h>
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

int ufbl_fastboot_getvar(char *cmd_parameter, char *var_parameter, char *response)
{
	int ret = 1;
	if (0) {
		/* cannot reach */
	}
#ifdef UFBL_FEATURE_UNLOCK
	else if (!strcmp(cmd_parameter, "unlock_code")) {
		unsigned char unlock_code[UNLOCK_CODE_LEN];
		unsigned int len = sizeof(unlock_code);

		if (!amzn_get_unlock_code(unlock_code, &len))
			fastboot_okay((char*)unlock_code, response);
		else
			fastboot_fail("Error in reading unlock_code", response);
	}
	else if (!strcmp(cmd_parameter, "unlock_status")) {
		fastboot_okay(amzn_target_is_unlocked() ? "true" : "false", response);
	}
#endif
#ifdef UFBL_FEATURE_ONETIME_UNLOCK
	else if (!strcmp(cmd_parameter, "otu_code")) {
		unsigned char otu_unlock_code[ONETIME_UNLOCK_CODE_LEN + 1];
		unsigned int len = sizeof(otu_unlock_code);

		if (!amzn_get_one_tu_code(otu_unlock_code, &len))
			fastboot_okay((char*)otu_unlock_code, response);
		else
			fastboot_fail("Error in reading otu_code", response);
	}
	else if (!strcmp(cmd_parameter, "otu_status")) {
		fastboot_okay(amzn_target_is_onetime_unlocked() ? "true" : "false", response);
	}
#endif
	else {
		ret = 0;
	}
	return ret;
}

int ufbl_fastboot_handle_command(const char *cmd_string, char *response)
{
	int ret = 1;

#ifdef UFBL_FEATURE_FASTBOOT_LOCKDOWN
	if (is_restricted_command_on_locked_hw((unsigned char*)cmd_string)) {
		pr_err("fastboot command %s is restricted on locked hardware\n", cmd_string);
		fastboot_fail("restricted command", response);
		return -1;
	}
#endif

	if (0) {
		/* cannot reach */
	}
#ifdef UFBL_FEATURE_UNLOCK
	else if (!strcmp(cmd_string, "oem relock")) {
		if (idme_update_var_ex("unlock_code", "", 0)) {
			fastboot_fail("oem relock failed", response);
		} else {
			fastboot_okay("relock success", response);
		}
	}
#endif
	else {
		ret = 0;
	}

	return ret;
}

int ufbl_fastboot_handle_flash(const char *cmd_parameter, void *download_buffer,
		u32 download_bytes, char *response)
{
	int ret = 1; /* 0 is not handled, 1 is handled*/

	if (0) {
		/* cannot reach */
	}
#ifdef UFBL_FEATURE_UNLOCK
	else if (!strcmp(cmd_parameter, "unlock")) {
		if (download_bytes != SIGNED_UNLOCK_CODE_LEN) {
			fastboot_fail("Invalid unlock code length", response);
		} else if (amzn_verify_unlock(download_buffer, download_bytes)) {
			fastboot_fail("unlock code error", response);
		} else if (idme_update_var_ex("unlock_code",
					(const char *)download_buffer,
					SIGNED_UNLOCK_CODE_LEN)) {
			fastboot_fail("unlock failed", response);
		} else {
			fastboot_okay("unlock success", response);
		}
	}
#endif
#ifdef UFBL_FEATURE_ONETIME_UNLOCK
	else if (!strcmp(cmd_parameter, "otucert")) {
		if (amzn_set_onetime_unlock_cert((void *)download_buffer,
					download_bytes)) {
			fastboot_fail("Failed to set otucert", response);
		} else {
			fastboot_okay("otucert success", response);
		}
	}
	else if (!strcmp(cmd_parameter, "otucode")) {
		if (amzn_set_onetime_unlock_code((void *)download_buffer,
					download_bytes)) {
			fastboot_fail("Failed to set otucode", response);
		} else {
			fastboot_okay("otucode success", response);
		}
	}
#endif
	else {
		ret = 0;
	}

	return ret;
}

