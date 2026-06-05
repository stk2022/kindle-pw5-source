/*
 * Copyright (c) 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * Copyright (c) 2020, Fitipower.
 */

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
#ifdef CONFIG_LAB126
#ifdef CONFIG_FRONTLIGHT
#include <linux/frontlight.h>
#endif
#endif
#ifdef CONFIG_FALCON
#include <asm/falcon_syscall.h>
#endif

//#define FP9966_DEBUG	0
//#define FP9966_BL_CTRL_CLEAR 1

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

/**
 * struct lm3692x_led -
 * @lock - Lock for reading/writing the device
 * @client - Pointer to the I2C client
 * @led_dev0 - LED 0 class device pointer
 * @led_dev1 - LED 1 class device pointer
 * @regmap - Devices register map
 * @enable_gpio - VDDIO/EN gpio to enable communication interface
 * @regulator - LED supply regulator pointer
 * @label_bl0: The name of the LED 0
 * @label_bl1: The name of the LED 1
 * @led0_enable - LED 0 sync to be enabled
 * @led1_enable - LED 1 sync to be enabled
 * @mapmode0 - LED 0 mapping mode
 * @mapmode1 - LED 1 mapping mode
 * @model_id - Current device model ID enumerated
 */
struct fp9966_bl {
	struct mutex lock;
	struct i2c_client *client;
	struct backlight_device *bl_dev0;
	struct backlight_device *bl_dev1;
	struct regmap *regmap;
	struct gpio_desc *enable_gpio;
	struct regulator *regulator;
	char label_bl0[BACKLIGHT_MAX_NAME_SIZE];
	char label_bl1[BACKLIGHT_MAX_NAME_SIZE];
	int led0_enable;
	int led1_enable;
	int mapmode0;
	int mapmode1;
	int model_id;
};

static const struct reg_default fp9966_reg_defs[] = {    /*register initialization */
	{FP9966_FUNCTION_SET_00, 0x11},                      /* ch1/ch2 BLADJ high active */
	{FP9966_RAMP_CTRL_01, 0x00},                         /* ch1/ch2 ramp ctrl init*/
	{FP9966_OCP_02, 0x00},                               /* ch1/ch2 over current threshold 0.5A*/
	{FP9966_OVP_03, 0xCC},                               /* ch1/ch2 over voltage threshold 33V*/
	{FP9966_AUTO_FREQ_H_THRD1_08, 0x80},                 /* suggesti ch1 auto frequency high threshold*/
	{FP9966_AUTO_FREQ_L_THRD1_09, 0x26},                 /* suggesti ch1 auto frequency low threshold*/
	{FP9966_AUTO_FREQ_H_THRD2_0A, 0x80},                 /* suggesti ch2 auto frequency high threshold*/
	{FP9966_AUTO_FREQ_L_THRD2_0B, 0x26},                 /* suggesti ch2 auto frequency low threshold*/

};

static const struct regmap_config fp9966_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.reg_defaults = fp9966_reg_defs,
	.num_reg_defaults = ARRAY_SIZE(fp9966_reg_defs),
	.max_register = FP9966_OTP_FAULT_FLAG_11,
	.cache_type = REGCACHE_RBTREE,

};

static void fp9966_fault_init_setting(struct fp9966_bl *bl)
{
	regmap_update_bits(bl->regmap, FP9966_FAULT_CTRL_0E, FP9966_CH2_OVP_SD, FP9966_SHUTDOWN_PROTECT_DISABLE);
	regmap_update_bits(bl->regmap, FP9966_FAULT_CTRL_0E, FP9966_CH2_OPEN_SD, FP9966_SHUTDOWN_PROTECT_DISABLE);
	regmap_update_bits(bl->regmap, FP9966_FAULT_CTRL_0E, FP9966_CH2_SHORT_SD, FP9966_SHUTDOWN_PROTECT_DISABLE);
	regmap_update_bits(bl->regmap, FP9966_FAULT_CTRL_0E, FP9966_CH2_OCP_SD, FP9966_SHUTDOWN_PROTECT_DISABLE);
	regmap_update_bits(bl->regmap, FP9966_FAULT_CTRL_0E, FP9966_CH1_OVP_SD, FP9966_SHUTDOWN_PROTECT_DISABLE);
	regmap_update_bits(bl->regmap, FP9966_FAULT_CTRL_0E, FP9966_CH1_OPEN_SD, FP9966_SHUTDOWN_PROTECT_DISABLE);
	regmap_update_bits(bl->regmap, FP9966_FAULT_CTRL_0E, FP9966_CH1_SHORT_SD, FP9966_SHUTDOWN_PROTECT_DISABLE);
	regmap_update_bits(bl->regmap, FP9966_FAULT_CTRL_0E, FP9966_CH1_OCP_SD, FP9966_SHUTDOWN_PROTECT_DISABLE);

	regmap_update_bits(bl->regmap, FP9966_OTP_CTRL_ENABLE_10, FP9966_OTP_SD, FP9966_SHUTDOWN_PROTECT_DISABLE);

	return;
}

static int fp9966_fault_check(struct fp9966_bl *bl)
{
	int ret;
	unsigned int read_buf;

	ret = regmap_read(bl->regmap, FP9966_FAULT_FLAG_0F, &read_buf);
	if (ret)
		return ret;

	/* check all fault flag */
	if (read_buf & FP9966_CH2_OVP_FLAG) {
		dev_err(&bl->client->dev, "Detected CH2 OVP fault 0x%X\n", read_buf);
	}

	if (read_buf & FP9966_CH2_OPEN_FLAG) {
		dev_err(&bl->client->dev, "Detected CH2 OPEN fault 0x%X\n", read_buf);
	}

	if (read_buf & FP9966_CH2_SHORT_FLAG) {
		dev_err(&bl->client->dev, "Detected CH2 SHORT fault 0x%X\n", read_buf);
	}

	if (read_buf & FP9966_CH2_OCP_FLAG) {
		dev_err(&bl->client->dev, "Detected CH2 OCP fault 0x%X\n", read_buf);
	}

	if (read_buf & FP9966_CH1_OVP_FLAG) {
		dev_err(&bl->client->dev, "Detected CH1 OVP fault 0x%X\n", read_buf);
	}

	if (read_buf & FP9966_CH1_OPEN_FLAG) {
		dev_err(&bl->client->dev, "Detected CH1 OPEN fault 0x%X\n", read_buf);
	}

	if (read_buf & FP9966_CH1_SHORT_FLAG) {
		dev_err(&bl->client->dev, "Detected CH1 SHORT fault 0x%X\n", read_buf);
	}

	if (read_buf & FP9966_CH1_OCP_FLAG) {
		dev_err(&bl->client->dev, "Detected CH1 OCP fault 0x%X\n", read_buf);
	}

	if (read_buf)
		return read_buf;

	/* check temperature fault flag */
	ret = regmap_read(bl->regmap, FP9966_OTP_FAULT_FLAG_11, &read_buf);
	if (ret)
		return ret;

	if (read_buf & FP9966_OTP_FLAG) {
		dev_err(&bl->client->dev, "Detect OTP fault 0x%X\n", read_buf);
	}

	return 0;
}

static int fp9966_backlight_bl0_get_brightness(struct backlight_device *bl_dev)
{
	struct fp9966_bl *bl = bl_get_data(bl_dev);
	int brightness = 0;
	int msb, lsb;
	int ret;

	mutex_lock(&bl->lock);

	ret = fp9966_fault_check(bl);
	if (ret) {
		dev_err(&bl->client->dev, "Cannot read/clear CTRL1 faults\n");
		goto out;
	}

	ret = regmap_read(bl->regmap, FP9966_BRIGHT_L_CTRL1_04, &lsb);
	if (ret < 0) {
		dev_err(&bl->client->dev, "Cannot read CTRL1 LSB\n");
		goto out;
	}

	ret = regmap_read(bl->regmap,  FP9966_BRIGHT_H_CTRL1_05, &msb);
	if (ret < 0) {
		dev_err(&bl->client->dev, "Cannot read CTRL1 MSB\n");
		goto out;
	}

	brightness = (msb << 3) | (lsb & 0x7);
	bl_dev->props.brightness = brightness;
	dev_dbg(&bl->client->dev, "%s: brightness=%d\n", __func__, brightness);

out:
	mutex_unlock(&bl->lock);

	return brightness;
}

static int fp9966_backlight_bl1_get_brightness(struct backlight_device *bl_dev)
{
	struct fp9966_bl *bl = bl_get_data(bl_dev);
	int brightness = 0;
	int msb, lsb;
	int ret;

	mutex_lock(&bl->lock);

	ret = fp9966_fault_check(bl);
	if (ret) {
		dev_err(&bl->client->dev, "Cannot read/clear CTRL2 faults\n");
		goto out;
	}

	ret = regmap_read(bl->regmap, FP9966_BRIGHT_L_CTRL2_06, &lsb);
	if (ret < 0) {
		dev_err(&bl->client->dev, "Cannot read CTRL2 LSB\n");
		goto out;
	}

	ret = regmap_read(bl->regmap,  FP9966_BRIGHT_H_CTRL2_07, &msb);
	if (ret < 0) {
		dev_err(&bl->client->dev, "Cannot read CTRL2 MSB\n");
		goto out;
	}

	brightness = (msb << 3) | (lsb & 0x7);
	bl_dev->props.brightness = brightness;
	dev_dbg(&bl->client->dev, "%s: brightness=%d\n", __func__, brightness);

out:
	mutex_unlock(&bl->lock);

	return brightness;
}

#ifdef FP9966_DEBUG
static int fp9966_backlight_get_all_reg_setting(struct fp9966_bl *bl)
{
	int data;
	int ret;

	mutex_lock(&bl->lock);

	ret = regmap_read(bl->regmap, FP9966_FUNCTION_SET_00, &data);
	dev_info(&bl->client->dev, "reg_00 = [%x]\n", data);

	ret = regmap_read(bl->regmap,  FP9966_RAMP_CTRL_01, &data);
	dev_info(&bl->client->dev, "reg_01 = [%x]\n", data);

	ret = regmap_read(bl->regmap,  FP9966_OCP_02, &data);
	dev_info(&bl->client->dev, "reg_02 = [%x]\n", data);

	ret = regmap_read(bl->regmap,  FP9966_OVP_03, &data);
	dev_info(&bl->client->dev, "reg_03 = [%x]\n", data);

	ret = regmap_read(bl->regmap,  FP9966_BRIGHT_L_CTRL1_04, &data);
	dev_info(&bl->client->dev, "reg_04 = [%x]\n", data);

	ret = regmap_read(bl->regmap,  FP9966_BRIGHT_H_CTRL1_05, &data);
	dev_info(&bl->client->dev, "reg_05 = [%x]\n", data);

	ret = regmap_read(bl->regmap,  FP9966_BRIGHT_L_CTRL2_06, &data);
	dev_info(&bl->client->dev, "reg_06 = [%x]\n", data);

	ret = regmap_read(bl->regmap,  FP9966_BRIGHT_H_CTRL2_07, &data);
	dev_info(&bl->client->dev, "reg_07 = [%x]\n", data);

	ret = regmap_read(bl->regmap,  FP9966_AUTO_FREQ_H_THRD1_08, &data);
	dev_info(&bl->client->dev, "reg_08 = [%x]\n", data);

	ret = regmap_read(bl->regmap,  FP9966_AUTO_FREQ_L_THRD1_09, &data);
	dev_info(&bl->client->dev, "reg_09 = [%x]\n", data);

	ret = regmap_read(bl->regmap,  FP9966_AUTO_FREQ_H_THRD2_0A, &data);
	dev_info(&bl->client->dev, "reg_0a = [%x]\n", data);

	ret = regmap_read(bl->regmap,  FP9966_AUTO_FREQ_L_THRD2_0B, &data);
	dev_info(&bl->client->dev, "reg_0b = [%x]\n", data);

	ret = regmap_read(bl->regmap,  FP9966_CH1_BK_LIGHT_THRD_0C, &data);
	dev_info(&bl->client->dev, "reg_0c = [%x]\n", data);

	ret = regmap_read(bl->regmap,  FP9966_CH2_BK_LIGHT_THRD_0D, &data);
	dev_info(&bl->client->dev, "reg_0d = [%x]\n", data);

	ret = regmap_read(bl->regmap,  FP9966_FAULT_CTRL_0E, &data);
	dev_info(&bl->client->dev, "reg_0e = [%x]\n", data);

	ret = regmap_read(bl->regmap,  FP9966_FAULT_FLAG_0F, &data);
	dev_info(&bl->client->dev, "reg_0f = [%x]\n", data);

	ret = regmap_read(bl->regmap,  FP9966_OTP_CTRL_ENABLE_10, &data);
	dev_info(&bl->client->dev, "reg_10 = [%x]\n", data);

	ret = regmap_read(bl->regmap,  FP9966_OTP_FAULT_FLAG_11, &data);
	dev_info(&bl->client->dev, "reg_11 = [%x]\n", data);

	mutex_unlock(&bl->lock);

	return 0;
}
#endif

static int fp9966_backlight_bl0_update_status(struct backlight_device *bl_dev)
{
	struct fp9966_bl *bl = bl_get_data(bl_dev);
	int brightness = bl_dev->props.brightness;
	int ret;
	int bl_brightness_msb = (brightness >> 3);
	int bl_brightness_lsb = (brightness & 0x7);

	if (bl_dev->props.state & BL_CORE_SUSPENDED)
	 	brightness = 0;

	dev_dbg(&bl->client->dev, "%s: state=%d, brightness=%d\n", __func__, bl_dev->props.state, brightness);

	mutex_lock(&bl->lock);

	ret = fp9966_fault_check(bl);
	if (ret) {
		dev_err(&bl->client->dev, "Cannot read/clear CTRL1 faults\n");
		goto out;
	}

	/* first write the LSB, then write the MSB */
	ret = regmap_write(bl->regmap, FP9966_BRIGHT_L_CTRL1_04, bl_brightness_lsb);
	if (ret) {
		dev_err(&bl->client->dev, "Cannot write CTRL1 LSB\n");
		goto out;
	}

	ret = regmap_write(bl->regmap, FP9966_BRIGHT_H_CTRL1_05, bl_brightness_msb);
	if (ret) {
		dev_err(&bl->client->dev, "Cannot write CTRL1 MSB\n");
		goto out;
	}

out:
	mutex_unlock(&bl->lock);

	return ret;
}

static int fp9966_backlight_bl1_update_status(struct backlight_device *bl_dev)
{
	struct fp9966_bl *bl = bl_get_data(bl_dev);
	int brightness = bl_dev->props.brightness;
	int ret;
	int bl_brightness_msb = (brightness >> 3);
	int bl_brightness_lsb = (brightness & 0x7);

	if (bl_dev->props.state & BL_CORE_SUSPENDED)
	 	brightness = 0;

	dev_dbg(&bl->client->dev, "%s: state=%d, brightness=%d\n", __func__, bl_dev->props.state, brightness);

	mutex_lock(&bl->lock);

	ret = fp9966_fault_check(bl);
	if (ret) {
		dev_err(&bl->client->dev, "Cannot read/clear CTRL2 faults\n");
		goto out;
	}

	/* first write the LSB, then write the MSB */
	ret = regmap_write(bl->regmap, FP9966_BRIGHT_L_CTRL2_06, bl_brightness_lsb);
	if (ret) {
		dev_err(&bl->client->dev, "Cannot write CTRL2 LSB\n");
		goto out;
	}

	ret = regmap_write(bl->regmap, FP9966_BRIGHT_H_CTRL2_07, bl_brightness_msb);
	if (ret) {
		dev_err(&bl->client->dev, "Cannot write CTRL2 MSB\n");
		goto out;
	}

out:
	mutex_unlock(&bl->lock);

	return ret;
}

static const struct backlight_ops fp9966_backlight_bl0_ops = {
	.update_status = fp9966_backlight_bl0_update_status,
	.get_brightness = fp9966_backlight_bl0_get_brightness,
};

static const struct backlight_ops fp9966_backlight_bl1_ops = {
	.update_status = fp9966_backlight_bl1_update_status,
	.get_brightness = fp9966_backlight_bl1_get_brightness,
};

static int fp9966_backlight_add_device(struct fp9966_bl *bl)
{
	struct backlight_properties props;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_PLATFORM;
	props.max_brightness = MAX_BRIGHTNESS_11BIT;

	if (bl->led0_enable) {
		bl->bl_dev0 = devm_backlight_device_register(&bl->client->dev, bl->label_bl0,
						&bl->client->dev, bl,
						&fp9966_backlight_bl0_ops, &props);
		if (IS_ERR(bl->bl_dev0))
			return PTR_ERR(bl->bl_dev0);
	}

	if (bl->led1_enable) {
		bl->bl_dev1 = devm_backlight_device_register(&bl->client->dev, bl->label_bl1,
						&bl->client->dev, bl,
						&fp9966_backlight_bl1_ops, &props);
		if (IS_ERR(bl->bl_dev1))
			return PTR_ERR(bl->bl_dev1);
	}

#ifdef CONFIG_LAB126
#ifdef CONFIG_FRONTLIGHT
	if (bl->led0_enable)
		frontlight_register(bl->bl_dev0, FRONTLIGHT_COLOR_AMBER);

	if (bl->led1_enable)
		frontlight_register(bl->bl_dev1, FRONTLIGHT_COLOR_WHITE);
#endif
#endif

	return 0;
}

#ifdef FP9966_BL_CTRL_CLEAR
static int fp9966_backlight_bl_ctrl_clean(struct fp9966_bl *bl)
{
	regmap_write(bl->regmap, FP9966_BRIGHT_L_CTRL1_04, 0x00);
	regmap_write(bl->regmap, FP9966_BRIGHT_H_CTRL1_05, 0x00);
	regmap_write(bl->regmap, FP9966_BRIGHT_L_CTRL2_06, 0x00);
	regmap_write(bl->regmap, FP9966_BRIGHT_H_CTRL2_07, 0x00);

	return 0;
}
#endif

static int fp9966_backlight_init(struct fp9966_bl *bl)
{
	u8 ramp_value;
	u8 func_set_val;
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

#ifdef FP9966_BL_CTRL_CLEAR
	ret = fp9966_backlight_bl_ctrl_clean(bl);
#endif

	fp9966_fault_init_setting(bl);

	ret = fp9966_fault_check(bl);
	if (ret) {
		dev_err(&bl->client->dev, "Cannot read/clear faults\n");
		goto out;
	}

	if (bl->led0_enable)
		func_set_val = FP9966_CH1_EN_MASK | FP9966_LED1_MAP_BLADJ;
	if (bl->led1_enable)
		func_set_val |= FP9966_CH2_EN_MASK | FP9966_LED2_MAP_BLADJ;

	ramp_value = (FP9966_CH2_RAMP_RATE_125us << 4) | FP9966_CH1_RAMP_RATE_125us;

	if (bl->mapmode0){
		func_set_val |= FP9966_LED1_MAP_MODE;
		ramp_value |= FP9966_CH1_RAMPEN;
	}
	if (bl->mapmode1){
		func_set_val |= FP9966_LED2_MAP_MODE;
		ramp_value |= FP9966_CH2_RAMPEN;
	}

	//Functional Setting Register
	regmap_write(bl->regmap, FP9966_FUNCTION_SET_00, func_set_val);
	//Ramp Control Register
	regmap_write(bl->regmap, FP9966_RAMP_CTRL_01, ramp_value);

	/* ch1/ch2 over voltage threshold 33V*/
	regmap_write(bl->regmap, FP9966_OVP_03, 0xCC);

	/* suggest ch1 auto frequency high threshold*/
	regmap_write(bl->regmap, FP9966_AUTO_FREQ_H_THRD1_08, 0x80);
	/* suggesti ch1 auto frequency low threshold*/
	regmap_write(bl->regmap, FP9966_AUTO_FREQ_L_THRD1_09, 0x26);
	/* suggesti ch2 auto frequency high threshold*/
	regmap_write(bl->regmap, FP9966_AUTO_FREQ_H_THRD2_0A, 0x80);
	/* suggesti ch2 auto frequency low threshold*/
	regmap_write(bl->regmap, FP9966_AUTO_FREQ_L_THRD2_0B, 0x26);

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


static int fp9966_backlight_probe_dt(struct fp9966_bl *bl)
{
	struct fwnode_handle *child = NULL;
	const char *name;
	int ret;

	bl->enable_gpio = devm_gpiod_get_optional(&bl->client->dev,
						   "enable", GPIOD_ASIS);
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
		dev_err(&bl->client->dev, "No Backlight Child node 0\n");
		return -ENODEV;
	}

	ret = fwnode_property_read_string(child, "label", &name);
	if (ret)
		snprintf(bl->label_bl0, sizeof(bl->label_bl0),
				"%s-bl0", bl->client->name);
	else
		snprintf(bl->label_bl0, sizeof(bl->label_bl0),
				"%s", name);

	ret = fwnode_property_read_u32(child, "enable", &bl->led0_enable);
	if (ret) {
		dev_warn(&bl->client->dev, "enable DT property missing\n");
	}

	ret = fwnode_property_read_u32(child, "mapmode", &bl->mapmode0);
	if (ret) {
		dev_warn(&bl->client->dev, "mapmode DT property missing\n");
	}

	child = device_get_next_child_node(&bl->client->dev, child);
	if (!child) {
		dev_err(&bl->client->dev, "No Backlight Child node 1\n");
			return -ENODEV;
	}

	ret = fwnode_property_read_string(child, "label", &name);
	if (ret)
		snprintf(bl->label_bl1, sizeof(bl->label_bl1),
				"%s-bl1", bl->client->name);
	else
		snprintf(bl->label_bl1, sizeof(bl->label_bl1),
				"%s", name);

	ret = fwnode_property_read_u32(child, "enable", &bl->led1_enable);
	if (ret) {
		dev_warn(&bl->client->dev, "enable DT property missing\n");
	}

	ret = fwnode_property_read_u32(child, "mapmode", &bl->mapmode1);
	if (ret) {
		dev_warn(&bl->client->dev, "mapmode DT property missing\n");
	}

	return 0;
}

static int fp9966_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct fp9966_bl *bl;
	int ret;

	bl = devm_kzalloc(&client->dev, sizeof(*bl), GFP_KERNEL);
	if (!bl)
		return -ENOMEM;

	mutex_init(&bl->lock);
	bl->client = client;
	bl->model_id = id->driver_data;
	i2c_set_clientdata(client, bl);

	bl->regmap = devm_regmap_init_i2c(client, &fp9966_regmap_config);
	if (IS_ERR(bl->regmap)) {
		ret = PTR_ERR(bl->regmap);
		dev_err(&client->dev, "Failed to allocate register map: %d\n",
			ret);
		return ret;
	}

	ret = fp9966_backlight_probe_dt(bl);
	if (ret)
		goto errors;

//fault disable

	ret = fp9966_backlight_init(bl);
	if (ret)
		goto errors;

#ifdef FP9966_DEBUG
	fp9966_backlight_get_all_reg_setting(bl);
#endif

	ret = fp9966_backlight_add_device(bl);
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

static int fp9966_remove(struct i2c_client *client)
{
	struct fp9966_bl *bl = i2c_get_clientdata(client);
	int ret;

	bl->bl_dev0->props.brightness = 0;
	bl->bl_dev1->props.brightness = 0;

	ret = regmap_write(bl->regmap, FP9966_FUNCTION_SET_00, 0x11); /* disable all functions*/
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

	if (bl->led0_enable)
		devm_backlight_device_unregister(&bl->client->dev, bl->bl_dev0);
	if (bl->led0_enable)
		devm_backlight_device_unregister(&bl->client->dev, bl->bl_dev1);

	mutex_destroy(&bl->lock);

	return 0;
}



static const struct i2c_device_id fp9966_id[] = {
	{ "fp9966", FP9966_MODEL },
	{ }
};
MODULE_DEVICE_TABLE(i2c, fp9966_id);

static const struct of_device_id of_fp9966_bl_match[] ={
	{ .compatible = "fp9966", },
	{},
};
MODULE_DEVICE_TABLE(of, of_fp9966_bl_match);

#ifdef CONFIG_PM_SLEEP
static int fp9966_suspend(struct device *dev)
{
	struct fp9966_bl *bl = dev_get_drvdata(dev);

#ifdef CONFIG_FALCON
	if (in_falcon()) {
		/* Do not change pinctrl for hibernation */
		return 0;
	}
#endif

	regmap_write(bl->regmap, FP9966_FUNCTION_SET_00, 0x11);

	if (bl->enable_gpio)
		gpiod_direction_output(bl->enable_gpio, 0);

	if (bl->regulator) {
		if (regulator_disable(bl->regulator))
			dev_err(&bl->client->dev,
				"Failed to disable regulator\n");
	}

	pinctrl_pm_select_sleep_state(dev);

	return 0;
}

static int fp9966_resume(struct device *dev)
{
	struct fp9966_bl *bl = dev_get_drvdata(dev);

	pinctrl_pm_select_default_state(dev);

#ifdef CONFIG_FALCON
	if (in_falcon()) {
		regcache_drop_region(bl->regmap, FP9966_BRIGHT_L_CTRL1_04, FP9966_BRIGHT_H_CTRL2_07);
		fp9966_backlight_bl0_get_brightness(bl->bl_dev0);
		fp9966_backlight_bl1_get_brightness(bl->bl_dev1);
	}
#endif

	return fp9966_backlight_init(bl);
}

static const struct dev_pm_ops fp9966_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(fp9966_suspend, fp9966_resume)
};
#endif

static struct i2c_driver fp9966_driver = {
	.driver = {
		.name	= "fp9966",
		.of_match_table = of_fp9966_bl_match,
#ifdef CONFIG_PM_SLEEP
		.pm 	= &fp9966_pm_ops,
#endif
	},
	.probe		= fp9966_probe,
	.remove		= fp9966_remove,
	.id_table	= fp9966_id,
};
module_i2c_driver(fp9966_driver);

MODULE_DESCRIPTION("Fitipower FP9966 LED driver");
MODULE_AUTHOR("Junchao Zhang <junchaz@lab126.com>");
MODULE_LICENSE("GPL v2");
