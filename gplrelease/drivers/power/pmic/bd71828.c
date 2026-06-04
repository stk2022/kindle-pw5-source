// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 * Author: Hsin-Hsiung Wang <hsin-hsiung.wang@mediatek.com>
 */

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <i2c.h>
#include <power/pmic.h>
#include <power/bd71828.h>

#define BD71828_BUS_NUM 1
#define BD71828_SLV_ADDR 0x4b

struct udevice *i2c_dev __attribute__((section(".data"))) = NULL;
struct udevice *bd71828_dev __attribute__((section(".data"))) = NULL;
static int dev_init = -1;

static int bd71828_init_i2c(void)
{
	struct udevice *bus;
	int ret;

	ret = uclass_get_device_by_seq(UCLASS_I2C, BD71828_BUS_NUM, &bus);
	if (ret) {
		pr_notice("%s: No bus\n", __func__);
		return 0;
	}

	ret = dm_i2c_probe(bus, BD71828_SLV_ADDR, 0, &bd71828_dev);
	if (ret) {
		pr_notice("%s: probe fail\n", __func__);
		return 0;
	}

	i2c_dev = bd71828_dev;
	return 1;
}

#define BD71828_REG_VBAT_U    0x84
#define BD71828_REG_BAT_STAT  0x67
#define BD71828_REG_DCIN_STAT 0x68
#define BD71828_REG_CHG_EN    0x6F
#define CHARGE_ENABLE_MASK    0X01
#define BD71828_REG_RESERVED2 0xF0

#define CRITICAL_BATTERY_SHOWN (1<<6)

int bd71828_check_vbat(uint16_t *voltage, bool *dcin, bool *shown)
{
	int ret;
	uint8_t *temp = (uint8_t*)voltage;
	uint8_t dcin_reg = 0, bat_reg = 0, chg_en_reg = 0, reserved2_reg = 0;

	if (!i2c_dev && (dev_init < 0))
		dev_init = bd71828_init_i2c();

	if (!dev_init)
		return -ENODEV;

	ret = dm_i2c_read(i2c_dev, BD71828_REG_BAT_STAT, &bat_reg, sizeof(bat_reg));
	if (ret)
		return ret;

	if (!(bat_reg & 0x20)) {
		debug("No battery detected\n");
		return -ENODEV;
	}

	//always enable charging in before check vbat
	ret = dm_i2c_read(i2c_dev, BD71828_REG_CHG_EN, &chg_en_reg, sizeof(chg_en_reg));
	if (ret)
		return ret;
	chg_en_reg |= CHARGE_ENABLE_MASK;
	ret = dm_i2c_write(i2c_dev, BD71828_REG_CHG_EN, &chg_en_reg, sizeof(chg_en_reg));
	if (ret)
		return ret;

	ret = dm_i2c_read(i2c_dev, BD71828_REG_RESERVED2, &reserved2_reg, sizeof(reserved2_reg));
	if (ret)
		return ret;
	if (shown) {
		if (*shown && !(reserved2_reg & CRITICAL_BATTERY_SHOWN)) {
			reserved2_reg |= CRITICAL_BATTERY_SHOWN;
			ret = dm_i2c_write(i2c_dev, BD71828_REG_RESERVED2, &reserved2_reg, sizeof(reserved2_reg));
			if (ret)
				return ret;
		}
		*shown = !!(reserved2_reg & CRITICAL_BATTERY_SHOWN);
	}

	ret = dm_i2c_read(i2c_dev, BD71828_REG_VBAT_U, temp, sizeof(voltage));
	if (ret)
		return ret;
	if (voltage)
		*voltage = be16_to_cpu(*voltage & 0x1fff);

	ret = dm_i2c_read(i2c_dev, BD71828_REG_DCIN_STAT, &dcin_reg, sizeof(dcin_reg));
	if (ret)
		return ret;
	if (dcin)
		*dcin = !!(dcin_reg & 0x1);

	return 0;
}

int bd71828_reg_read(uint dest_reg, uint mask, uint shift)
{
	u_char read_val;
	int ret;

	if (!i2c_dev && (dev_init < 0))
		dev_init = bd71828_init_i2c();

	if (!dev_init)
		return -1;

	ret = dm_i2c_read(i2c_dev, dest_reg, &read_val, 1);
	if (ret)
		return -1;

	read_val = (read_val >> shift) & mask;

	return read_val;
}

int bd71828_reg_write(uint dest_reg, uint dest_val, uint mask, uint shift)
{
	u_char read_val;
	int ret;

	if (!i2c_dev && (dev_init < 0))
		dev_init = bd71828_init_i2c();

	if (!dev_init)
		return -1;

	ret = dm_i2c_read(i2c_dev, dest_reg, &read_val, 1);
	if (ret)
		return ret;

	read_val &= (~mask);
	read_val |= (dest_val & mask);

	ret = dm_i2c_write(i2c_dev, dest_reg, &read_val, 1);
	if (ret)
		return -1;

	return 0;
}

void bd71828_power_off(void)
{
	bd71828_reg_write(0x4, 2, 0xff, 0);
}

void bd71828_enable_shipping_mode(void)
{
	int chrdet;

	chrdet = bd71828_reg_read(0x68, 0x1, 0);
	if (chrdet < 0)
		return;

	if (chrdet){
		printf("DCIN available, setting shipping mode bit.\n");
		bd71828_reg_write(0x4, 1, 0xff, 0);
	}
	else
		printf("DCIN not available! shipping mode bit NOT set!\n");
}

void bd71828_enable_ldo(uint power_id, uint en)
{
	uint en_reg;

	switch (power_id) {
		case LDO2:
			en_reg = 0x3b;
		break;

		case LDO3:
			en_reg = 0x3d;
		break;

		case LDO4:
			en_reg = 0x3f;
		break;

		case LDO6:
			en_reg = 0x44;
		break;

		case LDO_SNVS:
			en_reg = 0x45;
		break;

		default:
			return;
	}

	if (en)
		bd71828_reg_write(en_reg, 0x1 << 3, 0xF, 0);
	else
		bd71828_reg_write(en_reg, 0, 0xF, 0);
}

void bd71828_set_gpio_epden(uint en)
{
	bd71828_reg_write(0x48, en ? 1 : 0, 0x1, 0);
}

uint bd71828_get_gpio_epden(void)
{
	int reg_val = 0;

	reg_val = bd71828_reg_read(0x48, 0x1, 0);
	if (reg_val)
		return 1;

	return 0;
}

static int bd71828_probe(struct udevice *dev)
{
	ulong slv_addr = dev_get_driver_data(dev);

	pr_notice("[%s] slave addr 0x%x\n", dev->name, slv_addr);
	if (slv_addr == BD71828_SLV_ADDR && !bd71828_dev)
		bd71828_dev = dev;

	return 0;
};

static const struct udevice_id bd71828_ids[] = {
	{ .compatible = "rohm,bd71828", .data = BD71828_SLV_ADDR },
	{ }
};

U_BOOT_DRIVER(pmic_bd71828) = {
	.name = "bd71828 pmic",
	.id = UCLASS_PMIC,
	.of_match = bd71828_ids,
	.probe = bd71828_probe,
};
