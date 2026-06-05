/*
 * Copyright (c) 2019 MediaTek Inc.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include <mt-plat/rt-regmap.h>
#include <linux/power/rt9426_battery.h>

#define PRECISION_ENHANCE	5

struct rt9426_chip {
	struct i2c_client *i2c;
	struct device *dev;
	struct rt9426_platform_data *pdata;
	struct power_supply *fg_psy;
#ifdef CONFIG_DEBUG_FS
	struct dentry *dir_dentry;
	struct dentry *file_dentries[RT9426FG_DENTRY_NR];
#endif /* CONFIG_DEBUG_FS */
#ifdef CONFIG_RT_REGMAP
	struct rt_regmap_device *regmap;
#endif /* CONFIG_RT_REGMAP */
	struct mutex var_lock;
	int alert_irq;
	int capacity;
	int soc_offset;
	u8 online:1;
	int btemp;
	int bvolt;
	int bcurr;
	u16 ic_ver;
};

static const struct rt9426_platform_data def_platform_data = {
	.dtsi_version = { 0, 0 },
	.offset_interpolation_order = { 2, 2 },
	.soc_offset_size = { 2, 1 },
	.extreg_size = 17,
	.battery_type = 4352,
	.temp_source = 0,
	.volt_source = 0,
	.curr_source = 0,
	.otc_tth = 0x0064,
	.otc_chg_ith = 0x0b5f,
	.otd_tth = 0x0064,
	.otd_dchg_ith = 0x0b5f,
	.curr_db = 0x05,
	.uv_ov_threshold = 0x00ff,
	.us_threshold = 0,
	.design_capacity = 2000,
	.fcc = 2000,
	.op_config = { 0x9480, 0x3241, 0x3eff, 0x2000, 0x2a7f },
	.fc_vth = 0x78,
	.fc_ith = 0x0d,
	.fc_sth = 0x05,
	.fd_vth = 0x91,
};

static int rt9426_read_device(void *client, u32 reg, int len, void *dst)
{
	struct i2c_client *i2c = (struct i2c_client *)client;
	int ret = 0;

	if (len > 1)
		ret = i2c_smbus_read_i2c_block_data(i2c, reg, len, dst);
	else {
		ret = i2c_smbus_read_byte_data(i2c, reg);
		if (ret < 0)
			return ret;
		*(u8 *)dst = (u8)ret;
	}
	return ret;
}

static int rt9426_write_device(void *client, u32 reg, int len, const void *src)
{
	const u8 *data;
	struct i2c_client *i2c = (struct i2c_client *)client;
	int ret = 0;

	if (len > 1)
		ret = i2c_smbus_write_i2c_block_data(i2c, reg, len, src);
	else {
		data = src;
		ret = i2c_smbus_write_byte_data(i2c, reg, *data);
	}
	return ret;
}
static int rt9426_block_read(struct i2c_client *i2c, u8 reg, int len, void *dst)
{
	struct rt9426_chip *chip = i2c_get_clientdata(i2c);
	int ret;

#ifdef CONFIG_RT_REGMAP
	ret = rt_regmap_block_read(chip->regmap, reg, len, dst);
#else
	ret = rt9426_read_device(i2c, reg, len, dst);
#endif /* CONFIG_RT_REGMAP */
	if (ret < 0)
		dev_notice(chip->dev, "rt9426 block read 0x%02x fail\n", reg);
	return ret;
}

static int rt9426_block_write(struct i2c_client *i2c,
			      u8 reg, int len, const void *src)
{
	struct rt9426_chip *chip = i2c_get_clientdata(i2c);
	int ret;

#ifdef CONFIG_RT_REGMAP
	ret = rt_regmap_block_write(chip->regmap, reg, len, src);
#else
	ret = rt9426_write_device(i2c, reg, len, src);
#endif /* CONFIG_RT_REGMAP */
	if (ret < 0)
		dev_notice(chip->dev, "rt9426 block write 0x%02x fail\n", reg);
	return ret;
}


static int rt9426_reg_read_word(struct i2c_client *i2c, u8 reg)
{
	u16 data = 0;
	int ret;

	ret = rt9426_block_read(i2c, reg, 2, &data);
	return (ret < 0) ? ret : (s32)le16_to_cpu(data);
}

static int rt9426_reg_write_word(struct i2c_client *i2c, u8 reg, u16 data)
{
	data = cpu_to_le16(data);
	return rt9426_block_write(i2c, reg, 2, (uint8_t *)&data);
}

static int __maybe_unused rt9426_reg_write_word_with_check
		(struct rt9426_chip *chip, u8 reg, u16 data)
{
	int retry_times = 2, r_data = 0;

	while (retry_times) {
		rt9426_reg_write_word(chip->i2c, reg, data);
		rt9426_reg_write_word(chip->i2c, RT9426_REG_DUMMY, 0x0000);
		mdelay(5);
		r_data = rt9426_reg_read_word(chip->i2c, reg);
		if (data == r_data) {
			dev_info(chip->dev,
				 "TWrite REG_0x%.2x Successful\n", reg);
			break;
		}
		retry_times--;
		if (retry_times == 0)
			dev_notice(chip->dev, "Write REG_0x%.2x fail\n", reg);
	}
	return r_data;
}

#ifdef CONFIG_RT_REGMAP
RT_REG_DECL(RT9426_REG_CNTL, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_RSVD_Flag, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_CURR, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_TEMP, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_VBAT, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_FLAG1, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_FLAG2, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_RM, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_FCC, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_AI, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_TTE, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_DUMMY, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_VER, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_VGCOMP12, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_VGCOMP34, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_INTT, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_CYC, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_SOC, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_SOH, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_FLAG3, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_IRQ, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_DSNCAP, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_EXTREGCNTL, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_SWINDOW1, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_SWINDOW2, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_SWINDOW3, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_SWINDOW4, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_SWINDOW5, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_SWINDOW6, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_SWINDOW7, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_SWINDOW8, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_SWINDOW9, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_SWINDOW10, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_SWINDOW11, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_SWINDOW12, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_SWINDOW13, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_SWINDOW14, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_SWINDOW15, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_SWINDOW16, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_OCV, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_AV, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_AT, 2, RT_VOLATILE, {});
RT_REG_DECL(RT9426_REG_UN_FLT_SOC, 2, RT_VOLATILE, {});


static const rt_register_map_t rt9426_chip_regmap[] = {
	RT_REG(RT9426_REG_CNTL),
	RT_REG(RT9426_REG_RSVD_Flag),
	RT_REG(RT9426_REG_CURR),
	RT_REG(RT9426_REG_TEMP),
	RT_REG(RT9426_REG_VBAT),
	RT_REG(RT9426_REG_FLAG1),
	RT_REG(RT9426_REG_FLAG2),
	RT_REG(RT9426_REG_RM),
	RT_REG(RT9426_REG_FCC),
	RT_REG(RT9426_REG_AI),
	RT_REG(RT9426_REG_TTE),
	RT_REG(RT9426_REG_DUMMY),
	RT_REG(RT9426_REG_VER),
	RT_REG(RT9426_REG_VGCOMP12),
	RT_REG(RT9426_REG_VGCOMP34),
	RT_REG(RT9426_REG_INTT),
	RT_REG(RT9426_REG_CYC),
	RT_REG(RT9426_REG_SOC),
	RT_REG(RT9426_REG_SOH),
	RT_REG(RT9426_REG_FLAG3),
	RT_REG(RT9426_REG_IRQ),
	RT_REG(RT9426_REG_DSNCAP),
	RT_REG(RT9426_REG_EXTREGCNTL),
	RT_REG(RT9426_REG_SWINDOW1),
	RT_REG(RT9426_REG_SWINDOW2),
	RT_REG(RT9426_REG_SWINDOW3),
	RT_REG(RT9426_REG_SWINDOW4),
	RT_REG(RT9426_REG_SWINDOW5),
	RT_REG(RT9426_REG_SWINDOW6),
	RT_REG(RT9426_REG_SWINDOW7),
	RT_REG(RT9426_REG_SWINDOW8),
	RT_REG(RT9426_REG_SWINDOW9),
	RT_REG(RT9426_REG_SWINDOW10),
	RT_REG(RT9426_REG_SWINDOW11),
	RT_REG(RT9426_REG_SWINDOW12),
	RT_REG(RT9426_REG_SWINDOW13),
	RT_REG(RT9426_REG_SWINDOW14),
	RT_REG(RT9426_REG_SWINDOW15),
	RT_REG(RT9426_REG_SWINDOW16),
	RT_REG(RT9426_REG_OCV),
	RT_REG(RT9426_REG_AV),
	RT_REG(RT9426_REG_AT),
	RT_REG(RT9426_REG_UN_FLT_SOC),
};

static struct rt_regmap_fops rt9426_regmap_fops = {
	.read_device = rt9426_read_device,
	.write_device = rt9426_write_device,
};

static struct rt_regmap_properties rt9426_regmap_prop = {
	.register_num = ARRAY_SIZE(rt9426_chip_regmap),
	.rm = rt9426_chip_regmap,
	.rt_regmap_mode = RT_MULTI_BYTE,
	.name = "rt9426",
	.aliases = "rt9426",
};

static int rt9426_regmap_init(struct rt9426_chip *chip)
{
	chip->regmap = rt_regmap_device_register(&rt9426_regmap_prop,
			&rt9426_regmap_fops, chip->dev, chip->i2c, chip);
	if (!chip->regmap) {
		dev_notice(chip->dev, "rt9426 rt_regmap register fail\n");
		return -EINVAL;
	}
	return 0;
}
#else
static int rt9426_regmap_init(struct rt9426_chip *chip)
{
	return 0;
}
#endif /* CONFIG_RT_REGMAP */

static void rt9426_read_page_cmd(struct rt9426_chip *chip, uint8_t page)
{
	uint16_t read_page_cmd = 0x6500;

	read_page_cmd += page;
	rt9426_reg_write_word(chip->i2c, RT9426_REG_EXTREGCNTL, read_page_cmd);
	rt9426_reg_write_word(chip->i2c, RT9426_REG_EXTREGCNTL, read_page_cmd);
	mdelay(5);
}

static void rt9426_write_page_cmd(struct rt9426_chip *chip, uint8_t page)
{
	uint16_t write_page_cmd = 0x6550;

	write_page_cmd += page;
	rt9426_reg_write_word(chip->i2c, RT9426_REG_EXTREGCNTL, write_page_cmd);
	rt9426_reg_write_word(chip->i2c, RT9426_REG_EXTREGCNTL, write_page_cmd);
	mdelay(5);
}

static int rt9426_unseal_wi_retry(struct rt9426_chip *chip)
{
	int i, regval, retry_times, ret;

	retry_times = 3;
	for (i = 0; i < retry_times; i++) {
		regval = rt9426_reg_read_word(chip->i2c, RT9426_REG_FLAG3);

		if ((regval & RT9426_UNSEAL_MASK) == RT9426_UNSEAL_STATUS) {
			dev_info(chip->dev, "RT9426 Unseal Pass\n");
			ret = RT9426_UNSEAL_PASS;
			goto out;
		} else {
			dev_info(chip->dev,
				"RT9426 Unseal Fail Cnt = %d\n", i+1);

			if (i >= 2) {
				dev_info(chip->dev,
					"RT9426 Unseal Fail after 3 retries\n");
				ret = RT9426_UNSEAL_FAIL;
				goto out;
			} else if (i > 0) {
				dev_info(chip->dev,
				     "delay 1000ms before next Unseal retry\n");
				mdelay(1000);
			}

			rt9426_reg_write_word(chip->i2c, RT9426_REG_CNTL,
						  (RT9426_Unseal_Key & 0xffff));
			rt9426_reg_write_word(chip->i2c, RT9426_REG_CNTL,
						     (RT9426_Unseal_Key >> 16));
			rt9426_reg_write_word(chip->i2c,
						RT9426_REG_DUMMY, 0x0000);
			mdelay(5);
		}
	}
	ret = RT9426_UNSEAL_FAIL;
out:
	return ret;
}

static void rt9426_hibernate_duty_set(struct rt9426_chip *chip, uint16_t data)
{
	int regval;

	if (rt9426_unseal_wi_retry(chip) == RT9426_UNSEAL_PASS) {
		rt9426_read_page_cmd(chip, RT9426_PAGE_1);
		regval = rt9426_reg_read_word(chip->i2c, RT9426_REG_SWINDOW7);
		regval = ((regval & 0xfff8) | (data & 0x0007));
		rt9426_write_page_cmd(chip, RT9426_PAGE_1);
		rt9426_reg_write_word(chip->i2c, RT9426_REG_SWINDOW7, regval);
		rt9426_reg_write_word(chip->i2c, RT9426_REG_DUMMY, 0x0000);
		mdelay(10);
	}
}

static void rt9426_hibernate_duty_read(struct rt9426_chip *chip)
{
	int regval;

	if (rt9426_unseal_wi_retry(chip) == RT9426_UNSEAL_PASS) {
		rt9426_read_page_cmd(chip, RT9426_PAGE_1);
		regval = rt9426_reg_read_word(chip->i2c, RT9426_REG_SWINDOW7);
		regval = (regval & 0x0007);
		dev_info(chip->dev, "HIBERNATE_DUTTY = 2^%d sec)\n", regval);
	}
}

static void rt9426_enter_hibernate(struct rt9426_chip *chip)
{
	rt9426_reg_write_word(chip->i2c, RT9426_REG_CNTL, 0x74AA);
	rt9426_reg_write_word(chip->i2c, RT9426_REG_DUMMY, 0x0000);
}

static void rt9426_exit_hibernate(struct rt9426_chip *chip)
{
	rt9426_reg_write_word(chip->i2c, RT9426_REG_CNTL, 0x7400);
	rt9426_reg_write_word(chip->i2c, RT9426_REG_DUMMY, 0x0000);
}

static void rt9426_temperature_set(struct rt9426_chip *chip, int data)
{
	dev_info(chip->dev, "%s: temp = %d oC\n", __func__, data);
	rt9426_reg_write_word(chip->i2c, RT9426_REG_TEMP, ((data*10)+2732));
	rt9426_reg_write_word(chip->i2c, RT9426_REG_DUMMY, 0x0000);
}

static void rt9426_reset(struct rt9426_chip *chip)
{
	rt9426_reg_write_word(chip->i2c, RT9426_REG_TEMP, 0x0041);
	rt9426_reg_write_word(chip->i2c, RT9426_REG_DUMMY, 0x0000);
}
static int rt9426_get_avg_vbat(struct rt9426_chip *chip)
{
	int regval = 0;

	regval = rt9426_reg_read_word(chip->i2c, RT9426_REG_AV);
	if (regval < 0)
		return -EIO;
	return regval;
}

static int rt9426_get_volt(struct rt9426_chip *chip)
{
	if (chip->pdata->volt_source)
		chip->bvolt = rt9426_reg_read_word(chip->i2c,
						   chip->pdata->volt_source);
	return chip->bvolt;
}

static int rt9426_get_temp(struct rt9426_chip *chip)
{
	if (chip->pdata->temp_source) {
		chip->btemp = rt9426_reg_read_word(chip->i2c,
						   chip->pdata->temp_source);
		chip->btemp -= 2732;
	}
	return chip->btemp;
}

static unsigned int rt9426_get_cyccnt(struct rt9426_chip *chip)
{
	int ret;
	unsigned int cyccnt = 0;

	ret = rt9426_reg_read_word(chip->i2c, RT9426_REG_CYC);
	if (ret < 0)
		dev_notice(chip->dev, "%s: read cycle count fail\n", __func__);
	else
		cyccnt = ret;
	return cyccnt;
}

static unsigned int rt9426_get_current(struct rt9426_chip *chip)
{
	if (chip->pdata->curr_source) {
		chip->bcurr = rt9426_reg_read_word(chip->i2c,
						   chip->pdata->curr_source);
		if (chip->bcurr < 0)
			return -EIO;
		if (chip->bcurr > 0x7FFF) {
			chip->bcurr = 0x10000 - chip->bcurr;
			chip->bcurr = 0 - chip->bcurr;
		}
	}
	return chip->bcurr;
}

static int rt9426_fg_get_offset(struct rt9426_chip *chip, int soc, int temp);
static int rt9426_fg_get_soc(struct rt9426_chip *chip)
{
	int regval, capacity = 0, btemp;

	regval  = rt9426_reg_read_word(chip->i2c, RT9426_REG_SOC);
	if (regval < 0) {
		dev_notice(chip->dev, "read soc value fail\n");
		return -EIO;
	}
	capacity = (regval * 10);
	dev_dbg(chip->dev, "capacity before offset = %d\n", capacity);
	btemp = rt9426_get_temp(chip);
	dev_dbg(chip->dev, "TEMP = %d\n", btemp);
	chip->soc_offset = rt9426_fg_get_offset(chip, capacity, btemp);
	dev_dbg(chip->dev, "SOC_OFFSET = %d\n", chip->soc_offset);
	capacity += chip->soc_offset;
	dev_dbg(chip->dev, "capacity after offset = %d\n", capacity);
	if (capacity > 0)
		capacity = DIV_ROUND_UP(capacity, 10);
	else
		capacity = 0;
	if (capacity > 100)
		capacity = 100;
	dev_dbg(chip->dev, "SYS_SOC = %d\n", capacity);
	return capacity;
}

static void rt9426_update_info(struct rt9426_chip *chip)
{
	int regval = 0, ret = 0;
	struct power_supply *batt_psy;

	dev_dbg(chip->dev, "%s\n", __func__);

	/* get battery temp from battery power supply */
	batt_psy = power_supply_get_by_name("battery");
	if (!batt_psy) {
		dev_info(chip->dev, "%s: get batt_psy fail\n", __func__);
		goto end_update_info;
	}

	if (rt9426_unseal_wi_retry(chip) == RT9426_UNSEAL_FAIL)
		goto end_update_info;

	rt9426_read_page_cmd(chip, RT9426_PAGE_1);

	dev_dbg(chip->dev, "OPCFG(0x%x 0x%x 0x%x 0x%x 0x%x)\n",
		rt9426_reg_read_word(chip->i2c, RT9426_REG_SWINDOW1),
		rt9426_reg_read_word(chip->i2c, RT9426_REG_SWINDOW2),
		rt9426_reg_read_word(chip->i2c, RT9426_REG_SWINDOW3),
		rt9426_reg_read_word(chip->i2c, RT9426_REG_SWINDOW4),
		rt9426_reg_read_word(chip->i2c, RT9426_REG_SWINDOW5));

	ret = rt9426_reg_read_word(chip->i2c, RT9426_REG_FLAG2);
	rt9426_read_page_cmd(chip, RT9426_PAGE_2);

	regval = rt9426_reg_read_word(chip->i2c, RT9426_REG_SWINDOW1);
	regval = (regval & 0x0300) >> 8;
	if (((ret & 0x0800) >> 11) == 1)
		dev_dbg(chip->dev, "OCV table define by User\n");
	else {
		if (regval == 0)
			dev_dbg(chip->dev, "OCV(4200) Zero_Point(3200)\n");
		else if (regval == 1)
			dev_dbg(chip->dev, "OCV(4350) Zero_Point(3200)\n");
		else if (regval == 2)
			dev_dbg(chip->dev, "OCV(4400) Zero_Point(3200)\n");
		else
			dev_dbg(chip->dev, "OCV(4350) Zero_Point(3400)\n");
	}
	regval = rt9426_reg_read_word(chip->i2c, RT9426_REG_SWINDOW5);
	dev_dbg(chip->dev, "CSCOMP4(%d)\n", regval);
	regval = rt9426_reg_read_word(chip->i2c, RT9426_REG_SWINDOW4);
	dev_dbg(chip->dev, "CSCOMP5(%d)\n", regval);

	dev_dbg(chip->dev, "DSNCAP(%d) FCC(%d)\n",
		rt9426_reg_read_word(chip->i2c, RT9426_REG_DSNCAP),
		rt9426_reg_read_word(chip->i2c, RT9426_REG_FCC));

	dev_dbg(chip->dev,
	  "VOLT_SOURCE(0x%x) INPUT_VOLT(%d) FG_VBAT(%d) FG_OCV(%d) FG_AV(%d)\n",
		chip->pdata->volt_source, rt9426_get_volt(chip),
		rt9426_reg_read_word(chip->i2c, RT9426_REG_VBAT),
		rt9426_reg_read_word(chip->i2c, RT9426_REG_OCV),
		rt9426_reg_read_word(chip->i2c, RT9426_REG_AV));
	dev_dbg(chip->dev,
		"CURR_SOURCE(0x%x) INPUT_CURR(%d) FG_CURR(%d) FG_AI(%d)\n",
		chip->pdata->curr_source, rt9426_get_current(chip),
		rt9426_reg_read_word(chip->i2c, RT9426_REG_CURR),
		rt9426_reg_read_word(chip->i2c, RT9426_REG_AI));
	dev_dbg(chip->dev, "TEMP_SOURCE(0x%x) INPUT_TEMP(%d) FG_TEMP(%d)\n",
			chip->pdata->temp_source, rt9426_get_temp(chip),
			rt9426_reg_read_word(chip->i2c, RT9426_REG_TEMP));
	dev_dbg(chip->dev, "FG_FG_INTT(%d) FG_AT(%d)\n",
		rt9426_reg_read_word(chip->i2c, RT9426_REG_INTT),
		rt9426_reg_read_word(chip->i2c, RT9426_REG_AT));

	regval = rt9426_reg_read_word(chip->i2c, RT9426_REG_FLAG1);
	dev_dbg(chip->dev, "FLAG1(0x%x)\n", regval);
	if (((regval & 0x0200) >> 9) == 1)
		dev_dbg(chip->dev, "FC = 1\n");
	else
		dev_dbg(chip->dev, "FC = 0\n");

	if (((regval & 0x0004) >> 2) == 1)
		dev_dbg(chip->dev, "FD = 1\n");
	else
		dev_dbg(chip->dev, "FD = 0\n");

	regval = rt9426_reg_read_word(chip->i2c, RT9426_REG_FLAG2);
	dev_dbg(chip->dev, "FLAG2(0x%x)\n", regval);

	if (((regval & 0xE000) >> 13) == 0)
		dev_dbg(chip->dev, "Power_Mode (Active)\n");
	else if (((regval & 0xE000) >> 13) == 1)
		dev_dbg(chip->dev, "Power_Mode (FST_RSP_ACT)\n");
	else if (((regval & 0xE000) >> 13) == 2)
		dev_dbg(chip->dev, "Power_Mode (Shutdown)\n");
	else
		dev_dbg(chip->dev, "Power_Mode (Sleep)\n");

	if (((regval & 0x0800) >> 11) == 1)
		dev_dbg(chip->dev, "User_Define_Table (IN USE)\n");
	else
		dev_dbg(chip->dev, "User_Define_Table (NOT IN USE)\n");
	if (((regval & 0x0040) >> 6) == 1)
		dev_dbg(chip->dev, "Battery_Status (Inserted)\n");
	else
		dev_dbg(chip->dev, "Battery_Status (Removed)\n");

	if (((regval & 0x0001)) == 1)
		dev_dbg(chip->dev, "RLX = 1\n");
	else
		dev_dbg(chip->dev, "RLX = 0\n");

	regval = rt9426_reg_read_word(chip->i2c, RT9426_REG_FLAG3);
	dev_dbg(chip->dev, "FLAG3(0x%x)\n", regval);
	if (((regval & 0x0100) >> 8) == 1)
		dev_dbg(chip->dev, "RI = 1\n");
	else
		dev_dbg(chip->dev, "RI = 0\n");

	if (((regval & 0x0001)) == 1)
		dev_dbg(chip->dev, "RT9426 (Unseal)\n");
	else
		dev_dbg(chip->dev, "RT9426 (Seal)\n");

	dev_dbg(chip->dev, "CYCCNT(%d)\n", rt9426_get_cyccnt(chip));

	regval = rt9426_reg_read_word(chip->i2c, RT9426_REG_VGCOMP12);
	dev_dbg(chip->dev, "VGCOMP12(0x%x)\n", regval);
	regval = rt9426_reg_read_word(chip->i2c, RT9426_REG_VGCOMP34);
	dev_dbg(chip->dev, "VGCOMP34(0x%x)\n", regval);

	regval = rt9426_reg_read_word(chip->i2c, RT9426_REG_FCC);
	dev_dbg(chip->dev, "FCC(%d)\n", regval);
	regval = rt9426_reg_read_word(chip->i2c, RT9426_REG_DSNCAP);
	dev_dbg(chip->dev, "DSNCAP(%d)\n", regval);

	regval = rt9426_reg_read_word(chip->i2c, RT9426_REG_UN_FLT_SOC);
	dev_dbg(chip->dev, "UNFSOC(%d)\n", regval);
	chip->capacity = rt9426_fg_get_soc(chip);

	ret = rt9426_reg_read_word(chip->i2c, RT9426_REG_FCC);
	regval = chip->capacity;
	regval = regval * ret;
	regval = DIV_ROUND_UP(regval, 100);
	dev_dbg(chip->dev, "RM(%d)\n", regval);

	ret = rt9426_reg_read_word(chip->i2c, RT9426_REG_FCC);
	regval = rt9426_reg_read_word(chip->i2c, RT9426_REG_DSNCAP);
	ret = ret * 100;
	regval = DIV_ROUND_UP(ret, regval);
	dev_dbg(chip->dev, "SOH(%d)\n", regval);
	power_supply_changed(batt_psy);
	power_supply_put(batt_psy);
end_update_info:
	return;
}

static int rt9426_apply_pdata(struct rt9426_chip *);
static irqreturn_t rt9426_irq_handler(int irqno, void *param)
{
	struct rt9426_chip *chip = (struct rt9426_chip *)param;
	int i, regval, retry_times;

	dev_dbg(chip->dev, "%s\n", __func__);
	dev_dbg(chip->dev,
		"FG_IRQ(0x%x) FG_FLAG1(0x%x) FG_FLAG2(0x%x) FG_FLAG3(0x%x)\n",
		rt9426_reg_read_word(chip->i2c, RT9426_REG_IRQ),
		rt9426_reg_read_word(chip->i2c, RT9426_REG_FLAG1),
		rt9426_reg_read_word(chip->i2c, RT9426_REG_FLAG2),
		rt9426_reg_read_word(chip->i2c, RT9426_REG_FLAG3));

	retry_times = 30;
	for (i = 0 ; i < retry_times ; i++) {
		regval = rt9426_reg_read_word(chip->i2c, RT9426_REG_FLAG2);
		if (regval & RT9426_RDY_MASK) {
			regval = rt9426_reg_read_word(chip->i2c,
							RT9426_REG_FLAG3);
			if (!(regval & RT9426_RI_MASK)) {
				dev_info(chip->dev,
					"RI=0, bypass initial phase\n");
				goto break_for_loop;
			} else {
				dev_info(chip->dev,
				    "RI=1, delay 60ms before initialization\n");
				mdelay(60);
				rt9426_apply_pdata(chip);
				goto break_for_loop;
			}
		}
		mdelay(10);
	}

break_for_loop:
	regval = rt9426_reg_read_word(chip->i2c, RT9426_REG_FLAG2);
	if (!(regval & RT9426_BATPRES_MASK)) {
		dev_dbg(chip->dev, "battery remove detected\n");
		regval = rt9426_reg_read_word(chip->i2c, RT9426_REG_RSVD_Flag);
		regval = (regval & 0x7FFF);
		rt9426_reg_write_word(chip->i2c, RT9426_REG_RSVD_Flag, regval);
		rt9426_reg_write_word(chip->i2c, RT9426_REG_DUMMY, 0x0000);
	}
	regval = rt9426_reg_read_word(chip->i2c, RT9426_REG_RSVD_Flag);
	dev_dbg(chip->dev, "FG_RSVD_Flag(0x%x)\n", regval);
	rt9426_update_info(chip);
	return IRQ_HANDLED;
}

enum comp_offset_typs {
	FG_COMP = 0,
	SOC_OFFSET,
	EXTREG_UPDATE,
};

static void new_vgcomp_soc_offset_datas(struct device *dev, int type,
					struct rt9426_platform_data *pdata,
					int size_x, int size_y, int size_z)
{
	switch (type) {
	case SOC_OFFSET:
		if (pdata->soc_offset.soc_offset_data) {
			devm_kfree(dev,
				pdata->soc_offset.soc_offset_data);
			pdata->soc_offset.soc_offset_data = NULL;
		}
		if (size_x != 0 && size_y != 0)
			pdata->soc_offset.soc_offset_data =
				devm_kzalloc(dev,
				    size_x * size_y * sizeof(struct data_point),
				    GFP_KERNEL);
		if (pdata->soc_offset.soc_offset_data) {
			pdata->soc_offset.soc_voltnr = size_x;
			pdata->soc_offset.tempnr = size_y;

		} else {
			pdata->soc_offset.soc_voltnr = 0;
			pdata->soc_offset.tempnr = 0;
		}
		break;
	case EXTREG_UPDATE:
		if (pdata->extreg_update.extreg_update_data) {
			devm_kfree(dev,
				pdata->extreg_update.extreg_update_data);
			pdata->extreg_update.extreg_update_data = NULL;
		}
		if (size_x != 0)
			pdata->extreg_update.extreg_update_data =
				devm_kzalloc(dev,
				      size_x * sizeof(struct extreg_data_point),
				      GFP_KERNEL);
		break;
	default:
		WARN_ON(1);
	}
}

struct submask_condition {
	int x, y, z;
	int order_x, order_y, order_z;
	int xnr, ynr, znr;
	const struct data_point *mesh_src;
};

static inline const struct data_point *get_mesh_data(
	int i, int j, int k,
	const struct data_point *mesh, int xnr, int ynr)
{
	return mesh + k * ynr * xnr + j * xnr + i;
}

static int get_sub_mesh(int state, struct data_point *mesh_buffer,
				struct submask_condition *condition)
{
	int i = 0, j = 0, k = 0, x = 0, y = 0, z = 0;

	x = condition->x;
	y = condition->y;
	z = condition->z;
	for (i = 0; i < condition->xnr; ++i) {
		if (get_mesh_data(i, 0, 0, condition->mesh_src,
					condition->xnr, condition->ynr)->x >= x)
			break;
	}
	for (; i >= 0 && i < condition->xnr; --i) {
		if (get_mesh_data(i, 0, 0, condition->mesh_src,
					condition->xnr, condition->ynr)->x <= x)
			break;
	}

	for (j = 0; j < condition->ynr; ++j) {
		if (get_mesh_data(0, j, 0, condition->mesh_src,
					condition->xnr, condition->ynr)->y >= y)
			break;
	}
	for (; j >= 0 && j < condition->ynr; --j) {
		if (get_mesh_data(0, j, 0, condition->mesh_src,
					condition->xnr, condition->ynr)->y <= y)
			break;
	}

	if (state == FG_COMP) {
		for (k = 0; k < condition->znr; ++k) {
			if (get_mesh_data(0, 0, k, condition->mesh_src,
					condition->xnr, condition->ynr)->z >= z)
				break;
		}
		for (; k >= 0 && k < condition->znr; --k) {
			if (get_mesh_data(0, 0, k, condition->mesh_src,
					condition->xnr, condition->ynr)->z <= z)
				break;
		}
	}

	i -= ((condition->order_x - 1) / 2);
	j -= ((condition->order_y - 1) / 2);
	k -= ((condition->order_z - 1) / 2);

	if (i <= 0)
		i = 0;
	if (j <= 0)
		j = 0;
	if (k <= 0)
		k = 0;
	if ((i + condition->order_x) > condition->xnr)
		i = condition->xnr - condition->order_x;
	if ((j + condition->order_y) > condition->ynr)
		j = condition->ynr - condition->order_y;
	if ((k + condition->order_z) > condition->znr)
		k = condition->znr - condition->order_z;

	if (state == FG_COMP) {
		for (z = 0; z < condition->order_z; ++z) {
			for (y = 0; y < condition->order_y; ++y) {
				for (x = 0; x < condition->order_x; ++x) {
					*(mesh_buffer + z * condition->order_y *
							condition->order_z +
						y * condition->order_x + x)
						= *get_mesh_data(i + x, j + y,
							k + z,
							condition->mesh_src,
							condition->xnr,
							condition->ynr);
				}
			}
		}
	} else {
		for (y = 0; y < condition->order_y; ++y) {
			for (x = 0; x < condition->order_x; ++x) {
				*(mesh_buffer + y * condition->order_x + x)
					= *get_mesh_data(i + x, j + y, 0,
						condition->mesh_src,
						condition->xnr,
						condition->ynr);
			}
		}
	}
	return 0;
}

static int offset_li(int xnr, int ynr,
			const struct data_point *mesh, int x, int y)
{
	long long retval = 0;
	int i, j, k;
	long long wm, wd;
	const struct data_point *cache;

	for (i = 0; i < xnr; ++i) {
		for (j = 0; j < ynr; ++j) {
			wm = wd = 1;
			cache = get_mesh_data(i, j, 0, mesh, xnr, ynr);
			for (k = 0; k < xnr; ++k) {
				if (i != k) {
					wm *= (x - get_mesh_data(k, j, 0,
							mesh, xnr, ynr)->x);
					wd *= (cache->x - get_mesh_data(k, j, 0,
							mesh, xnr, ynr)->x);
				}
			}
			for (k = 0; k < ynr; ++k) {
				if (j != k) {
					wm *= (y - get_mesh_data(i, k, 0,
							mesh, xnr, ynr)->y);
					wd *= (cache->y - get_mesh_data(i, k, 0,
							mesh, xnr, ynr)->y);
				}
			}
			retval += div64_s64(
				((cache->w * wm) << PRECISION_ENHANCE), wd);
		}
	}
	return (int)((retval + (1 << (PRECISION_ENHANCE - 1)))
							>> PRECISION_ENHANCE);
}

static int rt9426_fg_get_offset(struct rt9426_chip *chip, int soc_val, int temp)
{
	const int ip_x = chip->pdata->offset_interpolation_order[0];
	const int ip_y = chip->pdata->offset_interpolation_order[1];
	struct data_point sub_mesh[ip_x * ip_y];
	int xnr, ynr;
	int offset;
	struct soc_offset_table *offset_table = NULL;
	struct submask_condition condition = {
		.x = soc_val,
		.y = temp,
	};

	mutex_lock(&chip->var_lock);
	offset_table = &chip->pdata->soc_offset;
	xnr = offset_table->soc_voltnr;
	ynr = offset_table->tempnr;
	if (xnr == 0 || ynr == 0) {
		mutex_unlock(&chip->var_lock);
		return 0;
	}
	condition.order_x = min(ip_x, xnr);
	condition.order_y = min(ip_y, ynr);
	condition.xnr = xnr;
	condition.ynr = ynr;
	condition.mesh_src = offset_table->soc_offset_data;
	get_sub_mesh(SOC_OFFSET, sub_mesh, &condition);
	offset = offset_li(condition.order_x, condition.order_y, sub_mesh,
			   soc_val, temp);
	mutex_unlock(&chip->var_lock);
	return offset;
}

static int rt_fg_get_property(struct power_supply *psy,
			      enum power_supply_property psp,
			      union power_supply_propval *val)
{
	struct rt9426_chip *chip = power_supply_get_drvdata(psy);
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = chip->online;
		dev_info(chip->dev, "psp_online = %d\n", val->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = rt9426_get_avg_vbat(chip);
		dev_info(chip->dev, "psp_volt_now = %d\n", val->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		val->intval = chip->pdata->battery_type;
		dev_info(chip->dev, "psp_volt_max_design = %d\n", val->intval);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		chip->capacity = rt9426_fg_get_soc(chip);
		val->intval = chip->capacity;
		dev_info(chip->dev, "psp_capacity = %d\n", val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = chip->pdata->design_capacity;
		dev_info(chip->dev,
			"psp_charge_full_design = %d\n", val->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = rt9426_get_current(chip);
		dev_info(chip->dev, "psp_curr_now = %d\n", val->intval);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = rt9426_get_temp(chip);
		val->intval = 25; /* wait bts driver */
		dev_info(chip->dev, "psp_temp = %d\n", val->intval);
		break;
	default:
		rc = -EINVAL;
		break;
	}
	return rc;
}

static int rt_fg_set_property(struct power_supply *psy,
			      enum power_supply_property psp,
			      const union power_supply_propval *val)
{
	int rc = 0;

	switch (psp) {
	default:
		rc = -EINVAL;
		break;
	}
	return rc;
}

static enum power_supply_property rt_fg_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CAPACITY,
};

static struct power_supply_desc fg_psy_desc = {
	.name = "rt-fuelgauge",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = rt_fg_props,
	.num_properties = ARRAY_SIZE(rt_fg_props),
	.get_property = rt_fg_get_property,
	.set_property = rt_fg_set_property,
};

static int get_parameters(char *buf, long int *param, int num_of_par)
{
	char *token;
	int base, cnt;

	token = strsep(&buf, " ");

	for (cnt = 0; cnt < num_of_par; cnt++) {
		if (token != NULL) {
			if ((token[1] == 'x') || (token[1] == 'X'))
				base = 16;
			else
				base = 10;

			if (kstrtol(token, base, &param[cnt]) != 0)
				return -EINVAL;

			token = strsep(&buf, " ");
		} else
			return -EINVAL;
	}
	return 0;
}

#ifdef CONFIG_DEBUG_FS
static int dentry_id_to_comp_offset_type[] = {
	[RT9426FG_SOC_OFFSET_SIZE] = SOC_OFFSET,
	[RT9426FG_SOC_OFFSET_DATA] = SOC_OFFSET,
	[RT9426FG_PARAM_LOCK] = -1, /* dummy */
	[RT9426FG_OFFSET_IP_ORDER] = -1, /* dummy */
	[RT9426FG_FIND_OFFSET_TEST] = -1,
	[RT9426FG_PARAM_CHECK] = -1,
};

struct rt9426_dbg_private_data {
	struct rt9426_chip *chip;
	int id;
	int counter;
	int temp;
	int volt;
	int curr;
	int soc_val;
	int temp2;
};

#define MAX_PARAMS 12

static ssize_t rt9426_fg_debug_write(struct file *filp,
			      const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	struct seq_file *s = filp->private_data;
	struct rt9426_dbg_private_data *prv_data = s->private;
	struct rt9426_dbg_private_data *prv_data_header;
	struct rt9426_chip *chip = prv_data->chip;
	struct data_point *data;
	char lbuf[128];
	int rc;
	int index;
	int comp_offset_type;
	long int param[MAX_PARAMS];

	if (cnt > sizeof(lbuf) - 1)
		return -EINVAL;
	rc = copy_from_user(lbuf, ubuf, cnt);
	if (rc)
		return -EFAULT;
	lbuf[cnt] = '\0';
	comp_offset_type = dentry_id_to_comp_offset_type[prv_data->id];
	prv_data_header = prv_data - prv_data->id;
	switch (prv_data->id) {
	case RT9426FG_SOC_OFFSET_SIZE:
		rc = get_parameters(lbuf, param, 2);
		if (rc < 0)
			return rc;
		new_vgcomp_soc_offset_datas(chip->dev, SOC_OFFSET,
					    chip->pdata, param[0], param[1], 0);
		prv_data_header[prv_data->id + 1].counter = 0;
		break;
	case RT9426FG_SOC_OFFSET_DATA:
		index = comp_offset_type - SOC_OFFSET;
		rc = get_parameters(lbuf, param, 3);
		if (rc < 0)
			return rc;
		data = chip->pdata->soc_offset.soc_offset_data +
							prv_data->counter;
		data->voltage = param[0];
		data->temperature = param[1];
		data->offset = param[2];

		prv_data->counter++;
		break;
	case RT9426FG_PARAM_LOCK:
		rc = get_parameters(lbuf, param, 1);
		if (rc < 0)
			return rc;
		if (param[0]) {
			if (prv_data->counter == 0)
				mutex_lock(&chip->var_lock);
			prv_data->counter = 1;
		} else {
			if (prv_data->counter == 1)
				mutex_unlock(&chip->var_lock);
			prv_data->counter = 0;
		}
		break;
	case RT9426FG_OFFSET_IP_ORDER:
		rc = get_parameters(lbuf, param, 2);
		if (rc < 0)
			return rc;
		chip->pdata->offset_interpolation_order[0] = param[0];
		chip->pdata->offset_interpolation_order[1] = param[1];
		break;
	case RT9426FG_FIND_OFFSET_TEST:
		rc = get_parameters(lbuf, param, 2);
		if (rc < 0)
			return rc;
		prv_data->soc_val = param[0];
		prv_data->temp2 = param[1];
		break;
	case RT9426FG_PARAM_CHECK:
		cnt = -ENOTSUPP;
		break;
	default:
		WARN_ON(1);
	}

	return cnt;
}

static void rt9426_platform_data_print(struct seq_file *s,
				       struct rt9426_chip *chip)
{
	int j;

	dev_info(chip->dev, "%s ++\n", __func__);
	seq_printf(s, "dtsi version = <%02d %04d>\n",
			chip->pdata->dtsi_version[0],
			chip->pdata->dtsi_version[1]);
	seq_printf(s, "battery name = %s\n", chip->pdata->bat_name);
	seq_printf(s, "offset interpolation order = <%d %d>\n",
	      chip->pdata->offset_interpolation_order[0],
	      chip->pdata->offset_interpolation_order[1]);
	seq_printf(s, "soc_offset_size = <%d %d>\n",
		chip->pdata->soc_offset.soc_voltnr,
		chip->pdata->soc_offset.tempnr);

	seq_puts(s, "fg_soc_offset_data\n");
	for (j = 0; j < chip->pdata->soc_offset.soc_voltnr *
			chip->pdata->soc_offset.tempnr; j++) {
		seq_printf(s, "<%d %d %d>\n",
			chip->pdata->soc_offset.soc_offset_data[j].x,
			chip->pdata->soc_offset.soc_offset_data[j].y,
			chip->pdata->soc_offset.soc_offset_data[j].offset);
	}

	seq_printf(s, "extreg_size = <%d>\n", chip->pdata->extreg_size);
	if (chip->pdata->extreg_size > 0) {
		seq_puts(s, "fg_extreg_data\n");
		for (j = 0; j < chip->pdata->extreg_size; j++)
			seq_printf(s, "<%d %d %d>\n",
			chip->pdata->
				extreg_update.extreg_update_data[j].extreg_page,
			chip->pdata->
				extreg_update.extreg_update_data[j].extreg_addr,
			chip->pdata->
			       extreg_update.extreg_update_data[j].extreg_data);
	}
	seq_printf(s, "battery_type = <%d>\n", chip->pdata->battery_type);
	seq_printf(s, "otc_tth = 0x%04x\n", chip->pdata->otc_tth);
	seq_printf(s, "otc_chg_ith = 0x%04x\n", chip->pdata->otc_chg_ith);
	seq_printf(s, "otd_tth = 0x%04x\n", chip->pdata->otd_tth);
	seq_printf(s, "otd_dchg_ith = 0x%04x\n", chip->pdata->otd_dchg_ith);
	seq_printf(s, "uv_ov_threshold = 0x%04x\n",
			chip->pdata->uv_ov_threshold);
	seq_printf(s, "us_threshold = 0x%02x\n", chip->pdata->us_threshold);
	seq_printf(s, "design capacity = %d\n", chip->pdata->design_capacity);
	seq_printf(s, "fcc = %d\n", chip->pdata->fcc);
	seq_printf(s, "fc_vth = %d mv\n", (3600+((chip->pdata->fc_vth)*5)));
	seq_printf(s, "fc_ith = %d mA\n", (chip->pdata->fc_ith)*4);
	seq_printf(s, "fc_sth = %d percent\n", chip->pdata->fc_sth);
	seq_printf(s, "fd_vth = %d mv\n", (2500+((chip->pdata->fd_vth)*5)));

	seq_puts(s, "fg_ocv_table\n");
	for (j = 0; j < 10; j++) {
		seq_printf(s, "<0x%04x 0x%04x 0x%04x 0x%04x\n",
			chip->pdata->ocv_table[j].data[0],
			chip->pdata->ocv_table[j].data[1],
			chip->pdata->ocv_table[j].data[2],
			chip->pdata->ocv_table[j].data[3]);
		seq_printf(s, "<0x%04x 0x%04x 0x%04x 0x%04x\n",
			chip->pdata->ocv_table[j].data[4],
			chip->pdata->ocv_table[j].data[5],
			chip->pdata->ocv_table[j].data[6],
			chip->pdata->ocv_table[j].data[7]);
	}

	seq_printf(s, "op_config = <0x%04x 0x%04x 0x%04x 0x%04x 0x%04x>\n",
				chip->pdata->op_config[0],
				chip->pdata->op_config[1],
				chip->pdata->op_config[2],
				chip->pdata->op_config[3],
				chip->pdata->op_config[4]);
	dev_info(chip->dev, "%s --\n", __func__);
}

static int rt9426_fg_debug_read(struct seq_file *s, void *unused)
{
	struct rt9426_dbg_private_data *prv_data = s->private;
	struct rt9426_chip *chip = prv_data->chip;
	struct rt9426_dbg_private_data *prv_data_header;
	struct data_point *data;
	int i = 0, offset = 0;
	int data_size;

	prv_data_header = prv_data - prv_data->id;
	switch (prv_data->id) {
	case RT9426FG_SOC_OFFSET_SIZE:
		seq_printf(s, "%d %d\n", chip->pdata->soc_offset.soc_voltnr,
			   chip->pdata->soc_offset.tempnr);

		break;
	case RT9426FG_SOC_OFFSET_DATA:
		data_size = chip->pdata->soc_offset.soc_voltnr *
				chip->pdata->soc_offset.tempnr;
		if (data_size == 0)
			seq_puts(s, "no data\n");

		data = chip->pdata->soc_offset.soc_offset_data;
		for (i = 0; i < data_size; i++, data++) {
			seq_printf(s, "%d %d %d\n", data->voltage,
				   data->temperature, data->offset);
		}
		break;
	case RT9426FG_PARAM_LOCK:
		seq_printf(s, "%d\n", prv_data->counter);
		break;
	case RT9426FG_OFFSET_IP_ORDER:
		seq_printf(s, "%d %d\n",
			   chip->pdata->offset_interpolation_order[0],
			   chip->pdata->offset_interpolation_order[1]);
		break;
	case RT9426FG_FIND_OFFSET_TEST:
		offset = rt9426_fg_get_offset(chip, prv_data->soc_val,
					      prv_data->temp2);
		seq_printf(s, "<%d %d> : offset = <%d>\n",
			   prv_data->soc_val, prv_data->temp2, offset);
		break;
	case RT9426FG_PARAM_CHECK:
		rt9426_platform_data_print(s, chip);
		break;
	default:
		WARN_ON(1);
	}
	return 0;
}

static int rt9426_fg_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, rt9426_fg_debug_read, inode->i_private);
}

static const struct file_operations rt9426_fg_debug_ops = {
	.open = rt9426_fg_debug_open,
	.write = rt9426_fg_debug_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const char * const dbgfs_names[] = {
	[RT9426FG_SOC_OFFSET_SIZE] = "soc_offset_size",
	[RT9426FG_SOC_OFFSET_DATA] = "soc_offset_data",
	[RT9426FG_PARAM_LOCK] = "param_lock",
	[RT9426FG_OFFSET_IP_ORDER] = "offset_ip_order",
	[RT9426FG_FIND_OFFSET_TEST] = "find_offset_test",
	[RT9426FG_PARAM_CHECK] = "param_check",
};

#define DECL_RT9426FG_PRV_DATA(_id)		\
{						\
	.id = _id,				\
	.counter = 0,				\
}

static struct rt9426_dbg_private_data rt9426_dbg_private_data[] = {
	DECL_RT9426FG_PRV_DATA(RT9426FG_SOC_OFFSET_SIZE),
	DECL_RT9426FG_PRV_DATA(RT9426FG_SOC_OFFSET_DATA),
	DECL_RT9426FG_PRV_DATA(RT9426FG_PARAM_LOCK),
	DECL_RT9426FG_PRV_DATA(RT9426FG_OFFSET_IP_ORDER),
	DECL_RT9426FG_PRV_DATA(RT9426FG_FIND_OFFSET_TEST),
	DECL_RT9426FG_PRV_DATA(RT9426FG_PARAM_CHECK),
};

static void rt9426_fg_create_debug_files(struct rt9426_chip *chip)
{
	int i;

	chip->dir_dentry = debugfs_create_dir("rt9426fg_table", 0);
	if (IS_ERR(chip->dir_dentry)) {
		dev_notice(chip->dev,
			"%s : cannot create rt9426fg_table\n", __func__);
		return;
	}
	WARN_ON(ARRAY_SIZE(dbgfs_names) != ARRAY_SIZE(rt9426_dbg_private_data));
	WARN_ON(ARRAY_SIZE(dbgfs_names) != RT9426FG_DENTRY_NR);
	for (i = 0; i < ARRAY_SIZE(dbgfs_names); ++i) {
		rt9426_dbg_private_data[i].chip = chip;
		chip->file_dentries[i] =
			debugfs_create_file(dbgfs_names[i],
				S_IFREG | 0444,
				chip->dir_dentry,
				&rt9426_dbg_private_data[i],
				&rt9426_fg_debug_ops);
	}
}
#else
static void rt9426_fg_create_debug_files(struct rt9426_chip *chip)
{
	dev_notice(chip->dev, "Not support debugfs\n");
}
#endif /* CONFIG_DEBUG_FS */

static ssize_t rt_fg_show_attrs(struct device *, struct device_attribute *,
				char *);
static ssize_t rt_fg_store_attrs(struct device *, struct device_attribute *,
				 const char *, size_t count);

#define RT_FG_ATTR(_name)				\
{							\
	.attr = {.name = #_name, .mode = 0664},		\
	.show = rt_fg_show_attrs,			\
	.store = rt_fg_store_attrs,			\
}

static struct device_attribute rt_fuelgauge_attrs[] = {
	RT_FG_ATTR(fg_temp),
	RT_FG_ATTR(volt),
	RT_FG_ATTR(curr),
	RT_FG_ATTR(update),
	RT_FG_ATTR(ocv_table),
	RT_FG_ATTR(enter_hibernate),
	RT_FG_ATTR(exit_hibernate),
	RT_FG_ATTR(set_hibernate_duty),
	RT_FG_ATTR(DBP0),
	RT_FG_ATTR(DBP1),
	RT_FG_ATTR(DBP2),
	RT_FG_ATTR(DBP3),
	RT_FG_ATTR(DBP4),
	RT_FG_ATTR(DBP5),
	RT_FG_ATTR(DBP6),
	RT_FG_ATTR(DBP7),
	RT_FG_ATTR(DBP8),
	RT_FG_ATTR(DBP9),
	RT_FG_ATTR(WCNTL),
	RT_FG_ATTR(WEXTCNTL),
	RT_FG_ATTR(WSW1),
	RT_FG_ATTR(WSW2),
	RT_FG_ATTR(WSW3),
	RT_FG_ATTR(WSW4),
	RT_FG_ATTR(WSW5),
	RT_FG_ATTR(WSW6),
	RT_FG_ATTR(WSW7),
	RT_FG_ATTR(WSW8),
	RT_FG_ATTR(WTEMP),
	RT_FG_ATTR(UNSEAL),
	RT_FG_ATTR(FG_SET_TEMP),    /* new ; 2019-09-09 */
	RT_FG_ATTR(FG_RESET),       /* new ; 2019-09-09 */
};

enum {
	FG_TEMP = 0,
	FG_VOLT,
	FG_CURR,
	FG_UPDATE,
	OCV_TABLE,
	ENTER_HIBERNATE,
	EXIT_HIBERNATE,
	SET_HIBERNATE_DUTY,
	DBP0,
	DBP1,
	DBP2,
	DBP3,
	DBP4,
	DBP5,
	DBP6,
	DBP7,
	DBP8,
	DBP9,
	WCNTL,
	WEXTCNTL,
	WSW1,
	WSW2,
	WSW3,
	WSW4,
	WSW5,
	WSW6,
	WSW7,
	WSW8,
	WTEMP,
	UNSEAL,
	FG_SET_TEMP,    /* new ; 2019-09-09 */
	FG_RESET,       /* new ; 2019-09-09 */
};

static ssize_t rt_fg_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rt9426_chip *chip = dev_get_drvdata(dev->parent);
	const ptrdiff_t offset = attr - rt_fuelgauge_attrs;
	int i = 0;

	switch (offset) {
	case FG_TEMP:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", chip->btemp);
		break;
	case FG_VOLT:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", chip->bvolt);
		break;
	case FG_CURR:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", chip->bcurr);
		break;
	case FG_UPDATE:
	case OCV_TABLE:
	case ENTER_HIBERNATE:
	case EXIT_HIBERNATE:
	case SET_HIBERNATE_DUTY:
	case DBP0:
	case DBP1:
	case DBP2:
	case DBP3:
	case DBP4:
	case DBP5:
	case DBP6:
	case DBP7:
	case DBP8:
	case DBP9:
	case WCNTL:
	case WEXTCNTL:
	case WSW1:
	case WSW2:
	case WSW3:
	case WSW4:
	case WSW5:
	case WSW6:
	case WSW7:
	case WSW8:
	case WTEMP:
	case UNSEAL:
	case FG_SET_TEMP:  /* new ; 2019-09-09 */
	case FG_RESET:     /* new ; 2019-09-09 */
	default:
		i = -EINVAL;
		break;
	}
	return i;
}

static ssize_t rt_fg_store_attrs(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct rt9426_chip *chip = dev_get_drvdata(dev->parent);
	const ptrdiff_t offset = attr - rt_fuelgauge_attrs;
	int ret = 0;
	int x, y = 0;
	int temp[8];
	long int val;

	switch (offset) {
	case FG_TEMP:
		ret = get_parameters((char *)buf, &val, 1);
		if (ret < 0) {
			dev_notice(dev, "get paremters fail\n");
			ret = -EINVAL;
		}
		chip->btemp = val;
		ret = count;
		break;
	case FG_VOLT:
		ret = get_parameters((char *)buf, &val, 1);
		if (ret < 0) {
			dev_notice(dev, "get paremters fail\n");
			ret = -EINVAL;
		}
		chip->bvolt = val;
		ret = count;
		break;
	case FG_CURR:
		ret = get_parameters((char *)buf, &val, 1);
		if (ret < 0) {
			dev_notice(dev, "get paremters fail\n");
			ret = -EINVAL;
		}
		chip->bcurr = val;
		ret = count;
		break;
	case FG_UPDATE:
		if (sscanf(buf, "%x\n", &x) == 1 && x == 1) {
			rt9426_update_info(chip);
			ret = count;
		} else
			ret = -EINVAL;
		break;
	case OCV_TABLE:
		if (sscanf(buf, "%x\n", &x) == 1 && x == 1) {
			for (x = 0; x < 10; x++) {
				rt9426_reg_write_word(chip->i2c,
					       RT9426_REG_EXTREGCNTL, 0xCA00+x);
				rt9426_reg_write_word(chip->i2c,
					       RT9426_REG_EXTREGCNTL, 0xCA00+x);
				for (y = 0; y < 8; y++)
					temp[y] =
						rt9426_reg_read_word(chip->i2c,
						   RT9426_REG_SWINDOW1 + y * 2);

				dev_info(dev, "fg_ocv_table_%d\n", x);
				dev_info(dev, "<0x%x 0x%x 0x%x 0x%x>\n",
					temp[0], temp[1], temp[2], temp[3]);
				dev_info(dev, "<0x%x 0x%x 0x%x 0x%x>\n",
					temp[4], temp[5], temp[6], temp[7]);
			}
			ret = count;
		} else
			ret = -EINVAL;
		break;
	case ENTER_HIBERNATE:
		if (sscanf(buf, "%x\n", &x) == 1 && x == 1) {
			rt9426_enter_hibernate(chip);
			dev_info(dev, "SLP_STS = %d\n", rt9426_reg_read_word(
					    chip->i2c, RT9426_REG_FLAG2) >> 15);
			ret = count;
		} else
			ret = -EINVAL;
		break;
	case EXIT_HIBERNATE:
		if (sscanf(buf, "%x\n", &x) == 1 && x == 1) {
			rt9426_exit_hibernate(chip);
			dev_info(dev, "SLP_STS = %d\n", rt9426_reg_read_word(
					    chip->i2c, RT9426_REG_FLAG2) >> 15);
			ret = count;
		} else
			ret = -EINVAL;
		break;
	case SET_HIBERNATE_DUTY:
		ret = get_parameters((char *)buf, &val, 1);
		if (ret < 0) {
			dev_notice(dev, "get paremters fail\n");
			ret = -EINVAL;
		}
		rt9426_hibernate_duty_set(chip, val);
		rt9426_hibernate_duty_read(chip);
		ret = count;
		break;
	case DBP0:
		if (sscanf(buf, "%x\n", &x) == 1 && x == 1) {
			rt9426_read_page_cmd(chip, RT9426_PAGE_0);
			for (x = 0; x < 8; x++)
				temp[x] = rt9426_reg_read_word(chip->i2c,
						   RT9426_REG_SWINDOW1 + x * 2);

			dev_info(dev, "fg_data_block_page0");
			dev_info(dev, "0x41:<0x%x>\n", temp[0]);
			dev_info(dev, "0x43:<0x%x>\n", temp[1]);
			dev_info(dev, "0x45:<0x%x>\n", temp[2]);
			dev_info(dev, "0x47:<0x%x>\n", temp[3]);
			dev_info(dev, "0x49:<0x%x>\n", temp[4]);
			dev_info(dev, "0x4B:<0x%x>\n", temp[5]);
			dev_info(dev, "0x4D:<0x%x>\n", temp[6]);
			dev_info(dev, "0x4F:<0x%x>\n", temp[7]);
			ret = count;
		} else
			ret = -EINVAL;
		break;
	case DBP1:
		if (sscanf(buf, "%x\n", &x) == 1 && x == 1) {
			rt9426_read_page_cmd(chip, RT9426_PAGE_1);
			for (x = 0; x < 8; x++)
				temp[x] = rt9426_reg_read_word(chip->i2c,
						   RT9426_REG_SWINDOW1 + x * 2);

			dev_info(dev, "fg_data_block_page1");
			dev_info(dev, "0x41:<0x%x>\n", temp[0]);
			dev_info(dev, "0x43:<0x%x>\n", temp[1]);
			dev_info(dev, "0x45:<0x%x>\n", temp[2]);
			dev_info(dev, "0x47:<0x%x>\n", temp[3]);
			dev_info(dev, "0x49:<0x%x>\n", temp[4]);
			dev_info(dev, "0x4B:<0x%x>\n", temp[5]);
			dev_info(dev, "0x4D:<0x%x>\n", temp[6]);
			dev_info(dev, "0x4F:<0x%x>\n", temp[7]);
			ret = count;
		} else
			ret = -EINVAL;
		break;
	case DBP2:
		if (sscanf(buf, "%x\n", &x) == 1 && x == 1) {
			rt9426_read_page_cmd(chip, RT9426_PAGE_2);
			for (x = 0; x < 8; x++)
				temp[x] = rt9426_reg_read_word(chip->i2c,
						   RT9426_REG_SWINDOW1 + x * 2);

			dev_info(dev, "fg_data_block_page2");
			dev_info(dev, "0x41:<0x%x>\n", temp[0]);
			dev_info(dev, "0x43:<0x%x>\n", temp[1]);
			dev_info(dev, "0x45:<0x%x>\n", temp[2]);
			dev_info(dev, "0x47:<0x%x>\n", temp[3]);
			dev_info(dev, "0x49:<0x%x>\n", temp[4]);
			dev_info(dev, "0x4B:<0x%x>\n", temp[5]);
			dev_info(dev, "0x4D:<0x%x>\n", temp[6]);
			dev_info(dev, "0x4F:<0x%x>\n", temp[7]);
			ret = count;
		} else
			ret = -EINVAL;
		break;
	case DBP3:
		if (sscanf(buf, "%x\n", &x) == 1 && x == 1) {
			rt9426_read_page_cmd(chip, RT9426_PAGE_3);
			for (x = 0; x < 8; x++)
				temp[x] = rt9426_reg_read_word(chip->i2c,
						   RT9426_REG_SWINDOW1 + x * 2);

			dev_info(dev, "fg_data_block_page3");
			dev_info(dev, "0x41:<0x%x>\n", temp[0]);
			dev_info(dev, "0x43:<0x%x>\n", temp[1]);
			dev_info(dev, "0x45:<0x%x>\n", temp[2]);
			dev_info(dev, "0x47:<0x%x>\n", temp[3]);
			dev_info(dev, "0x49:<0x%x>\n", temp[4]);
			dev_info(dev, "0x4B:<0x%x>\n", temp[5]);
			dev_info(dev, "0x4D:<0x%x>\n", temp[6]);
			dev_info(dev, "0x4F:<0x%x>\n", temp[7]);
			ret = count;
		} else
			ret = -EINVAL;
		break;
	case DBP4:
		if (sscanf(buf, "%x\n", &x) == 1 && x == 1) {
			rt9426_read_page_cmd(chip, RT9426_PAGE_4);
			for (x = 0; x < 8; x++)
				temp[x] = rt9426_reg_read_word(chip->i2c,
						   RT9426_REG_SWINDOW1 + x * 2);

			dev_info(dev, "fg_data_block_page4");
			dev_info(dev, "0x41:<0x%x>\n", temp[0]);
			dev_info(dev, "0x43:<0x%x>\n", temp[1]);
			dev_info(dev, "0x45:<0x%x>\n", temp[2]);
			dev_info(dev, "0x47:<0x%x>\n", temp[3]);
			dev_info(dev, "0x49:<0x%x>\n", temp[4]);
			dev_info(dev, "0x4B:<0x%x>\n", temp[5]);
			dev_info(dev, "0x4D:<0x%x>\n", temp[6]);
			dev_info(dev, "0x4F:<0x%x>\n", temp[7]);
			ret = count;
		} else
			ret = -EINVAL;
		break;
	case DBP5:
		if (sscanf(buf, "%x\n", &x) == 1 && x == 1) {
			rt9426_read_page_cmd(chip, RT9426_PAGE_5);
			for (x = 0; x < 8; x++)
				temp[x] = rt9426_reg_read_word(chip->i2c,
						   RT9426_REG_SWINDOW1 + x * 2);

			dev_info(dev, "fg_data_block_page5");
			dev_info(dev, "0x41:<0x%x>\n", temp[0]);
			dev_info(dev, "0x43:<0x%x>\n", temp[1]);
			dev_info(dev, "0x45:<0x%x>\n", temp[2]);
			dev_info(dev, "0x47:<0x%x>\n", temp[3]);
			dev_info(dev, "0x49:<0x%x>\n", temp[4]);
			dev_info(dev, "0x4B:<0x%x>\n", temp[5]);
			dev_info(dev, "0x4D:<0x%x>\n", temp[6]);
			dev_info(dev, "0x4F:<0x%x>\n", temp[7]);
			ret = count;
		} else
			ret = -EINVAL;
		break;
	case DBP6:
		if (sscanf(buf, "%x\n", &x) == 1 && x == 1) {
			rt9426_read_page_cmd(chip, RT9426_PAGE_6);
			for (x = 0; x < 8; x++)
				temp[x] = rt9426_reg_read_word(chip->i2c,
						   RT9426_REG_SWINDOW1 + x * 2);

			dev_info(dev, "fg_data_block_page6");
			dev_info(dev, "0x41:<0x%x>\n", temp[0]);
			dev_info(dev, "0x43:<0x%x>\n", temp[1]);
			dev_info(dev, "0x45:<0x%x>\n", temp[2]);
			dev_info(dev, "0x47:<0x%x>\n", temp[3]);
			dev_info(dev, "0x49:<0x%x>\n", temp[4]);
			dev_info(dev, "0x4B:<0x%x>\n", temp[5]);
			dev_info(dev, "0x4D:<0x%x>\n", temp[6]);
			dev_info(dev, "0x4F:<0x%x>\n", temp[7]);
			ret = count;
		} else
			ret = -EINVAL;
		break;
	case DBP7:
		if (sscanf(buf, "%x\n", &x) == 1 && x == 1) {
			rt9426_read_page_cmd(chip, RT9426_PAGE_7);
			for (x = 0; x < 8; x++)
				temp[x] = rt9426_reg_read_word(chip->i2c,
						   RT9426_REG_SWINDOW1 + x * 2);

			dev_info(dev, "fg_data_block_page7");
			dev_info(dev, "0x41:<0x%x>\n", temp[0]);
			dev_info(dev, "0x43:<0x%x>\n", temp[1]);
			dev_info(dev, "0x45:<0x%x>\n", temp[2]);
			dev_info(dev, "0x47:<0x%x>\n", temp[3]);
			dev_info(dev, "0x49:<0x%x>\n", temp[4]);
			dev_info(dev, "0x4B:<0x%x>\n", temp[5]);
			dev_info(dev, "0x4D:<0x%x>\n", temp[6]);
			dev_info(dev, "0x4F:<0x%x>\n", temp[7]);
			ret = count;
		} else
			ret = -EINVAL;
		break;
	case DBP8:
		if (sscanf(buf, "%x\n", &x) == 1 && x == 1) {
			rt9426_read_page_cmd(chip, RT9426_PAGE_8);
			for (x = 0; x < 8; x++)
				temp[x] = rt9426_reg_read_word(chip->i2c,
						   RT9426_REG_SWINDOW1 + x * 2);

			dev_info(dev, "fg_data_block_page8");
			dev_info(dev, "0x41:<0x%x>\n", temp[0]);
			dev_info(dev, "0x43:<0x%x>\n", temp[1]);
			dev_info(dev, "0x45:<0x%x>\n", temp[2]);
			dev_info(dev, "0x47:<0x%x>\n", temp[3]);
			dev_info(dev, "0x49:<0x%x>\n", temp[4]);
			dev_info(dev, "0x4B:<0x%x>\n", temp[5]);
			dev_info(dev, "0x4D:<0x%x>\n", temp[6]);
			dev_info(dev, "0x4F:<0x%x>\n", temp[7]);
			ret = count;
		} else
			ret = -EINVAL;
		break;
	case DBP9:
		if (sscanf(buf, "%x\n", &x) == 1 && x == 1) {
			rt9426_read_page_cmd(chip, RT9426_PAGE_9);
			mdelay(5);
			for (x = 0; x < 8; x++)
				temp[x] = rt9426_reg_read_word(chip->i2c,
						     RT9426_REG_SWINDOW1 + x*2);

			dev_info(dev, "fg_data_block_page9");
			dev_info(dev, "0x41:<0x%x>\n", temp[0]);
			dev_info(dev, "0x43:<0x%x>\n", temp[1]);
			dev_info(dev, "0x45:<0x%x>\n", temp[2]);
			dev_info(dev, "0x47:<0x%x>\n", temp[3]);
			dev_info(dev, "0x49:<0x%x>\n", temp[4]);
			dev_info(dev, "0x4B:<0x%x>\n", temp[5]);
			dev_info(dev, "0x4D:<0x%x>\n", temp[6]);
			dev_info(dev, "0x4F:<0x%x>\n", temp[7]);
			ret = count;
		} else
			ret = -EINVAL;
		break;
	case WCNTL:
		ret = get_parameters((char *)buf, &val, 1);
		if (ret < 0) {
			dev_notice(dev, "get paremters fail\n");
			ret = -EINVAL;
		}
		rt9426_reg_write_word(chip->i2c, RT9426_REG_CNTL, val);
		rt9426_reg_write_word(chip->i2c, RT9426_REG_DUMMY, 0x0000);
		ret = count;
		break;
	case WEXTCNTL:
		ret = get_parameters((char *)buf, &val, 1);
		if (ret < 0) {
			dev_notice(dev, "get paremters fail\n");
			ret = -EINVAL;
		}
		rt9426_reg_write_word(chip->i2c, RT9426_REG_EXTREGCNTL, val);
		rt9426_reg_write_word(chip->i2c, RT9426_REG_EXTREGCNTL, val);
		ret = count;
		break;
	case WSW1:
		ret = get_parameters((char *)buf, &val, 1);
		if (ret < 0) {
			dev_notice(dev, "get paremters fail\n");
			ret = -EINVAL;
		}
		rt9426_reg_write_word(chip->i2c, RT9426_REG_SWINDOW1, val);
		rt9426_reg_write_word(chip->i2c, RT9426_REG_DUMMY, 0x0000);
		ret = count;
		break;
	case WSW2:
		ret = get_parameters((char *)buf, &val, 1);
		if (ret < 0) {
			dev_notice(dev, "get paremters fail\n");
			ret = -EINVAL;
		}
		rt9426_reg_write_word(chip->i2c, RT9426_REG_SWINDOW2, val);
		rt9426_reg_write_word(chip->i2c, RT9426_REG_DUMMY, 0x0000);
		ret = count;
		break;
	case WSW3:
		ret = get_parameters((char *)buf, &val, 1);
		if (ret < 0) {
			dev_notice(dev, "get paremters fail\n");
			ret = -EINVAL;
		}
		rt9426_reg_write_word(chip->i2c, RT9426_REG_SWINDOW3, val);
		rt9426_reg_write_word(chip->i2c, RT9426_REG_DUMMY, 0x0000);
		ret = count;
		break;
	case WSW4:
		ret = get_parameters((char *)buf, &val, 1);
		if (ret < 0) {
			dev_notice(dev, "get paremters fail\n");
			ret = -EINVAL;
		}
		rt9426_reg_write_word(chip->i2c, RT9426_REG_SWINDOW4, val);
		rt9426_reg_write_word(chip->i2c, RT9426_REG_DUMMY, 0x0000);
		ret = count;
		break;
	case WSW5:
		ret = get_parameters((char *)buf, &val, 1);
		if (ret < 0) {
			dev_notice(dev, "get paremters fail\n");
			ret = -EINVAL;
		}
		rt9426_reg_write_word(chip->i2c, RT9426_REG_SWINDOW5, val);
		rt9426_reg_write_word(chip->i2c, RT9426_REG_DUMMY, 0x0000);
		ret = count;
		break;
	case WSW6:
		ret = get_parameters((char *)buf, &val, 1);
		if (ret < 0) {
			dev_notice(dev, "get paremters fail\n");
			ret = -EINVAL;
		}
		rt9426_reg_write_word(chip->i2c, RT9426_REG_SWINDOW6, val);
		rt9426_reg_write_word(chip->i2c, RT9426_REG_DUMMY, 0x0000);
		ret = count;
		break;
	case WSW7:
		ret = get_parameters((char *)buf, &val, 1);
		if (ret < 0) {
			dev_notice(dev, "get paremters fail\n");
			ret = -EINVAL;
		}
		rt9426_reg_write_word(chip->i2c, RT9426_REG_SWINDOW7, val);
		rt9426_reg_write_word(chip->i2c, RT9426_REG_DUMMY, 0x0000);
		ret = count;
		break;
	case WSW8:
		ret = get_parameters((char *)buf, &val, 1);
		if (ret < 0) {
			dev_notice(dev, "get paremters fail\n");
			ret = -EINVAL;
		}
		rt9426_reg_write_word(chip->i2c, RT9426_REG_SWINDOW8, val);
		rt9426_reg_write_word(chip->i2c, RT9426_REG_DUMMY, 0x0000);
		ret = count;
		break;
	case WTEMP:
		ret = get_parameters((char *)buf, &val, 1);
		if (ret < 0) {
			dev_notice(dev, "get paremters fail\n");
			ret = -EINVAL;
		}
		rt9426_reg_write_word(chip->i2c,
					RT9426_REG_TEMP, ((val*10)+2732));
		rt9426_reg_write_word(chip->i2c, RT9426_REG_DUMMY, 0x0000);
		ret = count;
		break;
	case UNSEAL:
		ret = get_parameters((char *)buf, &val, 1);
		if (ret < 0) {
			dev_notice(dev, "get paremters fail\n");
			ret = -EINVAL;
		}
		ret = rt9426_reg_read_word(chip->i2c, RT9426_REG_FLAG3);
		if ((ret & 0x0001) == 0) {
			/* Unseal RT9426 */
			rt9426_reg_write_word(chip->i2c, RT9426_REG_CNTL,
						(RT9426_Unseal_Key & 0xffff));
			rt9426_reg_write_word(chip->i2c, RT9426_REG_CNTL,
						(RT9426_Unseal_Key >> 16));
			rt9426_reg_write_word(chip->i2c,
						RT9426_REG_DUMMY, 0x0000);
			mdelay(10);

			ret = rt9426_reg_read_word(chip->i2c, RT9426_REG_FLAG3);
			if ((ret & 0x0001) == 0)
				dev_info(dev, "RT9426 Unseal Fail\n");
			else
				dev_info(dev, "RT9426 Unseal Pass\n");
		} else
			dev_info(dev, "RT9426 Unseal Pass\n");

		/* Unseal RT9426 */
		ret = count;
		break;
	case FG_SET_TEMP: /* New to set FG Temperature ; 2019-09-09 */
		ret = get_parameters((char *)buf, &val, 1);
		if (ret < 0) {
			dev_notice(dev, "get paremters fail\n");
			ret = -EINVAL;
		}
		rt9426_temperature_set(chip, val);
		ret = count;
		break;
	case FG_RESET: /* New to reset FG ; 2019-09-09 */
		rt9426_reset(chip);
		ret = count;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int rt_fg_create_attrs(struct device *dev)
{
	int i, rc;

	for (i = 0; i < ARRAY_SIZE(rt_fuelgauge_attrs); i++) {
		rc = device_create_file(dev, &rt_fuelgauge_attrs[i]);
		if (rc)
			goto create_attrs_failed;
	}
	goto create_attrs_succeed;

create_attrs_failed:
	dev_notice(dev, "%s: failed (%d)\n", __func__, rc);
	while (i--)
		device_remove_file(dev, &rt_fuelgauge_attrs[i]);
create_attrs_succeed:
	return rc;
}

static int rt9426_irq_enable(struct rt9426_chip *chip, bool enable)
{
	int regval;

	if (rt9426_unseal_wi_retry(chip) == RT9426_UNSEAL_PASS) {
		rt9426_write_page_cmd(chip, RT9426_PAGE_1);
		rt9426_reg_write_word(chip->i2c, RT9426_REG_SWINDOW2,
					enable ? chip->pdata->op_config[1] : 0);
		rt9426_reg_write_word(chip->i2c, RT9426_REG_DUMMY, 0x0000);
		mdelay(5);
		/* if disable, force clear irq status */
		if (!enable) {
			regval = rt9426_reg_read_word(chip->i2c,
							RT9426_REG_IRQ);
			dev_info(chip->dev,
				"previous irq status 0x%04x\n", regval);
		}
	}
	return 0;
}

static int rt9426_irq_init(struct rt9426_chip *chip)
{
	int rc = 0;

	dev_info(chip->dev, "%s\n", __func__);
	chip->alert_irq = chip->i2c->irq;
	rc = devm_request_threaded_irq(chip->dev, chip->alert_irq, NULL,
				       rt9426_irq_handler,
				       IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				       "rt9426_fg_irq", chip);
	if (rc < 0) {
		dev_notice(chip->dev, "irq register failed\n");
		return rc;
	}
	device_init_wakeup(chip->dev, true);
	return 0;
}

static int rt9426_irq_deinit(struct rt9426_chip *chip)
{
	device_init_wakeup(chip->dev, false);
	return 0;
}

static int rt9426_apply_pdata(struct rt9426_chip *chip)
{
	int regval = 0, i, j, retry_times, ret = 0;
	int op_config_reading[5] = {0};

	dev_info(chip->dev, "%s\n", __func__);

	retry_times = 30;
	for (i = 0 ; i < retry_times ; i++) {
		regval = rt9426_reg_read_word(chip->i2c, RT9426_REG_FLAG2);
		if (regval & RT9426_RDY_MASK) {
			regval = rt9426_reg_read_word(chip->i2c,
							RT9426_REG_FLAG3);
			if (!(regval & RT9426_RI_MASK)) {
				regval = rt9426_reg_read_word(chip->i2c,
							  RT9426_REG_RSVD_Flag);
				if ((regval & 0x7FFF) == RT9426_DRIVER_VER) {
					dev_info(chip->dev,
						"RI=0, bypass initial phase\n");
					goto out_apply;
				} else {
					dev_info(chip->dev,
					      "force init due to DRIVER_VER\n");
					mdelay(60);
					break;
				}
			} else {
				dev_info(chip->dev, "initialization due to RI\n");
				mdelay(60);
				break;
			}
		}
		mdelay(10);
	}

	if (rt9426_unseal_wi_retry(chip) == RT9426_UNSEAL_FAIL)
		goto out_apply;

	/*NOTICE!! Please follow below setting sequence & order */
	/* set OCV Table */
	if (chip->pdata->ocv_table[0].data[0] == 0x13) {
		retry_times = 3;
		dev_info(chip->dev, "Write NEW OCV Table\n");
		while (retry_times) {
			for (i = 0; i < 9; i++) {
				rt9426_reg_write_word(chip->i2c,
					     RT9426_REG_EXTREGCNTL, 0xCB00 + i);
				rt9426_reg_write_word(chip->i2c,
					     RT9426_REG_EXTREGCNTL, 0xCB00 + i);
				for (j = 0; j < 8; j++) {
					rt9426_reg_write_word(chip->i2c,
					     0x40 + j * 2,
					     chip->pdata->ocv_table[i].data[j]);
					mdelay(1);
				}
			}
			rt9426_reg_write_word(chip->i2c,
						RT9426_REG_EXTREGCNTL, 0xCB09);
			rt9426_reg_write_word(chip->i2c,
						RT9426_REG_EXTREGCNTL, 0xCB09);
			for (i = 0; i < 5; i++) {
				rt9426_reg_write_word(chip->i2c, 0x40 + i * 2,
				chip->pdata->ocv_table[9].data[i]);
				mdelay(1);
			}
			rt9426_reg_write_word(chip->i2c,
						RT9426_REG_DUMMY, 0x0000);
			mdelay(10);
			regval = rt9426_reg_read_word(chip->i2c,
							RT9426_REG_FLAG2);
			if (regval & RT9426_USR_TBL_USED_MASK) {
				dev_info(chip->dev,
					"OCV Table Write Successful\n");
				break;
			}
			retry_times--;
			if (retry_times == 0)
				dev_notice(chip->dev, "Set OCV Table fail\n");
		}
	}

	/* set alert threshold */
	rt9426_write_page_cmd(chip, RT9426_PAGE_3);
	rt9426_reg_write_word(chip->i2c, RT9426_REG_SWINDOW1,
					chip->pdata->otc_tth);
	rt9426_reg_write_word(chip->i2c, RT9426_REG_SWINDOW2,
					chip->pdata->otc_chg_ith);
	rt9426_reg_write_word(chip->i2c, RT9426_REG_SWINDOW3,
					chip->pdata->otd_tth);
	rt9426_reg_write_word(chip->i2c, RT9426_REG_SWINDOW4,
					chip->pdata->otd_dchg_ith);
	rt9426_reg_write_word(chip->i2c, RT9426_REG_SWINDOW5,
					chip->pdata->uv_ov_threshold);
	rt9426_reg_write_word(chip->i2c, RT9426_REG_SWINDOW6,
				(0x4600|(chip->pdata->us_threshold)));
	rt9426_reg_write_word(chip->i2c, RT9426_REG_DUMMY, 0x0000);
	mdelay(5);

	/* set OCV type	*/
	rt9426_write_page_cmd(chip, RT9426_PAGE_2);
	if (chip->pdata->battery_type == 4400) {
		rt9426_reg_write_word(chip->i2c, RT9426_REG_SWINDOW1, 0x8200);
		dev_info(chip->dev,
			"%s: ocv type = %d, set ocv type as = 0x8200\n",
			__func__, chip->pdata->battery_type);
	} else if (chip->pdata->battery_type == 4352) {
		rt9426_reg_write_word(chip->i2c, RT9426_REG_SWINDOW1, 0x8100);
		dev_info(chip->dev,
			"%s: ocv type = %d, set ocv type as = 0x8100\n",
			__func__, chip->pdata->battery_type);
	} else if (chip->pdata->battery_type == 4354) {
		rt9426_reg_write_word(chip->i2c, RT9426_REG_SWINDOW1, 0x8300);
		dev_info(chip->dev,
			"%s: ocv type = %d, set ocv type as = 0x8300\n",
			__func__, chip->pdata->battery_type);
	} else if (chip->pdata->battery_type == 4200) {
		rt9426_reg_write_word(chip->i2c, RT9426_REG_SWINDOW1, 0x8000);
		dev_info(chip->dev,
			"%s: ocv type = %d, set ocv type as = 0x8000\n",
			__func__, chip->pdata->battery_type);
	} else {
		rt9426_reg_write_word(chip->i2c, RT9426_REG_SWINDOW1, 0x8100);
		dev_info(chip->dev,
			"%s: ocv type = %d, set ocv type as = 0x8100\n",
			__func__, chip->pdata->battery_type);
	}

	/* set design capacity */
	rt9426_reg_write_word(chip->i2c, RT9426_REG_SWINDOW6,
				chip->pdata->design_capacity);

	/* set fcc */
	rt9426_reg_write_word(chip->i2c, RT9426_REG_SWINDOW7, chip->pdata->fcc);
	rt9426_reg_write_word(chip->i2c, RT9426_REG_DUMMY, 0x0000);
	mdelay(5);

	rt9426_read_page_cmd(chip, RT9426_PAGE_2);
	ret = rt9426_reg_read_word(chip->i2c, RT9426_REG_SWINDOW6);
	dev_info(chip->dev,
	     "%s: design capacity setting = %d, design capacity reading = %d\n",
	     __func__, chip->pdata->design_capacity, ret);
	if (ret == chip->pdata->design_capacity)
		dev_info(chip->dev, "%s: design capacity matched\n", __func__);
	else
		dev_info(chip->dev,
			"%s: design capacity mismatched\n", __func__);

	ret = rt9426_reg_read_word(chip->i2c, RT9426_REG_SWINDOW7);
	dev_info(chip->dev, "%s: fcc setting = %d, fcc reading = %d\n",
					__func__, chip->pdata->fcc, ret);
	if (ret == chip->pdata->fcc)
		dev_info(chip->dev, "%s: fcc matched\n", __func__);
	else
		dev_info(chip->dev, "%s: fcc mismatched\n", __func__);

	/* set op config  */
	rt9426_write_page_cmd(chip, RT9426_PAGE_1);
	rt9426_reg_write_word(chip->i2c, RT9426_REG_SWINDOW1,
					chip->pdata->op_config[0]);
	rt9426_reg_write_word(chip->i2c, RT9426_REG_SWINDOW2,
					chip->pdata->op_config[1]);
	rt9426_reg_write_word(chip->i2c, RT9426_REG_SWINDOW3,
					chip->pdata->op_config[2]);
	rt9426_reg_write_word(chip->i2c, RT9426_REG_SWINDOW4,
					chip->pdata->op_config[3]);
	rt9426_reg_write_word(chip->i2c, RT9426_REG_SWINDOW5,
					chip->pdata->op_config[4]);
	/* set curr deadband */
	rt9426_reg_write_word(chip->i2c, RT9426_REG_SWINDOW7,
				(0x0012 | (chip->pdata->curr_db << 8)));
	rt9426_reg_write_word(chip->i2c, RT9426_REG_DUMMY, 0x0000);
	mdelay(5);

	rt9426_read_page_cmd(chip, RT9426_PAGE_1);
	/* read all 5 op_config for check ; 2019-09-09 */
	for (i = 0; i < 5; i++) {
		op_config_reading[i] =
		   rt9426_reg_read_word(chip->i2c, RT9426_REG_SWINDOW1 + (i*2));
	}
	/* print op_config setting ; 2019-09-09 */
	dev_info(chip->dev,
	     "%s: op_config setting = 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x\n",
	     __func__, chip->pdata->op_config[0], chip->pdata->op_config[1],
	     chip->pdata->op_config[2], chip->pdata->op_config[3],
	     chip->pdata->op_config[4]);
	/* print op_config reading ; 2019-09-09 */
	dev_info(chip->dev,
	     "%s: op_config reading = 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x\n",
	     __func__, op_config_reading[0], op_config_reading[1],
	     op_config_reading[2], op_config_reading[3], op_config_reading[4]);

	/* compare OPCFG1 & show result ; 2019-09-09 */
	if (op_config_reading[0] != chip->pdata->op_config[0])
		dev_info(chip->dev, "%s: OPCFG1 mismatched\n", __func__);
	else {
		dev_info(chip->dev, "%s: OPCFG1 matched\n", __func__);

		if (((op_config_reading[0] & 0x00C0) >> 6) == 0)
			dev_info(chip->dev, "Rsense(2.5)mohm\n");
		else if (((op_config_reading[0] & 0x00C0) >> 6) == 1)
			dev_info(chip->dev, "Rsense(5.0)mohm\n");
		else if (((op_config_reading[0] & 0x00C0) >> 6) == 2)
			dev_info(chip->dev, "Rsense(10)mohm\n");
		else
			dev_info(chip->dev, "Rsense(20)mohm\n");

		if (((op_config_reading[0] & 0xC000) >> 14) == 0)
			dev_info(chip->dev, "Temperature (NTC)\n");
		else if (((op_config_reading[0] & 0xC000) >> 14) == 1)
			dev_info(chip->dev, "Temperature (AP)\n");
		else
			dev_info(chip->dev, "Temperature (INT_Temp)\n");
	}

	/* compare OPCFG2 & show result ; 2019-09-09 */
	if (op_config_reading[1] != chip->pdata->op_config[1])
		dev_info(chip->dev, "%s: OPCFG2 mismatched\n", __func__);
	else {
		dev_info(chip->dev, "%s: OPCFG2 matched\n", __func__);

		if (((op_config_reading[1] & 0x0080) >> 7) == 1)
			dev_info(chip->dev, "Auto_Sleep (Enable)\n");
		else
			dev_info(chip->dev, "Auto_Sleep (Disable)\n");

		if (((op_config_reading[1] & 0x0040) >> 6) == 1)
			dev_info(chip->dev, "Sleep_Mode (Enable)\n");
		else
			dev_info(chip->dev, "Sleep_Mode (Disable)\n");

		if (((op_config_reading[1] & 0x0020) >> 5) == 1)
			dev_info(chip->dev, "Shutdown_Mode (Enable)\n");
		else
			dev_info(chip->dev, "Shutdown_Mode (Disable)\n");

		if (((op_config_reading[1] & 0x0001)) == 1)
			dev_info(chip->dev, "Battery_Detection (Enable)\n");
		else
			dev_info(chip->dev, "Battery_Detection (Disable)\n");

		if (((op_config_reading[1] & 0x2000) >> 13) == 1)
			dev_info(chip->dev, "SOC_IRQ (Enable)\n");
		else
			dev_info(chip->dev, "SOC_IRQ (Disable)\n");

		if (((op_config_reading[1] & 0x1000) >> 12) == 1)
			dev_info(chip->dev, "Battery_Det_IRQ (Enable)\n");
		else
			dev_info(chip->dev, "Battery_Det_IRQ (Disable)\n");

		if (((op_config_reading[1] & 0x0200) >> 9) == 1)
			dev_info(chip->dev, "SC_IRQ (Enable)\n");
		else
			dev_info(chip->dev, "SC_IRQ (Disable)\n");
	}

	/* compare OPCFG3 & show result ; 2019-09-09 */
	if (op_config_reading[2] != chip->pdata->op_config[2])
		dev_info(chip->dev, "%s: OPCFG3 mismatched\n", __func__);
	else {
		dev_info(chip->dev, "%s: OPCFG3 matched\n", __func__);

		if (((op_config_reading[2] & 0x00E0) >> 4) == 0xE)
			dev_info(chip->dev, "FC+FD+RLX_Det (Enable)\n");
		else
			dev_info(chip->dev, "FC+FD+RLX_Det (Disable)\n");

		if (((op_config_reading[2] & 0x0018) >> 3) == 0x3)
			dev_info(chip->dev, "TLCOMP (Enable)\n");
		else
			dev_info(chip->dev, "TLCOMP (Disable)\n");

		if (((op_config_reading[2] & 0x0007)) == 0x7)
			dev_info(chip->dev, "BCCOMP (Enable)\n");
		else
			dev_info(chip->dev, "BCCOMP (Disable)\n");

		if (((op_config_reading[2] & 0x0200) >> 9) == 1)
			dev_info(chip->dev, "FC_LOCK (Enable)\n");
		else
			dev_info(chip->dev, "FC_LOCK (Disable)\n");
	}
	/* compare OPCFG4 & show result ; 2019-09-09 */
	if (op_config_reading[3] != chip->pdata->op_config[3])
		dev_info(chip->dev, "%s: OPCFG4 mismatched\n", __func__);
	else
		dev_info(chip->dev, "%s: OPCFG4 matched\n", __func__);

	/* compare OPCFG5 & show result ; 2019-09-09 */
	if (op_config_reading[4] != chip->pdata->op_config[4])
		dev_info(chip->dev, "%s: OPCFG5 mismatched\n", __func__);
	else
		dev_info(chip->dev, "%s: OPCFG5 matched\n", __func__);

	/* set fc_vth + fc_ith*/
	ret = (chip->pdata->fc_vth) | (chip->pdata->fc_ith << 8);
	rt9426_write_page_cmd(chip, RT9426_PAGE_5);
	rt9426_reg_write_word(chip->i2c, RT9426_REG_SWINDOW3, ret);

	/* set fc_sth */
	ret = 0x4100 | (chip->pdata->fc_sth);
	rt9426_reg_write_word(chip->i2c, RT9426_REG_SWINDOW4, ret);

	/* set fd_vth */
	ret = 0x1200 | (chip->pdata->fd_vth);
	rt9426_reg_write_word(chip->i2c, RT9426_REG_SWINDOW6, ret);
	rt9426_reg_write_word(chip->i2c, RT9426_REG_DUMMY, 0x0000);
	mdelay(5);

	/* set EXTREG  */
	ret = chip->pdata->extreg_size;
	dev_info(chip->dev, "CHIP_INIT => EXTREG_SIZE=<%d>\n", ret);
	for (i = 0; i < ret; i++) {
		rt9426_reg_write_word(chip->i2c, RT9426_REG_EXTREGCNTL,	0x6550 +
		(chip->pdata->extreg_update.extreg_update_data[i].extreg_page));
		rt9426_reg_write_word(chip->i2c, RT9426_REG_EXTREGCNTL,	0x6550 +
		(chip->pdata->extreg_update.extreg_update_data[i].extreg_page));
		mdelay(1);
		rt9426_reg_write_word(chip->i2c,
		  chip->pdata->extreg_update.extreg_update_data[i].extreg_addr,
		  chip->pdata->extreg_update.extreg_update_data[i].extreg_data);
		mdelay(5);
	}
	rt9426_reg_write_word(chip->i2c, RT9426_REG_DUMMY, 0x0000);

	/* Send QS Command to get INI SOC */
	rt9426_reg_write_word(chip->i2c, RT9426_REG_CNTL, 0x4000);
	rt9426_reg_write_word(chip->i2c, RT9426_REG_DUMMY, 0x0000);
	mdelay(5);

	/* clear RI, set 0 to RI bits */
	regval = rt9426_reg_read_word(chip->i2c, RT9426_REG_FLAG3);
	regval = (regval & ~RT9426_RI_MASK);
	rt9426_reg_write_word(chip->i2c, RT9426_REG_FLAG3, regval);
	rt9426_reg_write_word(chip->i2c, RT9426_REG_DUMMY, 0x0000);
	regval = rt9426_reg_read_word(chip->i2c, RT9426_REG_FLAG3);
	if (((regval&RT9426_RI_MASK) >> 8) == 0)
		dev_info(chip->dev, "RT9426 RI=0\n");
	else
		dev_info(chip->dev, "RT9426 RI=1\n");

	regval = rt9426_reg_read_word(chip->i2c, RT9426_REG_RSVD_Flag);
	if ((regval & 0x7FFF) != RT9426_DRIVER_VER) {
		regval = (regval & 0x8000) | RT9426_DRIVER_VER;
		rt9426_reg_write_word(chip->i2c, RT9426_REG_RSVD_Flag, regval);
		rt9426_reg_write_word(chip->i2c, RT9426_REG_DUMMY, 0x0000);
	}

out_apply:
	chip->online = 1;
	return 0;
}
struct dt_offset_params {
	int data[3];
};

struct dt_extreg_params {
	int edata[3];
};

static int rt_parse_dt(struct device *dev, struct rt9426_platform_data *pdata)
{
#ifdef CONFIG_OF
	struct device_node *np = dev->of_node;
	int sizes[3] = {0}, j, ret, i;
	struct dt_offset_params *offset_params;
	struct dt_extreg_params *extreg_params;
	const char *bat_name = "rt-fuelguage";

	dev_info(dev, "%s\n", __func__);

	ret = of_property_read_u32_array(np, "rt,dtsi_version",
						pdata->dtsi_version, 2);
	if (ret < 0)
		pdata->dtsi_version[0] = pdata->dtsi_version[1] = 0;

	ret = of_property_read_string(np, "rt,bat_name",
				      (char const **)&bat_name);
	if (ret == 0)
		pdata->bat_name = kasprintf(GFP_KERNEL, "%s", bat_name);

	ret = of_property_read_u32_array(np, "rt,offset_interpolation_order",
					 pdata->offset_interpolation_order, 2);
	if (ret < 0)
		pdata->offset_interpolation_order[0] =
				       pdata->offset_interpolation_order[1] = 2;

	sizes[0] = sizes[1] = 0;
	ret = of_property_read_u32_array(np, "rt,soc_offset_size", sizes, 2);
	if (ret < 0)
		dev_notice(dev, "Can't get prop soc_offset_size(%d)\n", ret);

	new_vgcomp_soc_offset_datas(dev, SOC_OFFSET,
						pdata, sizes[0], sizes[1], 0);
	if (pdata->soc_offset.soc_offset_data) {
		offset_params = devm_kzalloc(dev,
					     sizes[0] * sizes[1] *
					     sizeof(struct dt_offset_params),
					     GFP_KERNEL);

		of_property_read_u32_array(np, "rt,soc_offset_data",
				(u32 *)offset_params, sizes[0] * sizes[1] * 3);
		for (j = 0; j < sizes[0] * sizes[1]; j++) {
			pdata->soc_offset.
				soc_offset_data[j].x = offset_params[j].data[0];
			pdata->soc_offset.
				soc_offset_data[j].y = offset_params[j].data[1];
			pdata->soc_offset.
				soc_offset_data[j].offset =
						offset_params[j].data[2];
		}
		devm_kfree(dev, offset_params);
	}
	ret = of_property_read_u32(np,
				   "rt,extreg_size", &pdata->extreg_size);
	if (ret < 0)
		dev_notice(dev, "Can't get prop extreg_size(%d)\n", ret);

	dev_info(dev, "Parse EXTREG_SIZE=<%d>\n", pdata->extreg_size);
	new_vgcomp_soc_offset_datas(dev, EXTREG_UPDATE, pdata,
				    pdata->extreg_size, 0, 0);
	if (pdata->extreg_update.extreg_update_data) {
		extreg_params = devm_kzalloc(dev,
					     pdata->extreg_size *
					     sizeof(struct dt_extreg_params),
					     GFP_KERNEL);

		of_property_read_u32_array(np, "rt,extreg_data",
				  (u32 *)extreg_params, pdata->extreg_size * 3);
		for (j = 0; j < pdata->extreg_size;  ++j) {
			pdata->extreg_update.
				extreg_update_data[j].extreg_page =
						      extreg_params[j].edata[0];
			pdata->extreg_update.
				extreg_update_data[j].extreg_addr =
						      extreg_params[j].edata[1];
			pdata->extreg_update.
				extreg_update_data[j].extreg_data =
						      extreg_params[j].edata[2];
		}
		devm_kfree(dev, extreg_params);
	}
	dev_info(dev, "EXTREG_DATA Parse OK\n");

	ret = of_property_read_u32(np, "rt,battery_type", &pdata->battery_type);
	if (ret < 0) {
		dev_info(dev, "uset default battery_type 4350mV, EDV=3200mV\n");
		pdata->battery_type = 4352;
	}

	ret = of_property_read_u32(np, "rt,volt_source", &pdata->volt_source);
	if (ret < 0)
		pdata->volt_source = RT9426_REG_AV;

	if (pdata->volt_source == 0)
		pdata->volt_source = 0;
	else if (pdata->volt_source == 1)
		pdata->volt_source = RT9426_REG_VBAT;
	else if (pdata->volt_source == 2)
		pdata->volt_source = RT9426_REG_OCV;
	else if (pdata->volt_source == 3)
		pdata->volt_source = RT9426_REG_AV;
	else {
		dev_notice(dev, "pdata->volt_source is out of range, use 3\n");
		pdata->volt_source = RT9426_REG_AV;
	}

	ret = of_property_read_u32(np, "rt,temp_source", &pdata->temp_source);
	if (ret < 0)
		pdata->temp_source = 0;

	if (pdata->temp_source == 0)
		pdata->temp_source = 0;
	else if (pdata->temp_source == 1)
		pdata->temp_source = RT9426_REG_TEMP;
	else if (pdata->temp_source == 2)
		pdata->temp_source = RT9426_REG_INTT;
	else if (pdata->temp_source == 3)
		pdata->temp_source = RT9426_REG_AT;
	else {
		dev_notice(dev, "pdata->temp_source is out of range, use 0\n");
		pdata->temp_source = 0;
	}
	ret = of_property_read_u32(np, "rt,curr_source",
						&pdata->curr_source);
	if (ret < 0)
		pdata->curr_source = 0;
	if (pdata->curr_source == 0)
		pdata->curr_source = 0;
	else if (pdata->curr_source == 1)
		pdata->curr_source = RT9426_REG_CURR;
	else if (pdata->curr_source == 2)
		pdata->curr_source = RT9426_REG_AI;
	else {
		dev_notice(dev, "pdata->curr_source is out of range, use 2\n");
		pdata->curr_source = RT9426_REG_AI;
	}

	ret = of_property_read_u32(np, "rt,fg_otc_tth", &pdata->otc_tth);
	if (ret < 0) {
		dev_notice(dev, "no otc_tth property, use default 0x0064\n");
		pdata->otc_tth = 0x0064;
	}

	ret = of_property_read_u32(np,
				   "rt,fg_otc_chg_ith", &pdata->otc_chg_ith);
	if (ret < 0) {
		dev_notice(dev, "no otc_chg_ith property, use default 0x0B5F\n");
		pdata->otc_chg_ith = 0x0B5F;
	}

	ret = of_property_read_u32(np, "rt,fg_otd_tth", &pdata->otd_tth);
	if (ret < 0) {
		dev_notice(dev, "no otd_tth property, use default 0x0064\n");
		pdata->otd_tth = 0x0064;
	}

	ret = of_property_read_u32(np, "rt,fg_otd_dchg_ith",
					 &pdata->otd_dchg_ith);
	if (ret < 0) {
		dev_notice(dev, "no otd_dchg_ith property, use default 0x0B5F\n");
		pdata->otd_dchg_ith = 0x0B5F;
	}


	ret = of_property_read_u32(np, "rt,fg_uvov_threshold",
					 &pdata->uv_ov_threshold);
	if (ret < 0) {
		dev_notice(dev, "no uvov th property, use default 0x00ff\n");
		pdata->uv_ov_threshold = 0x00FF;
	}

	ret = of_property_read_u32(np, "rt,fg_us_threshold",
					&pdata->us_threshold);
	if (ret < 0) {
		dev_notice(dev, "no us th property, use default 0x00\n");
		pdata->us_threshold = 0x00;
	}

	ret = of_property_read_u32(np, "rt,design_capacity",
					 &pdata->design_capacity);
	if (ret < 0) {
		dev_notice(dev, "no design capacity property, use defaut 2000\n");
		pdata->design_capacity = 2000;
	}

	ret = of_property_read_u32(np, "rt,fcc",
					 &pdata->fcc);
	if (ret < 0) {
		dev_notice(dev, "no FCC property, use defaut 2000\n");
		pdata->fcc = 2000;
	}


	ret = of_property_read_u32_array(np, "rt,fg_ocv_table",
					(u32 *)pdata->ocv_table, 80);
	if (ret < 0) {
		dev_notice(dev, "no ocv table property\n");
		for (j = 0; j < 10; j++)
			for (i = 0; i < 8; i++)
				pdata->ocv_table[j].data[i] = 0;
	}

	ret = of_property_read_u32_array(np, "rt,fg_op_config",
					 (u32 *)pdata->op_config, 5);
	if (ret < 0) {
		dev_notice(dev, "no fg op config proeprty, use default\n");
		pdata->op_config[0] = 0x9480;
		pdata->op_config[1] = 0x3241;
		pdata->op_config[2] = 0x3EFF;
		pdata->op_config[3] = 0x2000;
		pdata->op_config[4] = 0x2A7F;
	}

	ret = of_property_read_u32(np, "rt,fg_curr_db", &pdata->curr_db);
	if (ret < 0) {
		dev_notice(dev, "no curr deadband property, use default 0x0005\n");
		pdata->curr_db = 0x0005;
	}

	ret = of_property_read_u32(np, "rt,fg_fc_vth", &pdata->fc_vth);
	if (ret < 0) {
		dev_notice(dev, "no fc_vth property, use default 4200mV\n");
		pdata->fc_vth = 0x0078;
	}

	ret = of_property_read_u32(np, "rt,fg_fc_ith", &pdata->fc_ith);
	if (ret < 0) {
		dev_notice(dev, "no fc_ith property, use default 52mA\n");
		pdata->fc_ith = 0x000D;
	}

	ret = of_property_read_u32(np, "rt,fg_fc_sth", &pdata->fc_sth);
	if (ret < 0) {
		dev_notice(dev, "no fc_sth property, use default 70 percent\n");
		pdata->fc_sth = 0x0046;
	}

	ret = of_property_read_u32(np, "rt,fg_fd_vth", &pdata->fd_vth);
	if (ret < 0) {
		dev_notice(dev, "no fd_vth property, use default 3225mV\n");
		pdata->fd_vth = 0x0091;
	}
#endif /* CONFIG_OF */
	return 0;
}

static int rt9426_i2c_chipid_check(struct i2c_client *i2c)
{
	u16 ver;
	int ret;

	ret = i2c_smbus_read_i2c_block_data(i2c, RT9426_REG_VER, 2, (u8 *)&ver);
	if (ret < 0)
		return ret;
	ver = le16_to_cpu(ver);
	if ((ver & 0xff00) != 0x0000) {
		dev_notice(&i2c->dev, "chip id not match\n");
		return -ENODEV;
	}
	return ver;
}

static int rt9426_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct rt9426_platform_data *pdata = i2c->dev.platform_data;
	struct rt9426_chip *chip;
	struct power_supply_config psy_config = {};
	u16 ic_ver;
	bool use_dt = i2c->dev.of_node;
	int ret = 0;

	/* check chip id first */
	ret = rt9426_i2c_chipid_check(i2c);
	if (ret < 0) {
		dev_notice(&i2c->dev, "chip id check fail\n");
		return ret;
	}
	ic_ver = (u16)ret;
	/* alloc memory */
	chip = devm_kzalloc(&i2c->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;
	if (use_dt) {
		pdata = devm_kzalloc(&i2c->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata)
			return -ENOMEM;
		memcpy(pdata, &def_platform_data, sizeof(*pdata));
		rt_parse_dt(&i2c->dev, pdata);
		chip->pdata = i2c->dev.platform_data = pdata;
	} else {
		if (!pdata) {
			dev_notice(&i2c->dev, " no platdata specified\n");
			return -EINVAL;
		}
	}

	chip->i2c = i2c;
	chip->dev = &i2c->dev;
	chip->alert_irq = -1;
	chip->btemp = 250;
	chip->bvolt = 3800;
	chip->bcurr = 1000;
	chip->ic_ver = ic_ver;
	mutex_init(&chip->var_lock);
	i2c_set_clientdata(i2c, chip);

	/* rt regmap init */
	ret = rt9426_regmap_init(chip);
	if (ret < 0) {
		dev_notice(chip->dev, "regmap init fail\n");
		return ret;
	}
	/* apply platform data */
	ret = rt9426_apply_pdata(chip);
	if (ret < 0) {
		dev_notice(chip->dev, "apply pdata fail\n");
		return ret;
	}
	/* fg psy register */
	psy_config.of_node = i2c->dev.of_node;
	psy_config.drv_data = chip;
	if (pdata->bat_name)
		fg_psy_desc.name = pdata->bat_name;
	chip->fg_psy = devm_power_supply_register(&i2c->dev,
						    &fg_psy_desc, &psy_config);
	if (IS_ERR(chip->fg_psy)) {
		dev_notice(chip->dev, "register batt psy fail\n");
		return PTR_ERR(chip->fg_psy);
	}
	rt_fg_create_attrs(&chip->fg_psy->dev);
	/* mask irq before irq register */
	ret = rt9426_irq_enable(chip, false);
	if (ret < 0) {
		dev_notice(chip->dev, "scirq mask fail\n");
		return ret;
	}
	ret = rt9426_irq_init(chip);
	if (ret < 0) {
		dev_notice(chip->dev, "irq init fail\n");
		return ret;
	}
	ret = rt9426_irq_enable(chip, true);
	if (ret < 0) {
		dev_notice(chip->dev, "scirq mask fail\n");
		return ret;
	}
	rt9426_fg_create_debug_files(chip);
	dev_info(chip->dev, "chip ver = 0x%04x\n", chip->ic_ver);
	return 0;
}

static int rt9426_i2c_remove(struct i2c_client *i2c)
{
	struct rt9426_chip *chip = i2c_get_clientdata(i2c);

	dev_info(chip->dev, "%s\n", __func__);
	rt9426_irq_enable(chip, false);
	rt9426_irq_deinit(chip);
	mutex_destroy(&chip->var_lock);
	return 0;
}

static int rt9426_i2c_suspend(struct device *dev)
{
	struct rt9426_chip *chip = dev_get_drvdata(dev);

	dev_dbg(chip->dev, "%s\n", __func__);
	if (device_may_wakeup(dev))
		enable_irq_wake(chip->alert_irq);
	return 0;
}

static int rt9426_i2c_resume(struct device *dev)
{
	struct rt9426_chip *chip = dev_get_drvdata(dev);

	dev_dbg(chip->dev, "%s\n", __func__);
	if (device_may_wakeup(dev))
		disable_irq_wake(chip->alert_irq);
	return 0;
}

static SIMPLE_DEV_PM_OPS(rt9426_pm_ops, rt9426_i2c_suspend, rt9426_i2c_resume);

static const struct i2c_device_id rt_i2c_id[] = {
	{ "rt9426", 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, rt_i2c_id);

static const struct of_device_id rt_match_table[] = {
	{ .compatible = "richtek,rt9426", },
	{},
};
MODULE_DEVICE_TABLE(of, rt_match_table);

static struct i2c_driver rt9426_i2c_driver = {
	.driver = {
		.name = "rt9426",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(rt_match_table),
		.pm = &rt9426_pm_ops,
	},
	.probe = rt9426_i2c_probe,
	.remove = rt9426_i2c_remove,
	.id_table = rt_i2c_id,
};
module_i2c_driver(rt9426_i2c_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jeff Chang <jeff_chang@richtek.com>");
MODULE_DESCRIPTION("RT9426 Fuelgauge Driver");
MODULE_VERSION("1.0.1_G");
