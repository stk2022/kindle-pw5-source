// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2019 ROHM Semiconductors

#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mfd/rohm-bd71828.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#ifdef CONFIG_FALCON
#include <asm/falcon_syscall.h>
#endif

#define MAGIC_GRACE_TIME	3000000000ULL
#define CLOSE_DELAY	250000000ULL


extern int idme_hwid_value;

/* Hall Sensor States */
typedef enum _hall_state {
	HALL_OPEN = 0,
	HALL_CLOSE
} hall_state_t;

struct bd71828_hall {
	struct regmap *regmap;
	struct device *dev;
	unsigned int open_state;
	hall_state_t hall_state;
	unsigned int hall_dbg;
	bool enabled:1;
	bool hibernation_only:1;
	struct delayed_work delayed_event;
};

static void hall_send_open_event(struct bd71828_hall *hall)
{
	char *envp[] = {"HALLSENSOR=opened", NULL};

	hall->hall_state = HALL_OPEN;
	if (hall->hall_dbg)
		dev_info(hall->dev, "KERNEL: I hall:open event:::current_state=%d\n", hall->hall_state);
	kobject_uevent_env(&hall->dev->kobj, KOBJ_ONLINE, envp);
}

static void hall_send_close_event(struct bd71828_hall *hall)
{
	char *envp[] = {"HALLSENSOR=closed", NULL};

	hall->hall_state = HALL_CLOSE;
	if (hall->hall_dbg)
		dev_info(hall->dev, "KERNEL: I hall:close event:::current_state=%d\n", hall->hall_state);
	kobject_uevent_env(&hall->dev->kobj, KOBJ_OFFLINE, envp);
}

static int lid_is_open(struct bd71828_hall *hall)
{
	unsigned int reg;
	int ret;

	ret = regmap_read(hall->regmap, BD71828_REG_IO_STAT, &reg);
	if (ret) {
		dev_err(hall->dev, "getting HALL status failed\n");
		return ret;
	}
	return (hall->open_state == (reg & BD71828_HALL_STATE_MASK));
}

static irqreturn_t hall_hndlr(int irq, void *data)
{
	struct bd71828_hall *hall = data;
	int open;
	static u64 open_time;

	/*
	 * Do we need this state flag or can we simply disable irq if hall is
	 * not enabled? Do we need to keep the IRQ enabled for wake to work?
	 */
	if (!hall->enabled)
		return IRQ_HANDLED;

	cancel_delayed_work_sync(&hall->delayed_event);

	/*
	 * We return IRQ none if reading fails as this may be a sign
	 * of broken HW and we want to avoid irq-storm which prevents debugging
	 */
	open = lid_is_open(hall);
	if (open < 0)
		return IRQ_NONE;

	if (!open) {
		hall_send_open_event(hall);
		/*
		 * We have IRQF_ONESHOT specified - it should be safe to omit
		 * locking
		 */
		open_time = ktime_get_ns();
	} else {
		/* schedule a delayed work to drop events in MAGIC_GRACE_TIME ms */
		u64 now = ktime_get_ns();
		unsigned int left;

		now -= open_time;
		left = ((unsigned int)(MAGIC_GRACE_TIME - now)) / 1000000;
		if (now < MAGIC_GRACE_TIME && left)
			schedule_delayed_work(&hall->delayed_event,
					      msecs_to_jiffies(left));
		else
			hall_send_close_event(hall);
	}

	return IRQ_HANDLED;
}

static ssize_t hall_debug_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct bd71828_hall *hall = dev_get_drvdata(dev);
	int value = 0;
	if (sscanf(buf, "%d", &value) <= 0) {
		dev_err(dev, "could not update hall debug ctrl \n");
		return  -EINVAL;
	}
	hall->hall_dbg = (value > 0) ? 1 : 0;
	return size;
}

static ssize_t hall_debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct bd71828_hall *hall = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", hall->hall_dbg);
}
static DEVICE_ATTR(hall_debug, 0644, hall_debug_show, hall_debug_store);


static ssize_t hall_detect_show(struct device *dev, struct device_attribute *attr, char *buf)
{

	struct bd71828_hall *hall = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", hall->hall_state);
}
static DEVICE_ATTR(hall_detect, 0444, hall_detect_show, NULL);

static ssize_t hall_hw_state_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	int ret;
	int open;
	struct bd71828_hall *hall = dev_get_drvdata(dev);

	open = lid_is_open(hall);
	if (open < 0)
		return open;

	ret = sprintf(buf, "%d", open);

	return ret;
}

static DEVICE_ATTR(hall_hw_state, 0444, hall_hw_state_show, NULL);

static ssize_t hall_enable_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t size)
{
	int ret;
	unsigned long int val;
	struct bd71828_hall *hall = dev_get_drvdata(dev);

	/*
	 *  Could we just enable/disable the IRQ here? Or do we need IRQ enabled
	 * for wake?
	 */
	ret = kstrtoul(buf, 0, &val);

	if (ret)
		return ret;

	hall->enabled = !!val;

	return size;
}

static ssize_t hall_enable_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct bd71828_hall *hall = dev_get_drvdata(dev);

	/*
	 *  Could we just use IRQ enable/disable state here? Or do we need IRQ
	 *  enabled for wake?
	 */
	return sprintf(buf, "%d\n", hall->enabled);
}

static DEVICE_ATTR(hall_enable, 0644, hall_enable_show, hall_enable_store);

struct attribute *bd71828_attrs[] = {
	&dev_attr_hall_debug.attr,
	&dev_attr_hall_detect.attr,
	&dev_attr_hall_hw_state.attr,
	&dev_attr_hall_enable.attr,
	NULL,
};

struct attribute_group bd71828_att_grp = {
	.attrs = bd71828_attrs,
};

static int bd71828_remove(struct platform_device *pdev)
{
	struct bd71828_hall *hall = dev_get_drvdata(&pdev->dev);

	sysfs_remove_group(&pdev->dev.kobj, &bd71828_att_grp);
	cancel_delayed_work_sync(&hall->delayed_event);

	return 0;
}

static void hall_close_work_fun(struct work_struct *work)
{
	struct bd71828_hall *hall = container_of(work, struct bd71828_hall,
						 delayed_event.work);

	hall_send_close_event(hall);
}

static int bd71828_probe(struct platform_device *pdev)
{
	int irq, ret;
	struct bd71828_hall *hall;
	struct rohm_regmap_dev *mfd;

	dev_info(&pdev->dev, "%s\n", __func__);
	mfd = dev_get_drvdata(pdev->dev.parent);
	if (!mfd) {
		dev_err(&pdev->dev, "No MFD driver data\n");
		return -EINVAL;
	}
	hall = devm_kzalloc(&pdev->dev, sizeof(*hall), GFP_KERNEL);
	if (!hall)
		return -ENOMEM;

	hall->regmap = mfd->regmap;
	hall->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, hall);

	hall->enabled = true;
	hall->hibernation_only = false;
#ifdef CONFIG_IDME
	if (idme_hwid_value > 4 || idme_hwid_value == 1) {
		/* we do not need interrupt from PMIC HALL,
		 * but we need pm_resume for hibernation to send uevent */
		dev_info(&pdev->dev, "HWID: %d, do not enable interrupt\n", idme_hwid_value);
		hall->enabled = false;
		hall->hibernation_only = true;
		return 0;
	}
#endif

	if (of_property_read_bool(pdev->dev.parent->of_node,
				  "rohm,lid-open-high"))
		hall->open_state = BD71828_HALL_STATE_MASK;

	hall->hall_dbg = 0;

	INIT_DELAYED_WORK(&hall->delayed_event, hall_close_work_fun);

	irq = platform_get_irq_byname(pdev, "bd71828-hall");
	ret = devm_request_threaded_irq(&pdev->dev, irq, NULL, &hall_hndlr,
					IRQF_ONESHOT, "bd70528-hall", hall);
	if (ret)
		return ret;

	ret = sysfs_create_group(&pdev->dev.kobj, &bd71828_att_grp);

	return ret;
}

#ifdef CONFIG_FALCON
static int bd71828_hall_resume(struct device *dev)
{
	struct bd71828_hall *hall = dev_get_drvdata(dev);
	unsigned int reg;
	int ret;

	if (hall->hibernation_only && in_falcon() && is_falcon_boot_status() == BS_BOOT) {
		/* Send cover open event if waking up with hall */
		ret = regmap_read(hall->regmap, BD71828_REG_BOOTSRC, &reg);
		dev_dbg(hall->dev, "%s(%d) BD71828_REG_BOOTSRC=0x%x\n", __func__, __LINE__, reg);
		if (ret) {
			dev_err(hall->dev, "failed to get BOOTSRC\n");
			return ret;
		}
		if (reg & BD71828_MASK_HALL_DET_BT) {
			hall_send_open_event(hall);
			dev_dbg(hall->dev, "%s(%d) sent hall open event.\n", __func__, __LINE__);
		}
	}
	return 0;
}

static const struct dev_pm_ops bd71828_hall_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(NULL, bd71828_hall_resume)
};
#endif

static struct platform_driver bd71828_hall = {
	.driver = {
		.name = "bd71828-lid-eink_hall",
#ifdef CONFIG_FALCON
		.pm = &bd71828_hall_pm_ops,
#endif
	},
	.probe = bd71828_probe,
	.remove = bd71828_remove,
};

module_platform_driver(bd71828_hall);

MODULE_AUTHOR("Matti Vaittinen <matti.vaittinen@fi.rohmeurope.com>");
MODULE_DESCRIPTION("BD71828 LID event driver");
MODULE_LICENSE("GPL");
