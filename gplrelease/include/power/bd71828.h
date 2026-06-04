// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 * Author: Hsin-Hsiung Wang <hsin-hsiung.wang@mediatek.com>
 */

#ifndef __POWER_BD71828_H__
#define __POWER_BD71828_H__

#include <linux/bitops.h>

/* I2C chip address */
#define BD71828_CHIP	0x4B
#define BD71828_BUS	0x1
#define BD71828_NUM_REGS 0xED

/* Registers */
enum {
	BUCK1,
	BUCK2,
	BUCK3,
	BUCK4,
	BUCK5,
	BUCK6,
	BUCK7,
	LDO1,
	LDO2,
	LDO3,
	LDO4,
	LDO5,
	LDO6,
	LDO_SNVS,
};

int bd71828_reg_read(uint dest_reg, uint mask, uint shift);
int bd71828_reg_write(uint dest_reg, uint dest_val, uint mask, uint shift);
void bd71828_power_off(void);
void bd71828_enable_shipping_mode(void);
void bd71828_enable_ldo(uint power_id, uint en);
void bd71828_set_gpio_epden(uint en);
uint bd71828_get_gpio_epden(void);

int bd71828_check_vbat(uint16_t *, bool *, bool *);

#endif	/* __POWER_BD71828_H__ */
