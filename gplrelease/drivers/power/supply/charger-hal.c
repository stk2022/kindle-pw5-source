/************************************************************
 *
 * file: charger_hal.c
 *
 * Description: Eink charger HAL
 *
 *------------------------------------------------------------
 *
 * Copyright (C) 2020 Amazon.com Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/power/charger-hal.h>

#define MAX_CURRENT_SDP_DEFAULT			 500		/* mA */

struct charger_hal_charger_device {
	int online;
	enum power_supply_type type;
	unsigned int cur_limit;
};

static struct charger_hal_pmic_device *pmic_dev = NULL;

/* charger status update callbacks */
void charger_hal_charger_update_status(enum power_supply_type type, int current_limit, int online)
{
	static struct charger_hal_charger_device cur_chg_dev = { 0 };

	if (!pmic_dev  || !pmic_dev->pmic_ops)		// No PMIC registered or operations available
		return;

	pr_info("%s: Charger online:%d, type: %d, current_limit: %d.\n", __func__, online, type, current_limit);

	mutex_lock(&pmic_dev->ops_lock);

	if (!cur_chg_dev.online) {
		/* current charger status is offline */
		if (online) {
			cur_chg_dev.online = online;
			cur_chg_dev.type = type;
			cur_chg_dev.cur_limit = current_limit;
			pmic_dev->pmic_ops->current_event_handler(cur_chg_dev.type, (void *)cur_chg_dev.cur_limit);
		}
	} else {
		/* current charger status is online */
		if (cur_chg_dev.type == POWER_SUPPLY_TYPE_WIRELESS || type != POWER_SUPPLY_TYPE_WIRELESS) {
			/* USB chargers have higher priority over wireless chargers.
				Do nothing if USB chargers are online currently. */
			if (online) {
				cur_chg_dev.online = online;
				cur_chg_dev.type = type;
				cur_chg_dev.cur_limit = current_limit;
				pmic_dev->pmic_ops->current_event_handler(cur_chg_dev.type, (void *)cur_chg_dev.cur_limit);
			} else {
				if (cur_chg_dev.type != POWER_SUPPLY_TYPE_WIRELESS || type == POWER_SUPPLY_TYPE_WIRELESS) {
					cur_chg_dev.online = online;
					cur_chg_dev.type = POWER_SUPPLY_TYPE_UNKNOWN;
					cur_chg_dev.cur_limit = MAX_CURRENT_SDP_DEFAULT;
					pmic_dev->pmic_ops->current_event_handler(cur_chg_dev.type, (void *)cur_chg_dev.cur_limit);
				}
			}
		}
	}

	mutex_unlock(&pmic_dev->ops_lock);

	return;
}

/* pmic device register to charger hal */
struct charger_hal_pmic_device *charger_hal_pmic_device_register(struct device *parent,
				struct charger_hal_pmic_ops *pmic_ops)
{
	pr_info("Charger HAL PMIC register.\n");

	if (!pmic_dev) {
		pmic_dev = kzalloc(sizeof(*pmic_dev), GFP_KERNEL);
		if (!pmic_dev)
			return ERR_PTR(-ENOMEM);

		mutex_init(&pmic_dev->ops_lock);
		pmic_dev->parent = parent;
		pmic_dev->pmic_ops = pmic_ops;
	} else {
		pr_info("%s: A PMIC Device already registered.\n", __FILE__);
		return ERR_PTR(-EBUSY);
	}

	return pmic_dev;
}

/* pmic device remove from charger hal */
void charger_hal_pmic_device_unregister(struct charger_hal_pmic_device *pmic_device)
{
	pr_info("Charger HAL PMIC unregister.\n");

	if (pmic_dev) {
		kfree(pmic_dev);
		pmic_dev = NULL;
	}

	return;
}

static int __init charger_hal_init(void)
{
	pr_info("Charger HAL driver init.\n");

	return 0;
}

static void __exit charger_hal_exit(void)
{
	if (pmic_dev) {
		charger_hal_pmic_device_unregister(pmic_dev);
	}

	pr_info("Charger HAL driver exit.\n");

	return;
}


module_init(charger_hal_init);
module_exit(charger_hal_exit);

MODULE_DESCRIPTION("Eink power supply charger hal");
MODULE_AUTHOR("Charlie Yao <charlyao@amazon.com>");
MODULE_LICENSE("GPL");
