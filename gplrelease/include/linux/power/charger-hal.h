/************************************************************
 *
 * file: charger_hal.h
 *
 * Description: Eink charger HAL Header
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

#ifndef __CHARGER_HAL_H__
#define __CHARGER_HAL_H__

#include <linux/power_supply.h>

/* charger hal pmic device operations */
struct charger_hal_pmic_ops {
    int (*current_event_handler)(unsigned long type, void *ptr);
};

/* charger hal pmic device struct */
struct charger_hal_pmic_device {
    struct device *parent;
    struct mutex ops_lock;
    struct charger_hal_pmic_ops *pmic_ops;
};

/* pmic device register to charger hal */
extern struct charger_hal_pmic_device *charger_hal_pmic_device_register(struct device *parent,
				struct charger_hal_pmic_ops *pmic_ops);
/* pmic device remove from charger hal */
extern void charger_hal_pmic_device_unregister(struct charger_hal_pmic_device *pmic_device);

/* charger update status callbacks */
extern void charger_hal_charger_update_status(enum power_supply_type type, int current_limit, int online);

#endif
