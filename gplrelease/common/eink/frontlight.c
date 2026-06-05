/*
 * Control frontlight devices
 *
 * Copyright (c) 2020 Amazon.com Inc
 *
 * Licensed under the GPL-2 or later.
 */

#include <common.h>
#include <command.h>
#include <errno.h>
#include <dm.h>
#include <backlight.h>
#ifdef UFBL_FEATURE_IDME
#include <idme.h>
#endif

const char* __weak board_frontlight_device(void) { return NULL; };
int __weak board_frontlight_brightness(void) { return 0; };

void frontlight_enable(bool enable, bool ramp)
{
	struct udevice *dev;
	char bootmode[8] = {0};
	int brightness = board_frontlight_brightness();

	if (enable && brightness <= 0) {
		return;
	}

	if (uclass_get_device_by_name(UCLASS_PANEL_BACKLIGHT,
				board_frontlight_device(), &dev)) {
		debug("No frontlight found\n");
		return;
	}

	if (!enable) {
		/* turn off frontlight */
		backlight_enable_ramp(dev, false);
		backlight_set_brightness(dev, 0);
		return;
	}

#ifdef UFBL_FEATURE_IDME
	/* check bootmode before enable frontlight */
	memset(bootmode, 0, sizeof(bootmode));
	if (idme_get_var_external("bootmode", bootmode, sizeof(bootmode)) ||
			!strcmp(bootmode, "ota")) {
		debug("ota update. no frontlight\n");
		return;
	}
#endif

	if (backlight_enable_ramp(dev, ramp) == 0) {
		/* Use HW ramp to save SW time */
		backlight_set_brightness(dev, brightness);
	} else {
		/* No HW ramp available, use SW ramp */
		int target = brightness;
		int brightness = ramp ? 0 : target;
		do {
			brightness += target / 20;
			if (brightness > target)
				brightness = target;
			backlight_set_brightness(dev, brightness);
			udelay(5000);
		} while (brightness < target);
	}

	puts("fl\n");
}

static struct udevice *current_device;

static int do_frontlight_dev(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int ret;
	unsigned long index;
	struct udevice *dev;

	if (argc == 1) {
		dev = current_device;
	} else if (argc == 2) {
		if (!strict_strtoul(argv[1], 10, &index)) {
			ret = uclass_get_device(UCLASS_PANEL_BACKLIGHT, index, &dev);
		} else {
			ret = uclass_get_device_by_name(UCLASS_PANEL_BACKLIGHT, argv[1], &dev);
		}
		if (ret) {
			printf("Cannot find frontlight device %s\n", argv[1]);
			return CMD_RET_FAILURE;
		}
		current_device = dev;
	} else {
		return CMD_RET_USAGE;
	}

	if (dev)
		printf("'%s' is current frontlight device\n", dev->name);
	else
		printf("No current frontlight device set\n");

	return CMD_RET_SUCCESS;
}

static int do_frontlight_set_brightness(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int brightness;
	int ret;

	if (argc != 2) {
		return CMD_RET_USAGE;
	}

	if (!current_device) {
		printf("No current frontlight device set\n");
		return CMD_RET_FAILURE;
	}

	brightness = simple_strtoul(argv[1], NULL, 10);
	ret = backlight_set_brightness(current_device, brightness);
	if (ret) {
		printf("Fail to set brightness to device '%s': errno=%d\n", current_device->name, ret);
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

static cmd_tbl_t cmd_frontlight[] = {
	U_BOOT_CMD_MKENT(dev, 2, 0, do_frontlight_dev, "", ""),
	U_BOOT_CMD_MKENT(set, 2, 0, do_frontlight_set_brightness, "", ""),
};

static int do_frontlight_ops(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	cmd_tbl_t *cp;

	cp = find_cmd_tbl(argv[1], cmd_frontlight, ARRAY_SIZE(cmd_frontlight));

	/* Drop the frontlight command */
	argc--;
	argv++;

	if (cp == NULL || argc > cp->maxargs)
		return CMD_RET_USAGE;
	if (flag == CMD_FLAG_REPEAT && !cmd_is_repeatable(cp))
		return CMD_RET_SUCCESS;
	return cp->cmd(cmdtp, flag, argc, argv);
}

U_BOOT_CMD(fl, 4, 0, do_frontlight_ops,
		"frontlight system",
		"dev [<index> | <name>]\n"
		"    - show or set current frontlight device\n"
		"fl set <brightness>\n"
		"    - set brightness of current frontlight device");
