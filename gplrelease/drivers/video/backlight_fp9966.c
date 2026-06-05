// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2020, Amazon.com
 */

#include <common.h>
#include <dm.h>
#include <backlight.h>
#include <asm/gpio.h>
#include <dt-bindings/gpio/gpio.h>
#include <i2c.h>

#define FP9966_MODEL	0
#define FP9966_SHUTDOWN_PROTECT_DISABLE 0               /* Fault handling disable*/
#define FP9966_SHUTDOWN_PROTECT_ENABLE  1               /* Fault handling enable*/

/*register setting:*/
#define FP9966_FUNCTION_SET_00			0x00
#define FP9966_RAMP_CTRL_01             0x01
#define FP9966_OCP_02                   0x02
#define FP9966_OVP_03                   0x03
#define FP9966_BRIGHT_L_CTRL1_04        0x04
#define FP9966_BRIGHT_H_CTRL1_05        0x05
#define FP9966_BRIGHT_L_CTRL2_06        0x06
#define FP9966_BRIGHT_H_CTRL2_07        0x07
#define FP9966_AUTO_FREQ_H_THRD1_08     0x08
#define FP9966_AUTO_FREQ_L_THRD1_09     0x09
#define FP9966_AUTO_FREQ_H_THRD2_0A     0x0A
#define FP9966_AUTO_FREQ_L_THRD2_0B     0x0B
#define FP9966_CH1_BK_LIGHT_THRD_0C     0x0C
#define FP9966_CH2_BK_LIGHT_THRD_0D     0x0D
#define FP9966_FAULT_CTRL_0E            0x0E
#define FP9966_FAULT_FLAG_0F            0x0F
#define FP9966_OTP_CTRL_ENABLE_10       0x10
#define FP9966_OTP_FAULT_FLAG_11        0x11

/*reg_00 function definition */
#define FP9966_CH2_EN               BIT(7)
#define FP9966_LED2_EN              BIT(6)
#define FP9966_LED2_MAP_MODE        BIT(5)
#define FP9966_LED2_MAP_BLADJ       BIT(4)
#define FP9966_CH1_EN               BIT(3)
#define FP9966_LED1_EN              BIT(2)
#define FP9966_LED1_MAP_MODE        BIT(1)
#define FP9966_LED1_MAP_BLADJ       BIT(0)
#define FP9966_ENABLE_MASK	( FP9966_CH2_EN | FP9966_LED2_EN |  FP9966_CH1_EN | FP9966_LED1_EN )
#define FP9966_CH1_EN_MASK	( FP9966_CH1_EN | FP9966_LED1_EN )
#define FP9966_CH2_EN_MASK	( FP9966_CH2_EN | FP9966_LED2_EN )

/*reg_0x01:  Ramp Step Bits */
#define FP9966_CH2_RAMPEN           BIT(7)
#define FP9966_CH2_RAMP_RATE_125us       0x00
#define FP9966_CH2_RAMP_RATE_250us       0x01
#define FP9966_CH2_RAMP_RATE_500us       0x02
#define FP9966_CH2_RAMP_RATE_1ms         0x03
#define FP9966_CH2_RAMP_RATE_2ms         0x04
#define FP9966_CH2_RAMP_RATE_4ms         0x05
#define FP9966_CH2_RAMP_RATE_8ms         0x06
#define FP9966_CH2_RAMP_RATE_16ms        0x07
#define FP9966_CH1_RAMPEN           BIT(3)
#define FP9966_CH1_RAMP_RATE_125us       0x00
#define FP9966_CH1_RAMP_RATE_250us       0x01
#define FP9966_CH1_RAMP_RATE_500us       0x02
#define FP9966_CH1_RAMP_RATE_1ms         0x03
#define FP9966_CH1_RAMP_RATE_2ms         0x04
#define FP9966_CH1_RAMP_RATE_4ms         0x05
#define FP9966_CH1_RAMP_RATE_8ms         0x06
#define FP9966_CH1_RAMP_RATE_16ms        0x07

/* reg_02: OCP setting */
#define FP9966_CH2_OCP_500mA        0x00
#define FP9966_CH2_OCP_750mA        0x01
#define FP9966_CH2_OCP_1000mA       0x02
#define FP9966_CH2_OCP_1250mA       0x03
#define FP9966_CH1_OCP_500mA        0x00
#define FP9966_CH1_OCP_750mA        0x01
#define FP9966_CH1_OCP_1000mA       0x02
#define FP9966_CH1_OCP_1250mA       0x03

/* reg_03: OVP setting */
#define FP9966_CH2_OVP_9V           0x00
#define FP9966_CH2_OVP_11V          0x01
#define FP9966_CH2_OVP_13V          0x02
#define FP9966_CH2_OVP_15V          0x03
#define FP9966_CH2_OVP_17V          0x04
#define FP9966_CH2_OVP_19V          0x05
#define FP9966_CH2_OVP_21V          0x06
#define FP9966_CH2_OVP_23V          0x07
#define FP9966_CH2_OVP_25V          0x08
#define FP9966_CH2_OVP_27V          0x09
#define FP9966_CH2_OVP_29V          0x0a
#define FP9966_CH2_OVP_31V          0x0b
#define FP9966_CH2_OVP_33V          0x0c

#define FP9966_CH1_OVP_9V           0x00
#define FP9966_CH1_OVP_11V          0x01
#define FP9966_CH1_OVP_13V          0x02
#define FP9966_CH1_OVP_15V          0x03
#define FP9966_CH1_OVP_17V          0x04
#define FP9966_CH1_OVP_19V          0x05
#define FP9966_CH1_OVP_21V          0x06
#define FP9966_CH1_OVP_23V          0x07
#define FP9966_CH1_OVP_25V          0x08
#define FP9966_CH1_OVP_27V          0x09
#define FP9966_CH1_OVP_29V          0x0a
#define FP9966_CH1_OVP_31V          0x0b
#define FP9966_CH1_OVP_33V          0x0c

/* reg_09: auto frequency threshold */

/* reg_0E: Fault Control Bits */
#define FP9966_CH2_OVP_SD           BIT(7)
#define FP9966_CH2_OPEN_SD          BIT(6)
#define FP9966_CH2_SHORT_SD         BIT(5)
#define FP9966_CH2_OCP_SD           BIT(4)
#define FP9966_CH1_OVP_SD           BIT(3)
#define FP9966_CH1_OPEN_SD          BIT(2)
#define FP9966_CH1_SHORT_SD         BIT(1)
#define FP9966_CH1_OCP_SD           BIT(0)

#define FP9966_OTP_SD               BIT(0)

/* reg_0F: Fault Flag Bits */
#define FP9966_CH2_OVP_FLAG         BIT(7)
#define FP9966_CH2_OPEN_FLAG        BIT(6)
#define FP9966_CH2_SHORT_FLAG       BIT(5)
#define FP9966_CH2_OCP_FLAG         BIT(4)
#define FP9966_CH1_OVP_FLAG         BIT(3)
#define FP9966_CH1_OPEN_FLAG        BIT(2)
#define FP9966_CH1_SHORT_FLAG       BIT(1)
#define FP9966_CH1_OCP_FLAG         BIT(0)

/* reg_0x11: OTP fault flag */
#define FP9966_OTP_FLAG             BIT(0)

/* Max Brightness */
#define MAX_BRIGHTNESS_11BIT		2047

#define BACKLIGHT_MAX_NAME_SIZE		64

#define FP9966_LED_COUNT		2

struct fp9966_backlight_priv {
	struct gpio_desc gpio_en;
	struct gpio_desc gpio_adj;
	uint32_t index;
};

static int fp9966_enabled = 0;


static int fp9966_write_reg(struct udevice *dev, uint reg, const uint8_t val)
{
	if (reg < 0 || reg > FP9966_OTP_FAULT_FLAG_11)
		return -ERANGE;

	return dm_i2c_write(dev, reg, &val, 1);
}

static int fp9966_backlight_enable_ramp(struct udevice *dev, bool enable)
{
	int ret;

	// Ramp Control Register
	if (enable) {
		ret = fp9966_write_reg(dev, FP9966_RAMP_CTRL_01, 0x88);
	} else {
		ret = fp9966_write_reg(dev, FP9966_RAMP_CTRL_01, 0x0);
	}

	return ret;
}

static int fp9966_backlight_set_brightness(struct udevice *dev, int brightness)
{
	struct fp9966_backlight_priv *priv = dev_get_priv(dev);
	int ret;
	int bl_brightness_msb;
	int bl_brightness_lsb;

	if ((priv->index >= FP9966_LED_COUNT) || (brightness > MAX_BRIGHTNESS_11BIT) ||
			(brightness < 0)) {
		return -ERANGE;
	}

	bl_brightness_msb = (brightness >> 3);
	bl_brightness_lsb = (brightness & 0x7);

	ret = fp9966_write_reg(dev, FP9966_BRIGHT_L_CTRL1_04 + priv->index * 2, bl_brightness_lsb);
	if (ret) {
		debug("%s: Cannot write LSB\n", dev->name);
		goto out;
	}

	ret = fp9966_write_reg(dev, FP9966_BRIGHT_H_CTRL1_05 + priv->index * 2, bl_brightness_msb);
	if (ret) {
		debug("%s: Cannot write MSB\n", dev->name);
	}

out:
	return ret;
}

static int fp9966_backlight_init(struct udevice *dev)
{
	struct dm_i2c_chip *chip = dev_get_parent_platdata(dev);
	int ret;

	/* check if hardware available */
	ret = dm_i2c_probe(dev_get_parent(dev), chip->chip_addr, 0, &dev);
	if (ret) {
		debug("%s: failed to find hardware: ret=%d\n", dev->name, ret);
		return ret;
	}

	// Disable Channel Register
	fp9966_write_reg(dev, FP9966_FUNCTION_SET_00, 0x0);

	// Clear brightness Register
	fp9966_write_reg(dev, FP9966_BRIGHT_L_CTRL1_04, 0x00);
	fp9966_write_reg(dev, FP9966_BRIGHT_H_CTRL1_05, 0x00);
	fp9966_write_reg(dev, FP9966_BRIGHT_L_CTRL2_06, 0x00);
	fp9966_write_reg(dev, FP9966_BRIGHT_H_CTRL2_07, 0x00);

	// Ramp Control Register
	fp9966_write_reg(dev, FP9966_RAMP_CTRL_01, 0x88);

	/* ch1/ch2 over voltage threshold 33V*/
	fp9966_write_reg(dev, FP9966_OVP_03, 0xCC);

	/* suggest ch1 auto frequency high threshold*/
	fp9966_write_reg(dev, FP9966_AUTO_FREQ_H_THRD1_08, 0x80);
	/* suggesti ch1 auto frequency low threshold*/
	fp9966_write_reg(dev, FP9966_AUTO_FREQ_L_THRD1_09, 0x26);
	/* suggesti ch2 auto frequency high threshold*/
	fp9966_write_reg(dev, FP9966_AUTO_FREQ_H_THRD2_0A, 0x80);
	/* suggesti ch2 auto frequency low threshold*/
	fp9966_write_reg(dev, FP9966_AUTO_FREQ_L_THRD2_0B, 0x26);

	// Functional Setting Register
	fp9966_write_reg(dev, FP9966_FUNCTION_SET_00, 0xFF);

	return ret;
}

static int fp9966_backlight_probe(struct udevice *dev)
{
	struct fp9966_backlight_priv *priv = dev_get_priv(dev);
	int ret;

	ret = gpio_request_by_name(dev, "adj-gpio", 0, &priv->gpio_adj, GPIOD_IS_OUT);
	if (ret) {
		debug("%s: cannot get adj GPIO: ret=%d\n", dev->name, ret);
		goto out;
	}

	priv->index = dev_read_u32_default(dev, "index", 0);

	if (!fp9966_enabled) {
		ret = gpio_request_by_name(dev, "enable-gpio", 0, &priv->gpio_en, GPIOD_IS_OUT);
		if (ret) {
			debug("%s: cannot get enable GPIO: ret=%d\n", dev->name, ret);
			goto out_gpio;
		}

		fp9966_enabled = 1;

		/* hardware enable */
		ret = dm_gpio_set_value(&priv->gpio_en, 1);
		if (ret) {
			debug("%s: failed to enable hardware: ret=%d\n", dev->name, ret);
			goto out_gpio1;
		}

		/* hardware init */
		ret = fp9966_backlight_init(dev);
		if (ret) {
			goto out_gpio1;
		}
	}

	return ret;

out_gpio1:
	dm_gpio_set_value(&priv->gpio_en, 0);
	dm_gpio_set_dir_flags(&priv->gpio_en, GPIOD_IS_IN);
	dm_gpio_free(dev, &priv->gpio_en);
	fp9966_enabled = 0;

out_gpio:
	dm_gpio_set_value(&priv->gpio_adj, 0);
	dm_gpio_set_dir_flags(&priv->gpio_adj, GPIOD_IS_IN);
	dm_gpio_free(dev, &priv->gpio_adj);

out:
	debug("%s: error init device. err=%d\n", dev->name, ret);

	return ret;
}

static const struct backlight_ops fp9966_backlight_ops = {
	.set_brightness = fp9966_backlight_set_brightness,
	.enable_ramp = fp9966_backlight_enable_ramp,
};

static const struct udevice_id fp9966_backlight_ids[] = {
	{ .compatible = "fiti,fp9966" },
	{ }
};

U_BOOT_DRIVER(fp9966_backlight) = {
	.name		= "fp9966_backlight",
	.id			= UCLASS_PANEL_BACKLIGHT,
	.of_match	= fp9966_backlight_ids,
	.probe		= fp9966_backlight_probe,
	.ops        = &fp9966_backlight_ops,
	.priv_auto_alloc_size = sizeof(struct fp9966_backlight_priv),
};

