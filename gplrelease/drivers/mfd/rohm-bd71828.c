// SPDX-License-Identifier: GPL-2.0-or-later
//
// Copyright (C) 2019 ROHM Semiconductors
//
// ROHM BD71828 PMIC driver

#include <linux/gpio_keys.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/irq.h>
#include <linux/mfd/core.h>
#include <linux/mfd/rohm-bd71828.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/pm.h>
#include <linux/regmap.h>
#include <linux/types.h>
#include <linux/reboot.h>

#ifdef CONFIG_FALCON
#include <asm/falcon_syscall.h>
#include <linux/mfd/rohm-bd71828_hibern_setting.h>
#endif

#define BD71828_SLV_ADDR 0x4B
#define BD71828_SLV_HIDDEN_ADDR 0x4D

#define BD71828_HBNT 0
#define BD71828_SHPM 1

#define SOFT_REBOOT			BIT(0)
#define POWEROFF			BIT(1)
#define HIBERNATION			BIT(2)
#define NO_LIGHT			BIT(7)
#define CORRUPTION			BIT(5)

static u16 reset_reason;
unsigned int bd71828_in_shpm = 0;
enum reset_reason_bits {		/* use BD71828_REG_RESERVED_2, 0xF0*/
	RR_SOFTWARE_RESTART = 0,	/* Interrupt Status register 3 (0xE2) bit 7 is set */
	RR_WATCHDOG_RST,		/* Interrupt Status register 3 (0xE2) bit 6 is set */
	RR_PWRON_LONGPRESS,		/* Interrupt Status register 3 (0xE2) bit 2 is set */
	RR_LOW_BAT_SHUTDOWN,		/* Interrupt Status register 4 (0xE3) bit 3 is set */
	RR_THERMAL_SHUTDOWN,		/* Interrupt Status register 11(0xEA) bit 3 is set */
	RR_POWER_OFF,			/* System is powered off; BD71828_REG_RESERVED_2 is set to POWEROFF */
	RR_HIBERNATION,			/* System in hibernate; BD71828_REG_RESERVED_2 is set to HIBERNATION */
	RR_DM_VERITY_CORRUPTION, /* dm-verity corruption */
	RR_MAX,
};

#define RR_DESC_ITEM(N,D) [RR_##N] = { #N, D },
static const char *reset_reason_desc[][2] = {
	RR_DESC_ITEM(SOFTWARE_RESTART,	"Software Shutdown")
	RR_DESC_ITEM(WATCHDOG_RST,	"Watchdog Triggered Reset")
	RR_DESC_ITEM(PWRON_LONGPRESS,	"Long Pressed Power Button Shutdown")
	RR_DESC_ITEM(LOW_BAT_SHUTDOWN,	"Low Battery Shutdown")
	RR_DESC_ITEM(THERMAL_SHUTDOWN,	"PMIC Overheated Thermal Shutdown")
	RR_DESC_ITEM(POWER_OFF,		"System Powered Off")
	RR_DESC_ITEM(HIBERNATION,	"System Hibernation")
	RR_DESC_ITEM(DM_VERITY_CORRUPTION,	"dm-verity Corruption")
};

static struct gpio_keys_button button = {
	.code = KEY_POWER,
	.gpio = -1,
	.type = EV_KEY,
	.wakeup = 1,
};

static struct gpio_keys_platform_data bd71828_powerkey_data = {
	.buttons = &button,
	.nbuttons = 1,
	.name = "bd71828-pwrkey",
};

static const struct resource rtc_irqs[] = {
	DEFINE_RES_IRQ_NAMED(BD71828_INT_RTC0, "bd71828-rtc-alm-0"),
	DEFINE_RES_IRQ_NAMED(BD71828_INT_RTC1, "bd71828-rtc-alm-1"),
	DEFINE_RES_IRQ_NAMED(BD71828_INT_RTC2, "bd71828-rtc-alm-2"),
};

static const struct resource charger_irqs[] = {
	DEFINE_RES_IRQ_NAMED(BD71828_INT_LONGPUSH, "bd71828-pwr-longpush"),
	DEFINE_RES_IRQ_NAMED(BD71828_INT_MIDPUSH, "bd71828-pwr-midpush"),
	DEFINE_RES_IRQ_NAMED(BD71828_INT_SHORTPUSH, "bd71828-pwr-shortpush"),
	DEFINE_RES_IRQ_NAMED(BD71828_INT_PUSH, "bd71828-pwr-push"),
	DEFINE_RES_IRQ_NAMED(BD71828_INT_DCIN_MON_RES, "bd71828-pwr-dcin-in"),
	DEFINE_RES_IRQ_NAMED(BD71828_INT_DCIN_MON_DET, "bd71828-pwr-dcin-out"),
	DEFINE_RES_IRQ_NAMED(BD71828_INT_CHG_DCIN_ILIM, "bd71828-pwr-dcin-ilim"),
	DEFINE_RES_IRQ_NAMED(BD71828_INT_BAT_MON_RES, "bd71828-vbat-normal"),
	DEFINE_RES_IRQ_NAMED(BD71828_INT_BAT_MON_DET, "bd71828-vbat-low"),
	DEFINE_RES_IRQ_NAMED(BD71828_INT_TEMP_BAT_HI_DET, "bd71828-btemp-hi"),
	DEFINE_RES_IRQ_NAMED(BD71828_INT_TEMP_BAT_HI_RES, "bd71828-btemp-cool"),
	DEFINE_RES_IRQ_NAMED(BD71828_INT_TEMP_BAT_LOW_DET, "bd71828-btemp-lo"),
	DEFINE_RES_IRQ_NAMED(BD71828_INT_TEMP_BAT_LOW_RES,
					"bd71828-btemp-warm"),
	DEFINE_RES_IRQ_NAMED(BD71828_INT_TEMP_CHIP_OVER_125_RES,
					"bd71828-temp-125-under"),
	DEFINE_RES_IRQ_NAMED(BD71828_INT_TEMP_CHIP_OVER_125_DET,
					"bd71828-temp-125-over"),
	DEFINE_RES_IRQ_NAMED(BD71828_INT_TEMP_CHIP_OVER_VF_DET,
					"bd71828-temp-hi"),
	DEFINE_RES_IRQ_NAMED(BD71828_INT_TEMP_CHIP_OVER_VF_RES,
					"bd71828-temp-norm"),
};

static const struct resource lid_irqs[] = {
	DEFINE_RES_IRQ_NAMED(BD71828_INT_VSYS_HALL_TOGGLE, "bd71828-hall"),
};

static struct mfd_cell bd71828_mfd_cells[] = {
	{ .name = "bd71828-pmic", },
	{ .name = "bd71828-gpio", },
	{ .name = "bd71828-led", },
	/*
	 * We use BD71837 driver to drive the clock block. Only differences to
	 * BD70528 clock gate are the register address and mask.
	 */
	{ .name = "bd718xx-clk", },
	{
		.name = "bd71827-power",
		.resources = charger_irqs,
		.num_resources = ARRAY_SIZE(charger_irqs),
	}, {
		.name = "bd70528-rtc",
		.resources = rtc_irqs,
		.num_resources = ARRAY_SIZE(rtc_irqs),
	}, {
		.name = "bd71828-lid-eink_hall",
		.resources = lid_irqs,
		.num_resources = ARRAY_SIZE(lid_irqs),
	}, {
		.name = "gpio-keys",
		.platform_data = &bd71828_powerkey_data,
		.pdata_size = sizeof(bd71828_powerkey_data),
	},
};

static const struct regmap_range volatile_ranges[] = {
	{
		.range_min = BD71828_REG_BOOTSRC,
		.range_max = BD71828_REG_PS_CTRL_1,
	}, {
		.range_min = BD71828_REG_PS_CTRL_3,
		.range_max = BD71828_REG_PS_CTRL_3,
	}, {
		.range_min = BD71828_REG_RTC_SEC,
		.range_max = BD71828_REG_RTC_YEAR,
	}, {
		/*
		 * For now make all charger registers volatile because many
		 * needs to be and because the charger block is not that
		 * performance critical. TBD: Check which charger registers
		 * could be cached
		 */
		.range_min = BD71828_REG_CHG_STATE,
		.range_max = BD71828_REG_CHG_FULL,
	}, {
		.range_min = BD71828_REG_INT_MAIN,
		.range_max = BD71828_REG_IO_STAT,
	},
};

static const struct regmap_access_table volatile_regs = {
	.yes_ranges = &volatile_ranges[0],
	.n_yes_ranges = ARRAY_SIZE(volatile_ranges),
};

static struct regmap_config bd71828_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.volatile_table = &volatile_regs,
	.max_register = BD71828_MAX_REGISTER,
	.cache_type = REGCACHE_RBTREE,
};

/*
 * Mapping of main IRQ register bits to sub-IRQ register offsets so that we can
 * access corect sub-IRQ registers based on bits that are set in main IRQ
 * register.
 */

unsigned int bit0_offsets[] = {11};		/* RTC IRQ register */
unsigned int bit1_offsets[] = {10};		/* TEMP IRQ register */
unsigned int bit2_offsets[] = {6, 7, 8, 9};	/* BAT MON IRQ registers */
unsigned int bit3_offsets[] = {5};		/* BAT IRQ register */
unsigned int bit4_offsets[] = {4};		/* CHG IRQ register */
unsigned int bit5_offsets[] = {3};		/* VSYS IRQ register */
unsigned int bit6_offsets[] = {1, 2};		/* DCIN IRQ registers */
unsigned int bit7_offsets[] = {0};		/* BUCK IRQ register */

static struct regmap_irq_sub_irq_map bd71828_sub_irq_offsets[] = {
	REGMAP_IRQ_MAIN_REG_OFFSET(bit0_offsets),
	REGMAP_IRQ_MAIN_REG_OFFSET(bit1_offsets),
	REGMAP_IRQ_MAIN_REG_OFFSET(bit2_offsets),
	REGMAP_IRQ_MAIN_REG_OFFSET(bit3_offsets),
	REGMAP_IRQ_MAIN_REG_OFFSET(bit4_offsets),
	REGMAP_IRQ_MAIN_REG_OFFSET(bit5_offsets),
	REGMAP_IRQ_MAIN_REG_OFFSET(bit6_offsets),
	REGMAP_IRQ_MAIN_REG_OFFSET(bit7_offsets),
};

static struct regmap_irq bd71828_irqs[] = {
	REGMAP_IRQ_REG(BD71828_INT_BUCK1_OCP, 0, BD71828_INT_BUCK1_OCP_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BUCK2_OCP, 0, BD71828_INT_BUCK2_OCP_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BUCK3_OCP, 0, BD71828_INT_BUCK3_OCP_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BUCK4_OCP, 0, BD71828_INT_BUCK4_OCP_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BUCK5_OCP, 0, BD71828_INT_BUCK5_OCP_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BUCK6_OCP, 0, BD71828_INT_BUCK6_OCP_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BUCK7_OCP, 0, BD71828_INT_BUCK7_OCP_MASK),
	REGMAP_IRQ_REG(BD71828_INT_PGFAULT, 0, BD71828_INT_PGFAULT_MASK),
	/* DCIN1 interrupts */
	REGMAP_IRQ_REG(BD71828_INT_DCIN_DET, 1, BD71828_INT_DCIN_DET_MASK),
	REGMAP_IRQ_REG(BD71828_INT_DCIN_RMV, 1, BD71828_INT_DCIN_RMV_MASK),
	REGMAP_IRQ_REG(BD71828_INT_CLPS_OUT, 1, BD71828_INT_CLPS_OUT_MASK),
	REGMAP_IRQ_REG(BD71828_INT_CLPS_IN, 1, BD71828_INT_CLPS_IN_MASK),
	/* DCIN2 interrupts */
	REGMAP_IRQ_REG(BD71828_INT_DCIN_MON_RES, 2,
		       BD71828_INT_DCIN_MON_RES_MASK),
	REGMAP_IRQ_REG(BD71828_INT_DCIN_MON_DET, 2,
		       BD71828_INT_DCIN_MON_DET_MASK),
	REGMAP_IRQ_REG(BD71828_INT_LONGPUSH, 2, BD71828_INT_LONGPUSH_MASK),
	REGMAP_IRQ_REG(BD71828_INT_MIDPUSH, 2, BD71828_INT_MIDPUSH_MASK),
	REGMAP_IRQ_REG(BD71828_INT_SHORTPUSH, 2, BD71828_INT_SHORTPUSH_MASK),
	REGMAP_IRQ_REG(BD71828_INT_PUSH, 2, BD71828_INT_PUSH_MASK),
	REGMAP_IRQ_REG(BD71828_INT_WDOG, 2, BD71828_INT_WDOG_MASK),
	REGMAP_IRQ_REG(BD71828_INT_SWRESET, 2, BD71828_INT_SWRESET_MASK),
	/* Vsys */
	REGMAP_IRQ_REG(BD71828_INT_VSYS_UV_RES, 3,
		       BD71828_INT_VSYS_UV_RES_MASK),
	REGMAP_IRQ_REG(BD71828_INT_VSYS_UV_DET, 3,
		       BD71828_INT_VSYS_UV_DET_MASK),
	REGMAP_IRQ_REG(BD71828_INT_VSYS_LOW_RES, 3,
		       BD71828_INT_VSYS_LOW_RES_MASK),
	REGMAP_IRQ_REG(BD71828_INT_VSYS_LOW_DET, 3,
		       BD71828_INT_VSYS_LOW_DET_MASK),
	REGMAP_IRQ_REG(BD71828_INT_VSYS_HALL_IN, 3,
		       BD71828_INT_VSYS_HALL_IN_MASK),
	REGMAP_IRQ_REG(BD71828_INT_VSYS_HALL_TOGGLE, 3,
		       BD71828_INT_VSYS_HALL_TOGGLE_MASK),
	REGMAP_IRQ_REG(BD71828_INT_VSYS_MON_RES, 3,
		       BD71828_INT_VSYS_MON_RES_MASK),
	REGMAP_IRQ_REG(BD71828_INT_VSYS_MON_DET, 3,
		       BD71828_INT_VSYS_MON_DET_MASK),
	/* Charger */
	REGMAP_IRQ_REG(BD71828_INT_CHG_DCIN_ILIM, 4,
		       BD71828_INT_CHG_DCIN_ILIM_MASK),
	REGMAP_IRQ_REG(BD71828_INT_CHG_TOPOFF_TO_DONE, 4,
		       BD71828_INT_CHG_TOPOFF_TO_DONE_MASK),
	REGMAP_IRQ_REG(BD71828_INT_CHG_WDG_TEMP, 4,
		       BD71828_INT_CHG_WDG_TEMP_MASK),
	REGMAP_IRQ_REG(BD71828_INT_CHG_WDG_TIME, 4,
		       BD71828_INT_CHG_WDG_TIME_MASK),
	REGMAP_IRQ_REG(BD71828_INT_CHG_RECHARGE_RES, 4,
		       BD71828_INT_CHG_RECHARGE_RES_MASK),
	REGMAP_IRQ_REG(BD71828_INT_CHG_RECHARGE_DET, 4,
		       BD71828_INT_CHG_RECHARGE_DET_MASK),
	REGMAP_IRQ_REG(BD71828_INT_CHG_RANGED_TEMP_TRANSITION, 4,
		       BD71828_INT_CHG_RANGED_TEMP_TRANSITION_MASK),
	REGMAP_IRQ_REG(BD71828_INT_CHG_STATE_TRANSITION, 4,
		       BD71828_INT_CHG_STATE_TRANSITION_MASK),
	/* Battery */
	REGMAP_IRQ_REG(BD71828_INT_BAT_TEMP_NORMAL, 5,
		       BD71828_INT_BAT_TEMP_NORMAL_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BAT_TEMP_ERANGE, 5,
		       BD71828_INT_BAT_TEMP_ERANGE_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BAT_TEMP_WARN, 5,
		       BD71828_INT_BAT_TEMP_WARN_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BAT_REMOVED, 5,
		       BD71828_INT_BAT_REMOVED_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BAT_DETECTED, 5,
		       BD71828_INT_BAT_DETECTED_MASK),
	REGMAP_IRQ_REG(BD71828_INT_THERM_REMOVED, 5,
		       BD71828_INT_THERM_REMOVED_MASK),
	REGMAP_IRQ_REG(BD71828_INT_THERM_DETECTED, 5,
		       BD71828_INT_THERM_DETECTED_MASK),
	/* Battery Mon 1 */
	REGMAP_IRQ_REG(BD71828_INT_BAT_DEAD, 6, BD71828_INT_BAT_DEAD_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BAT_SHORTC_RES, 6,
		       BD71828_INT_BAT_SHORTC_RES_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BAT_SHORTC_DET, 6,
		       BD71828_INT_BAT_SHORTC_DET_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BAT_LOW_VOLT_RES, 6,
		       BD71828_INT_BAT_LOW_VOLT_RES_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BAT_LOW_VOLT_DET, 6,
		       BD71828_INT_BAT_LOW_VOLT_DET_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BAT_OVER_VOLT_RES, 6,
		       BD71828_INT_BAT_OVER_VOLT_RES_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BAT_OVER_VOLT_DET, 6,
		       BD71828_INT_BAT_OVER_VOLT_DET_MASK),
	/* Battery Mon 2 */
	REGMAP_IRQ_REG(BD71828_INT_BAT_MON_RES, 7,
		       BD71828_INT_BAT_MON_RES_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BAT_MON_DET, 7,
		       BD71828_INT_BAT_MON_DET_MASK),
	/* Battery Mon 3 (Coulomb counter) */
	REGMAP_IRQ_REG(BD71828_INT_BAT_CC_MON1, 8,
		       BD71828_INT_BAT_CC_MON1_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BAT_CC_MON2, 8,
		       BD71828_INT_BAT_CC_MON2_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BAT_CC_MON3, 8,
		       BD71828_INT_BAT_CC_MON3_MASK),
	/* Battery Mon 4 */
	REGMAP_IRQ_REG(BD71828_INT_BAT_OVER_CURR_1_RES, 9,
		       BD71828_INT_BAT_OVER_CURR_1_RES_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BAT_OVER_CURR_1_DET, 9,
		       BD71828_INT_BAT_OVER_CURR_1_DET_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BAT_OVER_CURR_2_RES, 9,
		       BD71828_INT_BAT_OVER_CURR_2_RES_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BAT_OVER_CURR_2_DET, 9,
		       BD71828_INT_BAT_OVER_CURR_2_DET_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BAT_OVER_CURR_3_RES, 9,
		       BD71828_INT_BAT_OVER_CURR_3_RES_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BAT_OVER_CURR_3_DET, 9,
		       BD71828_INT_BAT_OVER_CURR_3_DET_MASK),
	/* Temperature */
	REGMAP_IRQ_REG(BD71828_INT_TEMP_BAT_LOW_RES, 10,
		       BD71828_INT_TEMP_BAT_LOW_RES_MASK),
	REGMAP_IRQ_REG(BD71828_INT_TEMP_BAT_LOW_DET, 10,
		       BD71828_INT_TEMP_BAT_LOW_DET_MASK),
	REGMAP_IRQ_REG(BD71828_INT_TEMP_BAT_HI_RES, 10,
		       BD71828_INT_TEMP_BAT_HI_RES_MASK),
	REGMAP_IRQ_REG(BD71828_INT_TEMP_BAT_HI_DET, 10,
		       BD71828_INT_TEMP_BAT_HI_DET_MASK),
	REGMAP_IRQ_REG(BD71828_INT_TEMP_CHIP_OVER_125_RES, 10,
		       BD71828_INT_TEMP_CHIP_OVER_125_RES_MASK),
	REGMAP_IRQ_REG(BD71828_INT_TEMP_CHIP_OVER_125_DET, 10,
		       BD71828_INT_TEMP_CHIP_OVER_125_DET_MASK),
	REGMAP_IRQ_REG(BD71828_INT_TEMP_CHIP_OVER_VF_DET, 10,
		       BD71828_INT_TEMP_CHIP_OVER_VF_DET_MASK),
	REGMAP_IRQ_REG(BD71828_INT_TEMP_CHIP_OVER_VF_RES, 10,
		       BD71828_INT_TEMP_CHIP_OVER_VF_RES_MASK),
	/* RTC Alarm */
	REGMAP_IRQ_REG(BD71828_INT_RTC0, 11, BD71828_INT_RTC0_MASK),
	REGMAP_IRQ_REG(BD71828_INT_RTC1, 11, BD71828_INT_RTC1_MASK),
	REGMAP_IRQ_REG(BD71828_INT_RTC2, 11, BD71828_INT_RTC2_MASK),
};

static int bd71828_handle_pre_irq(void *irq_drv_data)
{
	pr_info("!bd71828 PMIC irq!\n");
	return 0;
}

static struct regmap_irq_chip bd71828_irq_chip = {
	.name = "bd71828_irq",
	.main_status = BD71828_REG_INT_MAIN,
	.irqs = &bd71828_irqs[0],
	.num_irqs = ARRAY_SIZE(bd71828_irqs),
	.status_base = BD71828_REG_INT_BUCK,
	.mask_base = BD71828_REG_INT_MASK_BUCK,
	.ack_base = BD71828_REG_INT_BUCK,
	.mask_invert = true,
	.init_ack_masked = true,
	.num_regs = 12,
	.num_main_regs = 1,
	.sub_reg_offsets = &bd71828_sub_irq_offsets[0],
	.num_main_status_bits = 8,
	.irq_reg_stride = 1,
	.handle_pre_irq = bd71828_handle_pre_irq,
};

struct rohm_regmap_dev *bd71828_chip;

#ifdef BD71828_SHIPMODE_DCIN_EMULATE_WORKAROUND
struct rohm_regmap_dev *bd71828_hidden_chip;
#endif
static unsigned int poweroff_mode = BD71828_HBNT;
static unsigned int disable_boot_light = 0;

static unsigned int g_reg_address;
static ssize_t show_reg_access(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct rohm_regmap_dev *chip;
	unsigned int reg_value = 0;

	chip = dev_get_drvdata(dev);

	regmap_read(chip->regmap, g_reg_address, &reg_value);

	pr_info("[%s] 0x%x = 0x%x\n", __func__, g_reg_address, reg_value);
	return sprintf(buf, "0x%x = 0x%x\n", g_reg_address, reg_value);
}

static ssize_t store_reg_access(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf,
				 size_t size)
{
	int ret = 0;
	char *pvalue = NULL, *addr, *val;
	unsigned int reg_value = 0;
	unsigned int reg_address = 0;
	struct rohm_regmap_dev *chip;

	chip = dev_get_drvdata(dev);
	if (buf != NULL && size != 0) {
		pr_notice("[%s] size is %d, buf is %s\n"
			, __func__, (int)size, buf);

		pvalue = (char *)buf;
		addr = strsep(&pvalue, " ");
		val = strsep(&pvalue, " ");
		if (addr)
			ret = kstrtou32(addr, 16, (unsigned int *)&reg_address);

		if (val) {
			ret = kstrtou32(val, 16, (unsigned int *)&reg_value);
			pr_notice("[%s] write PMU reg 0x%x with value 0x%x !\n"
				, __func__, reg_address, reg_value);
			ret = regmap_write(chip->regmap,
				reg_address, reg_value);
		} else {
			ret = regmap_read(chip->regmap,
				reg_address, &reg_value);
			pr_notice("[%s] read PMU reg 0x%x with value 0x%x !\n"
				, __func__, reg_address, reg_value);
		}
		g_reg_address = reg_address;
	}

	return size;
}

static DEVICE_ATTR(reg_access, 0664, show_reg_access, store_reg_access);

/** @brief show die temperature */
static ssize_t bd71827_sysfs_show_die_temperature(struct device *dev,
					    struct device_attribute *attr, char *buf)
{
	u16 t = 0;
	u8 *tmp = (u8 *)&t;
	int temp = 3513 * 100000;
	int ret = -EINVAL;

	if (bd71828_chip)
		ret = regmap_bulk_read(bd71828_chip->regmap, BD71828_REG_VM_VF_U, &t, 2);

	*tmp &= BD71828_MASK_VM_VF_U;
	t = be16_to_cpu(t);

	if (ret || t > 3200)
		dev_err(dev, "Failed to read system min average voltage\n");

	temp -= 144375ULL * (unsigned int)t;
	temp = temp / 100000;

	return sprintf(buf, "%d\n", temp);
}

static DEVICE_ATTR(die_temp, S_IRUGO, bd71827_sysfs_show_die_temperature, NULL);

static ssize_t no_boot_light_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf,
				 size_t size)
{
	unsigned long val;
	int ret;

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return ret;
	if (val)
		disable_boot_light = 1;
	else
		disable_boot_light = 0;
	dev_info(dev, "disable_boot_light set to %d\n", disable_boot_light);

	return size;
}

static DEVICE_ATTR_WO(no_boot_light);

static ssize_t show_poweroff_flag(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	pr_info("[%s] ship_mode %d\n", __func__, poweroff_mode);
	return sprintf(buf, "ship_mode %d\n", poweroff_mode);
}

static ssize_t store_poweroff_flag(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf,
				 size_t size)
{
	int ret = 0;
	char *pvalue = NULL, *flag;

	if (buf != NULL && size != 0) {
		pr_notice("[%s] size is %d, buf is %s\n"
			, __func__, (int)size, buf);

		pvalue = (char *)buf;
		flag = strsep(&pvalue, " ");
		if (flag) {
			ret = kstrtou32(flag, 16, (unsigned int *)&poweroff_mode);
			if (ret)
				poweroff_mode = BD71828_HBNT;
			else if (poweroff_mode > BD71828_SHPM)
				poweroff_mode = BD71828_SHPM;
		}
	}

	return size;
}

static DEVICE_ATTR(ship_mode, 0664, show_poweroff_flag, store_poweroff_flag);

static void log_reset_reason(struct device *dev)
{
	int i;

	dev_notice(dev, "RESET_REASON = 0x%X\n", reset_reason);
	if (reset_reason == 0) {
		dev_err(dev, "Reboot Reason: UNKNOWN\n");
	} else {
		for (i = 0; i < ARRAY_SIZE(reset_reason_desc); i++) {
			if (reset_reason & (1<<i)) {
				dev_err(dev, "Reboot Reason: %-20s %s\n", reset_reason_desc[i][0], reset_reason_desc[i][1]);
			}
		}
	}
}

/** @brief show reset reasons */
static ssize_t bd71827_sysfs_show_reset_reasons(struct device *dev,
					    struct device_attribute *attr, char *buf)
{
	int i;
	int index = 0;

	for (i = 0; i < ARRAY_SIZE(reset_reason_desc); i++) {
		if (reset_reason & (1<<i)) {
			index += sprintf(buf + index, "%s\n", reset_reason_desc[i][0]);
		}
	}
	log_reset_reason(dev);
	return index;
}

static DEVICE_ATTR(reset_reasons, S_IRUGO, bd71827_sysfs_show_reset_reasons, NULL);

static void bd71828_enter_hbnt(void)
{
	if (bd71828_chip)
		regmap_write(bd71828_chip->regmap, BD71828_REG_PS_CTRL_1, 0x2);
}

static void bd71828_enter_shpm(void)
{
	unsigned int chr_det = 0;
	int ret = 0, retry = 3;

	if (!bd71828_chip 
#ifdef BD71828_SHIPMODE_DCIN_EMULATE_WORKAROUND
		|| !bd71828_hidden_chip
#endif
	   ) {
		pr_notice("[%s] no rohm dev\n", __func__);
		return;
	}

	regmap_read(bd71828_chip->regmap, BD71828_REG_DCIN_STAT, &chr_det);
	pr_notice("[%s] chr_det 0x%x\n", __func__, chr_det);
#ifdef BD71828_SHIPMODE_DCIN_EMULATE_WORKAROUND
	if (chr_det & BD71828_MASK_DCIN_DET)
		regmap_write(bd71828_chip->regmap, BD71828_REG_PS_CTRL_1, 1);
	else {
		regmap_write(bd71828_chip->regmap, 0xFE, 0x8C);
		regmap_write(bd71828_chip->regmap, 0xFF, 1);
		regmap_write(bd71828_chip->regmap, BD71828_REG_PS_CTRL_1, 1);
		regmap_write(bd71828_hidden_chip->regmap, 0x9A, 2);
		regmap_write(bd71828_hidden_chip->regmap, 0x9C, 1);
		regmap_write(bd71828_hidden_chip->regmap, 0x9C, 0);
	}
#else
	bd71828_in_shpm = 1;
	pr_notice("[%s] clear green led and setup amber led\n", __func__);
	ret = regmap_update_bits(bd71828_chip->regmap,
			 BD71828_REG_LED_CTRL,
			 BD71828_MASK_LED_GREEN,
			 ~BD71828_MASK_LED_GREEN);
	if(ret)
		pr_notice("[%s] clear green led failed\n", __func__);
	ret = regmap_update_bits(bd71828_chip->regmap,
			 BD71828_REG_LED_CTRL,
			 BD71828_MASK_LED_AMBER,
			 BD71828_MASK_LED_AMBER);
	if(ret)
		pr_notice("[%s] setup amber led failed\n", __func__);

	while(retry){
		ret = regmap_write(bd71828_chip->regmap, BD71828_REG_PS_CTRL_1, 1);
		retry--;
		if(ret)
			pr_notice("[%s] seting shipmode bit failed, %d retry left", __func__, retry);
		else
			break;
		msleep(200);
	}
#endif
}

/*
 *	Notifier for system reboot
 */
static int reboot_notify_handler(struct notifier_block *this, unsigned long code, void *cmd)
{
	u8 reg = SOFT_REBOOT;

	if (disable_boot_light)
		reg |= NO_LIGHT;

	if (cmd && !strcmp(cmd, "dm-verity device corrupted")) {
		reg |= CORRUPTION;
	}

	/* Always set software reboot flag will be overwrite if power off */
	pr_info("System reboot set reserved2 to 0x%x\n", reg);
	if (bd71828_chip)
		regmap_write(bd71828_chip->regmap, BD71828_REG_RESERVED2, reg);
	return 0;
}
/*
 *	The reboot handler needs to learn about the reasons of the reboot
 *	if the bootloader is in the command, set fastboot flag
 */
static struct notifier_block reboot_notifier = {
	.notifier_call = reboot_notify_handler,
};
static void bd71828_power_off(void)
{
	int ret = 0;
	pr_notice("[%s] power off system(%d)\n", __func__, poweroff_mode);

	if(bd71828_chip)
		ret = regmap_write(bd71828_chip->regmap, BD71828_REG_RESERVED2,POWEROFF);


	if (poweroff_mode == BD71828_HBNT)
		bd71828_enter_hbnt();
	else
		bd71828_enter_shpm();
	while (1) {
		mdelay(1000);

		pr_notice("[%s] power off\n", __func__);
	};
}

#ifdef BD71828_SHIPMODE_DCIN_EMULATE_WORKAROUND
static int bd71828_add_hidden_reg(struct i2c_client *i2c)
{
	int ret = 0;
	struct rohm_regmap_dev *chip;

	chip = devm_kzalloc(&i2c->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	dev_set_drvdata(&i2c->dev, chip);

	chip->chip_type = ROHM_CHIP_TYPE_BD71828;
	chip->regmap = devm_regmap_init_i2c(i2c, &bd71828_regmap);
	if (IS_ERR(chip->regmap)) {
		dev_err(&i2c->dev, "Failed to initialize hidden Regmap\n");
		return PTR_ERR(chip->regmap);
	}

	dev_notice(&i2c->dev, "add hidden register support\n");

	bd71828_hidden_chip = chip;
	ret = device_create_file(&(i2c->dev), &dev_attr_reg_access);

	return ret;
}
#endif


/**@ brief bd71828_get_events_recorder
 * @ this function read all the interrupts status registers and clear
 * @ all the interrupt status registers
 * @ return 0
 */
int bd71828_get_events_recorder(void)
{
	int ret;
	int r, i;
	unsigned long addr;
	unsigned int events_recorder[BD71828_REG_INT_RTC - BD71828_REG_INT_MAIN +2];


	if (!bd71828_chip) {
		pr_notice("[%s] no rohm dev\n", __func__);
		return -1;
	}
	/* read and clean the software reset flag register */
	ret = regmap_read(bd71828_chip->regmap, BD71828_REG_RESERVED2, &r);
	if (ret) {
		dev_err(bd71828_chip->dev, "Failed to read reserv2 register\n");
		return ret;
	}
	dev_notice(bd71828_chip->dev, "BD71828_REG_RESERVED2=0x%02X\n", r);
	ret = regmap_write(bd71828_chip->regmap, BD71828_REG_RESERVED2, 0x00);
	if (ret) {
		dev_err(bd71828_chip->dev, "Failed to read reserv2 register\n");
		return ret;
	}

	reset_reason = 0;

	if (r & SOFT_REBOOT) {
		reset_reason |= BIT(RR_SOFTWARE_RESTART);	/* software reset */
		dev_dbg(bd71828_chip->dev, "The last system restart was initiated by software !!!!!\n");
		if (r & CORRUPTION) {
			reset_reason |= BIT(RR_DM_VERITY_CORRUPTION);	/* software reset */
			dev_dbg(bd71828_chip->dev, "The last system restart was initiated by dm-verity corruption !!!!!\n");
		}
	} else if (r & POWEROFF) {
		reset_reason |= BIT(RR_POWER_OFF);		/* power off */
		dev_dbg(bd71828_chip->dev, "The system was powered off !!!!!\n");
	} else if (r & HIBERNATION) {
		reset_reason |= BIT(RR_HIBERNATION);		/* hibernation */
		dev_dbg(bd71828_chip->dev, "The system was in hibernation !!!!!\n");
	}

	/* read and show BD71828_REG_BOOTSRC */
	ret = regmap_read(bd71828_chip->regmap, BD71828_REG_BOOTSRC, &r);
	if (ret) {
		dev_err(bd71828_chip->dev, "Failed to read bootsrc register\n");
		return ret;
	}
	dev_notice(bd71828_chip->dev, "BD71828_REG_BOOTSRC=0x%02X\n", r);
	/* read, show and clear BD71828_REG_RESETSRC */
	ret = regmap_read(bd71828_chip->regmap, BD71828_REG_RESETSRC, &r);
	if (ret) {
		dev_err(bd71828_chip->dev, "Failed to read resetsrc register\n");
		return ret;
	}
	dev_notice(bd71828_chip->dev, "BD71828_REG_RESETSRC=0x%02X\n", r);
	ret = regmap_write(bd71828_chip->regmap, BD71828_REG_RESETSRC, 0xFF);
	if (ret) {
		dev_err(bd71828_chip->dev, "Failed to read resetsrc register\n");
		return ret;
	}

	/* record and clear all INT_STAT */
	/* bd71827 has another interrupt stat 13 */
	for (i = 0; i < BD71828_REG_INT_RTC - BD71828_REG_INT_MAIN +2; i++) {
			addr = BD71828_REG_INT_MAIN + i;
		ret = regmap_read(bd71828_chip->regmap, addr, &events_recorder[i]);
		if (ret) {
			dev_err(bd71828_chip->dev, "Failed to read interupt status register\n");
			return ret;
		}
		ret = regmap_write(bd71828_chip->regmap, addr, 0x00);
		if (ret) {
			dev_err(bd71828_chip->dev, "Failed to clear interrupt status register\n");
			return ret;
		}
		dev_err(bd71828_chip->dev, "BD71827_REG_INT_STAT_%02d=0x%02X ", i, events_recorder[i]);
	}

	if ((events_recorder[3] & BIT(6)) && (!(reset_reason & BIT(RR_SOFTWARE_RESTART))))
		reset_reason |= BIT(RR_WATCHDOG_RST);		/* watchdog reset */

	if (events_recorder[3] & BIT(2))
		reset_reason |= BIT(RR_PWRON_LONGPRESS);	/* power button long press reset */

	if (events_recorder[4] & BIT(3))
		reset_reason |= BIT(RR_LOW_BAT_SHUTDOWN);	/* battery low voltage reset */

	if (events_recorder[11] & BIT(3))
		reset_reason |= BIT(RR_THERMAL_SHUTDOWN); 	/* battery temperature high reset */

	log_reset_reason(bd71828_chip->dev);

#ifdef CONFIG_IDME
	{
		extern char idme_bootmode_value[];
		if (!strncmp(idme_bootmode_value, "ota", 3)) {
			disable_boot_light = 1;
			dev_info(bd71828_chip->dev, "disable_boot_light set to %d\n", disable_boot_light);
		}
	}
#endif

	return ret;
}


#ifdef CONFIG_FALCON
static int bd71828_suspend(struct device *dev)
{
	int i, size = sizeof(hibern_setting)/sizeof(hibern_setting[0]);
	struct rohm_regmap_dev *chip;
	struct reg_hibern* store_reg;
	unsigned int slv_addr;
	unsigned int reg_value = 0;

	if (!in_falcon())
		return 0;

	slv_addr = (unsigned int)of_device_get_match_data(dev);
	if (slv_addr == BD71828_SLV_HIDDEN_ADDR)
		return 0;

	chip = dev_get_drvdata(dev);
	regcache_cache_bypass(chip->regmap, true);

	for (i = 0; i < size; i++) {
		store_reg = &hibern_setting[i];
		regmap_read(chip->regmap, store_reg->reg, &store_reg->val);
	}

	regmap_write(chip->regmap, BD71828_REG_VSYS_MIN, BD71828_VSYS_MIN_3_4V);
	regmap_read(chip->regmap, BD71828_REG_VSYS_MIN, &reg_value);
	if( reg_value != BD71828_VSYS_MIN_3_4V)
		dev_err(chip->dev, "hibernation entry: vsys_min unexpected readback:0x%x!!!\n", reg_value);

	regcache_cache_bypass(chip->regmap, false);

	return 0;
}

static int bd71828_resume(struct device *dev)
{
	int i, size = sizeof(hibern_setting)/sizeof(hibern_setting[0]);
	struct rohm_regmap_dev *chip;
	struct reg_hibern* store_reg;
	unsigned int slv_addr;

	if (!in_falcon())
		return 0;

	slv_addr = (unsigned int)of_device_get_match_data(dev);
	if (slv_addr == BD71828_SLV_HIDDEN_ADDR)
		return 0;

	chip = dev_get_drvdata(dev);
	regcache_cache_bypass(chip->regmap, true);
	for (i = 0; i < size; i++) {
		store_reg = &hibern_setting[i];
		regmap_update_bits(chip->regmap, store_reg->reg, store_reg->mask, store_reg->val);
	}
	/* clear HIBERNATE bit if resume success from quickboot */
	regmap_write(chip->regmap, BD71828_REG_RESERVED2, 0);
	regcache_cache_bypass(chip->regmap, false);

	return 0;
}
static SIMPLE_DEV_PM_OPS(bd71828_pm, bd71828_suspend, bd71828_resume);
#define FALCON_PM_OPS &bd71828_pm
#else
#define FALCON_PM_OPS NULL
#endif

static int bd71828_i2c_probe(struct i2c_client *i2c,
			     const struct i2c_device_id *id)
{
	struct rohm_regmap_dev *chip;
	struct regmap_irq_chip_data *irq_data;
	int ret;
	unsigned int slv_addr;

	slv_addr = (unsigned int)of_device_get_match_data(&i2c->dev);
	if (!slv_addr)
		return -ENODEV;

#ifdef BD71828_SHIPMODE_DCIN_EMULATE_WORKAROUND
	if (slv_addr == BD71828_SLV_HIDDEN_ADDR) {
		ret = bd71828_add_hidden_reg(i2c);
		return ret;
	}
#endif

	if (!i2c->irq) {
		dev_err(&i2c->dev, "No IRQ configured\n");
		return -EINVAL;
	}

	chip = devm_kzalloc(&i2c->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	dev_set_drvdata(&i2c->dev, chip);

	chip->chip_type = ROHM_CHIP_TYPE_BD71828;
	chip->regmap = devm_regmap_init_i2c(i2c, &bd71828_regmap);
	if (IS_ERR(chip->regmap)) {
		dev_err(&i2c->dev, "Failed to initialize Regmap\n");
		return PTR_ERR(chip->regmap);
	}
	bd71828_chip = chip;
	bd71828_chip->dev = &i2c->dev;

	/* read and clear INT_STAT before requesting irq */
	bd71828_get_events_recorder();

	ret = devm_regmap_add_irq_chip(&i2c->dev, chip->regmap,
				       i2c->irq, IRQF_ONESHOT, 0,
				       &bd71828_irq_chip, &irq_data);
	if (ret) {
		dev_err(&i2c->dev, "Failed to add IRQ chip\n");
		return ret;
	}

	dev_dbg(&i2c->dev, "Registered %d IRQs for chip\n",
		bd71828_irq_chip.num_irqs);

	ret = regmap_irq_get_virq(irq_data, BD71828_INT_PUSH);

	if (ret < 0) {
		dev_err(&i2c->dev, "Failed to get the power-key IRQ\n");
		return ret;
	}

	button.irq = ret;

	ret = devm_mfd_add_devices(&i2c->dev, PLATFORM_DEVID_AUTO,
				   bd71828_mfd_cells,
				   ARRAY_SIZE(bd71828_mfd_cells), NULL, 0,
				   regmap_irq_get_domain(irq_data));
	if (ret)
		dev_err(&i2c->dev, "Failed to create subdevices\n");


	if (of_device_is_system_power_controller(i2c->dev.of_node)) {
		if (!pm_power_off)
			pm_power_off = bd71828_power_off;
		else
			dev_warn(&i2c->dev, "poweroff callback already assigned\n");
	}

	device_create_file(&(i2c->dev), &dev_attr_reg_access);
	device_create_file(&(i2c->dev), &dev_attr_die_temp);
	device_create_file(&(i2c->dev), &dev_attr_no_boot_light);
	device_create_file(&(i2c->dev), &dev_attr_ship_mode);
	device_create_file(&(i2c->dev), &dev_attr_reset_reasons);

	ret = register_reboot_notifier(&reboot_notifier);
	bd71828_in_shpm = 0;
	return ret;
}

static const struct of_device_id bd71828_of_match[] = {
	{ .compatible = "rohm,bd71828", .data = (void *)BD71828_SLV_ADDR },
#ifdef BD71828_SHIPMODE_DCIN_EMULATE_WORKAROUND
	{ .compatible = "rohm,bd71828_h", .data = (void *)BD71828_SLV_HIDDEN_ADDR },
#endif
	{ },
};

static const struct i2c_device_id bd71828_dev_id[] = {
	{"bd71828", 0},
	{},
};

static struct i2c_driver bd71828_drv = {
	.driver = {
		.name = "rohm-bd71828",
		.pm = FALCON_PM_OPS,
		.of_match_table = bd71828_of_match,
	},
	.probe = bd71828_i2c_probe,
	.id_table = bd71828_dev_id,
};
EXPORT_SYMBOL(bd71828_in_shpm);

module_i2c_driver(bd71828_drv);

MODULE_AUTHOR("Matti Vaittinen <matti.vaittinen@fi.rohmeurope.com>");
MODULE_DESCRIPTION("ROHM BD71828 Power Management IC driver");
MODULE_LICENSE("GPL");
