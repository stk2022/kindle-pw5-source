/*
 * Copyright (C) 2017 MediaTek Inc.
 * CY Huang <cy_huang@richtek.com>
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
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#ifdef CONFIG_RT_REGMAP
#include <mt-plat/rt-regmap.h>
#endif /* #ifdef CONFIG_RT_REGMAP */
#ifdef CONFIG_MTK_CHARGER_CLASS
#include <linux/power/mtk_charger_class.h>
#endif /* CONFIG_MTK_CHARGER_CLASS */
#ifdef CONFIG_CHARGER_MTK
#include <mt-plat/upmu_common.h>
#include <mt-plat/charger_type.h>
#endif /* CONFIG_CHARGER_MTK */

/* Interrupt is from linux kernel or not */
/* #define CONFIG_RT9460_KERNEL_INTR */

#if !defined(CONFIG_RT9460_KERNEL_INTR) && defined(CONFIG_MTK_HIFI4DSP_SUPPORT)
#include <adsp_ipi.h>
#include <linux/power/mtk_power_ipi_msg.h>
#endif /* !CONFIG_RT9460_KERNEL_INTR && CONFIG_MTK_HIFI4DSP_SUPPORT */

#include "rt9460.h"
#define RT9460_DRV_VERSION	"1.0.5_MTK"


static bool dbg_log_en;
module_param(dbg_log_en, bool, 0644);

enum rt9460_chgtyp {
	RT9460_CHGTYP_SDP = 0,
	RT9460_CHGTYP_SONY1,
	RT9460_CHGTYP_SONY2,
	RT9460_CHGTYP_APPLE_0_5A,
	RT9460_CHGTYP_APPLE_1A,
	RT9460_CHGTYP_NIKON_1A,
	RT9460_CHGTYP_CDP,
	RT9460_CHGTYP_DCP,
	RT9460_CHGTYP_NONE,
	RT9460_CHGTYP_MAX,
};

enum rt9460_charging_stat {
	RT9460_CHG_STAT_READY = 0,
	RT9460_CHG_STAT_PROGRESS,
	RT9460_CHG_STAT_DONE,
	RT9460_CHG_STAT_FAULT,
	RT9460_CHG_STAT_MAX,
};

enum rt9460_usbsw_state {
	RT9460_USBSW_CHG = 0,
	RT9460_USBSW_USB,
	RT9460_USBSW_MAX,
};

enum rt9460_jeita {
	RT9460_JEITA_COLD = 0,
	RT9460_JEITA_COOL,
	RT9460_JEITA_NORMAL,
	RT9460_JEITA_WARM,
	RT9460_JEITA_HOT,
	RT9460_JEITA_MAX,
};

static const char *rt9460_chgtyp_name[RT9460_CHGTYP_MAX] = {
	"SDP", "Sony1", "Sony2", "Apple2.5W", "Apple5W", "Nikon5W", "CDP",
	"DCP", "NONE",
};

static const char *rt9460_usbsw_name[RT9460_USBSW_MAX] = {
	"Charger", "USB",
};

static const char *rt9460_jeita_name[RT9460_JEITA_MAX] = {
	"cold", "cool", "normal", "warm", "hot",
};

#ifdef CONFIG_MTK_CHARGER_CLASS
static const char *rt9460_charging_stat_name[RT9460_CHG_STAT_MAX] = {
	"ready", "progress", "done", "fault",
};
#endif /* CONFIG_MTK_CHARGER_CLASS */

static const u8 rt9460_irq_maskall[RT9460_IRQ_REGNUM] = {
	0xf7, 0xff, 0xe0, 0xfc
};

struct rt9460_info {
	struct device *dev;
	struct i2c_client *i2c;
#ifdef CONFIG_RT_REGMAP
	struct rt_regmap_device *regmap_dev;
	struct rt_regmap_properties *regmap_props;
#endif /* #ifdef CONFIG_RT_REGMAP */
#ifdef CONFIG_MTK_CHARGER_CLASS
	struct charger_device *chg_dev;
#endif /* CONFIG_MTK_CHARGER_CLASS */
	struct mutex io_lock;
	int irq;
	u8 irq_mask[RT9460_IRQ_REGNUM];
	wait_queue_head_t waitq;
	struct task_struct *bc12_thread;
	atomic_t pwr_rdy;
	atomic_t pwr_change;
	struct mutex bc12_lock;
	enum rt9460_chgtyp chgtyp;
	enum rt9460_jeita jeita;
	enum rt9460_usbsw_state usbsw;
	bool dischg;
	struct mutex dischg_lock;
};

static const struct rt9460_platform_data rt9460_def_platform_data = {
	.chg_name = "primary_chg",
	.ichg = 2000000, /* unit: uA */
	.aicr = 500000, /* unit: uA */
	.mivr = 4500000, /* unit: uV */
	.cv = 4350000, /* unit : uV */
	.ieoc = 250000, /* unit: uA */
	.vprec = 2400000, /* unit: uV */
	.iprec = 250000, /* unit: uA */
	.vboost = 5000000, /* unit: uV */
	.wt_fc = 0x4,
	.wt_prc = 0x0,
	.twdt = 0x3,
	.eoc_timer = 0x0,
	.intr_gpio = -1,
};

#ifdef CONFIG_RT_REGMAP
RT_REG_DECL(RT9460_REG_CTRL1, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9460_REG_CTRL2, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9460_REG_CTRL3, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9460_REG_DEVID, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9460_REG_CTRL4, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9460_REG_CTRL5, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9460_REG_CTRL6, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9460_REG_CTRL7, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9460_REG_IRQ1, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9460_REG_IRQ2, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9460_REG_IRQ3, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9460_REG_MASK1, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9460_REG_MASK2, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9460_REG_MASK3, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9460_REG_CTRLDPDM, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9460_REG_CTRL8, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9460_REG_CTRL9, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9460_REG_CTRL10, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9460_REG_CTRL11, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9460_REG_CTRL12, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9460_REG_CTRL13, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9460_REG_STATIRQ, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9460_REG_STATMASK, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9460_REG_HIDDEN1, 1, RT_VOLATILE, {});

static const rt_register_map_t rt9460_register_map[] = {
	RT_REG(RT9460_REG_CTRL1),
	RT_REG(RT9460_REG_CTRL2),
	RT_REG(RT9460_REG_CTRL3),
	RT_REG(RT9460_REG_DEVID),
	RT_REG(RT9460_REG_CTRL4),
	RT_REG(RT9460_REG_CTRL5),
	RT_REG(RT9460_REG_CTRL6),
	RT_REG(RT9460_REG_CTRL7),
	RT_REG(RT9460_REG_IRQ1),
	RT_REG(RT9460_REG_IRQ2),
	RT_REG(RT9460_REG_IRQ3),
	RT_REG(RT9460_REG_MASK1),
	RT_REG(RT9460_REG_MASK2),
	RT_REG(RT9460_REG_MASK3),
	RT_REG(RT9460_REG_CTRLDPDM),
	RT_REG(RT9460_REG_CTRL8),
	RT_REG(RT9460_REG_CTRL9),
	RT_REG(RT9460_REG_CTRL10),
	RT_REG(RT9460_REG_CTRL11),
	RT_REG(RT9460_REG_CTRL12),
	RT_REG(RT9460_REG_CTRL13),
	RT_REG(RT9460_REG_STATIRQ),
	RT_REG(RT9460_REG_STATMASK),
	RT_REG(RT9460_REG_HIDDEN1),
};

#define RT9460_REGISTER_NUM ARRAY_SIZE(rt9460_register_map)

static const struct rt_regmap_properties rt9460_regmap_props = {
	.aliases = "rt9460",
	.register_num = RT9460_REGISTER_NUM,
	.rm = rt9460_register_map,
	.rt_regmap_mode = RT_SINGLE_BYTE,
};

static int rt9460_io_write(void *i2c, u32 addr, int len, const void *src)
{
	return i2c_smbus_write_i2c_block_data(i2c, addr, len, src);
}

static int rt9460_io_read(void *i2c, u32 addr, int len, void *dst)
{
	return i2c_smbus_read_i2c_block_data(i2c, addr, len, dst);
}

static struct rt_regmap_fops rt9460_regmap_fops = {
	.read_device = rt9460_io_read,
	.write_device = rt9460_io_write,
};
#else
#define rt9460_io_write(i2c, addr, len, src) \
	i2c_smbus_write_i2c_block_data(i2c, addr, len, src)
#define rt9460_io_read(i2c, addr, len, dst) \
	i2c_smbus_read_i2c_block_data(i2c, addr, len, dst)
#endif /* #ifdef CONFIG_RT_REGMAP */

/* common i2c transfer function ++ */
static int rt9460_reg_write(struct rt9460_info *ri, u8 reg, u8 data)
{
#ifdef CONFIG_RT_REGMAP
	struct rt_reg_data rrd = {0};

	return rt_regmap_reg_write(ri->regmap_dev, &rrd, reg, data);
#else
	int ret = 0;

	mutex_lock(&ri->io_lock);
	ret = rt9460_io_write(ri->i2c, reg, 1, &data);
	mutex_unlock(&ri->io_lock);
	return 0;
#endif /* #ifdef CONFIG_RT_REGMAP */
}

static int rt9460_reg_block_write(struct rt9460_info *ri, u8 reg,
				  int len, const void *src)
{
#ifdef CONFIG_RT_REGMAP
	return rt_regmap_block_write(ri->regmap_dev, reg, len, src);
#else
	int ret = 0;

	mutex_lock(&ri->io_lock);
	ret = rt9460_io_write(ri->i2c, reg, len, src);
	mutex_unlock(&ri->io_lock);
	return ret;
#endif /* #ifdef CONFIG_RT_REGMAP */
}

static int rt9460_reg_read(struct rt9460_info *ri, u8 reg)
{
#ifdef CONFIG_RT_REGMAP
	struct rt_reg_data rrd = {0};
	int ret = 0;

	ret = rt_regmap_reg_read(ri->regmap_dev, &rrd, reg);
	return (ret < 0 ? ret : rrd.rt_data.data_u32);
#else
	u8 data = 0;
	int ret = 0;

	mutex_lock(&ri->io_lock);
	ret = rt9460_io_read(ri->i2c, reg, 1, &data);
	mutex_unlock(&ri->io_lock);
	return (ret < 0) ? ret : data;
#endif /* #ifdef CONFIG_RT_REGMAP */
}

static int rt9460_reg_block_read(struct rt9460_info *ri, u8 reg,
				 int len, void *dst)
{
#ifdef CONFIG_RT_REGMAP
	return rt_regmap_block_read(ri->regmap_dev, reg, len, dst);
#else
	int ret = 0;

	mutex_lock(&ri->io_lock);
	ret = rt9460_io_read(ri->i2c, reg, len, dst);
	mutex_unlock(&ri->io_lock);
	return ret;
#endif /* #ifdef CONFIG_RT_REGMAP */
}

static int rt9460_reg_assign_bits(struct rt9460_info *ri,
				  u8 reg, u8 mask, u8 data)
{
#ifdef CONFIG_RT_REGMAP
	struct rt_reg_data rrd = {0};

	return rt_regmap_update_bits(ri->regmap_dev, &rrd, reg, mask, data);
#else
	u8 orig_data = 0;
	int ret = 0;

	mutex_lock(&ri->io_lock);
	ret = rt9460_io_read(ri->i2c, reg, 1, &orig_data);
	if (ret < 0)
		goto out_unlock;
	orig_data &= (~mask);
	orig_data |= (data & mask);
	ret = rt9460_io_write(ri->i2c, reg, 1, &orig_data);
out_unlock:
	mutex_unlock(&ri->io_lock);
	return ret;
#endif /* #ifdef CONFIG_RT_REGMAP */
}

static inline int rt9460_reg_set_bits(struct rt9460_info *ri, u8 reg, u8 mask)
{
	return rt9460_reg_assign_bits(ri, reg, mask, mask);
}

static inline int rt9460_reg_clr_bits(struct rt9460_info *ri, u8 reg, u8 mask)
{
	return rt9460_reg_assign_bits(ri, reg, mask, 0x00);
}
/* common i2c transfer function -- */

/* ================== */
/* Internal Functions */
/* ================== */
static inline int __rt9460_get_ichg(struct rt9460_info *ri, u32 *uA)
{
	int ret = 0;

	ret = rt9460_reg_read(ri, RT9460_REG_CTRL6);
	if (ret < 0)
		return ret;
	ret = (ret & RT9460_ICHG_MASK) >> RT9460_ICHG_SHFT;
	if (ret > RT9460_ICHG_MAX)
		ret = RT9460_ICHG_MAX;
	*uA = 1250000 + (ret * 125000);
	return 0;
}

static inline int __rt9460_set_ichg(struct rt9460_info *ri, u32 uA)
{
	u8 reg_data = 0;

	dev_dbg(ri->dev, "%s: ichg = %u\n", __func__, uA);
	if (uA >= 1250000) {
		reg_data = (uA - 1250000) / 125000;
		if (reg_data > RT9460_ICHG_MAX)
			reg_data = RT9460_ICHG_MAX;
	}
	return rt9460_reg_assign_bits(ri, RT9460_REG_CTRL6, RT9460_ICHG_MASK,
				      reg_data << RT9460_ICHG_SHFT);
}

static inline int __rt9460_get_aicr(struct rt9460_info *ri, u32 *uA)
{
	u8 reg_data = 0;
	int ret = 0;

	ret = rt9460_reg_read(ri, RT9460_REG_CTRL11);
	if (ret < 0)
		return ret;
	reg_data = (ret & RT9460_IAICR_MASK) >> RT9460_IAICR_SHFT;
	if (reg_data >= RT9460_IAICR_MAX)
		*uA = UINT_MAX;
	else if (reg_data >= 0x06 && reg_data < RT9460_IAICR_MAX)
		*uA = reg_data * 100000;
	else if (reg_data >= 0x04 && reg_data < 0x06)
		*uA = 500000;
	else if (reg_data >= 0x02 && reg_data < 0x04)
		*uA = 150000;
	else
		*uA = 100000;
	return 0;
}

static inline int __rt9460_set_aicr(struct rt9460_info *ri, u32 uA)
{
	u8 reg_data = 0;
	int ret = 0;

	dev_dbg(ri->dev, "%s: aicr = %u\n", __func__, uA);
	if (uA > 3000000) {
		dev_warn(ri->dev, "aicr over 3A, will set to disable state\n");
		reg_data = RT9460_IAICR_MAX;
	} else if (uA >= 600000 && uA <= 3000000) {
		/* from 600mA start, step is 100mA */
		reg_data = uA / 100000;
	} else if (uA >= 500000 && uA < 600000) {
		/* config to 500mA */
		reg_data = 0x04;
	} else if (uA >= 150000 && uA < 500000) {
		/* config to 150mA */
		reg_data = 0x02;
	} else {
		/* config to minimum 100mA */
		reg_data = 0x00;
	}
	/* config to only AICR */
	ret = rt9460_reg_assign_bits(ri, RT9460_REG_CTRLDPDM,
				     RT9460_IINLMTSEL_MASK,
				     0x01 << RT9460_IINLMTSEL_SHFT);
	if (ret < 0)
		dev_err(ri->dev, "%s: config iinlmtsel fail\n", __func__);
	/*  bypass otg pin decide */
	ret = rt9460_reg_assign_bits(ri, RT9460_REG_CTRL2,
				     RT9460_IININT_MASK, 0xff);
	if (ret < 0)
		dev_err(ri->dev, "%s: config iinint fail\n", __func__);
	return rt9460_reg_assign_bits(ri, RT9460_REG_CTRL11, RT9460_IAICR_MASK,
				      reg_data << RT9460_IAICR_SHFT);
}

static const u32 rt9460_support_mivr_lvl[] = {
	4000000, 4250000, 4500000, 4750000, 7000000, 7500000, 8000000, 8500000,
	9500000, 10000000, 10500000, 11000000,
};

static inline int __rt9460_get_mivr(struct rt9460_info *ri, u32 *uV)
{
	int ret = 0;

	ret = rt9460_reg_read(ri, RT9460_REG_CTRL9);
	if (ret < 0)
		return ret;
	ret = (ret & RT9460_MIVRLVL_MASK) >> RT9460_MIVRLVL_SHFT;
	if (ret > RT9460_MIVRLVL_MAX)
		ret = RT9460_MIVRLVL_MAX;
	*uV = rt9460_support_mivr_lvl[ret];
	return 0;
}

static inline int __rt9460_set_mivr(struct rt9460_info *ri, u32 uV)
{
	u8 reg_data = 0;

	if (uV >= 9500000 && uV <= 11000000)
		reg_data = DIV_ROUND_UP(uV - 9500000, 500000) + 8;
	else if (uV > 8500000 && uV < 9500000)
		reg_data = 0x07;
	else if (uV >= 7000000 && uV <= 8500000)
		reg_data = DIV_ROUND_UP(uV - 7000000, 500000) + 4;
	else if (uV > 4750000 && uV < 7000000)
		reg_data = 0x03;
	else if (uV >= 4000000 && uV <= 4750000)
		reg_data = DIV_ROUND_UP(uV - 4000000, 250000);
	else
		reg_data = 0x00;
	dev_info(ri->dev, "%s: mivr = %u, 0x%02X\n", __func__, uV, reg_data);
	return rt9460_reg_assign_bits(ri, RT9460_REG_CTRL9, RT9460_MIVRLVL_MASK,
				      reg_data << RT9460_MIVRLVL_SHFT);
}

static inline int __rt9460_force_discharge(struct rt9460_info *ri, bool dischg)
{
	int ret;

	mutex_lock(&ri->dischg_lock);
	dev_info(ri->dev, "%s en = %d\n", __func__, dischg);
	ri->dischg = dischg;
	ret = __rt9460_set_mivr(ri, dischg ? 11000000 : 4500000);
	mutex_unlock(&ri->dischg_lock);
	return ret;
}

static inline int __rt9460_get_cv(struct rt9460_info *ri, u32 *uV)
{
	int ret = 0;

	ret = rt9460_reg_read(ri, RT9460_REG_CTRL3);
	if (ret < 0)
		return ret;
	ret = (ret & RT9460_VOREG_MASK) >> RT9460_VOREG_SHFT;
	if (ret > RT9460_VOREG_MAX)
		ret = RT9460_VOREG_MAX;
	*uV = 3500000 + 20000 * ret;
	return 0;
}

static inline int __rt9460_set_cv(struct rt9460_info *ri, u32 uV)
{
	u8 reg_data = 0;

	dev_dbg(ri->dev, "%s: cv = %u\n", __func__, uV);
	if (uV >= 3500000) {
		reg_data = (uV - 3500000) / 20000;
		if (reg_data > RT9460_VOREG_MAX)
			reg_data = RT9460_VOREG_MAX;
	}
	return rt9460_reg_assign_bits(ri, RT9460_REG_CTRL3, RT9460_VOREG_MASK,
				      reg_data << RT9460_VOREG_SHFT);
}

static inline int __rt9460_get_ieoc(struct rt9460_info *ri, u32 *uA)
{
	int ret = 0;

	ret = rt9460_reg_read(ri, RT9460_REG_CTRL2);
	if (ret < 0)
		return ret;
	ret = (ret & RT9460_IEOC_MASK) >> RT9460_IEOC_SHFT;
	if (ret > RT9460_IEOC_MAX)
		ret = RT9460_IEOC_MAX;
	*uA = 100000 + 50000 * ret;
	return 0;
}

static inline int __rt9460_set_ieoc(struct rt9460_info *ri, u32 uA)
{
	u8 reg_data = 0;

	dev_dbg(ri->dev, "%s: ieoc = %u\n", __func__, uA);
	if (uA >= 100000) {
		reg_data = DIV_ROUND_UP(uA - 100000, 50000);
		if (reg_data > RT9460_IEOC_MAX)
			reg_data = RT9460_IEOC_MAX;
	}

	return rt9460_reg_assign_bits(ri, RT9460_REG_CTRL2, RT9460_IEOC_MASK,
				      reg_data << RT9460_IEOC_SHFT);
}

static inline int __rt9460_set_vprec(struct rt9460_info *ri, u32 uV)
{
	u8 reg_data = 0;

	dev_dbg(ri->dev, "%s: vprec = %u\n", __func__, uV);
	if (uV >= 2000000) {
		reg_data = (uV - 2000000) / 200000;
		if (reg_data > RT9460_VPREC_MAX)
			reg_data = RT9460_VPREC_MAX;
	}
	return rt9460_reg_assign_bits(ri, RT9460_REG_CTRL6, RT9460_VPREC_MASK,
				      reg_data << RT9460_VPREC_SHFT);
}

static inline int __rt9460_set_iprec(struct rt9460_info *ri, u32 uA)
{
	u8 reg_data = 0;

	dev_dbg(ri->dev, "%s: iprec = %u\n", __func__, uA);
	if (uA >= 100000) {
		reg_data = (uA - 100000) / 50000;
		if (reg_data > RT9460_IPREC_MAX)
			reg_data = RT9460_IPREC_MAX;
	}
	return rt9460_reg_assign_bits(ri, RT9460_REG_CTRL5, RT9460_IPREC_MASK,
				      reg_data << RT9460_IPREC_SHFT);
}

static inline int __rt9460_set_vboost(struct rt9460_info *ri, u32 uV)
{
	u8 reg_data = 0;

	dev_dbg(ri->dev, "%s: vboost = %u\n", __func__, uV);
	if (uV >= 4425000) {
		reg_data = (uV - 4425000) / 25000;
		if (reg_data > RT9460_VOREG_MAX)
			reg_data = RT9460_VOREG_MAX;
	}
	return rt9460_reg_assign_bits(ri, RT9460_REG_CTRL3, RT9460_VOREG_MASK,
				      reg_data << RT9460_VOREG_SHFT);
}

static inline int __rt9460_set_wt_fc(struct rt9460_info *ri, u32 sel)
{
	u8 reg_data = 0;

	dev_dbg(ri->dev, "%s: wt_fc = %u\n", __func__, sel);
	if (sel > RT9460_WTFC_MAX)
		reg_data = RT9460_WTFC_MAX;
	else
		reg_data = sel;
	return rt9460_reg_assign_bits(ri, RT9460_REG_CTRL10, RT9460_WTFC_MASK,
				      reg_data << RT9460_WTFC_SHFT);
}

static inline int __rt9460_set_wt_prc(struct rt9460_info *ri, u32 sel)
{
	u8 reg_data = 0;

	dev_dbg(ri->dev, "%s: wt_prc = %u\n", __func__, sel);
	if (sel > RT9460_WTPRC_MAX)
		reg_data = RT9460_WTPRC_MAX;
	else
		reg_data = sel;
	return rt9460_reg_assign_bits(ri, RT9460_REG_CTRL10, RT9460_WTPRC_MASK,
				      reg_data << RT9460_WTPRC_SHFT);
}

static inline int __rt9460_set_twdt(struct rt9460_info *ri, u32 sel)
{
	u8 reg_data = 0;

	dev_dbg(ri->dev, "%s: twdt = %u\n", __func__, sel);
	if (sel > RT9460_TWDT_MAX)
		reg_data = RT9460_TWDT_MAX;
	else
		reg_data = sel;
	return rt9460_reg_assign_bits(ri, RT9460_REG_CTRL13, RT9460_TWDT_MASK,
				      reg_data << RT9460_TWDT_SHFT);
}

static inline int __rt9460_set_eoc_timer(struct rt9460_info *ri, u32 sel)
{
	u8 reg_data = 0;

	dev_dbg(ri->dev, "%s: eoc_timer = %u\n", __func__, sel);
	if (sel > RT9460_EOCTIMER_MAX)
		reg_data = RT9460_EOCTIMER_MAX;
	else
		reg_data = sel;
	return rt9460_reg_assign_bits(ri, RT9460_REG_CTRL12,
				      RT9460_EOCTIMER_MASK,
				      reg_data << RT9460_EOCTIMER_SHFT);
}

static inline int __rt9460_set_te_enable(struct rt9460_info *ri, bool en)
{
	dev_dbg(ri->dev, "%s: en = %u\n", __func__, en);
	return rt9460_reg_assign_bits(ri, RT9460_REG_CTRL2, RT9460_TEEN_MASK,
				      en ? 0xff : 0);
}

static inline int __rt9460_set_mivr_disable(struct rt9460_info *ri, bool dis)
{
	dev_dbg(ri->dev, "%s: dis = %u\n", __func__, dis);
	return rt9460_reg_assign_bits(ri, RT9460_REG_CTRL9, RT9460_MIVRDIS_MASK,
				      dis ? 0xff : 0);
}

static inline int __rt9460_set_ccjeita_enable(struct rt9460_info *ri, bool en)
{
	dev_dbg(ri->dev, "%s: en = %u\n", __func__, en);
	return rt9460_reg_assign_bits(ri, RT9460_REG_CTRL7,
				      RT9460_CCJEITAEN_MASK, en ? 0xff : 0);
}

static inline int __rt9460_set_batd_enable(struct rt9460_info *ri, bool en)
{
	dev_dbg(ri->dev, "%s: en = %u\n", __func__, en);
	return rt9460_reg_assign_bits(ri, RT9460_REG_CTRL7, RT9460_BATDEN_MASK,
				      en ? 0xff : 0);
}

static
inline int __rt9460_set_irq_pulse_enable(struct rt9460_info *ri, bool en)
{
	dev_dbg(ri->dev, "%s: en = %u\n", __func__, en);
	return rt9460_reg_assign_bits(ri, RT9460_REG_CTRL12,
				      RT9460_IRQPULSE_MASK, en ? 0xff : 0);
}

static inline int __rt9460_set_wdt_enable(struct rt9460_info *ri, bool en)
{
	dev_dbg(ri->dev, "%s: en = %u\n", __func__, en);
	return rt9460_reg_assign_bits(ri, RT9460_REG_CTRL13, RT9460_WDTEN_MASK,
				      en ? 0xff : 0);
}

static
inline int __rt9460_set_safetimer_enable(struct rt9460_info *ri, bool en)
{
	dev_dbg(ri->dev, "%s: en = %u\n", __func__, en);
	return rt9460_reg_assign_bits(ri, RT9460_REG_CTRL10,
				      RT9460_TMRPAUSE_MASK, en ? 0 : 0xff);
}

static inline int __rt9460_set_chg_enable(struct rt9460_info *ri, bool en)
{
	struct rt9460_platform_data *pdata = dev_get_platdata(ri->dev);
	int ret = 0;

	dev_dbg(ri->dev, "%s: en = %u\n", __func__, en);
	/* set HZ mode of secondary charger */
	if (strcmp(pdata->chg_name, "secondary_chg") == 0) {
		ret = rt9460_reg_assign_bits(ri, RT9460_REG_CTRL2,
					     RT9460_HZ_MASK, en ? 0xff : 0);
		if (ret < 0)
			dev_err(ri->dev, "%s: set hz of sec chg fail\n",
				__func__);
	}
	return rt9460_reg_assign_bits(ri, RT9460_REG_CTRL7, RT9460_CHGEN_MASK,
				      en ? 0xff : 0);
}

static inline int __rt9460_get_charging_stat(struct rt9460_info *ri,
	enum rt9460_charging_stat *chg_stat)
{
	int ret = 0;

	ret = rt9460_reg_read(ri, RT9460_REG_CTRL1);
	if (ret < 0)
		return ret;
	*chg_stat = (ret & RT9460_STAT_MASK) >> RT9460_STAT_SHFT;
	return 0;
}

#ifdef CONFIG_MTK_CHARGER_CLASS
static int rt9460_plug_in(struct charger_device *chg_dev)
{
	struct rt9460_info *ri = charger_get_data(chg_dev);
	struct rt9460_platform_data *pdata = dev_get_platdata(ri->dev);
	int ret = 0;

	dev_dbg(ri->dev, "%s\n", __func__);
	/* Enable WDT */
	ret = (pdata->enable_wdt) ? __rt9460_set_wdt_enable(ri, true) : 0;
	if (ret < 0)
		dev_err(ri->dev, "%s: set wdt enable fail\n", __func__);
	/* Whatever current cv value is, reset orig_cv first before te enable */
	ret = __rt9460_set_cv(ri, pdata->cv);
	if (ret < 0)
		dev_err(ri->dev, "%s: set cv fail\n", __func__);
	return __rt9460_set_te_enable(ri, true);
}

static int rt9460_plug_out(struct charger_device *chg_dev)
{
	struct rt9460_info *ri = charger_get_data(chg_dev);
	struct rt9460_platform_data *pdata = dev_get_platdata(ri->dev);
	int ret = 0;

	dev_dbg(ri->dev, "%s\n", __func__);
	ret =  __rt9460_set_te_enable(ri, false);
	if (ret < 0)
		dev_err(ri->dev, "%s: set te enable fail\n", __func__);
	/* Disable WDT */
	ret = (pdata->enable_wdt) ? __rt9460_set_wdt_enable(ri, false) : 0;
	if (ret < 0)
		dev_err(ri->dev, "%s: set wdt enable fail\n", __func__);
	return ret;
}

static int rt9460_is_charging_done(struct charger_device *chg_dev,
					   bool *done)
{
	enum rt9460_charging_stat chg_stat = RT9460_CHG_STAT_READY;
	struct rt9460_info *ri = charger_get_data(chg_dev);
	int ret = 0;

	dev_dbg(ri->dev, "%s\n", __func__);
	ret = __rt9460_get_charging_stat(ri, &chg_stat);
	if (ret < 0) {
		dev_err(ri->dev, "%s: get chgstat fail\n", __func__);
		return ret;
	}
	/* Return is charging done or not */
	switch (chg_stat) {
	case RT9460_CHG_STAT_DONE:
		*done = true;
		break;
	case RT9460_CHG_STAT_READY:
	case RT9460_CHG_STAT_PROGRESS:
	case RT9460_CHG_STAT_FAULT:
	default:
		*done = false;
		break;
	}
	return 0;
}

static int rt9460_kick_wdt(struct charger_device *chg_dev)
{
	struct rt9460_info *ri = charger_get_data(chg_dev);
	struct rt9460_platform_data *pdata = dev_get_platdata(ri->dev);
	int ret = 0;

	dev_dbg(ri->dev, "%s\n", __func__);
	if (!pdata->enable_wdt)
		return 0;
	/* two way to reset watchdog timer, 1: toggle wdt 2: any i2c comm */
	ret = __rt9460_set_wdt_enable(ri, false);
	if (ret < 0)
		dev_err(ri->dev, "set wdt enable fail\n");
	return __rt9460_set_wdt_enable(ri, true);
}

static int rt9460_enable_otg(struct charger_device *chg_dev, bool en)
{
	struct rt9460_info *ri = charger_get_data(chg_dev);
	struct rt9460_platform_data *pdata = dev_get_platdata(ri->dev);
	int ret = 0;

	dev_dbg(ri->dev, "%s: en = %u\n", __func__, en);
	/* clear HZ mode */
	ret = rt9460_reg_clr_bits(ri, RT9460_REG_CTRL2, RT9460_HZ_MASK);
	if (ret < 0)
		dev_err(ri->dev, "force clear HZ fail\n");
	ret = en ? __rt9460_set_vboost(ri, pdata->vboost) :
	      __rt9460_set_cv(ri, pdata->cv);
	if (ret < 0)
		dev_err(ri->dev, "set_vboost or set_cv fail\n");
	return rt9460_reg_assign_bits(ri, RT9460_REG_CTRL2, RT9460_OPAMODE_MASK,
				      en ? 0xff : 0x00);
}

static int rt9460_enable_te(struct charger_device *chg_dev, bool en)
{
	struct rt9460_info *ri = charger_get_data(chg_dev);
	struct rt9460_platform_data *pdata = dev_get_platdata(ri->dev);

	dev_dbg(ri->dev, "%s: en = %u\n", __func__, en);
	/* if not specified to enable te function, bypass it */
	if (!pdata->enable_te)
		return 0;
	return __rt9460_set_te_enable(ri, en);
}

static int rt9460_enable_safety_timer(struct charger_device *chg_dev, bool en)
{
	struct rt9460_info *ri = charger_get_data(chg_dev);
	struct rt9460_platform_data *pdata = dev_get_platdata(ri->dev);

	dev_dbg(ri->dev, "%s: en = %u\n", __func__, en);
	/* if not specified to enable safety timer function, bypass it */
	if (!pdata->enable_safetimer)
		return 0;
	return __rt9460_set_safetimer_enable(ri, en);
}

static int rt9460_is_safety_timer_enabled(struct charger_device *chg_dev,
					   bool *en)
{
	struct rt9460_info *ri = charger_get_data(chg_dev);
	int ret = 0;

	ret = rt9460_reg_read(ri, RT9460_REG_CTRL10);
	if (ret < 0)
		return ret;
	*en = (ret & RT9460_TMRPAUSE_MASK) ? false : true;
	return 0;
}

static int rt9460_enable_powerpath(struct charger_device *chg_dev, bool en)
{
	struct rt9460_info *ri = charger_get_data(chg_dev);

	dev_dbg(ri->dev, "%s: en = %u\n", __func__, en);
	return rt9460_reg_assign_bits(ri, RT9460_REG_CTRL7, RT9460_CHIPEN_MASK,
				      en ? 0xff : 0x00);
}

static int rt9460_is_powerpath_enabled(struct charger_device *chg_dev, bool *en)
{
	struct rt9460_info *ri = charger_get_data(chg_dev);
	int ret = 0;

	ret = rt9460_reg_read(ri, RT9460_REG_CTRL7);
	if (ret < 0)
		return ret;
	*en = (ret & RT9460_CHIPEN_MASK) ? true : false;
	return 0;
}


static int rt9460_set_mivr(struct charger_device *chg_dev, u32 uV)
{
	int ret = 0;
	struct rt9460_info *ri = charger_get_data(chg_dev);

	mutex_lock(&ri->dischg_lock);
	if (ri->dischg) {
		dev_info(ri->dev, "%s dischg, can't modify mivr\n", __func__);
		goto out;
	}
	dev_dbg(ri->dev, "%s: mivr = %u\n", __func__, uV);
	ret = __rt9460_set_mivr(ri, uV);
out:
	mutex_unlock(&ri->dischg_lock);
	return ret;
}

static int rt9460_get_min_aicr(struct charger_device *chg_dev, u32 *uA)
{
	*uA = 100000;
	return 0;
}

static int rt9460_get_aicr(struct charger_device *chg_dev, u32 *uA)
{
	struct rt9460_info *ri = charger_get_data(chg_dev);

	return __rt9460_get_aicr(ri, uA);
}

static int rt9460_set_aicr(struct charger_device *chg_dev, u32 uA)
{
	struct rt9460_info *ri = charger_get_data(chg_dev);

	dev_dbg(ri->dev, "%s: aicr = %u\n", __func__, uA);
	return __rt9460_set_aicr(ri, uA);
}

static int rt9460_get_cv(struct charger_device *chg_dev, u32 *uV)
{
	struct rt9460_info *ri = charger_get_data(chg_dev);

	return __rt9460_get_cv(ri, uV);
}

static int rt9460_set_cv(struct charger_device *chg_dev, u32 uV)
{
	struct rt9460_info *ri = charger_get_data(chg_dev);

	dev_dbg(ri->dev, "%s: cv = %u\n", __func__, uV);
	return __rt9460_set_cv(ri, uV);
}

static int rt9460_get_min_ichg(struct charger_device *chg_dev, u32 *uA)
{
	*uA = 1250000;
	return 0;
}

static int rt9460_get_ichg(struct charger_device *chg_dev, u32 *uA)
{
	struct rt9460_info *ri = charger_get_data(chg_dev);

	return __rt9460_get_ichg(ri, uA);
}

static int rt9460_set_ichg(struct charger_device *chg_dev, u32 uA)
{
	struct rt9460_info *ri = charger_get_data(chg_dev);

	dev_dbg(ri->dev, "%s: ichg = %u\n", __func__, uA);
	return __rt9460_set_ichg(ri, uA);
}

static int rt9460_set_ieoc(struct charger_device *chg_dev, u32 uA)
{
	struct rt9460_info *ri = charger_get_data(chg_dev);

	dev_dbg(ri->dev, "%s: ieoc = %u\n", __func__, uA);
	return __rt9460_set_ieoc(ri, uA);
}

static int rt9460_enable_charging(struct charger_device *chg_dev, bool en)
{
	struct rt9460_info *ri = charger_get_data(chg_dev);

	dev_dbg(ri->dev, "%s: en = %u\n", __func__, en);
	return __rt9460_set_chg_enable(ri, en);
}

static int rt9460_is_charging_enabled(struct charger_device *chg_dev, bool *en)
{
	struct rt9460_info *ri = charger_get_data(chg_dev);
	int ret = 0;

	ret = rt9460_reg_read(ri, RT9460_REG_CTRL7);
	if (ret < 0)
		return ret;
	*en = (ret & RT9460_CHGEN_MASK) ? true : false;
	return 0;
}

static int rt9460_dump_registers(struct charger_device *chg_dev)
{
	struct rt9460_info *ri = charger_get_data(chg_dev);
	int i = 0, ret = 0;
	u32 ichg = 0, aicr = 0, mivr = 0, ieoc = 0, cv = 0;
	enum rt9460_charging_stat chg_stat = RT9460_CHG_STAT_READY;
	bool chg_en = 0;

	ret = __rt9460_get_ichg(ri, &ichg);
	if (ret < 0)
		dev_err(ri->dev, "%s: get ichg fail\n", __func__);
	ret = __rt9460_get_aicr(ri, &aicr);
	if (ret < 0)
		dev_err(ri->dev, "%s: get aicr fail\n", __func__);
	ret = __rt9460_get_mivr(ri, &mivr);
	if (ret < 0)
		dev_err(ri->dev, "%s: get cv fail\n", __func__);
	ret = __rt9460_get_ieoc(ri, &ieoc);
	if (ret < 0)
		dev_err(ri->dev, "%s: get mivr fail\n", __func__);
	ret = __rt9460_get_cv(ri, &cv);
	if (ret < 0)
		dev_err(ri->dev, "%s: get ieoc fail\n", __func__);
	ret = rt9460_is_charging_enabled(chg_dev, &chg_en);
	if (ret < 0)
		dev_err(ri->dev, "%s: get charger enabled fail\n", __func__);
	ret = __rt9460_get_charging_stat(ri, &chg_stat);
	if (ret < 0)
		dev_err(ri->dev, "%s: get charging status fail\n", __func__);

	if (dbg_log_en) {
		for (i = RT9460_REG_CTRL1; i <= RT9460_REG_STATMASK; i++) {
			/* byapss irq event register */
			if ((i >= RT9460_REG_IRQ1 && i <= RT9460_REG_IRQ3) ||
			     i == RT9460_REG_STATIRQ)
				continue;
			/* bypass not existed register */
			if (i > RT9460_REG_CTRLDPDM && i < RT9460_REG_CTRL8)
				continue;
			if (i > RT9460_REG_CTRL8 && i < RT9460_REG_CTRL9)
				continue;
			ret = rt9460_reg_read(ri, i);
			if (ret < 0)
				continue;
			dev_info(ri->dev, "[0x%02X] : 0x%02x\n", i, ret);
		}
	}
	dev_info(ri->dev,
		 "%s: ICHG = %umA, AICR = %umA, MIVR = %umV, IEOC = %umA\n",
		 __func__, ichg / 1000, aicr / 1000, mivr / 1000, ieoc / 1000);

	dev_info(ri->dev,
		 "%s: CV = %umV, CHG_EN = %u, CHG_STATUS = %s\n",
		 __func__, cv / 1000, chg_en,
		 rt9460_charging_stat_name[chg_stat]);
	return 0;
}

static int rt9460_do_event(struct charger_device *chg_dev, u32 event, u32 args)
{
	struct rt9460_info *ri = charger_get_data(chg_dev);

	switch (event) {
	case EVENT_EOC:
		dev_info(ri->dev, "do eoc event\n");
		charger_dev_notify(ri->chg_dev, CHARGER_DEV_NOTIFY_EOC);
		break;
	case EVENT_RECHARGE:
		dev_info(ri->dev, "do recharge event\n");
		charger_dev_notify(ri->chg_dev, CHARGER_DEV_NOTIFY_RECHG);
		break;
	default:
		break;
	}
	return 0;
}

static const struct charger_ops rt9460_chg_ops = {
	/* cable plug in/out */
	.plug_in = rt9460_plug_in,
	.plug_out = rt9460_plug_out,
	/* enable */
	.enable = rt9460_enable_charging,
	.is_enabled = rt9460_is_charging_enabled,
	/* charging current */
	.get_charging_current = rt9460_get_ichg,
	.set_charging_current = rt9460_set_ichg,
	.get_min_charging_current = rt9460_get_min_ichg,
	/* input current */
	.get_input_current = rt9460_get_aicr,
	.set_input_current = rt9460_set_aicr,
	.get_min_input_current = rt9460_get_min_aicr,
	/* charging voltage */
	.get_constant_voltage = rt9460_get_cv,
	.set_constant_voltage = rt9460_set_cv,
	/* MIVR */
	.set_mivr = rt9460_set_mivr,
	/* EOC */
	.set_eoc_current = rt9460_set_ieoc,
	/* kick WDT */
	.kick_wdt = rt9460_kick_wdt,
	/* safety timer */
	.enable_safety_timer = rt9460_enable_safety_timer,
	.is_safety_timer_enabled = rt9460_is_safety_timer_enabled,
	/* power path */
	.enable_powerpath = rt9460_enable_powerpath,
	.is_powerpath_enabled = rt9460_is_powerpath_enabled,
	/* charing termination */
	.enable_termination = rt9460_enable_te,
	/* OTG */
	.enable_otg = rt9460_enable_otg,
	/* misc */
	.is_charging_done = rt9460_is_charging_done,
	.dump_registers = rt9460_dump_registers,
	/* event */
	.event = rt9460_do_event,
};

static const struct charger_properties rt9460_chg_props = {
	.alias_name = "rt9460",
};
#endif /* CONFIG_MTK_CHARGER_CLASS */

#ifdef CONFIG_CHARGER_MTK
static enum charger_type to_mtk_chgtype(enum rt9460_chgtyp rt_chgtyp)
{
	enum charger_type mtk_chgtyp = CHARGER_UNKNOWN;

	switch (rt_chgtyp) {
	case RT9460_CHGTYP_SDP:
		mtk_chgtyp = STANDARD_HOST;
		break;
	case RT9460_CHGTYP_SONY1:
	case RT9460_CHGTYP_SONY2:
	case RT9460_CHGTYP_APPLE_0_5A:
	case RT9460_CHGTYP_APPLE_1A:
	case RT9460_CHGTYP_NIKON_1A:
		mtk_chgtyp = NONSTANDARD_CHARGER;
		break;
	case RT9460_CHGTYP_CDP:
		mtk_chgtyp = CHARGING_HOST;
		break;
	case RT9460_CHGTYP_DCP:
		mtk_chgtyp = STANDARD_CHARGER;
		break;
	case RT9460_CHGTYP_NONE:
		mtk_chgtyp = CHARGER_UNKNOWN;
		break;
	default:
		mtk_chgtyp = NONSTANDARD_CHARGER;
		break;
	}
	return mtk_chgtyp;
}
#endif /* CONFIG_CHARGER_MTK */

static int rt9460_set_usbsw_state(struct rt9460_info *ri,
				  enum rt9460_usbsw_state usbsw)
{
#ifdef CONFIG_CHARGER_MTK
	if (ri->usbsw != usbsw) {
		dev_info(ri->dev, "%s %s\n", __func__,
			 rt9460_usbsw_name[usbsw]);
		if (usbsw == RT9460_USBSW_CHG)
			Charger_Detect_Init();
		else
			Charger_Detect_Release();
		ri->usbsw = usbsw;
	}
#endif /* CONFIG_CHARGER_MTK */

	return 0;
}

static int rt9460_enable_bc12(struct rt9460_info *ri, bool en)
{
	dev_info(ri->dev, "%s %d\n", __func__, en);
	return (en ? rt9460_reg_set_bits : rt9460_reg_clr_bits)
		(ri, RT9460_REG_CTRLDPDM, RT9460_CHGDET_MASK);
}

static int rt9460_report_online(struct rt9460_info *ri)
{
	int ret;
	struct power_supply *chg_psy;
	union power_supply_propval chg_val;

	/* get power supply */
	chg_psy = power_supply_get_by_name("charger");
	if (!chg_psy)
		return -EINVAL;

	/* Online */
	chg_val.intval = atomic_read(&ri->pwr_rdy);
	ret = power_supply_set_property(chg_psy, POWER_SUPPLY_PROP_ONLINE,
					&chg_val);
	if (ret < 0)
		dev_err(ri->dev, "%s report onilne fail(%d)\n", __func__, ret);
	power_supply_put(chg_psy);
	return ret;
}

static int rt9460_report_chgtyp(struct rt9460_info *ri)
{
	int ret;
	struct power_supply *chg_psy;
	union power_supply_propval chg_val;

	/* get power supply */
	chg_psy = power_supply_get_by_name("charger");
	if (!chg_psy)
		return -EINVAL;

	if (ri->chgtyp == RT9460_CHGTYP_SDP ||
	    ri->chgtyp == RT9460_CHGTYP_CDP ||
	    ri->chgtyp == RT9460_CHGTYP_NONE) {
		rt9460_set_usbsw_state(ri, RT9460_USBSW_USB);
		rt9460_enable_bc12(ri, false);
	}
	chg_val.intval = ri->chgtyp;
#ifdef CONFIG_CHARGER_MTK
	chg_val.intval = to_mtk_chgtype(ri->chgtyp);
#endif /* CONFIG_CHARGER_MTK */

	ret = power_supply_set_property(chg_psy, POWER_SUPPLY_PROP_CHARGE_TYPE,
					&chg_val);
	if (ret < 0)
		dev_err(ri->dev, "%s report type fail(%d)\n", __func__, ret);
	power_supply_put(chg_psy);
	return ret;
}

static int rt9460_bc12_post_process(struct rt9460_info *ri)
{
	int i, ret;
	const int max_wait_time = 20;
	enum rt9460_chgtyp chgtyp = RT9460_CHGTYP_NONE;

	mutex_lock(&ri->bc12_lock);
	ret = rt9460_report_online(ri);
	__rt9460_force_discharge(ri, true);
	if (atomic_read(&ri->pwr_rdy)) {
		rt9460_set_usbsw_state(ri, RT9460_USBSW_CHG);
		ret = rt9460_enable_bc12(ri, true);
	} else
		goto out;
	/* Maximum 1s */
	for (i = 0; i < max_wait_time; i++) {
		if (!atomic_read(&ri->pwr_rdy))
			goto out;
		msleep(50);
		ret = rt9460_reg_read(ri, RT9460_REG_CTRLDPDM);
		if (ret < 0)
			continue;
		if (ret & RT9460_CHGRUN_MASK)
			continue;
		chgtyp = (ret & RT9460_CHGTYP_MASK) >> RT9460_CHGTYP_SHFT;
		break;
	}
	__rt9460_force_discharge(ri, false);

out:
	if (ri->chgtyp != chgtyp) {
		dev_info(ri->dev, "%s chgtype %s->%s\n", __func__,
			 rt9460_chgtyp_name[ri->chgtyp],
			 rt9460_chgtyp_name[chgtyp]);
		ri->chgtyp = chgtyp;
		ret = rt9460_report_chgtyp(ri);
	}
	mutex_unlock(&ri->bc12_lock);
	return 0;
}

static int rt9460_bc12_thread_fn(void *data)
{
	struct rt9460_info *ri = data;
	int ret;

	dev_info(ri->dev, "%s\n", __func__);
	while (!kthread_should_stop()) {
		ret = wait_event_interruptible(ri->waitq,
					       atomic_read(&ri->pwr_change));
		if (ret < 0)
			continue;
		atomic_set(&ri->pwr_change, 0);
		rt9460_bc12_post_process(ri);
	}
	return 0;
}

static irqreturn_t rt9460_irq_BATABI_handler(int irq, void *dev_id)
{
	struct rt9460_info *ri = (struct rt9460_info *)dev_id;

	dev_warn(ri->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t rt9460_irq_SYSUVPI_handler(int irq, void *dev_id)
{
	struct rt9460_info *ri = (struct rt9460_info *)dev_id;

	dev_warn(ri->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t rt9460_irq_CHTERM_TMRI_handler(int irq, void *dev_id)
{
	struct rt9460_info *ri = (struct rt9460_info *)dev_id;

	dev_info(ri->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t rt9460_irq_WATCHDOGI_handler(int irq, void *dev_id)
{
	struct rt9460_info *ri = (struct rt9460_info *)dev_id;

	dev_warn(ri->dev, "%s\n", __func__);
#ifdef CONFIG_MTK_CHARGER_CLASS
	rt9460_kick_wdt(ri->chg_dev);
#endif /* CONFIG_MTK_CHARGER_CLASS */
	return IRQ_HANDLED;
}

static irqreturn_t rt9460_irq_WAKEUPI_handler(int irq, void *dev_id)
{
	struct rt9460_info *ri = (struct rt9460_info *)dev_id;

	dev_info(ri->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t rt9460_irq_VINOVPI_handler(int irq, void *dev_id)
{
	struct rt9460_info *ri = (struct rt9460_info *)dev_id;

	dev_warn(ri->dev, "%s\n", __func__);
#ifdef CONFIG_MTK_CHARGER_CLASS
	ri->chg_dev->noti.vbusov_stat = true;
	charger_dev_notify(ri->chg_dev, CHARGER_DEV_NOTIFY_VBUS_OVP);
#endif /* CONFIG_MTK_CHARGER_CLASS */
	return IRQ_HANDLED;
}

static irqreturn_t rt9460_irq_TSDI_handler(int irq, void *dev_id)
{
	struct rt9460_info *ri = (struct rt9460_info *)dev_id;

	dev_err(ri->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t rt9460_irq_SYSWAKEUPI_handler(int irq, void *dev_id)
{
	struct rt9460_info *ri = (struct rt9460_info *)dev_id;

	dev_info(ri->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t rt9460_irq_CHTREGI_handler(int irq, void *dev_id)
{
	struct rt9460_info *ri = (struct rt9460_info *)dev_id;

	dev_warn(ri->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t rt9460_irq_CHTMRI_handler(int irq, void *dev_id)
{
	struct rt9460_info *ri = (struct rt9460_info *)dev_id;

	dev_warn(ri->dev, "%s\n", __func__);
#ifdef CONFIG_MTK_CHARGER_CLASS
	charger_dev_notify(ri->chg_dev, CHARGER_DEV_NOTIFY_SAFETY_TIMEOUT);
#endif /* CONFIG_MTK_CHARGER_CLASS */
	return IRQ_HANDLED;
}

static irqreturn_t rt9460_irq_CHRCHGI_handler(int irq, void *dev_id)
{
	struct rt9460_info *ri = (struct rt9460_info *)dev_id;

	dev_info(ri->dev, "%s\n", __func__);
#ifdef CONFIG_MTK_CHARGER_CLASS
	charger_dev_notify(ri->chg_dev, CHARGER_DEV_NOTIFY_RECHG);
#endif /* CONFIG_MTK_CHARGER_CLASS */
	return IRQ_HANDLED;
}

static irqreturn_t rt9460_irq_CHTERMI_handler(int irq, void *dev_id)
{
	struct rt9460_info *ri = (struct rt9460_info *)dev_id;

	dev_info(ri->dev, "%s\n", __func__);
#ifdef CONFIG_MTK_CHARGER_CLASS
	charger_dev_notify(ri->chg_dev, CHARGER_DEV_NOTIFY_EOC);
#endif /* CONFIG_MTK_CHARGER_CLASS */
	return IRQ_HANDLED;
}

static irqreturn_t rt9460_irq_CHBATOVI_handler(int irq, void *dev_id)
{
	struct rt9460_info *ri = (struct rt9460_info *)dev_id;

	dev_warn(ri->dev, "%s\n", __func__);
#ifdef CONFIG_MTK_CHARGER_CLASS
	charger_dev_notify(ri->chg_dev, CHARGER_DEV_NOTIFY_BAT_OVP);
#endif /* CONFIG_MTK_CHARGER_CLASS */
	return IRQ_HANDLED;
}

static irqreturn_t rt9460_irq_CHBADI_handler(int irq, void *dev_id)
{
	struct rt9460_info *ri = (struct rt9460_info *)dev_id;

	dev_warn(ri->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t rt9460_irq_CHRVPI_handler(int irq, void *dev_id)
{
	struct rt9460_info *ri = (struct rt9460_info *)dev_id;

	dev_info(ri->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t rt9460_irq_BSTLOWVI_handler(int irq, void *dev_id)
{
	struct rt9460_info *ri = (struct rt9460_info *)dev_id;

	dev_warn(ri->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t rt9460_irq_BSTOLI_handler(int irq, void *dev_id)
{
	struct rt9460_info *ri = (struct rt9460_info *)dev_id;

	dev_warn(ri->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t rt9460_irq_BSTVINOVI_handler(int irq, void *dev_id)
{
	struct rt9460_info *ri = (struct rt9460_info *)dev_id;

	dev_warn(ri->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t rt9460_irq_MIVRI_handler(int irq, void *dev_id)
{
	struct rt9460_info *ri = (struct rt9460_info *)dev_id;

	dev_warn(ri->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t rt9460_irq_PWR_RDYI_handler(int irq, void *dev_id)
{
	struct rt9460_info *ri = (struct rt9460_info *)dev_id;
	int ret = 0, pwr_rdy;

	dev_info(ri->dev, "%s\n", __func__);
	ret = rt9460_reg_read(ri, RT9460_REG_CTRL1);
	if (ret < 0) {
		dev_err(ri->dev, "%s: read reg ctrl1 fail\n", __func__);
		goto out_pwrrdy;
	}
	pwr_rdy = (ret & RT9460_PWRRDY_MASK) ? 1 : 0;
	dev_info(ri->dev, "pwr_rdy [%u]\n", pwr_rdy);
	if (atomic_read(&ri->pwr_rdy) == pwr_rdy)
		goto out_pwrrdy;
	atomic_set(&ri->pwr_rdy, pwr_rdy);
	atomic_set(&ri->pwr_change, 1);
	wake_up_interruptible(&ri->waitq);
out_pwrrdy:
	return IRQ_HANDLED;
}

static int rt9460_report_tbat(struct rt9460_info *ri)
{
#ifdef CONFIG_CHARGER_MTK
	int ret;
	struct power_supply *batt_psy;
	union power_supply_propval batt_val;

	/* get power supply */
	batt_psy = power_supply_get_by_name("battery");
	if (!batt_psy)
		return -EINVAL;
	switch (ri->jeita) {
	case RT9460_JEITA_COLD: /* ~ 0 */
		batt_val.intval = 0;
		break;
	case RT9460_JEITA_COOL: /* 0 ~ 10 */
		batt_val.intval = 5;
		break;
	case RT9460_JEITA_NORMAL: /* 10 ~ 45 */
		batt_val.intval = 25;
		break;
	case RT9460_JEITA_WARM: /* 45 ~ 60 */
		batt_val.intval = 55;
		break;
	case RT9460_JEITA_HOT: /* 60 ~ */
		batt_val.intval = 60;
		break;
	default:
		goto out;
	}
	ret = power_supply_set_property(batt_psy, POWER_SUPPLY_PROP_TEMP,
					&batt_val);
	if (ret < 0)
		dev_info(ri->dev, "%s report temp fail\n", __func__);
out:
	power_supply_put(batt_psy);
#endif /* CONFIG_CHARGER_MTK */
	return 0;
}

static int rt9460_jeita_postprocess(struct rt9460_info *ri)
{
	int ret;

	ret = rt9460_reg_read(ri, RT9460_REG_CTRL7);
	if (ret < 0)
		return ret;
	if (ret & RT9460_TSCOLD_MASK)
		ri->jeita = RT9460_JEITA_COLD;
	else if (ret & RT9460_TSCOOL_MASK)
		ri->jeita = RT9460_JEITA_COOL;
	else if (ret & RT9460_TSWARM_MASK)
		ri->jeita = RT9460_JEITA_WARM;
	else if (ret & RT9460_TSHOT_MASK)
		ri->jeita = RT9460_JEITA_HOT;
	else
		ri->jeita = RT9460_JEITA_NORMAL;
	dev_info(ri->dev, "%s %s\n", __func__, rt9460_jeita_name[ri->jeita]);
	rt9460_report_tbat(ri);
	return 0;
}

static irqreturn_t rt9460_irq_TSCOLDI_handler(int irq, void *dev_id)
{
	int ret;
	struct rt9460_info *ri = (struct rt9460_info *)dev_id;

	dev_info(ri->dev, "%s\n", __func__);
	ret = rt9460_jeita_postprocess(ri);
	if (ret < 0)
		dev_info(ri->dev, "%s: process jeita fail\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t rt9460_irq_TSCOOLI_handler(int irq, void *dev_id)
{
	int ret;
	struct rt9460_info *ri = (struct rt9460_info *)dev_id;

	dev_info(ri->dev, "%s\n", __func__);
	ret = rt9460_jeita_postprocess(ri);
	if (ret < 0)
		dev_info(ri->dev, "%s: process jeita fail\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t rt9460_irq_TSWARMI_handler(int irq, void *dev_id)
{
	int ret;
	struct rt9460_info *ri = (struct rt9460_info *)dev_id;

	dev_info(ri->dev, "%s\n", __func__);
	ret = rt9460_jeita_postprocess(ri);
	if (ret < 0)
		dev_info(ri->dev, "%s: process jeita fail\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t rt9460_irq_TSHOTI_handler(int irq, void *dev_id)
{
	int ret;
	struct rt9460_info *ri = (struct rt9460_info *)dev_id;

	dev_info(ri->dev, "%s\n", __func__);
	ret = rt9460_jeita_postprocess(ri);
	if (ret < 0)
		dev_info(ri->dev, "%s: process jeita fail\n", __func__);
	return IRQ_HANDLED;
}

static const irq_handler_t rt9460_irq_desc[RT9460_IRQ_MAXNUM] = {
	[RT9460_IRQ_BATABI]	 = rt9460_irq_BATABI_handler,
	[RT9460_IRQ_SYSUVPI]	 = rt9460_irq_SYSUVPI_handler,
	[RT9460_IRQ_CHTERM_TMRI] = rt9460_irq_CHTERM_TMRI_handler,
	[RT9460_IRQ_WATCHDOGI]	 = rt9460_irq_WATCHDOGI_handler,
	[RT9460_IRQ_WAKEUPI]	 = rt9460_irq_WAKEUPI_handler,
	[RT9460_IRQ_VINOVPI]	 = rt9460_irq_VINOVPI_handler,
	[RT9460_IRQ_TSDI]	 = rt9460_irq_TSDI_handler,
	[RT9460_IRQ_SYSWAKEUPI]	 = rt9460_irq_SYSWAKEUPI_handler,
	[RT9460_IRQ_CHTREGI]	 = rt9460_irq_CHTREGI_handler,
	[RT9460_IRQ_CHTMRI]	 = rt9460_irq_CHTMRI_handler,
	[RT9460_IRQ_CHRCHGI]	 = rt9460_irq_CHRCHGI_handler,
	[RT9460_IRQ_CHTERMI]	 = rt9460_irq_CHTERMI_handler,
	[RT9460_IRQ_CHBATOVI]	 = rt9460_irq_CHBATOVI_handler,
	[RT9460_IRQ_CHBADI]	 = rt9460_irq_CHBADI_handler,
	[RT9460_IRQ_CHRVPI]	 = rt9460_irq_CHRVPI_handler,
	[RT9460_IRQ_BSTLOWVI]	 = rt9460_irq_BSTLOWVI_handler,
	[RT9460_IRQ_BSTOLI]	 = rt9460_irq_BSTOLI_handler,
	[RT9460_IRQ_BSTVINOVI]	 = rt9460_irq_BSTVINOVI_handler,
	[RT9460_IRQ_MIVRI]	 = rt9460_irq_MIVRI_handler,
	[RT9460_IRQ_PWR_RDYI]	 = rt9460_irq_PWR_RDYI_handler,
	[RT9460_IRQ_TSCOLDI]	 = rt9460_irq_TSCOLDI_handler,
	[RT9460_IRQ_TSCOOLI]	 = rt9460_irq_TSCOOLI_handler,
	[RT9460_IRQ_TSWARMI]	 = rt9460_irq_TSWARMI_handler,
	[RT9460_IRQ_TSHOTI]	 = rt9460_irq_TSHOTI_handler,
};

static inline int rt9460_irqrez(struct rt9460_info *ri)
{
	int ret;

	/* irq rez workaround ++ */
	ret = rt9460_reg_set_bits(ri, RT9460_REG_CTRL12, RT9460_WKTMREN_MASK);
	if (ret < 0)
		dev_info(ri->dev, "%s: set wktimer fail\n", __func__);
	/* wait internal osc to be enabled */
	mdelay(10);
	ret = rt9460_reg_set_bits(ri, RT9460_REG_CTRL12, RT9460_IRQREZ_MASK);
	if (ret < 0)
		dev_info(ri->dev, "%s: set irqrez fail\n", __func__);
	ret = rt9460_reg_clr_bits(ri, RT9460_REG_CTRL12, RT9460_WKTMREN_MASK);
	if (ret < 0)
		dev_info(ri->dev, "%s: clr wktimer fail\n", __func__);
	/* irq rez workaround -- */
	return ret;
}

#if defined(CONFIG_RT9460_KERNEL_INTR) || defined(CONFIG_MTK_HIFI4DSP_SUPPORT)
static int __rt9460_intr_handler(struct rt9460_info *ri)
{
	u8 event[RT9460_IRQ_REGNUM] = {0};
	int i, j, id, ret = 0;

	dev_info(ri->dev, "%s triggered\n", __func__);

	ret = rt9460_reg_block_read(ri, RT9460_REG_IRQ1, 3, event);
	if (ret < 0) {
		dev_info(ri->dev, "%s: read irq event fail\n", __func__);
		goto out_intr_handler;
	}
	ret = rt9460_reg_read(ri, RT9460_REG_STATIRQ);
	if (ret < 0) {
		dev_info(ri->dev, "%s: read stat irq event fail\n", __func__);
		goto out_intr_handler;
	}
	event[3] = (u8)ret;
	for (i = 0; i < RT9460_IRQ_REGNUM; i++) {
		event[i] &= ~(ri->irq_mask[i]);
		if (!event[i])
			continue;
		for (j = 0; j < 8; j++) {
			if (!(event[i] & (1 << j)))
				continue;
			id = i * 8 + j;
			if (!rt9460_irq_desc[id]) {
				dev_warn(ri->dev, "no %d irq_handler", id);
				continue;
			}
			rt9460_irq_desc[id](id, ri);
		}
	}
out_intr_handler:
	rt9460_irqrez(ri);
	return ret;
}

static int __rt9460_chip_irq_init(struct rt9460_info *ri)
{
	struct rt9460_platform_data *pdata = dev_get_platdata(ri->dev);
	int ret;

	ri->irq_mask[0] = 0x04;
	ri->irq_mask[1] = 0x18;
	ri->irq_mask[2] = 0x00;
	if (pdata->enable_hwjeita)
		ri->irq_mask[3] = 0x04;
	else
		ri->irq_mask[3] = 0xf4;
	ret = rt9460_reg_block_write(ri, RT9460_REG_MASK1, 3, ri->irq_mask);
	if (ret < 0)
		dev_info(ri->dev, "%s: set irq masks fail\n", __func__);

	return rt9460_reg_write(ri, RT9460_REG_STATMASK, ri->irq_mask[3]);
}
#endif /* CONFIG_RT9460_KERNEL_INTR || CONFIG_MTK_HIFI4DSP_SUPPORT */

#ifdef CONFIG_RT9460_KERNEL_INTR
static irqreturn_t rt9460_intr_handler(int irq, void *dev_id)
{
	struct rt9460_info *ri = (struct rt9460_info *)dev_id;

	__rt9460_intr_handler(ri);
	return IRQ_HANDLED;
}
#elif defined(CONFIG_MTK_HIFI4DSP_SUPPORT)
static struct rt9460_info *g_ri;
static void rt9460_ipi_msg_dispatcher(int _id, void *data, unsigned int len)
{
	struct rt9460_info *ri = g_ri;
	u8 msg = *((u8 *)data);

	dev_info(ri->dev, "%s msg %d\n", __func__, msg);
	switch (msg) {
	case MTK_POWER_IPIMSG_EINT_INIT:
		__rt9460_chip_irq_init(ri);
		break;
	case MTK_POWER_IPIMSG_EINT_EVT:
		__rt9460_intr_handler(ri);
		break;
	default:
		break;
	}
}
#endif /* CONFIG_RT9460_KERNEL_INTR */

static int rt9460_chip_irq_init(struct rt9460_info *ri)
{
	int ret = 0;
	u8 event[RT9460_IRQ_REGNUM] = {0};

	dev_info(ri->dev, "%s\n", __func__);

	/* mask & read all irq */
	ret = rt9460_reg_block_write(ri, RT9460_REG_MASK1, 3, ri->irq_mask);
	ret = rt9460_reg_write(ri, RT9460_REG_STATMASK, ri->irq_mask[3]);
	ret = rt9460_reg_block_read(ri, RT9460_REG_IRQ1, 3, event);
	ret = rt9460_reg_read(ri, RT9460_REG_STATIRQ);
	rt9460_irqrez(ri);

#ifdef CONFIG_RT9460_KERNEL_INTR
	ret = devm_gpio_request_one(ri->dev, pdata->intr_gpio, GPIOF_IN,
				    "rt9460_intr_gpio");
	if (ret < 0) {
		dev_err(ri->dev, "%s: request gpio fail\n", __func__);
		return ret;
	}
	ri->irq = gpio_to_irq(pdata->intr_gpio);
	ret = devm_request_threaded_irq(ri->dev, ri->irq, NULL,
					rt9460_intr_handler,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					dev_name(ri->dev), ri);
	if (ret < 0) {
		dev_err(ri->dev, "%s: request interrupt fail\n", __func__);
		return ret;
	}
	device_init_wakeup(ri->dev, true);
	return __rt9460_chip_irq_init(ri);
#elif defined(CONFIG_MTK_HIFI4DSP_SUPPORT)
	adsp_ipi_registration(ADSP_IPI_CHARGER, rt9460_ipi_msg_dispatcher,
			      "rt9460_irq");
	return 0;
#else
	return 0;
#endif /* CONFIG_RT9460_KERNEL_INTR */
}

static int rt9460_register_rt_regmap(struct rt9460_info *ri)
{
	dev_dbg(ri->dev, "%s\n", __func__);
#ifdef CONFIG_RT_REGMAP
	ri->regmap_props = devm_kzalloc(ri->dev, sizeof(*ri->regmap_props),
					GFP_KERNEL);
	if (!ri->regmap_props)
		return -ENOMEM;
	memcpy(ri->regmap_props, &rt9460_regmap_props,
	       sizeof(*ri->regmap_props));
	ri->regmap_props->name = kasprintf(GFP_KERNEL, "rt9460.%s",
					  dev_name(ri->dev));
	ri->regmap_dev = rt_regmap_device_register(ri->regmap_props,
				&rt9460_regmap_fops, ri->dev, ri->i2c, ri);
	if (!ri->regmap_dev)
		return -EINVAL;
	return 0;
#else
	return 0;
#endif /* #ifdef CONFIG_RT_REGMAP */
}

static int rt9460_chip_pdata_init(struct rt9460_info *ri)
{
	struct rt9460_platform_data *pdata = dev_get_platdata(ri->dev);
	int ret = 0;

	dev_dbg(ri->dev, "%s\n", __func__);
	ret = __rt9460_set_ichg(ri, pdata->ichg);
	if (ret < 0) {
		dev_err(ri->dev, "%s: set ichg fail\n", __func__);
		return ret;
	}
	ret = __rt9460_set_aicr(ri, pdata->aicr);
	if (ret < 0) {
		dev_err(ri->dev, "%s: set aicr fail\n", __func__);
		return ret;
	}
	ret = __rt9460_set_mivr(ri, pdata->mivr);
	if (ret < 0) {
		dev_err(ri->dev, "%s: set mivr fail\n", __func__);
		return ret;
	}
	ret = __rt9460_set_cv(ri, pdata->cv);
	if (ret < 0) {
		dev_err(ri->dev, "%s: set cv fail\n", __func__);
		return ret;
	}
	ret = __rt9460_set_ieoc(ri, pdata->ieoc);
	if (ret < 0) {
		dev_err(ri->dev, "%s: set ieoc fail\n", __func__);
		return ret;
	}
	ret = __rt9460_set_vprec(ri, pdata->vprec);
	if (ret < 0) {
		dev_err(ri->dev, "%s: set vprec fail\n", __func__);
		return ret;
	}
	ret = __rt9460_set_iprec(ri, pdata->iprec);
	if (ret < 0) {
		dev_err(ri->dev, "%s: set iprec fail\n", __func__);
		return ret;
	}
	ret = __rt9460_set_wt_fc(ri, pdata->wt_fc);
	if (ret < 0) {
		dev_err(ri->dev, "%s: set wt_fc fail\n", __func__);
		return ret;
	}
	ret = __rt9460_set_wt_prc(ri, pdata->wt_prc);
	if (ret < 0) {
		dev_err(ri->dev, "%s: set wt_prc fail\n", __func__);
		return ret;
	}
	ret = __rt9460_set_twdt(ri, pdata->twdt);
	if (ret < 0) {
		dev_err(ri->dev, "%s: set twdt fail\n", __func__);
		return ret;
	}
	ret = __rt9460_set_eoc_timer(ri, pdata->eoc_timer);
	if (ret < 0) {
		dev_err(ri->dev, "%s: set eoc_timer fail\n", __func__);
		return ret;
	}
	ret = __rt9460_set_mivr_disable(ri, pdata->disable_mivr);
	if (ret < 0) {
		dev_err(ri->dev, "%s: set mivr disable fail\n", __func__);
		return ret;
	}
	ret = __rt9460_set_ccjeita_enable(ri, pdata->enable_ccjeita);
	if (ret < 0) {
		dev_err(ri->dev, "%s: set ccjeita enable fail\n", __func__);
		return ret;
	}
	ret = __rt9460_set_batd_enable(ri, pdata->enable_batd);
	if (ret < 0) {
		dev_err(ri->dev, "%s: set batd enable fail\n", __func__);
		return ret;
	}
	ret = __rt9460_set_irq_pulse_enable(ri, pdata->enable_irq_pulse);
	if (ret < 0) {
		dev_err(ri->dev, "%s: set irq_pulse enable fail\n", __func__);
		return ret;
	}
	ret = __rt9460_set_safetimer_enable(ri, pdata->enable_safetimer);
	if (ret < 0) {
		dev_err(ri->dev, "%s: set safetimer enable fail\n", __func__);
		return ret;
	}
#ifdef CONFIG_CHARGER_MTK
	ret = rt9460_enable_bc12(ri, false);
	if (ret < 0) {
		dev_info(ri->dev, "%s: disable bc12 fail\n", __func__);
		return ret;
	}
#endif /* CONFIG_CHARGER_MTK */
	if (strcmp(pdata->chg_name, "secondary_chg") == 0)
		__rt9460_set_chg_enable(ri, false);
	return ret;
}

static int rt9460_chip_reset(struct i2c_client *i2c)
{
	u8 tmp[RT9460_IRQ_REGNUM] = {0};
	u8 reg_data = 0;
	int ret = 0;

	dev_dbg(&i2c->dev, "%s\n", __func__);
	/* guarantee the digital clock is enabled before reset */
	ret = i2c_smbus_write_byte_data(i2c, RT9460_REG_CTRL12, 0x04);
	if (ret < 0) {
		dev_err(&i2c->dev,
			"%s: write ctrl12 wake timer fail\n", __func__);
		return ret;
	}
	/* bypass the return value when writing chip reset */
	i2c_smbus_write_byte_data(i2c, RT9460_REG_CTRL4, 0x80);
	mdelay(20);
	/* fix ppsense icc acuracy ++ */
	ret = i2c_smbus_read_byte_data(i2c, RT9460_REG_CTRL8);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s: read ctrl8 fail\n", __func__);
		return ret;
	}
	reg_data = (u8)ret;
	reg_data |= 0x7;
	ret = i2c_smbus_write_byte_data(i2c, RT9460_REG_CTRL8, reg_data);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s: write ctrl8 fail\n", __func__);
		return ret;
	}
	/* fix ppsense icc acuracy -- */
	/* config internal vdda to 4.4v workaround ++ */
	ret = i2c_smbus_read_byte_data(i2c, RT9460_REG_HIDDEN1);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s: read hidden1 fail\n", __func__);
		return ret;
	}
	reg_data = (u8)ret;
	reg_data |= 0x80;
	ret = i2c_smbus_write_byte_data(i2c, RT9460_REG_HIDDEN1, reg_data);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s: write hidden1 fail\n", __func__);
		return ret;
	}
	/* config internal vdda to 4.4v workaround -- */
	/* default disable safety timer */
	ret = i2c_smbus_write_byte_data(i2c, RT9460_REG_CTRL10, 0x01);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s: default dis timer fail\n", __func__);
		return ret;
	}
	/* mask all irq events and read status to be cleared */
	memset(tmp, 0xff, RT9460_IRQ_REGNUM);
	ret = i2c_smbus_write_i2c_block_data(i2c, RT9460_REG_MASK1, 3, tmp);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s: set all irq masked fail\n", __func__);
		return ret;
	}
	ret = i2c_smbus_write_byte_data(i2c, RT9460_REG_STATMASK, tmp[3]);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s: set stat irq masked fail\n", __func__);
		return ret;
	}
	ret = i2c_smbus_read_i2c_block_data(i2c, RT9460_REG_IRQ1, 3, tmp);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s: read all irqevents fail\n", __func__);
		return ret;
	}
	ret = i2c_smbus_read_byte_data(i2c, RT9460_REG_STATIRQ);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s: read stat irqevent fail\n", __func__);
		return ret;
	}
	tmp[3] = (u8)ret;
	return 0;
}

static inline int rt9460_i2c_detect_devid(struct i2c_client *i2c)
{
	int ret = 0;

	dev_dbg(&i2c->dev, "%s\n", __func__);
	ret = i2c_smbus_read_byte_data(i2c, RT9460_REG_DEVID);
	if (ret < 0) {
		dev_info(&i2c->dev, "%s: chip io may bail\n", __func__);
		return ret;
	}
	dev_info(&i2c->dev, "%s: dev_id 0x%02x\n", __func__, ret);
	if ((ret & 0xf0) != 0x40) {
		dev_info(&i2c->dev, "%s: vendor id not correct\n", __func__);
		return -ENODEV;
	}
	/* finally return the rev id */
	return (ret & 0x0f);
}

static void rt_parse_dt(struct device *dev, struct rt9460_platform_data *pdata)
{
	dev_dbg(dev, "%s\n", __func__);
	/* just used to prevent the null parameter */
	if (!dev || !pdata)
		return;
	if (of_property_read_string(dev->of_node,
				    "charger_name", &pdata->chg_name) < 0)
		dev_warn(dev, "not specified chg_name\n");
	if (of_property_read_u32(dev->of_node, "ichg", &pdata->ichg) < 0)
		dev_warn(dev, "not specified ichg value\n");
	if (of_property_read_u32(dev->of_node, "aicr", &pdata->aicr) < 0)
		dev_warn(dev, "not specified aicr value\n");
	if (of_property_read_u32(dev->of_node, "mivr", &pdata->mivr) < 0)
		dev_warn(dev, "not specified mivr value\n");
	if (of_property_read_u32(dev->of_node, "cv", &pdata->cv) < 0)
		dev_warn(dev, "not specified cv value\n");
	if (of_property_read_u32(dev->of_node, "ieoc", &pdata->ieoc) < 0)
		dev_warn(dev, "not specified ieoc value\n");
	if (of_property_read_u32(dev->of_node, "vprec", &pdata->vprec) < 0)
		dev_warn(dev, "not specified vprec value\n");
	if (of_property_read_u32(dev->of_node, "iprec", &pdata->iprec) < 0)
		dev_warn(dev, "not specified iprec value\n");
	if (of_property_read_u32(dev->of_node, "vboost", &pdata->vboost) < 0)
		dev_warn(dev, "not specified vboost value\n");
	if (of_property_read_u32(dev->of_node, "wt_fc", &pdata->wt_fc) < 0)
		dev_warn(dev, "not specified wt_fc value\n");
	if (of_property_read_u32(dev->of_node, "wt_prc", &pdata->wt_prc) < 0)
		dev_warn(dev, "not specified wt_prc value\n");
	if (of_property_read_u32(dev->of_node, "twdt", &pdata->twdt) < 0)
		dev_warn(dev, "not specified twdt value\n");
	if (of_property_read_u32(dev->of_node,
				 "eoc_timer", &pdata->eoc_timer) < 0)
		dev_warn(dev, "not specified eoc_timer value\n");
#if (!defined(CONFIG_MTK_GPIO) || defined(CONFIG_MTK_GPIOLIB_STAND))
	pdata->intr_gpio = of_get_named_gpio(dev->of_node, "rt,intr_gpio", 0);
#else
	if (of_property_read_u32(np, "rt,intr_gpio_num", &pdata->intr_gpio) < 0)
		dev_warn(dev, "not specified irq gpio number\n");
#endif /* #if (!defined(CONFIG_MTK_GPIO) || defined(CONFIG_MTK_GPIOLIB_STAND) */

	pdata->enable_te = of_property_read_bool(dev->of_node, "enable_te");
	pdata->disable_mivr =
		of_property_read_bool(dev->of_node, "disable_mivr");
	pdata->enable_ccjeita =
		of_property_read_bool(dev->of_node, "enable_ccjeita");
	pdata->enable_hwjeita =
		of_property_read_bool(dev->of_node, "enable_hwjeita");
	pdata->enable_batd =
		of_property_read_bool(dev->of_node, "enable_batd");
	pdata->enable_irq_pulse =
		of_property_read_bool(dev->of_node, "enable_irq_pulse");
	pdata->enable_wdt =
		of_property_read_bool(dev->of_node, "enable_wdt");
	pdata->enable_safetimer =
		of_property_read_bool(dev->of_node, "enable_safetimer");
}

static int rt9460_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct rt9460_platform_data *pdata = dev_get_platdata(&i2c->dev);
	struct rt9460_info *ri = NULL;
	bool use_dt = i2c->dev.of_node;
	int ret = 0;

	dev_info(&i2c->dev, "%s start\n", __func__);
	ret = rt9460_i2c_detect_devid(i2c);
	if (ret < 0)
		return ret;
	/* driver data */
	ri = devm_kzalloc(&i2c->dev, sizeof(*ri), GFP_KERNEL);
	if (!ri)
		return -ENOMEM;
	/* platform data */
	if (use_dt) {
		pdata = devm_kzalloc(&i2c->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata)
			return -ENOMEM;
		memcpy(pdata, &rt9460_def_platform_data, sizeof(*pdata));
		i2c->dev.platform_data = pdata;
		rt_parse_dt(&i2c->dev, pdata);
	} else {
		if (!pdata) {
			dev_info(&i2c->dev, "no pdata specify\n");
			return -EINVAL;
		}
	}
	ri->dev = &i2c->dev;
	ri->i2c = i2c;
	ri->chgtyp = RT9460_CHGTYP_NONE;
	ri->jeita = RT9460_JEITA_NORMAL;
	ri->usbsw = RT9460_USBSW_USB;
	/* Always handle power change once */
	atomic_set(&ri->pwr_change, 1);
	init_waitqueue_head(&ri->waitq);
	mutex_init(&ri->io_lock);
	mutex_init(&ri->bc12_lock);
	mutex_init(&ri->dischg_lock);
	memcpy(ri->irq_mask, rt9460_irq_maskall, RT9460_IRQ_REGNUM);
	i2c_set_clientdata(i2c, ri);
#ifdef CONFIG_MTK_HIFI4DSP_SUPPORT
	g_ri = ri;
#endif /* CONFIG_MTK_HIFI4DSP_SUPPORT */
	/* do whole chip reset */
	ret = rt9460_chip_reset(i2c);
	if (ret < 0)
		return ret;
	/* rt-regmap register */
	ret = rt9460_register_rt_regmap(ri);
	if (ret < 0)
		return ret;
	ret = rt9460_chip_pdata_init(ri);
	if (ret < 0)
		return ret;
#ifdef CONFIG_MTK_CHARGER_CLASS
	/* charger class register */
	ri->chg_dev = charger_device_register(pdata->chg_name, ri->dev, ri,
					      &rt9460_chg_ops,
					      &rt9460_chg_props);
	if (IS_ERR(ri->chg_dev)) {
		dev_info(ri->dev, "charger device register fail\n");
		return PTR_ERR(ri->chg_dev);
	}
#endif /* CONFIG_MTK_CHARGER_CLASS */
	ret = rt9460_chip_irq_init(ri);
	if (ret < 0)
		return ret;
	ri->bc12_thread = kthread_run(rt9460_bc12_thread_fn, ri,
				      "rt9460_bc12_thread");
	if (IS_ERR(ri->bc12_thread))
		return PTR_ERR(ri->bc12_thread);
	wake_up_interruptible(&ri->waitq);
	dev_info(ri->dev, "%s end\n", __func__);
	return 0;
}

static int rt9460_i2c_remove(struct i2c_client *i2c)
{
	struct rt9460_info *ri = i2c_get_clientdata(i2c);

	dev_info(ri->dev, "%s start\n", __func__);
#ifdef CONFIG_MTK_CHARGER_CLASS
	charger_device_unregister(ri->chg_dev);
#endif /* CONFIG_MTK_CHARGER_CLASS */
#ifdef CONFIG_RT_REGMAP
	rt_regmap_device_unregister(ri->regmap_dev);
#endif /* #ifdef CONFIG_RT_REGMAP */
	dev_info(ri->dev, "%s end\n", __func__);
	return 0;
}

static void rt9460_i2c_shutdown(struct i2c_client *i2c)
{
	struct rt9460_info *ri = i2c_get_clientdata(i2c);

	disable_irq(ri->irq);
	rt9460_chip_reset(i2c);
}

static int rt9460_i2c_suspend(struct device *dev)
{
	struct rt9460_info *ri = dev_get_drvdata(dev);

	if (device_may_wakeup(dev))
		enable_irq_wake(ri->irq);
	return 0;
}

static int rt9460_i2c_resume(struct device *dev)
{
	struct rt9460_info *ri = dev_get_drvdata(dev);

	if (device_may_wakeup(dev))
		disable_irq_wake(ri->irq);
	return 0;
}

static SIMPLE_DEV_PM_OPS(rt9460_pm_ops, rt9460_i2c_suspend, rt9460_i2c_resume);

static const struct of_device_id of_id_table[] = {
	{ .compatible = "richtek,rt9460"},
	{ },
};
MODULE_DEVICE_TABLE(of, of_id_table);

static const struct i2c_device_id i2c_id_table[] = {
	{ "rt9460", 0},
	{ },
};
MODULE_DEVICE_TABLE(i2c, i2c_id_table);

static struct i2c_driver rt9460_i2c_driver = {
	.driver = {
		.name = "rt9460",
		.owner = THIS_MODULE,
		.pm = &rt9460_pm_ops,
		.of_match_table = of_match_ptr(of_id_table),
	},
	.probe = rt9460_i2c_probe,
	.remove = rt9460_i2c_remove,
	.shutdown = rt9460_i2c_shutdown,
	.id_table = i2c_id_table,
};
module_i2c_driver(rt9460_i2c_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("CY Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("Richtek RT9460 Charger Driver");
MODULE_VERSION(RT9460_DRV_VERSION);

/*
 * Release Note
 * 1.0.5
 * (1) Add Jeita process for MTK
 *
 * 1.0.4
 * (1) Add MTK USBSW for BC12
 *
 * 1.0.3
 * (1) Use chg power supply instead of ac/usb
 * (2) Add MTK charger type
 *
 * 1.0.2
 * (1) Add BC12 polling thread
 * (2) Get ac/usb power supply in probe
 * (3) Fix build error when CONFIG_MTK_CHARGER_CLASS=n
 *
 * 1.0.1
 * (1) Revise debug messages
 * (2) Revise enable/disable functions, enable/disable argument type: bool,
 *     name: en/dis
 * (3) Fix coding style
 * (4) Change set_mivr sel logic
 * (5) Add device_init_wakeup for irq wakeup capable
 * (6) Remove the callings of __rt9460_set_te_enable() and
 *     __rt9460_set_wdt_enable() from rt9460_chip_pdata_init()
 *
 * 1.0.0
 * (1) Initial release
 * (2) Port RT9460 Charger driver to GM30
 */
