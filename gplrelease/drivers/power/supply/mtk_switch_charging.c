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

#include <linux/device.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/time.h>

#include <mt-plat/mtk_boot.h>

#include "mtk_charger.h"
#include "mtk_switch_charging.h"

static inline int _uA_to_mA(int uA)
{
	return (uA == -1) ? -1 : (uA / 1000);
}

static void mtk_swchg_select_charging_current(struct charger_manager *chg_mgr)
{
	int ret = 0;
	int boot_mode = get_boot_mode();
	u32 ichg_min = 0, aicr_min = 0;
	struct charger_data *chg = &chg_mgr->chg1_data;
	struct charger_custom_data *data = chg_mgr->cust_data;
	struct switch_charging_alg_data *swchg_alg = chg_mgr->algorithm_data;

	mutex_lock(&swchg_alg->ichg_aicr_lock);

	/* AICL */
	charger_dev_run_aicl(chg_mgr->chg1_dev,
			     &chg->input_current_limit_by_aicl);

	if (chg->force_charging_current > 0) {
		chg->charging_current_limit = chg->force_charging_current;
		if (chg->force_charging_current <= 450000)
			chg->input_current_limit = 500000;
		else {
			chg->input_current_limit =
				data->ac_charger_input_current;
			chg->charging_current_limit = data->ac_charger_current;
		}
		goto done;
	}

	if (chg_mgr->usb_unlimited) {
		chg->input_current_limit = data->ac_charger_input_current;
		chg->charging_current_limit = data->ac_charger_current;
		goto done;
	}

	if (boot_mode == META_BOOT || boot_mode == ADVMETA_BOOT) {
		chg->input_current_limit = 200000; /* 200mA */
		goto done;
	}

	if (chg_mgr->atm_enabled == true &&
	    (chg_mgr->chg_type == STANDARD_HOST ||
	     chg_mgr->chg_type == CHARGING_HOST)) {
		chg->input_current_limit = 100000; /* 100mA */
		goto done;
	}

	if (chg_mgr->chg_type == STANDARD_HOST) {
		if (IS_ENABLED(CONFIG_USBIF_COMPLIANCE)) {
			if (chg_mgr->usb_state == USB_SUSPEND)
				chg->input_current_limit =
					data->usb_charger_current_suspend;
			else if (chg_mgr->usb_state == USB_UNCONFIGURED)
				chg->input_current_limit =
					data->usb_charger_current_unconfigured;
			else if (chg_mgr->usb_state == USB_CONFIGURED)
				chg->input_current_limit =
					data->usb_charger_current_configured;
			else
				chg->input_current_limit =
					data->usb_charger_current_unconfigured;
			chg->charging_current_limit = chg->input_current_limit;
		} else {
			chg->input_current_limit = data->usb_charger_current;
			/* it can be larger */
			chg->charging_current_limit = data->usb_charger_current;
		}
	} else if (chg_mgr->chg_type == NONSTANDARD_CHARGER) {
		chg->input_current_limit = data->non_std_ac_charger_current;
		chg->charging_current_limit = data->non_std_ac_charger_current;
	} else if (chg_mgr->chg_type == STANDARD_CHARGER) {
		chg->input_current_limit = data->ac_charger_input_current;
		chg->charging_current_limit = data->ac_charger_current;
	} else if (chg_mgr->chg_type == CHARGING_HOST) {
		chg->input_current_limit = data->charging_host_charger_current;
		chg->charging_current_limit =
			data->charging_host_charger_current;
	} else if (chg_mgr->chg_type == APPLE_1_0A_CHARGER) {
		chg->input_current_limit = data->apple_1_0a_charger_current;
		chg->charging_current_limit = data->apple_1_0a_charger_current;
	} else if (chg_mgr->chg_type == APPLE_2_1A_CHARGER) {
		chg->input_current_limit = data->apple_2_1a_charger_current;
		chg->charging_current_limit = data->apple_2_1a_charger_current;
	}

	if (chg_mgr->enable_sw_jeita) {
		if (IS_ENABLED(CONFIG_USBIF_COMPLIANCE) &&
		    chg_mgr->chg_type == STANDARD_HOST)
			chg_debug("USBIF & STAND_HOST skip current check\n");
		else {
			if (chg_mgr->sw_jeita.sm == TEMP_T0_TO_T1) {
				chg->input_current_limit = 500000;
				chg->charging_current_limit = 350000;
			}
		}
	}

	if ((chg->thermal_charging_current_limit != -1) &&
	    (chg->thermal_charging_current_limit < chg->charging_current_limit))
		chg->charging_current_limit =
			chg->thermal_charging_current_limit;

	if ((chg->thermal_input_current_limit != -1) &&
	    (chg->thermal_input_current_limit < chg->input_current_limit))
		chg->input_current_limit = chg->thermal_input_current_limit;
done:
	ret = charger_dev_get_min_charging_current(chg_mgr->chg1_dev,
						   &ichg_min);
	if (ret != -ENOTSUPP && chg->charging_current_limit < ichg_min)
		chg->charging_current_limit = ichg_min;

	ret = charger_dev_get_min_input_current(chg_mgr->chg1_dev, &aicr_min);
	if (ret != -ENOTSUPP && chg->input_current_limit < aicr_min)
		chg->input_current_limit = 0;

	chg_info("force:%d thermal:%d,%d setting:%d %d type:%d usb_unlimited:%d usbif:%d usbsm:%d aicl:%d atm:%d\n",
		 _uA_to_mA(chg->force_charging_current),
		 _uA_to_mA(chg->thermal_input_current_limit),
		 _uA_to_mA(chg->thermal_charging_current_limit),
		 _uA_to_mA(chg->input_current_limit),
		 _uA_to_mA(chg->charging_current_limit), chg_mgr->chg_type,
		 chg_mgr->usb_unlimited, IS_ENABLED(CONFIG_USBIF_COMPLIANCE),
		 chg_mgr->usb_state, chg->input_current_limit_by_aicl,
		 chg_mgr->atm_enabled);
	charger_dev_set_input_current(chg_mgr->chg1_dev,
				      chg->input_current_limit);
	charger_dev_set_charging_current(chg_mgr->chg1_dev,
					 chg->charging_current_limit);

	/*
	 * If thermal current limit is larger than charging IC's minimum
	 * current setting, enable the charger immediately
	 */
	if (chg->input_current_limit > aicr_min &&
	    chg->charging_current_limit > ichg_min && chg_mgr->can_charging)
		charger_dev_enable(chg_mgr->chg1_dev, true);
	mutex_unlock(&swchg_alg->ichg_aicr_lock);
}

static int mtk_swchg_select_cv(struct charger_manager *chg_mgr)
{
	if (chg_mgr->enable_sw_jeita && chg_mgr->sw_jeita.cv != 0)
		return charger_dev_set_constant_voltage(chg_mgr->chg1_dev,
							chg_mgr->sw_jeita.cv);

	return charger_dev_set_constant_voltage(chg_mgr->chg1_dev,
						chg_mgr->cust_data->battery_cv);
}

static int mtk_swchg_charging_process(struct charger_manager *chg_mgr)
{
	struct switch_charging_alg_data *swchg_alg = chg_mgr->algorithm_data;
	bool chg_en = true;
	int boot_mode = get_boot_mode();

	if (swchg_alg->state == CHG_STATE_ERROR) {
		chg_en = false;
		chg_err("%s charging error, disable charging!\n", __func__);
	} else if (boot_mode == META_BOOT || boot_mode == ADVMETA_BOOT) {
		chg_en = false;
		chg_mgr->chg1_data.input_current_limit = 200000; /* 200mA */
		charger_dev_set_input_current(chg_mgr->chg1_dev,
			chg_mgr->chg1_data.input_current_limit);
		chg_info("%s meta mode, disable charging and AICR = 200mA\n",
			 __func__);
	} else {
		mtk_swchg_select_charging_current(chg_mgr);
		if (chg_mgr->chg1_data.input_current_limit == 0 ||
		    chg_mgr->chg1_data.charging_current_limit == 0) {
			chg_en = false;
			chg_info("%s AICR/ICC = 0mA, disable charging!\n",
				 __func__);
			goto out;
		}
		mtk_swchg_select_cv(chg_mgr);
	}

out:
	return charger_dev_enable(chg_mgr->chg1_dev, chg_en);
}

static bool mtk_swchg_check_charging_time(struct charger_manager *chg_mgr)
{
	struct switch_charging_alg_data *swchg_alg = chg_mgr->algorithm_data;
	struct timespec time_now;

	if (!chg_mgr->enable_sw_safety_timer)
		return true;

	get_monotonic_boottime(&time_now);
	chg_debug("%s begin: %ld, now: %ld\n", __func__,
		  swchg_alg->charging_begin_time.tv_sec, time_now.tv_sec);

	if (swchg_alg->total_charging_time >=
	    chg_mgr->cust_data->max_charging_time) {
		chg_err("%s SW safety timeout: %d sec > %d sec\n",
			__func__, swchg_alg->total_charging_time,
			chg_mgr->cust_data->max_charging_time);
		charger_dev_notify(chg_mgr->chg1_dev,
				   CHARGER_DEV_NOTIFY_SAFETY_TIMEOUT);
		return false;
	}
	return true;
}

static int mtk_swchg_cc(struct charger_manager *chg_mgr)
{
	int ret;
	bool chg_done = false;
	struct switch_charging_alg_data *swchg_alg = chg_mgr->algorithm_data;
	struct timespec time_now, charging_time;

	get_monotonic_boottime(&time_now);
	charging_time = timespec_sub(time_now, swchg_alg->charging_begin_time);
	swchg_alg->total_charging_time = charging_time.tv_sec;
	mtk_swchg_charging_process(chg_mgr);
	ret = charger_dev_is_charging_done(chg_mgr->chg1_dev, &chg_done);
	if (ret >= 0 && chg_done) {
		swchg_alg->state = CHG_STATE_FULL;
		charger_dev_do_event(chg_mgr->chg1_dev, EVENT_EOC, 0);
		chg_info("%s battery full!\n", __func__);
	}

	return 0;
}

static int mtk_swchg_full(struct charger_manager *chg_mgr)
{
	int ret;
	bool chg_done = false;
	struct switch_charging_alg_data *swchg_alg = chg_mgr->algorithm_data;

	swchg_alg->total_charging_time = 0;

	/*
	 * If CV is set to lower value by JEITA,
	 * Reset CV to normal value if temperture is in normal zone
	 */
	mtk_swchg_select_cv(chg_mgr);
	chg_mgr->polling_interval = CHGFULL_POLLING_INTERVAL;
	ret = charger_dev_is_charging_done(chg_mgr->chg1_dev, &chg_done);
	if (ret >= 0 && !chg_done) {
		swchg_alg->state = CHG_STATE_CC;
		charger_dev_do_event(chg_mgr->chg1_dev, EVENT_RECHARGE, 0);
		get_monotonic_boottime(&swchg_alg->charging_begin_time);
		chg_info("%s battery recharging!\n", __func__);
		chg_mgr->polling_interval = CHG_POLLING_INTERVAL;
	}
	return 0;
}

static int mtk_swchg_err(struct charger_manager *chg_mgr)
{
	struct switch_charging_alg_data *swchg_alg = chg_mgr->algorithm_data;

	if (chg_mgr->enable_sw_jeita) {
		if ((chg_mgr->sw_jeita.sm == TEMP_BELOW_T0) ||
		    (chg_mgr->sw_jeita.sm == TEMP_ABOVE_T4))
			chg_mgr->sw_jeita.error_recovery_flag = false;

		if ((chg_mgr->sw_jeita.error_recovery_flag == false) &&
		    (chg_mgr->sw_jeita.sm != TEMP_BELOW_T0) &&
		    (chg_mgr->sw_jeita.sm != TEMP_ABOVE_T4)) {
			chg_mgr->sw_jeita.error_recovery_flag = true;
			swchg_alg->state = CHG_STATE_CC;
			get_monotonic_boottime(&swchg_alg->charging_begin_time);
		}
	}

	swchg_alg->total_charging_time = 0;
	charger_dev_enable(chg_mgr->chg1_dev, false);
	return 0;
}

static int mtk_switch_charging_plug_in(struct charger_manager *chg_mgr)
{
	struct switch_charging_alg_data *swchg_alg = chg_mgr->algorithm_data;

	swchg_alg->state = CHG_STATE_CC;
	chg_mgr->polling_interval = CHG_POLLING_INTERVAL;
	swchg_alg->disable_charging = false;
	get_monotonic_boottime(&swchg_alg->charging_begin_time);
	mtk_charger_manager_notify(chg_mgr, CHGMGR_NOTIFY_START_CHARGING);
	return 0;
}

static int mtk_switch_charging_plug_out(struct charger_manager *chg_mgr)
{
	struct switch_charging_alg_data *swchg_alg = chg_mgr->algorithm_data;

	swchg_alg->total_charging_time = 0;
	mtk_charger_manager_notify(chg_mgr, CHGMGR_NOTIFY_STOP_CHARGING);
	return 0;
}

static int mtk_switch_charging_do_charging(struct charger_manager *chg_mgr,
					   bool en)
{
	struct switch_charging_alg_data *swchg_alg = chg_mgr->algorithm_data;

	chg_info("%s (%s) en = %d\n", __func__, chg_mgr->algorithm_name, en);

	if (en) {
		swchg_alg->disable_charging = false;
		swchg_alg->state = CHG_STATE_CC;
		get_monotonic_boottime(&swchg_alg->charging_begin_time);
		mtk_charger_manager_notify(chg_mgr, CHGMGR_NOTIFY_NORMAL);
	} else {
		/* disable charging might change state, so call it first */
		charger_dev_enable(chg_mgr->chg1_dev, false);
		swchg_alg->disable_charging = true;
		swchg_alg->state = CHG_STATE_ERROR;
		mtk_charger_manager_notify(chg_mgr, CHGMGR_NOTIFY_ERROR);
	}

	return 0;
}

static int mtk_switch_charging_current(struct charger_manager *chg_mgr)
{
	mtk_swchg_select_charging_current(chg_mgr);
	return 0;
}

static int mtk_switch_charging_run(struct charger_manager *chg_mgr)
{
	int ret = 0;
	struct switch_charging_alg_data *swchg_alg = chg_mgr->algorithm_data;

	chg_info("%s state = %d, time = %d\n", __func__, swchg_alg->state,
		 swchg_alg->total_charging_time);

	do {
		switch (swchg_alg->state) {
		case CHG_STATE_CC:
			ret = mtk_swchg_cc(chg_mgr);
			break;
		case CHG_STATE_FULL:
			ret = mtk_swchg_full(chg_mgr);
			break;
		case CHG_STATE_ERROR:
			ret = mtk_swchg_err(chg_mgr);
			break;
		default:
			break;
		}
	} while (ret != 0);

	mtk_swchg_check_charging_time(chg_mgr);
	charger_dev_dump_registers(chg_mgr->chg1_dev);
	return 0;
}

static int mtk_switch_charging_do_event(struct notifier_block *nb,
					unsigned long event, void *data)
{
	struct charger_manager *chg_mgr =
		container_of(nb, struct charger_manager, chg1_nb);
	struct chgdev_notify *noti = data;

	chg_info("%s %ld", __func__, event);

	switch (event) {
	case CHARGER_DEV_NOTIFY_EOC:
		mtk_charger_manager_notify(chg_mgr, CHGMGR_NOTIFY_EOC);
		chg_info("%s end of charge\n", __func__);
		break;
	case CHARGER_DEV_NOTIFY_RECHG:
		mtk_charger_manager_notify(chg_mgr,
					   CHGMGR_NOTIFY_START_CHARGING);
		chg_info("%s recharge\n", __func__);
		break;
	case CHARGER_DEV_NOTIFY_SAFETY_TIMEOUT:
		chg_mgr->safety_timeout = true;
		chg_err("%s safety timer timeout\n", __func__);

		/* If sw safety timer timeout, do not wake up charger thread */
		if (chg_mgr->enable_sw_safety_timer)
			return NOTIFY_DONE;
		break;
	case CHARGER_DEV_NOTIFY_VBUS_OVP:
		chg_mgr->vbusov_stat = noti->vbusov_stat;
		chg_err("%s vbus ovp = %d\n", __func__, chg_mgr->vbusov_stat);
		break;
	default:
		return NOTIFY_DONE;
	}

	if (!chg_mgr->chg1_dev->is_polling_mode)
		mtk_wake_up_charger(chg_mgr);

	return NOTIFY_DONE;
}

int mtk_switch_charging_init(struct charger_manager *chg_mgr)
{
	struct switch_charging_alg_data *swchg_alg;

	swchg_alg = devm_kzalloc(chg_mgr->dev, sizeof(*swchg_alg), GFP_KERNEL);
	if (!swchg_alg)
		return -ENOMEM;

	chg_mgr->chg1_dev = get_charger_by_name("primary_chg");
	if (chg_mgr->chg1_dev)
		chg_info("Found primary charger [%s]\n",
			 chg_mgr->chg1_dev->props.alias_name);
	else
		chg_info("*** Error : can't find primary charger ***\n");

	mutex_init(&swchg_alg->ichg_aicr_lock);
	chg_mgr->algorithm_data = swchg_alg;
	chg_mgr->do_algorithm = mtk_switch_charging_run;
	chg_mgr->plug_in = mtk_switch_charging_plug_in;
	chg_mgr->plug_out = mtk_switch_charging_plug_out;
	chg_mgr->do_charging = mtk_switch_charging_do_charging;
	chg_mgr->do_event = mtk_switch_charging_do_event;
	chg_mgr->change_current_setting = mtk_switch_charging_current;
	return 0;
}
