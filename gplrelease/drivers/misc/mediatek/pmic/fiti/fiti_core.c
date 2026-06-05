/*****************************************************************************
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * Accelerometer Sensor Driver
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 *****************************************************************************/


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/machine.h>
#include <linux/mfd/core.h>
#include <asm/mach-types.h>
//#include "hwtcon_def.h"
#include "fiti_core.h"
//#include "hwtcon_hal.h"
//#include "hwtcon_reg.h"
#include <linux/interrupt.h>
#include <linux/of_irq.h>

#define DEFAULT_FITI_TEMP	25

#define FITI_ERR(string, args...) \
	pr_notice("[FITI ERR]"string" @%s,%u\n", ##args, __func__, __LINE__)
#define FITI_LOG(string, args...) \
	pr_debug("[FITI LOG]"string" @%s,%u\n", ##args, __func__, __LINE__)

extern int idme_hwid_value;
extern int idme_vcom_value;
struct i2c_client *fiti_client;
struct fiti_context g_fiti;

static const int vcom_value = 3300;
static const int vcom_default_value = 1830;
static const int vcom_reg_value = 0x74;
static const int vcom_value_precision = 22;

/* waitqueue for fiti power ok. */
wait_queue_head_t g_fiti_power_OK_wait_queue;

#define FITI_ENABLE_TIMEOUT_MS (1000)
#define FITI_OFF_TIMEOUT_MS (500)

unsigned int cal_vcom_reg(void)
{
	unsigned int reg_value = 0;

	reg_value =
	    vcom_reg_value +
	    ((vcom_value - vcom_default_value) / vcom_value_precision);
	return reg_value;
}

int i2c_write(struct i2c_client *client, u8 *wr_buf, int wr_len)
{
	int ret = 0;
	struct i2c_adapter *adap = client->adapter;
	struct i2c_msg msg;
	u8 *w_buf = NULL;

	memset(&msg, 0, sizeof(struct i2c_msg));
	w_buf = kzalloc(wr_len, GFP_KERNEL);

	if (w_buf == NULL) {
		FITI_ERR("w_buf==null wr_len:%d!\n", wr_len);
		return -1;
	}

	memcpy(w_buf, wr_buf, wr_len);
	msg.addr = client->addr;
	msg.flags = 0;		//write
	msg.len = wr_len;
	msg.buf = w_buf;	//register addr + data

	ret = i2c_transfer(adap, &msg, 1);
	kfree(w_buf);
	return ret;

}

static int i2c_read(struct i2c_client *client, u8 reg_addr, u8 *rd_buf,
		    int rd_len)
{
	int ret = 0;
	struct i2c_adapter *adap = client->adapter;
	struct i2c_msg msg[2];
	u8 *w_buf = NULL;
	u8 *r_buf = NULL;

	memset(msg, 0, 2 * sizeof(struct i2c_msg));
	w_buf = kzalloc(2, GFP_KERNEL);
	if (w_buf == NULL)
		return -1;

	r_buf = kzalloc(rd_len, GFP_KERNEL);
	if (r_buf == NULL) {
		kfree(w_buf);
		return -1;
	}

	*w_buf = reg_addr;
	msg[0].addr = client->addr;
	msg[0].flags = 0;	//write
	msg[0].len = 1;
	msg[0].buf = w_buf;
	msg[1].addr = client->addr;
	msg[1].flags = 1;	//read
	msg[1].len = rd_len;
	msg[1].buf = r_buf;

	ret = i2c_transfer(adap, msg, 2);
	memcpy(rd_buf, r_buf, rd_len);
	kfree(w_buf);
	kfree(r_buf);
	return ret;

}

int fiti_i2c_write(unsigned char reg, unsigned char writedata)
{
	u8 databuf[3] = { 0 };
	int ret = 0;

	databuf[0] = reg;
	databuf[1] = writedata;

	if (fiti_client == NULL)
		return -1;

	ret = i2c_write(fiti_client, databuf, 2);
	if (ret < 0)
		FITI_ERR("fiti_i2c_write send data failed ret:%d!\n", ret);

	return ret;
}
EXPORT_SYMBOL(fiti_i2c_write);


int fiti_i2c_read(unsigned char reg, unsigned char *rd_buf)
{
	int ret = 0;


	if (fiti_client == NULL)
		return -1;

	ret = i2c_read(fiti_client, reg, rd_buf, 1);
	if (ret < 0) {
		FITI_ERR(" codec? --- fiti_i2c_write read data failed !\n");
		return -1;
	}
	return ret;
}
EXPORT_SYMBOL(fiti_i2c_read);

int fiti_read_vcom(void)
{
	int ret = 0;
	unsigned char reg_value = 0x00;
	int vcom_value = -2500;

	ret = fiti_i2c_read(FITI9930_VCOM_SETTING, &reg_value);
	vcom_value = reg_value * 5000 / 255 * -1;
	FITI_LOG("read reg:0x%x,val:0x%x,vcom:%d", FITI9930_VCOM_SETTING,
		reg_value, vcom_value);
	return vcom_value;
}
EXPORT_SYMBOL(fiti_read_vcom);

void fiti_write_vcom(unsigned int vcom_value)
{
	int ret = 0;
	unsigned char reg_value = 0x00;
	g_fiti.pmic_setting.VCOM_SETTING = vcom_value;

	reg_value = vcom_value * 255 / 5000;
	FITI_LOG("set reg:0x%x,val:0x%x,vcom:%d", FITI9930_VCOM_SETTING,
		reg_value, vcom_value);
	ret = fiti_i2c_write(FITI9930_VCOM_SETTING, reg_value);
}
EXPORT_SYMBOL(fiti_write_vcom);

void fiti_set_version(unsigned int version)
{
	FITI_LOG("set fiti version:%d", version);
	g_fiti.version = version;
}

EXPORT_SYMBOL(fiti_set_version);

int fiti_read_temperature(void)
{
	int ret = 0;
	unsigned char reg_value = 0x00;
	
	int temperature_value = DEFAULT_FITI_TEMP;
	
#ifdef CONFIG_IDME
	if (idme_hwid_value <= 1) {
		return temperature_value;
	}
#endif
	

	/* en pmic ts to enable temperature read */
	gpio_set_value(g_fiti.EPD_EN_TS, 1);

	/* the limit is 20ms */
	msleep(FITI_TS_WAIT_TIME_MS * 2);

	ret = fiti_i2c_read(FITI9930_TMST_VALUE, &reg_value);

	FITI_LOG("I2C read: 0x%x = 0x%x",
		0x00, reg_value);

	/* conver temperature */
	if (reg_value > 0x80)
		temperature_value = (0x100 - reg_value)*-1;
	else
		temperature_value = reg_value;
	FITI_LOG("temperature value = %d!", temperature_value);

	/* disable pmic ts after temperature read */
	gpio_set_value(g_fiti.EPD_EN_TS, 0);

	return temperature_value;
}
EXPORT_SYMBOL(fiti_read_temperature);

void fiti_set_night_mode(bool enable)
{
	int ret = 0;
	unsigned char reg_value = 0x00;

	if (g_fiti.fiti_id == FITI_9929) {
		ret = fiti_i2c_read(0x10, &reg_value);

		FITI_LOG("I2C read: 0x%x = 0x%x",
			0x10, reg_value);

		if (enable)
			ret = fiti_i2c_write(0x10, reg_value|(enable<<4));
		else
			ret = fiti_i2c_write(0x10, reg_value&(~(0x1<<4)));

	} else if (g_fiti.fiti_id == FITI_9930) {
		ret = fiti_i2c_read(FITI9930_CONTROL_REG1, &reg_value);

		FITI_LOG("I2C read: 0x%x = 0x%x",
			0x0B, reg_value);
		if (enable)
			ret = fiti_i2c_write(FITI9930_CONTROL_REG1,
				reg_value|(enable<<0));
		else
			ret = fiti_i2c_write(FITI9930_CONTROL_REG1,
				reg_value&(~(0x1<<0)));
	}
}
EXPORT_SYMBOL(fiti_set_night_mode);

irqreturn_t fiti_power_good_irq(int irq, void *data)
{
	unsigned long flags;

	if (g_fiti.power_good_status)
		irq_set_irq_type(g_fiti.power_good_irq, IRQ_TYPE_LEVEL_HIGH);
	else
		irq_set_irq_type(g_fiti.power_good_irq, IRQ_TYPE_LEVEL_LOW);

	g_fiti.power_good_status = !g_fiti.power_good_status;
	FITI_LOG("[fiti] power_good_irq status:%d", g_fiti.power_good_status);

	spin_lock_irqsave(&g_fiti.power_state_lock, flags);
	g_fiti.current_state = g_fiti.power_good_status ? POWER_ON : POWER_OFF;
	spin_unlock_irqrestore(&g_fiti.power_state_lock, flags);
	/* fiti_pmic_config_pmic */
	queue_work(g_fiti.power_workqueue, &g_fiti.power_work);
	wake_up(&g_fiti_power_OK_wait_queue);

	return IRQ_HANDLED;
}

void fiti9930_register_setting(void)
{
	int reg_value = 0x00;

	reg_value = g_fiti.pmic_setting.VGH_EXT / VGH_EXT_STEP;
	fiti_i2c_write(FITI9930_VGH_EXT, reg_value);

	reg_value = g_fiti.pmic_setting.VGL_EXT / VGL_EXT_STEP;
	fiti_i2c_write(FITI9930_VGL_EXT, reg_value);

	if (g_fiti.pmic_setting.XON_LEN > XON_DELAY_MAX)
		g_fiti.pmic_setting.XON_LEN = XON_DELAY_MAX;

	reg_value = g_fiti.pmic_setting.XON_LEN / XON_LEN_STEP;
	fiti_i2c_write(FITI9930_XON_LEN, reg_value);

	reg_value = g_fiti.pmic_setting.XON_DELAY / XON_DELAY_STEP;
	fiti_i2c_write(FITI9930_XON_DELAY, reg_value);

	reg_value = g_fiti.pmic_setting.VGHNM_SETTING * 100 /
		g_fiti.pmic_setting.VGH;

	if (reg_value >= VGHNM_SETTING_VALID)
		reg_value = reg_value - VGHNM_SETTING_VALID;
	else {
		FITI_ERR("VGHNM_SETTING value:%d error! force to set 0",
			reg_value);
		reg_value = 0;
	}
	fiti_i2c_write(FITI9930_VGHNM_SETTING, reg_value);

	reg_value = g_fiti.pmic_setting.VGHNM_EXT / VGHNM_EXT_STEP;

	fiti_i2c_write(FITI9930_VGHNM_EXT, reg_value);

	/* VPOS VNEG +-15V */
	reg_value = 0x28;
	fiti_i2c_write(FITI9930_VPOS_VNEG_SETTING, reg_value);

#ifdef CONFIG_IDME
	fiti_write_vcom(idme_vcom_value != S32_MIN? idme_vcom_value : vcom_default_value);
#else
	fiti_write_vcom(g_fiti.pmic_setting.VCOM_SETTING);
#endif

}

void edp_vdd_enable(void)
{
	int ret = 0;

	if (g_fiti.reg_edp == NULL)
		FITI_ERR("reg edp is NULL");

	if (!regulator_is_enabled(g_fiti.reg_edp)) {
		regulator_set_voltage(g_fiti.reg_edp, 1800000, 1800000);
		ret = regulator_enable(g_fiti.reg_edp);
	}
}
EXPORT_SYMBOL(edp_vdd_enable);

void edp_vdd_disable(void)
{
	int ret = 0;

	if (g_fiti.reg_edp == NULL)
		FITI_ERR("reg edp is NULL");
	if (regulator_is_enabled(g_fiti.reg_edp))
		ret = regulator_disable(g_fiti.reg_edp);
}
EXPORT_SYMBOL(edp_vdd_disable);

void fiti_setting_get_from_waveform(char *waveform_addr)
{
	g_fiti.pmic_setting.VPOS = ((*(waveform_addr+0x62)<<8)+
		*(waveform_addr+0x63))*25/2;
	g_fiti.pmic_setting.VNEG = ((*(waveform_addr+0x64)<<8)+
		*(waveform_addr+0x65))*25/2;
	g_fiti.pmic_setting.VGH = ((*(waveform_addr+0x66)<<8)+
		*(waveform_addr+0x67))*25/2;
	g_fiti.pmic_setting.VGL = ((*(waveform_addr+0x68)<<8)+
		*(waveform_addr+0x69))*25/2;
	g_fiti.pmic_setting.VGH_EXT = *(waveform_addr+0x72)*50;
	g_fiti.pmic_setting.VGL_EXT = *(waveform_addr+0x73)*50;
	g_fiti.pmic_setting.XON_LEN = *(waveform_addr+0x74)*10;
	g_fiti.pmic_setting.XON_DELAY = *(waveform_addr+0x75)*5;
	g_fiti.pmic_setting.VGHNM_SETTING = ((*(waveform_addr+0x7C)<<8)+
		*(waveform_addr+0x7D))*25/2;
	g_fiti.pmic_setting.VGHNM_EXT = *(waveform_addr+0x7E)*50;

	fiti9930_register_setting();

	FITI_LOG("vgh_ext:%d, vgl_ext:%d",
		g_fiti.pmic_setting.VGH_EXT,
		g_fiti.pmic_setting.VGL_EXT);

	FITI_LOG("xon_len:%d, xon_delay:%d, vgh:%d",
		g_fiti.pmic_setting.XON_LEN,
		g_fiti.pmic_setting.XON_DELAY,
		g_fiti.pmic_setting.VGH);

	FITI_LOG("vgl:%d, vcom:%d, vghnm_setting:%d",
		g_fiti.pmic_setting.VGL,
		g_fiti.pmic_setting.VCOM_SETTING,
		g_fiti.pmic_setting.VGHNM_SETTING);

	FITI_LOG("vghnm_ext:%d, vpos:%d",
		g_fiti.pmic_setting.VGHNM_EXT,
		g_fiti.pmic_setting.VPOS);

}
EXPORT_SYMBOL(fiti_setting_get_from_waveform);

static void hwtcon_fiti_pinmux_control(struct device *pdev)
{
	g_fiti.pctrl = devm_pinctrl_get(pdev);
	if (IS_ERR(g_fiti.pctrl))
		FITI_LOG("fiti devm_pinctrl_get error!");

	g_fiti.pin_state_active = pinctrl_lookup_state(g_fiti.pctrl, "active");
	if (IS_ERR(g_fiti.pin_state_active))
		FITI_LOG("fiti pinctrl_lookup_state active error!");

	g_fiti.pin_state_inactive =
		pinctrl_lookup_state(g_fiti.pctrl, "inactive");
	if (IS_ERR(g_fiti.pin_state_inactive))
		FITI_LOG("fiti pinctrl_lookup_state inactive error!");
}

static void hwtcon_fiti_pinmux_active(void)
{
	FITI_LOG("hwtcon_fiti_pinmux_active!");
	pinctrl_select_state(g_fiti.pctrl, g_fiti.pin_state_active);
}

static void hwtcon_fiti_pinmux_inactive(void)
{
	FITI_LOG("hwtcon_fiti_pinmux_inactive!");
	pinctrl_select_state(g_fiti.pctrl, g_fiti.pin_state_inactive);
}

static void fiti_9929_control_init(bool enable)
{
	int ret;
	unsigned char reg_value = 0x00;

	//ret = fiti_i2c_write(REG_VCOM_SETTING,0x96);

	if (enable) {
		ret = fiti_i2c_read(REG_VCOM_SETTING, &reg_value);

		//ret = fiti_i2c_write(REG_VCOM_SETTING, 0x6c);

		FITI_LOG("I2C read: 0x%x = 0x%x",
			REG_VCOM_SETTING, reg_value);

		ret = fiti_i2c_read(REG_VCOM_SETTING, &reg_value);

		FITI_LOG("I2C read: 0x%x = 0x%x",
			REG_VCOM_SETTING, reg_value);

		mdelay(10);
		gpio_set_value(g_fiti.EPD_PMIC_EN, 0x1);
		mdelay(21);
		gpio_set_value(g_fiti.EPD_EN_TS, 0x1);
	} else {
		gpio_set_value(g_fiti.EPD_EN_TS, 0x0);
		gpio_set_value(g_fiti.EPD_PMIC_EN, 0x0);
	}

}

static void fiti_9930_control_init(bool enable)
{
	if (enable) {
		hwtcon_fiti_pinmux_active();
		if (!g_fiti.version) {
			fiti9930_register_setting();
		}
		edp_vdd_enable();
		gpio_direction_output(g_fiti.EPD_PMIC_EN, 0x1);
		if (!g_fiti.version) {
			gpio_set_value(g_fiti.EPD_EN_TS, 1);
		}
	} else {
		gpio_set_value(g_fiti.EPD_EN_TS, 0);
		gpio_direction_output(g_fiti.EPD_PMIC_EN, 0x0);
		//edp_vdd_disable();
		hwtcon_fiti_pinmux_inactive();
	}
}

bool fiti_pmic_judge_power_on_going(void)
{
	return g_fiti.current_state == POWER_ON_GOING;
}
EXPORT_SYMBOL(fiti_pmic_judge_power_on_going);

static void fiti_pmic_config_pmic(	struct work_struct *work_item)
{
	enum FITI_POWER_STATE_ENUM target_state;
	enum FITI_POWER_STATE_ENUM current_state;
	unsigned long flags;

	spin_lock_irqsave(&g_fiti.power_state_lock, flags);
	target_state = g_fiti.target_state;
	current_state = g_fiti.current_state;
	spin_unlock_irqrestore(&g_fiti.power_state_lock, flags);

	if (target_state == POWER_ON) {
		switch (current_state) {
		case POWER_ON:
		case POWER_ON_GOING:
			/* Do Nothing*/
			return;
		case POWER_OFF:
			/* power on fiti */
			if (g_fiti.fiti_id == FITI_9929) {
				FITI_LOG("power on fiti 9929");
				fiti_9929_control_init(true);
			} else if (g_fiti.fiti_id == FITI_9930) {
				FITI_LOG("power on fiti 9930");
				fiti_9930_control_init(true);
			}
			spin_lock_irqsave(&g_fiti.power_state_lock, flags);
			g_fiti.current_state = POWER_ON_GOING;
			spin_unlock_irqrestore(&g_fiti.power_state_lock, flags);
			break;
		default:
			FITI_ERR("invalid current state:%d", current_state);
			break;
		}
		return;
	} else if (target_state == POWER_OFF) {
		switch (current_state) {
		case POWER_ON:
			/* power off fiti */
			if (g_fiti.fiti_id == FITI_9929) {
				FITI_LOG("power off fiti 9929");
				fiti_9929_control_init(false);
			} else if (g_fiti.fiti_id == FITI_9930) {
				FITI_LOG("power off fiti 9930");
				fiti_9930_control_init(false);
			}
			spin_lock_irqsave(&g_fiti.power_state_lock, flags);
			g_fiti.current_state = POWER_OFF;
			spin_unlock_irqrestore(&g_fiti.power_state_lock, flags);
			break;
		case POWER_ON_GOING: /* power good irq will queue this work again */
		case POWER_OFF:
			/* Do nothing */
			break;
		default:
			FITI_ERR("invalid current state:%d", current_state);
			break;
		}
		return;
	} else {
		FITI_ERR("invalid target state:%d", target_state);
		return;
	}
}


static int fiti_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct device_node *np = client->dev.of_node;
	int ret = 0;
	int status = 0;
	int gpio_power_good;
	int debtime;
	unsigned long flags;

	if (!np)
		return -ENODEV;

	fiti_client = client;

	if (client->addr == 0x48) {
		g_fiti.fiti_id = FITI_9929;
		FITI_LOG("fiti 9929");
	} else if (client->addr == 0x18) {
		g_fiti.fiti_id = FITI_9930;
		FITI_LOG("fiti 9930");
	} else {
		FITI_ERR("fiti id error, i2c addr:%x\n", client->addr);
	}

	spin_lock_init(&g_fiti.power_state_lock);
	spin_lock_irqsave(&g_fiti.power_state_lock, flags);
	g_fiti.current_state = POWER_OFF;
	g_fiti.target_state = POWER_OFF;
	spin_unlock_irqrestore(&g_fiti.power_state_lock, flags);

	INIT_WORK(&g_fiti.power_work, fiti_pmic_config_pmic);
	g_fiti.power_workqueue =
		create_singlethread_workqueue("power_fiti");

	g_fiti.EPD_PMIC_EN = of_get_named_gpio(np, "epd-pmic-en", 0);
	if (g_fiti.EPD_PMIC_EN < 0)
		FITI_ERR("[fiti] not find epd-pmic-en\n");
	status = gpio_request(g_fiti.EPD_PMIC_EN, "epd-pmic-en");
	if (status)
		FITI_ERR("gpio_request fail, ret(%d)\n", ret);
	gpio_direction_output(g_fiti.EPD_PMIC_EN, 0);

	g_fiti.EPD_EN_TS = of_get_named_gpio(np, "epd-pmic-op", 0);
	if (g_fiti.EPD_EN_TS < 0)
		FITI_ERR("[fiti] not find epd-pmic-op\n");
	status = gpio_request(g_fiti.EPD_EN_TS, "epd-pmic-op");
	if (status)
		FITI_ERR("gpio_request fail, ret(%d)\n", ret);
	gpio_direction_output(g_fiti.EPD_EN_TS, 0);

	g_fiti.EPD_PMIC_NM_EN = of_get_named_gpio(np, "epd-pmic-nm", 0);
	if (g_fiti.EPD_PMIC_NM_EN < 0)
		FITI_ERR("[fiti] not find epd-pmic-nm\n");
	status = gpio_request(g_fiti.EPD_PMIC_NM_EN, "epd-pmic-nm");
	if (status)
		FITI_ERR("gpio_request fail, ret(%d)\n", ret);
	gpio_direction_output(g_fiti.EPD_PMIC_NM_EN, 0);


	g_fiti.power_good_irq = irq_of_parse_and_map(np, 0);
	FITI_LOG("[accdet]accdet_irq=%d", g_fiti.power_good_irq);


	ret = request_irq(g_fiti.power_good_irq, fiti_power_good_irq,
			IRQ_TYPE_NONE, "fiti-eint", NULL);
	if (ret < 0)
		FITI_ERR("[fiti]EINT IRQ LINE NOT AVAILABLE\n");

	gpio_power_good = of_get_named_gpio(np, "deb-gpios", 0);
	if (gpio_power_good < 0)
		dev_info(&client->dev, "can not get gpio_power_good\n");

	ret = of_property_read_u32(np, "debounce", &debtime);
	if (ret < 0)
		dev_info(&client->dev, "debounce time not found\n");

	if (g_fiti.fiti_id == FITI_9930) {
		of_property_read_u32(np, "vgh_ext",
			&g_fiti.pmic_setting.VGH_EXT);
		of_property_read_u32(np, "vgl_ext",
			&g_fiti.pmic_setting.VGL_EXT);
		of_property_read_u32(np, "xon_len",
			&g_fiti.pmic_setting.XON_LEN);
		of_property_read_u32(np, "xon_delay",
			&g_fiti.pmic_setting.XON_DELAY);
		of_property_read_u32(np, "vgh",
			&g_fiti.pmic_setting.VGH);
		of_property_read_u32(np, "vgl",
			&g_fiti.pmic_setting.VGL);
		of_property_read_u32(np, "vcom_setting",
			&g_fiti.pmic_setting.VCOM_SETTING);
		of_property_read_u32(np, "vghnm_setting",
			&g_fiti.pmic_setting.VGHNM_SETTING);
		of_property_read_u32(np, "vghnm_ext",
			&g_fiti.pmic_setting.VGHNM_EXT);
		of_property_read_u32(np, "vpos",
			&g_fiti.pmic_setting.VPOS);
		of_property_read_u32(np, "vneg",
			&g_fiti.pmic_setting.VNEG);

		#ifdef CONFIG_IDME /*overwrite g_fiti.pmic_setting.VCOM_SETTING with idme_vcom_value*/
		g_fiti.pmic_setting.VCOM_SETTING = (idme_vcom_value != 0) ? idme_vcom_value : vcom_default_value;
		if(g_fiti.pmic_setting.VCOM_SETTING < 0)
			g_fiti.pmic_setting.VCOM_SETTING = -g_fiti.pmic_setting.VCOM_SETTING;
		#endif

		hwtcon_fiti_pinmux_control(&client->dev);

		g_fiti.reg_edp = regulator_get(&client->dev, "epd");

		if (g_fiti.reg_edp == NULL)
			FITI_ERR("reg_edp parse fail");

		FITI_LOG("vgh_ext:%d, vgl_ext:%d",
			g_fiti.pmic_setting.VGH_EXT,
			g_fiti.pmic_setting.VGL_EXT);

		FITI_LOG("xon_len:%d, xon_delay:%d, vgh:%d",
			g_fiti.pmic_setting.XON_LEN,
			g_fiti.pmic_setting.XON_DELAY,
			g_fiti.pmic_setting.VGH);

		FITI_LOG("vgl:%d, vcom:%d, vghnm_setting:%d",
			g_fiti.pmic_setting.VGL,
			g_fiti.pmic_setting.VCOM_SETTING,
			g_fiti.pmic_setting.VGHNM_SETTING);

		FITI_LOG("vghnm_ext:%d, vpos:%d",
			g_fiti.pmic_setting.VGHNM_EXT,
			g_fiti.pmic_setting.VPOS);
		fiti9930_register_setting();
	}

	g_fiti.version = 0;
	
#ifdef CONFIG_IDME
	if (idme_hwid_value > 0)
		g_fiti.version = 1;
#endif

	g_fiti.power_off_status = true;

	dev_info(&client->dev, "PMIC fiti for eInk display\n");

	init_waitqueue_head(&g_fiti_power_OK_wait_queue);

	return ret;

}

void fiti_power_enable(bool enable)
{
#ifndef FPGA_EARLY_PORTING
	unsigned long flags;

	spin_lock_irqsave(&g_fiti.power_state_lock, flags);
	g_fiti.target_state = enable ? POWER_ON : POWER_OFF;
	spin_unlock_irqrestore(&g_fiti.power_state_lock, flags);
	/* fiti_pmic_config_pmic */
	queue_work(g_fiti.power_workqueue, &g_fiti.power_work);
#endif
}
EXPORT_SYMBOL(fiti_power_enable);


void fiti_wait_power_good(void)
{
#ifndef FPGA_EARLY_PORTING
	int status = 0;

	/* wait for pmic enable OK */
	status = wait_event_timeout(
		g_fiti_power_OK_wait_queue,
		g_fiti.power_good_status,
		msecs_to_jiffies(FITI_ENABLE_TIMEOUT_MS));
	/* wait timeout */
	if (status == 0) {
		FITI_ERR("enable fiti:%d timeout currnt:%d target:%d",
			g_fiti.fiti_id,
			g_fiti.current_state,
			g_fiti.target_state);
		WARN_ON(1);
	}
#endif
}

EXPORT_SYMBOL(fiti_wait_power_good);

static int fiti_remove(struct i2c_client *i2c)
{

	return 0;
}

static const struct i2c_device_id fiti_id[] = {
	{"fiti", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, fiti_id);

static const struct of_device_id fiti_dt_ids[] = {
	{
	 .compatible = "fiti,pmic",
	 },
};

MODULE_DEVICE_TABLE(of, fiti_dt_ids);


static struct i2c_driver fiti_driver = {
	.driver = {
		   .name = "fiti",
		   .owner = THIS_MODULE,
		   .of_match_table = fiti_dt_ids,
		   },
	.probe = fiti_probe,
	.remove = fiti_remove,
	.id_table = fiti_id,
};

static int __init fiti_init(void)
{
	return i2c_add_driver(&fiti_driver);
}

static void __exit fiti_exit(void)
{
	i2c_del_driver(&fiti_driver);
}

/*
 * Module entry points
 */
late_initcall(fiti_init);
module_exit(fiti_exit);
MODULE_DESCRIPTION("fiti core driver");
MODULE_LICENSE("GPL");
