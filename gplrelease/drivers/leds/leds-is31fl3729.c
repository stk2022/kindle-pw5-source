/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */


#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

/* Software Shutdown bit in Shutdown Register */
#define IS31FL3729_SHUTDOWN_SSD_ENABLE  0
#define IS31FL3729_SHUTDOWN_SSD_DISABLE BIT(0)

static unsigned int  gpio_led_en;

struct is31fl3729_priv;
struct is31fl3729_led_data {
	struct led_classdev cdev;
	u8 channel; /* 1-based, max priv->cdef->channels */
	struct is31fl3729_priv *priv;
};

struct is31fl3729_priv {
	const struct is31fl3729_chipdef *cdef;
	struct i2c_client *client;
	unsigned int num_leds;
	struct is31fl3729_led_data leds[0];
};

/**
 * struct is31fl3729_chipdef - chip-specific attributes
 * @channels            : Number of LED channels
 * @config_reg          : address of configuration register
 * @global_control_reg  : address of Global Control register
 * @reset_reg           : address of Reset register
 * @pwm_register_base   : address of first PWM register
 * @scal_register_base  : address of first scaling register
 * @pwm_freq_register   : address of pwm frequency register
 */
struct is31fl3729_chipdef {
	u8	channels;
	u8	config_reg;
	u8	global_control_reg;
	u8	reset_reg;
	u8	pwm_register_base;
	u8	scal_register_base;
	u8	pwm_freq_register;
};

static const struct is31fl3729_chipdef is31fl3729_cdef = {
	.channels				= 96, /* 6SW*16CS */
	.config_reg				= 0xa0,
	.global_control_reg	    = 0xa1,
	.reset_reg				= 0xcf,
	.pwm_register_base		= 0x01,
	.scal_register_base		= 0x90,
	.pwm_freq_register	    = 0xb2,
};

static int is31fl3729_write(struct is31fl3729_priv *priv, u8 reg, u8 val)
{
	int ret;

	dev_dbg(&priv->client->dev, "writing register 0x%02X=0x%02X", reg, val);

	ret =  i2c_smbus_write_byte_data(priv->client, reg, val);
	if (ret) {
		dev_info(&priv->client->dev,
			"register write to 0x%02X failed (error %d)",
			reg, ret);
	}
	return ret;
}

static int is31fl3729_brightness_set(struct led_classdev *led_cdev,
				     enum led_brightness brightness)
{
	const struct is31fl3729_led_data *led_data =
		container_of(led_cdev, struct is31fl3729_led_data, cdev);
	const struct is31fl3729_chipdef *cdef = led_data->priv->cdef;
	u8 pwm_register_offset;
	int ret;

	dev_info(led_cdev->dev, "%s: %d\n", __func__, brightness);

	/* NOTE: led_data->channel is 1-based */
	pwm_register_offset = led_data->channel - 1;

	ret = is31fl3729_write(led_data->priv,
			       cdef->pwm_register_base + pwm_register_offset,
			       brightness);
	if (ret)
		return ret;

	return 0;
}

static int is31fl3729_reset_regs(struct is31fl3729_priv *priv)
{
	const struct is31fl3729_chipdef *cdef = priv->cdef;
	int ret;

	ret = is31fl3729_write(priv, cdef->reset_reg, 0); /* 0xAE */
	if (ret)
		return ret;

	return 0;
}

static int is31fl3729_software_shutdown(struct is31fl3729_priv *priv,
					bool enable)
{
	const struct is31fl3729_chipdef *cdef = priv->cdef;
	int ret;

	u8 value = enable ? IS31FL3729_SHUTDOWN_SSD_ENABLE :
			    IS31FL3729_SHUTDOWN_SSD_DISABLE;
	ret = is31fl3729_write(priv, cdef->config_reg, value);
	if (ret)
		return ret;

	return 0;
}

static int is31fl3729_init_regs(struct is31fl3729_priv *priv)
{
	const struct is31fl3729_chipdef *cdef = priv->cdef;
	int i, ret;

	ret = is31fl3729_reset_regs(priv);
	ret = is31fl3729_write(priv, cdef->config_reg, 0x31); /* 00110001 */
	ret = is31fl3729_write(priv, cdef->global_control_reg, 0x40);

	for (i = 0; i < 16; i++)
		is31fl3729_write(priv, cdef->scal_register_base + i, 0xff);
	/* ret = is31fl3729_write(priv, cdef->pwm_freq_register, 0x1); */
	if (ret)
		return ret;
	return 0;
}

static inline size_t sizeof_is31fl3729_priv(int num_leds)
{
	return sizeof(struct is31fl3729_priv) +
		      (sizeof(struct is31fl3729_led_data) * num_leds);
}

static int is31fl3729_parse_child_dt(const struct device *dev,
				     const struct device_node *child,
				     struct is31fl3729_led_data *led_data)
{
	struct led_classdev *cdev = &led_data->cdev;
	int ret = 0;
	u32 reg;

	if (of_property_read_string(child, "label", &cdev->name))
		cdev->name = child->name;

	ret = of_property_read_u32(child, "reg", &reg);
	if (ret || reg < 1 || reg > led_data->priv->cdef->channels) {
		dev_info(dev,
			"Child node %s does not have a valid reg property\n",
			child->full_name);
		return -EINVAL;
	}
	led_data->channel = reg;

	of_property_read_string(child, "linux,default-trigger",
				&cdev->default_trigger);

	cdev->brightness_set_blocking = is31fl3729_brightness_set;

	return 0;
}

static struct is31fl3729_led_data *is31fl3729_find_led_data(
					struct is31fl3729_priv *priv,
					u8 channel)
{
	size_t i;

	for (i = 0; i < priv->num_leds; i++) {
		if (priv->leds[i].channel == channel)
			return &priv->leds[i];
	}

	return NULL;
}

static int is31fl3729_parse_dt(struct device *dev,
			       struct is31fl3729_priv *priv)
{
	struct device_node *child;
	int ret = 0;

	for_each_child_of_node(dev->of_node, child) {
		struct is31fl3729_led_data *led_data =
			&priv->leds[priv->num_leds];
		const struct is31fl3729_led_data *other_led_data;

		led_data->priv = priv;

		ret = is31fl3729_parse_child_dt(dev, child, led_data);
		if (ret)
			goto err;

		/* Detect if channel is already in use by another child */
		other_led_data = is31fl3729_find_led_data(priv,
							  led_data->channel);
		if (other_led_data) {
			dev_info(dev,
				"%s and %s both attempting to use channel %d\n",
				led_data->cdev.name,
				other_led_data->cdev.name,
				led_data->channel);
			goto err;
		}

		ret = devm_led_classdev_register(dev, &led_data->cdev);
		if (ret) {
			dev_info(dev, "failed to register PWM led for %s: %d\n",
				led_data->cdev.name, ret);
			goto err;
		}

		priv->num_leds++;
	}

	return 0;

err:
	of_node_put(child);
	return ret;
}

static const struct of_device_id of_is31fl3729_match[] = {
	{ .compatible = "issi,is31fl3729", .data = &is31fl3729_cdef, },
	{},
};

MODULE_DEVICE_TABLE(of, of_is31fl3729_match);

static int is31fl3729_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	const struct is31fl3729_chipdef *cdef;
	const struct of_device_id *of_dev_id;
	struct device *dev = &client->dev;
	struct is31fl3729_priv *priv;
	int count;
	int ret = 0;

	of_dev_id = of_match_device(of_is31fl3729_match, dev);
	if (!of_dev_id)
		return -EINVAL;

	cdef = of_dev_id->data;

	count = of_get_child_count(dev->of_node);
	if (!count)
		return -EINVAL;

	priv = devm_kzalloc(dev, sizeof_is31fl3729_priv(count),
			    GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->client = client;
	priv->cdef = cdef;
	i2c_set_clientdata(client, priv);

	gpio_led_en = of_get_named_gpio(dev->of_node, "gpio_en", 0);
	ret = gpio_request(gpio_led_en, "IS31FL3729_DRV_EN");
	if (ret)
		pr_notice("gpio led en = 0x%x fail\n", gpio_led_en);
	gpio_direction_output(gpio_led_en, 1);
	gpio_set_value(gpio_led_en, 1);

	ret = is31fl3729_init_regs(priv);
	if (ret)
		return ret;

	ret = is31fl3729_parse_dt(dev, priv);
	if (ret)
		return ret;

	return 0;
}

static int is31fl3729_remove(struct i2c_client *client)
{
	struct is31fl3729_priv *priv = i2c_get_clientdata(client);

	return is31fl3729_reset_regs(priv);
}

/*
 * i2c-core (and modalias) requires that id_table be properly filled,
 * even though it is not used for DeviceTree based instantiation.
 */
static const struct i2c_device_id is31fl3729_id[] = {
	{ "is31fl3729" },
	{},
};

MODULE_DEVICE_TABLE(i2c, is31fl3729_id);

static struct i2c_driver is31fl3729_driver = {
	.driver = {
		.name	= "is31fl3729",
		.of_match_table = of_is31fl3729_match,
	},
	.probe		= is31fl3729_probe,
	.remove		= is31fl3729_remove,
	.id_table	= is31fl3729_id,
};

module_i2c_driver(is31fl3729_driver);

MODULE_AUTHOR("Crystal Guo <crystal.guo@mediatek.com>");
MODULE_DESCRIPTION("ISSI IS31FL3729 LED driver");
MODULE_LICENSE("GPL v2");
