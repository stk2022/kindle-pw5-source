/*
 * Copyright 2020 Amazon Technologies, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

/* #define DEBUG */
#include <common.h>
#include <config.h>
#include <errno.h>
#include <asm/arch/boot_args.h>
#ifdef UFBL_FEATURE_IDME
#include <idme.h>
#endif
#ifdef CONFIG_UFBL
#include <ufbl.h>
#endif

static void ft_fixup_bootargs(void *fdt)
{
	int  chosen_offset;
	char *bootargs_chosen = NULL;
	char bootargs[512];
	char *str;
	int  str_offset = 0;
	int  err = 0;
	bool secure_cpu = false, production = false, locked = false;

#ifdef CONFIG_UFBL
	secure_cpu = ufbl_is_secure_cpu();
	production = ufbl_is_production_device();
	locked     = ufbl_is_locked_production_device();
#endif

	/* find bootargs in "/chosen" node. */
	chosen_offset = fdt_find_or_add_subnode(fdt, 0, "chosen");
	if (chosen_offset >= 0) {
		bootargs_chosen = strdup(fdt_getprop(fdt, chosen_offset, "bootargs", NULL));
	} else {
		debug("WARNING: No chosen found in dts\n");
	}

	/* Add prepend bootargs */
	if (!locked) {
		str = env_get("bootargs_prepend");
		if (str && *str != 0)
			str_offset += sprintf(bootargs+str_offset, "%s ", str);
	}

	/* Add original bootargs from chosen */
	if (bootargs_chosen && *bootargs_chosen != 0) {
		bootargs_chosen = strdup(bootargs_chosen);
		do {
			str = strsep(&bootargs_chosen, " \t\n");
			if (!str || *str == 0)
				continue;

			if (locked) {
				/* remove earlycon */
				if (!strncmp(str, "earlycon=", 9))
					continue;
				/* replace console with ttynull */
				if (!strncmp(str, "console=", 8)) {
					str_offset += sprintf(bootargs+str_offset, "console=ttynull ");
					continue;
				}
			}
			str_offset += sprintf(bootargs+str_offset, "%s ", str);
		} while (str != NULL);
		free(bootargs_chosen);
	}

	/* Add rpmb status */
	str_offset += sprintf(bootargs+str_offset, "androidboot.rpmb_state=%d ", !!get_rpmb_key_status_from_boot_args());

	/* Add secure status */
	printf("   secure_cpu: %d, production: %d, unlocked: %d\n", secure_cpu, production, !locked);
	str_offset += sprintf(bootargs+str_offset, "secure_cpu=%d androidboot.secure_cpu=%d androidboot.prod=%d androidboot.unlocked_kernel=%s ",
			secure_cpu, secure_cpu, production, !locked ? "true" : "false");

	/* Add append bootargs */
	if (!locked) {
		str = env_get("bootargs_append");
		if (str && *str != 0)
			str_offset += sprintf(bootargs+str_offset, "%s ", str);
	}

	/* Always append quiet for locked device */
	if (locked)
		sprintf(bootargs+str_offset, "quiet");

	err = fdt_setprop(fdt, chosen_offset, "bootargs", bootargs, strlen(bootargs) + 1);
	if (err < 0) {
		printf("WARNING: could not set bootargs %s.\n",
				fdt_strerror(err));
	}
}

static void ft_fixup_idme(void *blob)
{
	// reopen fdt in place with larger size for IDME
	fdt_open_into(blob, blob, fdt_totalsize(blob)+0x4000);
	printf("   %s to initialize idme device tree\n", \
			idme_device_tree_initialize(blob) ? "Fail" : "OK");
}

int ft_board_setup(void *blob, bd_t *bd)
{
	ft_fixup_bootargs(blob);

#ifdef UFBL_FEATURE_IDME
	ft_fixup_idme(blob);
#endif

#ifdef CONFIG_HIBERNATION_FALCON
	extern void ft_fixup_quickboot(void *blob);
	ft_fixup_quickboot(blob);
#endif

	return 0;
}
