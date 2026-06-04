// SPDX-License-Identifier: GPL-2.0
// TI LM3692x LED driver
// Copyright (C) 2017-18 Texas Instruments Incorporated - http://www.ti.com/

#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/backlight.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/pwm.h>
#ifdef CONFIG_LAB126
#ifdef CONFIG_FRONTLIGHT
#include <linux/frontlight.h>
#endif
#endif

#define LM36922_MODEL	0
#define LM36923_MODEL	1

#define LM3692X_REV		0x0
#define LM3692X_RESET		0x1
#define LM3692X_EN		0x10
#define LM3692X_BRT_CTRL	0x11
#define LM3692X_PWM_CTRL	0x12
#define LM3692X_BOOST_CTRL	0x13
#define LM3692X_AUTO_FREQ_HI	0x15
#define LM3692X_AUTO_FREQ_LO	0x16
#define LM3692X_BL_ADJ_THRESH	0x17
#define LM3692X_BRT_LSB		0x18
#define LM3692X_BRT_MSB		0x19
#define LM3692X_FAULT_CTRL	0x1e
#define LM3692X_FAULT_FLAGS	0x1f

#define LM3692X_SW_RESET	BIT(0)
#define LM3692X_DEVICE_EN	BIT(0)
#define LM3692X_LED1_EN		BIT(1)
#define LM3692X_LED2_EN		BIT(2)
#define LM36923_LED3_EN		BIT(3)
#define LM3692X_ENABLE_MASK	(LM3692X_DEVICE_EN | LM3692X_LED1_EN | \
				 LM3692X_LED2_EN | LM36923_LED3_EN)

/* Brightness Control Bits */
#define LM3692X_BL_ADJ_POL	BIT(0)
#define LM3692X_RAMP_RATE_125us	0x00
#define LM3692X_RAMP_RATE_250us	BIT(1)
#define LM3692X_RAMP_RATE_500us BIT(2)
#define LM3692X_RAMP_RATE_1ms	(BIT(1) | BIT(2))
#define LM3692X_RAMP_RATE_2ms	BIT(3)
#define LM3692X_RAMP_RATE_4ms	(BIT(3) | BIT(1))
#define LM3692X_RAMP_RATE_8ms	(BIT(2) | BIT(3))
#define LM3692X_RAMP_RATE_16ms	(BIT(1) | BIT(2) | BIT(3))
#define LM3692X_RAMP_EN		BIT(4)
#define LM3692X_BRHT_MODE_REG	0x00
#define LM3692X_BRHT_MODE_PWM	BIT(5)
#define LM3692X_BRHT_MODE_MULTI_RAMP BIT(6)
#define LM3692X_BRHT_MODE_RAMP_MULTI (BIT(5) | BIT(6))
#define LM3692X_MAP_MODE_EXP	BIT(7)

/* PWM Register Bits */
#define LM3692X_PWM_FILTER_100	BIT(0)
#define LM3692X_PWM_FILTER_150	BIT(1)
#define LM3692X_PWM_FILTER_200	(BIT(0) | BIT(1))
#define LM3692X_PWM_HYSTER_1LSB BIT(2)
#define LM3692X_PWM_HYSTER_2LSB	BIT(3)
#define LM3692X_PWM_HYSTER_3LSB (BIT(3) | BIT(2))
#define LM3692X_PWM_HYSTER_4LSB BIT(4)
#define LM3692X_PWM_HYSTER_5LSB (BIT(4) | BIT(2))
#define LM3692X_PWM_HYSTER_6LSB (BIT(4) | BIT(3))
#define LM3692X_PWM_POLARITY	BIT(5)
#define LM3692X_PWM_SAMP_4MHZ	BIT(6)
#define LM3692X_PWM_SAMP_24MHZ	BIT(7)

/* Boost Control Bits */
#define LM3692X_OCP_PROT_1A	BIT(0)
#define LM3692X_OCP_PROT_1_25A	BIT(1)
#define LM3692X_OCP_PROT_1_5A	(BIT(0) | BIT(1))
#define LM3692X_OVP_21V		BIT(2)
#define LM3692X_OVP_25V		BIT(3)
#define LM3692X_OVP_29V		(BIT(2) | BIT(3))
#define LM3692X_MIN_IND_22UH	BIT(4)
#define LM3692X_BOOST_SW_1MHZ	BIT(5)
#define LM3692X_BOOST_SW_NO_SHIFT	BIT(6)

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

/* Max Brightness */
#define MAX_BRIGHTNESS_11BIT		2047

#define BACKLIGHT_MAX_NAME_SIZE		64


/**
 * struct lm3692x_bl -
 * @lock - Lock for reading/writing the device
 * @client - Pointer to the I2C client
 * @bl_dev - Backlight device pointer
 * @regmap - Devices register map
 * @enable_gpio - VDDIO/EN gpio to enable communication interface
 * @regulator - LED supply regulator pointer
 * @label - LED label
 * @led_enable - LED sync to be enabled
 * @model_id - Current device model ID enumerated
 */
struct lm3692x_bl {
	struct mutex lock;
	struct i2c_client *client;
	struct backlight_device *bl_dev;
	struct pwm_device *pwm;
	struct regmap *regmap;
	struct gpio_desc *enable_gpio;
	struct regulator *regulator;
	char label[BACKLIGHT_MAX_NAME_SIZE];
	int led_enable;
	int model_id;
	int frontlight_index;
};

static const struct reg_default lm3692x_reg_defs[] = {
	{LM3692X_EN, 0xf},
	{LM3692X_BRT_CTRL, 0x61},
	{LM3692X_PWM_CTRL, 0x73},
	{LM3692X_BOOST_CTRL, 0x6f},
	{LM3692X_AUTO_FREQ_HI, 0x0},
	{LM3692X_AUTO_FREQ_LO, 0x0},
	{LM3692X_BL_ADJ_THRESH, 0x0},
	{LM3692X_BRT_LSB, 0x7},
	{LM3692X_BRT_MSB, 0xff},
	{LM3692X_FAULT_CTRL, 0x7},
};

static const struct regmap_config lm3692x_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = LM3692X_FAULT_FLAGS,
	.reg_defaults = lm3692x_reg_defs,
	.num_reg_defaults = ARRAY_SIZE(lm3692x_reg_defs),
	.cache_type = REGCACHE_RBTREE,
};

static int lm3692x_fault_check(struct lm3692x_bl *bl)
{
	int ret;
	unsigned int read_buf;

	ret = regmap_read(bl->regmap, LM3692X_FAULT_FLAGS, &read_buf);
	if (ret)
		return ret;

	if (read_buf)
		dev_err(&bl->client->dev, "Detected a fault 0x%X\n", read_buf);

	/* The first read may clear the fault.  Check again to see if the fault
	 * still exits and return that value.
	 */
	regmap_read(bl->regmap, LM3692X_FAULT_FLAGS, &read_buf);
	if (read_buf)
		dev_err(&bl->client->dev, "Second read of fault flags 0x%X\n",
			read_buf);

	return read_buf;
}

static int lm3692x_compute_duty_cycle(struct lm3692x_bl *bl, int brightness)
{
	unsigned int duty;

	//ROUNDDOWN(Brightness*(24000/40kHz)/2047,0)/(24000/40kHz)*100
	duty=brightness*600/2047*(bl->pwm->args.period)/600;

    return duty;
}

static void lm3692x_pwm_ctrl(struct lm3692x_bl *bl, int brightness)
{
	unsigned int duty, period;

	if (!bl->pwm)
		return;

	duty= lm3692x_compute_duty_cycle(bl, brightness);
	period = bl->pwm->args.period;

	pwm_config(bl->pwm, duty, period);
	if (duty)
		pwm_enable(bl->pwm);
	else
		pwm_disable(bl->pwm);

	pr_debug("%s: period:%d,duty:%d\n", __func__, bl->pwm->args.period, duty);

	return;
}

static int lm3692x_backlight_get_brightness(struct backlight_device *bl_dev)
{
	struct lm3692x_bl *bl = bl_get_data(bl_dev);
	int brightness = 0;
	int msb, lsb;
	int ret;

	mutex_lock(&bl->lock);

	ret = lm3692x_fault_check(bl);
	if (ret) {
		dev_err(&bl->client->dev, "Cannot read/clear faults\n");
		goto out;
	}

	ret = regmap_read(bl->regmap, LM3692X_BRT_LSB, &lsb);
	if (ret < 0) {
		dev_err(&bl->client->dev, "Cannot read LSB\n");
		goto out;
	}

	ret = regmap_read(bl->regmap, LM3692X_BRT_MSB, &msb);
	if (ret < 0) {
		dev_err(&bl->client->dev, "Cannot read MSB\n");
		goto out;
	}

	brightness = (msb << 3) | (lsb & 0x7);
	bl_dev->props.brightness = brightness;
	pr_debug("%s: brightness=%d\n", __func__, brightness);

out:
	mutex_unlock(&bl->lock);

	return brightness;
}

static int lm3692x_backlight_update_status(struct backlight_device *bl_dev)
{
	struct lm3692x_bl *bl = bl_get_data(bl_dev);
	int brightness = bl_dev->props.brightness;
	int ret;
	int bl_brightness_msb = (brightness >> 3);
	int bl_brightness_lsb = (brightness & 0x7);

	if (bl_dev->props.state & BL_CORE_SUSPENDED)
	 	brightness = 0;

	pr_debug("%s: state=%d, brightness=%d\n", __func__, bl_dev->props.state, brightness);

	mutex_lock(&bl->lock);

	ret = lm3692x_fault_check(bl);
	if (ret) {
		dev_err(&bl->client->dev, "Cannot read/clear faults\n");
		goto out;
	}

	if (bl->pwm){
		lm3692x_pwm_ctrl(bl,brightness);
	}

	ret = regmap_write(bl->regmap, LM3692X_BRT_LSB, bl_brightness_lsb);
	if (ret) {
		dev_err(&bl->client->dev, "Cannot write LSB\n");
		goto out;
	}

	ret = regmap_write(bl->regmap, LM3692X_BRT_MSB, bl_brightness_msb);
	if (ret) {
		dev_err(&bl->client->dev, "Cannot write MSB\n");
		goto out;
	}

out:
	mutex_unlock(&bl->lock);

	return ret;
}

static const struct backlight_ops lm3692x_backlight_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.update_status = lm3692x_backlight_update_status,
	.get_brightness = lm3692x_backlight_get_brightness,
};

static int lm3692x_backlight_init(struct lm3692x_bl *bl)
{
	int enable_state;
	int ret;

	if (bl->regulator) {
		ret = regulator_enable(bl->regulator);
		if (ret) {
			dev_err(&bl->client->dev,
				"Failed to enable regulator\n");
			return ret;
		}
	}

	if (bl->enable_gpio)
		gpiod_direction_output(bl->enable_gpio, 1);

	ret = lm3692x_fault_check(bl);
	if (ret) {
		dev_err(&bl->client->dev, "Cannot read/clear faults\n");
		goto out;
	}

	ret = regmap_write(bl->regmap, LM3692X_BRT_CTRL, 0x00);
	if (ret)
		goto out;

	/*
	 * For glitch free operation, the following data should
	 * only be written while LEDx enable bits are 0 and the device enable
	 * bit is set to 1.
	 * per Section 7.5.14 of the data sheet
	 */
	ret = regmap_write(bl->regmap, LM3692X_EN, LM3692X_DEVICE_EN);
	if (ret)
		goto out;

	/* Set the brightness to 0 so when enabled the LEDs do not come
	 * on with full brightness.
	 */
	ret = regmap_write(bl->regmap, LM3692X_BRT_LSB, 0);
	if (ret)
		goto out;

	ret = regmap_write(bl->regmap, LM3692X_BRT_MSB, 0);
	if (ret)
		goto out;

	ret = regmap_write(bl->regmap, LM3692X_PWM_CTRL,
		LM3692X_PWM_FILTER_200 | LM3692X_PWM_SAMP_24MHZ | LM3692X_PWM_POLARITY);
	if (ret)
		goto out;

	if (bl->pwm)
		ret = regmap_write(bl->regmap, LM3692X_BRT_CTRL,
			LM3692X_MAP_MODE_EXP |
			LM3692X_BRHT_MODE_RAMP_MULTI |
			LM3692X_BL_ADJ_POL |
			LM3692X_RAMP_RATE_125us);
	else 
		ret = regmap_write(bl->regmap, LM3692X_BRT_CTRL,
			LM3692X_MAP_MODE_EXP |
			LM3692X_BRHT_MODE_REG |
			LM3692X_BL_ADJ_POL |
			LM3692X_RAMP_RATE_125us);
	if (ret)
		goto out;

	ret = regmap_write(bl->regmap, LM3692X_AUTO_FREQ_HI, 0x00);
	if (ret)
		goto out;

	ret = regmap_write(bl->regmap, LM3692X_AUTO_FREQ_LO, 0x00);
	if (ret)
		goto out;

	ret = regmap_write(bl->regmap, LM3692X_BL_ADJ_THRESH, 0x00);
	if (ret)
		goto out;

	ret = regmap_write(bl->regmap, LM3692X_BOOST_CTRL,
			LM3692X_OCP_PROT_1_5A | LM3692X_OVP_29V |
			LM3692X_BOOST_SW_1MHZ | LM3692X_BOOST_SW_NO_SHIFT);
	if (ret)
		goto out;

	switch (bl->led_enable) {
	case 0:
	default:
		if (bl->model_id == LM36923_MODEL)
			enable_state = LM3692X_LED1_EN | LM3692X_LED2_EN |
			       LM36923_LED3_EN;
		else
			enable_state = LM3692X_LED1_EN | LM3692X_LED2_EN;

		break;
	case 1:
		enable_state = LM3692X_LED1_EN;
		break;
	case 2:
		enable_state = LM3692X_LED2_EN;
		break;

	case 3:
		if (bl->model_id == LM36923_MODEL) {
			enable_state = LM36923_LED3_EN;
			break;
		}

		ret = -EINVAL;
		dev_err(&bl->client->dev,
			"LED3 sync not available on this device\n");
		goto out;
	}

	ret = regmap_update_bits(bl->regmap, LM3692X_EN, LM3692X_ENABLE_MASK,
				 enable_state | LM3692X_DEVICE_EN);

	return ret;

out:
	dev_err(&bl->client->dev, "Fail writing initialization values\n");

	if (bl->enable_gpio)
		gpiod_direction_output(bl->enable_gpio, 0);

	if (bl->regulator) {
		if (regulator_disable(bl->regulator))
			dev_err(&bl->client->dev,
				"Failed to disable regulator\n");
	}

	return ret;
}

static int lm3692x_backlight_add_device(struct  lm3692x_bl *bl)
{
	struct backlight_properties props;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_PLATFORM;
	props.max_brightness = MAX_BRIGHTNESS_11BIT;

	bl->bl_dev = devm_backlight_device_register(&bl->client->dev, bl->label,
					&bl->client->dev, bl,
					&lm3692x_backlight_ops, &props);
	if (IS_ERR(bl->bl_dev))
		return PTR_ERR(bl->bl_dev);

#ifdef CONFIG_LAB126
#ifdef CONFIG_FRONTLIGHT
	frontlight_register(bl->bl_dev, bl->frontlight_index);
#endif
#endif

	return 0;
}

static int lm3692x_backlight_probe_dt(struct lm3692x_bl *bl)
{
	struct fwnode_handle *child = NULL;
	const char *name;
	int ret;

	bl->enable_gpio = devm_gpiod_get_optional(&bl->client->dev,
						   "enable", GPIOD_OUT_LOW);
	if (IS_ERR(bl->enable_gpio)) {
		bl->enable_gpio = NULL;
		ret = PTR_ERR(bl->enable_gpio);
		dev_err(&bl->client->dev, "Failed to get enable gpio: %d\n",
			ret);
		return ret;
	}

	bl->regulator = devm_regulator_get(&bl->client->dev, "vled");
	if (IS_ERR(bl->regulator))
		bl->regulator = NULL;

	child = device_get_next_child_node(&bl->client->dev, child);
	if (!child) {
		dev_err(&bl->client->dev, "No Backlight Child node\n");
		return -ENODEV;
	}

	ret = fwnode_property_read_string(child, "label", &name);
	if (ret)
		snprintf(bl->label, sizeof(bl->label),
			"%s", bl->client->name);
	else
		snprintf(bl->label, sizeof(bl->label),
			 "%s", name);

	ret = fwnode_property_read_u32(child, "reg", &bl->led_enable);
	if (ret) {
		dev_err(&bl->client->dev, "reg DT property missing\n");
		return ret;
	}

	ret = fwnode_property_read_u32(child, "frontlight", &bl->frontlight_index);
	if (ret) {
		dev_err(&bl->client->dev, "reg DT property missing\n");
		return ret;
	}

	bl->pwm = devm_pwm_get(&bl->client->dev, NULL);
	if (IS_ERR(bl->pwm)) {
		dev_err(&bl->client->dev, "unable to request PWM\n");
		bl->pwm=NULL;
	}

	return 0;
}

static int lm3692x_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct lm3692x_bl *bl;
	int ret;

	bl = devm_kzalloc(&client->dev, sizeof(*bl), GFP_KERNEL);
	if (!bl)
		return -ENOMEM;

	mutex_init(&bl->lock);
	bl->client = client;
	bl->model_id = id->driver_data;
	i2c_set_clientdata(client, bl);

	bl->regmap = devm_regmap_init_i2c(client, &lm3692x_regmap_config);
	if (IS_ERR(bl->regmap)) {
		ret = PTR_ERR(bl->regmap);
		dev_err(&client->dev, "Failed to allocate register map: %d\n",
			ret);
		return ret;
	}

	ret = lm3692x_backlight_probe_dt(bl);
	if (ret)
		goto errors;

	ret = lm3692x_backlight_init(bl);
	if (ret)
		goto errors;

	ret = lm3692x_backlight_add_device(bl);
	if (ret)
		goto errors;

	return 0;

errors:
	if (bl->enable_gpio) {
		gpiod_direction_output(bl->enable_gpio, 0);
		devm_gpiod_put(&bl->client->dev, bl->enable_gpio);
	}

	return ret;
}

static int lm3692x_remove(struct i2c_client *client)
{
	struct lm3692x_bl *bl = i2c_get_clientdata(client);
	int ret;

	bl->bl_dev->props.brightness = 0;

	ret = regmap_update_bits(bl->regmap, LM3692X_EN, LM3692X_DEVICE_EN, 0);
	if (ret) {
		dev_err(&bl->client->dev, "Failed to disable regulator\n");
		return ret;
	}

	if (bl->enable_gpio) {
		gpiod_direction_output(bl->enable_gpio, 0);
		devm_gpiod_put(&bl->client->dev, bl->enable_gpio);
	}

	if (bl->regulator) {
		if (regulator_disable(bl->regulator))
			dev_err(&bl->client->dev,
				"Failed to disable regulator\n");
	}

	devm_backlight_device_unregister(&bl->client->dev, bl->bl_dev);

	mutex_destroy(&bl->lock);

	return 0;
}

static const struct i2c_device_id lm3692x_id[] = {
	{ "lm36922", LM36922_MODEL },
	{ "lm36922h", LM36922_MODEL },
	{ "lm36923", LM36923_MODEL },
	{ }
};
MODULE_DEVICE_TABLE(i2c, lm3692x_id);

static const struct of_device_id of_lm3692x_bl_match[] = {
	{ .compatible = "ti,lm36922", },
	{ .compatible = "ti,lm36922h", },
	{ .compatible = "ti,lm36923", },
	{},
};
MODULE_DEVICE_TABLE(of, of_lm3692x_bl_match);

#ifdef CONFIG_PM_SLEEP
static int lm3692x_suspend(struct device *dev)
{
	struct lm3692x_bl *bl = dev_get_drvdata(dev);

	if (bl->enable_gpio)
		gpiod_direction_output(bl->enable_gpio, 0);

	if (bl->regulator) {
		if (regulator_disable(bl->regulator))
			dev_err(&bl->client->dev,
				"Failed to disable regulator\n");
	}

	return 0;
}

static int lm3692x_resume(struct device *dev)
{
	struct lm3692x_bl *bl = dev_get_drvdata(dev);

	return lm3692x_backlight_init(bl);
}

static const struct dev_pm_ops lm3692x_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(lm3692x_suspend, lm3692x_resume)
};
#endif

static struct i2c_driver lm3692x_driver = {
	.driver = {
		.name	= "lm3692x",
		.of_match_table = of_lm3692x_bl_match,
#ifdef CONFIG_PM_SLEEP
		.pm 	= &lm3692x_pm_ops,
#endif
	},
	.probe		= lm3692x_probe,
	.remove		= lm3692x_remove,
	.id_table	= lm3692x_id,
};
module_i2c_driver(lm3692x_driver);

MODULE_DESCRIPTION("Texas Instruments LM3692x Backlight driver");
MODULE_AUTHOR("Dan Murphy <dmurphy@ti.com>");
MODULE_LICENSE("GPL v2");

