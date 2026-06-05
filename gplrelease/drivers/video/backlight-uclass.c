// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2016 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <dm.h>
#include <backlight.h>

int backlight_enable(struct udevice *dev)
{
	const struct backlight_ops *ops = backlight_get_ops(dev);

	if (!ops->enable)
		return -ENOSYS;

	return ops->enable(dev);
}

int backlight_set_brightness(struct udevice *dev, int percent)
{
	const struct backlight_ops *ops = backlight_get_ops(dev);

	if (!ops->set_brightness)
		return -ENOSYS;

	return ops->set_brightness(dev, percent);
}

int backlight_enable_ramp(struct udevice *dev, bool ramp)
{
	const struct backlight_ops *ops = backlight_get_ops(dev);

	if (!ops->enable_ramp)
		return -ENOSYS;

	return ops->enable_ramp(dev, ramp);
}

UCLASS_DRIVER(backlight) = {
	.id		= UCLASS_PANEL_BACKLIGHT,
	.name		= "backlight",
};
