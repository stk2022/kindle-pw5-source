/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2016 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef _BACKLIGHT_H
#define _BACKLIGHT_H

enum {
	BACKLIGHT_MAX		= 100,
	BACKLIGHT_MIN		= 0,
	BACKLIGHT_OFF		= -1,
	BACKLIGHT_DEFAULT	= -2,
};

struct backlight_ops {
	/**
	 * enable() - Enable a backlight
	 *
	 * @dev:	Backlight device to enable
	 * @return 0 if OK, -ve on error
	 */
	int (*enable)(struct udevice *dev);

	/**
	 * set_brightness - Set brightness
	 *
	 * @dev:	Backlight device to update
	 * @percent:	Brightness value (0 to 100, or BACKLIGHT_... value)
	 * @return 0 if OK, -ve on error
	 */
	int (*set_brightness)(struct udevice *dev, int percent);
	/**
	 * backlight_enable_ramp - enable HW ramp
	 *
	 * @dev:	Backlight device to update
	 * @enable:	Enable HW ramp
	 * @return 0 if OK, -ve on error
	 */
	int (*enable_ramp)(struct udevice *dev, bool ramp);
};

#define backlight_get_ops(dev)	((struct backlight_ops *)(dev)->driver->ops)

/**
 * backlight_enable() - Enable a backlight
 *
 * @dev:	Backlight device to enable
 * @return 0 if OK, -ve on error
 */
int backlight_enable(struct udevice *dev);

/**
 * backlight_set_brightness - Set brightness
 *
 * @dev:	Backlight device to update
 * @percent:	Brightness value (0 to 100, or BACKLIGHT_... value)
 * @return 0 if OK, -ve on error
 */
int backlight_set_brightness(struct udevice *dev, int percent);

/**
 * backlight_enable_ramp - enable hw ramp
 *
 * @dev:	Backlight device to update
 * @enable:	Enable HW ramp
 * @return 0 if OK, -ve on error
 */
int backlight_enable_ramp(struct udevice *dev, bool ramp);

#endif
