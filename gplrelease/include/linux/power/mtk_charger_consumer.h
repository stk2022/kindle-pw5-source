/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef LINUX_POWER_MTK_CHARGER_CONSUMER_H
#define LINUX_POWER_MTK_CHARGER_CONSUMER_H

/* charger_manager notify charger_consumer */
enum {
	CHGMGR_NOTIFY_EOC,
	CHGMGR_NOTIFY_START_CHARGING,
	CHGMGR_NOTIFY_STOP_CHARGING,
	CHGMGR_NOTIFY_ERROR,
	CHGMGR_NOTIFY_NORMAL,
};

enum mtk_charger_role {
	CHG_ROLE_MAIN,
	/* Add more if necessary */
};

struct charger_consumer {
	struct device *dev;
	void *cm;
	struct notifier_block *nb;
	struct list_head list;
};

extern struct charger_consumer *mtk_charger_manager_get_consumer(
	struct device *dev);
extern int mtk_charger_manager_register_notifier(
	struct charger_consumer *consumer, struct notifier_block *nb);
extern int mtk_charger_manager_unregister_notifier(
	struct charger_consumer *consumer, struct notifier_block *nb);
extern int mtk_charger_manager_get_tbat(struct charger_consumer *consumer,
					int *tbat);
#endif /* LINUX_POWER_MTK_CHARGER_CONSUMER_H */
