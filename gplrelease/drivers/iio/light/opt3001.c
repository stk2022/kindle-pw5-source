/**
 * opt3001.c - Texas Instruments OPT3001 Light Sensor
 *
 * Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com
 *
 * Author: Andreas Dannenberg <dannenberg@ti.com>
 * Based on previous work from: Felipe Balbi <balbi@ti.com>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 of the License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/types.h>

#include <linux/iio/events.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>

#ifdef CONFIG_LAB126
#include <linux/of_gpio.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/lab126_als.h>
#include <linux/frontlight.h>
#define OPT3001_ALS_DEV_MINOR		166
#endif

#define OPT3001_RESULT		0x00
#define OPT3001_CONFIGURATION	0x01
#define OPT3001_LOW_LIMIT	0x02
#define OPT3001_HIGH_LIMIT	0x03
#define OPT3001_MANUFACTURER_ID	0x7e
#define OPT3001_DEVICE_ID	0x7f

#define OPT3001_CONFIGURATION_RN_MASK	(0xf << 12)
#define OPT3001_CONFIGURATION_RN_AUTO	(0xc << 12)

#define OPT3001_CONFIGURATION_CT	BIT(11)

#define OPT3001_CONFIGURATION_M_MASK	(3 << 9)
#define OPT3001_CONFIGURATION_M_SHUTDOWN (0 << 9)
#define OPT3001_CONFIGURATION_M_SINGLE	(1 << 9)
#define OPT3001_CONFIGURATION_M_CONTINUOUS (2 << 9) /* also 3 << 9 */

#define OPT3001_CONFIGURATION_OVF	BIT(8)
#define OPT3001_CONFIGURATION_CRF	BIT(7)
#define OPT3001_CONFIGURATION_FH	BIT(6)
#define OPT3001_CONFIGURATION_FL	BIT(5)
#define OPT3001_CONFIGURATION_L		BIT(4)
#define OPT3001_CONFIGURATION_POL	BIT(3)
#define OPT3001_CONFIGURATION_ME	BIT(2)

#define OPT3001_CONFIGURATION_FC_MASK	(3 << 0)

/* The end-of-conversion enable is located in the low-limit register */
#define OPT3001_LOW_LIMIT_EOC_ENABLE	0xc000

#define OPT3001_REG_EXPONENT(n)		((n) >> 12)
#define OPT3001_REG_MANTISSA(n)		((n) & 0xfff)

#define OPT3001_INT_TIME_LONG		800000
#define OPT3001_INT_TIME_SHORT		100000

#define CONFIG_OPT3001_ADJUST_WITH_BASELINE
#define CONFIG_OPT3001_BASELINE_PRECISION		(100)	/* 0.01 */

/*
 * Time to wait for conversion result to be ready. The device datasheet
 * sect. 6.5 states results are ready after total integration time plus 3ms.
 * This results in worst-case max values of 113ms or 883ms, respectively.
 * Add some slack to be on the safe side.
 */
#define OPT3001_RESULT_READY_SHORT	150
#define OPT3001_RESULT_READY_LONG	1000

struct opt3001 {
	struct i2c_client	*client;
	struct device		*dev;

	struct mutex		lock;
	bool			ok_to_ignore_lock;
	bool			result_ready;
	wait_queue_head_t	result_ready_queue;
	u16			result;

	u32			int_time;
	u32			mode;

	u16			high_thresh_mantissa;
	u16			low_thresh_mantissa;

	u8			high_thresh_exp;
	u8			low_thresh_exp;

	bool			use_irq;

#ifdef CONFIG_OPT3001_ADJUST_WITH_BASELINE
	u32			k1;
	u32			k2;
	u32			k3;
#endif

#ifdef CONFIG_LAB126
	struct work_struct	opt3001_als_wq;
#endif
};

struct opt3001_scale {
	int	val;
	int	val2;
};

static const struct opt3001_scale opt3001_scales[] = {
	{
		.val = 40,
		.val2 = 950000,
	},
	{
		.val = 81,
		.val2 = 900000,
	},
	{
		.val = 163,
		.val2 = 800000,
	},
	{
		.val = 327,
		.val2 = 600000,
	},
	{
		.val = 655,
		.val2 = 200000,
	},
	{
		.val = 1310,
		.val2 = 400000,
	},
	{
		.val = 2620,
		.val2 = 800000,
	},
	{
		.val = 5241,
		.val2 = 600000,
	},
	{
		.val = 10483,
		.val2 = 200000,
	},
	{
		.val = 20966,
		.val2 = 400000,
	},
	{
		.val = 83865,
		.val2 = 600000,
	},
};

static void opt3001_set_mode(struct opt3001 *opt, u16 *reg, u16 mode);

#ifdef CONFIG_LAB126
#define OPT3001_HYSTERESIS 10
#define LAB126_LOW_LUX_REPORTING_RANGE 15

static struct miscdevice als_opt3001_misc_device;

#ifdef CONFIG_OPT3001_ADJUST_WITH_BASELINE
extern struct backlight_device *backlight_devices[];

#define BRIGHTNESS_CURRENT_ARRAY_SIZE		20
static int brightnesss_current[BRIGHTNESS_CURRENT_ARRAY_SIZE] = {
			1136, 1307, 1408, 1478, 1534, 1579, 1869, 1649, 1679, 1705,
			1728, 1749, 1769, 1788, 1805, 1820, 1835, 1850, 1863, 1876,
		};

static int opt3001_get_brightness_current(int update) {
	int i, j;
	int brightness;
	static int br_cur = 0;

	if (!update)
		return br_cur / 2;

	br_cur = 0;

	for (i = 0; i < FRONTLIGHT_COLOR_LAST; i++) {
		if (unlikely(backlight_devices[i] == NULL))
			continue;

		brightness = backlight_devices[i]->props.brightness;

		for (j = 0; j < BRIGHTNESS_CURRENT_ARRAY_SIZE; j++) {
			if (brightness <= brightnesss_current[j])
				break;
		}

		br_cur += j;
	}

	if (br_cur % 2)
		br_cur++;

	return br_cur / 2;
}
#endif

static inline int opt3001_convert_count(struct opt3001 *opt) {
	return OPT3001_REG_MANTISSA(opt->result) << OPT3001_REG_EXPONENT(opt->result);
}

static inline int opt3001_convert_lux(struct opt3001 *opt) {
#ifdef CONFIG_OPT3001_ADJUST_WITH_BASELINE
	int lux;

	/* Lux = K2 * (Lux_count - K1) - K3 * LED_current */
	lux = opt->k2 * (opt3001_convert_count(opt) - opt->k1) -
			(opt->k3 * opt3001_get_brightness_current(1));

	if (lux < CONFIG_OPT3001_BASELINE_PRECISION)
		return 0;

	return lux / CONFIG_OPT3001_BASELINE_PRECISION;
#else
	return opt3001_convert_count(opt);
#endif
}

/*
 * Initialize opt3006 to continuous conversion with
 * end-of-conversion interrupt mechanism
 */
static int opt3001_init_automode(struct opt3001 *opt) {
	int ret = 0;
	u16 reg;

	dev_dbg(opt->dev, "%s\n", __func__);

	mutex_lock(&opt->lock);

	/* initialize end-of-conversion interrupt mechanism */
	if (opt->use_irq) {
		ret = i2c_smbus_write_word_swapped(opt->client,
					OPT3001_LOW_LIMIT,
					OPT3001_LOW_LIMIT_EOC_ENABLE);
		if (ret < 0) {
			dev_err(opt->dev, "failed to write register %02x\n",
					OPT3001_LOW_LIMIT);
			goto failed;
		}
	}

	ret = i2c_smbus_read_word_swapped(opt->client, OPT3001_CONFIGURATION);
	if (ret < 0) {
		dev_err(opt->dev, "failed to read register %02x\n",
				OPT3001_CONFIGURATION);
		goto failed;
	}
	reg = ret;

	/* Enable automatic full-scale setting mode */
	reg &= ~OPT3001_CONFIGURATION_RN_MASK;
	reg |= OPT3001_CONFIGURATION_RN_AUTO;

	/* initialize conversion time to 0.8 sec */
	reg |= OPT3001_CONFIGURATION_CT;
	opt->int_time = OPT3001_INT_TIME_LONG;

	/* initialize continuous conversion */
	opt3001_set_mode(opt, &reg, OPT3001_CONFIGURATION_M_CONTINUOUS);

	ret = i2c_smbus_write_word_swapped(opt->client, OPT3001_CONFIGURATION, reg);
	if (ret < 0) {
		dev_err(opt->dev, "failed to write register %02x\n",
				OPT3001_CONFIGURATION);
		goto failed;
	}

	mutex_unlock(&opt->lock);

	return 0;

failed:
	mutex_unlock(&opt->lock);
	dev_err(opt->dev, "%s: couldn't initialize %d.\n", __func__, ret);

	return ret;
}

/*
 * Initialize opt3001 to manual mode
 */
static int opt3001_init_mm(struct opt3001 *opt) {
	int ret = 0;
	u16 reg;

	dev_dbg(opt->dev, "%s\n", __func__);

	mutex_lock(&opt->lock);

	ret = i2c_smbus_read_word_swapped(opt->client, OPT3001_CONFIGURATION);
	if (ret < 0) {
		dev_err(opt->dev, "failed to read register %02x\n",
				OPT3001_CONFIGURATION);
		goto failed;
	}
	reg = ret;

	/* shutdown conversion */
	opt3001_set_mode(opt, &reg, OPT3001_CONFIGURATION_M_SHUTDOWN);

	ret = i2c_smbus_write_word_swapped(opt->client, OPT3001_CONFIGURATION, reg);
	if (ret < 0) {
		dev_err(opt->dev, "failed to write register %02x\n",
				OPT3001_CONFIGURATION);
		goto failed;
	}

	/* initialize upper threshold */
	opt->high_thresh_mantissa = 0xFFF;
	opt->high_thresh_exp = 0xB;
	ret = i2c_smbus_write_word_swapped(opt->client, OPT3001_HIGH_LIMIT, 0xBFFF);
	if (ret < 0) {
		dev_err(opt->dev, "failed to write register %02x\n", OPT3001_HIGH_LIMIT);
		goto failed;
	}

	/* initialize lower threshold */
	opt->low_thresh_mantissa = 0;
	opt->low_thresh_exp = 0;

	ret = i2c_smbus_write_word_swapped(opt->client, OPT3001_LOW_LIMIT, 0);
	if (ret < 0) {
		dev_err(opt->dev, "failed to write register %02x\n", OPT3001_LOW_LIMIT);
		goto failed;
	}

	mutex_unlock(&opt->lock);

	return 0;

failed:
	mutex_unlock(&opt->lock);
	dev_err(opt->dev, "%s: couldn't initialize manual mode %d.\n", __func__, ret);

	return ret;
}

static void opt3001_xthresh_uev(int lux) {
	char buf[32];
	char *envp[] = {"ALS=xthreshold", buf, NULL};

	snprintf(buf, 31, "LUX=%d", lux);
	kobject_uevent_env(&als_opt3001_misc_device.this_device->kobj, KOBJ_CHANGE, envp);
}

static long als_opt3001_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct device *this_dev = als_opt3001_misc_device.this_device;
	struct iio_dev *iio = dev_get_drvdata(als_opt3001_misc_device.parent);
	struct opt3001 *opt = iio_priv(iio);
	int __user *argp = (int __user *)arg;
	ALS_REGS als_reg;
	int automode;
	int ret = 0;
	int lux;
	int count;
	u16 *value = (u16 *)&als_reg.value[0];

	switch (cmd) {
		case ALS_IOCTL_GET_LUX:
			dev_dbg(this_dev, "%s:%d: ALS_IOCTL_GET_LUX\n", __func__, __LINE__);

			lux = opt3001_convert_lux(opt) / 100;

			if (put_user(lux, argp) == 0) {
				dev_dbg(this_dev, "%s:%d: lux:%d", __func__, __LINE__, lux);
			} else {
				dev_err(this_dev, "ALS_IOCTL_GET_LUX: put_user FAILED\n");
				ret = -EFAULT;
			}
		break;

		case ALS_IOCTL_GET_COUNT:
			dev_dbg(this_dev, "%s:%d: ALS_IOCTL_GET_COUNT\n", __func__, __LINE__);

			count = opt3001_convert_count(opt);

			if (put_user(count, argp) == 0) {
				dev_dbg(this_dev, "%s:%d: lux:%d", __func__, __LINE__, count);
			} else {
				dev_err(this_dev, "ALS_IOCTL_GET_COUNT: put_user FAILED\n");
				ret = -EFAULT;
			}
			break;

		case ALS_IOCTL_READ_LUX_REGS:
			dev_dbg(this_dev, "%s:%d:ALS_IOCTL_READ_REGS\n", __func__, __LINE__);

			lux = i2c_smbus_read_word_swapped(opt->client, OPT3001_RESULT);
			if (lux < 0) {
				dev_err(this_dev, "ALS_IOCTL_READ_REGS: opt3001_read_lux_regs FAILED\n");
				ret = -EFAULT;
			} else {
				*value = (u16)lux;
				if (copy_to_user(argp, &als_reg, sizeof(ALS_REGS)) != 0)	{
					dev_err(this_dev, "ALS_IOCTL_READ_REGS: copy_to_user FAILED\n");
					ret = -EFAULT;
				}
			}
			break;

		case ALS_IOCTL_READ_REG:
			dev_dbg(this_dev, "%s:%d: ALS_IOCTL_READ_REG\n", __func__, __LINE__);

			if (copy_from_user(&als_reg, argp, sizeof(ALS_REGS)) == 0) {
				count = i2c_smbus_read_word_swapped(opt->client, als_reg.addr);
				if (count < 0) {
					dev_err(this_dev, "ALS_IOCTL_READ_REG: opt3001_read_lux_regs FAILED\n");
					ret = -EFAULT;
				} else {
					*value = (u16)count;
					if (copy_to_user(argp, &als_reg, sizeof(ALS_REGS)) != 0) {
						ret = -EFAULT;
					}
				}
			} else {
				dev_err(this_dev, "ALS_IOCTL_READ_REG copy_from_user FAILED\n");
				ret = -EFAULT;
			}
			break;

		case ALS_IOCTL_WRITE_REG:
			dev_dbg(this_dev, "\n%s:%d: ALS_IOCTL_WRITE_REG\n", __func__, __LINE__);
			ret = -EFAULT;

			if (copy_from_user(&als_reg, argp, sizeof(ALS_REGS)) == 0) {
				if (i2c_smbus_write_word_swapped(opt->client, als_reg.addr, *value) >= 0) {
					ret = 0;
				}
			}
			break;

		case ALS_IOCTL_AUTOMODE_EN:
			dev_dbg(this_dev, "%s:%d: ALS_IOCTL_AUTOMODE_EN\n", __func__, __LINE__);

			if(get_user(automode, argp)) {
				dev_err(this_dev, "%s:%d get_user failed\n", __func__, __LINE__);
				ret = -EFAULT;
				break;
			}

			if (automode){
				if(opt3001_init_automode(opt)) {
					dev_err(this_dev, "%s:%d Could not initialize automode\n", __func__, __LINE__);
					ret = -EFAULT;
				}
			}else{
				if(opt3001_init_mm(opt)) {
					dev_err(this_dev, "%s:%d Could not initialize manual mode\n", __func__, __LINE__);
					ret = -EFAULT;
			    }
			}
			break;

		default:
			dev_err(this_dev, "ALS_IOCTL_CMD: unknown command: %d\n", cmd);
			ret = -EINVAL;
			break;
	}

	return ret;
}

static ssize_t als_opt3001_misc_write(struct file *file, const char __user *buf,
				size_t count, loff_t *pos)
{
	return 0;
}

static ssize_t als_opt3001_misc_read(struct file *file, char __user *buf,
				size_t count, loff_t *pos)
{
	return 0;
}

static const struct file_operations als_opt3001_misc_fops =
{
	.owner = THIS_MODULE,
	.read  = als_opt3001_misc_read,
	.write = als_opt3001_misc_write,
	.unlocked_ioctl = als_opt3001_ioctl,
};

static struct miscdevice als_opt3001_misc_device =
{
	.minor = OPT3001_ALS_DEV_MINOR,
	.name  = ALS_MISC_DEV_NAME,
	.fops  = &als_opt3001_misc_fops,
};

/*
 *  Converts a lux value into a threshold mantissa and exponent.
 */
static int opt3001_lux_to_thresh(struct opt3001 *opt, int lux)
{
	long result;
	int mantissa;
	int exponent = 0;

	if (lux <= 0) {
		/* 0 Has no mantissa */
		pr_debug("%s: lux_to_thresh lux=%d result = 0x%02x\n", __func__, lux, 0);

		return 0;
	}

#ifdef CONFIG_OPT3001_ADJUST_WITH_BASELINE
	/* result = ((Lux * 100) + (k3 * LED_current)) / K2 + K1 */
	result = ((lux * 100 * CONFIG_OPT3001_BASELINE_PRECISION) +
			(opt->k3 * opt3001_get_brightness_current(0)))
			/ opt->k2 + opt->k1;
#else
	result = lux * 100;
#endif
	while (result > 0xFFF) {
		result >>= 1;
		exponent++;
	}

	mantissa = result;
	result = (exponent << 12) | mantissa;

	pr_debug("%s: lux_to_thresh lux=%d mantissa=0x%x exponent=0x%x result=0x%x\n", __func__, lux, mantissa, exponent, (int)result);

	return (int)result;
}

static void opt3001_als_wqthread(struct work_struct *work)
{
	struct opt3001 *opt = container_of(work, struct opt3001, opt3001_als_wq);
	struct device *dev = opt->dev;
	int upper_thresh, lower_thresh;
	int lux;
	int ret;

	mutex_lock(&opt->lock);

	ret = i2c_smbus_read_word_swapped(opt->client, OPT3001_RESULT);
	if (ret < 0) {
		dev_err(opt->dev, "failed to read register %02x\n",
					OPT3001_RESULT);
		goto err;
	}
	opt->result = ret;

	lux = opt3001_convert_lux(opt) / 100;

	dev_info(dev, "%s: to upper layer lux = %d\n", __func__, lux);

	opt3001_xthresh_uev(lux);

	if (lux < LAB126_LOW_LUX_REPORTING_RANGE) {
		/* limit the low lux reporting every second for e-ink application
		 use the same lux ranges as Juno UI used in low lux conditons for reporting
		 lux range: 0~15 */
		upper_thresh = opt3001_lux_to_thresh(opt, LAB126_LOW_LUX_REPORTING_RANGE - 1);
		lower_thresh = opt3001_lux_to_thresh(opt, 0);
	} else {
		/* Adjusted lux interrupt thresholds by hysteresis and 0.5 lux widener */
		upper_thresh = opt3001_lux_to_thresh(opt, lux + ((lux * OPT3001_HYSTERESIS) >> 6));
		lower_thresh = opt3001_lux_to_thresh(opt, lux - ((lux * OPT3001_HYSTERESIS) >> 6));
	}

	/* set upper threshold */
	opt->high_thresh_mantissa = OPT3001_REG_MANTISSA(upper_thresh);
	opt->high_thresh_exp = OPT3001_REG_EXPONENT(upper_thresh);

	ret = i2c_smbus_write_word_swapped(opt->client, OPT3001_HIGH_LIMIT, (u16)upper_thresh);
	if (ret < 0) {
		dev_warn(opt->dev, "failed to write register %02x\n", OPT3001_HIGH_LIMIT);
	}

	/* set lower threshold */
	opt->low_thresh_mantissa = OPT3001_REG_MANTISSA(lower_thresh);
	opt->low_thresh_exp = OPT3001_REG_EXPONENT(lower_thresh);

	ret = i2c_smbus_write_word_swapped(opt->client, OPT3001_LOW_LIMIT, (u16)lower_thresh);
	if (ret < 0) {
		dev_warn(opt->dev, "failed to write register %02x\n", OPT3001_LOW_LIMIT);
	}

err:
	mutex_unlock(&opt->lock);
}
#endif

static int opt3001_find_scale(const struct opt3001 *opt, int val,
		int val2, u8 *exponent)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(opt3001_scales); i++) {
		const struct opt3001_scale *scale = &opt3001_scales[i];

		/*
		 * Combine the integer and micro parts for comparison
		 * purposes. Use milli lux precision to avoid 32-bit integer
		 * overflows.
		 */
		if ((val * 1000 + val2 / 1000) <=
				(scale->val * 1000 + scale->val2 / 1000)) {
			*exponent = i;
			return 0;
		}
	}

	return -EINVAL;
}

static void opt3001_to_iio_ret(struct opt3001 *opt, u8 exponent,
		u16 mantissa, int *val, int *val2)
{
	int lux;

#ifdef CONFIG_OPT3001_ADJUST_WITH_BASELINE
	lux = 10 * opt3001_convert_lux(opt);
#else
	lux = 10 * (mantissa << exponent);
#endif
	*val = lux / 1000;
	*val2 = (lux - (*val * 1000)) * 1000;
}

static void opt3001_set_mode(struct opt3001 *opt, u16 *reg, u16 mode)
{
	*reg &= ~OPT3001_CONFIGURATION_M_MASK;
	*reg |= mode;
	opt->mode = mode;
}

static IIO_CONST_ATTR_INT_TIME_AVAIL("0.1 0.8");

static struct attribute *opt3001_attributes[] = {
	&iio_const_attr_integration_time_available.dev_attr.attr,
	NULL
};

static const struct attribute_group opt3001_attribute_group = {
	.attrs = opt3001_attributes,
};

static const struct iio_event_spec opt3001_event_spec[] = {
	{
		.type = IIO_EV_TYPE_THRESH,
		.dir = IIO_EV_DIR_RISING,
		.mask_separate = BIT(IIO_EV_INFO_VALUE) |
			BIT(IIO_EV_INFO_ENABLE),
	},
	{
		.type = IIO_EV_TYPE_THRESH,
		.dir = IIO_EV_DIR_FALLING,
		.mask_separate = BIT(IIO_EV_INFO_VALUE) |
			BIT(IIO_EV_INFO_ENABLE),
	},
};

static const struct iio_chan_spec opt3001_channels[] = {
	{
		.type = IIO_LIGHT,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |
				BIT(IIO_CHAN_INFO_PROCESSED) |
				BIT(IIO_CHAN_INFO_INT_TIME),
		.event_spec = opt3001_event_spec,
		.num_event_specs = ARRAY_SIZE(opt3001_event_spec),
	},
	IIO_CHAN_SOFT_TIMESTAMP(1),
};

static int opt3001_get_lux(struct opt3001 *opt, int *val, int *val2)
{
	int ret;
	u16 mantissa;
	u16 reg;
	u8 exponent;
	u16 value;
	long timeout;

	if (opt->use_irq) {
		/*
		 * Enable the end-of-conversion interrupt mechanism. Note that
		 * doing so will overwrite the low-level limit value however we
		 * will restore this value later on.
		 */
		ret = i2c_smbus_write_word_swapped(opt->client,
					OPT3001_LOW_LIMIT,
					OPT3001_LOW_LIMIT_EOC_ENABLE);
		if (ret < 0) {
			dev_err(opt->dev, "failed to write register %02x\n",
					OPT3001_LOW_LIMIT);
			return ret;
		}

		/* Allow IRQ to access the device despite lock being set */
		opt->ok_to_ignore_lock = true;
	}

	/* Reset data-ready indicator flag */
	opt->result_ready = false;

	/* Configure for single-conversion mode and start a new conversion */
	ret = i2c_smbus_read_word_swapped(opt->client, OPT3001_CONFIGURATION);
	if (ret < 0) {
		dev_err(opt->dev, "failed to read register %02x\n",
				OPT3001_CONFIGURATION);
		goto err;
	}

	reg = ret;
	opt3001_set_mode(opt, &reg, OPT3001_CONFIGURATION_M_SINGLE);

	ret = i2c_smbus_write_word_swapped(opt->client, OPT3001_CONFIGURATION,
			reg);
	if (ret < 0) {
		dev_err(opt->dev, "failed to write register %02x\n",
				OPT3001_CONFIGURATION);
		goto err;
	}

	if (opt->use_irq) {
		/* Wait for the IRQ to indicate the conversion is complete */
		ret = wait_event_timeout(opt->result_ready_queue,
				opt->result_ready,
				msecs_to_jiffies(OPT3001_RESULT_READY_LONG));
	} else {
		/* Sleep for result ready time */
		timeout = (opt->int_time == OPT3001_INT_TIME_SHORT) ?
			OPT3001_RESULT_READY_SHORT : OPT3001_RESULT_READY_LONG;
		msleep(timeout);

		/* Check result ready flag */
		ret = i2c_smbus_read_word_swapped(opt->client,
						  OPT3001_CONFIGURATION);
		if (ret < 0) {
			dev_err(opt->dev, "failed to read register %02x\n",
				OPT3001_CONFIGURATION);
			goto err;
		}

		if (!(ret & OPT3001_CONFIGURATION_CRF)) {
			ret = -ETIMEDOUT;
			goto err;
		}

		/* Obtain value */
		ret = i2c_smbus_read_word_swapped(opt->client, OPT3001_RESULT);
		if (ret < 0) {
			dev_err(opt->dev, "failed to read register %02x\n",
				OPT3001_RESULT);
			goto err;
		}
		opt->result = ret;
		opt->result_ready = true;
	}

err:
	if (opt->use_irq)
		/* Disallow IRQ to access the device while lock is active */
		opt->ok_to_ignore_lock = false;

	if (ret == 0)
		return -ETIMEDOUT;
	else if (ret < 0)
		return ret;

	if (opt->use_irq) {
		/*
		 * Disable the end-of-conversion interrupt mechanism by
		 * restoring the low-level limit value (clearing
		 * OPT3001_LOW_LIMIT_EOC_ENABLE). Note that selectively clearing
		 * those enable bits would affect the actual limit value due to
		 * bit-overlap and therefore can't be done.
		 */
		value = (opt->low_thresh_exp << 12) | opt->low_thresh_mantissa;
		ret = i2c_smbus_write_word_swapped(opt->client,
						   OPT3001_LOW_LIMIT,
						   value);
		if (ret < 0) {
			dev_err(opt->dev, "failed to write register %02x\n",
					OPT3001_LOW_LIMIT);
			return ret;
		}
	}

	exponent = OPT3001_REG_EXPONENT(opt->result);
	mantissa = OPT3001_REG_MANTISSA(opt->result);

	opt3001_to_iio_ret(opt, exponent, mantissa, val, val2);

	return IIO_VAL_INT_PLUS_MICRO;
}

static int opt3001_get_count(struct opt3001 *opt, int *val, int *val2)
{
	int ret;

	ret = opt3001_get_lux(opt, val, val2);
	if (ret < 0) {
		return ret;
	}

	*val = opt3001_convert_count(opt);

	return IIO_VAL_INT;
}

static int opt3001_get_int_time(struct opt3001 *opt, int *val, int *val2)
{
	*val = 0;
	*val2 = opt->int_time;

	return IIO_VAL_INT_PLUS_MICRO;
}

static int opt3001_set_int_time(struct opt3001 *opt, int time)
{
	int ret;
	u16 reg;

	ret = i2c_smbus_read_word_swapped(opt->client, OPT3001_CONFIGURATION);
	if (ret < 0) {
		dev_err(opt->dev, "failed to read register %02x\n",
				OPT3001_CONFIGURATION);
		return ret;
	}

	reg = ret;

	switch (time) {
	case OPT3001_INT_TIME_SHORT:
		reg &= ~OPT3001_CONFIGURATION_CT;
		opt->int_time = OPT3001_INT_TIME_SHORT;
		break;
	case OPT3001_INT_TIME_LONG:
		reg |= OPT3001_CONFIGURATION_CT;
		opt->int_time = OPT3001_INT_TIME_LONG;
		break;
	default:
		return -EINVAL;
	}

	return i2c_smbus_write_word_swapped(opt->client, OPT3001_CONFIGURATION,
			reg);
}

static int opt3001_read_raw(struct iio_dev *iio,
		struct iio_chan_spec const *chan, int *val, int *val2,
		long mask)
{
	struct opt3001 *opt = iio_priv(iio);
	int ret;

	if (opt->mode == OPT3001_CONFIGURATION_M_CONTINUOUS)
		return -EBUSY;

	if (chan->type != IIO_LIGHT)
		return -EINVAL;

	mutex_lock(&opt->lock);

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		ret = opt3001_get_count(opt, val, val2);
		break;
	case IIO_CHAN_INFO_PROCESSED:
		ret = opt3001_get_lux(opt, val, val2);
		break;
	case IIO_CHAN_INFO_INT_TIME:
		ret = opt3001_get_int_time(opt, val, val2);
		break;
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&opt->lock);

	return ret;
}

static int opt3001_write_raw(struct iio_dev *iio,
		struct iio_chan_spec const *chan, int val, int val2,
		long mask)
{
	struct opt3001 *opt = iio_priv(iio);
	int ret;

	if (opt->mode == OPT3001_CONFIGURATION_M_CONTINUOUS)
		return -EBUSY;

	if (chan->type != IIO_LIGHT)
		return -EINVAL;

	if (mask != IIO_CHAN_INFO_INT_TIME)
		return -EINVAL;

	if (val != 0)
		return -EINVAL;

	mutex_lock(&opt->lock);
	ret = opt3001_set_int_time(opt, val2);
	mutex_unlock(&opt->lock);

	return ret;
}

static int opt3001_read_event_value(struct iio_dev *iio,
		const struct iio_chan_spec *chan, enum iio_event_type type,
		enum iio_event_direction dir, enum iio_event_info info,
		int *val, int *val2)
{
	struct opt3001 *opt = iio_priv(iio);
	int ret = IIO_VAL_INT_PLUS_MICRO;

	mutex_lock(&opt->lock);

	switch (dir) {
	case IIO_EV_DIR_RISING:
		opt3001_to_iio_ret(opt, opt->high_thresh_exp,
				opt->high_thresh_mantissa, val, val2);
		break;
	case IIO_EV_DIR_FALLING:
		opt3001_to_iio_ret(opt, opt->low_thresh_exp,
				opt->low_thresh_mantissa, val, val2);
		break;
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&opt->lock);

	return ret;
}

static int opt3001_write_event_value(struct iio_dev *iio,
		const struct iio_chan_spec *chan, enum iio_event_type type,
		enum iio_event_direction dir, enum iio_event_info info,
		int val, int val2)
{
	struct opt3001 *opt = iio_priv(iio);
	int ret;

	u16 mantissa;
	u16 value;
	u16 reg;

	u8 exponent;

	if (val < 0)
		return -EINVAL;

	mutex_lock(&opt->lock);

	ret = opt3001_find_scale(opt, val, val2, &exponent);
	if (ret < 0) {
		dev_err(opt->dev, "can't find scale for %d.%06u\n", val, val2);
		goto err;
	}

	mantissa = (((val * 1000) + (val2 / 1000)) / 10) >> exponent;
	value = (exponent << 12) | mantissa;

	switch (dir) {
	case IIO_EV_DIR_RISING:
		reg = OPT3001_HIGH_LIMIT;
		opt->high_thresh_mantissa = mantissa;
		opt->high_thresh_exp = exponent;
		break;
	case IIO_EV_DIR_FALLING:
		reg = OPT3001_LOW_LIMIT;
		opt->low_thresh_mantissa = mantissa;
		opt->low_thresh_exp = exponent;
		break;
	default:
		ret = -EINVAL;
		goto err;
	}

	ret = i2c_smbus_write_word_swapped(opt->client, reg, value);
	if (ret < 0) {
		dev_err(opt->dev, "failed to write register %02x\n", reg);
		goto err;
	}

err:
	mutex_unlock(&opt->lock);

	return ret;
}

static int opt3001_read_event_config(struct iio_dev *iio,
		const struct iio_chan_spec *chan, enum iio_event_type type,
		enum iio_event_direction dir)
{
	struct opt3001 *opt = iio_priv(iio);

	return opt->mode == OPT3001_CONFIGURATION_M_CONTINUOUS;
}

static int opt3001_write_event_config(struct iio_dev *iio,
		const struct iio_chan_spec *chan, enum iio_event_type type,
		enum iio_event_direction dir, int state)
{
	struct opt3001 *opt = iio_priv(iio);
	int ret;
	u16 mode;
	u16 reg;

	if (state && opt->mode == OPT3001_CONFIGURATION_M_CONTINUOUS)
		return 0;

	if (!state && opt->mode == OPT3001_CONFIGURATION_M_SHUTDOWN)
		return 0;

	mutex_lock(&opt->lock);

	mode = state ? OPT3001_CONFIGURATION_M_CONTINUOUS
		: OPT3001_CONFIGURATION_M_SHUTDOWN;

	ret = i2c_smbus_read_word_swapped(opt->client, OPT3001_CONFIGURATION);
	if (ret < 0) {
		dev_err(opt->dev, "failed to read register %02x\n",
				OPT3001_CONFIGURATION);
		goto err;
	}

	reg = ret;
	opt3001_set_mode(opt, &reg, mode);

	ret = i2c_smbus_write_word_swapped(opt->client, OPT3001_CONFIGURATION,
			reg);
	if (ret < 0) {
		dev_err(opt->dev, "failed to write register %02x\n",
				OPT3001_CONFIGURATION);
		goto err;
	}

err:
	mutex_unlock(&opt->lock);

	return ret;
}

static const struct iio_info opt3001_info = {
	.driver_module = THIS_MODULE,
	.attrs = &opt3001_attribute_group,
	.read_raw = opt3001_read_raw,
	.write_raw = opt3001_write_raw,
	.read_event_value = opt3001_read_event_value,
	.write_event_value = opt3001_write_event_value,
	.read_event_config = opt3001_read_event_config,
	.write_event_config = opt3001_write_event_config,
};

static int opt3001_read_id(struct opt3001 *opt)
{
	char manufacturer[2];
	u16 device_id;
	int ret;

	ret = i2c_smbus_read_word_swapped(opt->client, OPT3001_MANUFACTURER_ID);
	if (ret < 0) {
		dev_err(opt->dev, "failed to read register %02x\n",
				OPT3001_MANUFACTURER_ID);
		return ret;
	}

	manufacturer[0] = ret >> 8;
	manufacturer[1] = ret & 0xff;

	ret = i2c_smbus_read_word_swapped(opt->client, OPT3001_DEVICE_ID);
	if (ret < 0) {
		dev_err(opt->dev, "failed to read register %02x\n",
				OPT3001_DEVICE_ID);
		return ret;
	}

	device_id = ret;

	dev_info(opt->dev, "Found %c%c OPT%04x\n", manufacturer[0],
			manufacturer[1], device_id);

	return 0;
}

static int opt3001_configure(struct opt3001 *opt)
{
	int ret;
	u16 reg;

	ret = i2c_smbus_read_word_swapped(opt->client, OPT3001_CONFIGURATION);
	if (ret < 0) {
		dev_err(opt->dev, "failed to read register %02x\n",
				OPT3001_CONFIGURATION);
		return ret;
	}

	reg = ret;

	/* Enable automatic full-scale setting mode */
	reg &= ~OPT3001_CONFIGURATION_RN_MASK;
	reg |= OPT3001_CONFIGURATION_RN_AUTO;

	/* Reflect status of the device's integration time setting */
	if (reg & OPT3001_CONFIGURATION_CT)
		opt->int_time = OPT3001_INT_TIME_LONG;
	else
		opt->int_time = OPT3001_INT_TIME_SHORT;

	/* Ensure device is in shutdown initially */
	opt3001_set_mode(opt, &reg, OPT3001_CONFIGURATION_M_SHUTDOWN);

	/* Configure for latched window-style comparison operation */
	reg |= OPT3001_CONFIGURATION_L;
	reg &= ~OPT3001_CONFIGURATION_POL;
	reg &= ~OPT3001_CONFIGURATION_ME;
	reg &= ~OPT3001_CONFIGURATION_FC_MASK;

	ret = i2c_smbus_write_word_swapped(opt->client, OPT3001_CONFIGURATION,
			reg);
	if (ret < 0) {
		dev_err(opt->dev, "failed to write register %02x\n",
				OPT3001_CONFIGURATION);
		return ret;
	}

	ret = i2c_smbus_read_word_swapped(opt->client, OPT3001_LOW_LIMIT);
	if (ret < 0) {
		dev_err(opt->dev, "failed to read register %02x\n",
				OPT3001_LOW_LIMIT);
		return ret;
	}

	opt->low_thresh_mantissa = OPT3001_REG_MANTISSA(ret);
	opt->low_thresh_exp = OPT3001_REG_EXPONENT(ret);

	ret = i2c_smbus_read_word_swapped(opt->client, OPT3001_HIGH_LIMIT);
	if (ret < 0) {
		dev_err(opt->dev, "failed to read register %02x\n",
				OPT3001_HIGH_LIMIT);
		return ret;
	}

	opt->high_thresh_mantissa = OPT3001_REG_MANTISSA(ret);
	opt->high_thresh_exp = OPT3001_REG_EXPONENT(ret);

	return 0;
}

static irqreturn_t opt3001_irq(int irq, void *_iio)
{
	struct iio_dev *iio = _iio;
	struct opt3001 *opt = iio_priv(iio);
	int ret;
	bool wake_result_ready_queue = false;

	if (!opt->ok_to_ignore_lock)
		mutex_lock(&opt->lock);

	ret = i2c_smbus_read_word_swapped(opt->client, OPT3001_CONFIGURATION);
	if (ret < 0) {
		dev_err(opt->dev, "failed to read register %02x\n",
				OPT3001_CONFIGURATION);
		goto out;
	}

	if ((ret & OPT3001_CONFIGURATION_M_MASK) ==
			OPT3001_CONFIGURATION_M_CONTINUOUS) {
#ifdef CONFIG_LAB126
		schedule_work(&opt->opt3001_als_wq);
#else
		if (ret & OPT3001_CONFIGURATION_FH)
			iio_push_event(iio,
					IIO_UNMOD_EVENT_CODE(IIO_LIGHT, 0,
							IIO_EV_TYPE_THRESH,
							IIO_EV_DIR_RISING),
					iio_get_time_ns(iio));
		if (ret & OPT3001_CONFIGURATION_FL)
			iio_push_event(iio,
					IIO_UNMOD_EVENT_CODE(IIO_LIGHT, 0,
							IIO_EV_TYPE_THRESH,
							IIO_EV_DIR_FALLING),
					iio_get_time_ns(iio));
#endif
	} else if (ret & OPT3001_CONFIGURATION_CRF) {
		ret = i2c_smbus_read_word_swapped(opt->client, OPT3001_RESULT);
		if (ret < 0) {
			dev_err(opt->dev, "failed to read register %02x\n",
					OPT3001_RESULT);
			goto out;
		}
		opt->result = ret;
		opt->result_ready = true;
		wake_result_ready_queue = true;
	}

out:
	if (!opt->ok_to_ignore_lock)
		mutex_unlock(&opt->lock);

	if (wake_result_ready_queue)
		wake_up(&opt->result_ready_queue);

	return IRQ_HANDLED;
}

#ifdef CONFIG_OPT3001_ADJUST_WITH_BASELINE
#define ALS_CAL1_OF_PATH "/idme/alscal1"
#define ALS_CAL1_FORMAT "%d,%d,%d,%x"

/*
 *  Calibration format is
 *    alscal1: <k1>,<k2>,<k3>,<reg01>
 *
 *    Where: <All values are unsigned integers.>
 *    <k1>: ALS reads at 0 Lux.
 *    <k2>: Gain factor.
 *    <k3>: LED contamination/leakage compensation factor.
 *    <reg01>: Configuration Register
 */
static void opt3001_get_calibration(struct opt3001 *opt)
{
	struct device_node *ap = NULL;
	char *alscal1_idme = NULL;
	u32 n, k1, k2, k3, reg01;

	ap = of_find_node_by_path(ALS_CAL1_OF_PATH);
	if (ap)
		alscal1_idme = (char *)of_get_property(ap, "value", NULL);
	else {
		pr_warn("ALSCAL: could not get alscal idme entry\n");
		goto failed;
	}

	n = sscanf(alscal1_idme, ALS_CAL1_FORMAT, &k1, &k2, &k3, &reg01);

	if (n != 4) {
		pr_warn("ALSCAL: alscal1 format incorrect\n");
		goto failed;
	}

	opt->k1 = k1;
	opt->k2 = k2;
	opt->k3 = k3;
	pr_info("ALSCAL: alscal k1=%d k2=%d k3=%d\n", opt->k1, opt->k2, opt->k3);

	return;

failed:
	/* Calibration data is not available */
	opt->k1 = 0;
	opt->k2 = 20 * CONFIG_OPT3001_BASELINE_PRECISION;
	opt->k3 = 0;
	pr_info("ALSCAL: alscal default used - k1=%d k2=%d k3=%d\n", opt->k1, opt->k2, opt->k3);
}
#endif

static int opt3001_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct iio_dev *iio;
	struct opt3001 *opt;
	int irq = client->irq;
	int ret;

#ifdef CONFIG_LAB126
	struct device_node *dt = client->dev.of_node;
	int hwsku_gpio;

	hwsku_gpio = of_get_named_gpio(dt, "hwsku-gpios", 0);
	if (gpio_is_valid(hwsku_gpio)) {
		ret = gpio_request(hwsku_gpio, "hw_sku");
		if (ret < 0) {
			dev_warn(dev,
				"%s: Failed to request GPIO:%d, ERRNO:%d\n",
				__func__, (s32)hwsku_gpio, ret);
		} else {
			gpio_direction_input(hwsku_gpio);

			if (!gpio_get_value(hwsku_gpio)) {
				dev_info(dev, "%s: Premium HW Config, Probing ALS Driver.\n", __func__);
				gpio_free(hwsku_gpio);
			} else {
				dev_info(dev, "%s: Basic HW Config, Skip ALS Driver Probing.\n", __func__);
				gpio_free(hwsku_gpio);
				return -ENODEV;
			}
		}
	}else {
		dev_warn(dev, "%s: No valid hw sku gpio\n", __func__);
	}
#endif

	iio = devm_iio_device_alloc(dev, sizeof(*opt));
	if (!iio)
		return -ENOMEM;

	opt = iio_priv(iio);
	opt->client = client;
	opt->dev = dev;

	mutex_init(&opt->lock);
	init_waitqueue_head(&opt->result_ready_queue);
	i2c_set_clientdata(client, iio);

	ret = opt3001_read_id(opt);
	if (ret)
		return ret;

	ret = opt3001_configure(opt);
	if (ret)
		return ret;

	iio->name = client->name;
	iio->channels = opt3001_channels;
	iio->num_channels = ARRAY_SIZE(opt3001_channels);
	iio->dev.parent = dev;
	iio->modes = INDIO_DIRECT_MODE;
	iio->info = &opt3001_info;

	ret = devm_iio_device_register(dev, iio);
	if (ret) {
		dev_err(dev, "failed to register IIO device\n");
		return ret;
	}

#ifdef CONFIG_OPT3001_ADJUST_WITH_BASELINE
	opt3001_get_calibration(opt);
#endif

#ifdef CONFIG_LAB126
	if (misc_register(&als_opt3001_misc_device)) {
		dev_err(dev, "%s Couldn't register device %d \n", __func__, OPT3001_ALS_DEV_MINOR);
		return -EBUSY;
	}
	als_opt3001_misc_device.parent = dev;

	INIT_WORK(&opt->opt3001_als_wq, opt3001_als_wqthread);
#endif

	/* Make use of INT pin only if valid IRQ no. is given */
	if (irq > 0) {
		ret = request_threaded_irq(irq, NULL, opt3001_irq,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"opt3001", iio);
		if (ret) {
			dev_err(dev, "failed to request IRQ #%d\n", irq);
#ifdef CONFIG_LAB126
			goto misc_register_fail;
#else
			return ret;
#endif
		}
		opt->use_irq = true;
	} else {
		dev_dbg(opt->dev, "enabling interrupt-less operation\n");
	}

	return 0;

#ifdef CONFIG_LAB126
misc_register_fail:
	misc_deregister(&als_opt3001_misc_device);

	return ret;
#endif
}

static int opt3001_remove(struct i2c_client *client)
{
	struct iio_dev *iio = i2c_get_clientdata(client);
	struct opt3001 *opt = iio_priv(iio);
	int ret;
	u16 reg;

	if (opt->use_irq)
		free_irq(client->irq, iio);

#ifdef CONFIG_LAB126
	misc_deregister(&als_opt3001_misc_device);
#endif

	ret = i2c_smbus_read_word_swapped(opt->client, OPT3001_CONFIGURATION);
	if (ret < 0) {
		dev_err(opt->dev, "failed to read register %02x\n",
				OPT3001_CONFIGURATION);
		return ret;
	}

	reg = ret;
	opt3001_set_mode(opt, &reg, OPT3001_CONFIGURATION_M_SHUTDOWN);

	ret = i2c_smbus_write_word_swapped(opt->client, OPT3001_CONFIGURATION,
			reg);
	if (ret < 0) {
		dev_err(opt->dev, "failed to write register %02x\n",
				OPT3001_CONFIGURATION);
		return ret;
	}

	return 0;
}

static int opt3001_suspend(struct device *dev)
{
	dev_info(dev, "suspend()\n");

	pinctrl_pm_select_sleep_state(dev);

	return 0;
}

static int opt3001_resume(struct device *dev)
{
	struct iio_dev *iio = dev_get_drvdata(dev);
	struct opt3001 *opt = iio_priv(iio);
	int ret;

	pinctrl_pm_select_default_state(dev);

	dev_info(dev, "resume()\n");

	ret = opt3001_configure(opt);

	return ret;
}


static const struct i2c_device_id opt3001_id[] = {
	{ "opt3001", 0 },
	{ "opt3006", 1 },
	{ } /* Terminating Entry */
};
MODULE_DEVICE_TABLE(i2c, opt3001_id);

static const struct of_device_id opt3001_of_match[] = {
	{ .compatible = "ti,opt3001" },
	{ .compatible = "ti,opt3006" },
	{ }
};
MODULE_DEVICE_TABLE(of, opt3001_of_match);

static const struct dev_pm_ops opt3001_pm_ops = {
	.suspend = opt3001_suspend,
	.resume = opt3001_resume,
};

static struct i2c_driver opt3001_driver = {
	.probe = opt3001_probe,
	.remove = opt3001_remove,
	.id_table = opt3001_id,

	.driver = {
		.name = "opt3001",
		.of_match_table = of_match_ptr(opt3001_of_match),
		.pm = &opt3001_pm_ops,
	},
};

module_i2c_driver(opt3001_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Andreas Dannenberg <dannenberg@ti.com>");
MODULE_DESCRIPTION("Texas Instruments OPT3001 Light Sensor Driver");
