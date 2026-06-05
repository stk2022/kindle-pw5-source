// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2020, Amazon.com
 */

#include <common.h>
#include <dm.h>
#include <backlight.h>
#include <asm/gpio.h>
#include <i2c.h>

#define LM3692X_REV     0x0
#define LM3692X_RESET       0x1
#define LM3692X_EN      0x10
#define LM3692X_BRT_CTRL    0x11
#define LM3692X_PWM_CTRL    0x12
#define LM3692X_BOOST_CTRL  0x13
#define LM3692X_AUTO_FREQ_HI    0x15
#define LM3692X_AUTO_FREQ_LO    0x16
#define LM3692X_BL_ADJ_THRESH   0x17
#define LM3692X_BRT_LSB     0x18
#define LM3692X_BRT_MSB     0x19
#define LM3692X_FAULT_CTRL  0x1e
#define LM3692X_FAULT_FLAGS 0x1f

#define LM3692X_SW_RESET    BIT(0)
#define LM3692X_DEVICE_EN   BIT(0)
#define LM3692X_LED1_EN     BIT(1)
#define LM3692X_LED2_EN     BIT(2)
#define LM36923_LED3_EN     BIT(3)
#define LM3692X_ENABLE_MASK (LM3692X_DEVICE_EN | LM3692X_LED1_EN | \
		LM3692X_LED2_EN | LM36923_LED3_EN)

/* Brightness Control Bits */
#define LM3692X_BL_ADJ_POL  BIT(0)
#define LM3692X_RAMP_RATE_125us 0x00
#define LM3692X_RAMP_RATE_250us BIT(1)
#define LM3692X_RAMP_RATE_500us BIT(2)
#define LM3692X_RAMP_RATE_1ms   (BIT(1) | BIT(2))
#define LM3692X_RAMP_RATE_2ms   BIT(3)
#define LM3692X_RAMP_RATE_4ms   (BIT(3) | BIT(1))
#define LM3692X_RAMP_RATE_8ms   (BIT(2) | BIT(3))
#define LM3692X_RAMP_RATE_16ms  (BIT(1) | BIT(2) | BIT(3))
#define LM3692X_RAMP_EN     BIT(4)
#define LM3692X_BRHT_MODE_REG   0x00
#define LM3692X_BRHT_MODE_PWM   BIT(5)
#define LM3692X_BRHT_MODE_MULTI_RAMP BIT(6)
#define LM3692X_BRHT_MODE_RAMP_MULTI (BIT(5) | BIT(6))
#define LM3692X_MAP_MODE_EXP    BIT(7)

/* PWM Register Bits */
#define LM3692X_PWM_FILTER_100  BIT(0)
#define LM3692X_PWM_FILTER_150  BIT(1)
#define LM3692X_PWM_FILTER_200  (BIT(0) | BIT(1))
#define LM3692X_PWM_HYSTER_1LSB BIT(2)
#define LM3692X_PWM_HYSTER_2LSB BIT(3)
#define LM3692X_PWM_HYSTER_3LSB (BIT(3) | BIT(2))
#define LM3692X_PWM_HYSTER_4LSB BIT(4)
#define LM3692X_PWM_HYSTER_5LSB (BIT(4) | BIT(2))
#define LM3692X_PWM_HYSTER_6LSB (BIT(4) | BIT(3))
#define LM3692X_PWM_POLARITY    BIT(5)
#define LM3692X_PWM_SAMP_4MHZ   BIT(6)
#define LM3692X_PWM_SAMP_24MHZ  BIT(7)

/* Boost Control Bits */
#define LM3692X_OCP_PROT_1A BIT(0)
#define LM3692X_OCP_PROT_1_25A  BIT(1)
#define LM3692X_OCP_PROT_1_5A   (BIT(0) | BIT(1))
#define LM3692X_OVP_21V     BIT(2)
#define LM3692X_OVP_25V     BIT(3)
#define LM3692X_OVP_29V     (BIT(2) | BIT(3))
#define LM3692X_MIN_IND_22UH    BIT(4)
#define LM3692X_BOOST_SW_1MHZ   BIT(5)
#define LM3692X_BOOST_SW_NO_SHIFT   BIT(6)

/* Fault Control Bits */
#define LM3692X_FAULT_CTRL_OVP BIT(0)
#define LM3692X_FAULT_CTRL_OCP BIT(1)
#define LM3692X_FAULT_CTRL_TSD BIT(2)
#define LM3692X_FAULT_CTRL_OPEN BIT(3)

/* Fault Flag Bits */
#define LM3692X_FAULT_FLAG_OVP BIT(0)
#define LM3692X_FAULT_FLAG_OCP BIT(1)
#define LM3692X_FAULT_FLAG_TSD BIT(2)
#define LM3692X_FAULT_FLAG_SHRT BIT(3)
#define LM3692X_FAULT_FLAG_OPEN BIT(4)

enum lm3692x_device_type {
	LM3692X_TYPE_LM36922H,
	LM3692X_TYPE_LM36923,
};

/* Max Brightness */
#define MAX_BRIGHTNESS_11BIT        2047

#define BACKLIGHT_MAX_NAME_SIZE     64

struct lm3692x_backlight_priv {
	struct gpio_desc gpio;
};

static int lm3692x_read_reg(struct udevice *dev, int reg, u8 *val)
{
	return dm_i2c_read(dev, reg, val, 1);
}

static int lm3692x_write_reg(struct udevice *dev, int reg, u8 val)
{
	return dm_i2c_write(dev, reg, &val, 1);
}

static int lm3692x_backlight_set_brightness(struct udevice *dev, int brightness)
{
	int ret;
	int bl_brightness_msb;
	int bl_brightness_lsb;

	if (brightness > MAX_BRIGHTNESS_11BIT || brightness < 0) {
		return -ERANGE;
	}

	bl_brightness_msb = (brightness >> 3);
	bl_brightness_lsb = (brightness & 0x7);

	ret = lm3692x_write_reg(dev, LM3692X_BRT_LSB, bl_brightness_lsb);
	if (ret) {
		debug("%s: Cannot write LSB\n", dev->name);
		goto out;
	}

	ret = lm3692x_write_reg(dev, LM3692X_BRT_MSB, bl_brightness_msb);
	if (ret) {
		debug("%s: Cannot write MSB\n", dev->name);
	}

out:
	return ret;
}

static int lm3692x_backlight_enable_ramp(struct udevice *dev, bool enable)
{
	u8 reg;
	int ret;

	ret = lm3692x_read_reg(dev, LM3692X_BRT_CTRL, &reg);
	if (ret) {
		debug("%s: Cannot read LM3692X_BRT_CTRL\n", dev->name);
		return ret;
	}

	if (enable)
		reg |= LM3692X_RAMP_EN;
	else
		reg &= ~LM3692X_RAMP_EN;

	return lm3692x_write_reg(dev, LM3692X_BRT_CTRL, reg);
}

static int lm3692x_backlight_probe(struct udevice *dev)
{
	struct lm3692x_backlight_priv *priv = dev_get_priv(dev);
	int ret;
	u8 led_enable;

	ret = gpio_request_by_name(dev, "enable-gpios", 0, &priv->gpio,
			GPIOD_IS_OUT);
	if (ret) {
		debug("%s: cannot get GPIO: ret=%d\n", dev->name, ret);
		goto out_gpio;
	}

	/* hardware enable */
	dm_gpio_set_value(&priv->gpio, 1);

	ret = lm3692x_write_reg(dev, LM3692X_BRT_CTRL, 0x00);
	if (ret)
		goto out;

	ret = lm3692x_write_reg(dev, LM3692X_EN, LM3692X_DEVICE_EN);
	if (ret)
		goto out;

	ret = lm3692x_write_reg(dev, LM3692X_BRT_LSB, 0);
	if (ret)
		goto out;

	ret = lm3692x_write_reg(dev, LM3692X_BRT_MSB, 0);
	if (ret)
		goto out;

	ret = lm3692x_write_reg(dev, LM3692X_PWM_CTRL,
			LM3692X_PWM_FILTER_100 | LM3692X_PWM_SAMP_24MHZ);
	if (ret)
		goto out;

	ret = lm3692x_write_reg(dev, LM3692X_BRT_CTRL,
			LM3692X_MAP_MODE_EXP |
			LM3692X_BRHT_MODE_REG |
			LM3692X_BL_ADJ_POL |
			LM3692X_RAMP_RATE_125us);
	if (ret)
		goto out;

	ret = lm3692x_write_reg(dev, LM3692X_AUTO_FREQ_HI, 0x00);
	if (ret)
		goto out;

	ret = lm3692x_write_reg(dev, LM3692X_AUTO_FREQ_LO, 0x00);
	if (ret)
		goto out;

	ret = lm3692x_write_reg(dev, LM3692X_BL_ADJ_THRESH, 0x00);
	if (ret)
		goto out;

	ret = lm3692x_write_reg(dev, LM3692X_BOOST_CTRL,
			LM3692X_OCP_PROT_1_5A | LM3692X_OVP_29V |
			LM3692X_BOOST_SW_1MHZ | LM3692X_BOOST_SW_NO_SHIFT);
	if (ret)
		goto out;

	if (dev_get_driver_data(dev) == LM3692X_TYPE_LM36923)
		led_enable = LM3692X_LED1_EN | LM3692X_LED2_EN | LM36923_LED3_EN;
	else
		led_enable = LM3692X_LED1_EN | LM3692X_LED2_EN;

	ret = lm3692x_write_reg(dev, LM3692X_EN, led_enable | LM3692X_DEVICE_EN);

	return ret;
out:
	debug("%s: error init device. err=%d\n", dev->name, ret);
	dm_gpio_set_value(&priv->gpio, 0);
	dm_gpio_set_dir_flags(&priv->gpio, GPIOD_IS_IN);
	dm_gpio_free(dev, &priv->gpio);
out_gpio:
	return ret;
}

static const struct backlight_ops lm3692x_backlight_ops = {
	.set_brightness = lm3692x_backlight_set_brightness,
	.enable_ramp = lm3692x_backlight_enable_ramp,
};

static const struct udevice_id lm3692x_backlight_ids[] = {
	{ .compatible = "ti,lm36923",  .data = LM3692X_TYPE_LM36923 },
	{ .compatible = "ti,lm36922h", .data = LM3692X_TYPE_LM36922H },
	{ }
};

U_BOOT_DRIVER(lm3692x_backlight) = {
	.name		= "lm3692x_backlight",
	.id			= UCLASS_PANEL_BACKLIGHT,
	.of_match	= lm3692x_backlight_ids,
	.ops		= &lm3692x_backlight_ops,
	.probe		= lm3692x_backlight_probe,
	.priv_auto_alloc_size	= sizeof(struct lm3692x_backlight_priv),
};
