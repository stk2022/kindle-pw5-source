/************************************************************
 *
 * file: p922x_wireless_power.h
 *
 * Description: Interface of P9222 to AP access included file
 *
 *------------------------------------------------------------
 *
 * Copyright (c) 2018, Integrated Device Technology Co., Ltd.
 * Copyright (C) 2020 Amazon.com Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *************************************************************/

#ifndef __IDTP9222_H__
#define __IDTP9222_H__

/* RX -> TX */
#define PROPRIETARY18		0x18
#define PROPRIETARY28		0x28
#define PROPRIETARY38		0x38
#define PROPRIETARY48		0x48
#define PROPRIETARY58		0x58

/* bits mask */
#define BIT0			0x01
#define BIT1			0x02
#define BIT2			0x04
#define BIT3			0x08
#define BIT4			0x10
#define BIT5			0x20
#define BIT6			0x40
#define BIT7			0x80
#define BIT8			(1 << 8)
#define BIT9			(1 << 9)
#define BIT10			(1 << 10)
#define BIT11			(1 << 11)
#define BIT12			(1 << 12)
#define BIT13			(1 << 13)
#define BIT14			(1 << 14)
#define BIT15			(1 << 15)

/* status low regiter bits define */
#define STATUS_VRECT_ON		BIT6
#define STATUS_TX_DATA_RECV		BIT5
#define STATUS_OV_VOL		BIT4
#define STATUS_OV_CURR		BIT3
#define STATUS_OV_TEMP		BIT2

/*
 * bitmap for status flags
 * 1: indicates a pending interrupt for LDO Vout state change from OFF to ON
 */
#define VOUTCHANGED		BIT7 /* Stat_Vout_ON */
/*
 * 1: indicates a pending interrupt for TX Data Received.
 * (Change from "No Received Data" state to "data Received" state)
 */
#define TXDATARCVD		BIT4 /* TX Data Received */

/* used registers define */
#define REG_CHIP_ID		0x0000
#define REG_CHIP_REV		0x0002
#define REG_OTP_FW		0x0004
#define REG_STATUS		0x0034
#define REG_INT		0x0036
#define REG_INT_EN		0x0038
#define REG_INT_CLEAR		0x003A
#define REG_OP_MODE		0x003F
#define REG_CHG_STATUS		0x004E
#define REG_EPT			0x004F
#define REG_ADC_VOUT		0x0050
#define REG_VOUT_SET		0x0052
/* Vrect Adjust */
#define REG_VRECT_ADJ		0x0053
#define REG_ADC_VRECT		0x0054
#define REG_RX_IOUT		0x0058
#define REG_DIE_TEMP		0x005A
#define REG_OP_FREQ		0x005C
#define REG_ILIM_SET		0x0060
#define REG_COMMAND		0x0062
#define REG_OV_SET		0x007E
/* Communocation CAP Enable */
#define REG_CM_CAP_EN_ADDR		0x007F
/* FOD parameters addr, 16 bytes */
#define REG_FOD_COEF_ADDR	0x0084
#define REG_FOD_DUMP_MAX	0x0093
/* ADC result Register addr, 16 bytes */
#define REG_ADC_RESULT_0	0x00D4
#define REG_ADC_RESULT_1	0x00D6
#define REG_ADC_RESULT_2	0x00D8
#define REG_ADC_RESULT_3	0x00DA
#define REG_EXT_TEMP		0x00B0
#define REG_RX_NEGO_PWR		0x00B9
/* Signal Strength Register */
#define REG_SS			0x0123
#define REG_VRECCT_TARGET		0x0090
#define REG_VRECCT_KNEE		0x0092
#define REG_VRECCT_CF		0x0093
#define REG_VRECCT_MAX_CR		0x0094
#define REG_VRECCT_MIN_CR		0x0096
#define REG_ADC_DIE		0x012A
#define REG_ADC_GP0		0x012C

/* Power Receive Register */
#define REG_PRX			0x0048
/* Proprietary Packet Header Register (0x64) */
#define REG_PROPPKT_ADDR	0x0064
/* PPP Data Value Register (0X65 - 0x69) */
#define REG_PPPDATA_ADDR	0x0065
/* Back Channel Packet Register (0x6C) */
#define REG_BCHEADER_ADDR	0x006C
/* Back Channel Packet Register (0x6D - 0x6F) */
#define REG_BCDATA_ADDR		0x006D
#define REG_FC_VOLTAGE		0x00FF

#define REG_DUMP_MAX		0x007F

#define IDT_FAST_CHARING_EN	0
#define IDT_PROGRAM_FIRMWARE_EN	1

#define CHARGER_VOUT_10W 9000
#define CHARGER_VOUT_5W 6500

#define CHARGER_IOUT_7W5 1500
#define CHARGER_IOUT_5W 1000
#define CHARGER_VOUT_EN 0
#define CHARGER_EPP_EN  1

#define READY_DETECT_TIME (50*HZ/1000)

#define SWITCH_DETECT_TIME (300*HZ/1000)
#define SWITCH_10W_VTH_L 8500
#define SWITCH_10W_VTH_H 9500
#define SWITCH_5W_VTH_L 6000
#define SWITCH_5W_VTH_H 7000
#define SWITCH_VOLTAGE_COUNT 6
#define DIS_CM_CAP	0x00
#define EN_CM_CAP	0X30

 /* bitmap for SSCmnd register 0x62 */
#define VSWITCH			BIT7
/*
 * If AP sets this bit to 1 then IDTP9222 M0 start the device authintication
 */
#define SEND_DEVICE_AUTH		BIT6
/*
 * If AP sets this bit to “1” then IDTP9222 M0 sends WPC ADT data to TX.
 * This bit is automatically set to “0” upon completion of sending all data.
 */
#define SEND_ADT		BIT9
/*
 * If AP sets this bit to "1" then IDTP9222 M0 clears the interrupt
 * corresponding to the bit(s) which has a value of "1"
 */
#define CLRINT			BIT5
/*
 * If AP sets this bit to 1 then IDTP9222 M0 sends the Charge Status packet
 * (defined in the Battery Charge Status Register) to TX and then sets this bit to 0
 */
#define CHARGE_STAT		BIT4
/*
 * If AP sets this bit to "1" then IDTP9222 M0 sends the End of Power packet
 * (define in the End of Power Transfer Register) to Tx and then sets this bit to "0"
 */
#define SENDEOP			BIT3
/*
 * If AP sets this bit to "1" then IDTP9222 MCU reset itself,
 * and then IDTP9222s MCU sets this bit to “0”.
 */
#define MCU_RESET		BIT2
/*
 * If AP sets this bit to "1" then IDTP9222 M0 toggles LDO output once
 * (from on to off, or from off to on), and then sets this bit to "0"
 */
#define LDOTGL			BIT1
/*
 * If AP sets this bit to "1" then IDTP9222 M0 sends the Proprietary Packet
 */
#define SENDPROPP		BIT0

/* bitmap for interrupt register 0x36 */
#define	P922X_INT_ID_AUTH_SUCCESS		BIT14
#define	P922X_INT_ID_AUTH_FAIL		BIT13
#define	P922X_INT_DEVICE_AUTH_SUCCESS		BIT11
#define	P922X_INT_DEVICE_AUTH_FAIL		BIT1

#define	P922X_INT_TX_DATA_RECV		BIT15
#define	P922X_INT_EXT_MODE		BIT12
#define	P922X_INT_AC_MISS_DETECT		BIT10
#define	P922X_INT_TX_ADT_RECV		BIT9
#define	P922X_INT_RX_ADT_SEND		BIT8
#define	P922X_INT_VOUT		BIT7
#define	P922X_INT_VRECT		BIT6
#define P922X_INT_MODE_CHANGED		BIT5
#define P922X_INT_OV_VOLT		BIT4
#define P922X_INT_OV_CURRENT		BIT3
#define P922X_INT_OV_TEMP		BIT2
#define P922X_INT_ADT_ERROR		BIT0
#define P922X_INT_LIMIT_MASK		(P922X_INT_OV_TEMP | \
					P922X_INT_OV_VOLT | \
					P922X_INT_OV_CURRENT)

 /* bitmap for customer command */
#define BC_NONE			0x00
#define BC_SET_FREQ		0x03
#define BC_GET_FREQ		0x04
#define BC_READ_FW_VER		0x05
#define BC_READ_Iin		0x06
#define BC_READ_Vin		0x07
#define BC_SET_Vin		0x0a
#define BC_ADAPTER_TYPE		0x0b
#define BC_RESET		0x0c
#define BC_READ_I2C		0x0d
#define BC_WRITE_I2C		0x0e
#define BC_VI2C_INIT		0x10

#define IDT_INT			"p9222-int"
#define IDT_PG			"p9222-pg"

#define SET_VOUT_VAL		6500
#define SET_VOUT_MAX		12500
#define SET_VOUT_MIN		3500

/* End of Power packet types */
#define EOP_OVER_TEMP		0x03
#define EOP_OVER_VOLT		0x04
#define EOP_OVER_CURRENT	0x05

#define FOD_COEF_ARRY_LENGTH	8
#define FOD_COEF_PARAM_LENGTH	16

#define P922X_ADC_VOLTAGE_DEFAULT	1800	/* mV */
#define P922X_DIE_TEMP_DEFAULT	-2290	/* Tenths of degree Celsius */

#ifdef CONFIG_AMAZON_METRICS_LOG
#define BATTERY_METRICS_BUFF_SIZE 512
static char metrics_buf[BATTERY_METRICS_BUFF_SIZE];

#define p922x_metrics_log(domain, fmt, ...)				\
do {									 \
	memset(metrics_buf, 0, BATTERY_METRICS_BUFF_SIZE); 	 \
	snprintf(metrics_buf, sizeof(metrics_buf), fmt, ##__VA_ARGS__);\
	log_to_metrics(ANDROID_LOG_INFO, domain, metrics_buf);  \
} while (0)
#else
//static inline void p922x_metrics_log(void) {}
#define p922x_metrics_log(domain, fmt, ...)	{}
#endif

/* Adapter Type */
enum adapter_list {
	ADAPTER_UNKNOWN	 = 0x00,
	ADAPTER_SDP	 = 0x01,
	ADAPTER_CDP	 = 0x02,
	ADAPTER_DCP	 = 0x03,
	ADAPTER_QC20	 = 0x05,
	ADAPTER_QC30	 = 0x06,
	ADAPTER_PD	 = 0x07,
};

enum charge_mode {
	CHARGE_5W_MODE,
	CHARGE_10W_MODE,
	CHARGE_MODE_MAX,
};

struct idtp922xpgmtype {	 /* write to structure at SRAM address 0x0400 */
	u16 status;	 /* Read/Write by both 9220 and 9220 host */
	u16 startAddr; 	 /* OTP image address of the current packet */
	u16 codeLength;	 /* The size of the OTP image data in the current packet */
	u16 dataChksum;	 /* Checksum of the current packet */
	u8  dataBuf[128];	 /* OTP image data of the current packet */
};

/* proprietary packet type */
struct propkt_type {
	u8 header;  /* The header consists of a single byte that indicates the Packet type. */
	u8 cmd;	 /* Back channel command */
	u8 msg[5];  /* Send data buffer */
};

struct idtp9220_access_func {
	int (*read)(void *data, u16 reg, u8 *val);
	int (*write)(void *data, u16 reg, u8 val);
	int (*read_buf)(void *data, u16 reg, u8 *buf, u32 size);
	int (*write_buf)(void *data, u16 reg, u8 *buf, u32 size);
};

struct p922x_desc {
	const char *chg_dev_name;
	const char *alias_name;
};

struct p922x_switch_voltage {
	int voltage_low;
	int voltage_target;
	int voltage_high;
};

/*
 * In each RPP (Received Power Packet), RX will use
 * 'RPP = calcuatedPower * gain + offset' to report the power.
 */
struct p922x_fodcoeftype {
	u8 gain;
	u8 offs;
};

struct p922x_reg {
	u16 addr;
	u8  size;
};

struct p922x_dev {
	char *name;
	struct i2c_client *client;
	struct device *dev;
	struct regmap *regmap;
	struct idtp9220_access_func bus;
	struct p922x_desc *desc;
	struct charger_device *chg_dev;
#ifdef CONFIG_MTK_CHARGER_CLASS
	struct charger_properties chg_props;
#endif
	struct mutex sys_lock;
	struct mutex irq_lock;
	struct mutex fod_lock;
	struct power_supply_desc wpc_desc;
	struct power_supply_config wpc_cfg;
	struct delayed_work wpc_init_work;
	struct p922x_reg reg;
	struct power_supply *wpc_psy;
	int int_gpio;
	int pg_gpio;
	int int_num;
	int pg_num;
	atomic_t online;
	unsigned int wpc_support;
	bool use_buck;
	struct pinctrl *wpc_en_pinctrl;
	struct pinctrl_state *wpc_en_default;
	struct pinctrl_state *wpc_en0;
	struct pinctrl_state *wpc_en1;
	struct pinctrl *sleep_en_pinctrl;
	struct pinctrl_state *sleep_en_default;
	struct pinctrl_state *sleep_en0;
	struct pinctrl_state *sleep_en1;
	bool wpc_en;
	bool support_wpc_en;
	bool sleep_en;
	bool support_sleep_en;
	/* communication cap enable */
	bool cm_cap_en;
#ifdef CONFIG_CHARGER_HAL
	/* charger status update */
	struct delayed_work chg_update_status_work;
	volatile unsigned long chg_flag;
#endif

	/* fastcharge switch */
	bool tx_id_authen_status;
	bool tx_dev_authen_status;
	bool tx_authen_complete;
	struct delayed_work power_switch_work;
	enum charger_type power_switch_target;
	int switch_vth_low;
	int switch_vth_high;
	int wpc_mivr[CHARGE_MODE_MAX];
	struct p922x_switch_voltage switch_voltage[CHARGE_MODE_MAX];
	struct p922x_fodcoeftype bpp_5w_fod[FOD_COEF_ARRY_LENGTH];
	struct p922x_fodcoeftype bpp_10w_fod[FOD_COEF_ARRY_LENGTH];
	struct switch_dev dock_state;
	bool force_switch;
	bool is_hv_adapter;
	bool is_enabled;
	u8 tx_adapter_type;
	u8 over_reason;
	u8 bpp_5w_fod_num;
	u8 bpp_10w_fod_num;
	u8 dev_auth_retry;

	/* OVP issue workaround */
	unsigned int ovp_cnt;
};

enum dock_state_type {
	TYPE_DOCKED = 7,
	TYPE_UNDOCKED = 8,
};

enum led_mode {
	LED_CONSTANT_ON = 99,
	LED_CONSTANT_OFF = 100,
};

#endif
