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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/power_supply.h>
#include <linux/pm_wakeup.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/scatterlist.h>
#include <linux/suspend.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/reboot.h>
#include <linux/power/mtk_charger_consumer.h>

#include <mt-plat/charger_type.h>
#include <mt-plat/mtk_boot.h>

#include "mtk_charger.h"

static struct list_head consumer_head = LIST_HEAD_INIT(consumer_head);
static DEFINE_MUTEX(consumer_lock);

/*
 * For those old APIs do not have charger manager
 * Should be modified in the future...
 */
static struct charger_manager *g_chg_mgr;

//static int chg_log_level = CHGLOG_LEVEL_ERROR;
static int chg_log_level = CHGLOG_LEVEL_INFO;
int chg_get_log_level(void)
{
	return chg_log_level;
}

static struct charger_custom_data charger_custom_data_default = {
	.battery_cv = 4350000,
	.max_charger_voltage = 6500000,
	.max_charger_voltage_setting = 6500000,
	.min_charger_voltage = 4600000,
	.usb_charger_current_suspend = 0,
	.usb_charger_current_unconfigured = 70000,
	.usb_charger_current_configured = 500000,
	.usb_charger_current = 500000,
	.ac_charger_current = 2050000,
	.ac_charger_input_current = 3200000,
	.non_std_ac_charger_current = 500000,
	.charging_host_charger_current = 650000,
	.apple_1_0a_charger_current = 650000,
	.apple_2_1a_charger_current = 800000,
	.ta_ac_charger_current = 3000000,

	/* sw jeita */
	.jeita_temp_above_t4_cv = 4240000,
	.jeita_temp_t3_to_t4_cv = 4240000,
	.jeita_temp_t2_to_t3_cv = 4340000,
	.jeita_temp_t1_to_t2_cv = 4240000,
	.jeita_temp_t0_to_t1_cv = 4040000,
	.jeita_temp_below_t0_cv = 4040000,
	.temp_t4_thres = 50,
	.temp_t4_thres_minus_x_degree = 47,
	.temp_t3_thres = 45,
	.temp_t3_thres_minus_x_degree = 39,
	.temp_t2_thres = 10,
	.temp_t2_thres_plus_x_degree = 16,
	.temp_t1_thres = 0,
	.temp_t1_thres_plus_x_degree = 6,
	.temp_t0_thres = 0,
	.temp_t0_thres_plus_x_degree = 0,
	.temp_neg_10_thres = 0,

	.max_charging_time = 12 * 60 * 60, /* second */
};

static struct battery_thermal_protection_data batt_thermal_data_default = {
	.sm = BAT_TEMP_NORMAL,
	.enable_min_charge_temp = true,
	.min_charge_temp = 0,
	.min_charge_temp_plus_x_degree = 6,
	.max_charge_temp = 50,
	.max_charge_temp_minus_x_degree = 47,
};

static bool is_charger_disabled(struct charger_manager *chg_mgr)
{
	if (!chg_mgr)
		return true;
	if (chg_mgr->disable_charger || IS_ENABLED(CONFIG_POWER_EXT))
		return true;
	return false;
}

/*
 * For USB, extern in usb20/mt8518/usb20.h
 * Should be modified in the future...
 */
void BATTERY_SetUSBState(int usb_state)
{
	if (is_charger_disabled(g_chg_mgr)) {
		chg_err("[BATTERY_SetUSBState] in FPGA/EVB, no service\n");
		return;
	}
	if ((usb_state < USB_SUSPEND) || ((usb_state > USB_CONFIGURED))) {
		chg_err("%s Fail! Restore to default value\n", __func__);
		usb_state = USB_UNCONFIGURED;
	} else {
		chg_err("%s Success! Set %d\n", __func__, usb_state);
		if (g_chg_mgr)
			g_chg_mgr->usb_state = usb_state;
	}
}

static int __mtk_charger_do_charging(struct charger_manager *chg_mgr, bool en)
{
	if (!chg_mgr && chg_mgr->do_charging)
		chg_mgr->do_charging(chg_mgr, en);
	return 0;
}

static int
__mtk_charger_manager_enable_charging(struct charger_consumer *consumer,
				      enum mtk_charger_role role, bool en)
{
	struct charger_manager *chg_mgr = consumer->cm;
	struct charger_data *chg;

	chg_err("%s dev:%s role:%d en:%d\n", __func__, dev_name(consumer->dev),
		role, en);

	if (!chg_mgr)
		return -EBUSY;

	if (role == CHG_ROLE_MAIN)
		chg = &chg_mgr->chg1_data;
	else
		return -ENOTSUPP;

	if (!en) {
		__mtk_charger_do_charging(chg_mgr, en);
		chg->disable_charging_count++;
	} else {
		if (chg->disable_charging_count == 1) {
			__mtk_charger_do_charging(chg_mgr, en);
			chg->disable_charging_count = 0;
		} else if (chg->disable_charging_count > 1)
			chg->disable_charging_count--;
	}
	chg_err("%s dev:%s idx:%d en:%d cnt:%d\n", __func__,
		dev_name(consumer->dev), role, en, chg->disable_charging_count);

	return 0;
}

/* user interface end*/

/* factory mode */
#define CHARGER_DEVNAME "charger_ftm"
#define GET_IS_SLAVE_CHARGER_EXIST _IOW('k', 13, int)

static struct class *charger_class;
static struct cdev *charger_cdev;
static int charger_major;
static dev_t charger_devno;

static int is_slave_charger_exist(void)
{
	if (get_charger_by_name("secondary_chg") == NULL)
		return 0;
	return 1;
}

static long charger_ftm_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	int ret = 0;
	int out_data = 0;
	void __user *user_data = (void __user *)arg;

	switch (cmd) {
	case GET_IS_SLAVE_CHARGER_EXIST:
		out_data = is_slave_charger_exist();
		ret = copy_to_user(user_data, &out_data, sizeof(out_data));
		chg_err("[%s] SLAVE_CHARGER_EXIST: %d\n", __func__, out_data);
		break;
	default:
		chg_err("[%s] Error ID\n", __func__);
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long charger_ftm_compat_ioctl(struct file *file, unsigned int cmd,
				     unsigned long arg)
{
	int ret = 0;

	switch (cmd) {
	case GET_IS_SLAVE_CHARGER_EXIST:
		ret = file->f_op->unlocked_ioctl(file, cmd, arg);
		break;
	default:
		chg_err("[%s] Error ID\n", __func__);
		break;
	}

	return ret;
}
#endif

static int charger_ftm_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int charger_ftm_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations charger_ftm_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = charger_ftm_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = charger_ftm_compat_ioctl,
#endif
	.open = charger_ftm_open,
	.release = charger_ftm_release,
};

static void charger_ftm_init(void)
{
	struct class_device *class_dev = NULL;
	int ret = 0;

	ret = alloc_chrdev_region(&charger_devno, 0, 1, CHARGER_DEVNAME);
	if (ret < 0) {
		chg_err("[%s]Can't get major num for charger_ftm\n", __func__);
		return;
	}

	charger_cdev = cdev_alloc();
	if (!charger_cdev) {
		chg_err("[%s]cdev_alloc fail\n", __func__);
		goto unregister;
	}
	charger_cdev->owner = THIS_MODULE;
	charger_cdev->ops = &charger_ftm_fops;

	ret = cdev_add(charger_cdev, charger_devno, 1);
	if (ret < 0) {
		chg_err("[%s] cdev_add failed\n", __func__);
		goto free_cdev;
	}

	charger_major = MAJOR(charger_devno);
	charger_class = class_create(THIS_MODULE, CHARGER_DEVNAME);
	if (IS_ERR(charger_class)) {
		chg_err("[%s] class_create failed\n", __func__);
		goto free_cdev;
	}

	class_dev = (struct class_device *)device_create(charger_class,
				NULL, charger_devno, NULL, CHARGER_DEVNAME);
	if (IS_ERR(class_dev)) {
		chg_err("[%s] device_create failed\n", __func__);
		goto free_class;
	}

	pr_debug("%s done\n", __func__);
	return;

free_class:
	class_destroy(charger_class);
free_cdev:
	cdev_del(charger_cdev);
unregister:
	unregister_chrdev_region(charger_devno, 1);
}
/* factory mode end */

static void mtk_charger_get_atm_mode(struct charger_manager *chg_mgr)
{
	char atm_str[64];
	char *ptr, *ptr_e;

	memset(atm_str, 0x0, sizeof(atm_str));
	ptr = strstr(saved_command_line, "androidboot.atm=");
	if (ptr != 0) {
		ptr_e = strstr(ptr, " ");

		if (ptr_e != 0) {
			strncpy(atm_str, ptr + 16, ptr_e - ptr - 16);
			atm_str[ptr_e - ptr - 16] = '\0';
		}

		if (!strncmp(atm_str, "enable", strlen("enable")))
			chg_mgr->atm_enabled = true;
		else
			chg_mgr->atm_enabled = false;
	} else
		chg_mgr->atm_enabled = false;

	chg_info("%s: atm_enabled = %d\n", __func__, chg_mgr->atm_enabled);
}

/* sw jeita */
void do_sw_jeita_state_machine(struct charger_manager *chg_mgr)
{
	struct sw_jeita_data *sw_jeita = &chg_mgr->sw_jeita;
	struct charger_custom_data *cust_data = chg_mgr->cust_data;

	sw_jeita->pre_sm = sw_jeita->sm;
	sw_jeita->charging = true;

	/* JEITA battery temp Standard */
	if (chg_mgr->battery_temp >= cust_data->temp_t4_thres) {
		chg_err("[SW_JEITA] Battery over high temperature(%d)\n",
			cust_data->temp_t4_thres);
		sw_jeita->sm = TEMP_ABOVE_T4;
		sw_jeita->charging = false;
	} else if (chg_mgr->battery_temp > cust_data->temp_t3_thres) {
		/* control 45 degree to normal behavior */
		if ((sw_jeita->sm == TEMP_ABOVE_T4) &&
		    (chg_mgr->battery_temp >=
		     cust_data->temp_t4_thres_minus_x_degree)) {
			chg_err("[SW_JEITA] Battery temperature between %d and %d, not allow charging yet\n",
				cust_data->temp_t4_thres_minus_x_degree,
				cust_data->temp_t4_thres);
			sw_jeita->charging = false;
		} else {
			chg_err("[SW_JEITA] Battery temperature between %d and %d\n",
				cust_data->temp_t3_thres,
				cust_data->temp_t4_thres);
			sw_jeita->sm = TEMP_T3_TO_T4;
		}
	} else if (chg_mgr->battery_temp >= cust_data->temp_t2_thres) {
		if (((sw_jeita->sm == TEMP_T3_TO_T4) &&
		    (chg_mgr->battery_temp >=
		     cust_data->temp_t3_thres_minus_x_degree)) ||
		    ((sw_jeita->sm == TEMP_T1_TO_T2) &&
		     (chg_mgr->battery_temp <=
		      cust_data->temp_t2_thres_plus_x_degree)))
			chg_err("[SW_JEITA] Battery temperature not recovery to normal temperature charging mode yet\n");
		else {
			chg_err("[SW_JEITA] Battery normal temperature between %d and %d\n",
				cust_data->temp_t2_thres,
				cust_data->temp_t3_thres);
			sw_jeita->sm = TEMP_T2_TO_T3;
		}
	} else if (chg_mgr->battery_temp >= cust_data->temp_t1_thres) {
		if ((sw_jeita->sm == TEMP_T0_TO_T1 ||
		    sw_jeita->sm == TEMP_BELOW_T0) &&
		    (chg_mgr->battery_temp <=
		     cust_data->temp_t1_thres_plus_x_degree)) {
			if (sw_jeita->sm == TEMP_T0_TO_T1)
				chg_err("[SW_JEITA] Battery temperature between %d and %d\n",
					cust_data->temp_t1_thres_plus_x_degree,
					cust_data->temp_t2_thres);
			if (sw_jeita->sm == TEMP_BELOW_T0) {
				chg_err("[SW_JEITA] Battery temperature between %d and %d, not allow charging yet\n",
					cust_data->temp_t1_thres,
					cust_data->temp_t1_thres_plus_x_degree);
				sw_jeita->charging = false;
			}
		} else {
			chg_err("[SW_JEITA] Battery temperature between %d and %d\n",
				cust_data->temp_t1_thres,
				cust_data->temp_t2_thres);
			sw_jeita->sm = TEMP_T1_TO_T2;
		}
	} else if (chg_mgr->battery_temp >= cust_data->temp_t0_thres) {
		if ((sw_jeita->sm == TEMP_BELOW_T0) &&
		    (chg_mgr->battery_temp <=
		     cust_data->temp_t0_thres_plus_x_degree)) {
			chg_err("[SW_JEITA] Battery temperature between %d and %d, not allow charging yet\n",
				cust_data->temp_t0_thres,
				cust_data->temp_t0_thres_plus_x_degree);
			sw_jeita->charging = false;
		} else {
			chg_err("[SW_JEITA] Battery temperature between %d and %d\n",
				cust_data->temp_t0_thres,
				cust_data->temp_t1_thres);
			sw_jeita->sm = TEMP_T0_TO_T1;
		}
	} else {
		chg_err("[SW_JEITA] Battery below low temperature(%d)\n",
			cust_data->temp_t0_thres);
		sw_jeita->sm = TEMP_BELOW_T0;
		sw_jeita->charging = false;
	}

	/* set CV after temperature changed */
	/* In normal range, we adjust CV dynamically */
	if (sw_jeita->sm != TEMP_T2_TO_T3) {
		if (sw_jeita->sm == TEMP_ABOVE_T4)
			sw_jeita->cv = cust_data->jeita_temp_above_t4_cv;
		else if (sw_jeita->sm == TEMP_T3_TO_T4)
			sw_jeita->cv = cust_data->jeita_temp_t3_to_t4_cv;
		else if (sw_jeita->sm == TEMP_T2_TO_T3)
			sw_jeita->cv = 0;
		else if (sw_jeita->sm == TEMP_T1_TO_T2)
			sw_jeita->cv = cust_data->jeita_temp_t1_to_t2_cv;
		else if (sw_jeita->sm == TEMP_T0_TO_T1)
			sw_jeita->cv = cust_data->jeita_temp_t0_to_t1_cv;
		else if (sw_jeita->sm == TEMP_BELOW_T0)
			sw_jeita->cv = cust_data->jeita_temp_below_t0_cv;
		else
			sw_jeita->cv = cust_data->battery_cv;
	} else
		sw_jeita->cv = 0;

	chg_err("[SW_JEITA] preState:%d newState:%d tmp:%d cv:%d\n",
		sw_jeita->pre_sm, sw_jeita->sm, chg_mgr->battery_temp,
		sw_jeita->cv);
}
/* sw jeita end */

static int mtk_charger_plug_in(struct charger_manager *chg_mgr,
			       enum charger_type chg_type)
{
	chg_mgr->chg_type = chg_type;
	chg_mgr->charger_thread_polling = true;
	chg_mgr->can_charging = true;
	chg_mgr->safety_timeout = false;
	chg_mgr->vbusov_stat = false;

	chg_info("%s, type = %d\n", __func__, chg_type);
	if (chg_mgr->plug_in)
		chg_mgr->plug_in(chg_mgr);

	charger_dev_plug_in(chg_mgr->chg1_dev);
	return 0;
}

static int mtk_charger_plug_out(struct charger_manager *chg_mgr)
{
	struct charger_data *chg = &chg_mgr->chg1_data;

	chg_err("%s\n", __func__);
	chg_mgr->chg_type = CHARGER_UNKNOWN;
	chg_mgr->charger_thread_polling = false;

	chg->disable_charging_count = 0;
	chg->input_current_limit_by_aicl = -1;

	if (chg_mgr->plug_out)
		chg_mgr->plug_out(chg_mgr);

	charger_dev_set_input_current(chg_mgr->chg1_dev, 500000);
	charger_dev_plug_out(chg_mgr->chg1_dev);
	return 0;
}

static bool mtk_is_charger_on(struct charger_manager *chg_mgr)
{
	int ret;
	enum charger_type chg_type = CHARGER_UNKNOWN;
	struct power_supply *chg_psy;
	union power_supply_propval val;

	/* Get charger type from power supply mtk_charger */
	chg_psy = power_supply_get_by_name("charger");
	if (chg_psy) {
		ret = power_supply_get_property(chg_psy,
						POWER_SUPPLY_PROP_CHARGE_TYPE,
						&val);
		if (ret >= 0) {
			chg_type = val.intval;
			chg_info("%s chg type %d\n", __func__, chg_type);
		}
	}
	if ((chg_type == CHARGER_UNKNOWN) &&
	    (chg_mgr->chg_type != CHARGER_UNKNOWN))
		mtk_charger_plug_out(chg_mgr);
	else if ((chg_type != CHARGER_UNKNOWN) &&
		 (chg_mgr->chg_type == CHARGER_UNKNOWN))
		mtk_charger_plug_in(chg_mgr, chg_type);
	else
		chg_mgr->chg_type = chg_type;

	return (chg_type == CHARGER_UNKNOWN) ? false : true;
}

static int mtk_chgstat_notify(struct charger_manager *chg_mgr)
{
	int ret;
	char *env[2] = {"CHGSTAT=1", NULL};

	chg_err("%s 0x%x\n", __func__, chg_mgr->notify_code);
	ret = kobject_uevent_env(&chg_mgr->dev->kobj, KOBJ_CHANGE, env);
	if (ret)
		chg_err("%s kobject_uevent_fail(%d)\n", __func__, ret);
	return ret;
}

static void mtk_battery_notify_vchg_check(struct charger_manager *chg_mgr)
{
#ifdef BATTERY_NOTIFY_CASE_0001_VCHARGER
	int vchr = 0;
	struct charger_custom_data *cust_data = chg_mgr->cust_data;

	/* TODO: Get real vbus */
	vchr = 5000000;
	if (vchr < cust_data->max_charger_voltage)
		chg_mgr->notify_code &= ~CHG_VBUS_OV_STATUS;
	else {
		chg_mgr->notify_code |= CHG_VBUS_OV_STATUS;
		chg_err("[BATTERY] charger_vol(%dmV) > %d mV\n",
			vchr / 1000, cust_data->max_charger_voltage / 1000);
		mtk_chgstat_notify(chg_mgr);
	}
#endif /* BATTERY_NOTIFY_CASE_0001_VCHARGER */
}

static void mtk_battery_notify_vbat_tbat_check(struct charger_manager *chg_mgr)
{
#ifdef BATTERY_NOTIFY_CASE_0002_VBATTEMP
	struct battery_thermal_protection_data *thermal = chg_mgr->thermal;
	struct charger_custom_data *cust_data = chg_mgr->cust_data;

	if (chg_mgr->battery_temp >= thermal->max_charge_temp) {
		chg_mgr->notify_code |= CHG_BAT_OT_STATUS;
		chg_err("[BATTERY] bat_temp(%d) out of range(too high)\n",
			chg_mgr->battery_temp);
		mtk_chgstat_notify(chg_mgr);
	} else
		chg_mgr->notify_code &= ~CHG_BAT_OT_STATUS;

	if (chg_mgr->enable_sw_jeita) {
		if (chg_mgr->battery_temp < cust_data->temp_neg_10_thres) {
			chg_mgr->notify_code |= CHG_BAT_LT_STATUS;
			chg_err("bat_temp(%d) out of range(too low)\n",
				chg_mgr->battery_temp);
			mtk_chgstat_notify(chg_mgr);
		} else
			chg_mgr->notify_code &= ~CHG_BAT_LT_STATUS;
	} else {
#ifdef BAT_LOW_TEMP_PROTECT_ENABLE
		if (chg_mgr->battery_temp < thermal->min_charge_temp) {
			chg_mgr->notify_code |= CHG_BAT_LT_STATUS;
			chg_err("bat_temp(%d) out of range(too low)\n",
				chg_mgr->battery_temp);
			mtk_chgstat_notify(chg_mgr);
		} else
			chg_mgr->notify_code &= ~CHG_BAT_LT_STATUS;
#endif /* BAT_LOW_TEMP_PROTECT_ENABLE */
	}
#endif /* BATTERY_NOTIFY_CASE_0002_VBATTEMP */
}

static void mtk_battery_notify_ui_test(struct charger_manager *chg_mgr)
{
	switch (chg_mgr->notify_test_mode) {
	case 1:
		chg_mgr->notify_code = CHG_VBUS_OV_STATUS;
		chg_debug("[%s] CASE_0001_VCHARGER\n", __func__);
		break;
	case 2:
		chg_mgr->notify_code = CHG_BAT_OT_STATUS;
		chg_debug("[%s] CASE_0002_VBATTEMP\n", __func__);
		break;
	case 3:
		chg_mgr->notify_code = CHG_OC_STATUS;
		chg_debug("[%s] CASE_0003_ICHARGING\n", __func__);
		break;
	case 4:
		chg_mgr->notify_code = CHG_BAT_OV_STATUS;
		chg_debug("[%s] CASE_0004_VBAT\n", __func__);
		break;
	case 5:
		chg_mgr->notify_code = CHG_ST_TMO_STATUS;
		chg_debug("[%s] CASE_0005_TOTAL_CHARGINGTIME\n", __func__);
		break;
	case 6:
		chg_mgr->notify_code = CHG_BAT_LT_STATUS;
		chg_debug("[%s] CASE6: VBATTEMP_LOW\n", __func__);
		break;
	default:
		chg_debug("[%s] Unknown BN_TestMode Code: %x\n", __func__,
			  chg_mgr->notify_test_mode);
	}
	mtk_chgstat_notify(chg_mgr);
}

static void mtk_battery_notify_check(struct charger_manager *chg_mgr)
{
	if (chg_mgr->notify_test_mode == 0x0000) {
		mtk_battery_notify_vchg_check(chg_mgr);
		mtk_battery_notify_vbat_tbat_check(chg_mgr);
	} else
		mtk_battery_notify_ui_test(chg_mgr);
}

static void charger_update_data(struct charger_manager *chg_mgr)
{
	int ret;
	struct power_supply *batt_psy;
	union power_supply_propval batt_val;

	batt_psy = power_supply_get_by_name("battery");
	if (!batt_psy)
		return;
	ret = power_supply_get_property(batt_psy, POWER_SUPPLY_PROP_TEMP,
					&batt_val);
	if (ret >= 0) {
		chg_info("%s battery temp %d\n", __func__, batt_val.intval);
		chg_mgr->battery_temp = batt_val.intval;
	}
	power_supply_put(batt_psy);
}

static void check_battery_exist(struct charger_manager *chg_mgr)
{
	/* TODO: check battery exist from charger */
}

static void mtk_chg_get_tchg(struct charger_manager *chg_mgr)
{
	int ret;
	int tchg_min, tchg_max;
	struct charger_data *chg = &chg_mgr->chg1_data;

	ret = charger_dev_get_temperature(chg_mgr->chg1_dev, &tchg_min,
					  &tchg_max);

	if (ret < 0) {
		chg->junction_temp_min = -127;
		chg->junction_temp_max = -127;
	} else {
		chg->junction_temp_min = tchg_min;
		chg->junction_temp_max = tchg_max;
	}
}

/* return false if vbus is over max_charger_voltage */
static bool mtk_chg_check_vbus(struct charger_manager *chg_mgr)
{
	int vchr = 0;
	struct charger_custom_data *cust_data = chg_mgr->cust_data;

	/* TODO: Get real vbus */
	vchr = 5000000;
	if (vchr > cust_data->max_charger_voltage) {
		chg_err("%s vbus(%dmV) > %dmV\n", __func__, vchr / 1000,
			cust_data->max_charger_voltage / 1000);
		return false;
	}
	return true;
}

static void charger_check_status(struct charger_manager *chg_mgr)
{
	bool charging = true;
	int tbat = chg_mgr->battery_temp;
	struct battery_thermal_protection_data *thermal = chg_mgr->thermal;

	if (chg_mgr->enable_sw_jeita) {
		do_sw_jeita_state_machine(chg_mgr);
		if (!chg_mgr->sw_jeita.charging) {
			charging = false;
			goto stop_charging;
		}
	} else {
		if (thermal->enable_min_charge_temp) {
			if (tbat < thermal->min_charge_temp) {
				chg_err("Battery under temperature or NTC fail %d %d\n",
					tbat, thermal->min_charge_temp);
				thermal->sm = BAT_TEMP_LOW;
				charging = false;
				goto stop_charging;
			} else if (thermal->sm == BAT_TEMP_LOW) {
				if (tbat >=
				    thermal->min_charge_temp_plus_x_degree) {
					chg_err("Battery temperature raise from %d to %d(%d), allow charging\n",
					thermal->min_charge_temp, tbat,
					thermal->min_charge_temp_plus_x_degree);
					thermal->sm = BAT_TEMP_NORMAL;
				} else {
					charging = false;
					goto stop_charging;
				}
			}
		}
		if (tbat >= thermal->max_charge_temp) {
			chg_err("Battery over temperature or NTC fail %d %d\n",
				tbat, thermal->max_charge_temp);
			thermal->sm = BAT_TEMP_HIGH;
			charging = false;
			goto stop_charging;
		} else if (thermal->sm == BAT_TEMP_HIGH) {
			if (tbat < thermal->max_charge_temp_minus_x_degree) {
				chg_err("Battery temperature raise from %d to %d(%d), allow charging\n",
				thermal->max_charge_temp, tbat,
				thermal->max_charge_temp_minus_x_degree);
				thermal->sm = BAT_TEMP_NORMAL;
			} else {
				charging = false;
				goto stop_charging;
			}
		}
	}

	mtk_chg_get_tchg(chg_mgr);

	if (!mtk_chg_check_vbus(chg_mgr)) {
		charging = false;
		goto stop_charging;
	}

	if (chg_mgr->cmd_discharging)
		charging = false;
	if (chg_mgr->safety_timeout)
		charging = false;
	if (chg_mgr->vbusov_stat)
		charging = false;

stop_charging:
	mtk_battery_notify_check(chg_mgr);

	chg_err("tmp:%d (jeita:%d sm:%d cv:%d en:%d) (sm:%d) en:%d %d c:%d s:%d ov:%d\n",
		tbat, chg_mgr->enable_sw_jeita, chg_mgr->sw_jeita.sm,
		chg_mgr->sw_jeita.cv, chg_mgr->sw_jeita.charging, thermal->sm,
		chg_mgr->can_charging, charging, chg_mgr->cmd_discharging,
		chg_mgr->safety_timeout, chg_mgr->vbusov_stat);

	if (chg_mgr->can_charging != charging)
		__mtk_charger_manager_enable_charging(chg_mgr->chg1_consumer, 0,
						      charging);

	chg_mgr->can_charging = charging;
}

static void kpoc_power_off_check(struct charger_manager *info)
{
	/* TODO: Check vbus valid from charger and power off if necessary */
}

static enum hrtimer_restart charger_kthread_hrtimer_func(struct hrtimer *timer)
{
	struct charger_manager *chg_mgr =
		container_of(timer, struct charger_manager,
			     charger_kthread_timer);

	mtk_wake_up_charger(chg_mgr);
	return HRTIMER_NORESTART;
}

static void mtk_charger_init_timer(struct charger_manager *chg_mgr)
{
	ktime_t ktime = ktime_set(chg_mgr->polling_interval, 0);

	hrtimer_init(&chg_mgr->charger_kthread_timer, CLOCK_MONOTONIC,
		     HRTIMER_MODE_REL);
	chg_mgr->charger_kthread_timer.function =
		charger_kthread_hrtimer_func;
	hrtimer_start(&chg_mgr->charger_kthread_timer, ktime, HRTIMER_MODE_REL);
}

static void mtk_charger_start_timer(struct charger_manager *chg_mgr)
{
	ktime_t ktime = ktime_set(chg_mgr->polling_interval, 0);

	hrtimer_start(&chg_mgr->charger_kthread_timer, ktime, HRTIMER_MODE_REL);
}

static int charger_routine_thread(void *data)
{
	struct charger_manager *chg_mgr = data;
	unsigned long flags;
	bool is_charger_on;

	while (1) {
		wait_event(chg_mgr->wait_que,
			   (chg_mgr->charger_thread_timeout == true));
		mutex_lock(&chg_mgr->chg_lock);
		spin_lock_irqsave(&chg_mgr->slock, flags);
		if (!chg_mgr->chg_wlock.active)
			__pm_stay_awake(&chg_mgr->chg_wlock);
		spin_unlock_irqrestore(&chg_mgr->slock, flags);
		chg_mgr->charger_thread_timeout = false;

		chg_info("charger_routine_thread %d\n",
			 chg_mgr->charger_thread_polling);
		is_charger_on = mtk_is_charger_on(chg_mgr);
		if (chg_mgr->charger_thread_polling == true)
			mtk_charger_start_timer(chg_mgr);

		charger_update_data(chg_mgr);
		check_battery_exist(chg_mgr);
		charger_check_status(chg_mgr);
		kpoc_power_off_check(chg_mgr);

		if (is_charger_disabled(chg_mgr))
			chg_debug("%s charging disabled\n", __func__);
		else if (is_charger_on && chg_mgr->do_algorithm)
			chg_mgr->do_algorithm(chg_mgr);

		spin_lock_irqsave(&chg_mgr->slock, flags);
		__pm_relax(&chg_mgr->chg_wlock);
		spin_unlock_irqrestore(&chg_mgr->slock, flags);
		chg_info("charger_routine_thread end, %d\n",
			 chg_mgr->charger_thread_timeout);
		mutex_unlock(&chg_mgr->chg_lock);
	}

	return 0;
}

/* Released Charger Manager API */
int mtk_charger_manager_notify(struct charger_manager *chg_mgr, int event)
{
	return srcu_notifier_call_chain(&chg_mgr->evt_nh, event, NULL);
}

void mtk_wake_up_charger(struct charger_manager *chg_mgr)
{
	unsigned long flags;

	if (chg_mgr == NULL)
		return;

	chg_err("%s init %d\n", __func__, chg_mgr->init_done);
	spin_lock_irqsave(&chg_mgr->slock, flags);
	if (!chg_mgr->chg_wlock.active)
		__pm_stay_awake(&chg_mgr->chg_wlock);
	spin_unlock_irqrestore(&chg_mgr->slock, flags);
	chg_mgr->charger_thread_timeout = true;
	wake_up(&chg_mgr->wait_que);
}

/* extern in mt-plat/charger_type.h */
void mtk_charger_int_handler(void)
{
	chg_info("%s\n", __func__);

	if (!g_chg_mgr) {
		chg_err("charger is not rdy, skip1\n");
		return;
	}

	if (!g_chg_mgr->init_done) {
		chg_err("charger is not rdy, skip2\n");
		return;
	}

	chg_info("%s wake up charger\n", __func__);
	mtk_wake_up_charger(g_chg_mgr);
}

/* Released Charger Consumer API */
int mtk_charger_manager_get_tbat(struct charger_consumer *consumer, int *tbat)
{
	struct charger_manager *chg_mgr;

	if (!consumer)
		return -EINVAL;
	if (!consumer->cm)
		return -EINVAL;
	chg_mgr = consumer->cm;
	*tbat = chg_mgr->battery_temp;
	return 0;
}

struct charger_consumer *mtk_charger_manager_get_consumer(struct device *dev)
{
	struct charger_consumer *consumer;

	if (!dev)
		return ERR_PTR(EINVAL);

	consumer = kzalloc(sizeof(*consumer), GFP_KERNEL);
	if (!consumer)
		return ERR_PTR(ENOMEM);

	mutex_lock(&consumer_lock);
	consumer->dev = dev;
	list_add(&consumer->list, &consumer_head);
	if (g_chg_mgr)
		consumer->cm = g_chg_mgr;
	mutex_unlock(&consumer_lock);
	return consumer;
}

int mtk_charger_manager_register_notifier(struct charger_consumer *consumer,
					  struct notifier_block *nb)
{
	int ret = 0;
	struct charger_manager *chg_mgr;

	if (!consumer)
		return -EINVAL;
	chg_mgr = consumer->cm;
	mutex_lock(&consumer_lock);
	if (chg_mgr)
		ret = srcu_notifier_chain_register(&chg_mgr->evt_nh, nb);
	else
		consumer->nb = nb;
	mutex_unlock(&consumer_lock);
	return ret;
}

int mtk_charger_manager_unregister_notifier(struct charger_consumer *consumer,
					    struct notifier_block *nb)
{
	int ret = 0;
	struct charger_manager *chg_mgr;

	if (!consumer)
		return -EINVAL;
	chg_mgr = consumer->cm;
	mutex_lock(&consumer_lock);
	if (chg_mgr)
		ret = srcu_notifier_chain_unregister(&chg_mgr->evt_nh, nb);
	else
		consumer->nb = NULL;
	mutex_unlock(&consumer_lock);
	return ret;
}

static int mtk_charger_parse_dt(struct charger_manager *chg_mgr)
{
	int ret;
	u32 val;
	struct device_node *np = chg_mgr->dev->of_node;
	struct charger_custom_data *cust_data;
	struct battery_thermal_protection_data *batt_thermal_data;

	dev_info(chg_mgr->dev, "%s\n", __func__);

	if (!np) {
		dev_err(chg_mgr->dev, "%s no device node\n", __func__);
		return -ENODEV;
	}

	cust_data = devm_kzalloc(chg_mgr->dev, sizeof(*cust_data), GFP_KERNEL);
	if (!cust_data)
		return -ENOMEM;
	batt_thermal_data = devm_kzalloc(chg_mgr->dev,
					 sizeof(*batt_thermal_data),
					 GFP_KERNEL);
	if (!batt_thermal_data)
		return -ENOMEM;
	memcpy(cust_data, &charger_custom_data_default, sizeof(*cust_data));
	memcpy(batt_thermal_data, &batt_thermal_data_default,
	       sizeof(*batt_thermal_data));
	chg_mgr->cust_data = cust_data;
	chg_mgr->thermal = batt_thermal_data;

	if (of_property_read_string(np, "algorithm_name",
				    &chg_mgr->algorithm_name) < 0) {
		dev_info(chg_mgr->dev, "%s no algorithm_name\n", __func__);
		chg_mgr->algorithm_name = "switch_charging";
	}
	if (strcmp(chg_mgr->algorithm_name, "switch_charging") == 0) {
		ret = mtk_switch_charging_init(chg_mgr);
		if (ret < 0)
			return ret;
	}
	chg_mgr->disable_charger = of_property_read_bool(np, "disable_charger");
	chg_mgr->enable_sw_safety_timer =
		of_property_read_bool(np, "enable_sw_safety_timer");
	chg_mgr->sw_safety_timer_setting = chg_mgr->enable_sw_safety_timer;
	chg_mgr->enable_sw_jeita = of_property_read_bool(np, "enable_sw_jeita");

	/* common */
	if (of_property_read_u32(np, "battery_cv", &val) >= 0)
		cust_data->battery_cv = val;
	if (of_property_read_u32(np, "max_charger_voltage", &val) >= 0)
		cust_data->max_charger_voltage = val;
	cust_data->max_charger_voltage_setting = cust_data->max_charger_voltage;
	if (of_property_read_u32(np, "min_charger_voltage", &val) >= 0)
		cust_data->min_charger_voltage = val;

	/* charging current */
	if (of_property_read_u32(np, "usb_charger_current_suspend", &val) >= 0)
		cust_data->usb_charger_current_suspend = val;
	if (of_property_read_u32(np, "usb_charger_current_unconfigured",
				 &val) >= 0)
		cust_data->usb_charger_current_unconfigured = val;
	if (of_property_read_u32(np, "usb_charger_current_configured",
				 &val) >= 0)
		cust_data->usb_charger_current_configured = val;
	if (of_property_read_u32(np, "usb_charger_current", &val) >= 0)
		cust_data->usb_charger_current = val;
	if (of_property_read_u32(np, "ac_charger_current", &val) >= 0)
		cust_data->ac_charger_current = val;
	if (of_property_read_u32(np, "ac_charger_input_current", &val) >= 0)
		cust_data->ac_charger_input_current = val;
	if (of_property_read_u32(np, "non_std_ac_charger_current", &val) >= 0)
		cust_data->non_std_ac_charger_current = val;
	if (of_property_read_u32(np, "charging_host_charger_current",
				 &val) >= 0)
		cust_data->charging_host_charger_current = val;
	if (of_property_read_u32(np, "apple_1_0a_charger_current", &val) >= 0)
		cust_data->apple_1_0a_charger_current = val;
	if (of_property_read_u32(np, "apple_2_1a_charger_current", &val) >= 0)
		cust_data->apple_2_1a_charger_current = val;
	if (of_property_read_u32(np, "ta_ac_charger_current", &val) >= 0)
		cust_data->ta_ac_charger_current = val;

	/* sw jeita */
	if (of_property_read_u32(np, "jeita_temp_above_t4_cv", &val) >= 0)
		cust_data->jeita_temp_above_t4_cv = val;
	if (of_property_read_u32(np, "jeita_temp_t3_to_t4_cv", &val) >= 0)
		cust_data->jeita_temp_t3_to_t4_cv = val;
	if (of_property_read_u32(np, "jeita_temp_t2_to_t3_cv", &val) >= 0)
		cust_data->jeita_temp_t2_to_t3_cv = val;
	if (of_property_read_u32(np, "jeita_temp_t1_to_t2_cv", &val) >= 0)
		cust_data->jeita_temp_t1_to_t2_cv = val;
	if (of_property_read_u32(np, "jeita_temp_t0_to_t1_cv", &val) >= 0)
		cust_data->jeita_temp_t0_to_t1_cv = val;
	if (of_property_read_u32(np, "jeita_temp_below_t0_cv", &val) >= 0)
		cust_data->jeita_temp_below_t0_cv = val;
	if (of_property_read_u32(np, "temp_t4_thres", &val) >= 0)
		cust_data->temp_t4_thres = val;
	if (of_property_read_u32(np, "temp_t4_thres_minus_x_degree", &val) >= 0)
		cust_data->temp_t4_thres_minus_x_degree = val;
	if (of_property_read_u32(np, "temp_t3_thres", &val) >= 0)
		cust_data->temp_t3_thres = val;
	if (of_property_read_u32(np, "temp_t3_thres_minus_x_degree", &val) >= 0)
		cust_data->temp_t3_thres_minus_x_degree = val;
	if (of_property_read_u32(np, "temp_t2_thres", &val) >= 0)
		cust_data->temp_t2_thres = val;
	if (of_property_read_u32(np, "temp_t2_thres_plus_x_degree", &val) >= 0)
		cust_data->temp_t2_thres_plus_x_degree = val;
	if (of_property_read_u32(np, "temp_t1_thres", &val) >= 0)
		cust_data->temp_t1_thres = val;
	if (of_property_read_u32(np, "temp_t1_thres_plus_x_degree", &val) >= 0)
		cust_data->temp_t1_thres_plus_x_degree = val;
	if (of_property_read_u32(np, "temp_t0_thres", &val) >= 0)
		cust_data->temp_t0_thres = val;
	if (of_property_read_u32(np, "temp_t0_thres_plus_x_degree", &val) >= 0)
		cust_data->temp_t0_thres_plus_x_degree = val;
	if (of_property_read_u32(np, "temp_neg_10_thres", &val) >= 0)
		cust_data->temp_neg_10_thres = val;
	if (of_property_read_u32(np, "max_charging_time", &val) >= 0)
		cust_data->max_charging_time = val;

	/* battery temperature protection */
	batt_thermal_data->enable_min_charge_temp =
		of_property_read_bool(np, "enable_min_charge_temp");
	if (of_property_read_u32(np, "min_charge_temp", &val) >= 0)
		batt_thermal_data->min_charge_temp = val;
	if (of_property_read_u32(np, "min_charge_temp_plus_x_degree",
				 &val) >= 0)
		batt_thermal_data->min_charge_temp_plus_x_degree = val;
	if (of_property_read_u32(np, "max_charge_temp", &val) >= 0)
		batt_thermal_data->max_charge_temp = val;
	if (of_property_read_u32(np, "max_charge_temp_minus_x_degree",
				 &val) >= 0)
		batt_thermal_data->max_charge_temp_minus_x_degree = val;
	return 0;
}

static ssize_t show_sw_jeita(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct charger_manager *chg_mgr = dev_get_drvdata(dev);

	chg_info("%s %d\n", __func__, chg_mgr->enable_sw_jeita);
	return sprintf(buf, "%d\n", chg_mgr->enable_sw_jeita);
}

static ssize_t store_sw_jeita(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t size)
{
	struct charger_manager *chg_mgr = dev_get_drvdata(dev);
	signed int temp;

	if (kstrtoint(buf, 10, &temp) == 0) {
		if (temp == 0)
			chg_mgr->enable_sw_jeita = false;
		else
			chg_mgr->enable_sw_jeita = true;

	} else
		chg_err("%s format error!\n", __func__);
	return size;
}
static DEVICE_ATTR(sw_jeita, 0664, show_sw_jeita, store_sw_jeita);

static ssize_t show_charger_log_level(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	chg_err("%s: %d\n", __func__, chg_get_log_level());
	return sprintf(buf, "%d\n", chg_get_log_level());
}

static ssize_t store_charger_log_level(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret;

	chg_err("%s\n", __func__);

	if (buf != NULL && size != 0) {
		chg_err("%s buf(%s)\n", __func__, buf);
		ret = kstrtoul(buf, 10, &val);
		if (val < 0) {
			chg_err("%s val is inavlid(%ld)\n", __func__, val);
			val = CHGLOG_LEVEL_ERROR;
		}
		chg_log_level = val;
		chg_err("%s log level = %d\n", __func__, chg_log_level);
	}
	return size;
}
static DEVICE_ATTR(charger_log_level, 0664, show_charger_log_level,
		   store_charger_log_level);


static ssize_t show_input_current(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct charger_manager *chg_mgr = dev_get_drvdata(dev);
	struct charger_data *chg = &chg_mgr->chg1_data;

	chg_debug("%s %x\n", __func__, chg->thermal_input_current_limit);
	return sprintf(buf, "%u\n", chg->thermal_input_current_limit);
}

static ssize_t store_input_current(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct charger_manager *chg_mgr = dev_get_drvdata(dev);
	struct charger_data *chg = &chg_mgr->chg1_data;
	u32 reg = 0;
	int ret;

	chg_debug("%s\n", __func__);
	if (buf != NULL && size != 0) {
		chg_debug("%s buf(%s), size = %Zu\n", __func__, buf, size);
		ret = kstrtouint(buf, 16, &reg);
		chg->thermal_input_current_limit = reg;
		chg_debug("%s %x\n", __func__,
			  chg->thermal_input_current_limit);
	}
	return size;
}
static DEVICE_ATTR(input_current, 0664, show_input_current,
		   store_input_current);

static ssize_t show_chg1_current(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct charger_manager *chg_mgr = dev_get_drvdata(dev);
	struct charger_data *chg = &chg_mgr->chg1_data;

	chg_debug("%s %x\n", __func__, chg->thermal_charging_current_limit);
	return sprintf(buf, "%u\n", chg->thermal_charging_current_limit);
}

static ssize_t store_chg1_current(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct charger_manager *chg_mgr = dev_get_drvdata(dev);
	struct charger_data *chg = &chg_mgr->chg1_data;
	u32 reg = 0;
	int ret;

	chg_debug("%s\n", __func__);
	if (buf != NULL && size != 0) {
		chg_debug("%s buf(%s), size = %Zu\n", __func__, buf, size);
		ret = kstrtouint(buf, 16, &reg);
		chg->thermal_charging_current_limit = reg;
		chg_debug("%s %x\n", __func__,
			  chg->thermal_charging_current_limit);
	}
	return size;
}
static DEVICE_ATTR(chg1_current, 0664, show_chg1_current, store_chg1_current);

/* procfs */
static int mtk_chg_current_cmd_show(struct seq_file *m, void *data)
{
	struct charger_manager *chg_mgr = m->private;

	seq_printf(m, "%d %d\n", chg_mgr->usb_unlimited,
		   chg_mgr->cmd_discharging);
	return 0;
}

static ssize_t mtk_chg_current_cmd_write(struct file *file, const char *buffer,
					 size_t count, loff_t *data)
{
	int len = 0;
	char desc[32];
	int current_unlimited = 0;
	int cmd_discharging = 0;
	struct charger_manager *chg_mgr = PDE_DATA(file_inode(file));

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (sscanf(desc, "%d %d", &current_unlimited, &cmd_discharging) == 2) {
		chg_mgr->usb_unlimited = current_unlimited;
		if (cmd_discharging == 1) {
			chg_mgr->cmd_discharging = true;
			charger_dev_enable(chg_mgr->chg1_dev, false);
			mtk_charger_manager_notify(chg_mgr,
						   CHGMGR_NOTIFY_STOP_CHARGING);
		} else if (cmd_discharging == 0) {
			chg_mgr->cmd_discharging = false;
			charger_dev_enable(chg_mgr->chg1_dev, true);
			mtk_charger_manager_notify(chg_mgr,
				CHGMGR_NOTIFY_START_CHARGING);
		}

		chg_debug("%s current_unlimited = %d, cmd_discharging = %d\n",
			  __func__, current_unlimited, cmd_discharging);
		return count;
	}

	chg_err("bad argument, echo [usb_unlimited] [disable] > current_cmd\n");
	return count;
}

static int mtk_chg_en_power_path_show(struct seq_file *m, void *data)
{
	struct charger_manager *chg_mgr = m->private;
	bool power_path_en = true;

	charger_dev_is_powerpath_enabled(chg_mgr->chg1_dev, &power_path_en);
	seq_printf(m, "%d\n", power_path_en);
	return 0;
}

static ssize_t mtk_chg_en_power_path_write(struct file *file,
					   const char *buffer, size_t count,
					   loff_t *data)
{
	int len = 0, ret = 0;
	char desc[32];
	unsigned int enable = 0;
	struct charger_manager *chg_mgr = PDE_DATA(file_inode(file));

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';
	ret = kstrtou32(desc, 10, &enable);
	if (ret == 0) {
		charger_dev_enable_powerpath(chg_mgr->chg1_dev, enable);
		chg_debug("%s: enable power path = %d\n", __func__, enable);
		return count;
	}

	chg_err("bad argument, echo [enable] > en_power_path\n");
	return count;
}

static int mtk_chg_en_safety_timer_show(struct seq_file *m, void *data)
{
	struct charger_manager *chg_mgr = m->private;
	bool safety_timer_en = false;

	charger_dev_is_safety_timer_enabled(chg_mgr->chg1_dev,
					    &safety_timer_en);
	seq_printf(m, "%d\n", safety_timer_en);
	return 0;
}

static ssize_t mtk_chg_en_safety_timer_write(struct file *file,
					     const char *buffer, size_t count,
					     loff_t *data)
{
	int len = 0, ret = 0;
	char desc[32];
	unsigned int enable = 0;
	struct charger_manager *chg_mgr = PDE_DATA(file_inode(file));

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	ret = kstrtou32(desc, 10, &enable);
	if (ret == 0) {
		charger_dev_enable_safety_timer(chg_mgr->chg1_dev, enable);
		chg_debug("%s enable safety timer = %d\n", __func__, enable);

		/* SW safety timer */
		if (chg_mgr->sw_safety_timer_setting == true) {
			if (enable)
				chg_mgr->enable_sw_safety_timer = true;
			else
				chg_mgr->enable_sw_safety_timer = false;
		}

		return count;
	}

	chg_err("bad argument, echo [enable] > en_safety_timer\n");
	return count;
}

#define PROC_FOPS_RW(name)						\
static int mtk_chg_##name##_open(struct inode *node, struct file *file)	\
{									\
	return single_open(file, mtk_chg_##name##_show, PDE_DATA(node));\
}									\
static const struct file_operations mtk_chg_##name##_fops = {		\
	.owner = THIS_MODULE,						\
	.open = mtk_chg_##name##_open,					\
	.read = seq_read,						\
	.llseek = seq_lseek,						\
	.release = single_release,					\
	.write = mtk_chg_##name##_write,				\
}

PROC_FOPS_RW(current_cmd);
PROC_FOPS_RW(en_power_path);
PROC_FOPS_RW(en_safety_timer);

/* Create sysfs and procfs attributes */
static int mtk_charger_setup_files(struct charger_manager *chg_mgr)
{
	int ret = 0;
	struct proc_dir_entry *battery_dir = NULL;

	ret = device_create_file(chg_mgr->dev, &dev_attr_sw_jeita);
	if (ret)
		goto out;
	ret = device_create_file(chg_mgr->dev, &dev_attr_charger_log_level);
	if (ret)
		goto out;
	ret = device_create_file(chg_mgr->dev, &dev_attr_input_current);
	if (ret)
		goto out;
	ret = device_create_file(chg_mgr->dev, &dev_attr_chg1_current);
	if (ret)
		goto out;

	battery_dir = proc_mkdir("mtk_battery_cmd", NULL);
	if (!battery_dir) {
		chg_err("%s mkdir /proc/mtk_battery_cmd fail\n", __func__);
		return -ENOMEM;
	}
	proc_create_data("current_cmd", 0644, battery_dir,
			 &mtk_chg_current_cmd_fops, chg_mgr);
	proc_create_data("en_power_path", 0644, battery_dir,
			 &mtk_chg_en_power_path_fops, chg_mgr);
	proc_create_data("en_safety_timer", 0644, battery_dir,
			 &mtk_chg_en_safety_timer_fops, chg_mgr);

out:
	return ret;
}


static int mtk_charger_probe(struct platform_device *pdev)
{
	int ret;
	struct charger_manager *chg_mgr;
	struct list_head *pos;
	struct list_head *phead = &consumer_head;
	struct charger_consumer *consumer;

	dev_info(&pdev->dev, "%s\n", __func__);

	chg_mgr = devm_kzalloc(&pdev->dev, sizeof(*chg_mgr), GFP_KERNEL);
	if (!chg_mgr)
		return -ENOMEM;
	g_chg_mgr = chg_mgr;

	spin_lock_init(&chg_mgr->slock);
	mutex_init(&chg_mgr->chg_lock);
	atomic_set(&chg_mgr->enable_kpoc_shdn, 1);
	wakeup_source_init(&chg_mgr->chg_wlock, "charger suspend wlock");
	chg_mgr->dev = &pdev->dev;
	platform_set_drvdata(pdev, chg_mgr);
	ret = mtk_charger_parse_dt(chg_mgr);
	if (ret < 0)
		return ret;

	/* init thread */
	init_waitqueue_head(&chg_mgr->wait_que);
	chg_mgr->polling_interval = CHG_POLLING_INTERVAL;

	chg_mgr->battery_temp = 25;
	chg_mgr->chg1_data.thermal_charging_current_limit = -1;
	chg_mgr->chg1_data.thermal_input_current_limit = -1;
	chg_mgr->chg1_data.input_current_limit_by_aicl = -1;
	chg_mgr->sw_jeita.error_recovery_flag = true;

	mtk_charger_init_timer(chg_mgr);
	kthread_run(charger_routine_thread, chg_mgr, "charger_thread");

	/* register charger device notifier */
	if (chg_mgr->chg1_dev != NULL && chg_mgr->do_event != NULL) {
		chg_mgr->chg1_nb.notifier_call = chg_mgr->do_event;
		register_charger_device_notifier(chg_mgr->chg1_dev,
						 &chg_mgr->chg1_nb);
		charger_dev_set_drvdata(chg_mgr->chg1_dev, chg_mgr);
	}

	/* register power supply notifier */
	srcu_init_notifier_head(&chg_mgr->evt_nh);
	ret = mtk_charger_setup_files(chg_mgr);
	if (ret)
		chg_err("Error creating sysfs interface\n");

	charger_ftm_init();
	mtk_charger_get_atm_mode(chg_mgr);

#ifdef CONFIG_MTK_CHARGER_UNLIMITED
	chg_mgr->usb_unlimited = true;
	chg_mgr->enable_sw_safety_timer = false;
	charger_dev_enable_safety_timer(chg_mgr->chg1_dev, false);
#endif /* CONFIG_MTK_CHARGER_UNLIMITED */

	mutex_lock(&consumer_lock);
	/* Make sure all consumers have pointer of charger manager */
	list_for_each(pos, phead) {
		consumer = container_of(pos, struct charger_consumer, list);
		consumer->cm = chg_mgr;
		if (consumer->nb) {
			srcu_notifier_chain_register(&chg_mgr->evt_nh,
						     consumer->nb);
			consumer->nb = NULL;
		}
	}
	mutex_unlock(&consumer_lock);
	chg_mgr->chg1_consumer = mtk_charger_manager_get_consumer(chg_mgr->dev);
	chg_mgr->init_done = true;
	mtk_wake_up_charger(chg_mgr);
	chg_info("%s successfully\n", __func__);
	return 0;
}

static int mtk_charger_remove(struct platform_device *pdev)
{
	struct charger_manager *chg_mgr = platform_get_drvdata(pdev);

	dev_info(&pdev->dev, "%s\n", __func__);
	if (chg_mgr)
		mutex_destroy(&chg_mgr->chg_lock);
	return 0;
}

static void mtk_charger_shutdown(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "%s\n", __func__);
}

static int mtk_charger_suspend(struct device *dev)
{
	dev_info(dev, "%s\n", __func__);
	return 0;
}

static int mtk_charger_resume(struct device *dev)
{
	dev_info(dev, "%s\n", __func__);
	return 0;
}

static SIMPLE_DEV_PM_OPS(mtk_charger_pm_ops, mtk_charger_suspend,
			 mtk_charger_resume);

static const struct platform_device_id mtk_charger_id_table[] = {
	{ "mtk_charger", },
	{},
};
MODULE_DEVICE_TABLE(platform, mtk_charger_id_table);

#ifdef CONFIG_OF
static const struct of_device_id mtk_charger_of_match_table[] = {
	{.compatible = "mediatek,charger",},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_charger_of_match_table);
#endif /* CONFIG_OF */

static struct platform_driver charger_driver = {
	.probe = mtk_charger_probe,
	.remove = mtk_charger_remove,
	.shutdown = mtk_charger_shutdown,
	.driver = {
		   .name = "mtk_charger",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(mtk_charger_of_match_table),
		   .pm = &mtk_charger_pm_ops,
	},
	.id_table = mtk_charger_id_table,
};

static int __init mtk_charger_init(void)
{
	return platform_driver_register(&charger_driver);
}
late_initcall(mtk_charger_init);

static void __exit mtk_charger_exit(void)
{
	platform_driver_unregister(&charger_driver);
}
module_exit(mtk_charger_exit);

MODULE_AUTHOR("ShuFan Lee <shufan_lee@richtek.com>");
MODULE_DESCRIPTION("MTK Charger Manager");
MODULE_LICENSE("GPL");
