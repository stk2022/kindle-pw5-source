/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __MTK_CHARGER_H
#define __MTK_CHARGER_H

#include <linux/device.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/wait.h>

#include <mt-plat/charger_type.h>
#include <linux/power/mtk_charger_consumer.h>
#include <linux/power/mtk_charger_class.h>

/* Second */
#define CHG_POLLING_INTERVAL		10
#define CHGFULL_POLLING_INTERVAL	20

extern int chg_get_log_level(void);
#define CHGLOG_LEVEL_ERROR	1
#define CHGLOG_LEVEL_INFO	2
#define CHGLOG_LEVEL_DEBUG	3

#define chg_err(fmt, args...)					\
do {								\
	if (chg_get_log_level() >= CHGLOG_LEVEL_ERROR)		\
		pr_err(fmt, ##args);				\
} while (0)

#define chg_info(fmt, args...)					\
do {								\
	if (chg_get_log_level() >= CHGLOG_LEVEL_INFO)		\
		pr_info(fmt, ##args);				\
} while (0)

#define chg_debug(fmt, args...)					\
do {								\
	if (chg_get_log_level() >= CHGLOG_LEVEL_DEBUG)		\
		pr_debug(fmt, ##args);				\
} while (0)

#define CHG_STATE_CC	(0x0001)
#define CHG_STATE_FULL	(0x0002)
#define CHG_STATE_ERROR	(0x0003)

/* charging abnormal status */
#define CHG_VBUS_OV_STATUS	(1 << 0)
#define CHG_BAT_OT_STATUS	(1 << 1)
#define CHG_OC_STATUS		(1 << 2)
#define CHG_BAT_OV_STATUS	(1 << 3)
#define CHG_ST_TMO_STATUS	(1 << 4)
#define CHG_BAT_LT_STATUS	(1 << 5)

/*
 * Software JEITA
 * T0: -10 degree Celsius
 * T1: 0 degree Celsius
 * T2: 10 degree Celsius
 * T3: 45 degree Celsius
 * T4: 50 degree Celsius
 */
enum sw_jeita_state_enum {
	TEMP_BELOW_T0 = 0,
	TEMP_T0_TO_T1,
	TEMP_T1_TO_T2,
	TEMP_T2_TO_T3,
	TEMP_T3_TO_T4,
	TEMP_ABOVE_T4
};

struct sw_jeita_data {
	int sm;
	int pre_sm;
	int cv;
	bool charging;
	bool error_recovery_flag;
};

/* battery thermal protection */
enum bat_temp_state_enum {
	BAT_TEMP_LOW = 0,
	BAT_TEMP_NORMAL,
	BAT_TEMP_HIGH,
};

struct battery_thermal_protection_data {
	int sm;
	bool enable_min_charge_temp;
	int min_charge_temp;
	int min_charge_temp_plus_x_degree;
	int max_charge_temp;
	int max_charge_temp_minus_x_degree;
};

struct charger_custom_data {
	int battery_cv;
	int max_charger_voltage;
	int max_charger_voltage_setting;
	int min_charger_voltage;

	int usb_charger_current_suspend;
	int usb_charger_current_unconfigured;
	int usb_charger_current_configured;
	int usb_charger_current;
	int ac_charger_current;
	int ac_charger_input_current;
	int non_std_ac_charger_current;
	int charging_host_charger_current;
	int apple_1_0a_charger_current;
	int apple_2_1a_charger_current;
	int ta_ac_charger_current;
	int pd_charger_current;

	/* sw jeita */
	int jeita_temp_above_t4_cv;
	int jeita_temp_t3_to_t4_cv;
	int jeita_temp_t2_to_t3_cv;
	int jeita_temp_t1_to_t2_cv;
	int jeita_temp_t0_to_t1_cv;
	int jeita_temp_below_t0_cv;
	int temp_t4_thres;
	int temp_t4_thres_minus_x_degree;
	int temp_t3_thres;
	int temp_t3_thres_minus_x_degree;
	int temp_t2_thres;
	int temp_t2_thres_plus_x_degree;
	int temp_t1_thres;
	int temp_t1_thres_plus_x_degree;
	int temp_t0_thres;
	int temp_t0_thres_plus_x_degree;
	int temp_neg_10_thres;

	int max_charging_time; /* second */
};

struct charger_data {
	int force_charging_current;
	int thermal_input_current_limit;
	int thermal_charging_current_limit;
	int input_current_limit;
	int charging_current_limit;
	int disable_charging_count;
	int input_current_limit_by_aicl;
	int junction_temp_min;
	int junction_temp_max;
};

struct charger_manager {
	struct device *dev;

	bool init_done; /* probe complete */
	struct charger_custom_data *cust_data; /* charger configuration */
	struct battery_thermal_protection_data *thermal; /* battery thermal */
	const char *algorithm_name;
	void *algorithm_data;

	int usb_state;
	bool usb_unlimited;
	bool disable_charger;
	struct charger_device *chg1_dev;
	struct notifier_block chg1_nb;
	struct charger_data chg1_data;
	struct charger_consumer *chg1_consumer;

	enum charger_type chg_type;
	bool can_charging;

	int (*do_algorithm)(struct charger_manager *);
	int (*plug_in)(struct charger_manager *);
	int (*plug_out)(struct charger_manager *);
	int (*do_charging)(struct charger_manager *, bool en);
	int (*do_event)(struct notifier_block *nb, unsigned long ev, void *v);
	int (*change_current_setting)(struct charger_manager *);

	/* notify charger user */
	struct srcu_notifier_head evt_nh;

	/* common info */
	int battery_temp;

	/* sw jeita */
	bool enable_sw_jeita;
	struct sw_jeita_data sw_jeita;

	bool cmd_discharging;
	bool safety_timeout;
	bool vbusov_stat;

	/* battery warning */
	u32 notify_code;
	u32 notify_test_mode;

	bool enable_sw_safety_timer;
	bool sw_safety_timer_setting;

	/* thread related */
	struct hrtimer charger_kthread_timer;
	struct wakeup_source chg_wlock;
	struct mutex chg_lock;
	spinlock_t slock;
	u32 polling_interval;
	bool charger_thread_timeout;
	wait_queue_head_t wait_que;
	bool charger_thread_polling;

	atomic_t enable_kpoc_shdn;

	/* ATM */
	bool atm_enabled;
};

/* charger related module interface */
extern void mtk_wake_up_charger(struct charger_manager *chg_mgr);
extern int mtk_switch_charging_init(struct charger_manager *chg_mgr);
extern int mtk_charger_manager_notify(struct charger_manager *chg_mgr,
				      int event);

/* FIXME */
enum usb_state_enum {
	USB_SUSPEND = 0,
	USB_UNCONFIGURED,
	USB_CONFIGURED
};

#endif /* __MTK_CHARGER_H */
