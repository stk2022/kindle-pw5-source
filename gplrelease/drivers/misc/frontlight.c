/*
 * Copyright (c) 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/mxcfb.h>
#include <linux/frontlight.h>

#define FRONTLIGHT_DRIVER_NAME	"frontlight"
#define FRONTLIGHT_MINOR	162

static DEFINE_MUTEX(core_lock);
static struct backlight_device *backlight_devices[FRONTLIGHT_COLOR_LAST] = { NULL };
EXPORT_SYMBOL(backlight_devices);
static struct miscdevice frontlight_misc_device;

struct fl_nightmode_ctrl {
	struct delayed_work fl_nm_work;
	int start[FRONTLIGHT_COLOR_LAST];	/* reduced to level for gck16 */
	int stride[FRONTLIGHT_COLOR_LAST];	/* back to original level gradually: default */
	int current_level[FRONTLIGHT_COLOR_LAST];	/* current brighness setting */
};

static struct fl_nightmode_ctrl fl_nm_ctrl;
#define STRIDE_STEPS	10

static int frontlight_set_brightness(struct backlight_device *bl_dev, int brightness, int from_ui)
{
	int ret = -EINVAL;

	if (!unlikely(bl_dev))
		return ret;

	mutex_lock(&bl_dev->ops_lock);
	if (bl_dev->ops) {
		if (unlikely(brightness > bl_dev->props.max_brightness)) {
			pr_err("%s: brightness larger than max: %u\n", dev_name(&bl_dev->dev), brightness);
			ret = -EINVAL;
		} else {
			pr_debug("%s: set brightness to %u\n", dev_name(&bl_dev->dev), brightness);
			bl_dev->props.brightness = brightness;
			backlight_update_status(bl_dev);
			if (from_ui)
				bl_dev->props.ui_brightness = brightness;
			ret = 0;
		}
	}
	mutex_unlock(&bl_dev->ops_lock);

	return ret;
}

static int frontlight_set_brightness_force(struct backlight_device *bl_dev, int brightness, int from_ui)
{
	int ret = -EINVAL;

	if (!unlikely(bl_dev))
		return ret;

	mutex_lock(&bl_dev->ops_lock);
	if (bl_dev->ops) {
		if (unlikely(brightness > bl_dev->props.max_brightness)) {
			pr_err("%s: brightness larger than max: %u\n", dev_name(&bl_dev->dev), brightness);
			ret = -EINVAL;
		} else {
			pr_debug("%s: set brightness to %u\n", dev_name(&bl_dev->dev), brightness);
			mutex_lock(&bl_dev->update_lock);
			bl_dev->ops->get_brightness(bl_dev);
			bl_dev->props.brightness = brightness;
			bl_dev->ops->update_status(bl_dev);
			if (from_ui)
				bl_dev->props.ui_brightness = brightness;
			mutex_unlock(&bl_dev->update_lock);
			ret = 0;
		}
	}
	mutex_unlock(&bl_dev->ops_lock);

	return ret;
}

static inline int frontlight_get_brightness(struct backlight_device *bl_dev, int *brightness)
{
	if (!unlikely(bl_dev))
		return -EINVAL;

	*brightness = bl_dev->props.brightness;

	return 0;
}

static inline int frontlight_get_max_brightness(struct backlight_device *bl_dev, int *brightness)
{
	if (!unlikely(bl_dev))
		return -EINVAL;

	*brightness = bl_dev->props.max_brightness;

	return 0;
}

static void fl_nm_work_func(struct work_struct *work)
{
	int bright[FRONTLIGHT_COLOR_LAST] = { 0 };
	int stride[FRONTLIGHT_COLOR_LAST] = { 0 };
	int steps[FRONTLIGHT_COLOR_LAST] = { 0 };
	int fl_color;
	int stride_step;

	for (fl_color = (FRONTLIGHT_COLOR_LAST -1); fl_color >= 0; fl_color--) {
		if (!backlight_devices[fl_color])
			continue;

		bright[fl_color] = fl_nm_ctrl.current_level[fl_color];

		if (!bright[fl_color] )
			continue;

		stride[fl_color] = (bright[fl_color] - fl_nm_ctrl.start[fl_color]) / STRIDE_STEPS;
		steps[fl_color] = fl_nm_ctrl.start[fl_color] + stride[fl_color];
		pr_debug("%s %d: steps[%d]=%d, stride[%d]=%d\n", __func__, __LINE__,
				fl_color, steps[fl_color], fl_color, stride[fl_color]);
	}

	for (stride_step = 0; stride_step <= STRIDE_STEPS; stride_step++) {
		for (fl_color = (FRONTLIGHT_COLOR_LAST -1); fl_color >= 0; fl_color--) {
			if (!bright[fl_color])
				continue;

			if (stride_step == STRIDE_STEPS) {
				steps[fl_color] = bright[fl_color];
			}

			frontlight_set_brightness(backlight_devices[fl_color], steps[fl_color], 0);
			steps[fl_color] += stride[fl_color];
		}

		msleep(1);
	}
}

/* function for epdc driver to control backlight directly */
/*   on_off: 1, turn on; 0: turn off                      */
/*   fl_color: 0: white; 1: amber                         */
int fl_switch(struct mxcfb_nightmode_ctrl *night_mode, const bool on_off)
{
	struct backlight_device *bl_dev;
	int fl_color;

	if (unlikely(night_mode->disable))
		return 0;

	pr_debug("%s: turn %s\n", __func__, on_off ? "on": "off");
	if (!on_off) {
		for (fl_color = (FRONTLIGHT_COLOR_LAST -1); fl_color >= 0; fl_color--) {
			bl_dev = backlight_devices[fl_color];

			if (!bl_dev)
				continue;

			fl_nm_ctrl.start[fl_color] = night_mode->start;
			pr_debug("%s:  start[%d]=%d\n", __func__, fl_color,  fl_nm_ctrl.start[fl_color]);

			if (bl_dev->props.ui_brightness == 0)
				continue;

			frontlight_set_brightness(bl_dev, fl_nm_ctrl.start[fl_color], false);
		}
	} else {
		for (fl_color = (FRONTLIGHT_COLOR_LAST -1); fl_color >= 0; fl_color--) {
			bl_dev = backlight_devices[fl_color];

			if (!bl_dev)
				continue;

			fl_nm_ctrl.current_level[fl_color] = bl_dev->props.ui_brightness;
			pr_debug("%s:  current[%d]=%d\n", __func__, fl_color, fl_nm_ctrl.current_level[fl_color]);
		}

		schedule_delayed_work(&fl_nm_ctrl.fl_nm_work, msecs_to_jiffies(5));
	}

	return 0;
}
EXPORT_SYMBOL(fl_switch);

static long frontlight_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int __user *argp = (int __user *)arg;
	int ret = -EINVAL;
	int brightness = 0;

	switch (cmd) {
		case FL_IOCTL_SET_INTENSITY_FORCED:
			if (get_user(brightness, argp))
				ret = -EFAULT;
			else
				ret = frontlight_set_brightness_force(backlight_devices[FRONTLIGHT_COLOR_WHITE], brightness, true);
			break;
		case FL_IOCTL_SET_INTENSITY:
			if (get_user(brightness, argp))
				ret = -EFAULT;
			else
				ret = frontlight_set_brightness(backlight_devices[FRONTLIGHT_COLOR_WHITE], brightness, true);
			break;
		case FL_IOCTL_GET_INTENSITY:
			if (frontlight_get_brightness(backlight_devices[FRONTLIGHT_COLOR_WHITE], &brightness) == 0) {
				if (put_user(brightness, argp))
					ret = -EFAULT;
				else
					ret = 0;
			}
			break;
		case FL_IOCTL_GET_RANGE_MAX:
			if (frontlight_get_max_brightness(backlight_devices[FRONTLIGHT_COLOR_WHITE], &brightness) == 0) {
				if (put_user(brightness, argp))
					ret = -EFAULT;
				else
					ret = 0;
			}
			break;
		case FL_IOCTL_SET_INTENSITY_FORCED_AMBER:
			if (get_user(brightness, argp))
				ret = -EFAULT;
			else
				ret = frontlight_set_brightness_force(backlight_devices[FRONTLIGHT_COLOR_AMBER], brightness, true);
			break;
		case FL_IOCTL_SET_INTENSITY_AMBER:
			if (get_user(brightness, argp))
				ret = -EFAULT;
			else
				ret = frontlight_set_brightness(backlight_devices[FRONTLIGHT_COLOR_AMBER], brightness, true);
			break;
		case FL_IOCTL_GET_INTENSITY_AMBER:
			if (frontlight_get_brightness(backlight_devices[FRONTLIGHT_COLOR_AMBER], &brightness) == 0) {
				if (put_user(brightness, argp))
					ret = -EFAULT;
				else
					ret = 0;
			}
			break;
		case FL_IOCTL_GET_RANGE_MAX_AMBER:
			if (frontlight_get_max_brightness(backlight_devices[FRONTLIGHT_COLOR_AMBER], &brightness) == 0) {
				if (put_user(brightness, argp))
					ret = -EFAULT;
				else
					ret = 0;
			}
			break;
		default:
			break;
	}

	return ret;
}

static ssize_t fl_misc_write(struct file *file, const char __user *buf,
		size_t count, loff_t *pos)
{
	return 0;
}

static ssize_t fl_misc_read(struct file *file, char __user *buf,
		size_t count, loff_t *pos)
{
	return 0;
}

static const struct file_operations frontlight_misc_fops =
{
	.owner = THIS_MODULE,
	.read  = fl_misc_read,
	.write = fl_misc_write,
	.unlocked_ioctl = frontlight_ioctl,
};

static struct miscdevice frontlight_misc_device =
{
	.minor = FRONTLIGHT_MINOR,
	.name  = FRONTLIGHT_DRIVER_NAME,
	.fops  = &frontlight_misc_fops,
};

int frontlight_register(struct backlight_device *device, enum frontlight_color fl_color)
{
	struct backlight_device **bl_dev;
	int ret = 0;

	if (!unlikely(device))
		return -EINVAL;

	if (fl_color >= FRONTLIGHT_COLOR_LAST)
		return -EINVAL;

	bl_dev = &backlight_devices[fl_color];;

	mutex_lock(&core_lock);
	if (unlikely(*bl_dev)) {
		pr_err("another device (%s:%s) already registered.\n",
				dev_driver_string(&(*bl_dev)->dev), dev_name(&(*bl_dev)->dev));
		ret = -EBUSY;
	} else {
		*bl_dev = device;
		pr_info("new frontlight device (%s:%s) registered.\n",
				dev_driver_string(&(*bl_dev)->dev), dev_name(&(*bl_dev)->dev));
	}
	mutex_unlock(&core_lock);

	return ret;
}
EXPORT_SYMBOL(frontlight_register);

static int __init frontlight_dev_init(void)
{
	int res;

	printk(KERN_INFO "frontlight dev entries driver.\n");
	res = misc_register(&frontlight_misc_device);
	if (res) {
		pr_err("misc frontlight: Coulnd't register device %d.\n", frontlight_misc_device.minor);
		goto out;
	}

	INIT_DELAYED_WORK(&fl_nm_ctrl.fl_nm_work, fl_nm_work_func);

	return 0;

out:
	printk(KERN_ERR "%s: Driver Initialisation failed\n", __FILE__);
	return res;
}

static void __exit frontlight_dev_exit(void)
{
	int i;

	for (i = 0; i < FRONTLIGHT_COLOR_LAST; i++)
		backlight_devices[i] = NULL;

	misc_deregister(&frontlight_misc_device);
}

MODULE_AUTHOR("Amazon Lab126");
MODULE_DESCRIPTION("Frontlight dev entries driver");
MODULE_LICENSE("GPL");

module_init(frontlight_dev_init);
module_exit(frontlight_dev_exit);