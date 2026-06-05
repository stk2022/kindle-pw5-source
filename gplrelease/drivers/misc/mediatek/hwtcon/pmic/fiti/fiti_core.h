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


#ifndef __FITI_CORE_H_
#define __FITI_CORE_H_

#include <linux/regulator/consumer.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/consumer.h>


//#define PMIC_FPGA	1

#define VGH_EXT_STEP		50
#define VGL_EXT_STEP		50
#define XON_LEN_STEP		10
#define XON_DELAY_STEP		5
#define VCOM_SETTING_STEP	(5000/255)
#define VGHNM_SETTING_VALID	5
#define VGHNM_EXT_STEP		50
#define XON_DELAY_MAX		2500



enum {
	FITI_9929 = 0x00,
	FITI_9930 = 0x01,
};

struct fiti_pmic_setting {
	int VGH_EXT;
	int VGL_EXT;
	int XON_LEN;
	int XON_DELAY;
	int VGH;
	int VGL;
	int VCOM_SETTING;
	int VGHNM_SETTING;
	int VGHNM_EXT;
	int VPOS;
	int VNEG;
};

struct fiti_context {
	int EPD_PMIC_EN;
	int EPD_EN_TS;
	int EPD_PMIC_NM_EN;
	int fiti_id;
	bool power_good_status;
	bool power_off_status;
	int power_good_irq;
	struct fiti_pmic_setting pmic_setting;
	struct regulator *reg_edp;
	struct pinctrl *pctrl;
	struct pinctrl_state *pin_state_active;
	struct pinctrl_state *pin_state_inactive;
	int version;
};

enum {
	REG_TMST_VALUE = 0x00,
	REG_FUNC_ADJUST = 0x01,
	REG_VCOM_SETTING = 0x02,
	REG_VDDH_EXT = 0x0A,
	REG_VEE_EXT = 0x0B,
	REG_VPDD_LEN = 0x0C,
	REG_VPDD = 0x0D,
	REG_NM = 0x10,
};

enum {
	FITI9930_TMST_VALUE = 0x00,
	FITI9930_VCOM_SETTING = 0x01,
	FITI9930_VPOS_VNEG_SETTING = 0x02,
	FITI9930_PWRON_DELAY = 0x03,
	FITI9930_VGH_EXT = 0x04,
	FITI9930_VGL_EXT = 0x05,
	FITI9930_VGHNM_EXT = 0x06,
	FITI9930_VGHNM_SETTING = 0x07,
	FITI9930_DISA_DELAY = 0x08,
	FITI9930_XON_DELAY = 0x09,
	FITI9930_XON_LEN = 0x0A,
	FITI9930_CONTROL_REG1 = 0x0B,
	FITI9930_CONTROL_REG2 = 0x0C,
	FITI9930_REG_NUM
};

void fiti_pmic_control_init(bool enable);
int fiti_i2c_write(unsigned char reg, unsigned char writedata);
int fiti_i2c_read(unsigned char reg, unsigned char *rd_buf);
int fiti_read_temperature(void);
void fiti_set_night_mode(bool enable);
void hwtcon_fiti_pinmux_control(struct device *pdev);
void hwtcon_fiti_pinmux_active(void);
void hwtcon_fiti_pinmux_inactive(void);
int fiti_read_vcom(void);
void fiti_setting_get_from_waveform(char *waveform_addr);
void fiti_write_vcom(unsigned int vcom_value);
void fiti_set_version(unsigned int version);
#endif
