/*
 * Maxim MAX20342 USB Charger Detector driver
 *
 * Copyright (C) 2020 Amazon.com Inc. All rights reserved.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>
#include <linux/mutex.h>
#include <linux/power_supply.h>

#ifdef CONFIG_CHARGER_HAL
#include <linux/power/charger-hal.h>
#endif

#ifdef CONFIG_CPU_FREQ_OVERRIDE_LAB126
#include <linux/cpufreq.h>
#endif


#define MAX20342_NAME		"max20342"
#define MAX20342_OF_NODE_NAME	"maxim,max20342"


//#define __AUTO_CONFIG__

/*
 * USER INTERRUPTS
 */
#define MAX20342_REVISION_ID		0x00
#define MAX20342_COMMON_INT		0x01
#define MAX20342_CC_INT			0x02
#define MAX20342_BC_INT			0x03
#define MAX20342_OVP_INT		0x04
#define MAX20342_RES_INT1		0x05
#define MAX20342_RES_INT2		0x06
#define MAX20342_COMMON_STATUS		0x07
#define MAX20342_CC_STATUS1		0x08
#define MAX20342_CC_STATUS2		0x09
#define MAX20342_BC_STATUS		0x0A
#define MAX20342_OVP_STATUS		0x0B
#define MAX20342_COMMON_MASK		0x0C
#define MAX20342_CC_MASK		0x0D
#define MAX20342_BC_MASK		0x0E
#define MAX20342_OVP_MASK		0x0F
#define MAX20342_RES_MASK1		0x10
#define MAX20342_RES_MASK2		0x11

/*
 * USER COMMON
 */
#define MAX20342_COMM_CTRL1		0x15
#define MAX20342_COMM_CTRL2		0x16
#define MAX20342_RFU_RW			0x17
#define MAX20342_RFU_RO			0x18
#define MAX20342_COMM_CTRL3		0x19

/*
 * USER OVP
 */
#define MAX20342_OVP_CTRL		0x1A

/*
 * USER USBC
 */
#define MAX20342_CC_CTRL0		0x20
#define MAX20342_CC_CTRL1		0x21
#define MAX20342_CC_CTRL2		0x22
#define MAX20342_CC_CTRL3		0x23
#define MAX20342_CC_CTRL4		0x24
#define MAX20342_CC_CTRL5		0x25
#define MAX20342_CC_CTRL6		0x26
#define MAX20342_VCONN_ILIM		0x28

/*
 * USER BC12
 */
#define MAX20342_BC_CTRL0		0x2A
#define MAX20342_BC_CTRL1		0x2B

/*
 * SBU_DET_RESULT
 */
#define MAX20342_SBU1DET_RESULT1	0x2C
#define MAX20342_SBU1DET_RESULT2	0x2D
#define MAX20342_SBU2DER_RESULT1	0x2E
#define MAX20342_SBU2DET_RESULT2	0x2F

/*
 * SBU_DET_CONFIG
 */
#define MAX20342_SBUDET_CTRL		0x30
#define MAX20342_RACC1DET_VMAX		0x31
#define MAX20342_RACC1DET_VMIN		0x32
#define MAX20342_RACC1DET_LPU		0x33
#define MAX20342_RACC2DET_VMAX		0x34
#define MAX20342_RACC2DET_VMI		0x35
#define MAX20342_RACC2DET_LPU		0x36
#define MAX20342_RACC3DET_VMAX		0x37
#define MAX20342_RACC3DET_VMI		0x38
#define MAX20342_RACC3DET_LPU		0x39
#define MAX20342_RACC4DET_VMAX		0x3A
#define MAX20342_RACC4DET_VMI		0x3B
#define MAX20342_RACC4DET_LPU		0x3C
#define MAX20342_RACC5DET_VMAX		0x3D
#define MAX20342_RACC5DET_VMI		0x3E
#define MAX20342_RACC5DET_LPU		0x3F

/*
 * MOIST_DET
 */
#define MAX20342_RMOISTDET_VTH			0x50
#define MAX20342_MOISTDET_CTRL			0x51
#define MAX20342_MOSITDET_PUCONFIG		0x52
#define MAX20342_MOSITDET_PDCONFIG		0x53
#define MAX20342_MOISTDET_AUTOCC1_RESULT1	0x54
#define MAX20342_MOISTDET_AUTOCC1_RESULT2	0x55
#define MAX20342_MOISTDET_AUTOCC2_RESULT1	0x56
#define MAX20342_MOISTDET_AUTOCC2_RESULT2	0x57
#define MAX20342_MOISTDET_AUTOSBU1_RESULT1	0x58
#define MAX20342_MOISTDET_AUTOSBU1_RESULT2	0x59
#define MAX20342_MOISTDET_AUTOSBU2_RESULT1	0x5A
#define MAX20342_MOISTDET_AUTOSBU2_RESULT2	0x5B

/*
 * ADC_CONFIG
 */
#define MAX20342_ADC_CTRL1		0x5C
#define MAX20342_ADC_CTRL2		0x5D
#define MAX20342_ADC_CTRL3		0x5E
#define MAX20342_ADC_CTRL4		0x5F
#define MAX20342_ADC_RESULT_AVG		0x60
#define MAX20342_ADC_RESULT_MAX		0x61
#define MAX20342_ADC_RESULT_MIN		0x62

/*
 * USER VB
 */
#define MAX20342_VB_CTRL		0x63


/* REGISTER MASK */
#define COMMON_INT_VB_VALID_INT_MASK	BIT(0)
#define COMMON_INT_V_SAFE_0V_INT_MASK	BIT(1)
#define COMMON_INT_SHDN_WAKE_INT_MASK	BIT(2)
#define COMMON_INT_LOW_PWR_INT_MASK	BIT(3)
#define COMMON_INT_THM_INT_MASK		BIT(4)
#define COMMON_INT_BAT_UVLO_INT_MASK	BIT(5)
#define COMMON_INT_BAT_OVLO_INT_MASK	BIT(6)
#define COMMON_INT_FAULT_INT_MASK	BIT(7)
#define COMMON_INT_VBUS_MASKS		0x03

#define BC_INT_CHG_DET_RUN_F_INT_MASK	BIT(1)
#define BC_INT_CHG_DET_RUN_R_INT_MASK	BIT(2)
#define BC_INT_PRCHG_TYP_INT_MASK	BIT(3)
#define BC_INT_CHG_TYP_INT_MASK		BIT(4)
#define BC_CHG_DET_MASKS		0x1a
#define BC_CHG_DET_RUN_MASKS		0x1e

#define OVP_INT_OVLO_INT_MASK		BIT(0)
#define OVP_INT_SWT_CLOSED_INT_MASK	BIT(1)

#define RES_INT1_RES_ACC1_INT_MASK	BIT(0)
#define RES_INT1_RES_ACC2_INT_MASK	BIT(1)
#define RES_INT1_RES_ACC3_INT_MASK	BIT(2)
#define RES_INT1_RES_ACC4_INT_MASK	BIT(3)
#define RES_INT1_RES_ACC5_INT_MASK	BIT(4)

#define RES_INT2_RES_MOIST_INT_MASK	BIT(0)
#define RES_INT2_RES_ABORT_INT_MASK	BIT(1)
#define RES_INT2_RES_OPEN_INT_MASK	BIT(2)
#define RES_INT2_RES_FINITE_INT_MASK	BIT(3)
#define RES_INT2_RES_SBU_INT_MASK	BIT(4)
#define RES_INT2_RES_GROUND_INT_MASK	BIT(5)
#define RES_INT2_MOIST_DET_MASKS	0x0f

#define COMMON_STATUS_VB_VALID_MASK	BIT(0)
#define COMMON_STATUS_V_SAFE_0V_MASK	BIT(1)
#define COMMON_STATUS_LOW_PWR_MASK	BIT(3)
#define COMMON_STATUS_THM_MASK		BIT(4)
#define COMMON_STATUS_BAT_UVLO_MASK	BIT(5)
#define COMMON_STATUS_BAT_OVLO_MASK	BIT(6)
#define COMMON_STATUS_FAULT_MASK	BIT(7)
#define COMMON_STATUS_FAULT_STATE_MASKS	0x50

#define CC_STATUS1_CHG_DET_ABORT_MASK	BIT(0)

#define BC_STATUS_DCD_TMO_MASK		BIT(0)
#define BC_STATUS_CHG_DET_RUN_MASK	BIT(1)

#define OVP_STATUS_OVLO_MASK		BIT(0)
#define OVP_STATUS_SWT_CLOSED_MASK	BIT(1)
#define OVP_STATUS_ITFRDY_MASK		BIT(7)

#define COMMON_MASK_SHDN_WAKE_MASK	BIT(2)
#define COMMON_MASK_LOW_PWR_MASK	BIT(3)
#define COMMON_MASK_THM_MASK		BIT(4)
#define COMMON_MASK_BAT_UVLO_MASK	BIT(5)
#define COMMON_MASK_BAT_OVLO_MASK	BIT(6)
#define COMMON_MASK_FAULT_MASK		BIT(7)

#define RES_MASK2_RES_MOIST_INT_MASK	BIT(0)
#define RES_MASK2_RES_ABORT_INT_MASK	BIT(1)
#define RES_MASK2_RES_OPEN_INT_MASK	BIT(2)
#define RES_MASK2_RES_FINITE_INT_MASK	BIT(3)
#define RES_MASK2_RES_SBU_INT_MASK	BIT(4)
#define RES_MASK2_RES_GROUND_INT_MASK	BIT(5)

#define COMM_CTRL1_SHDN_MODE_MASK	BIT(0)
#define COMM_CTRL1_LP_UFP_MASK		BIT(1)
#define COMM_CTRL1_USBAUTO_MASK		BIT(5)
#define COMM_CTRL1_FACTAUTO_MASK	BIT(6)
#define COMM_CTRL1_INTEN_MASK		BIT(7)

#define COMM_CTRL2_CE_FRC_MASK		BIT(0)
#define COMM_CTRL2_CE_MASK		BIT(1)
#define COMM_CTRL2_DB_MASK		BIT(4)
#define COMM_CTRL2_NOT_USB_CMPL_MASK	BIT(5)

#define COMM_CTRL3_FAULT_UNLOCK_MASK	BIT(0)
#define COMM_CTRL3_SW_RESET_MASK	BIT(1)

#define BC_CTRL0_CHG_DET_MAN_MASK	BIT(0)
#define BC_CTRL1_CHG_DET_EN_MASK	BIT(0)

#define MOISTDET_CTRL_MOISTDET_MAN_EN	BIT(2)
#define MOISTDET_CTRL_MOISTDET_PER_EN	BIT(3)
#define MOISTDET_CTRL_MOISTDET_AUTOCFG	BIT(4)

#define MOSITDET_PUCONFIG_CC1_MASK	BIT(0)
#define MOSITDET_PUCONFIG_CC2_MASK	BIT(1)
#define MOSITDET_PUCONFIG_SBU1_MASK	BIT(2)
#define MOSITDET_PUCONFIG_SBU2_MASK	BIT(3)
#define MOSITDET_PUCONFIG_CDP_MASK	BIT(4)
#define MOSITDET_PUCONFIG_CDN_MASK	BIT(5)

#define MOSITDET_PDCONFIG_CC1_MASK	BIT(0)
#define MOSITDET_PDCONFIG_CC2_MASK	BIT(1)
#define MOSITDET_PDCONFIG_SBU1_MASK	BIT(2)
#define MOSITDET_PDCONFIG_SBU2_MASK	BIT(3)
#define MOSITDET_PDCONFIG_CDP_MASK	BIT(4)
#define MOSITDET_PDCONFIG_CDN_MASK	BIT(5)
#define MOSITDET_PDCONFIG_VB_MASK	BIT(6)

#define MOISTDET_AUTO_RESULT_OPEN	BIT(2)

#define COMMON_MASK_DEFAULT		0xf7
#define CC_MASK_DEFAULT			0x00
#define BC_MASK_DEFAULT			0x1b
#define OVP_MASK_DEFAULT		0x03
#define RES_MASK1_DEFAULT		0x00
#ifdef __AUTO_CONFIG__
#define RES_MASK2_DEFAULT		0x1f  // Unmask open interrupt
#else
#define RES_MASK2_DEFAULT		0x1b  // Mask open interrupt
#endif

#define COMM_CTRL1_DEFAULT		0x62
#define COMM_CTRL2_DEFAULT		0xc1
#define COMM_CTRL3_DEFAULT		0x01  // FaultUnlock
#define OVP_CTRL_DEFAULT		0x02
#define BC_CTRL0_DEFAULT		0x00
#define BC_CTRL1_DEFAULT		0x01

#define CC_CTRL1_DEFAULT		0x89
#ifdef __AUTO_CONFIG__
#define MOISTDET_CTRL_DEFAULT		0x18
#else
#define MOISTDET_CTRL_DEFAULT		0x08
#endif
#define RMOISTDET_VTH_DEFAULT		204
#define MOSITDET_PUCONFIG_DEFAULT	0x00
#define MOSITDET_PDCONFIG_DEFAULT	0x00

#define REG_TO_READ			12
#define INIT_REG_TO_READ		7
#define ADC_VAL_REG_TO_READ		3
#define MOIST_DET_REG_TO_READ		8
#define ALLREG_TO_READ			27

/* Bits operation helper begin */
#define U8_FFS(_x) \
	((_x) & 0x0F ? \
		((_x) & 0x03 ? ((_x) & 0x01 ? 0 : 1) : \
		               ((_x) & 0x04 ? 2 : 3)) \
		         : \
		((_x) & 0x30 ? ((_x) & 0x10 ? 4 : 5) : \
		               ((_x) & 0x40 ? 6 : 7)) \
	)

#ifdef  BITS
#undef  BITS
#endif
#define BITS(_msb, _lsb) \
	(BIT(_msb) - BIT(_lsb) + BIT(_msb))

#define BITS_MASKED_GET(_val, _mask) \
	((_val) & (_mask)) >> U8_FFS(_mask)

#define BITS_MASKED_SET(_var, _mask, _val) \
	(_var) &= ~(_mask); \
	(_var) |= ((_val) << U8_FFS(_mask)) & (_mask);

#define U4_TO_BINARY_FORMAT "0b%c%c%c%c"
#define U4_TO_BINARY(_val) \
	((_val) & 0x08 ? '1' : '0'), \
	((_val) & 0x04 ? '1' : '0'), \
	((_val) & 0x02 ? '1' : '0'), \
	((_val) & 0x01 ? '1' : '0')

#define U3_TO_BINARY_FORMAT "0b%c%c%c"
#define U3_TO_BINARY(_val) \
	((_val) & 0x04 ? '1' : '0'), \
	((_val) & 0x02 ? '1' : '0'), \
	((_val) & 0x01 ? '1' : '0')

#define U2_TO_BINARY_FORMAT "0b%c%c"
#define U2_TO_BINARY(_val) \
	((_val) & 0x02 ? '1' : '0'), \
	((_val) & 0x01 ? '1' : '0')

#define U1_TO_BINARY_FORMAT "%c"
#define U1_TO_BINARY(_val) \
	((_val) & 0x01 ? '1' : '0')
/* Bits operation helper end */

#define USB_FACT_AUTO_MASK		BITS(6, 5)
#define USB_FACT_AUTO_FORMAT		U2_TO_BINARY_FORMAT
#define USB_FACT_AUTO_VALUE		U2_TO_BINARY

#define CCI_STAT_MASK			BITS(1, 0)
#define CCI_STAT_FORMAT			U2_TO_BINARY_FORMAT
#define CCI_STAT_VALUE			U2_TO_BINARY

#define USB_SWC_MASK			BITS(7, 6)
#define USB_SWC_FORMAT			U2_TO_BINARY_FORMAT
#define USB_SWC_VALUE			U2_TO_BINARY

#define USB_CHGTYP_MASK			BITS(6, 5)
#define USB_CHGTYP_FORMAT		U2_TO_BINARY_FORMAT
#define USB_CHGTYP_VALUE		U2_TO_BINARY

#define USB_PRCHGTYP_MASK		BITS(4, 2)
#define USB_PRCHGTYP_FORMAT		U3_TO_BINARY_FORMAT
#define USB_PRCHGTYP_VALUE		U3_TO_BINARY

#define USB_VB_OVP_EN_MASK		BITS(1, 0)
#define USB_VB_OVP_EN_FORMAT		U2_TO_BINARY_FORMAT
#define USB_VB_OVP_EN_VALUE		U2_TO_BINARY

#define R_MOIST_DET_I_PU_MASK		BITS(1, 0)
#define R_MOIST_DET_I_PU_FORMAT		U2_TO_BINARY_FORMAT
#define R_MOIST_DET_I_PU_VALUE		U2_TO_BINARY

#define I_PU_RESULT_MASK		BITS(7, 6)
#define I_PU_RESULT_FORMAT		U2_TO_BINARY_FORMAT
#define I_PU_RESULT_VALUE		U2_TO_BINARY

#define CCX_MOIST_I_PU_MASK		BITS(1, 0)
#define CCX_MOIST_I_PU_FORMAT		U2_TO_BINARY_FORMAT
#define CCX_MOIST_I_PU_VALUE		U2_TO_BINARY

#define SBUX_MOIST_I_PU_MASK		BITS(1, 0)
#define SBUX_MOIST_I_PU_FORMAT		U2_TO_BINARY_FORMAT
#define SBUX_MOIST_I_PU_VALUE		U2_TO_BINARY


#define INITIAL_CHECK_DELAY		4000
#define DETECT_WORK_DELAY		2500	/* DCD timeout max is 2200ms and BC 1.2 state timeout max is 220ms */
#define MOIST_MAN_DETECT_WORK_DELAY	1000	/* 1s for man detection */
#define MOIST_DETECTED_WORK_DELAY	300000	/* Detect moisture every 5 minutes */
#define MOIST_FORCE_DETECTED_WORK_DELAY	10000	/* 10s for force water_detected mode */

#define GPIO_INT_ACTIVE_LOW		1


#define MOISTURE_NAME			"max20342_moisture"

/*
 * Changer types are defined by Maxim, don't insert or reorder
 */
enum max20342_chg_type {
	MAX20342_NO_CHARGER	= 0,
	MAX20342_SDP_CHARGER,
	MAX20342_CDP_CHARGER,
	MAX20342_DCP_CHARGER,
	MAX20342_SAMSUNG_2A_CHARGER,
	MAX20342_APPLE_500MA_CHARGER,
	MAX20342_APPLE_1A_CHARGER,
	MAX20342_APPLE_2A_CHARGER,
	MAX20342_APPLE_12W_CHARGER,
	MAX20342_CCI_500MA_CHARGER,
	MAX20342_CCI_1500MA_CHARGER,
	MAX20342_CCI_3A_CHARGER,
	MAX20342_WET_CHARGER,
	MAX20342_CHARGER_UNKNOWN
};

struct max20342_chg_type_props {
	enum power_supply_type type;
	int current_limit;
};

/*
 * Moisture detection status
 */
enum max20342_moist_det_state {
	MAX20342_MOIST_MANUAL_DP	= 0,
	MAX20342_MOIST_MANUAL_DN,
#ifdef __AUTO_CONFIG__
	MAX20342_MOIST_AUTOCONFIG,
#endif
	MAX20342_MOIST_MANUAL_SUB1,
	MAX20342_MOIST_MANUAL_SUB2
};

static const struct max20342_chg_type_props chg_type_props[] = {
	{ POWER_SUPPLY_TYPE_UNKNOWN,  500 },
	{ POWER_SUPPLY_TYPE_USB,      500 }, /* SDP */
	{ POWER_SUPPLY_TYPE_USB_CDP, 1500 }, /* CDP */
	{ POWER_SUPPLY_TYPE_USB_DCP, 1500 }, /* DCP */
	{ POWER_SUPPLY_TYPE_USB_DCP, 2000 }, /* Samsung 2A DCP */
	{ POWER_SUPPLY_TYPE_USB_DCP,  500 }, /* Apple 500mA */
	{ POWER_SUPPLY_TYPE_USB_DCP, 1000 }, /* Apple 1A */
	{ POWER_SUPPLY_TYPE_USB_DCP, 2000 }, /* Apple 2A */
	{ POWER_SUPPLY_TYPE_USB_DCP, 2400 }, /* Apple 12W, max current is 2.4A */
	{ POWER_SUPPLY_TYPE_USB_DCP,  500 }, /* CC pin detected and max VB current allowed is 500mA */
	{ POWER_SUPPLY_TYPE_USB_DCP, 1500 }, /* CC pin detected and max VB current allowed is 1.5A */
	{ POWER_SUPPLY_TYPE_USB_DCP, 3000 }, /* CC pin detected and max VB current allowed is 3.0A */
	{ POWER_SUPPLY_TYPE_USB_WET,  100 }, /* Charging current when water is detected */
};

struct max20342_chip {
	struct i2c_client				*client;
	const struct attribute_group			*attrs_group;

	struct delayed_work				init_work;
	struct delayed_work				detect_work;
#ifdef __AUTO_CONFIG__
	struct delayed_work				moist_man_detect_work;
#endif
	struct delayed_work				moist_detected_work;
	struct mutex					work_lock;
	struct power_supply_desc			wet_desc;
	struct power_supply_config			wet_cfg;
	struct power_supply				*wet_psy;
	enum max20342_chg_type 				chg_type;
	enum max20342_moist_det_state			moist_det_state;

	int						gpio_chg_det_int;
	int						irq_chg_det_int;
	int						gpio_chg_det_ceb;
#ifdef __AUTO_CONFIG__
	u8						nr_open_pin_det;
#endif

	struct kobject					*kobj;

	bool						fault:1;		// max20342 fault state flag
	bool						shdn_mode:1;		// shutdown mode flag
	bool 						vb_ovp_swc_ctrl:1;
	bool 						chg_redet:1;		// charger re-detection flag
	bool						moist_detected:1; 	// moist_detected mode flag
	bool						moist_force_detected:1; // force device to be moist_detected mode flag
#ifdef __AUTO_CONFIG__
	bool 						moist_check:1;
#endif

#define ONLINE						0
#define INT_ENABLED					1
#define SBU_CABLE_DETECTED				2
	unsigned long					bitflags;
};


/*
 * Read single register
 */
static int max20342_read_reg(struct i2c_client *client, int reg, u8 *val)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0) {
		pr_err("%s:%d i2c read fail: can't read from %02x: %d\n", __func__, __LINE__, reg, ret);
		return ret;
	}
	*val = ret;
	return 0;
}

/*
 * Write single register
 */
static int max20342_write_reg(struct i2c_client *client, int reg, u8 val)
{
	s32 ret;

	ret = i2c_smbus_write_byte_data(client, reg, val);
	if (ret < 0) {
		pr_err("%s:%d i2c write fail: can't write %02x to %02x: %d\n", __func__, __LINE__, val, reg, ret);
		return ret;
	}
	return 0;
}

/*
 * Read contiguous registers
 */
static int max20342_read_block_reg(struct i2c_client *client, u8 reg, u8 length, u8 *val)
{
	s32 ret;

	ret = i2c_smbus_read_i2c_block_data(client, reg, length, val);
	if (ret < 0) {
		pr_err("%s:%d failed to block read reg 0x%x: %d\n", __func__, __LINE__, reg, ret);
		return ret;
	}

	return 0;
}


/*
 * Update register bits
 */
static int max20342_update_reg_bits(struct i2c_client *client, u8 reg, u8 mask, u8 val)
{
	u8 reg_val;
	s32 ret;

	ret = max20342_read_reg(client, reg, &reg_val);
	if (ret)
		return ret;

	BITS_MASKED_SET(reg_val, mask, val);

	ret = max20342_write_reg(client, reg, reg_val);
	if (ret)
		return ret;

	return 0;
}


#if defined(CONFIG_LAB126) && defined(CONFIG_TOI)
extern void cancel_toi_hibernation(void);
#endif

// Enable USB module
//extern void mt_usb_connect(void);
//extern void mt_usb_disconnect(void);

/* This function is called after binding fsg */
/* DON'T reuse g_chip, it acts as an enclosure to usb_gadget_installed */
static struct max20342_chip *g_chip = NULL;

int usb_gadget_installed(void)
{
	struct max20342_chip *chip = g_chip;

	if (!g_chip) {
		return 0;
	}
	g_chip = NULL;

	/* Trigger a manual detection */
	pr_info("%s:%d Trigger a manual detection\n", __func__, __LINE__);

	if (max20342_update_reg_bits(chip->client, MAX20342_BC_CTRL0, BC_CTRL0_CHG_DET_MAN_MASK, 1))
		return -ENODEV;

	return 0;
}
EXPORT_SYMBOL(usb_gadget_installed);


/*
 * Check if there is pending interrupt(s)
 */
static inline bool max20342_is_int_pending(struct max20342_chip *chip)
{
	if (test_bit(INT_ENABLED, &chip->bitflags)) {
#if GPIO_INT_ACTIVE_LOW
		return gpio_get_value(chip->gpio_chg_det_int) ? false : true;
#else
		return gpio_get_value(chip->gpio_chg_det_int) ? true : false;
#endif /* GPIO_INT_ACTIVE_LOW */
	} else {
		return false;
	}
}


void usb_gadget_chg_det_sync(int delay_ms)
{
	struct max20342_chip *chip = g_chip;

	if (!g_chip) {
		pr_info("charger driver is ready \n");
		return;
	}

#ifdef CONFIG_CPU_FREQ_OVERRIDE_LAB126
	cpufreq_override(2);
#endif

	pr_info("%s\n", __func__);
	cancel_delayed_work(&chip->detect_work);
	schedule_delayed_work(&chip->detect_work, msecs_to_jiffies(delay_ms));
}
EXPORT_SYMBOL(usb_gadget_chg_det_sync);


static int max20342_init_regs(struct max20342_chip *chip, bool shdn_wakeup)
{
	struct i2c_client *client = chip->client;

	/*
	 * INTERRUPT MASKS
	 */
	if (max20342_write_reg(client, MAX20342_COMMON_MASK, COMMON_MASK_DEFAULT))
		return -EINVAL;

	if (max20342_write_reg(client, MAX20342_CC_MASK, CC_MASK_DEFAULT))
		return -EINVAL;

	if (max20342_write_reg(client, MAX20342_BC_MASK, BC_MASK_DEFAULT))
		return -EINVAL;

	if (max20342_write_reg(client, MAX20342_OVP_MASK, OVP_MASK_DEFAULT))
		return -EINVAL;

	if (max20342_write_reg(client, MAX20342_RES_MASK1, RES_MASK1_DEFAULT))
		return -EINVAL;

	if (max20342_write_reg(client, MAX20342_RES_MASK2, RES_MASK2_DEFAULT))
		return -EINVAL;

	/*
	 * USER COMMON
	 */
	// USBAuto and FactAuto are set
	if (shdn_wakeup) {
		if (max20342_write_reg(client, MAX20342_COMM_CTRL1, COMM_CTRL1_DEFAULT|COMM_CTRL1_INTEN_MASK))
			return -EINVAL;
	}
	else {
		if (max20342_write_reg(client, MAX20342_COMM_CTRL1, COMM_CTRL1_DEFAULT))
			return -EINVAL;
	}

	// Control CE pin manually at charger detection function.
	if (max20342_write_reg(client, MAX20342_COMM_CTRL2, COMM_CTRL2_DEFAULT))
		return -EINVAL;

	if (max20342_write_reg(client, MAX20342_COMM_CTRL3, COMM_CTRL3_DEFAULT))
		return -EINVAL;

	if (max20342_write_reg(client, MAX20342_OVP_CTRL, OVP_CTRL_DEFAULT))
		return -EINVAL;
	/*
	 * USER USBC
	 */
	// Disable CC_AUD_EN
	if (max20342_write_reg(client, MAX20342_CC_CTRL1, CC_CTRL1_DEFAULT))
		return -EINVAL;

	/*
	 * USER BC12
	 */
	if (max20342_write_reg(client, MAX20342_BC_CTRL0, BC_CTRL0_DEFAULT))
		return -EINVAL;

	if (max20342_write_reg(client, MAX20342_BC_CTRL1, BC_CTRL1_DEFAULT))
		return -EINVAL;

	/*
	 * MOIST DET
	 */
	if (max20342_write_reg(client, MAX20342_MOISTDET_CTRL, MOISTDET_CTRL_DEFAULT))
		return -EINVAL;

	if (max20342_write_reg(client, MAX20342_RMOISTDET_VTH, RMOISTDET_VTH_DEFAULT))
		return -EINVAL;

#ifdef __AUTO_CONFIG__
	// 1. MAX20342_MOSITDET_PUCONFIG
	if (max20342_write_reg(chip->client, MAX20342_MOSITDET_PUCONFIG, MOSITDET_PUCONFIG_DEFAULT))
		return -EINVAL;

	// 2. MAX20342_MOSITDET_PDCONFIG
	if (max20342_write_reg(chip->client, MAX20342_MOSITDET_PDCONFIG, MOSITDET_PDCONFIG_DEFAULT))
		return -EINVAL;
#else
	// For SBU1
	// 1. MAX20342_MOSITDET_PUCONFIG
	if (max20342_write_reg(chip->client, MAX20342_MOSITDET_PUCONFIG, MOSITDET_PUCONFIG_SBU1_MASK))
		return -EINVAL;

	// 2. MAX20342_MOSITDET_PDCONFIG
	if (max20342_write_reg(chip->client, MAX20342_MOSITDET_PDCONFIG, ~(u8)MOSITDET_PDCONFIG_SBU1_MASK))
		return -EINVAL;
#endif

	return 0;
}

static enum max20342_chg_type max20342_inquiry_charger_type_with_ccistat(u8 cc_status2){
	int cci_stat = 0;


	cci_stat = BITS_MASKED_GET(cc_status2, CCI_STAT_MASK);
	pr_info("%s:%d cci_stat="CCI_STAT_FORMAT"\n", __func__, __LINE__,
				CCI_STAT_VALUE(cci_stat));

	if (cci_stat == 3)
		return MAX20342_CCI_3A_CHARGER;
	else if (cci_stat == 1)
		return MAX20342_CCI_500MA_CHARGER;
	else if (cci_stat == 2)
		return MAX20342_CCI_1500MA_CHARGER;
	else if(cci_stat == 0)
		return MAX20342_NO_CHARGER;

	return MAX20342_CHARGER_UNKNOWN;
}

static enum max20342_chg_type max20342_inquiry_charger_type(u8 bc_status){
	int chg_typ = 0;
	int pr_chg_typ = 0;
	enum max20342_chg_type ret;


	chg_typ = BITS_MASKED_GET(bc_status, USB_CHGTYP_MASK);
	pr_chg_typ = BITS_MASKED_GET(bc_status, USB_PRCHGTYP_MASK);

	pr_info("%s:%d chg_type="USB_CHGTYP_FORMAT" pr_chg_type="USB_PRCHGTYP_FORMAT"\n", __func__, __LINE__,
				USB_CHGTYP_VALUE(chg_typ), USB_PRCHGTYP_VALUE(pr_chg_typ));

	if(chg_typ == 0)
		return MAX20342_NO_CHARGER;

	ret = MAX20342_CHARGER_UNKNOWN;

	switch (pr_chg_typ) {
		case 0:
			switch (chg_typ) {
				case 0:
					ret = MAX20342_NO_CHARGER;
					break;
				case 1:
					ret = MAX20342_SDP_CHARGER;
					break;
				case 2:
					ret = MAX20342_CDP_CHARGER;
					break;
				case 3:
					ret = MAX20342_DCP_CHARGER;
					break;
			}
			break;
		case 1:
			if(chg_typ == 3)
				ret = MAX20342_SAMSUNG_2A_CHARGER;
			break;
		case 2:
			if(chg_typ == 1)
				ret = MAX20342_APPLE_500MA_CHARGER;
			break;
		case 3:
			if(chg_typ == 2)
				ret = MAX20342_APPLE_1A_CHARGER;
			break;
		case 4:
			if(chg_typ == 1)
				ret = MAX20342_APPLE_2A_CHARGER;
			break;
		case 5:
			if(chg_typ == 3)
				ret = MAX20342_APPLE_12W_CHARGER;
			break;
	}

	return ret;
}

static int max20342_update_wet_chg_state(struct max20342_chip *chip, const u8 *buf)
{
	/* Do not check charger type if the charger detection state machine is still running */
	if (!(buf[MAX20342_BC_STATUS] & BC_STATUS_CHG_DET_RUN_MASK)) {
		if (buf[MAX20342_COMMON_STATUS] & COMMON_STATUS_V_SAFE_0V_MASK) {
			pr_info("%s:%d bitflags: 0x%02lx\n", __func__, __LINE__, chip->bitflags);

			/*
			 * When connecting to an Apple C-to-C cable, vbus varies between 0.6V to 5V.
			 * So, we will keep seeing Vbus valid interrupt during the cable is inserted.
			 */

			if (!test_and_set_bit(ONLINE, &chip->bitflags)) {
#ifdef CONFIG_CHARGER_HAL
				charger_hal_charger_update_status(chg_type_props[MAX20342_WET_CHARGER].type,
					chg_type_props[MAX20342_WET_CHARGER].current_limit, true);
#endif
				if (max20342_write_reg(chip->client, MAX20342_RES_MASK2, 0x00))
					return -EINVAL;

				if ((buf[MAX20342_RES_INT2] == RES_MASK2_RES_SBU_INT_MASK) &&
					(buf[MAX20342_RES_INT1] == RES_INT1_RES_ACC3_INT_MASK)) {
					pr_info("max20342 wet cable in\n");
					set_bit(SBU_CABLE_DETECTED, &chip->bitflags);
				}
				else {
					pr_info("max20342 wet cable in uevent\n");
					// Send uevent to framework
					kobject_uevent_env(chip->kobj, KOBJ_CHANGE, (char *[]){"WETUSB=PLUG", NULL});
				}
			}
		}
		else {
#ifdef CONFIG_CHARGER_HAL
			charger_hal_charger_update_status(chg_type_props[MAX20342_WET_CHARGER].type,
				chg_type_props[MAX20342_WET_CHARGER].current_limit, false);
#endif
			pr_info("%s:%d bitflags: 0x%02lx\n", __func__, __LINE__, chip->bitflags);

			if (test_and_clear_bit(ONLINE, &chip->bitflags)) {
				if (!(test_and_clear_bit(SBU_CABLE_DETECTED, &chip->bitflags))) {
					// Send uevent to framework
					pr_info("max20342 wet cable out uevent\n");
					kobject_uevent_env(chip->kobj, KOBJ_CHANGE, (char *[]){"WETUSB=UNPLUG", NULL});
				}
				else {
					pr_info("max20342 wet cable out\n");
				}
			}

			if (max20342_write_reg(chip->client, MAX20342_RES_MASK2, RES_MASK2_DEFAULT|RES_INT2_RES_OPEN_INT_MASK))
				return -EINVAL;

			if (max20342_update_reg_bits(chip->client, MAX20342_MOISTDET_CTRL, MOISTDET_CTRL_MOISTDET_MAN_EN, 1)) {
				pr_err("%s:%d failed to update registers\n", __func__, __LINE__);
				return -EINVAL;
			}

#ifdef __AUTO_CONFIG__
			/* Schedule deferred detection in case of no interrupt for open and abort of auto configuration */
			if (chip->moist_det_state == MAX20342_MOIST_AUTOCONFIG) {
				cancel_delayed_work(&chip->moist_man_detect_work);
				schedule_delayed_work(&chip->moist_man_detect_work, msecs_to_jiffies(MOIST_MAN_DETECT_WORK_DELAY));
				chip->moist_check = true;
			}
#endif
		}
	}

	return 0;
}

static int max20342_update_chg_state(struct max20342_chip *chip, const u8 *buf)
{
	/* Do not check charger type if the charger detection state machine is still running */
	if (!(buf[MAX20342_BC_STATUS] & BC_STATUS_CHG_DET_RUN_MASK)) {
		/* Check and update charger type */
		chip->chg_type = max20342_inquiry_charger_type(buf[MAX20342_BC_STATUS]);

		// If some chargers are detected as unknown chargers, we check CCIStat further
		if ((chip->chg_type == MAX20342_CHARGER_UNKNOWN)
				&& (buf[MAX20342_COMMON_STATUS] & COMMON_STATUS_VB_VALID_MASK)) {
			chip->chg_type = max20342_inquiry_charger_type_with_ccistat(buf[MAX20342_CC_STATUS2]);
		}

		// If some chargers are detected as unknown or CCI 500mA chargers, we re-detect for only once
		if (((chip->chg_type == MAX20342_CHARGER_UNKNOWN) || (chip->chg_type == MAX20342_CCI_500MA_CHARGER))
				&& (buf[MAX20342_COMMON_STATUS] & COMMON_STATUS_VB_VALID_MASK)
				&& (chip->chg_redet == false) && (chip->vb_ovp_swc_ctrl == false)) {
			chip->chg_redet = true;
			pr_info("max20342 re-detects unknown charger\n");

			// Connect Vbus
			// Without this code, we will see DC-in => DC-remove -> DC-in during the charger redetect
			// Set VBOVPEn as 01 (switch closed when VB > VBDET)
			if (max20342_update_reg_bits(chip->client, MAX20342_OVP_CTRL, USB_VB_OVP_EN_MASK, 1))
				return -EINVAL;

			/* Trigger a manual detection */
			if (max20342_update_reg_bits(chip->client, MAX20342_BC_CTRL0, BC_CTRL0_CHG_DET_MAN_MASK, 1))
				return -EINVAL;
			return 0;
		}

		if (chip->chg_redet) {
			chip->chg_redet = false;
			if (chip->vb_ovp_swc_ctrl == false) {
				// Set VBOVPEn as 10 (switch based on table 4)
				if (max20342_update_reg_bits(chip->client, MAX20342_OVP_CTRL, USB_VB_OVP_EN_MASK, 2))
					return -EINVAL;
			}
		}

		if ((chip->chg_type != MAX20342_NO_CHARGER) && (buf[MAX20342_COMMON_STATUS] & COMMON_STATUS_VB_VALID_MASK)) {
#ifdef CONFIG_CHARGER_HAL
			if (chip->vb_ovp_swc_ctrl == false) {
				if (chip->chg_type < MAX20342_CHARGER_UNKNOWN) {
					charger_hal_charger_update_status(chg_type_props[chip->chg_type].type,
						chg_type_props[chip->chg_type].current_limit, true);
				} else {
					charger_hal_charger_update_status(POWER_SUPPLY_TYPE_UNKNOWN,
						chg_type_props[POWER_SUPPLY_TYPE_UNKNOWN].current_limit, true);
				}
			}
#endif
			pr_info("%s:%d bitflags: 0x%02lx\n", __func__, __LINE__, chip->bitflags);

			if (!test_and_set_bit(ONLINE, &chip->bitflags)) {
				pr_info("max20342 cable in\n");

				// Mask all res2 interrupts
				if (max20342_write_reg(chip->client, MAX20342_RES_MASK2, 0x00))
					return -EINVAL;

				// Turn on USB module only if connecting to USB host
				if ((chip->chg_type == MAX20342_CDP_CHARGER) || (chip->chg_type == MAX20342_SDP_CHARGER)) {
					if (max20342_update_reg_bits(chip->client, MAX20342_COMM_CTRL2, COMM_CTRL2_CE_MASK, 1))
						return -EINVAL;
				}

				/* Force CDP/CDN switch to TDP/TDN when CDP is detected */
				if (chip->chg_type == MAX20342_CDP_CHARGER) {
					pr_info("CDP detected, force switch to USB\n");

					/*
					 * If VBOVPEN is set through sys entry, we will not set automatically here
					 * it is useful when testing wireless charger function
					 */
					if (chip->vb_ovp_swc_ctrl == false) {
						// Set VBOVPEn as 01 (switch closed when VB > VBDET)
						if (max20342_update_reg_bits(chip->client, MAX20342_OVP_CTRL, USB_VB_OVP_EN_MASK, 1))
							return -EINVAL;
					}

					// Force CDP/CDN switch to TDP/TDN position
					if (max20342_update_reg_bits(chip->client, MAX20342_COMM_CTRL2, USB_SWC_MASK, 2))
						return -EINVAL;
				}
			}
		} else {
#ifdef CONFIG_CHARGER_HAL
			charger_hal_charger_update_status(POWER_SUPPLY_TYPE_UNKNOWN,
				chg_type_props[POWER_SUPPLY_TYPE_UNKNOWN].current_limit, false);
#endif

			pr_info("%s:%d bitflags: 0x%02lx\n", __func__, __LINE__, chip->bitflags);

			if (test_and_clear_bit(ONLINE, &chip->bitflags)) {
				pr_info("max20342 cable out\n");

				// Unmask all res2 interrupts
				if (max20342_write_reg(chip->client, MAX20342_RES_MASK2, RES_MASK2_DEFAULT))
					return -EINVAL;

				// Turn off USB module
				if (max20342_update_reg_bits(chip->client, MAX20342_COMM_CTRL2, COMM_CTRL2_CE_MASK, 0))
					return -EINVAL;

				/* Set back as USBAuto mode when USB cable is plugged out */
				chip->vb_ovp_swc_ctrl = false;
				// Set VBOVPEn as 10 (switch based on table 4)
				if (max20342_update_reg_bits(chip->client, MAX20342_OVP_CTRL, USB_VB_OVP_EN_MASK, 2))
					return -EINVAL;

				// Set CDP/CDN switch to follow FSM
				if (max20342_update_reg_bits(chip->client, MAX20342_COMM_CTRL2, USB_SWC_MASK, 3))
					return -EINVAL;
			}
		}
	}

	return 0;
}

static int max20342_check_moisture_detect_result(struct max20342_chip *chip, u8 *ipu, u8 *adcbuf)
{
	// Read I_PU_RESULT (pull up current)
	if (max20342_read_reg(chip->client, MAX20342_ADC_CTRL2, ipu))
		return -EINVAL;

	// Read ADC results
	if (max20342_read_block_reg(chip->client, MAX20342_ADC_RESULT_AVG, ADC_VAL_REG_TO_READ, adcbuf))
		return -EINVAL;

	*ipu = BITS_MASKED_GET(*ipu, I_PU_RESULT_MASK);
	pr_info("IpuResult: "I_PU_RESULT_FORMAT", ADC_Avg:%d, ADC_Max:%d, ADC_Min:%d\n", I_PU_RESULT_VALUE(*ipu), adcbuf[0], adcbuf[1], adcbuf[2]);
	return 0;
}

static int max20342_mancfg_moist_detect_handler(struct max20342_chip *chip, u8 res_int2)
{
	u8 adcbuf[ADC_VAL_REG_TO_READ] = {0};
	u8 ipu = 0;


	if (max20342_check_moisture_detect_result(chip, &ipu, adcbuf))
		return -EINVAL;

	if (res_int2 & RES_INT2_RES_FINITE_INT_MASK) {
		if (chip->moist_detected) {
			cancel_delayed_work(&chip->moist_detected_work);
			schedule_delayed_work(&chip->moist_detected_work, msecs_to_jiffies(MOIST_DETECTED_WORK_DELAY));
			pr_info("Finite: Trigger moist_detected_worker\n");
		}
		return 0;
	}

	if ((res_int2 & RES_INT2_RES_OPEN_INT_MASK) && (chip->moist_detected) && (chip->moist_force_detected == false)) {
		if (chip->moist_det_state == MAX20342_MOIST_MANUAL_SUB1) {
			pr_info("SUB1 is open\n");

			chip->moist_det_state = MAX20342_MOIST_MANUAL_SUB2;

			// 1. MAX20342_MOSITDET_PUCONFIG
			if (max20342_write_reg(chip->client, MAX20342_MOSITDET_PUCONFIG, MOSITDET_PUCONFIG_SBU2_MASK))
				return -EINVAL;

			// 2. MAX20342_MOSITDET_PDCONFIG
			if (max20342_write_reg(chip->client, MAX20342_MOSITDET_PDCONFIG, ~(u8)MOSITDET_PDCONFIG_SBU2_MASK))
				return -EINVAL;

			// 3. Manual detection for SUB2 pin
			if (max20342_update_reg_bits(chip->client, MAX20342_MOISTDET_CTRL, MOISTDET_CTRL_MOISTDET_MAN_EN, 1))
				return -EINVAL;
		}
		else if (chip->moist_det_state == MAX20342_MOIST_MANUAL_SUB2) {
			pr_info("SUB2 is open\n");

			chip->moist_det_state = MAX20342_MOIST_MANUAL_DP;

			// 1. MAX20342_MOSITDET_PUCONFIG
			if (max20342_write_reg(chip->client, MAX20342_MOSITDET_PUCONFIG, MOSITDET_PUCONFIG_CDP_MASK))
				return -EINVAL;

			// 2. MAX20342_MOSITDET_PDCONFIG
			if (max20342_write_reg(chip->client, MAX20342_MOSITDET_PDCONFIG, ~(u8)MOSITDET_PDCONFIG_CDP_MASK))
				return -EINVAL;

			// 3. Manual detection for DP pin
			if (max20342_update_reg_bits(chip->client, MAX20342_MOISTDET_CTRL, MOISTDET_CTRL_MOISTDET_MAN_EN, 1))
				return -EINVAL;
		}
		else if (chip->moist_det_state == MAX20342_MOIST_MANUAL_DP) {
			pr_info("DP is open\n");

			chip->moist_det_state = MAX20342_MOIST_MANUAL_DN;

			// 1. MAX20342_MOSITDET_PUCONFIG
			if (max20342_write_reg(chip->client, MAX20342_MOSITDET_PUCONFIG, MOSITDET_PUCONFIG_CDN_MASK))
				return -EINVAL;

			// 2. MAX20342_MOSITDET_PDCONFIG
			if (max20342_write_reg(chip->client, MAX20342_MOSITDET_PDCONFIG, ~(u8)MOSITDET_PDCONFIG_CDN_MASK))
				return -EINVAL;

			// 3. Manual detection for DN pin
			if (max20342_update_reg_bits(chip->client, MAX20342_MOISTDET_CTRL, MOISTDET_CTRL_MOISTDET_MAN_EN, 1))
				return -EINVAL;
		}
		else if (chip->moist_det_state == MAX20342_MOIST_MANUAL_DN) {
			pr_info("DN is open, no water exists\n");

		#ifdef __AUTO_CONFIG__
			chip->moist_det_state = MAX20342_MOIST_AUTOCONFIG;

			// 1. MAX20342_MOSITDET_PUCONFIG
			if (max20342_write_reg(chip->client, MAX20342_MOSITDET_PUCONFIG, MOSITDET_PUCONFIG_DEFAULT))
				return -EINVAL;

			// 2. MAX20342_MOSITDET_PDCONFIG
			if (max20342_write_reg(chip->client, MAX20342_MOSITDET_PDCONFIG, MOSITDET_PDCONFIG_DEFAULT))
				return -EINVAL;

			// 3. Enable autoconfig
			if (max20342_update_reg_bits(chip->client, MAX20342_MOISTDET_CTRL, MOISTDET_CTRL_MOISTDET_AUTOCFG, 1))
				return -EINVAL;
		#else
			chip->moist_det_state = MAX20342_MOIST_MANUAL_SUB1;

			// 1. MAX20342_MOSITDET_PUCONFIG
			if (max20342_write_reg(chip->client, MAX20342_MOSITDET_PUCONFIG, MOSITDET_PUCONFIG_SBU1_MASK))
				return -EINVAL;

			// 2. MAX20342_MOSITDET_PDCONFIG
			if (max20342_write_reg(chip->client, MAX20342_MOSITDET_PDCONFIG, ~(u8)MOSITDET_PDCONFIG_SBU1_MASK))
				return -EINVAL;

			// 3. Mask open interrupt
			if (max20342_update_reg_bits(chip->client, MAX20342_RES_MASK2, RES_INT2_RES_OPEN_INT_MASK, 0))
				return -ENODEV;
		#endif
			if (chip->moist_detected == true) {
				chip->moist_detected = false;

				// 1. Set back to 10s period auto detection
				if (max20342_update_reg_bits(chip->client, MAX20342_MOISTDET_CTRL, MOISTDET_CTRL_MOISTDET_PER_EN, 1))
					return -EINVAL;

				pr_info("Sent unwet uevent\n");
				// 2. Send uevent, online means water is done and USB can work hereafter
				kobject_uevent(chip->kobj, KOBJ_ONLINE);
			}
		}
		return 0;
	}

	// Moisture is detected at SUB1/SUB2/DP/DN pin
	if ((res_int2 & RES_INT2_RES_MOIST_INT_MASK) || (chip->moist_force_detected == true)) {

		if (chip->moist_det_state == MAX20342_MOIST_MANUAL_DP)
			pr_info("Moist det: Moist detected at DP!!\n");
		else if (chip->moist_det_state == MAX20342_MOIST_MANUAL_DN)
			pr_info("Moist det: Moist detected at DN!!\n");
#ifndef __AUTO_CONFIG__
		else if (chip->moist_det_state == MAX20342_MOIST_MANUAL_SUB1)
			pr_info("Moist det: Moist detected at SUB1!!\n");
		else if (chip->moist_det_state == MAX20342_MOIST_MANUAL_SUB2)
			pr_info("Moist det: Moist detected at SUB2!!\n");
#endif

		if (chip->moist_detected == false) {
			chip->moist_detected = true;

			// 1. Disable 10s periodic measurement
			if (max20342_update_reg_bits(chip->client, MAX20342_MOISTDET_CTRL, MOISTDET_CTRL_MOISTDET_PER_EN, 0))
				return -EINVAL;

			pr_info("Sent wet uevent\n");
			// 2. Send uevent, offline means water is detected and USB cannot work hereafter
			kobject_uevent(chip->kobj, KOBJ_OFFLINE);

			// 3. Unmask open interrupt
			if (max20342_update_reg_bits(chip->client, MAX20342_RES_MASK2, RES_INT2_RES_OPEN_INT_MASK, 1))
				return -ENODEV;
		}

		// 3. Trigger a 5 min delayed work to re-check moisture
		cancel_delayed_work(&chip->moist_detected_work);

		if (likely(chip->moist_force_detected == false))
			schedule_delayed_work(&chip->moist_detected_work, msecs_to_jiffies(MOIST_DETECTED_WORK_DELAY));
		else
			schedule_delayed_work(&chip->moist_detected_work, msecs_to_jiffies(MOIST_FORCE_DETECTED_WORK_DELAY));
		pr_info("Moist: Trigger moist_detected_worker\n");
		return 0;
	}

	if (unlikely(res_int2 & RES_INT2_RES_ABORT_INT_MASK)) {
		pr_info("Abort interrupt! re-det\n");
		if (max20342_update_reg_bits(chip->client, MAX20342_MOISTDET_CTRL, MOISTDET_CTRL_MOISTDET_MAN_EN, 1)) {
			pr_err("%s:%d failed to update registers\n", __func__, __LINE__);
			return -EINVAL;
		}
	}

	return 0;
}

#ifdef __AUTO_CONFIG__
static int max20342_autocfg_moist_detect_handler(struct max20342_chip *chip, u8 res_int2)
{
	u8 adcbuf[ADC_VAL_REG_TO_READ] = {0};
	u8 buf[MOIST_DET_REG_TO_READ] = {0};
	u8 ipu = 0;
	u8 val = 0;


	// Read moist_det_man bit
	if (max20342_read_reg(chip->client, MAX20342_MOISTDET_CTRL, &val)) {
		pr_err("%s:%d failed to read registers\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (val & MOISTDET_CTRL_MOISTDET_MAN_EN) {
		pr_info("MOISTDET_MAN_EN is set\n");
		/* Schedule deferred detection in case of no interrupt for open and abort of auto configuration */
		if (chip->moist_det_state == MAX20342_MOIST_AUTOCONFIG) {
			schedule_delayed_work(&chip->moist_man_detect_work, msecs_to_jiffies(MOIST_MAN_DETECT_WORK_DELAY));
			chip->moist_check = true;
		}
		return 0;
	}

	chip->moist_check = false;
	if (max20342_check_moisture_detect_result(chip, &ipu, adcbuf))
		return -EINVAL;

	// Open: I_PU_RESULT = 00 && ADC_RESULT_AVG = 0xff
	if (((res_int2 & RES_INT2_RES_FINITE_INT_MASK) || ((ipu == 0) && (adcbuf[0] == 255)))
						&& (chip->moist_force_detected == false)) {
		if (likely(res_int2 & RES_INT2_RES_FINITE_INT_MASK))
			pr_info("Moist det: Finite res!!\n");
		else if ((ipu == 0) && (adcbuf[0] == 255))
			pr_info("Moist det: Open!!\n");

		if (chip->moist_detected == true) {
			chip->nr_open_pin_det++;

			if (chip->nr_open_pin_det < 2) {
				// Trigger a 5 min delayed work to re-check moisture of another CC pin
				pr_info("Check the other CC pin\n");
				cancel_delayed_work(&chip->moist_detected_work);
				schedule_delayed_work(&chip->moist_detected_work, msecs_to_jiffies(MOIST_DETECTED_WORK_DELAY));
				return 0;
			}
			else if (chip->nr_open_pin_det >= 2) {
				chip->nr_open_pin_det = 0;
				// CC pins are okay, test DP/DN pin.
				chip->moist_det_state = MAX20342_MOIST_MANUAL_DP;

				// 1. Disable autoconfig
				if (max20342_update_reg_bits(chip->client, MAX20342_MOISTDET_CTRL, MOISTDET_CTRL_MOISTDET_AUTOCFG, 0))
					return -EINVAL;

				// 2. MAX20342_MOSITDET_PUCONFIG
				if (max20342_write_reg(chip->client, MAX20342_MOSITDET_PUCONFIG, MOSITDET_PUCONFIG_CDP_MASK))
					return -EINVAL;

				// 3. MAX20342_MOSITDET_PDCONFIG
				if (max20342_write_reg(chip->client, MAX20342_MOSITDET_PDCONFIG, ~(u8)MOSITDET_PDCONFIG_CDP_MASK))
					return -EINVAL;

				// 4. Trigger a manual detection for DP pin
				if (max20342_update_reg_bits(chip->client, MAX20342_MOISTDET_CTRL, MOISTDET_CTRL_MOISTDET_MAN_EN, 1))
					return -EINVAL;
			}
		}
		return 0;
	}

	if ((res_int2 & RES_INT2_RES_MOIST_INT_MASK) || (chip->moist_force_detected == true)) {
		pr_info("Moist det: Moist detected!!\n");
		chip->nr_open_pin_det = 0;

		// Burst measurements on cc1, cc2, sbu1, sbu2
		if (likely(res_int2 & RES_INT2_RES_MOIST_INT_MASK)) {
			// Read results from all pins
			if (max20342_read_block_reg(chip->client, MAX20342_MOISTDET_AUTOCC1_RESULT1, MOIST_DET_REG_TO_READ, buf))
				return -EINVAL;

			pr_info("AutoCC1Result:0x%02x; AutoCC1ADC:%d\n", buf[0], buf[1]);
			pr_info("AutoCC2Result:0x%02x; AutoCC2ADC:%d\n", buf[2], buf[3]);
			pr_info("AutoSBU1Result:0x%02x; AutoSBU1ADC:%d\n", buf[4], buf[5]);
			pr_info("AutoSBU2Result:0x%02x; AutoSBU2ADC:%d\n", buf[6], buf[7]);

			if (chip->moist_detected == false) {
				/*
				 * If connecting to a Apple C-to-C cable at USB-C connector, it will trigger a moisture interrupt.
				 * It always detects some resistance on "only one" CC pin, others are open.
				 *
				 * Therefore, we check all 4 pins. If only one CC pin is not open, then it could be a false moisture detection.
				 * We will check DP/DN pins further to make sure they are all open.
				 * Then it must be a false alarm.
				 */
				// If SBU1 and SBU2 are not open, then the moisture is detected.
				if (!(buf[4] & MOISTDET_AUTO_RESULT_OPEN) || !(buf[6] & MOISTDET_AUTO_RESULT_OPEN))
					goto moist_detected;

				// If Both CC1 and CC2 are not open, then the moisture is detected.
				if (!(buf[0] & MOISTDET_AUTO_RESULT_OPEN) && !(buf[2] & MOISTDET_AUTO_RESULT_OPEN))
					goto moist_detected;

				// If only one CC pin is "not open", then it could be an Apple cable
				if (!(buf[0] & MOISTDET_AUTO_RESULT_OPEN) || !(buf[2] & MOISTDET_AUTO_RESULT_OPEN)) {
					pr_info("could be a false alarm\n");

					// Test DP/DN
					chip->moist_det_state = MAX20342_MOIST_MANUAL_DP;

					// 1. Disable autoconfig
					if (max20342_update_reg_bits(chip->client, MAX20342_MOISTDET_CTRL, MOISTDET_CTRL_MOISTDET_AUTOCFG, 0))
						return -EINVAL;

					// 2. MAX20342_MOSITDET_PUCONFIG
					if (max20342_write_reg(chip->client, MAX20342_MOSITDET_PUCONFIG, MOSITDET_PUCONFIG_CDP_MASK))
						return -EINVAL;

					// 3. MAX20342_MOSITDET_PDCONFIG
					if (max20342_write_reg(chip->client, MAX20342_MOSITDET_PDCONFIG, ~(u8)MOSITDET_PDCONFIG_CDP_MASK))
						return -EINVAL;

					// 4. Trigger a manual detection
					if (max20342_update_reg_bits(chip->client, MAX20342_MOISTDET_CTRL, MOISTDET_CTRL_MOISTDET_MAN_EN, 1))
						return -EINVAL;
					return 0;
				}
			}
		}

moist_detected:

		if (chip->moist_detected == false) {
			chip->moist_detected = true;

			// 1. Disable 10s periodic measurement
			if (max20342_update_reg_bits(chip->client, MAX20342_MOISTDET_CTRL, MOISTDET_CTRL_MOISTDET_PER_EN, 0))
				return -EINVAL;

			pr_info("Sent wet uevent\n");
			// 2. Send uevent, offline means water is detected and USB cannot work hereafter
			kobject_uevent(chip->kobj, KOBJ_OFFLINE);
		}

		// 3. Trigger a 5 min delayed work to re-check moisture
		cancel_delayed_work(&chip->moist_detected_work);

		if (likely(chip->moist_force_detected == false))
			schedule_delayed_work(&chip->moist_detected_work, msecs_to_jiffies(MOIST_DETECTED_WORK_DELAY));
		else
			schedule_delayed_work(&chip->moist_detected_work, msecs_to_jiffies(MOIST_FORCE_DETECTED_WORK_DELAY));
		pr_info("Moist: Trigger moist_detected_worker\n");
		return 0;
	}

	// Abort: I_PU_RESULT = 00 && ADC_RESULT_AVG = 0x00
	if (unlikely((ipu == 0) && (adcbuf[0] == 0))) {
		pr_err("Abort! re-det\n");
		if (max20342_update_reg_bits(chip->client, MAX20342_MOISTDET_CTRL, MOISTDET_CTRL_MOISTDET_MAN_EN, 1)) {
			pr_err("%s:%d failed to update registers\n", __func__, __LINE__);
			return -EINVAL;
		}

		/* Schedule deferred detection in case of no interrupt for open and abort of auto configuration */
		cancel_delayed_work(&chip->moist_man_detect_work);
		schedule_delayed_work(&chip->moist_man_detect_work, msecs_to_jiffies(MOIST_MAN_DETECT_WORK_DELAY));
		chip->moist_check = true;
	}

	if (unlikely(res_int2 & RES_INT2_RES_ABORT_INT_MASK))
		pr_err("Auto configuration does not support abort interrupt\n");

	if (unlikely(res_int2 & RES_INT2_RES_OPEN_INT_MASK))
		pr_err("Auto configuration does not support open interrupt\n");
	return 0;
}
#endif

static int max20342_detect_handler(struct max20342_chip *chip)
{
	u8 buf[REG_TO_READ] = {0};
	int ret = 0;

	mutex_lock(&chip->work_lock);

#ifdef CONFIG_CPU_FREQ_OVERRIDE_LAB126
	cpufreq_override(2);
#endif
#if defined(CONFIG_LAB126) && defined(CONFIG_TOI)
	cancel_toi_hibernation();
#endif

	do {
		/* Read 12 contiguous registers starting from MAX20342_REVISION_ID */
		ret = max20342_read_block_reg(chip->client, MAX20342_REVISION_ID, REG_TO_READ, buf);
		if (ret) {
			pr_err("%s:%d failed to read registers\n", __func__, __LINE__);
			goto finally;
		}

		if (buf[MAX20342_BC_INT] & BC_INT_CHG_DET_RUN_R_INT_MASK) {
			// Set CDP/CDN switch to follow FSM
			if (max20342_update_reg_bits(chip->client, MAX20342_COMM_CTRL2, USB_SWC_MASK, 3))
				return -EINVAL;
		}

		if (buf[MAX20342_COMMON_INT] != 0)
			pr_info("common_int:0x%02x\n", buf[MAX20342_COMMON_INT]);
		if (buf[MAX20342_CC_INT] != 0)
			pr_info("cc_int:0x%02x\n", buf[MAX20342_CC_INT]);
		if (buf[MAX20342_BC_INT] != 0)
			pr_info("bc_int:0x%02x\n", buf[MAX20342_BC_INT]);
		if (buf[MAX20342_OVP_INT] != 0)
			pr_info("ovp_int:0x%02x\n", buf[MAX20342_OVP_INT]);
		if (buf[MAX20342_RES_INT1] != 0)
			pr_info("res_int1:0x%02x\n", buf[MAX20342_RES_INT1]);
		if (buf[MAX20342_RES_INT2] != 0)
			pr_info("res_int2:0x%02x\n", buf[MAX20342_RES_INT2]);
		if (buf[MAX20342_COMMON_STATUS] != 0)
			pr_info("common_status:0x%02x\n", buf[MAX20342_COMMON_STATUS]);
		if (buf[MAX20342_CC_STATUS1] != 0)
			pr_info("cc_status1:0x%02x\n", buf[MAX20342_CC_STATUS1]);
		if (buf[MAX20342_CC_STATUS2] != 0)
			pr_info("cc_status2:0x%02x\n", buf[MAX20342_CC_STATUS2]);
		if (buf[MAX20342_BC_STATUS] != 0)
			pr_info("bc_status:0x%02x\n", buf[MAX20342_BC_STATUS]);
		if (buf[MAX20342_OVP_STATUS] != OVP_STATUS_ITFRDY_MASK)
			pr_info("ovp_status:0x%02x\n", buf[MAX20342_OVP_STATUS]);

		/* Fault state handling */
		if (unlikely(buf[MAX20342_COMMON_STATUS] & COMMON_STATUS_FAULT_MASK)) {
			if (chip->fault == false) {
				pr_info("max20342 in fault state\n");
				chip->fault = true;
			}

			if (!(buf[MAX20342_COMMON_STATUS] & COMMON_STATUS_FAULT_STATE_MASKS)) {
				if (max20342_update_reg_bits(chip->client, MAX20342_COMM_CTRL3, COMM_CTRL3_FAULT_UNLOCK_MASK, 1)) {
					ret = -ENODEV;
					goto finally;
				}
				pr_info("max20342 exits fault state by self faultunlock\n");
			}
		}

		if (unlikely(buf[MAX20342_COMMON_INT] & COMMON_INT_FAULT_INT_MASK)) {
			if (!(buf[MAX20342_COMMON_STATUS] & COMMON_STATUS_FAULT_MASK)) {
				chip->fault = false;
				pr_info("max20342 cleans fault state and resets FaultUnlock bit\n");

				if (max20342_update_reg_bits(chip->client, MAX20342_COMM_CTRL3, COMM_CTRL3_FAULT_UNLOCK_MASK, 1)) {
					ret = -ENODEV;
					goto finally;
				}
			}
		}

		/* Shutdown mode wake up interrupt handling */
		if (buf[MAX20342_COMMON_INT] & COMMON_INT_SHDN_WAKE_INT_MASK) {
			chip->shdn_mode = false;
			pr_info("max20342 wakes up from shdn mode\n");

			/* All registers need to be re-initialized after waking up from shutdown mode */
			ret = max20342_init_regs(chip, true);
			if (ret) {
				goto finally;
			}
		}

		/* OVLO (Over VBus voltage) handling */
		// Over VB voltage will disconnect the VB switch
		if (unlikely(buf[MAX20342_OVP_STATUS] & OVP_STATUS_OVLO_MASK)) {
			if (buf[MAX20342_OVP_STATUS] & OVP_STATUS_SWT_CLOSED_MASK)
				pr_err("VB switch should open when over voltage\n");
		}

		/*
		 * Check interrupt sources.
		 * Detect moisture first.
		 * If moisture is detected, then we must check for open state, regardless of interrupt source.
		 *
		 * When charger/USB is connected, max20342 will not perform moisture detection.
		 * After charger/USB is plugged out, we will re-trigger wet detection at max20342_update_wet_chg_state().
		 */
#ifdef __AUTO_CONFIG__
		if (buf[MAX20342_BC_INT] & BC_CHG_DET_RUN_MASKS)
			chip->moist_check = false;

		// 1. Moisture detection
		if ((!(buf[MAX20342_RES_INT2] & RES_MASK2_RES_SBU_INT_MASK)) &&
			((buf[MAX20342_RES_INT2] & RES_INT2_MOIST_DET_MASKS) || (chip->moist_check))) {
			/*
			 * We always employ the auto config first (CC1 and CC2),
			 * then we manually configure for DP/DN detection.
			 */
			cancel_delayed_work(&chip->moist_detected_work);
			cancel_delayed_work(&chip->moist_man_detect_work);

			if (chip->moist_det_state == MAX20342_MOIST_AUTOCONFIG)
				ret = max20342_autocfg_moist_detect_handler(chip, buf[MAX20342_RES_INT2]);
			else
				ret = max20342_mancfg_moist_detect_handler(chip, buf[MAX20342_RES_INT2]);
#else
		if ((!(buf[MAX20342_RES_INT2] & RES_MASK2_RES_SBU_INT_MASK)) &&
			(buf[MAX20342_RES_INT2] & RES_INT2_MOIST_DET_MASKS)) {
			cancel_delayed_work(&chip->moist_detected_work);
			ret = max20342_mancfg_moist_detect_handler(chip, buf[MAX20342_RES_INT2]);
#endif

			if (ret) {
				pr_err("%s:%d failed to read registers\n", __func__, __LINE__);
				goto finally;
			}
		}

		/*
		 * 2. Charger type detection
		 * The sequence of BC status interrupts are:
		 * (1). Chg_Det_Run_R_Int; (2). Chg_Typ_Int; (3). Chg_Det_Run_F_Int (and Pr_Chg_Typ_Int).
		 * The Chg_Det_Run bit is clean at (3).
		 * We must go into this function in max20342_init_worker.
		 * Otherwise, if we plug in USB to boot the device, then the interrupts have been cleared
		 * at hw_init and there will be no USB function.
		 */
		if (likely(chip->moist_detected == false)) {
			if ((buf[MAX20342_BC_INT] & BC_CHG_DET_MASKS) || (!(test_bit(INT_ENABLED, &chip->bitflags)))) {
				ret = max20342_update_chg_state(chip, buf);
				if (ret) {
					pr_err("%s:%d failed to read registers\n", __func__, __LINE__);
					goto finally;
				}
			}
		}
		else {
			if ((buf[MAX20342_BC_INT] & BC_CHG_DET_MASKS) || (buf[MAX20342_COMMON_INT] & COMMON_INT_VBUS_MASKS) || (!(test_bit(INT_ENABLED, &chip->bitflags)))) {
				// We stop timer here for the case the wet detection and charger detection interrupts happen together
				// So we start a timer at max20342_autocfg_moist_detect_handler() and stop here
				// After charger is plugged out, we will re-detect again
#ifdef __AUTO_CONFIG__
				cancel_delayed_work(&chip->moist_man_detect_work);
#endif
				cancel_delayed_work(&chip->moist_detected_work);
				ret = max20342_update_wet_chg_state(chip, buf);

				if (ret) {
					pr_err("%s:%d failed to read registers\n", __func__, __LINE__);
					goto finally;
				}
			}
		}

		// Not a USB cable
		if (buf[MAX20342_RES_INT2] & RES_MASK2_RES_SBU_INT_MASK)
			pr_info("This cable is not a USB cable or water exists at the USB connector\n");
	} while (max20342_is_int_pending(chip));

finally:
	mutex_unlock(&chip->work_lock);
	return ret;
}

static void max20342_init_worker(struct work_struct *work)
{
	struct max20342_chip *chip =
		container_of(work, struct max20342_chip, init_work.work);

	max20342_detect_handler(chip);

	/* Turn on interrupts */
	if (max20342_update_reg_bits(chip->client, MAX20342_COMM_CTRL1, COMM_CTRL1_INTEN_MASK, 1))
		return;

	set_bit(INT_ENABLED, &chip->bitflags);
	enable_irq(chip->irq_chg_det_int);
}

static void max20342_detect_worker(struct work_struct *work)
{
	struct max20342_chip *chip =
		container_of(work, struct max20342_chip, detect_work.work);

	max20342_detect_handler(chip);
}

#ifdef __AUTO_CONFIG__
static void max20342_moist_man_detect_worker(struct work_struct *work)
{
	struct max20342_chip *chip =
		container_of(work, struct max20342_chip, moist_man_detect_work.work);


	if ((chip->moist_force_detected == false) &&
		((chip->moist_detected == false) || (chip->moist_det_state != MAX20342_MOIST_AUTOCONFIG)))
		return;

	// When arriving here, it means open or abort.
	// Otherwise, there should be an interrupt triggered.
	mutex_lock(&chip->work_lock);
	if (chip->moist_det_state == MAX20342_MOIST_AUTOCONFIG) {
		cancel_delayed_work(&chip->moist_man_detect_work);
		cancel_delayed_work(&chip->moist_detected_work);
		max20342_autocfg_moist_detect_handler(chip, 0);
	}
	mutex_unlock(&chip->work_lock);
}
#endif

/* moist_detected_worker is activated when it has detected moisture by 10s built-in timer */
static void max20342_moist_detected_worker(struct work_struct *work)
{
	struct max20342_chip *chip =
		container_of(work, struct max20342_chip, moist_detected_work.work);
	u8 val;


	pr_info("moist_detected_worker\n");

	if (chip->moist_detected == false)
		return;

	// Check Vbus
	if (max20342_read_reg(chip->client, MAX20342_COMMON_STATUS, &val)) {
		pr_err("Vbus check i2c error\n");
		return;
	}

	mutex_lock(&chip->work_lock);

	if (val & COMMON_STATUS_VB_VALID_MASK) {
		pr_info("Vbus is valid, do moist detection when cable out and re-trigger a delayer work\n");

		//Trigger a 5 min delayed work to re-check moisture
		cancel_delayed_work(&chip->moist_detected_work);

		if (likely(chip->moist_force_detected == false))
			schedule_delayed_work(&chip->moist_detected_work, msecs_to_jiffies(MOIST_DETECTED_WORK_DELAY));
		else
			schedule_delayed_work(&chip->moist_detected_work, msecs_to_jiffies(MOIST_FORCE_DETECTED_WORK_DELAY));
		mutex_unlock(&chip->work_lock);
		return;
	}

	/* Trigger a manual detection */
	if (max20342_update_reg_bits(chip->client, MAX20342_MOISTDET_CTRL, MOISTDET_CTRL_MOISTDET_MAN_EN, 1)) {
		pr_err("%s:%d failed to update registers\n", __func__, __LINE__);
		mutex_unlock(&chip->work_lock);
		return;
	}

	pr_info("moist_det_man is set\n");
#ifdef __AUTO_CONFIG__
	/* Schedule deferred detection in case of no interrupt for open and abort of auto configuration */
	if (chip->moist_det_state == MAX20342_MOIST_AUTOCONFIG) {
		cancel_delayed_work(&chip->moist_man_detect_work);
		schedule_delayed_work(&chip->moist_man_detect_work, msecs_to_jiffies(MOIST_MAN_DETECT_WORK_DELAY));
		chip->moist_check = true;
	}
#endif
	mutex_unlock(&chip->work_lock);
}

/*
 * MAX20342 IRQ handlers
 */
static irqreturn_t max20342_chg_det_int(int irq, void *data)
{
	struct max20342_chip *chip = data;


	pr_info("%s: ISR is triggered\n", __func__);

	if (likely(max20342_is_int_pending(chip))) {
		// Do not cancel delayed work here; otherwise, any interrupt will stop the timer
		//cancel_delayed_work(&chip->moist_man_detect_work);
		//cancel_delayed_work(&chip->moist_detected_work);
		cancel_delayed_work(&chip->detect_work);
		max20342_detect_handler(chip);
	}
	else {
		pr_err("%s: ISR is triggered but INT pin is not pending\n", __func__);
	}

	return IRQ_HANDLED;
}

static int setup_gpio_irq(struct max20342_chip *chip, const char *of_name,
	const char *irq_name, irq_handler_t handler, int* out_irq, int *out_gpio)
{
	struct device *dev = &chip->client->dev;
	struct device_node *np = NULL;
	int gpio = 0, irq = 0;
	int ret = 0;

	np = of_find_compatible_node(NULL, NULL, MAX20342_OF_NODE_NAME);
	if (!np) {
		pr_err("%s:%d invalid pnode\n", __func__, __LINE__);
		ret = -EINVAL;
		goto err_exit;
	}

	gpio = of_get_named_gpio(np, of_name, 0);
	if ((gpio < 0) || !gpio_is_valid(gpio)) {
		pr_err("%s:%d get %s GPIO failed, gpio=%u\n", __func__, __LINE__, of_name, gpio);
		ret = -EINVAL;
		goto err_exit;
	}

	// For chg_det_ceb (GPIO31), this GPIO will be used by extcon driver
	if (!handler) {
		if (out_gpio) {
			*out_gpio = gpio;
		}
		return ret;
	}

	ret = devm_gpio_request(dev, gpio, irq_name);
	if (ret) {
		pr_err("%s:%d failed to request gpio %u for %s, err=%d\n", __func__, __LINE__, gpio, of_name, ret);
		ret = -EINVAL;
		goto err_exit;
	}

	ret = gpio_direction_input(gpio);
	if (ret) {
		pr_err("%s:%d configure %s failed, err=%d\n", __func__, __LINE__, of_name, ret);
		ret = -EINVAL;
		goto err_exit;
	}

	if (handler) {
		irq = gpio_to_irq(gpio);
		if (irq < 0) {
			pr_err("%s:%d irq failure\n", __func__, __LINE__);
			ret = -EINVAL;
			goto err_exit;
		}

		irq_set_status_flags(irq, IRQ_NOAUTOEN);
		ret = devm_request_threaded_irq(dev, irq, NULL, handler,
#if GPIO_INT_ACTIVE_LOW
				IRQF_TRIGGER_LOW
#else
				IRQF_TRIGGER_HIGH
#endif /* GPIO_INT_ACTIVE_LOW */
				| IRQF_ONESHOT, irq_name, chip);
		if (ret) {
			pr_err("%s:%d Failed to claim irq %u for %s, error %d \n", __func__, __LINE__, irq, irq_name, ret);
			ret = -EFAULT;
			goto err_exit;
		}
		if (out_irq) {
			*out_irq = irq;
		}
	}

	if (out_gpio) {
		*out_gpio = gpio;
	}

err_exit:
	return ret;
}

/* MAX20342 HW init function */
static int max20342_hw_init(struct max20342_chip *chip)
{
	struct i2c_client *client = chip->client;
	u8 buf[INIT_REG_TO_READ] = {0};
	int ret = 0;
	u8 val = 0;


	/* Read 7 contiguous registers for IC revision ID and for clearing all current pending interrupt flags */
	if (max20342_read_block_reg(client, MAX20342_REVISION_ID, INIT_REG_TO_READ, buf)) {
		pr_err("%s:%d failed to read registers\n", __func__, __LINE__);
		return -ENODEV;
	}

	/* Check IC revision ID */
	if (buf[MAX20342_REVISION_ID] != 0x01) {
		// Todo: might not return error here?
		pr_err("%s:%d wrong revision ID %d\n", __func__, __LINE__, buf[MAX20342_REVISION_ID]);
		return -ENODEV;
	}

	// Read COMMON_STATUS
	if (max20342_read_reg(chip->client, MAX20342_COMMON_STATUS, &val))
		return -EINVAL;

	if (val & COMMON_STATUS_LOW_PWR_MASK)
		pr_info("max20342 is in low power state\n");

	if (unlikely(val & COMMON_STATUS_FAULT_MASK)) {
		pr_info("max20342 is in fault state\n");
		chip->fault = false;
	}
	else
		chip->fault = false;
#ifdef __AUTO_CONFIG__
	chip->moist_det_state = MAX20342_MOIST_AUTOCONFIG;
	chip->moist_check = false;
	chip->nr_open_pin_det = 0;
#else
	chip->moist_det_state = MAX20342_MOIST_MANUAL_SUB1;
#endif
	chip->moist_force_detected = false; // for debug/test only
	chip->moist_detected = false;
	chip->vb_ovp_swc_ctrl = false;
	chip->shdn_mode = false;
	chip->chg_redet = false;

	// Initize all registers
	ret = max20342_init_regs(chip, false);
	if (ret)
		return ret;

	dev_info(&client->dev, "max20342 revision ID %d\n", buf[MAX20342_REVISION_ID]);
	return 0;
}

/*
 * Sys entries
 */
/* Return value follows enum max20342_chg_type */
static ssize_t chg_type_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct max20342_chip *chip = dev_get_drvdata(dev);
	u8 val = 0;

	// Show CEb value, see MAXIM20342 Table 3
	pr_info("%s:%d chg_det_ceb=%d\n", __func__, __LINE__, gpio_get_value(chip->gpio_chg_det_ceb));

	if (max20342_read_reg(chip->client, MAX20342_BC_STATUS, &val))
		return scnprintf(buf, PAGE_SIZE, "\n");

	chip->chg_type = max20342_inquiry_charger_type(val);

	return scnprintf(buf, PAGE_SIZE, U4_TO_BINARY_FORMAT"\n", U4_TO_BINARY(chip->chg_type));
}

static ssize_t online_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct max20342_chip *chip = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", test_bit(ONLINE, &chip->bitflags) ? 1 : 0);
}

static ssize_t ovp_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct max20342_chip *chip = dev_get_drvdata(dev);
	u8 val = 0;

	if (max20342_read_reg(chip->client, MAX20342_OVP_STATUS, &val))
		return scnprintf(buf, PAGE_SIZE, "\n");

	return scnprintf(buf, PAGE_SIZE, "%d\n", (val & OVP_STATUS_OVLO_MASK) ? 1 : 0);
}

static ssize_t vb_swc_closed_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct max20342_chip *chip = dev_get_drvdata(dev);
	u8 val = 0;

	if (max20342_read_reg(chip->client, MAX20342_OVP_STATUS, &val))
		return scnprintf(buf, PAGE_SIZE, "\n");

	return scnprintf(buf, PAGE_SIZE, "%d\n", (val & OVP_STATUS_SWT_CLOSED_MASK) ? 1 : 0);
}

static ssize_t vb_ovp_en_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct max20342_chip *chip = dev_get_drvdata(dev);
	u8 val = 0;

	if (max20342_read_reg(chip->client, MAX20342_OVP_CTRL, &val))
		return scnprintf(buf, PAGE_SIZE, "\n");

	val = BITS_MASKED_GET(val, USB_VB_OVP_EN_MASK);

	return scnprintf(buf, PAGE_SIZE, USB_VB_OVP_EN_FORMAT"\n", USB_VB_OVP_EN_VALUE(val));
}

static ssize_t vb_ovp_en_store(struct device *dev, struct device_attribute *devattr, const char *buf, size_t count)
{
	struct max20342_chip *chip = dev_get_drvdata(dev);
	unsigned long vb_ovp_en = 0;

	if (kstrtoul(buf, 2, &vb_ovp_en))
		pr_info("Input string parse error\n");

	mutex_lock(&chip->work_lock);

	pr_info("vb_ovp_en is set:%d\n", (u8)vb_ovp_en);
	if (max20342_update_reg_bits(chip->client, MAX20342_OVP_CTRL, USB_VB_OVP_EN_MASK, (u8)vb_ovp_en)) {
		mutex_unlock(&chip->work_lock);
		return -ENODEV;
	}

	if (vb_ovp_en == 2) {// (Vbus switch based on table 4)
		chip->vb_ovp_swc_ctrl = false;

#ifdef CONFIG_CHARGER_HAL
		if (chip->chg_type != MAX20342_NO_CHARGER) {
			charger_hal_charger_update_status(chg_type_props[chip->chg_type].type,
				chg_type_props[chip->chg_type].current_limit, true);
		}
#endif
	}
	else {
		chip->vb_ovp_swc_ctrl = true;

#ifdef CONFIG_CHARGER_HAL
		if ((vb_ovp_en == 0) && (chip->chg_type != MAX20342_NO_CHARGER)) {
			// Vbus switch is forced to open, send offline to HAL
			charger_hal_charger_update_status(POWER_SUPPLY_TYPE_UNKNOWN,
				chg_type_props[POWER_SUPPLY_TYPE_UNKNOWN].current_limit, false);
		}
#endif
	}

	mutex_unlock(&chip->work_lock);
	return count;
}

static ssize_t usb_swc_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct max20342_chip *chip = dev_get_drvdata(dev);
	u8 val = 0;

	if (max20342_read_reg(chip->client, MAX20342_COMM_CTRL2, &val))
		return scnprintf(buf, PAGE_SIZE, "\n");

	val = BITS_MASKED_GET(val, USB_SWC_MASK);

	return scnprintf(buf, PAGE_SIZE, USB_SWC_FORMAT"\n", USB_SWC_VALUE(val));
}

static ssize_t usb_swc_store(struct device *dev, struct device_attribute *devattr, const char *buf, size_t count)
{
	struct max20342_chip *chip = dev_get_drvdata(dev);
	unsigned long usb_swc = 0;


	if (kstrtoul(buf, 2, &usb_swc))
		pr_info("Input string parse error\n");

	if (usb_swc == 3) {
		// usb_swc == 3 is to follow the state machine.
		// So set VBOVPEn as 10 (switch based on table 4).
		if (max20342_update_reg_bits(chip->client, MAX20342_OVP_CTRL, USB_VB_OVP_EN_MASK, 2))
			return -ENODEV;
	}
	else {
		// Set VBOVPEn as 01 (switch closed when VB > VBDET)
		if (max20342_update_reg_bits(chip->client, MAX20342_OVP_CTRL, USB_VB_OVP_EN_MASK, 1))
			return -ENODEV;
	}

	if (max20342_update_reg_bits(chip->client, MAX20342_COMM_CTRL2, USB_SWC_MASK, (u8)usb_swc))
		return -ENODEV;

	return count;
}

static ssize_t chg_det_man_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct max20342_chip *chip = dev_get_drvdata(dev);
	u8 val = 0;

	if (max20342_read_reg(chip->client, MAX20342_BC_CTRL0, &val))
		return scnprintf(buf, PAGE_SIZE, "\n");

	return scnprintf(buf, PAGE_SIZE, "%d\n", (val & BC_CTRL0_CHG_DET_MAN_MASK) ? 1 : 0);
}

static ssize_t chg_det_man_store(struct device *dev, struct device_attribute *devattr, const char *buf, size_t count)
{
	struct max20342_chip *chip = dev_get_drvdata(dev);
	u8 chg_det_man;

	chg_det_man = simple_strtoul(buf, NULL, 2) ? 1 : 0;

	/* Trigger a manual detection */
	if (max20342_update_reg_bits(chip->client, MAX20342_BC_CTRL0, BC_CTRL0_CHG_DET_MAN_MASK, chg_det_man))
		return -ENODEV;

	return count;
}

static ssize_t chg_det_en_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct max20342_chip *chip = dev_get_drvdata(dev);
	u8 val = 0;

	if (max20342_read_reg(chip->client, MAX20342_BC_CTRL1, &val))
		return scnprintf(buf, PAGE_SIZE, "\n");

	return scnprintf(buf, PAGE_SIZE, "%d\n", (val & BC_CTRL1_CHG_DET_EN_MASK) ? 1 : 0);
}

static ssize_t chg_det_en_store(struct device *dev,
			struct device_attribute *devattr, const char *buf, size_t count)
{
	struct max20342_chip *chip = dev_get_drvdata(dev);
	u8 chg_det_en = 0;

	chg_det_en = simple_strtoul(buf, NULL, 2) ? 1 : 0;

	if (max20342_update_reg_bits(chip->client, MAX20342_BC_CTRL1, BC_CTRL1_CHG_DET_EN_MASK, chg_det_en))
		return -ENODEV;

	return count;
}

static ssize_t sw_reset_store(struct device *dev,
			struct device_attribute *devattr, const char *buf, size_t count)
{
	struct max20342_chip *chip = dev_get_drvdata(dev);
	u8 sw_reset = 0;

	sw_reset = simple_strtoul(buf, NULL, 2) ? 1 : 0;

	if (max20342_update_reg_bits(chip->client, MAX20342_COMM_CTRL3, COMM_CTRL3_SW_RESET_MASK, sw_reset))
		return -ENODEV;

	return count;
}

static ssize_t fault_unlock_store(struct device *dev,
			struct device_attribute *devattr, const char *buf, size_t count)
{
	struct max20342_chip *chip = dev_get_drvdata(dev);
	u8 fault_unlock = 0;

	fault_unlock = simple_strtoul(buf, NULL, 2) ? 1 : 0;

	if (max20342_update_reg_bits(chip->client, MAX20342_COMM_CTRL3, COMM_CTRL3_FAULT_UNLOCK_MASK, fault_unlock))
		return -ENODEV;

	return count;
}

static ssize_t low_power_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct max20342_chip *chip = dev_get_drvdata(dev);
	u8 val = 0;

	if (max20342_read_reg(chip->client, MAX20342_COMMON_STATUS, &val))
		return scnprintf(buf, PAGE_SIZE, "\n");

	return scnprintf(buf, PAGE_SIZE, "%d\n", (val & COMMON_STATUS_LOW_PWR_MASK) ? 1 : 0);
}

static ssize_t low_power_store(struct device *dev,
			struct device_attribute *devattr, const char *buf, size_t count)
{
	struct max20342_chip *chip = dev_get_drvdata(dev);
	u8 low_power = 0;

	low_power = simple_strtoul(buf, NULL, 2) ? 1 : 0;

	if (max20342_update_reg_bits(chip->client, MAX20342_COMM_CTRL1, COMM_CTRL1_LP_UFP_MASK, low_power))
		return -ENODEV;

	return count;
}

static ssize_t shdnmode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct max20342_chip *chip = dev_get_drvdata(dev);

	/* Cannot use i2c; otherwise MAX20342 will be waked up from shdn mode */
	return scnprintf(buf, PAGE_SIZE, "%d\n", (chip->shdn_mode) ? 1 : 0);
}

static ssize_t shdnmode_store(struct device *dev,
			struct device_attribute *devattr, const char *buf, size_t count)
{
	struct max20342_chip *chip = dev_get_drvdata(dev);
	u8 shdnmode = 0;
	bool mode_val;


	shdnmode = simple_strtoul(buf, NULL, 2) ? 1 : 0;
	mode_val = shdnmode ? true : false;

	if (mode_val == chip->shdn_mode)
		return count;

	chip->shdn_mode = mode_val;

	if (max20342_update_reg_bits(chip->client, MAX20342_COMM_CTRL1, COMM_CTRL1_SHDN_MODE_MASK, shdnmode))
		return -ENODEV;

	return count;
}

static ssize_t ce_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct max20342_chip *chip = dev_get_drvdata(dev);
	u8 val = 0;

	val = !gpio_get_value(chip->gpio_chg_det_ceb);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t ce_store(struct device *dev,
			struct device_attribute *devattr, const char *buf, size_t count)
{
	struct max20342_chip *chip = dev_get_drvdata(dev);
	u8 ce = 0;

	if (!strncmp(buf, "a", 1)) {
		// auto mode, control by HW FSM
		ce = 0;
	} else {
		ce = simple_strtoul(buf, NULL, 2) ? 0x03 : 0x01;
	}

	if (max20342_update_reg_bits(chip->client, MAX20342_COMM_CTRL2, COMM_CTRL2_CE_MASK|COMM_CTRL2_CE_FRC_MASK, ce))
		return -ENODEV;

	return count;
}

static ssize_t moist_det_man_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct max20342_chip *chip = dev_get_drvdata(dev);
	u8 val = 0;

	if (max20342_read_reg(chip->client, MAX20342_MOISTDET_CTRL, &val))
		return scnprintf(buf, PAGE_SIZE, "\n");

	return scnprintf(buf, PAGE_SIZE, "%d\n", (val & MOISTDET_CTRL_MOISTDET_MAN_EN) ? 1 : 0);
}

static ssize_t moist_det_man_store(struct device *dev, struct device_attribute *devattr, const char *buf, size_t count)
{
	struct max20342_chip *chip = dev_get_drvdata(dev);
	u8 moist_det_man;


	moist_det_man = simple_strtoul(buf, NULL, 2) ? 1 : 0;

	mutex_lock(&chip->work_lock);
	/* Trigger a manual detection.
	 * If the result is "Open", then we won't get interrupt since we have masked the open interrupt.
	 */
	if (max20342_update_reg_bits(chip->client, MAX20342_MOISTDET_CTRL, MOISTDET_CTRL_MOISTDET_MAN_EN, moist_det_man)) {
		mutex_unlock(&chip->work_lock);
		return -ENODEV;
	}

#ifdef __AUTO_CONFIG__
	/* Schedule deferred detection in case of no interrupt for open and abort case */
	if ((moist_det_man) && (chip->moist_det_state == MAX20342_MOIST_AUTOCONFIG)) {
		cancel_delayed_work(&chip->moist_man_detect_work);
		schedule_delayed_work(&chip->moist_man_detect_work, msecs_to_jiffies(MOIST_MAN_DETECT_WORK_DELAY));
		chip->moist_check = true;
	}

	if ((moist_det_man == 0) && (chip->moist_check)) {
		cancel_delayed_work(&chip->moist_man_detect_work);
		chip->moist_check = false;
	}
#endif
	mutex_unlock(&chip->work_lock);
	return count;
}

static ssize_t moist_detected_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct max20342_chip *chip = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", (chip->moist_detected) ? 1 : 0);
}

// For test/debug only, so we do not have to pour water at USB-C connector
static ssize_t moist_detected_store(struct device *dev,
			struct device_attribute *devattr, const char *buf, size_t count)
{
	struct max20342_chip *chip = dev_get_drvdata(dev);
	u8 force_moist_detected = 0;
	int ret;

	force_moist_detected = simple_strtoul(buf, NULL, 2) ? 1 : 0;
	mutex_lock(&chip->work_lock);

	if (force_moist_detected) {
		chip->moist_force_detected = true;

		// Trigger a 10s delayed work to re-check moisture
		//cancel_delayed_work(&chip->moist_detected_work);
		//schedule_delayed_work(&chip->moist_detected_work, msecs_to_jiffies(MOIST_FORCE_DETECTED_WORK_DELAY));

		/* Trigger a manual detection */
		ret = max20342_update_reg_bits(chip->client, MAX20342_MOISTDET_CTRL, MOISTDET_CTRL_MOISTDET_MAN_EN, 1);
		if (ret) {
			pr_err("%s:%d failed to update registers\n", __func__, __LINE__);
			mutex_unlock(&chip->work_lock);
			return ret;
		}

		// Unmask open interrupt
		if (max20342_update_reg_bits(chip->client, MAX20342_RES_MASK2, RES_INT2_RES_OPEN_INT_MASK, 1))
			return -ENODEV;
#ifdef __AUTO_CONFIG__
		/* Schedule deferred detection in case of no interrupt for open and abort of auto configuration */
		if (chip->moist_det_state == MAX20342_MOIST_AUTOCONFIG) {
			cancel_delayed_work(&chip->moist_man_detect_work);
			schedule_delayed_work(&chip->moist_man_detect_work, msecs_to_jiffies(MOIST_MAN_DETECT_WORK_DELAY));
			chip->moist_check = true;
		}
#endif
	}
	else {
#ifdef __AUTO_CONFIG__
		if ((chip->moist_force_detected) && (chip->moist_detected))
			chip->nr_open_pin_det = 1;
#endif
		chip->moist_force_detected = false;
	}

	mutex_unlock(&chip->work_lock);
	return count;
}

// For HW testing only, HW needs to force to disable moisture detection while moisture is detected
static ssize_t disable_moist_det_store(struct device *dev, struct device_attribute *devattr, const char *buf, size_t count)
{
	struct max20342_chip *chip = dev_get_drvdata(dev);
	u8 disable_moist_det = 0;
	int ret;


	disable_moist_det = simple_strtoul(buf, NULL, 2) ? 1 : 0;

	mutex_lock(&chip->work_lock);

	if (disable_moist_det) {
		ret = max20342_update_reg_bits(chip->client, MAX20342_MOISTDET_CTRL, MOISTDET_CTRL_MOISTDET_MAN_EN|MOISTDET_CTRL_MOISTDET_PER_EN, 0);
		if (ret)
			goto error;

		cancel_delayed_work(&chip->moist_detected_work);
#ifdef __AUTO_CONFIG__
		cancel_delayed_work(&chip->moist_man_detect_work);
#endif
		if (chip->moist_detected) {
			chip->moist_detected = false;

			pr_info("Sent unwet uevent by system entry\n");

			// Send online uevent
			kobject_uevent(chip->kobj, KOBJ_ONLINE);
		}

		chip->moist_force_detected = false;
#ifdef __AUTO_CONFIG__
		chip->nr_open_pin_det = 0;
		chip->moist_check = false;
#endif
	}
	else {
		// This should be called only after echo 1 > disable_moist_det
		ret = max20342_update_reg_bits(chip->client, MAX20342_MOISTDET_CTRL, MOISTDET_CTRL_MOISTDET_PER_EN, 1);
		if (ret)
			goto error;
	}

	mutex_unlock(&chip->work_lock);
	return count;
error:
	mutex_unlock(&chip->work_lock);
	return ret;
}

static ssize_t rmoist_det_ipu_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct max20342_chip *chip = dev_get_drvdata(dev);
	u8 val = 0;

	if (max20342_read_reg(chip->client, MAX20342_MOISTDET_CTRL, &val))
		return scnprintf(buf, PAGE_SIZE, "\n");

	val = BITS_MASKED_GET(val, R_MOIST_DET_I_PU_MASK);

	return scnprintf(buf, PAGE_SIZE, R_MOIST_DET_I_PU_FORMAT"\n", R_MOIST_DET_I_PU_VALUE(val));
}

static ssize_t rmoist_det_ipu_store(struct device *dev, struct device_attribute *devattr, const char *buf, size_t count)
{
	struct max20342_chip *chip = dev_get_drvdata(dev);
	unsigned long rmoist_det_ipu = 0;

	if (kstrtoul(buf, 2, &rmoist_det_ipu))
		pr_info("Input string parse error\n");

	if (max20342_update_reg_bits(chip->client, MAX20342_MOISTDET_CTRL, R_MOIST_DET_I_PU_MASK, (u8)rmoist_det_ipu))
		return -ENODEV;

	return count;
}

static ssize_t rmoist_det_vth_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct max20342_chip *chip = dev_get_drvdata(dev);
	u8 val = 0;

	if (max20342_read_reg(chip->client, MAX20342_RMOISTDET_VTH, &val))
		return scnprintf(buf, PAGE_SIZE, "\n");

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t rmoist_det_vth_store(struct device *dev, struct device_attribute *devattr, const char *buf, size_t count)
{
	struct max20342_chip *chip = dev_get_drvdata(dev);
	unsigned long rmoist_det_vth = 0;

	// base is 10
	if (kstrtoul(buf, 10, &rmoist_det_vth))
		pr_info("Input string parse error\n");

	if (max20342_update_reg_bits(chip->client, MAX20342_RMOISTDET_VTH, 0xff, (u8)rmoist_det_vth))
		return -ENODEV;

	return count;
}

static ssize_t wet_usb_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct max20342_chip *chip = dev_get_drvdata(dev);

	if (chip->moist_detected)
		return scnprintf(buf, PAGE_SIZE, "%d\n", test_bit(ONLINE, &chip->bitflags) ? 1 : 0);
	else
		return scnprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t cc1_ipu_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct max20342_chip *chip = dev_get_drvdata(dev);
	u8 val = 0;

	if (max20342_read_reg(chip->client, MAX20342_MOISTDET_AUTOCC1_RESULT1, &val))
		return scnprintf(buf, PAGE_SIZE, "\n");

	val = BITS_MASKED_GET(val, CCX_MOIST_I_PU_MASK);

	return scnprintf(buf, PAGE_SIZE, CCX_MOIST_I_PU_FORMAT"\n", CCX_MOIST_I_PU_VALUE(val));
}

static ssize_t cc1_adc_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct max20342_chip *chip = dev_get_drvdata(dev);
	u8 val = 0;

	if (max20342_read_reg(chip->client, MAX20342_MOISTDET_AUTOCC1_RESULT2, &val))
		return scnprintf(buf, PAGE_SIZE, "\n");

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t cc2_ipu_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct max20342_chip *chip = dev_get_drvdata(dev);
	u8 val = 0;

	if (max20342_read_reg(chip->client, MAX20342_MOISTDET_AUTOCC2_RESULT1, &val))
		return scnprintf(buf, PAGE_SIZE, "\n");

	val = BITS_MASKED_GET(val, CCX_MOIST_I_PU_MASK);

	return scnprintf(buf, PAGE_SIZE, CCX_MOIST_I_PU_FORMAT"\n", CCX_MOIST_I_PU_VALUE(val));
}

static ssize_t cc2_adc_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct max20342_chip *chip = dev_get_drvdata(dev);
	u8 val = 0;

	if (max20342_read_reg(chip->client, MAX20342_MOISTDET_AUTOCC2_RESULT2, &val))
		return scnprintf(buf, PAGE_SIZE, "\n");

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t sbu1_ipu_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct max20342_chip *chip = dev_get_drvdata(dev);
	u8 val = 0;

#ifdef __AUTO_CONFIG__
	if (max20342_read_reg(chip->client, MAX20342_MOISTDET_AUTOSBU1_RESULT1, &val))
		return scnprintf(buf, PAGE_SIZE, "\n");

	val = BITS_MASKED_GET(val, SBUX_MOIST_I_PU_MASK);

	return scnprintf(buf, PAGE_SIZE, SBUX_MOIST_I_PU_FORMAT"\n", SBUX_MOIST_I_PU_VALUE(val));
#else
	if (chip->moist_det_state == MAX20342_MOIST_MANUAL_SUB1) {
		// Read I_PU_RESULT (pull up current)
		if (max20342_read_reg(chip->client, MAX20342_ADC_CTRL2, &val))
			return scnprintf(buf, PAGE_SIZE, "\n");

		val = BITS_MASKED_GET(val, I_PU_RESULT_MASK);
	}

	return scnprintf(buf, PAGE_SIZE, I_PU_RESULT_FORMAT"\n", I_PU_RESULT_VALUE(val));
#endif
}

static ssize_t sbu1_adc_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct max20342_chip *chip = dev_get_drvdata(dev);
	u8 val = 0;

#ifdef __AUTO_CONFIG__
	if (max20342_read_reg(chip->client, MAX20342_MOISTDET_AUTOSBU1_RESULT2, &val))
		return scnprintf(buf, PAGE_SIZE, "\n");

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
#else
	if (chip->moist_det_state == MAX20342_MOIST_MANUAL_SUB1) {
		// Read ADC results
		if (max20342_read_reg(chip->client, MAX20342_ADC_RESULT_AVG, &val))
			return scnprintf(buf, PAGE_SIZE, "\n");
	}

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
#endif
}

static ssize_t sbu2_ipu_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct max20342_chip *chip = dev_get_drvdata(dev);
	u8 val = 0;

#ifdef __AUTO_CONFIG__
	if (max20342_read_reg(chip->client, MAX20342_MOISTDET_AUTOSBU2_RESULT1, &val))
		return scnprintf(buf, PAGE_SIZE, "\n");

	val = BITS_MASKED_GET(val, SBUX_MOIST_I_PU_MASK);

	return scnprintf(buf, PAGE_SIZE, SBUX_MOIST_I_PU_FORMAT"\n", SBUX_MOIST_I_PU_VALUE(val));
#else
	if (chip->moist_det_state == MAX20342_MOIST_MANUAL_SUB2) {
		// Read I_PU_RESULT (pull up current)
		if (max20342_read_reg(chip->client, MAX20342_ADC_CTRL2, &val))
			return scnprintf(buf, PAGE_SIZE, "\n");

		val = BITS_MASKED_GET(val, I_PU_RESULT_MASK);
	}

	return scnprintf(buf, PAGE_SIZE, I_PU_RESULT_FORMAT"\n", I_PU_RESULT_VALUE(val));
#endif
}

static ssize_t sbu2_adc_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct max20342_chip *chip = dev_get_drvdata(dev);
	u8 val = 0;

#ifdef __AUTO_CONFIG__
	if (max20342_read_reg(chip->client, MAX20342_MOISTDET_AUTOSBU2_RESULT2, &val))
		return scnprintf(buf, PAGE_SIZE, "\n");

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
#else
	if (chip->moist_det_state == MAX20342_MOIST_MANUAL_SUB2) {
		// Read ADC results
		if (max20342_read_reg(chip->client, MAX20342_ADC_RESULT_AVG, &val))
			return scnprintf(buf, PAGE_SIZE, "\n");
	}

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
#endif
}


static DEVICE_ATTR_RO(chg_type);
static DEVICE_ATTR_RO(online);
static DEVICE_ATTR_RO(ovp);
static DEVICE_ATTR_RO(vb_swc_closed);
static DEVICE_ATTR_RW(vb_ovp_en);
static DEVICE_ATTR_RW(usb_swc);
static DEVICE_ATTR_RW(chg_det_man);
static DEVICE_ATTR_RW(chg_det_en);
static DEVICE_ATTR_WO(sw_reset);
static DEVICE_ATTR_WO(fault_unlock);
static DEVICE_ATTR_RW(low_power);
static DEVICE_ATTR_RW(shdnmode);
static DEVICE_ATTR_RW(ce);
static DEVICE_ATTR_RW(moist_det_man);
static DEVICE_ATTR_RW(moist_detected);  //RW is for debug purpose
static DEVICE_ATTR_RW(rmoist_det_ipu);
static DEVICE_ATTR_RW(rmoist_det_vth);
static DEVICE_ATTR_WO(disable_moist_det);
static DEVICE_ATTR_RO(wet_usb);
static DEVICE_ATTR_RO(cc1_ipu);
static DEVICE_ATTR_RO(cc1_adc);
static DEVICE_ATTR_RO(cc2_ipu);
static DEVICE_ATTR_RO(cc2_adc);
static DEVICE_ATTR_RO(sbu1_ipu);
static DEVICE_ATTR_RO(sbu1_adc);
static DEVICE_ATTR_RO(sbu2_ipu);
static DEVICE_ATTR_RO(sbu2_adc);

static struct attribute *max20342_attrs[] = {
	&dev_attr_chg_type.attr,
	&dev_attr_online.attr,
	&dev_attr_ovp.attr,
	&dev_attr_vb_swc_closed.attr,
 	&dev_attr_vb_ovp_en.attr,
	&dev_attr_usb_swc.attr,
	&dev_attr_chg_det_man.attr,
	&dev_attr_chg_det_en.attr,
	&dev_attr_sw_reset.attr,
	&dev_attr_fault_unlock.attr,
	&dev_attr_low_power.attr,
	&dev_attr_shdnmode.attr,
	&dev_attr_ce.attr,
	&dev_attr_moist_det_man.attr,
	&dev_attr_moist_detected.attr,
	&dev_attr_rmoist_det_ipu.attr,
	&dev_attr_rmoist_det_vth.attr,
	&dev_attr_disable_moist_det.attr,
	&dev_attr_wet_usb.attr,
	&dev_attr_cc1_ipu.attr,
	&dev_attr_cc1_adc.attr,
	&dev_attr_cc2_ipu.attr,
	&dev_attr_cc2_adc.attr,
	&dev_attr_sbu1_ipu.attr,
	&dev_attr_sbu1_adc.attr,
	&dev_attr_sbu2_ipu.attr,
	&dev_attr_sbu2_adc.attr,
	NULL
};

static const struct attribute_group max20342_attrs_group = {
	.attrs = max20342_attrs,
};

static int max20342_moisture_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	return 0;
}

/*
 * Probe function
 */
static int max20342_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct i2c_adapter *adapter = to_i2c_adapter(dev->parent);
	struct max20342_chip *chip; /* per device structure */
	int ret = 0;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		pr_err("%s:%d No support for SMBUS_BYTE_DATA\n", __func__, __LINE__);
		return -ENODEV;
	}

	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	dev_set_drvdata(dev, chip);

	chip->client = client;
	chip->kobj = &dev->kobj;

	chip->wet_desc.name = MOISTURE_NAME;
	chip->wet_desc.type = POWER_SUPPLY_TYPE_USB_WET;
	chip->wet_desc.get_property = max20342_moisture_get_property;
	chip->wet_cfg.drv_data = chip;
	chip->wet_psy = power_supply_register_no_ws(dev, &chip->wet_desc, &chip->wet_cfg);

	if (IS_ERR(chip->wet_psy)) {
		dev_err(dev, "failed to register wet power supply: %ld\n", PTR_ERR(chip->wet_psy));
		ret = PTR_ERR(chip->wet_psy);
		goto err_free_chip;
	}

	mutex_init(&chip->work_lock);
	INIT_DELAYED_WORK(&chip->init_work, max20342_init_worker);
	INIT_DELAYED_WORK(&chip->detect_work, max20342_detect_worker);
#ifdef __AUTO_CONFIG__
	INIT_DELAYED_WORK(&chip->moist_man_detect_work, max20342_moist_man_detect_worker);
#endif
	INIT_DELAYED_WORK(&chip->moist_detected_work, max20342_moist_detected_worker);

	ret = max20342_hw_init(chip);
	if (ret) {
		ret = -ENODEV;
		goto err_free_chip;
	}

	ret = setup_gpio_irq(chip, "chg_det_int", "chg_det_int", max20342_chg_det_int, &chip->irq_chg_det_int, &chip->gpio_chg_det_int);
	if (unlikely(ret)) {
		pr_info("gpio chg_det_int error\n");
		goto err_free_chip;
	}

	ret = setup_gpio_irq(chip, "chg_det_ceb", "chg_det_ceb", NULL, NULL, &chip->gpio_chg_det_ceb);
	if (unlikely(ret)) {
		pr_info("gpio chg_det_ce error\n");
		goto err_free_chip;
	}

	chip->attrs_group = &max20342_attrs_group;
	/* device now accessible at user space */
	ret = sysfs_create_group(chip->kobj, chip->attrs_group);
	if (unlikely(ret)) {
		pr_err("%s:%d failed to create attribute group ret=[%d]\n", __func__, __LINE__, ret);
		chip->attrs_group = NULL;
		goto err_free_chip;
	}

	schedule_delayed_work(&chip->init_work, msecs_to_jiffies(INITIAL_CHECK_DELAY));

	g_chip = chip;

	return 0;

err_free_chip:
	if (chip->attrs_group) {
		sysfs_remove_group(chip->kobj, chip->attrs_group);
	}

	if (chip->gpio_chg_det_int) {
		devm_free_irq(dev, gpio_to_irq(chip->gpio_chg_det_int), chip);
	}

	dev_set_drvdata(dev, NULL);
	devm_kfree(dev, chip);
	return ret;
}

#ifdef CONFIG_PM_SLEEP

static int max20342_suspend(struct device *dev)
{
	struct max20342_chip *chip = dev_get_drvdata(dev);
	u8 val;
	u8 ovp_ctrl;

	pr_info("%s:%d bitflags: 0x%02lx\n", __func__, __LINE__, chip->bitflags);

	if (unlikely(chip->moist_detected)) {
		pr_err("Error! max20342 device should not enter suspend while wet is detected\n");
		return 0;
	}

	if (max20342_read_reg(chip->client, MAX20342_OVP_STATUS, &val))
		return -ENODEV;

	if (max20342_read_reg(chip->client, MAX20342_OVP_CTRL, &ovp_ctrl))
		return -ENODEV;
	ovp_ctrl = BITS_MASKED_GET(ovp_ctrl, USB_VB_OVP_EN_MASK);
	pr_info("vb_close:%d; vb_ovp_en:"USB_VB_OVP_EN_FORMAT"\n", ((val & OVP_STATUS_SWT_CLOSED_MASK) ? 1 : 0), USB_VB_OVP_EN_VALUE(ovp_ctrl));

	// Check Vbus
	if (max20342_read_reg(chip->client, MAX20342_COMMON_STATUS, &val))
		return -ENODEV;

	// If Vbus is valid, then max20342 would not go into shutdown mode.
	// This is for factory side RIT suspend/hibernation tests using echo 0 > ce
	if (unlikely(val & COMMON_STATUS_VB_VALID_MASK)) {
		pr_info("max20342 is not in shutdown mode when Vbus is valid\n");
		return 0;
	}

	// max20342 enters shutdown mode
	mutex_lock(&chip->work_lock);
	if (max20342_update_reg_bits(chip->client, MAX20342_COMM_CTRL1, COMM_CTRL1_SHDN_MODE_MASK, 1)) {
		mutex_unlock(&chip->work_lock);
		return -ENODEV;
	}

	chip->shdn_mode = true;
	mutex_unlock(&chip->work_lock);
	return 0;
}

static int max20342_resume(struct device *dev)
{
	struct max20342_chip *chip = dev_get_drvdata(dev);
	int ret;
	u8 val;
	u8 ovp_ctrl;

	pr_info("%s:%d bitflags: 0x%02lx\n", __func__, __LINE__, chip->bitflags);

	if (chip->shdn_mode) {
		// max20342 exits from the shutdown mode by accessing through I2C
		// 1. If resume by pressing power key, then accessing I2c here will wake up max20342
		// 2. If resume by inserting USB cable, then shdnwakeup interrupt will run first,
		//    then we may not need to block here
		mutex_lock(&chip->work_lock);
		if (chip->shdn_mode) {
			ret = max20342_read_reg(chip->client, MAX20342_REVISION_ID, &val);
			if (ret) {
				pr_info("I2c access failed because max20342 is in shutdown mode\n");
			}
		}
		mutex_unlock(&chip->work_lock);
	}
	else {
		if (max20342_read_reg(chip->client, MAX20342_OVP_STATUS, &val))
			return -ENODEV;

		if (max20342_read_reg(chip->client, MAX20342_OVP_CTRL, &ovp_ctrl))
			return -ENODEV;
		ovp_ctrl = BITS_MASKED_GET(ovp_ctrl, USB_VB_OVP_EN_MASK);
		pr_info("vb_close:%d; vb_ovp_en:"USB_VB_OVP_EN_FORMAT"\n", ((val & OVP_STATUS_SWT_CLOSED_MASK) ? 1 : 0), USB_VB_OVP_EN_VALUE(ovp_ctrl));
	}
	return 0;
}

static SIMPLE_DEV_PM_OPS(max20342_pm_ops, max20342_suspend, max20342_resume);

#define MAX20342_PM_OPS (&max20342_pm_ops)

#else

#define MAX20342_PM_OPS NULL

#endif /* CONFIG_PM_SLEEP */

static const struct i2c_device_id max20342_id[] = {
	{ MAX20342_NAME, 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, max20342_id);

/* matching by compatible property (for DT) */
static const struct of_device_id max20342_match_table[] = {
	{ .compatible = MAX20342_OF_NODE_NAME, },
	{}
};
MODULE_DEVICE_TABLE(of, max20342_match_table);

static struct i2c_driver max20342_i2c_driver = {
	.driver = {
		.name	= MAX20342_NAME,
		.of_match_table = max20342_match_table,
		.pm	= MAX20342_PM_OPS,
	},
	.probe	= max20342_probe,
	.id_table	= max20342_id,
};
module_i2c_driver(max20342_i2c_driver);

MODULE_DESCRIPTION("MAX20342 USB charger detector");
MODULE_AUTHOR("Chih Chieh Chou <chihcho@amazon.com>");
MODULE_LICENSE("GPL v2");

