// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2019 ROHM Semiconductors

#include <linux/device.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/mfd/rohm-bd71828.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>

#define BD71828_LED_TO_DATA(l) ((l)->id == ID_GREEN_LED ? \
	container_of((l), struct bd71828_leds, green) : \
	container_of((l), struct bd71828_leds, amber))

enum {
	ID_GREEN_LED,
	ID_AMBER_LED,
	ID_NMBR_OF,
};

struct bd71828_led {
	int id;
	struct led_classdev l;
	u8 force_mask;
};

struct bd71828_leds {
	struct rohm_regmap_dev *bd71828;
	struct bd71828_led green;
	struct bd71828_led amber;
};

static int bd71828_led_brightness_set(
	struct led_classdev *led_cdev, enum led_brightness value)
{
	struct bd71828_led *l = container_of(led_cdev, struct bd71828_led, l);
	struct bd71828_leds *data;
	unsigned int val = BD71828_LED_OFF;

	data = BD71828_LED_TO_DATA(l);
	if (value != LED_OFF)
		val = BD71828_LED_ON;

	return regmap_update_bits(data->bd71828->regmap, BD71828_REG_LED_CTRL,
			    l->force_mask, val);
}

static int bd71828_led_probe(struct platform_device *pdev)
{
	struct rohm_regmap_dev *bd71828;
	struct bd71828_leds *l;
	struct bd71828_led *g, *a;
	static const char *GNAME = "bd71828-green-led";
	static const char *ANAME = "bd71828-amber-led";
	int ret;

	pr_info("bd71828 LED driver probed\n");

	bd71828 = dev_get_drvdata(pdev->dev.parent);
	l = devm_kzalloc(&pdev->dev, sizeof(*l), GFP_KERNEL);
	if (!l)
		return -ENOMEM;
	l->bd71828 = bd71828;
	a = &l->amber;
	g = &l->green;
	a->id = ID_AMBER_LED;
	g->id = ID_GREEN_LED;
	a->force_mask = BD71828_MASK_LED_AMBER;
	g->force_mask = BD71828_MASK_LED_GREEN;

	a->l.name = ANAME;
	g->l.name = GNAME;
	a->l.brightness_set_blocking = bd71828_led_brightness_set;
	g->l.brightness_set_blocking = bd71828_led_brightness_set;

	ret = devm_led_classdev_register(&pdev->dev, &g->l);
	if (ret)
		return ret;

	return devm_led_classdev_register(&pdev->dev, &a->l);
}

static struct platform_driver bd71828_led_driver = {
	.driver = {
		.name  = "bd71828-led",
	},
	.probe  = bd71828_led_probe,
};

module_platform_driver(bd71828_led_driver);

MODULE_AUTHOR("Matti Vaittinen <matti.vaittinen@fi.rohmeurope.com>");
MODULE_DESCRIPTION("ROHM BD71828 LED driver");
MODULE_LICENSE("GPL");
