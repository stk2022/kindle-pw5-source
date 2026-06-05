/*
 * bd71827-power.c
 * @file ROHM BD71827 Charger driver
 *
 * Copyright 2016.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

//#define DEBUG
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/power_supply.h>
#include <linux/mfd/rohm-bd71827.h>
#include <linux/mfd/rohm-bd71828.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/rtc.h>
#ifdef CONFIG_CHARGER_HAL
#include <linux/power/charger-hal.h>
#endif
#ifdef CONFIG_MTK_CHARGER_CLASS
#include <linux/power/mtk_charger_class.h>
#endif /* CONFIG_MTK_CHARGER_CLASS */
#include <linux/thermal.h>
#define MAX(X, Y) ((X) >= (Y) ? (X) : (Y))
#define uAMP_TO_mAMP(ma) ((ma) / 1000)
#define mAMP_TO_uAMP(ma) ((ma) * 1000)

/* BD71828 and BD71827 common defines */
#define BD7182x_MASK_VBAT_U		0x1f
#define BD7182x_MASK_VDCIN_U	0x0f
#define BD7182x_MASK_IBAT_U		0x3f
#define BD7182x_MASK_CURDIR_DISCHG	0x80
#define BD7182x_MASK_CC_CCNTD_HI	0x0FFF
#define BD7182x_MASK_CC_CCNTD		0x0FFFFFFF
#define BD7182x_MASK_CHG_STATE		0x7f
#define BD7182x_MASK_CC_FULL_CLR	0x10
#define BD7182x_MASK_BAT_TEMP		0x07
#define BD7182x_MASK_DCIN_DET		0x01
#define BD7182x_MASK_CONF_PON		0x01
#define BD7182x_MASK_BAT_STAT		0x3f
#define BD7182x_MASK_DCIN_STAT		0x07
#define BD7182x_MASK_CHG_INIT_CHGRST	0x02
#define BD7182x_MASK_DCIN_ILIM		0x3f
#define BD7182x_MASK_CHG_IFST		0x3f
#define BD7182x_MASK_CCNTRST		0x80
#define BD7182x_MASK_CCNTENB		0x40
#define BD7182x_MASK_CCCALIB		0x20
#define BD7182x_MASK_WDT_AUTO		0x40
#define BD7182x_MASK_WDT_DIS		0x80
#define BD7182x_MASK_VBAT_ALM_LIMIT_U	0x01
#define BD7182x_MASK_CHG_EN		0x01

#define BD7182x_DCIN_COLLAPSE_DEFAULT	0x36

#define CHG_DCIN_SET_STEP 	50
#define CHG_IFST_STEP 		25

/* Measured min and max value clear bits */
#define BD7182x_MASK_VSYS_MIN_AVG_CLR	0x10
#define BD7182x_MASK_VBAT_MIN_AVG_CLR	0x01

#define JITTER_DEFAULT			10000		/* 10 seconds */
#define JITTER_REPORT_CAP		30000		/* 30 seconds */
#define BATTERY_CAP_MAH_DEFAULT		1708
#define MAX_VOLTAGE_DEFAULT		ocv_table_default[0]
#define MIN_VOLTAGE_DEFAULT		3500000
#define THR_VOLTAGE_DEFAULT		4350000
#define AC_NAME					"bd71827_ac"
#define BAT_NAME				"bd71827_bat"
#define BATTERY_FULL_DEFAULT	100

#define LOW_BATT_VOLT_LEVEL	0
#define CRIT_BATT_VOLT_LEVEL	1
#define FG_LOW_BATT_CT		3
#define SYS_LOW_VOLT_THRESH	3500
#define SYS_CRIT_VOLT_THRESH	3400
#define LOW_BATT_CHECK_DELAY	2000

#define BD71827_COOLING_DEV		"bd71827_cooling_dev"
#define MAX_BD71827_COOLING_DEV_STATE	1
static unsigned long bd71827_cooling_dev_state = 1;
#define bd71827_cooling_dev_register   thermal_cooling_device_register


/*Safe charge settings*/
/* DCIN safe charging delay -- 7days==7*24*60*60*1000*/
#define DCIN_SAFE_CHARGING_DELAY       	604800000


#define RESET_CHGINT_DELAY       	100
#define CURRENT_RAMP_DELAY       	1000 /*1 second*/
#define CURRENT_RAMP_STEP       	100 /*100 mA*/

#define	ILIM_DCIN_RECOVER_DELAY		1800000 /* 30*60 seconds */
extern unsigned int bd71828_in_shpm;

/* 3.72V to 4.60V range, 10mV step: 0x0-3.72, 0x1-3.73, ... */
#define BD71828_CHG_VBAT_CHG_4P2V 	0x30 //maps to 4.2V
#define BD71828_CHG_VBAT_CHG_4P1V 	0x26 //maps to 4.1V
#define BD71828_CHG_VBAT_CHG_1_DEFAULT	0x44 //maps to 4.4V == OTP value
#define BD71828_CHG_VBAT_CHG_2_DEFAULT	0x26 //maps to 4.1V == OTP value
#define BD71828_CHG_VBAT_CHG_3_DEFAULT	0x44 //maps to 4.4v == OTP value
/*Safe charge settings*/

#define BD71828_CHG_WDT_FST_DEFAULT	0x64 //default WDT_FST value

#define BY_BAT_VOLT				0
#define BY_VBATLOAD_REG			1
#define INIT_COULOMB			BY_VBATLOAD_REG

#define CALIB_CURRENT_A2A3		0xCE9E


#define CHG_DCIN_UPPER_LIMIT			1500		/* mA */
#define MAX_CURRENT_DCP_DEFAULT			1425000		/* uA */
#define MAX_CURRENT_CHG_OUTPUT_DCP_DEFAULT	1500000		/* uA */
#define MAX_CURRENT_SDP_DEFAULT			 475000		/* uA */
#define MAX_CURRENT_CHG_OUTPUT_SDP_DEFAULT	 525000		/* uA, @TODO fine tune this value */
#define MAX_CURRENT_WIRELESS_HIGH_DEFAULT	1500000		/* uA */
#define MAX_CURRENT_WIRELESS_LOW_DEFAULT	 500000		/* uA */


/*
 * VBAT Low voltage detection Threshold
 * 0x00D4*16mV = 212*0.016 = 3.392v
 */
#define VBAT_LOW_TH				0x00D4
#define RS_30mOHM
#ifdef RS_30mOHM
#define A10s_mAh(s)		((s) * 1000 / (360 * 3))
#define mAh_A10s(m)		((m) * (360 * 3) / 1000)
#else
#define A10s_mAh(s)		((s) * 1000 / 360)
#define mAh_A10s(m)		((m) * 360 / 1000)
#endif

#define THR_RELAX_CURRENT_DEFAULT	5		/* mA */
#define THR_RELAX_TIME_DEFAULT		(60 * 60)	/* sec. */

#define DGRD_CYC_CAP_DEFAULT		88	/* 1 micro Ah unit */

#define DGRD_TEMP_H_DEFAULT			45	/* 1 degrees C unit */
#define DGRD_TEMP_M_DEFAULT			25	/* 1 degrees C unit */
#define DGRD_TEMP_L_DEFAULT			5	/* 1 degrees C unit */
#define DGRD_TEMP_VL_DEFAULT		0	/* 1 degrees C unit */

#define SOC_EST_MAX_NUM_DEFAULT		5
#define DGRD_TEMP_CAP_H_DEFAULT		(0)	/* 1 micro Ah unit */
#define DGRD_TEMP_CAP_M_DEFAULT		(0)	/* 1 micro Ah unit */
#define DGRD_TEMP_CAP_L_DEFAULT		(0)	/* 1 micro Ah unit */

#define PWRCTRL_NORMAL				0x22
#define PWRCTRL_RESET				0x23

extern int idme_hwid_value;

/* TODO: Evaluate which members of "pwr" are really updated/read from separate
 * threads and actually do require memory barriers. Furthermore, evaluate
 * if the smp_rmb() is only required at start of update cycle / start of
 * request callbacks. This current 'call barrier for every access to "pwr"
 * is probably terrible for cache usage on the system...
 */

#define PWRCTRL_HACK
struct pwr_regs {
	u8 vbat_init;
	u8 vbat_init2;
	u8 vbat_init3;
	u8 vbat_avg;
	u8 bat_set2;
	u8 ibat;
	u8 ibat_avg;
	u8 vsys_avg;
	u8 vbat_min_avg;
	u8 meas_clear;
	u8 vsys_min;
	u8 vsys_min_avg;
	u8 btemp_vth;
	u8 chg_state;
	u8 coulomb3;
	u8 coulomb2;
	u8 coulomb1;
	u8 coulomb0;
	u8 coulomb_ctrl;
	u8 rex_ctrl;
	u8 rex_curcd_th;
	u8 vbat_rex_avg;
	u8 rex_clear_reg;
	u8 rex_clear_mask;
	u8 coulomb_full3;
	u8 cc_full_clr;
	u8 coulomb_chg3;
	u8 bat_temp;
	u8 bat_id;
	u8 dcin_stat;
	u8 dcin_collapse_limit;
	u8 ilim_dcin_stat;
	u8 dcin_set;
	u8 chg_wdt_stat;
	u8 chg_wdt_fst;
	u8 chg_set1;
	u8 chg_init;
	u8 chg_en;
	u8 chg_ifst;
	u8 chg_vbat_1;
	u8 chg_vbat_2;
	u8 chg_vbat_3;
	u8 vbat_alm_limit_u;
	u8 batcap_mon_limit_u;
	u8 conf;
	u8 bat_stat;
	u8 vdcin;
#ifdef PWRCTRL_HACK
	u8 pwrctrl;
	u8 ps_ctrl_2;
	u8 hibernate_mask;

#endif // PWRCTRL_HACK
};

struct pwr_regs pwr_regs_bd71827 = {
	.vbat_init = BD71827_REG_VM_OCV_PRE_U,
	.vbat_init2 = BD71827_REG_VM_OCV_PST_U,
	.vbat_init3 = BD71827_REG_VM_OCV_PWRON_U,
	.vbat_avg = BD71827_REG_VM_SA_VBAT_U,
	.ibat = BD71827_REG_CC_CURCD_U,
	.ibat_avg = BD71827_REG_CC_SA_CURCD_U,
	.vsys_avg = BD71827_REG_VM_SA_VSYS_U,
	.vbat_min_avg = BD71827_REG_VM_SA_VBAT_MIN_U,
	.meas_clear = BD71827_REG_VM_SA_MINMAX_CLR,
	.vsys_min_avg = BD71827_REG_VM_SA_VSYS_MIN_U,
	.btemp_vth = BD71827_REG_VM_BTMP,
	.chg_state = BD71827_REG_CHG_STATE,
	.coulomb3 = BD71827_REG_CC_CCNTD_3,
	.coulomb2 = BD71827_REG_CC_CCNTD_2,
	.coulomb1 = BD71827_REG_CC_CCNTD_1,
	.coulomb0 = BD71827_REG_CC_CCNTD_0,
	.coulomb_ctrl = BD71827_REG_CC_CTRL,
	.vbat_rex_avg = BD71827_REG_REX_SA_VBAT_U,
	.rex_clear_reg = BD71827_REG_REX_CTRL_1,
	.rex_clear_mask = BD71827_REX_CLR_MASK,
	.coulomb_full3 = BD71827_REG_FULL_CCNTD_3,
	.cc_full_clr = BD71827_REG_FULL_CTRL,
	.coulomb_chg3 = BD71827_REG_CCNTD_CHG_3,
	.bat_temp = BD71827_REG_BAT_TEMP,
	.dcin_stat = BD71827_REG_DCIN_STAT,
	.dcin_collapse_limit = BD71827_REG_DCIN_CLPS,
	.dcin_set = BD71827_REG_DCIN_SET,
	.chg_set1 = BD71827_REG_CHG_SET1,
	.chg_en = BD71827_REG_CHG_SET1,
	.chg_ifst = BD71827_REG_CHG_IFST,
	.vbat_alm_limit_u = BD71827_REG_ALM_VBAT_TH_U,
	.batcap_mon_limit_u = BD71827_REG_CC_BATCAP1_TH_U,
	.conf = BD71827_REG_CONF,
	.bat_stat = BD71827_REG_BAT_STAT,
	.vdcin = BD71827_REG_VM_DCIN_U,
#ifdef PWRCTRL_HACK
	.pwrctrl = BD71827_REG_PWRCTRL,
	.hibernate_mask = 0x1,
#endif
};

struct pwr_regs pwr_regs_bd71828 = {
	.vbat_init = BD71828_REG_VBAT_INITIAL1_U,
	.vbat_init2 = BD71828_REG_VBAT_INITIAL2_U,
	.vbat_init3 = BD71828_REG_OCV_PWRON_U,
	.vbat_avg = BD71828_REG_VBAT_U,
	.bat_set2 = BD71828_REG_BAT_SET_2,
	.ibat = BD71828_REG_IBAT_U,
	.ibat_avg = BD71828_REG_IBAT_AVG_U,
	.vsys_avg = BD71828_REG_VSYS_AVG_U,
	.vbat_min_avg = BD71828_REG_VBAT_MIN_AVG_U,
	.meas_clear = BD71828_REG_MEAS_CLEAR,
	.vsys_min = BD71828_REG_VSYS_MIN,
	.vsys_min_avg = BD71828_REG_VSYS_MIN_AVG_U,
	.btemp_vth = BD71828_REG_VM_BTMP_U,
	.chg_state = BD71828_REG_CHG_STATE,
	.coulomb3 = BD71828_REG_CC_CNT3,
	.coulomb2 = BD71828_REG_CC_CNT2,
	.coulomb1 = BD71828_REG_CC_CNT1,
	.coulomb0 = BD71828_REG_CC_CNT0,
	.coulomb_ctrl = BD71828_REG_COULOMB_CTRL,
	.rex_ctrl = BD71828_REG_REX_CTRL,
	.rex_curcd_th = BD71828_REG_REX_CURCD_TH,
	.vbat_rex_avg = BD71828_REG_VBAT_REX_AVG_U,
	.rex_clear_reg = BD71828_REG_COULOMB_CTRL2,
	.rex_clear_mask = BD71828_MASK_REX_CC_CLR,
	.coulomb_full3 = BD71828_REG_CC_CNT_FULL3,
	.cc_full_clr = BD71828_REG_COULOMB_CTRL2,
	.coulomb_chg3 = BD71828_REG_CC_CNT_CHG3,
	.bat_temp = BD71828_REG_BAT_TEMP,
	.bat_id = BD71828_REG_VM_BATID,
	.dcin_stat = BD71828_REG_DCIN_STAT,
	.dcin_collapse_limit = BD71828_REG_DCIN_CLPS,
	.ilim_dcin_stat = BD71828_REG_ILIM_DCIN_STAT,
	.dcin_set = BD71828_REG_DCIN_SET,
	.chg_wdt_stat = BD71828_REG_CHG_WDT_STAT,
	.chg_wdt_fst = BD71828_REG_CHG_WDT_FST,
	.chg_init = BD71828_REG_CHG_INIT,
	.chg_set1 = BD71828_REG_CHG_SET1,
	.chg_en   = BD71828_REG_CHG_EN,
	.chg_ifst = BD71828_REG_CHG_IFST,
	.chg_vbat_1 = BD71828_REG_CHG_VBAT_1,
	.chg_vbat_2 = BD71828_REG_CHG_VBAT_2,
	.chg_vbat_3 = BD71828_REG_CHG_VBAT_3,
	.vbat_alm_limit_u = BD71828_REG_ALM_VBAT_LIMIT_U,
	.batcap_mon_limit_u = BD71828_REG_BATCAP_MON_LIMIT_U,
	.conf = BD71828_REG_CONF,
	.bat_stat = BD71828_REG_BAT_STAT,
	.vdcin = BD71828_REG_VDCIN_U,
#ifdef PWRCTRL_HACK
	.pwrctrl = BD71828_REG_PS_CTRL_1,
	.ps_ctrl_2= BD71828_REG_PS_CTRL_2,
	.hibernate_mask = 0x2,
#endif
};

#define USE_BAT_PARAMS_DEFAULT	0
#define USE_BAT_PARAMS_15KOHM_ATL	1
#define USE_BAT_PARAMS_22KOHM_MURATA 	2
#define USE_BAT_PARAMS_29KOHM_LIWINON 	3

/********
 Range of the BATID reading, should not overlap
 * 15KOHM (ATL), battery id sensor voltage range: 152mV to 161mv
 * 22KOHM (MURATA), battery id sensor voltage range: 210mv to 222mv
 * 29.4KOHM (LIWINON), battery id sensor voltage range: 265mv to 280mv
 * PMIC  register granuality is 4.71mv, so the range is :
 * 15KOHM, 32--35, [20h, 23h]
 * 22KOHM, 44--48, [2Ch, 30h]
 * 29.4KOHM, 56--60, [38h, 3Ch]
 * update with +/-1 of tolerance to be:
 * 15KOHM, 31--36, [1Fh, 24h]
 * 22KOHM, 43--49, [2Bh, 31h]
 * 29.4KOHM, 55--61, [37h, 3Dh]
*********/
#define BAT_ID_15K_RANGE_LOW	0x1F
#define BAT_ID_15K_RANGE_HIGH	0x24
#define BAT_ID_22K_RANGE_LOW	0x2B
#define BAT_ID_22K_RANGE_HIGH	0x31
#define BAT_ID_29K_RANGE_LOW	0x37
#define BAT_ID_29K_RANGE_HIGH	0x3D


static int ocv_table_default[23] = {
	4350000,
	4325945,
	4255935,
	4197476,
	4142843,
	4090615,
	4047113,
	3987352,
	3957835,
	3920815,
	3879834,
	3827010,
	3807239,
	3791379,
	3779925,
	3775038,
	3773530,
	3756695,
	3734099,
	3704867,
	3635377,
	3512942,
	3019825
};	/* unit 1 micro V */

static int soc_table_default[23] = {
	1000,
	1000,
	950,
	900,
	850,
	800,
	750,
	700,
	650,
	600,
	550,
	500,
	450,
	400,
	350,
	300,
	250,
	200,
	150,
	100,
	50,
	0,
	-50
	/* unit 0.1% */
};

static int vdr_table_h_default[23] = {
	100,
	100,
	102,
	104,
	105,
	108,
	111,
	115,
	122,
	138,
	158,
	96,
	108,
	112,
	117,
	123,
	137,
	109,
	131,
	150,
	172,
	136,
	218
};

static int vdr_table_m_default[23] = {
	100,
	100,
	100,
	100,
	102,
	104,
	114,
	110,
	127,
	141,
	139,
	96,
	102,
	106,
	109,
	113,
	130,
	134,
	149,
	188,
	204,
	126,
	271
};

static int vdr_table_l_default[23] = {
	100,
	100,
	98,
	96,
	96,
	96,
	105,
	94,
	108,
	105,
	95,
	89,
	90,
	92,
	99,
	112,
	129,
	143,
	155,
	162,
	156,
	119,
	326
};

static int vdr_table_vl_default[23] = {
	100,
	100,
	98,
	96,
	95,
	97,
	101,
	92,
	100,
	97,
	91,
	89,
	90,
	93,
	103,
	115,
	128,
	139,
	148,
	148,
	156,
	246,
	336
};


// Battery Parameters s ID15KOHM
static int ocv_table_15KOHM[23] = {
	4400000,
	4376596,
	4323174,
	4267767,
	4212433,
	4158597,
	4106762,
	4060597,
	4010210,
	3970026,
	3922432,
	3871715,
	3846390,
	3824908,
	3807177,
	3792605,
	3778561,
	3753938,
	3735012,
	3701662,
	3688472,
	3617389,
	2932946
};	/* unit 1 micro V */

static int vdr_table_h_15KOHM[23] = {
	100,
	100,
	102,
	102,
	103,
	104,
	104,
	107,
	111,
	114,
	120,
	94,
	102,
	108,
	115,
	115,
	110,
	104,
	110,
	109,
	116,
	128,
	525
};

static int vdr_table_m_15KOHM[23] = {
	100,
	100,
	100,
	100,
	100,
	100,
	100,
	100,
	98,
	95,
	93,
	90,
	88,
	90,
	93,
	95,
	98,
	98,
	100,
	106,
	109,
	130,
	451
};

static int vdr_table_l_15KOHM[23] = {
	100,
	100,
	98,
	96,
	95,
	98,
	102,
	114,
	109,
	112,
	100,
	100,
	100,
	102,
	105,
	107,
	110,
	112,
	115,
	125,
	144,
	219,
	393
};

static int vdr_table_vl_15KOHM[23] = {
	100,
	100,
	98,
	95,
	95,
	95,
	95,
	105,
	98,
	99,
	94,
	91,
	91,
	92,
	95,
	98,
	103,
	113,
	117,
	130,
	157,
	195,
	31,
};

static int ocv_table_15KOHM_4p2v[] = {
	4200000,
	4182178,
	4137741,
	4096313,
	4056400,
	4018038,
	3982363,
	3951652,
	3902790,
	3870728,
	3848475,
	3830104,
	3812723,
	3801598,
	3790082,
	3774046,
	3756685,
	3739121,
	3716635,
	3696893,
	3681166,
	3570006,
	2756654
};	/* unit 1 micro V */

static int vdr_table_h_15KOHM_4p2v[] = {
	100,
	100,
	102,
	104,
	105,
	106,
	107,
	107,
	105,
	102,
	100,
	95,
	102,
	108,
	110,
	110,
	108,
	108,
	110,
	109,
	116,
	128,
	525
};

static int vdr_table_m_15KOHM_4p2v[] = {
	100,
	85,
	80,
	80,
	80,
	85,
	85,
	85,
	82,
	80,
	82,
	84,
	85,
	88,
	90,
	90,
	95,
	98,
	100,
	106,
	109,
	130,
	451
};

static int vdr_table_l_15KOHM_4p2v[] = {
	100,
	100,
	98,
	96,
	94,
	93,
	93,
	93,
	88,
	85,
	83,
	85,
	85,
	90,
	92,
	95,
	98,
	103,
	111,
	122,
	144,
	219,
	393
};

static int vdr_table_vl_15KOHM_4p2v[] = {
	100,
	88,
	80,
	75,
	80,
	85,
	88,
	88,
	85,
	83,
	85,
	85,
	85,
	91,
	95,
	98,
	101,
	110,
	117,
	130,
	157,
	195,
	31,
};

// Battery Parameters s ID15KOHM END

// Battery Parameters s ID22KOHM
static int ocv_table_22KOHM[23] = {
	4400000,
	4367975,
	4305984,
	4248505,
	4192249,
	4138218,
	4087217,
	4039966,
	3996696,
	3955019,
	3908135,
	3868556,
	3840593,
	3819052,
	3801609,
	3787412,
	3771371,
	3749296,
	3728382,
	3697892,
	3674466,
	3588512,
	3100049
};	/* unit 1 micro V */

static int vdr_table_h_22KOHM[23] = {
	100,
	100,
	101,
	103,
	104,
	104,
	109,
	112,
	118,
	120,
	108,
	106,
	105,
	108,
	111,
	121,
	120,
	118,
	117,
	115,
	122,
	138,
	386
};

static int vdr_table_m_22KOHM[23] = {
	100,
	100,
	102,
	104,
	105,
	105,
	110,
	113,
	116,
	118,
	114,
	111,
	113,
	120,
	124,
	131,
	133,
	120,
	137,
	128,
	136,
	160,
	316
};

static int vdr_table_l_22KOHM[23] = {
	100,
	95,
	90,
	95,
	100,
	102,
	104,
	104,
	105,
	105,
	105,
	103,
	106,
	110,
	114,
	119,
	125,
	128,
	144,
	155,
	170,
	347,
	404
};

static int vdr_table_vl_22KOHM[23] = {
	100,
	100,
	103,
	106,
	108,
	110,
	111,
	110,
	108,
	105,
	105,
	105,
	105,
	110,
	113,
	121,
	123,
	131,
	141,
	168,
	217,
	360,
	446
};

static int ocv_table_22KOHM_4p2v[] = {
	4200000,
	4175985,
	4129910,
	4087196,
	4047492,
	4010767,
	3976627,
	3942327,
	3900211,
	3868072,
	3844260,
	3825346,
	3806891,
	3796569,
	3785451,
	3769905,
	3752091,
	3735041,
	3713713,
	3687477,
	3665022,
	3581425,
	3165616
};	/* unit 1 micro V */

static int vdr_table_h_22KOHM_4p2v[] = {
	100,
	100,
	100,
	102,
	103,
	103,
	103,
	103,
	103,
	103,
	104,
	104,
	104,
	105,
	108,
	111,
	112,
	113,
	117,
	115,
	122,
	138,
	386
};

static int vdr_table_m_22KOHM_4p2v[] = {
	100,
	98,
	95,
	98,
	100,
	104,
	108,
	110,
	108,
	107,
	105,
	107,
	110,
	113,
	116,
	117,
	118,
	120,
	125,
	128,
	136,
	160,
	316
};

static int vdr_table_l_22KOHM_4p2v[] = {
	100,
	83,
	78,
	75,
	80,
	83,
	85,
	88,
	86,
	85,
	86,
	90,
	92,
	100,
	103,
	105,
	110,
	115,
	134,
	155,
	170,
	347,
	404
};

static int vdr_table_vl_22KOHM_4p2v[] = {
	100,
	75,
	71,
	80,
	85,
	90,
	93,
	95,
	95,
	95,
	95,
	98,
	102,
	112,
	115,
	121,
	123,
	131,
	141,
	168,
	217,
	360,
	446
};
//Battery Parameters ID22KOHM end


int use_load_bat_params = USE_BAT_PARAMS_DEFAULT;

static int battery_cap_mah;
static int battery_cap;
int max_voltage;
int min_voltage;
int thr_voltage;

int dgrd_cyc_cap;

int soc_est_max_num;

int dgrd_temp_cap_h;
int dgrd_temp_cap_m;
int dgrd_temp_cap_l;

static unsigned int battery_cycle;
static unsigned int battery_stressed = false;

#define SAFE_CHARGING_CONTROLLED_BY_USERSPASE
static unsigned int battery_safe_charging = false;

int ocv_table[23];
int soc_table[23];
int vdr_table_h[23];
int vdr_table_m[23];
int vdr_table_l[23];
int vdr_table_vl[23];

#define BD71827_SUSPEND 	0
#define BD71827_RESUME  	1
static int bd71827_suspend_status;

struct bd7182x_soc_data {
	int    vbus_status;		/* < last vbus status */
	int    charge_status;		/* < last charge status */
	int    bat_status;		/* < last bat status */

	int	bat_online;		/* < battery connect */
	int	charger_online;		/* < charger connect */
	int	vcell;			/* < battery voltage */
	int	vsys;			/* < system voltage */
	int	vcell_min;		/* < minimum battery voltage */
	int	vsys_min;		/* < minimum system voltage */
	int	rpt_status;		/* < battery status report */
	int	prev_rpt_status;	/* < previous battery status report */
	int	bat_health;		/* < battery health */
	int	designed_cap;		/* < battery designed capacity */
	int	full_cap;		/* < battery capacity */
	int	curr;			/* < battery current from ADC */
	int	curr_avg;		/* < average battery current */
	int	temp;			/* < battery tempature */
	u32	coulomb_cnt;		/* < Coulomb Counter */
	int	state_machine;		/* < initial-procedure state machine */
	u32	soc_norm;		/* < State Of Charge using full
					 * capacity without by load
					 */
	u32	soc;			/* < State Of Charge using full
					 * capacity with by load
					 */
	u32	clamp_soc;		/* < Clamped State Of Charge using
					 * full capacity with by load
					 */
	int	relax_time;		/* < Relax Time */
	u32	cycle;			/* < Charging and Discharging cycle
					 * number
					 */
	u8	ilim_dcin_recover_pending;
	u8	dcin_val;		/*DCIN reg value*/
	u8	ifst_val;		/*IFST reg value*/
	u8	ilim_dcin_stat;		/*ilim_dcin_stat value*/
	int	curr_dcin;
	int	chg_dcin_cur_limit;		/* charger DCIN current limit */
	int	charge_control_limit;		/* charge control limit */
	u32	thermal_input_power_limit;		/* Thermal charge current limit */

};

/* @brief power device */
struct bd71827_power {
	struct rohm_regmap_dev *mfd;	/* < parent for access register */
	struct power_supply *ac;	/* < alternating current power */
	struct power_supply *bat;	/* < battery power */
	int gauge_delay;		/* < Schedule to call gauge algorithm */
	struct bd7182x_soc_data d_r;	/* < SOC algorithm data for reporting */
	struct bd7182x_soc_data d_w;	/* < internal SOC algorithm data */
	spinlock_t dlock;
	struct delayed_work bd_work;	/* < delayed work for timed work */
	struct delayed_work bd_lobat_check_work;		/**< delayed work for timed work */
#ifndef SAFE_CHARGING_CONTROLLED_BY_USERSPASE
	struct delayed_work bd_safe_charging_work;	/** delayed work for usb safe charging control **/
#endif
	struct delayed_work bd_reset_chgint_work;	/** delayed work for resetting PMIC chgint upon charger source change **/
	struct delayed_work bd_log_save_work;	/** delayed work for restoring pstore log after reboot caused by long press **/
	struct delayed_work bd_current_ramp_work;	/** delayed work for ramping up charging current **/
	struct delayed_work bd_ilim_dcin_recover_work;	/** delayed work for recovering dcin_ilim **/

	struct pwr_regs *regs;
	/* Reg val to uA */
	int curr_factor;
	int (*get_temp)(struct bd71827_power *pwr, int *temp);
#ifdef CONFIG_CHARGER_HAL
	struct charger_hal_pmic_device *bd71827_pmic_dev;
#endif

	struct thermal_cooling_device *bd71827_cooling_dev;
};

#define CALIB_NORM			0
#define CALIB_START			1
#define CALIB_GO			2

enum {
	STAT_POWER_ON,
	STAT_INITIALIZED,
};


int critbat_event_sent = 0;
int lobat_event_sent = 0;

static unsigned int bd71827_calc_soc_org(u32 cc, int designed_cap);
static void bd71827_safe_charging_control(struct bd71827_power *pwr, int on_off);

int pmic_current_event_handler(unsigned long event, void *ptr);
int bd71828_get_batt_temperature(void);

static int bd7182x_write16(struct bd71827_power *pwr, int reg, u16 val)
{
	val = cpu_to_be16(val);

	return regmap_bulk_write(pwr->mfd->regmap, reg, &val, sizeof(val));
}

static int bd7182x_read16_himask(struct bd71827_power *pwr, int reg, int himask,
				 u16 *val)
{
	struct regmap *regmap = pwr->mfd->regmap;
	int ret;
	u8 *tmp = (u8 *)val;

	ret = regmap_bulk_read(regmap, reg, val, sizeof(*val));
	if (!ret) {
		*tmp &= himask;
		*val = be16_to_cpu(*val);
	}
	return ret;
}

#if INIT_COULOMB == BY_VBATLOAD_REG
#define INITIAL_OCV_REGS 3
/* @brief get initial battery voltage and current
 * @param pwr power device
 * @return 0
 */
static int bd71827_get_init_bat_stat(struct bd71827_power *pwr,
				     int *ocv)
{
	int ret;
	int i;
	u8 regs[INITIAL_OCV_REGS] = {
		pwr->regs->vbat_init,
		pwr->regs->vbat_init2,
		pwr->regs->vbat_init3
	};
	uint16_t vals[INITIAL_OCV_REGS];

	*ocv = 0;
	for (i = 0; i < INITIAL_OCV_REGS; i++) {
		ret = bd7182x_read16_himask(pwr, regs[i], BD7182x_MASK_VBAT_U,
					    &vals[i]);
		if (ret) {
			dev_err(pwr->mfd->dev,
				"Failed to read initial battery voltage\n");
			return ret;
		}
		*ocv = MAX(vals[i], *ocv);

		dev_dbg(pwr->mfd->dev, "VM_OCV_%d = %d\n", i,
			((int)vals[i]) * 1000);
	}

	*ocv *= 1000;
	return ret;
}
#endif

/* @brief get battery average voltage
 * @param pwr power device
 * @param vcell pointer to return back voltage in unit uV.
 * @return 0
 */
static int bd71827_get_vbat(struct bd71827_power *pwr, int *vcell)
{
	u16 tmp_vcell;
	int ret;

	ret = bd7182x_read16_himask(pwr, pwr->regs->vbat_avg,
				    BD7182x_MASK_VBAT_U, &tmp_vcell);
	if (ret)
		dev_err(pwr->mfd->dev,
			"Failed to read battery average voltage\n");
	else
		*vcell = ((int)tmp_vcell) * 1000;

	return ret;
}

#if INIT_COULOMB == BY_BAT_VOLT
/* @brief get battery average voltage and current
 * @param pwr power device
 * @param vcell pointer to return back voltage in unit uV.
 * @param curr  pointer to return back current in unit uA.
 * @return 0
 */
static int bd71827_get_vbat_curr(struct bd71827_power *pwr,
				int *vcell, int *curr)
{
	int ret;

	ret = bd71827_get_vbat(pwr, vcell);
	*curr = 0;

	return ret;
}
#endif

/* @brief get battery current and battery average current from DS-ADC
 * @param pwr power device
 * @param current in unit uA
 * @param average current in unit uA
 * @return 0
 */
static int bd71827_get_current_ds_adc(struct bd71827_power *pwr,
					int *curr, int *curr_avg)
{
	u16 tmp_curr;
	char *tmp = (char *)&tmp_curr;
	int dir = 1;
	int regs[] = { pwr->regs->ibat, pwr->regs->ibat_avg };
	int *vals[] = { curr, curr_avg };
	int ret, i;

	for (dir = 1, i = 0; i < ARRAY_SIZE(regs); i++) {
		ret = regmap_bulk_read(pwr->mfd->regmap, regs[i], &tmp_curr,
				       sizeof(tmp_curr));
		if (ret)
			break;

		if (*tmp & BD7182x_MASK_CURDIR_DISCHG)
			dir = -1;

		*tmp &= BD7182x_MASK_IBAT_U;
		tmp_curr = be16_to_cpu(tmp_curr);

		*vals[i] = dir * ((int)tmp_curr) * pwr->curr_factor;
	}

	return ret;
}

/* @brief get system average voltage
 * @param pwr power device
 * @param vcell pointer to return back voltage in unit uV.
 * @return 0
 */
static int bd71827_get_vsys(struct bd71827_power *pwr, int *vsys)
{
	u16 tmp_vsys;
	int ret;

	ret = bd7182x_read16_himask(pwr, pwr->regs->vsys_avg,
				    BD7182x_MASK_VBAT_U, &tmp_vsys);
	if (ret)
		dev_err(pwr->mfd->dev,
			"Failed to read system average voltage\n");
	else
		*vsys = ((int)tmp_vsys) * 1000;

	return ret;
}

/* @brief get battery minimum average voltage
 * @param pwr power device
 * @param vcell pointer to return back voltage in unit uV.
 * @return 0
 */
static int bd71827_get_vbat_min(struct bd71827_power *pwr, int *vcell)
{
	u16 tmp_vcell = 0;
	int ret;

	ret = bd7182x_read16_himask(pwr, pwr->regs->vbat_min_avg,
				    BD7182x_MASK_VBAT_U, &tmp_vcell);
	if (ret)
		dev_err(pwr->mfd->dev,
			"Failed to read battery min average voltage\n");
	else
		ret = regmap_update_bits(pwr->mfd->regmap,
					 pwr->regs->meas_clear,
					 BD7182x_MASK_VBAT_MIN_AVG_CLR,
					 BD7182x_MASK_VBAT_MIN_AVG_CLR);

	*vcell = ((int)tmp_vcell) * 1000;

	return ret;
}

/* @brief get system minimum average voltage
 * @param pwr power device
 * @param vcell pointer to return back voltage in unit uV.
 * @return 0
 */
static int bd71827_get_vsys_min(struct bd71827_power *pwr, int *vcell)
{
	u16 tmp_vcell = 0;
	int ret;

	ret = bd7182x_read16_himask(pwr, pwr->regs->vsys_min_avg,
				    BD7182x_MASK_VBAT_U, &tmp_vcell);
	if (ret)
		dev_err(pwr->mfd->dev,
			"Failed to read system min average voltage\n");
	else
		ret = regmap_update_bits(pwr->mfd->regmap,
					 pwr->regs->meas_clear,
					 BD7182x_MASK_VSYS_MIN_AVG_CLR,
					 BD7182x_MASK_VSYS_MIN_AVG_CLR);

	*vcell = ((int)tmp_vcell) * 1000;

	return ret;
}

/* @brief get battery capacity
 * @param ocv open circuit voltage
 * @return capcity in unit 0.1 percent
 */
static int bd71827_voltage_to_capacity(int ocv)
{
	int i = 0;
	int soc;

	if (ocv > ocv_table[0]) {
		soc = soc_table[0];
	} else {
		for (i = 0; soc_table[i] != -50; i++) {
			if ((ocv <= ocv_table[i]) && (ocv > ocv_table[i + 1])) {
				soc = (soc_table[i] - soc_table[i + 1]) *
				      (ocv - ocv_table[i + 1]) /
				      (ocv_table[i] - ocv_table[i + 1]);
				soc += soc_table[i + 1];
				break;
			}
		}
		if (soc_table[i] == -50)
			soc = soc_table[i];
	}
	return soc;
}

/* @brief get battery temperature
 * @param pwr power device
 * @return temperature in unit deg.Celsius
 */
static int bd71827_get_temp(struct bd71827_power *pwr, int *temp)
{
	struct regmap *regmap = pwr->mfd->regmap;
	int ret;
	int t = 0;

	ret = regmap_read(regmap, pwr->regs->btemp_vth, &t);
	t = 200 - t;

	if (ret || t > 200) {
		dev_err(pwr->mfd->dev, "Failed to read battery temperature\n");
		*temp = 200;
	} else {
		*temp = t;
	}

	return ret;
}

static int bd71828_get_temp(struct bd71827_power *pwr, int *temp)
{
	u16 t = 0;
	int ret;
	int tmp = 200 * 10000;

	ret = bd7182x_read16_himask(pwr, pwr->regs->btemp_vth,
				    BD71828_MASK_VM_BTMP_U, &t);
	if (ret || t > 3200)
		dev_err(pwr->mfd->dev,
			"Failed to read system min average voltage\n");

	tmp -= 625ULL * (unsigned int)t;
	*temp = tmp / 10000;

	return ret;
}

static int bd71827_reset_coulomb_count(struct bd71827_power *pwr,
				       struct bd7182x_soc_data *wd);

/* @brief get battery charge status
 * @param pwr power device
 * @return 0 at success or negative error code.
 */
static int bd71827_charge_status(struct bd71827_power *pwr,
				 struct bd7182x_soc_data *wd)
{
	unsigned int state;
	int ret;

	wd->prev_rpt_status = wd->rpt_status;

	ret = regmap_read(pwr->mfd->regmap, pwr->regs->chg_state, &state);
	if (ret) {
		dev_err(pwr->mfd->dev, "charger status reading failed (%d)\n",
			ret);
		return ret;
	}

	state &= BD7182x_MASK_CHG_STATE;

	dev_dbg(pwr->mfd->dev, "%s(): CHG_STATE %d\n", __func__, state);

	switch (state) {
	case 0x00:
		wd->rpt_status = POWER_SUPPLY_STATUS_DISCHARGING;
		wd->bat_health = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x0E:
		wd->rpt_status = POWER_SUPPLY_STATUS_CHARGING;
		wd->bat_health = POWER_SUPPLY_HEALTH_GOOD;
		critbat_event_sent = 0;
		lobat_event_sent = 0;
		break;
	case 0x0F:
		wd->rpt_status = POWER_SUPPLY_STATUS_FULL;
		wd->bat_health = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case 0x10:
	case 0x11:
	case 0x12:
	case 0x13:
	case 0x14:
	case 0x20:
	case 0x21:
	case 0x22:
	case 0x23:
	case 0x24:
		wd->rpt_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		wd->bat_health = POWER_SUPPLY_HEALTH_OVERHEAT;
		break;
	case 0x30:
	case 0x31:
	case 0x32:
	case 0x40:
		wd->rpt_status = POWER_SUPPLY_STATUS_DISCHARGING;
		wd->bat_health = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case 0x7f:
	default:
		wd->rpt_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		wd->bat_health = POWER_SUPPLY_HEALTH_DEAD;
		break;
	}

	ret = bd71827_reset_coulomb_count(pwr, wd);

	return ret;
}

#if INIT_COULOMB == BY_BAT_VOLT
static int bd71827_calib_voltage(struct bd71827_power *pwr, int *ocv)
{
	int r, curr, volt, ret;

	bd71827_get_vbat_curr(pwr, &volt, &curr);

	ret = regmap_read(pwr->mfd->regmap, pwr->regs->chg_state, &r);
	if (ret) {
		dev_err(pwr->mfd->dev, "Charger state reading failed (%d)\n",
			ret);
	} else if (curr > 0) {
		/* voltage increment caused by battery inner resistor */
		if (r == 3)
			volt -= 100 * 1000;
		else if (r == 2)
			volt -= 50 * 1000;
	}
	*ocv = volt;

	return 0;
}
#endif
static int __write_cc(struct bd71827_power *pwr, u16 bcap,
		      unsigned int reg, u32 *new)
{
	int ret;
	u32 tmp;
	u16 *swap_hi = (u16 *)&tmp;
	u16 *swap_lo = swap_hi + 1;

	*swap_hi = cpu_to_be16(bcap & BD7182x_MASK_CC_CCNTD_HI);
	*swap_lo = 0;

	ret = regmap_bulk_write(pwr->mfd->regmap, reg, &tmp, sizeof(tmp));
	if (ret) {
		dev_err(pwr->mfd->dev, "Failed to write coulomb counter\n");
		return ret;
	}
	if (new)
		*new = cpu_to_be32(tmp);

	return ret;
}

static int write_cc(struct bd71827_power *pwr, u16 bcap)
{
	int ret;
	u32 new;

	ret = __write_cc(pwr, bcap, pwr->regs->coulomb3, &new);
	if (!ret)
		pwr->d_w.coulomb_cnt = new;

	return ret;
}

static int stop_cc(struct bd71827_power *pwr)
{
	struct regmap *r = pwr->mfd->regmap;

	return regmap_update_bits(r, pwr->regs->coulomb_ctrl,
				  BD7182x_MASK_CCNTENB, 0);
}

static int start_cc(struct bd71827_power *pwr)
{
	struct regmap *r = pwr->mfd->regmap;

	return regmap_update_bits(r, pwr->regs->coulomb_ctrl,
				  BD7182x_MASK_CCNTENB, BD7182x_MASK_CCNTENB);
}

static int update_cc(struct bd71827_power *pwr, u16 bcap)
{
	int ret;

	ret = stop_cc(pwr);
	if (ret)
		goto err_out;

	ret = write_cc(pwr, bcap);
	if (ret)
		goto enable_out;

	ret = start_cc(pwr);
	if (ret)
		goto enable_out;

	return 0;

enable_out:
	start_cc(pwr);
err_out:
	dev_err(pwr->mfd->dev, "Coulomb counter write failed  (%d)\n", ret);
	return ret;
}

static int __read_cc(struct bd71827_power *pwr, u32 *cc, unsigned int reg)
{
	int ret;
	u32 tmp_cc = 0;

	ret = regmap_bulk_read(pwr->mfd->regmap, reg, &tmp_cc, sizeof(tmp_cc));
	if (ret) {
		dev_err(pwr->mfd->dev, "Failed to read coulomb counter\n");
		return ret;
	}
	*cc = be32_to_cpu(tmp_cc) & BD7182x_MASK_CC_CCNTD;

	return 0;
}

static int read_cc_full(struct bd71827_power *pwr, u32 *cc)
{
	return __read_cc(pwr, cc, pwr->regs->coulomb_full3);
}

static int read_cc(struct bd71827_power *pwr, u32 *cc)
{
	return __read_cc(pwr, cc, pwr->regs->coulomb3);
}

static int limit_cc(struct bd71827_power *pwr, struct bd7182x_soc_data *wd,
			 u32 *soc_org)
{
	uint16_t bcap;
	int ret;

	*soc_org = 100;
	bcap = wd->designed_cap + wd->designed_cap / 200;
	ret = update_cc(pwr, bcap);

	dev_dbg(pwr->mfd->dev,  "Limit Coulomb Counter\n");
	dev_dbg(pwr->mfd->dev,  "CC_CCNTD = %d\n", wd->coulomb_cnt);

	return ret;
}


/* @brief set initial coulomb counter value from battery voltage
 * @param pwr power device
 * @return 0
 */
static int calibration_coulomb_counter(struct bd71827_power *pwr,
				       struct bd7182x_soc_data *wd)
{
	struct regmap *regmap = pwr->mfd->regmap;
	u32 bcap;
	int soc, ocv, ret = 0, tmpret = 0;

#if INIT_COULOMB == BY_VBATLOAD_REG
	/* Get init OCV by HW */
	bd71827_get_init_bat_stat(pwr, &ocv);

	dev_dbg(pwr->mfd->dev, "ocv %d\n", ocv);
#elif INIT_COULOMB == BY_BAT_VOLT
	bd71827_calib_voltage(pwr, &ocv);
#endif

	/* Get init soc from ocv/soc table */
	soc = bd71827_voltage_to_capacity(ocv);
	dev_dbg(pwr->mfd->dev, "soc %d[0.1%%]\n", soc);
	if (soc < 0)
		soc = 0;
	bcap = wd->designed_cap * soc / 1000;

	tmpret = write_cc(pwr, bcap + wd->designed_cap / 200);
	if (tmpret)
		goto enable_cc_out;

	dev_dbg(pwr->mfd->dev, "%s() CC_CCNTD = %d\n", __func__,
		wd->coulomb_cnt);

enable_cc_out:
	/* Start canceling offset of the DS ADC. This needs 1 second at least */
	ret = regmap_update_bits(regmap, pwr->regs->coulomb_ctrl,
				 BD7182x_MASK_CCCALIB, BD7182x_MASK_CCCALIB);

	return (tmpret) ? tmpret : ret;
}

/* @brief adjust coulomb counter values at relaxed state
 * @param pwr power device
 * @return 0
 */
static int bd71827_adjust_coulomb_count(struct bd71827_power *pwr,
					struct bd7182x_soc_data *wd)
{
	int relax_ocv;
	u16 tmp;
	struct regmap *regmap = pwr->mfd->regmap;
	int ret;

	ret = bd7182x_read16_himask(pwr, pwr->regs->vbat_rex_avg,
			BD7182x_MASK_VBAT_U, &tmp);
	if (ret)
		return ret;

	relax_ocv = ((int)tmp) * 1000;

	dev_dbg(pwr->mfd->dev,  "%s(): relax_ocv = 0x%x\n", __func__,
		relax_ocv);
	if (relax_ocv != 0) {
		u32 bcap;
		int soc;

		/* Clear Relaxed Coulomb Counter */
		ret = regmap_update_bits(regmap, pwr->regs->rex_clear_reg,
					 pwr->regs->rex_clear_mask,
					 pwr->regs->rex_clear_mask);
		if (ret)
			return ret;

		/* Get soc at relaxed state from ocv/soc table */
		soc = bd71827_voltage_to_capacity(relax_ocv);
		dev_dbg(pwr->mfd->dev,  "soc %d[0.1%%]\n", soc);
		if (soc < 0)
			soc = 0;

		bcap = wd->designed_cap * soc / 1000;
		bcap = (bcap + wd->designed_cap / 200);

		ret = update_cc(pwr, bcap);
		if (ret)
			return ret;

		dev_dbg(pwr->mfd->dev,
			"Adjust Coulomb Counter at Relaxed State\n");
		dev_dbg(pwr->mfd->dev, "CC_CCNTD = %d\n",
			wd->coulomb_cnt);
		dev_info(pwr->mfd->dev,"relaxed_ocv:%d, bcap:%d, soc:%d[0.1%%], coulomb_cnt:0x%x\n",
			relax_ocv, bcap, soc, wd->coulomb_cnt);

		/* If the following commented out code is enabled,
		 * the SOC is not clamped at the relax time.
		 */
		/* Reset SOCs */
		/* bd71827_calc_soc_org(pwr, wd); */
		/* wd->soc_norm = wd->soc_org; */
		/* wd->soc = wd->soc_norm; */
		/* wd->clamp_soc = wd->soc; */
	}

	return ret;
}

/* @brief reset coulomb counter values at full charged state
 * @param pwr power device
 * @return 0
 */
static int bd71827_reset_coulomb_count(struct bd71827_power *pwr,
				       struct bd7182x_soc_data *wd)
{
	u32 full_charged_coulomb_cnt;
	struct regmap *regmap = pwr->mfd->regmap;
	int ret;

	ret = read_cc_full(pwr, &full_charged_coulomb_cnt);
	if (ret) {
		dev_err(pwr->mfd->dev, "failed to read full coulomb counter\n");
		return ret;
	}

	dev_dbg(pwr->mfd->dev, "%s(): full_charged_coulomb_cnt=0x%x\n",
		__func__, full_charged_coulomb_cnt);
	if (full_charged_coulomb_cnt != 0) {
		int diff_coulomb_cnt;
		u32 cc;
		u16 bcap;

		/* Clear Full Charged Coulomb Counter */
		ret = regmap_update_bits(regmap, pwr->regs->cc_full_clr,
					 BD7182x_MASK_CC_FULL_CLR,
					 BD7182x_MASK_CC_FULL_CLR);

		ret = read_cc(pwr, &cc);
		if (ret)
			return ret;

		diff_coulomb_cnt = full_charged_coulomb_cnt - cc;

		diff_coulomb_cnt = diff_coulomb_cnt >> 16;
		if (diff_coulomb_cnt > 0)
			diff_coulomb_cnt = 0;

		dev_dbg(pwr->mfd->dev,  "diff_coulomb_cnt = %d\n",
			diff_coulomb_cnt);

		bcap = wd->designed_cap + wd->designed_cap / 200 +
		       diff_coulomb_cnt;
		ret = update_cc(pwr, bcap);
		if (ret)
			return ret;
		dev_dbg(pwr->mfd->dev,
			"Reset Coulomb Counter at POWER_SUPPLY_STATUS_FULL\n");
		dev_dbg(pwr->mfd->dev, "CC_CCNTD = %d\n", wd->coulomb_cnt);
		dev_info(pwr->mfd->dev,"%s -- set cc to 0x%x (%d mAh)", __func__,wd->coulomb_cnt, A10s_mAh(bcap));
	}

	return 0;
}

/* @brief get battery voltage and current only, this won't read/reset vsys_min.
 * @param pwr power device
 * @return 0 if success
 */
static int bd71827_get_voltage_current_directly(struct bd71827_power *pwr,
				       struct bd7182x_soc_data *wd)
{
	int ret;
	int temp, temp2;

	if (pwr->mfd->chip_type != ROHM_CHIP_TYPE_BD71828 &&
	    pwr->mfd->chip_type != ROHM_CHIP_TYPE_BD71827) {
		return -EINVAL;
	}

	ret = bd71827_get_vbat(pwr, &temp);
	if (ret)
		return ret;

	wd->vcell = temp;
	ret = bd71827_get_current_ds_adc(pwr, &temp, &temp2);
	if (ret)
		return ret;
	wd->curr_avg = temp2;
	wd->curr = temp;

	return 0;
}

/* @brief get battery parameters, such as voltages, currents, temperatures.
 * @param pwr power device
 * @return 0
 */
static int bd71827_get_voltage_current(struct bd71827_power *pwr,
				       struct bd7182x_soc_data *wd)
{
	int ret;
	int temp, temp2;

	if (pwr->mfd->chip_type != ROHM_CHIP_TYPE_BD71828 &&
	    pwr->mfd->chip_type != ROHM_CHIP_TYPE_BD71827) {
		return -EINVAL;
	}

	ret = bd71827_get_vbat(pwr, &temp);
	if (ret)
		return ret;

	wd->vcell = temp;
	ret = bd71827_get_current_ds_adc(pwr, &temp, &temp2);
	if (ret)
		return ret;
	wd->curr_avg = temp2;
	wd->curr = temp;

	/* Read detailed vsys */
	ret = bd71827_get_vsys(pwr, &temp);
	if (ret)
		return ret;

	wd->vsys = temp;
	dev_dbg(pwr->mfd->dev,  "VM_VSYS = %d\n", temp);

	/* Read detailed vbat_min */
	ret = bd71827_get_vbat_min(pwr, &temp);
	if (ret)
		return ret;
	wd->vcell_min = temp;
	dev_dbg(pwr->mfd->dev,  "VM_VBAT_MIN = %d\n", temp);

	/* Read detailed vsys_min */
	ret = bd71827_get_vsys_min(pwr, &temp);
	if (ret)
		return ret;

	wd->vsys_min = temp;
	dev_dbg(pwr->mfd->dev,  "VM_VSYS_MIN = %d\n", temp);

	/* Get tempature */
	ret = pwr->get_temp(pwr, &temp);

	if (ret)
		return ret;

	wd->temp = temp;

	return 0;
}

/* @brief adjust coulomb counter values at relaxed state by SW
 * @param pwr power device
 * @return 0
 */

static int bd71827_adjust_coulomb_count_sw(struct bd71827_power *pwr,
					   struct bd7182x_soc_data *wd)
{
	int tmp_curr_mA, ret;

	tmp_curr_mA = uAMP_TO_mAMP(wd->curr);
	if ((tmp_curr_mA * tmp_curr_mA) <=
	    (THR_RELAX_CURRENT_DEFAULT * THR_RELAX_CURRENT_DEFAULT))
		 /* No load */
		wd->relax_time = wd->relax_time + (JITTER_DEFAULT / 1000);
	else {
		if(wd->relax_time > 0) {
			dev_info(pwr->mfd->dev,"%s(): pwr->relax_time reset to 0 (was %d seconds), curr:%d mA\n", __func__,
					wd->relax_time, tmp_curr_mA);
		}
		wd->relax_time = 0;
	}

	dev_dbg(pwr->mfd->dev,  "%s(): pwr->relax_time = 0x%x\n", __func__,
		wd->relax_time);
	if (wd->relax_time >= THR_RELAX_TIME_DEFAULT) {
		/* Battery is relaxed. */
		u32 bcap;
		int soc, ocv;

		wd->relax_time = 0;

		/* Get OCV */
		ocv = wd->vcell;

		/* Get soc at relaxed state from ocv/soc table */
		soc = bd71827_voltage_to_capacity(ocv);
		dev_dbg(pwr->mfd->dev,  "soc %d[0.1%%]\n", soc);
		if (soc < 0)
			soc = 0;

		dev_info(pwr->mfd->dev, "%s adjust CC to %d[0.1%%], ocv:%d\n",__func__, soc, ocv);
		bcap = wd->designed_cap * soc / 1000;

		ret = update_cc(pwr, bcap + wd->designed_cap / 200);
		if (ret)
			return ret;

		dev_dbg(pwr->mfd->dev,
			"Adjust Coulomb Counter by SW at Relaxed State\n");
		dev_dbg(pwr->mfd->dev, "CC_CCNTD = %d\n", wd->coulomb_cnt);

		/* If the following commented out code is enabled,
		 * the SOC is not clamped at the relax time.
		 */
		/* Reset SOCs */
		/* bd71827_calc_soc_org(pwr, wd); */
		/* wd->soc_norm = wd->soc_org; */
		/* wd->soc = wd->soc_norm; */
		/* wd->clamp_soc = wd->soc; */
	}

	return 0;
}

/* @brief get coulomb counter values
 * @param pwr power device
 * @return 0
 */
static int bd71827_coulomb_count(struct bd71827_power *pwr,
				 struct bd7182x_soc_data *wd)
{
	int ret = 0;

	dev_dbg(pwr->mfd->dev, "%s(): pwr->state_machine = 0x%x\n", __func__,
		wd->state_machine);
	if (wd->state_machine == STAT_POWER_ON) {
		wd->state_machine = STAT_INITIALIZED;
		/* Start Coulomb Counter */
		ret = start_cc(pwr);
	} else if (wd->state_machine == STAT_INITIALIZED) {
		u32 cc = 0;

		ret = read_cc(pwr, &cc);
		wd->coulomb_cnt = cc;
	}
	return ret;
}

/* @brief calc cycle
 * @param pwr power device
 * @return 0
 */
static int bd71827_update_cycle(struct bd71827_power *pwr,
				struct bd7182x_soc_data *wd)
{
	int tmpret, ret;
	u16 charged_coulomb_cnt;

	ret = bd7182x_read16_himask(pwr, pwr->regs->coulomb_chg3, 0xff,
				    &charged_coulomb_cnt);
	if (ret) {
		dev_err(pwr->mfd->dev, "Failed to read charging CC (%d)\n",
			ret);
		return ret;
	}

	dev_dbg(pwr->mfd->dev, "%s(): charged_coulomb_cnt = 0x%x\n", __func__,
		(int)charged_coulomb_cnt);
	if (charged_coulomb_cnt >= wd->designed_cap) {
		wd->cycle++;
		dev_dbg(pwr->mfd->dev,  "Update cycle = %d\n", wd->cycle);
		battery_cycle = wd->cycle;
		charged_coulomb_cnt -= wd->designed_cap;

		ret = stop_cc(pwr);
		if (ret)
			return ret;

		ret = bd7182x_write16(pwr, pwr->regs->coulomb_chg3,
				      charged_coulomb_cnt);
		if (ret) {
			dev_err(pwr->mfd->dev,
				"Failed to update charging CC (%d)\n", ret);
		}

		tmpret = start_cc(pwr);
		if (tmpret)
			return tmpret;
	}
	return ret;
}

/* @brief calc full capacity value by Cycle and Temperature
 * @param pwr power device
 * @return 0
 */
static int bd71827_calc_full_cap(struct bd71827_power *pwr,
				 struct bd7182x_soc_data *wd)
{
	u32 designed_cap_uAh;
	u32 full_cap_uAh;

	/* Calculate full capacity by cycle */
	designed_cap_uAh = A10s_mAh(wd->designed_cap) * 1000;

	if (dgrd_cyc_cap * wd->cycle >= designed_cap_uAh) {
		/* Battry end of life? */
		wd->full_cap = 1;
		return 0;
	}

	full_cap_uAh = designed_cap_uAh - dgrd_cyc_cap * wd->cycle;
	wd->full_cap = mAh_A10s(uAMP_TO_mAMP(full_cap_uAh));
	dev_dbg(pwr->mfd->dev,  "Calculate full capacity by cycle\n");
	dev_dbg(pwr->mfd->dev,  "%s() pwr->full_cap = %d\n", __func__,
		wd->full_cap);

	/* Calculate full capacity by temperature */
	dev_dbg(pwr->mfd->dev,  "Temperature = %d\n", wd->temp);
	if (wd->temp >= DGRD_TEMP_M_DEFAULT) {
		full_cap_uAh += (wd->temp - DGRD_TEMP_M_DEFAULT) *
				dgrd_temp_cap_h;
		wd->full_cap = mAh_A10s(uAMP_TO_mAMP(full_cap_uAh));
	} else if (wd->temp >= DGRD_TEMP_L_DEFAULT) {
		full_cap_uAh += (wd->temp - DGRD_TEMP_M_DEFAULT) *
				dgrd_temp_cap_m;
		wd->full_cap = mAh_A10s(uAMP_TO_mAMP(full_cap_uAh));
	} else {
		full_cap_uAh += (DGRD_TEMP_L_DEFAULT - DGRD_TEMP_M_DEFAULT) *
				dgrd_temp_cap_m;
		full_cap_uAh += (wd->temp - DGRD_TEMP_L_DEFAULT) *
				dgrd_temp_cap_l;
		wd->full_cap = mAh_A10s(uAMP_TO_mAMP(full_cap_uAh));
	}

	if (wd->full_cap < 1)
		wd->full_cap = 1;

	dev_dbg(pwr->mfd->dev,  "Calculate full capacity by cycle and temperature\n");
	dev_dbg(pwr->mfd->dev,  "%s() pwr->full_cap = %d\n", __func__,
		wd->full_cap);

	return 0;
}

/* @brief calculate SOC values by designed capacity
 * @param pwr power device
 * @return 0
 */
static unsigned int bd71827_calc_soc_org(u32 cc, int designed_cap)
{
	return (cc >> 16) * 100 / designed_cap;
}

/* @brief calculate SOC values by full capacity
 * @param pwr power device
 * @return 0
 */
static int bd71827_calc_soc_norm(struct bd71827_power *pwr,
				 struct bd7182x_soc_data *wd)
{
	int lost_cap;
	int mod_coulomb_cnt;

	lost_cap = wd->designed_cap - wd->full_cap;
	dev_dbg(pwr->mfd->dev,  "%s() lost_cap = %d\n", __func__, lost_cap);

	mod_coulomb_cnt = (wd->coulomb_cnt >> 16) - lost_cap;
	if ((mod_coulomb_cnt > 0) && (wd->full_cap > 0))
		wd->soc_norm = mod_coulomb_cnt * 100 / wd->full_cap;
	else
		wd->soc_norm = 0;

	if (wd->soc_norm > 100)
		wd->soc_norm = 100;

	dev_dbg(pwr->mfd->dev,  "%s() pwr->soc_norm = %d\n", __func__,
		wd->soc_norm);

	return 0;
}

/* @brief get OCV value by SOC
 * @param pwr power device
 * @return 0
 */
int bd71827_get_ocv(struct bd71827_power *pwr, int dsoc)
{
	int i = 0;
	int ocv = 0;

	if (dsoc > soc_table[0]) {
		ocv = MAX_VOLTAGE_DEFAULT;
	} else if (dsoc == 0) {
		ocv = ocv_table[21];
	} else {
		i = 0;
		while (i < 22) {
			if ((dsoc <= soc_table[i]) &&
					(dsoc > soc_table[i + 1])) {
				ocv = (ocv_table[i] - ocv_table[i + 1]) *
				      (dsoc - soc_table[i + 1]) /
				      (soc_table[i] - soc_table[i + 1]) +
				      ocv_table[i + 1];
				break;
			}
			i++;
		}
		if (i == 22)
			ocv = ocv_table[22];
	}
	dev_dbg(pwr->mfd->dev,  "%s() ocv = %d\n", __func__, ocv);
	return ocv;
}

static void calc_vdr(int *res, int *vdr, int temp, int dgrd_temp,
		     int *vdr_hi, int dgrd_temp_hi, int items)
{
	int i;

	for (i = 0; i < items; i++)
		res[i] = vdr[i] + (temp - dgrd_temp) * (vdr_hi[i] - vdr[i]) /
			 (dgrd_temp_hi - dgrd_temp);
}

/* @brief get VDR(Voltage Drop Rate) value by SOC
 * @param pwr power device
 * @return 0
 */
static int bd71827_get_vdr(struct bd71827_power *pwr, int dsoc,
			   struct bd7182x_soc_data *wd)
{
	int i = 0;
	int vdr = 100;
	int vdr_table[23] = {0};

	/* Calculate VDR by temperature */
	if (wd->temp >= DGRD_TEMP_H_DEFAULT)
		for (i = 0; i < 23; i++)
			vdr_table[i] = vdr_table_h[i];
	else if (wd->temp >= DGRD_TEMP_M_DEFAULT)
		calc_vdr(vdr_table, vdr_table_m, wd->temp, DGRD_TEMP_M_DEFAULT,
			 vdr_table_h, DGRD_TEMP_H_DEFAULT, 23);
	else if (wd->temp >= DGRD_TEMP_L_DEFAULT)
		calc_vdr(vdr_table, vdr_table_l, wd->temp, DGRD_TEMP_L_DEFAULT,
			 vdr_table_m, DGRD_TEMP_M_DEFAULT, 23);
	else if (wd->temp >= DGRD_TEMP_VL_DEFAULT)
		calc_vdr(vdr_table, vdr_table_vl, wd->temp,
			 DGRD_TEMP_VL_DEFAULT, vdr_table_l, DGRD_TEMP_L_DEFAULT,
			 23);
	else
		for (i = 0; i < 23; i++)
			vdr_table[i] = vdr_table_vl[i];

	if (dsoc > soc_table[0]) {
		vdr = 100;
	} else if (dsoc == 0) {
		vdr = vdr_table[21];
	} else {
		for (i = 0; i < 22; i++)
			if ((dsoc <= soc_table[i]) &&
				(dsoc > soc_table[i + 1])) {
				vdr = (vdr_table[i] - vdr_table[i + 1]) *
				      (dsoc - soc_table[i + 1]) /
				      (soc_table[i] - soc_table[i + 1]) +
				      vdr_table[i + 1];
				break;
			}
		if (i == 22)
			vdr = vdr_table[22];
	}
	dev_dbg(pwr->mfd->dev, "%s() vdr = %d\n", __func__, vdr);
	return vdr;
}

/* @brief calculate SOC value by full_capacity and load
 * @param pwr power device
 * @return OCV
 */

static void soc_not_charging(struct bd71827_power *pwr,
			    struct bd7182x_soc_data *wd)
{
	int ocv_table_load[23];
	int i;
	int ocv;
	int lost_cap;
	int mod_coulomb_cnt;
	int dsoc;

	lost_cap = wd->designed_cap - wd->full_cap;
	mod_coulomb_cnt = (wd->coulomb_cnt >> 16) - lost_cap;
	dsoc = mod_coulomb_cnt * 1000 /  wd->full_cap;
	dev_dbg(pwr->mfd->dev,  "%s() dsoc = %d\n", __func__,
		dsoc);

	ocv = bd71827_get_ocv(pwr, dsoc);
	for (i = 1; i < 23; i++) {
		ocv_table_load[i] = ocv_table[i] - (ocv - wd->vsys_min);
		if (ocv_table_load[i] <= min_voltage) {
			dev_dbg(pwr->mfd->dev,
				"%s() ocv_table_load[%d] = %d\n", __func__,
				i, ocv_table_load[i]);
			break;
		}
	}
	if (i < 23) {
		int j, k, m;
		int dv;
		int lost_cap2, new_lost_cap2;
		int mod_coulomb_cnt2, mod_full_cap;
		int dsoc0;
		int vdr, vdr0;

		dv = (ocv_table_load[i - 1] - ocv_table_load[i]) / 5;
		for (j = 1; j < 5; j++) {
			if ((ocv_table_load[i] + dv * j) >
			    min_voltage) {
				break;
			}
		}
		lost_cap2 = ((21 - i) * 5 + (j - 1)) * wd->full_cap / 100;
		dev_dbg(pwr->mfd->dev, "%s() lost_cap2-1 = %d\n", __func__,
			lost_cap2);
		for (m = 0; m < soc_est_max_num; m++) {
			new_lost_cap2 = lost_cap2;
			dsoc0 = lost_cap2 * 1000 / wd->full_cap;
			if ((dsoc >= 0 && dsoc0 > dsoc) ||
			    (dsoc < 0 && dsoc0 < dsoc))
				dsoc0 = dsoc;

			dev_dbg(pwr->mfd->dev, "%s() dsoc0(%d) = %d\n",
				__func__, m, dsoc0);

			vdr = bd71827_get_vdr(pwr, dsoc, wd);
			vdr0 = bd71827_get_vdr(pwr, dsoc0, wd);

			for (k = 1; k < 23; k++) {
				ocv_table_load[k] = ocv_table[k] -
						    (ocv - wd->vsys_min) * vdr0
						    / vdr;
				if (ocv_table_load[k] <= min_voltage) {
					dev_dbg(pwr->mfd->dev,
						"%s() ocv_table_load[%d] = %d\n",
						__func__, k, ocv_table_load[k]);
					break;
				}
			}
			if (k < 23) {
				dv = (ocv_table_load[k - 1] -
				     ocv_table_load[k]) / 5;
				for (j = 1; j < 5; j++)
					if ((ocv_table_load[k] + dv * j) >
					     min_voltage)
						break;

				new_lost_cap2 = ((21 - k) * 5 + (j - 1)) *
						wd->full_cap / 100;
				if (soc_est_max_num == 1)
					lost_cap2 = new_lost_cap2;
				else
					lost_cap2 +=
					(new_lost_cap2 - lost_cap2) /
					(2 * (soc_est_max_num - m));

				dev_dbg(pwr->mfd->dev,
					"%s() lost_cap2-2(%d) = %d\n", __func__,
					m, lost_cap2);
			}
			if (new_lost_cap2 == lost_cap2)
				break;
		}
		mod_coulomb_cnt2 = mod_coulomb_cnt - lost_cap2;
		mod_full_cap = wd->full_cap - lost_cap2;
		if ((mod_coulomb_cnt2 > 0) && (mod_full_cap > 0))
			wd->soc = mod_coulomb_cnt2 * 100 / mod_full_cap;
		else
			wd->soc = 0;

		dev_dbg(pwr->mfd->dev,  "%s() pwr->soc(by load) = %d\n",
			__func__, wd->soc);
	}
}

static int bd71827_calc_soc(struct bd71827_power *pwr,
			    struct bd7182x_soc_data *wd)
{
	wd->soc = wd->soc_norm;

	 /* Adjust for 0% between thr_voltage and min_voltage */
	switch (wd->rpt_status) {
	case POWER_SUPPLY_STATUS_DISCHARGING:
	case POWER_SUPPLY_STATUS_NOT_CHARGING:
		if (wd->vsys_min <= thr_voltage)
			soc_not_charging(pwr, wd);
		break;
	default:
		break;
	}

	switch (wd->rpt_status) {/* Adjust for 0% and 100% */
	case POWER_SUPPLY_STATUS_DISCHARGING:
	case POWER_SUPPLY_STATUS_NOT_CHARGING:
		if (wd->vsys_min <= min_voltage)
			wd->soc = 0;
		else if (wd->soc == 0)
			wd->soc = 1;
		break;
	case POWER_SUPPLY_STATUS_CHARGING:
		if (wd->soc == 100)
			wd->soc = 99;
		break;
	default:
		break;
	}
	dev_dbg(pwr->mfd->dev,  "%s() pwr->soc = %d\n", __func__, wd->soc);
	return 0;
}

/* @brief calculate Clamped SOC value by full_capacity and load
 * @param pwr power device
 * @return OCV
 */
static int bd71827_calc_soc_clamp(struct bd71827_power *pwr,
				  struct bd7182x_soc_data *wd)
{
	static u32 last_soc = 0;

	switch (wd->rpt_status) {/* Adjust for 0% and 100% */
	case POWER_SUPPLY_STATUS_DISCHARGING:
	case POWER_SUPPLY_STATUS_NOT_CHARGING:
		if (wd->soc <= wd->clamp_soc)
			wd->clamp_soc = wd->soc;
		break;
	default:
		wd->clamp_soc = wd->soc;
		break;
	}
	if(last_soc != wd->clamp_soc) {
		dev_info(pwr->mfd->dev,"clamp_soc = %d, vbat:%d\n", wd->clamp_soc, wd->vcell);
		last_soc = wd->clamp_soc;
	}
	return 0;
}

/* @brief get battery and DC online status
 * @param pwr power device
 * @return 0
 */
static int bd71827_get_online(struct bd71827_power *pwr,
			      struct bd7182x_soc_data *wd)
{
	int r, ret;

#if 0
#define TS_THRESHOLD_VOLT	0xD9
	r = bd71827_reg_read(pwr->mfd, BD71827_REG_VM_VTH);
	pwr->bat_online = (r > TS_THRESHOLD_VOLT);
#endif
#if 0
	r = bd71827_reg_read(pwr->mfd, BD71827_REG_BAT_STAT);
	if (r >= 0 && (r & BAT_DET_DONE))
		pwr->bat_online = (r & BAT_DET) != 0;
#endif
#if 1
#define BAT_OPEN	0x7
	ret = regmap_read(pwr->mfd->regmap, pwr->regs->bat_temp, &r);
	if (ret) {
		dev_err(pwr->mfd->dev, "Failed to read battery temperature\n");
		return ret;
	}
	wd->bat_online = ((r & BD7182x_MASK_BAT_TEMP) != BAT_OPEN);
#endif
	ret = regmap_read(pwr->mfd->regmap, pwr->regs->dcin_stat, &r);
	if (ret) {
		dev_err(pwr->mfd->dev, "Failed to read DCIN status\n");
		return ret;
	}
	wd->charger_online = ((r & BD7182x_MASK_DCIN_DET) != 0);

	//thermal charging limit only valid when charger is online
	if(wd->charger_online == 0 && wd->charge_control_limit != -1) {
		dev_info(pwr->mfd->dev,"%s: clear charge_control_limit (%d->-1) as charger is not online\n", __func__, wd->charge_control_limit);
		wd->charge_control_limit = -1;
	}


	ret = regmap_read(pwr->mfd->regmap, pwr->regs->ilim_dcin_stat, &r);
	if (ret) {
		dev_err(pwr->mfd->dev, "Failed to read ILIM_DCIN_STAT status\n");
		return ret;
	}
	wd->ilim_dcin_stat = r;
	if (wd->ilim_dcin_stat < wd->dcin_val){
		dev_info(pwr->mfd->dev,"!!!DCIN limited!!!, ILIM_DCIN_STAT:0x%x, DCIN:0x%x\n",
			 wd->ilim_dcin_stat, wd->dcin_val);
		if(!bd71828_in_shpm && !wd->ilim_dcin_recover_pending && wd->charge_control_limit == -1) {
			dev_info(pwr->mfd->dev, "%s: scheduling ilim_dcin_recover_work in %d seconds\n", __func__, ILIM_DCIN_RECOVER_DELAY/1000);
			if(!schedule_delayed_work(&pwr->bd_ilim_dcin_recover_work, msecs_to_jiffies(ILIM_DCIN_RECOVER_DELAY))) {
				cancel_delayed_work(&pwr->bd_ilim_dcin_recover_work);
				schedule_delayed_work(&pwr->bd_ilim_dcin_recover_work, msecs_to_jiffies(ILIM_DCIN_RECOVER_DELAY));
				wd->ilim_dcin_recover_pending = 1;
			}
		}
	} else {
		if(wd->ilim_dcin_recover_pending){
			dev_info(pwr->mfd->dev, "%s: DCIN no longer limited, canceling recover work\n", __func__);
			cancel_delayed_work(&pwr->bd_ilim_dcin_recover_work);
		}
		wd->ilim_dcin_recover_pending = 0;
	}
	dev_dbg(pwr->mfd->dev,
		"%s(): pwr->bat_online = %d, pwr->charger_online = %d\n",
		__func__, wd->bat_online, wd->charger_online);

	return 0;
}

/* @brief init bd71827 sub module charger
 * @param pwr power device
 * @return 0
 */
static int bd71827_init_hardware(struct bd71827_power *pwr,
				 struct bd7182x_soc_data *wd)
{
	int r, temp, ret;
	u32 cc, sorg;

	ret = regmap_write(pwr->mfd->regmap, pwr->regs->dcin_collapse_limit,
			   BD7182x_DCIN_COLLAPSE_DEFAULT);
	if (ret) {
		dev_err(pwr->mfd->dev, "Failed to write DCIN collapse limit\n");
		return ret;
	}

	ret = regmap_read(pwr->mfd->regmap, pwr->regs->conf, &r);
	if (ret) {
		dev_err(pwr->mfd->dev, "Failed to read CONF register\n");
		return ret;
	}

	/* Always set default Battery Capacity ? */
	wd->designed_cap = battery_cap;
	wd->full_cap = battery_cap;
	/* Why BD71827_REG_CC_BATCAP_U is not used? */
	// bd71827_reg_read16(pwr->mfd, BD71827_REG_CC_BATCAP_U);

	if (r & BD7182x_MASK_CONF_PON) {
		/* Init HW, when the battery is inserted. */

		ret = regmap_update_bits(pwr->mfd->regmap, pwr->regs->conf,
					 BD7182x_MASK_CONF_PON, 0);
		if (ret) {
			dev_err(pwr->mfd->dev, "Failed to clear CONF register\n");
			return ret;
		}

		/* Stop Coulomb Counter */
		ret = stop_cc(pwr);
		if (ret)
			return ret;

		/* Set Coulomb Counter Reset bit*/
		ret = regmap_update_bits(pwr->mfd->regmap,
					 pwr->regs->coulomb_ctrl,
					 BD7182x_MASK_CCNTRST,
					 BD7182x_MASK_CCNTRST);
		if (ret)
			return ret;

		/* Clear Coulomb Counter Reset bit*/
		ret = regmap_update_bits(pwr->mfd->regmap,
					 pwr->regs->coulomb_ctrl,
					 BD7182x_MASK_CCNTRST, 0);
		if (ret)
			return ret;

		/* Clear Relaxed Coulomb Counter */
		ret = regmap_update_bits(pwr->mfd->regmap,
					 pwr->regs->rex_clear_reg,
					 pwr->regs->rex_clear_mask,
					 pwr->regs->rex_clear_mask);

		/* Set initial Coulomb Counter by HW OCV */
		calibration_coulomb_counter(pwr, wd);


		/* VBAT Low voltage detection Setting, added by John Zhang*/
		/* bd71827_reg_write16(mfd,
		 * BD71827_REG_ALM_VBAT_TH_U, VBAT_LOW_TH);
		 */

		ret = bd7182x_write16(pwr, pwr->regs->vbat_alm_limit_u,
				      VBAT_LOW_TH);
		if (ret)
			return ret;

		/* Set Battery Capacity Monitor threshold1 as 95% */
		dev_info(pwr->mfd->dev,"BD71827_REG_CC_BATCAP1_TH = %d\n",
			(battery_cap * 95 / 100));
		ret = bd7182x_write16(pwr, pwr->regs->batcap_mon_limit_u,
				      battery_cap * 95 / 100);
		if (ret)
			return ret;

		/* Enable LED ON when charging
		 * Should we do this decision here? Should the enabling be
		 * in LED driver and come from DT?
		 * bd71827_set_bits(pwr->mfd,
		 * BD71827_REG_LED_CTRL, CHGDONE_LED_EN);
		 */
		wd->state_machine = STAT_POWER_ON;
	} else {
		wd->state_machine = STAT_INITIALIZED;	// STAT_INITIALIZED
	}

	ret = pwr->get_temp(pwr, &temp);
	if (ret)
		return ret;

	wd->temp = temp;
	dev_dbg(pwr->mfd->dev,  "Temperature = %d\n", wd->temp);
	bd71827_reset_coulomb_count(pwr, wd);
	bd71827_adjust_coulomb_count(pwr, wd);
	ret = read_cc(pwr, &cc);
	if (ret)
		return ret;

	/* clear CHGRST set, in case if system previously shutdown when CHGRST bit is set */
	ret = regmap_update_bits(pwr->mfd->regmap, pwr->regs->chg_init,
				 BD7182x_MASK_CHG_INIT_CHGRST,
				 0);
	if (ret)
		return ret;

	dev_info(pwr->mfd->dev,"set vsys_min to 3.0v(0x%x)\n",BD71828_VSYS_MIN_3_0V);
	/*Set VSYS_MIN to 3.0v on system reset/power up */
	ret = regmap_write(pwr->mfd->regmap, pwr->regs->vsys_min, BD71828_VSYS_MIN_3_0V);
	if (ret)
		return ret;

	/*Set recharge threshold VBAT_MNT to VBAT_CHG1/2/3 - 0.1v on system reset/power up */
	ret = regmap_update_bits(pwr->mfd->regmap, pwr->regs->bat_set2, BD71828_MASK_BAT_SET_2_VBAT_MNT, BD71828_BAT_SET_2_VBAT_MNT_010);
	if (ret)
		return ret;

	/*Set REX_CTRL to use PMIC STATE as REX condition*/
	ret = regmap_update_bits(pwr->mfd->regmap, pwr->regs->rex_ctrl, BD71828_MASK_REX_CTRL_PMU_STATE, 0);
	if (ret)
		return ret;

	/*Set REX_CURCD_TH to 0 to use PMIC STATE for REX condition*/
	ret = regmap_write(pwr->mfd->regmap, pwr->regs->rex_curcd_th, 0);
	if (ret)
		return ret;


	/* Set Battery Capacity Monitor threshold1 as 95% */
	dev_info(pwr->mfd->dev,"BD71827_REG_CC_BATCAP1_TH = %d\n",
			(battery_cap * 95 / 100));
	ret = bd7182x_write16(pwr, pwr->regs->batcap_mon_limit_u,
				      battery_cap * 95 / 100);
	if (ret)
		return ret;

	bd71827_safe_charging_control(pwr, false);

	wd->coulomb_cnt = cc;
	/* If we boot up with CC stopped and both REX and FULL CC being 0
	 * - then the bd71827_adjust_coulomb_count and
	 * bd71827_reset_coulomb_count wont start CC. Just start CC here for
	 * now to mimic old operation where bd71827_calc_soc_org did
	 * always stop and start cc.
	 */
	start_cc(pwr);
	sorg = bd71827_calc_soc_org(wd->coulomb_cnt, wd->designed_cap);
	if (sorg > 100)
		limit_cc(pwr, wd, &sorg);

	wd->soc_norm = sorg;
	wd->soc = wd->soc_norm;
	wd->clamp_soc = wd->soc;
	dev_dbg(pwr->mfd->dev,  "%s() CC_CCNTD = %d\n",
		__func__, wd->coulomb_cnt);
	dev_dbg(pwr->mfd->dev,  "%s() pwr->soc = %d\n", __func__, wd->soc);
	dev_dbg(pwr->mfd->dev,  "%s() pwr->clamp_soc = %d\n",
		__func__, wd->clamp_soc);

	wd->cycle = battery_cycle;
	wd->curr = 0;
	wd->relax_time = 0;

	wd->chg_dcin_cur_limit = -1;		/* charger DCIN current limit */
	wd->charge_control_limit = -1;		/* charge control limit */
	return 0;
}

/* @brief set bd71827 battery parameters for ATL battery pack (15KHOM)
 * @param limited, indicate to use the battery parameter with limited condition (stressed, or safe charge)
 */
static void bd71827_load_battery_parameter_ATL(int limited)
{
	int i=0;
	if( !limited ){
		max_voltage = 4400000;
		min_voltage = 3500000;
		thr_voltage = 4350000;
		battery_cap_mah = 1706;
		dgrd_cyc_cap = 135;
		soc_est_max_num = 5;
		dgrd_temp_cap_h = (0);
		dgrd_temp_cap_m = (0);
		dgrd_temp_cap_l = (0);

		for (i = 0; i < 23; i++) {
			ocv_table[i] = ocv_table_15KOHM[i];
			vdr_table_h[i] = vdr_table_h_15KOHM[i];
			vdr_table_m[i] = vdr_table_m_15KOHM[i];
			vdr_table_l[i] = vdr_table_l_15KOHM[i];
			vdr_table_vl[i] = vdr_table_vl_15KOHM[i];
		}
	} else {
		max_voltage = 4200000;
		min_voltage = 3500000;
		thr_voltage = 4150000;
		battery_cap_mah = 1424;
		dgrd_cyc_cap = 177;
		soc_est_max_num = 5;
		dgrd_temp_cap_h = (0);
		dgrd_temp_cap_m = (0);
		dgrd_temp_cap_l = (0);

		for (i = 0; i < 23; i++) {
			ocv_table[i] = ocv_table_15KOHM_4p2v[i];
			vdr_table_h[i] = vdr_table_h_15KOHM_4p2v[i];
			vdr_table_m[i] = vdr_table_m_15KOHM_4p2v[i];
			vdr_table_l[i] = vdr_table_l_15KOHM_4p2v[i];
			vdr_table_vl[i] = vdr_table_vl_15KOHM_4p2v[i];
		}
	}
	return;
}

/* @brief set bd71827 battery parameters for MURATA battery pack (22KHOM)
 * @param limited, indicate to use the battery parameter with limited condition (stressed, or safe charge)
 */
static void bd71827_load_battery_parameter_MURATA(int limited)
{
	int i=0;
	if( !limited ){
		max_voltage = 4400000;
		min_voltage = 3500000;
		thr_voltage = 4350000;
		battery_cap_mah = 1665;
		dgrd_cyc_cap = 72;
		soc_est_max_num = 5;
		dgrd_temp_cap_h = (0);
		dgrd_temp_cap_m = (0);
		dgrd_temp_cap_l = (0);

		for (i = 0; i < 23; i++) {
			ocv_table[i] = ocv_table_22KOHM[i];
			vdr_table_h[i] = vdr_table_h_22KOHM[i];
			vdr_table_m[i] = vdr_table_m_22KOHM[i];
			vdr_table_l[i] = vdr_table_l_22KOHM[i];
			vdr_table_vl[i] = vdr_table_vl_22KOHM[i];
		}
	} else {
		max_voltage = 4200000;
		min_voltage = 3500000;
		thr_voltage = 4150000;
		battery_cap_mah = 1408;
		dgrd_cyc_cap = 30;
		soc_est_max_num = 5;
		dgrd_temp_cap_h = (0);
		dgrd_temp_cap_m = (0);
		dgrd_temp_cap_l = (0);

		for (i = 0; i < 23; i++) {
			ocv_table[i] = ocv_table_22KOHM_4p2v[i];
			vdr_table_h[i] = vdr_table_h_22KOHM_4p2v[i];
			vdr_table_m[i] = vdr_table_m_22KOHM_4p2v[i];
			vdr_table_l[i] = vdr_table_l_22KOHM_4p2v[i];
			vdr_table_vl[i] = vdr_table_vl_22KOHM_4p2v[i];
		}
	}
	return;
}
/* @brief set bd71827 battery parameters for LIWINON battery pack (29.4KHOM)
 * @param limited, indicate to use the battery parameter with limited condition (stressed, or safe charge)
 */
static void bd71827_load_battery_parameter_LIWINON(int limited)
{
	return;
}
/* @brief set bd71827 battery parameters for LIWINON battery pack (29.4KHOM)
 * @param limited, indicate to use the battery parameter with limited condition (stressed, or safe charge)
 * shall never occur...
 */
static void bd71827_load_battery_parameter_DEFAULT(int limited)
{
	int i=0;
	max_voltage = MAX_VOLTAGE_DEFAULT;
	min_voltage = MIN_VOLTAGE_DEFAULT;
	thr_voltage = THR_VOLTAGE_DEFAULT;
	battery_cap_mah = BATTERY_CAP_MAH_DEFAULT;
	dgrd_cyc_cap = DGRD_CYC_CAP_DEFAULT;
	soc_est_max_num = SOC_EST_MAX_NUM_DEFAULT;
	dgrd_temp_cap_h = DGRD_TEMP_CAP_H_DEFAULT;
	dgrd_temp_cap_m = DGRD_TEMP_CAP_M_DEFAULT;
	dgrd_temp_cap_l = DGRD_TEMP_CAP_L_DEFAULT;
	for (i = 0; i < 23; i++) {
		ocv_table[i] = ocv_table_default[i];
		vdr_table_h[i] = vdr_table_h_default[i];
		vdr_table_m[i] = vdr_table_m_default[i];
		vdr_table_l[i] = vdr_table_l_default[i];
		vdr_table_vl[i] = vdr_table_vl_default[i];
	}
	return;
}

/* @brief set bd71827 battery parameters
 * @param pwr power device
 * @param limited, indicate to use the battery parameter with limited condition (stressed, or safe charge)
 * @return 0
 */
static int bd71827_set_battery_parameters(struct bd71827_power *pwr, int limited)
{
	int i = 0, ret = 0, batId = 0;

	if (idme_hwid_value  < 3) {
		use_load_bat_params = USE_BAT_PARAMS_22KOHM_MURATA;
		dev_info(pwr->mfd->dev,"%s detect hwid:%d, skipping battID and use battType:%d\n",__func__, idme_hwid_value, use_load_bat_params);
	}
	else {
		ret = regmap_read(pwr->mfd->regmap, pwr->regs->bat_id, &batId);
		if (ret)
			goto Error;
		if( batId >= BAT_ID_15K_RANGE_LOW && batId <= BAT_ID_15K_RANGE_HIGH ) {
			use_load_bat_params = USE_BAT_PARAMS_15KOHM_ATL;
			ret = regmap_update_bits(pwr->mfd->regmap, pwr->regs->chg_en,
				BD7182x_MASK_CHG_EN, BD7182x_MASK_CHG_EN);
			if (ret)
				goto Error;
		}
		else if( batId >= BAT_ID_22K_RANGE_LOW && batId <= BAT_ID_22K_RANGE_HIGH ) {
			use_load_bat_params = USE_BAT_PARAMS_22KOHM_MURATA;
			ret = regmap_update_bits(pwr->mfd->regmap, pwr->regs->chg_en,
				BD7182x_MASK_CHG_EN, BD7182x_MASK_CHG_EN);
			if (ret)
				goto Error;
		}
		else if( batId >= BAT_ID_29K_RANGE_LOW && batId <= BAT_ID_29K_RANGE_HIGH ) {
			dev_info(pwr->mfd->dev, "%s detect ID-29KOHM, use 22KOHM params for now", __func__);
			use_load_bat_params = USE_BAT_PARAMS_22KOHM_MURATA /*!!!Todo!!! correct when 29K parameters are ready*/;
			ret = regmap_update_bits(pwr->mfd->regmap, pwr->regs->chg_en,
				BD7182x_MASK_CHG_EN, BD7182x_MASK_CHG_EN);
			if (ret)
				goto Error;
		}
		else {
			pr_err("%s !!!unknown battID:0x%x detected, disabling charging!!!\n",__func__, batId);
			ret = regmap_update_bits(pwr->mfd->regmap, pwr->regs->chg_en,
				 BD7182x_MASK_CHG_EN, 0);
			if (ret)
				goto Error;
		}
	}
	dev_info(pwr->mfd->dev,"%s found battType:%d, battID:0x%x\n",__func__, use_load_bat_params, batId);

	if (use_load_bat_params == USE_BAT_PARAMS_15KOHM_ATL) {
		dev_info(pwr->mfd->dev,"Set battery parameters for ID-15KOHM, limited:%d\n", limited);
		bd71827_load_battery_parameter_ATL(limited);
	}
	else if (use_load_bat_params == USE_BAT_PARAMS_22KOHM_MURATA) {
		dev_info(pwr->mfd->dev,"Set battery parameters for ID-22KOHM, limited:%d\n", limited);
		bd71827_load_battery_parameter_MURATA(limited);
	}
	else if (use_load_bat_params == USE_BAT_PARAMS_29KOHM_LIWINON) {
		dev_info(pwr->mfd->dev,"Set battery parameters for ID-29KOHM, limited:%d\n", limited);
		dev_info(pwr->mfd->dev,"!!!--TODO--LIWINON battery parameter not available yet!!!");
		bd71827_load_battery_parameter_LIWINON(limited);
	}
	else { /*Did not find right BATID, use default*/
		dev_info(pwr->mfd->dev,"No valid Battery ID found, use default battery parameters\n");
		bd71827_load_battery_parameter_DEFAULT(limited);
	}
	for (i = 0; i < 23; i++)
		soc_table[i] = soc_table_default[i];

	battery_cap = mAh_A10s(battery_cap_mah);
	dev_info(pwr->mfd->dev, "battery_cap updated to: %d mAh.\n",battery_cap_mah);
	ret = bd7182x_write16(pwr, pwr->regs->batcap_mon_limit_u,
			      battery_cap * 95 / 100);
	if(ret)
		dev_err(pwr->mfd->dev, "Failed to update batcap register: ret:%d.\n",ret);
	pwr->d_w.designed_cap = battery_cap;
	pwr->d_w.full_cap = battery_cap;
	smp_wmb(); /* wait for sync */
	return 0;

Error:
	pr_err("%s Error updating PMIC registers\n", __func__);
	return ret;

}

static void update_soc_data(struct bd71827_power *pwr)
{
	spin_lock(&pwr->dlock);
	pwr->d_r = pwr->d_w;
	spin_unlock(&pwr->dlock);
}

/*
 * Post a low battery or a critical battery event to the userspace
 */
void send_lobat_event(struct device  *dev, int crit_level)
{
	printk(KERN_ERR "%s lobat_event_sent %d critbat_event_sent %d",
		__func__, lobat_event_sent, critbat_event_sent);
	if (!crit_level) {
		if (!lobat_event_sent) {
			char *envp[] = { "BATTERY=low", NULL };
			printk(KERN_CRIT "KERNEL: I pmic:fg battery valrtmin::lowbat event\n");
			kobject_uevent_env(&(dev->kobj), KOBJ_CHANGE, envp);
			lobat_event_sent = 1;
		}
	} else {
		if (!critbat_event_sent) {
			char *envp[] = { "BATTERY=critical", NULL };
			printk(KERN_CRIT "KERNEL: I pmic:fg battery mbattlow::critbat event\n");
			kobject_uevent_env(&(dev->kobj), KOBJ_CHANGE, envp);
			critbat_event_sent = 1;
		}
	}
}

static void pmic_lobat_check_work(struct work_struct *work)
{
	struct bd71827_power *pwr;
	struct delayed_work *delayed_work;
	struct bd7182x_soc_data *wd;
	struct bd7182x_soc_data *wr;

	static int low_bat_count = 0;
	int ret = 0, charger_online = 0;


	delayed_work = container_of(work, struct delayed_work, work);
	pwr = container_of(delayed_work, struct bd71827_power, bd_lobat_check_work);
	wd = &pwr->d_w;
	wr = &pwr->d_r;
	if(bd71827_get_voltage_current_directly(pwr, wd))
		dev_err(pwr->mfd->dev, "Failed to get voltage/current directly.\n");
	update_soc_data(pwr);
	ret = regmap_read(pwr->mfd->regmap, pwr->regs->dcin_stat, &charger_online);
	if (ret) {
		dev_err(pwr->mfd->dev, "%s, Failed to read DCIN status, ret:%d\n", __func__, ret);
	}
	if(charger_online & BD7182x_MASK_DCIN_DET){
		dev_info(pwr->mfd->dev,"%s skipped as charger_online:%d\n",__func__, charger_online);
		low_bat_count = 0;
		return;
	}

	dev_info(pwr->mfd->dev,"%s vcell: %d, lo_bat_count%d\n",__func__, wr->vcell, low_bat_count);
	if ( wr->vcell/1000 <= SYS_CRIT_VOLT_THRESH)
		send_lobat_event(pwr->mfd->dev, CRIT_BATT_VOLT_LEVEL);
	else if (wr->vcell/1000 <= SYS_LOW_VOLT_THRESH) {
		low_bat_count++;
		if(low_bat_count > FG_LOW_BATT_CT)
			send_lobat_event(pwr->mfd->dev, LOW_BATT_VOLT_LEVEL);
		else
			if(!schedule_delayed_work(&pwr->bd_lobat_check_work, msecs_to_jiffies(LOW_BATT_CHECK_DELAY))) {
				cancel_delayed_work(&pwr->bd_lobat_check_work);
				schedule_delayed_work(&pwr->bd_lobat_check_work, msecs_to_jiffies(LOW_BATT_CHECK_DELAY));
			}
	}
	else
		low_bat_count = 0;
}

static void bd_ilim_dcin_recover_work_callback(struct work_struct *work)
{
	struct bd71827_power *pwr;
	struct delayed_work *delayed_work;

	delayed_work = container_of(work, struct delayed_work, work);
	pwr = container_of(delayed_work, struct bd71827_power, bd_ilim_dcin_recover_work);

	dev_info(pwr->mfd->dev, "%s\n", __func__);
	if(!schedule_delayed_work(&pwr->bd_reset_chgint_work, msecs_to_jiffies(0))) {
		cancel_delayed_work(&pwr->bd_reset_chgint_work);
		schedule_delayed_work(&pwr->bd_reset_chgint_work, msecs_to_jiffies(0));
	}
}

/* @brief timed work function called by system
 *  read battery capacity,
 *  sense change of charge status, etc.
 * @param work work struct
 * @return  void
 */
static void bd_work_callback(struct work_struct *work)
{
	struct bd71827_power *pwr;
	struct delayed_work *delayed_work;
	int status, changed = 0, ret = 0;
	unsigned int sorg;
	static int cap_counter;
	const char *errstr = "bd71827 in suspend";
	struct bd7182x_soc_data *wd;

	delayed_work = container_of(work, struct delayed_work, work);
	pwr = container_of(delayed_work, struct bd71827_power, bd_work);
	wd = &pwr->d_w;

	dev_dbg(pwr->mfd->dev, "%s(): in\n", __func__);
	if(bd71827_suspend_status == BD71827_SUSPEND)
		goto err_out;

	errstr = "DCIN status reading failed";
	ret = regmap_read(pwr->mfd->regmap, pwr->regs->dcin_stat, &status);
	if (ret)
		goto err_out;

	status &= BD7182x_MASK_DCIN_STAT;
	if (status != wd->vbus_status) {
		dev_dbg(pwr->mfd->dev, "DCIN_STAT CHANGED from 0x%X to 0x%X\n",
			wd->vbus_status, status);

		wd->vbus_status = status;
		changed = 1;
	}

	ret = regmap_read(pwr->mfd->regmap, pwr->regs->bat_stat, &status);

	errstr = "battery status reading failed";
	if (ret)
		goto err_out;

	status &= BD7182x_MASK_BAT_STAT;
	status &= ~BAT_DET_DONE;
	if (status != wd->bat_status) {
		dev_dbg(pwr->mfd->dev, "BAT_STAT CHANGED from 0x%X to 0x%X\n",
			wd->bat_status, status);
		wd->bat_status = status;
		changed = 1;
	}

	ret = regmap_read(pwr->mfd->regmap, pwr->regs->chg_state, &status);
	errstr = "Charger state reading failed";
	if (ret)
		goto err_out;

	status &= BD7182x_MASK_CHG_STATE;

	if (status != wd->charge_status) {
		dev_dbg(pwr->mfd->dev, "CHG_STATE CHANGED from 0x%X to 0x%X\n",
			wd->charge_status, status);
		wd->charge_status = status;
	}
	ret = bd71827_get_voltage_current(pwr, wd);
	errstr = "Failed to get current voltage";
	if (ret)
		goto err_out;

	errstr = "Failed to reset coulomb count";
	ret = bd71827_reset_coulomb_count(pwr, wd);
	if (ret)
		goto err_out;

	errstr = "Failed to adjust coulomb count";
	ret = bd71827_adjust_coulomb_count(pwr, wd);
	if (ret)
		goto err_out;

	errstr = "Failed to adjust coulomb count (sw)";
	ret = bd71827_adjust_coulomb_count_sw(pwr, wd);
	if (ret)
		goto err_out;

	errstr = "Failed to get coulomb count";
	ret = bd71827_coulomb_count(pwr, wd);
	if (ret)
		goto err_out;

	errstr = "Failed to perform update cycle";
	ret = bd71827_update_cycle(pwr, wd);
	if (ret)
		goto err_out;

	errstr = "Failed to calculate full capacity";
	ret = bd71827_calc_full_cap(pwr, wd);
	if (ret)
		goto err_out;

	errstr = "Failed to calculate org state of charge";
	sorg = bd71827_calc_soc_org(wd->coulomb_cnt, wd->designed_cap);
	if (sorg > 100)
		ret = limit_cc(pwr, wd, &sorg);
	if (ret)
		goto err_out;

	errstr = "Failed to calculate norm state of charge";
	ret = bd71827_calc_soc_norm(pwr, wd);
	if (ret)
		goto err_out;

	errstr = "Failed to calculate state of charge";
	ret = bd71827_calc_soc(pwr, wd);
	if (ret)
		goto err_out;

	errstr = "Failed to calculate clamped state of charge";
	ret = bd71827_calc_soc_clamp(pwr, wd);
	if (ret)
		goto err_out;

	errstr = "Failed to get charger online status";
	ret = bd71827_get_online(pwr, wd);
	if (ret)
		goto err_out;

	errstr = "Failed to get charger state";
	ret = bd71827_charge_status(pwr, wd);
	if (ret)
		goto err_out;


	update_soc_data(pwr);
	if (changed) {
		power_supply_changed(pwr->ac);
		power_supply_changed(pwr->bat);
	}
	if( cap_counter++ >= JITTER_REPORT_CAP / JITTER_DEFAULT-1) {
		dev_info(pwr->mfd->dev,"clamp_soc:%d, curr:%d, vbat:%d, vsys:%d, vsys_min:%d, cc:0x%x, stressed/safe_charging:%d/%d, cycle:%d, vbus/batt/chg status:0x%x/0x%x/0x%x\n",
			pwr->d_r.clamp_soc, pwr->d_r.curr, pwr->d_r.vcell, pwr->d_r.vsys, pwr->d_r.vsys_min, pwr->d_r.coulomb_cnt, battery_stressed, battery_safe_charging, battery_cycle, pwr->d_r.vbus_status, pwr->d_r.bat_status, pwr->d_r.charge_status);
		cap_counter = 0;
	}

	pwr->gauge_delay = JITTER_DEFAULT;
	schedule_delayed_work(&pwr->bd_work,
			      msecs_to_jiffies(JITTER_DEFAULT));
        if ( pwr->d_r.vcell/1000 <= SYS_CRIT_VOLT_THRESH)  {
                send_lobat_event(pwr->mfd->dev, CRIT_BATT_VOLT_LEVEL);
        } else if (pwr->d_r.vcell/1000 <= SYS_LOW_VOLT_THRESH) {
		if(!schedule_delayed_work(&pwr->bd_lobat_check_work, msecs_to_jiffies(LOW_BATT_CHECK_DELAY))) {
			cancel_delayed_work(&pwr->bd_lobat_check_work);
			schedule_delayed_work(&pwr->bd_lobat_check_work, msecs_to_jiffies(LOW_BATT_CHECK_DELAY));
		}
	}
	return;
err_out:
	dev_err(pwr->mfd->dev, "fuel-gauge cycle error %d - %s\n", ret,
		(errstr) ? errstr : "Unknown error");
	schedule_delayed_work(&pwr->bd_work,
			      msecs_to_jiffies(JITTER_DEFAULT));
}

/* @brief get property of power supply ac
 * @param psy power supply device
 * @param psp property to get
 * @param val property value to return
 * @retval 0  success
 * @retval negative fail
 */
static int bd71827_charger_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct bd71827_power *pwr = dev_get_drvdata(psy->dev.parent);
	u32 vot;
	u16 tmp;
	unsigned int reg_dcin_stat = 0;
	int ret;
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		ret = regmap_read(pwr->mfd->regmap, pwr->regs->dcin_stat, &reg_dcin_stat);
		if (ret) {
			dev_err(pwr->mfd->dev, "Failed to read DCIN status\n");
			return ret;
		}
		dev_info(pwr->mfd->dev, "%s PMIC reg dcin_stat:0x%x\n", __func__, reg_dcin_stat);
		val->intval = !!(reg_dcin_stat & BD7182x_MASK_DCIN_DET);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = bd7182x_read16_himask(pwr, pwr->regs->vdcin,
					    BD7182x_MASK_VDCIN_U, &tmp);
		if (ret)
			return ret;

		vot = tmp;
		val->intval = 5000 * vot;		// 5 milli volt steps
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/* @brief get property of power supply bat
 *  @param psy power supply device
 *  @param psp property to get
 *  @param val property value to return
 *  @retval 0  success
 *  @retval negative fail
 */

static int bd71827_battery_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct bd71827_power *pwr = dev_get_drvdata(psy->dev.parent);
	struct bd7182x_soc_data *wr = &pwr->d_r;
	struct bd7182x_soc_data *wd = &pwr->d_w;
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if(bd71827_charge_status(pwr, wd))
			dev_err(pwr->mfd->dev, "update charge status error\n");
		update_soc_data(pwr);
		spin_lock(&pwr->dlock);
		val->intval = wr->rpt_status;
		spin_unlock(&pwr->dlock);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		spin_lock(&pwr->dlock);
		val->intval = wr->bat_health;
		spin_unlock(&pwr->dlock);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		spin_lock(&pwr->dlock);
		if (wr->rpt_status == POWER_SUPPLY_STATUS_CHARGING)
			val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
		else
			val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
		spin_unlock(&pwr->dlock);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		spin_lock(&pwr->dlock);
		val->intval = wr->bat_online;
		spin_unlock(&pwr->dlock);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		if(bd71827_get_voltage_current_directly(pwr, wd))
			dev_err(pwr->mfd->dev, "Failed to get voltage/current directly.\n");
		update_soc_data(pwr);
		spin_lock(&pwr->dlock);
		val->intval = wr->vcell;
		spin_unlock(&pwr->dlock);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		spin_lock(&pwr->dlock);
		val->intval = wr->clamp_soc;
		spin_unlock(&pwr->dlock);
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
	{
		u32 t;

		t = wr->coulomb_cnt >> 16;
		t = A10s_mAh(t);
		if (t > A10s_mAh(wr->designed_cap))
			t = A10s_mAh(wr->designed_cap);
		/* uA to report */
		val->intval = t * 1000;
		break;
	}
	case POWER_SUPPLY_PROP_PRESENT:
		spin_lock(&pwr->dlock);
		val->intval = wr->bat_online;
		spin_unlock(&pwr->dlock);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = BATTERY_FULL_DEFAULT *
			      A10s_mAh(wr->designed_cap) * 10;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		val->intval = BATTERY_FULL_DEFAULT *
			      A10s_mAh(wr->full_cap) * 10;
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		spin_lock(&pwr->dlock);
		val->intval = wr->curr_avg;
		spin_unlock(&pwr->dlock);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if(bd71827_get_voltage_current_directly(pwr,wd))
			dev_err(pwr->mfd->dev, "Failed to get voltage/current directly.\n");
		update_soc_data(pwr);
		spin_lock(&pwr->dlock);
		val->intval = wr->curr;
		spin_unlock(&pwr->dlock);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		spin_lock(&pwr->dlock);
		val->intval = wr->temp;
		spin_unlock(&pwr->dlock);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = max_voltage;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN:
		val->intval = min_voltage;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = MAX_CURRENT_WIRELESS_HIGH_DEFAULT;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		spin_lock(&pwr->dlock);
		val->intval = wr->charge_control_limit;
		spin_unlock(&pwr->dlock);
		break;
	case POWER_SUPPLY_PROP_THERMAL_INPUT_POWER_LIMIT:
		spin_lock(&pwr->dlock);
		val->intval = wr->thermal_input_power_limit;
		spin_unlock(&pwr->dlock);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

/* @brief set property of power supply bat
 *	@param psy power supply device
 *	@param psp property to get
 *	@param val property value to return
 *	@retval 0  success
 *	@retval negative fail
 */

static int bd71827_battery_set_property(struct power_supply *psy,
					enum power_supply_property psp,
					const union power_supply_propval *val)
{
	struct bd71827_power *pwr = dev_get_drvdata(psy->dev.parent);
	struct bd7182x_soc_data *wd = &pwr->d_w;
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
	{
		wd->charge_control_limit = val->intval * 95 /100;
		update_soc_data(pwr);

		dev_info(pwr->mfd->dev,"%s:%d charge_control_limit:%d mA\n", __func__, __LINE__, wd->charge_control_limit);

		if(!schedule_delayed_work(&pwr->bd_current_ramp_work, msecs_to_jiffies(0))) {
			cancel_delayed_work(&pwr->bd_current_ramp_work);
			schedule_delayed_work(&pwr->bd_current_ramp_work, msecs_to_jiffies(0));
		}
		break;
	}
	case POWER_SUPPLY_PROP_THERMAL_INPUT_POWER_LIMIT:
		wd->thermal_input_power_limit = val->intval;
		update_soc_data(pwr);
		if (val->intval == 0)
			wireless_charger_dev_set_wpc_en(get_charger_by_name("wireless_chg"), false);
		else
			wireless_charger_dev_set_wpc_en(get_charger_by_name("wireless_chg"), true);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

/* @brief ac properties */
static enum power_supply_property bd71827_charger_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
};

/* @brief bat properies */
static enum power_supply_property bd71827_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MIN,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
	POWER_SUPPLY_PROP_THERMAL_INPUT_POWER_LIMIT,
};

static struct power_supply_desc bd71827_ac_desc = {
	.name		= AC_NAME,
	.type		= POWER_SUPPLY_TYPE_MAINS,
	.properties	= bd71827_charger_props,
	.num_properties	= ARRAY_SIZE(bd71827_charger_props),
	.get_property	= bd71827_charger_get_property,
};

static const struct power_supply_desc bd71827_battery_desc = {
	.name		= BAT_NAME,
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties	= bd71827_battery_props,
	.num_properties	= ARRAY_SIZE(bd71827_battery_props),
	.get_property	= bd71827_battery_get_property,
	.set_property	= bd71827_battery_set_property,
};

#ifdef CONFIG_CHARGER_HAL
static struct charger_hal_pmic_ops bd71827_pmic_ops = {
	.current_event_handler = pmic_current_event_handler,
};
#endif

static ssize_t bd71827_sysfs_set_charging(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf,
					   size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct bd71827_power *pwr = power_supply_get_drvdata(psy);
	int ret;
	unsigned int val;

	ret = kstrtoint(buf, 0, &val);
	if ((ret != 0) || (val > 1)) {
		dev_warn(dev, "use 0/1 to disable/enable charging, %d\n", ret);
		return -EINVAL;
	}

	dev_info(pwr->mfd->dev, "%s, set PMIC CHG_EN as %d\n", __func__, val);
	if (val == 1)
		ret = regmap_update_bits(pwr->mfd->regmap, pwr->regs->chg_en,
				BD7182x_MASK_CHG_EN, BD7182x_MASK_CHG_EN);
	else
		ret = regmap_update_bits(pwr->mfd->regmap, pwr->regs->chg_en,
				 BD7182x_MASK_CHG_EN, 0);
	if (ret)
		return ret;

	return count;
}

static ssize_t bd71827_sysfs_show_charging(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct bd71827_power *pwr = power_supply_get_drvdata(psy);
	int chg_en, ret, charger_online;

	ret = regmap_read(pwr->mfd->regmap, pwr->regs->chg_en, &chg_en);
	if (ret)
		return ret;

	ret = regmap_read(pwr->mfd->regmap, pwr->regs->dcin_stat, &charger_online);
	if (ret) {
		dev_err(pwr->mfd->dev, "Failed to read DCIN status\n");
		return ret;
	}
	smp_rmb(); /* wait for sync */
	chg_en &= BD7182x_MASK_CHG_EN;
	charger_online &= BD7182x_MASK_DCIN_DET;
	return sprintf(buf, "%x\n", charger_online && chg_en);
}

static DEVICE_ATTR(charging, 0644,
		bd71827_sysfs_show_charging, bd71827_sysfs_set_charging);

static ssize_t bd71827_sysfs_set_gauge(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf,
					   size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct bd71827_power *pwr = power_supply_get_drvdata(psy);
	int ret, delay;

	ret = kstrtoint(buf, 0, &delay);
	if (ret != 0) {
		dev_err(pwr->mfd->dev, "error: write a integer string");
		return -EINVAL;
	}

	if (delay == -1) {
		dev_info(pwr->mfd->dev, "Gauge schedule cancelled\n");
		cancel_delayed_work(&pwr->bd_work);
		return count;
	}

	dev_info(pwr->mfd->dev, "Gauge schedule in %d\n", delay);
	pwr->gauge_delay = delay;
	smp_wmb(); /* wait for sync */
	schedule_delayed_work(&pwr->bd_work, msecs_to_jiffies(delay));

	return count;
}

static ssize_t bd71827_sysfs_show_gauge(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct bd71827_power *pwr = power_supply_get_drvdata(psy);
	ssize_t ret;

	smp_rmb(); /* wait for sync */
	ret = sprintf(buf, "Gauge schedule in %d\n",
		      pwr->gauge_delay);
	return ret;
}

static DEVICE_ATTR(gauge, 0644,
		bd71827_sysfs_show_gauge, bd71827_sysfs_set_gauge);

static ssize_t bd71827_sysfs_show_current_event(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	ssize_t ret;

	ret = sprintf(buf, "Current charger type is  %d\ntest current type with: \n1(USB), 2(CDP), 3(DCP), 4(WIRELESS-1500mA), 5(WIRELESS-750mA), other number(Unknown)\n",
		      bd71827_ac_desc.type);
	return ret;
}
static int cur_limit;

static ssize_t bd71827_sysfs_set_current_event(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf,
					   size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct bd71827_power *pwr = power_supply_get_drvdata(psy);
	int ret, mode;

	ret = kstrtoint(buf, 0, &mode);
	if (ret != 0) {
		dev_err(pwr->mfd->dev, "error: write a integer string");
		return -EINVAL;
	}

	switch (mode) {
		case 1:
			dev_info(pwr->mfd->dev, "set charger as POWER_SUPPLY_TYPE_USB\n");
			cur_limit = 500;
			pmic_current_event_handler(POWER_SUPPLY_TYPE_USB, (void *)cur_limit);
			return count;
		break;
		case 2:
			dev_info(pwr->mfd->dev, "set charger as POWER_SUPPLY_TYPE_USB_CDP\n");
			cur_limit = 1500;
			pmic_current_event_handler(POWER_SUPPLY_TYPE_USB_CDP, (void *)cur_limit);
			return count;
		break;
		case 3:
			dev_info(pwr->mfd->dev, "set charger as POWER_SUPPLY_TYPE_USB_DCP\n");
			cur_limit = 1500;
			pmic_current_event_handler(POWER_SUPPLY_TYPE_USB_DCP, (void *)cur_limit);
			return count;
		break;
		case 4:
			dev_info(pwr->mfd->dev, "set charger as POWER_SUPPLY_TYPE_WIRELESS, 1500mA\n");
			cur_limit = 1500;
			pmic_current_event_handler(POWER_SUPPLY_TYPE_WIRELESS, (void *)cur_limit);
			return count;
		break;
		case 5:
			dev_info(pwr->mfd->dev, "set charger as POWER_SUPPLY_TYPE_WIRELESS, 750mA\n");
			cur_limit = 750;
			pmic_current_event_handler(POWER_SUPPLY_TYPE_WIRELESS, (void *)cur_limit);
			return count;
		break;
		default:
			dev_info(pwr->mfd->dev, "set charger as POWER_SUPPLY_TYPE_UNKNOWN\n");
			cur_limit = 550;
			pmic_current_event_handler(POWER_SUPPLY_TYPE_UNKNOWN, (void *)cur_limit);
			return count;
		break;
	}

	return count;
}


static DEVICE_ATTR(current_event, 0644,
		bd71827_sysfs_show_current_event, bd71827_sysfs_set_current_event);

// will use clamp_soc * designed_cap to show the mah, so that the mah logged in kdm is consistent with
// the soc show on the device.
static ssize_t bd71827_sysfs_show_battery_mah(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct bd71827_power *pwr = power_supply_get_drvdata(psy);
	struct bd7182x_soc_data *wr = &pwr->d_r;

//	dev_info(pwr->mfd->dev,"pwr->clamp_soc:%d, full_cap:%d, designed_cap:%d\n",wr->clamp_soc, A10s_mAh(wr->full_cap), A10s_mAh(wr->designed_cap));
	return sprintf(buf, "%d\n", wr->clamp_soc * A10s_mAh(wr->full_cap)/100);
}
static DEVICE_ATTR(battery_mah, S_IRUGO,bd71827_sysfs_show_battery_mah, NULL);

static ssize_t bd71827_battery_cycle_show(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	return sprintf(buf, "%d\n", battery_cycle);
}

static ssize_t
bd71827_battery_cycle_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{

	int value;
        struct power_supply *psy = dev_get_drvdata(dev);
        struct bd71827_power *pwr = power_supply_get_drvdata(psy);
	struct bd7182x_soc_data *wd;

	wd = &pwr->d_w;
	if (sscanf(buf, "%d", &value) <= 0) {
		return -EINVAL;
	}
	if (value >= 0) {
		dev_info(pwr->mfd->dev,"battery_cycle is restored to %d\n", value);
		battery_cycle=value;
		wd->cycle=battery_cycle;
	} else {
		return -EINVAL;
	}
	return count;
}

static DEVICE_ATTR(battery_cycle, S_IWUSR | S_IRUGO, bd71827_battery_cycle_show, bd71827_battery_cycle_store);

static ssize_t bd71827_wdog_sel_show(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
        struct power_supply *psy = dev_get_drvdata(dev);
        struct bd71827_power *pwr = power_supply_get_drvdata(psy);
	int ret = 0, value = 0;
	ret = regmap_read(pwr->mfd->regmap, pwr->regs->ps_ctrl_2, &value);
	if (ret) {
		dev_err(pwr->mfd->dev, "Failed to read PMIC PS_CTRL_2 WDOGB_RESET_SEL\n");
		return ret;
	}

	return sprintf(buf, "%d\n", value&BD71828_MASK_PS_CTRL_2_WDOGB_RESET_SEL?1:0);
}

static ssize_t
bd71827_wdog_sel_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{

	int value;
        struct power_supply *psy = dev_get_drvdata(dev);
        struct bd71827_power *pwr = power_supply_get_drvdata(psy);
	int ret;

	if (sscanf(buf, "%d", &value) <= 0) {
		return -EINVAL;
	}
	if (value == 0 || value == 1) {
		if (idme_hwid_value  >= 6) {
		//set WDOGB_RESET_SEL to cold reboot for EVT and beyond as the HW WDOG gating
		//logic is finalized since EVT
			dev_info(pwr->mfd->dev, "set PMIC PS_CTRL_2  WDOGB_RESET_SEL(bit 1) to %d\n", value);
			ret = regmap_update_bits(pwr->mfd->regmap, pwr->regs->ps_ctrl_2,
					 BD71828_MASK_PS_CTRL_2_WDOGB_RESET_SEL, value?BD71828_MASK_PS_CTRL_2_WDOGB_RESET_SEL:0);
			if (ret) {
				dev_err(pwr->mfd->dev, "Failed to set PMIC PS_CTRL_2 WDOGB_RESET_SEL\n");
			}
		}else
			dev_err(pwr->mfd->dev, "hwid:%d, changing WDOGB_RESET_SEL setting not allowed\n", idme_hwid_value);

	} else {
		return -EINVAL;
	}
	return count;
}

static DEVICE_ATTR(wdog_sel, S_IWUSR | S_IRUGO, bd71827_wdog_sel_show, bd71827_wdog_sel_store);

static ssize_t bd71827_battery_stressed_show(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	return sprintf(buf, "%d\n", battery_stressed);
}

static ssize_t
bd71827_battery_stressed_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{

	int value;
        struct power_supply *psy = dev_get_drvdata(dev);
        struct bd71827_power *pwr = power_supply_get_drvdata(psy);
	if (sscanf(buf, "%d", &value) <= 0) {
		return -EINVAL;
	}
	if (value >= 0) {
		dev_info(pwr->mfd->dev,"battery_stressed is set to %d\n", value);
		battery_stressed = value;
		bd71827_set_battery_parameters(pwr, battery_stressed || battery_safe_charging);
		bd71827_safe_charging_control(pwr, battery_stressed || battery_safe_charging);

	} else {
		return -EINVAL;
	}
	return count;
}


static DEVICE_ATTR(battery_stressed, S_IWUSR | S_IRUGO, bd71827_battery_stressed_show, bd71827_battery_stressed_store);

static ssize_t bd71827_battery_safe_charging_show(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	return sprintf(buf, "%d\n", battery_safe_charging);
}

static ssize_t
bd71827_battery_safe_charging_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{

	int value;
        struct power_supply *psy = dev_get_drvdata(dev);
        struct bd71827_power *pwr = power_supply_get_drvdata(psy);
	if (sscanf(buf, "%d", &value) <= 0) {
		return -EINVAL;
	}
	if (value >= 0) {
		dev_info(pwr->mfd->dev,"battery_safe_charging is set to %d\n", value);
		battery_safe_charging = value;
		bd71827_set_battery_parameters(pwr, battery_stressed || battery_safe_charging);
		bd71827_safe_charging_control(pwr, battery_stressed || battery_safe_charging);

	} else {
		return -EINVAL;
	}
	return count;
}

static DEVICE_ATTR(battery_safe_charging, S_IWUSR | S_IRUGO, bd71827_battery_safe_charging_show, bd71827_battery_safe_charging_store);

static struct attribute *bd71827_sysfs_attributes[] = {
	&dev_attr_charging.attr,
	&dev_attr_gauge.attr,
	&dev_attr_current_event.attr,
	&dev_attr_battery_mah.attr,
	&dev_attr_battery_cycle.attr,
	&dev_attr_wdog_sel.attr,
	&dev_attr_battery_stressed.attr,
	&dev_attr_battery_safe_charging.attr,
	NULL,
};

static const struct attribute_group bd71827_sysfs_attr_group = {
	.attrs = bd71827_sysfs_attributes,
};

/* @brief powers supplied by bd71827_ac */
static char *bd71827_ac_supplied_to[] = {
	BAT_NAME,
};

#ifdef PWRCTRL_HACK
/* This is not-so-pretty hack for allowing external code to call
 * bd71827_chip_hibernate() without this power device-data
 */
static struct bd71827_power *hack;
static struct mutex pwrlock;

static struct bd71827_power *get_power(void)
{
	mutex_lock(&pwrlock);
	if (!hack) {
		mutex_unlock(&pwrlock);
		return (struct bd71827_power *)-ENOENT;
	}
	return hack;
}

static void put_power(void)
{
	mutex_unlock(&pwrlock);
}

static int set_power(struct bd71827_power *pwr)
{
	mutex_lock(&pwrlock);
	hack = pwr;
	mutex_unlock(&pwrlock);
	return 0;
}

static void free_power(void)
{
	mutex_lock(&pwrlock);
	hack = NULL;
	mutex_unlock(&pwrlock);
}

/* called from pm inside machine_halt */
void bd71827_chip_hibernate(void)
{
	struct bd71827_power *pwr = get_power();

	if (IS_ERR(pwr)) {
		pr_err("%s called before device is ready\n", __func__);
		put_power();
		return;
	}

	/* programming sequence in EANAB-151 */
	regmap_update_bits(pwr->mfd->regmap, pwr->regs->pwrctrl,
			   pwr->regs->hibernate_mask, 0);
	regmap_update_bits(pwr->mfd->regmap, pwr->regs->pwrctrl,
			   pwr->regs->hibernate_mask,
			   pwr->regs->hibernate_mask);
	put_power();
}



static int bd71827_cooling_dev_get_max_state(struct thermal_cooling_device *cdev,
	unsigned long *state)
{
	*state = MAX_BD71827_COOLING_DEV_STATE;
	return 0;
}

static int bd71827_cooling_dev_get_cur_state(struct thermal_cooling_device *cdev,
	unsigned long *state)
{
	unsigned long dev_state = *(unsigned long *)(cdev->devdata);

	if (state)
		*state = dev_state;
	printk("%s return cur_state:%ld\n",__func__, dev_state);
	return 0;
}

static int bd71827_cooling_dev_set_cur_state(struct thermal_cooling_device *cdev,
	unsigned long state)
{
	unsigned long curr_state;
	int ret;
	struct bd71827_power *pwr = get_power();

	if (IS_ERR(pwr) ) {
		pr_err("%s called before device is ready\n",__func__);
		return -ENODEV;
	}
	if (!cdev->devdata){
		pr_err("%s, bd71827_cooling_dev no internal data\n", __func__);
		ret = -1;
		goto end;
	}

	if(state > MAX_BD71827_COOLING_DEV_STATE){
		pr_err("%ld exceeding maximal cooling state:%d\n",state, MAX_BD71827_COOLING_DEV_STATE);
		ret = -1;
		goto end;
	}
	curr_state = *(unsigned long *)(cdev->devdata);
	*(unsigned long *)(cdev->devdata) = state;

	printk("%s set cur_state:%ld\n",__func__, state);

	if (state == 1)
		ret = regmap_update_bits(pwr->mfd->regmap, pwr->regs->chg_en,
				BD7182x_MASK_CHG_EN, BD7182x_MASK_CHG_EN);
	else
		ret = regmap_update_bits(pwr->mfd->regmap, pwr->regs->chg_en,
				 BD7182x_MASK_CHG_EN, 0);
end:
	put_power();
	return ret;
}

static struct thermal_cooling_device_ops bd71827_cooling_dev_ops = {
	.get_max_state = bd71827_cooling_dev_get_max_state,
	.get_cur_state = bd71827_cooling_dev_get_cur_state,
	.set_cur_state = bd71827_cooling_dev_set_cur_state,
};

int pmic_current_event_handler(unsigned long event, void *ptr)
{
	struct bd71827_power *pwr = get_power();
	enum power_supply_type type_new = event;
	int dc_input_cur = (int) ptr; // mA units
	int chg_output_cur = 0;
	struct bd7182x_soc_data *wd;

	wd = &pwr->d_w;

	if (IS_ERR(pwr) || pwr->ac == NULL) {
		pr_err("%s called before device is ready\n",__func__);
		put_power();
		return -ENODEV;
	}
//Per latest discussion between Rohm/Lab, we set the current limit as:
//SDP: DCIN 450mA, IFST 550mA
//DCP: DCIN 1350mA, IFST 1500mA
	// "MAX_CURRENT" is maximum charging current in uA units

	dev_info(pwr->mfd->dev,"%s:%d event:%ld max dcin current:%dmA, ifst:%dmA\n", __func__, __LINE__, event, dc_input_cur, chg_output_cur);
	switch (event) {
	case POWER_SUPPLY_TYPE_USB:
		dc_input_cur = dc_input_cur*95/100; // to compensate +/- 10% offset from PMIC ADC
		dc_input_cur = min(dc_input_cur, MAX_CURRENT_SDP_DEFAULT / 1000);
		chg_output_cur = MAX_CURRENT_CHG_OUTPUT_SDP_DEFAULT / 1000;
		break;
	case POWER_SUPPLY_TYPE_USB_CDP:
	case POWER_SUPPLY_TYPE_USB_DCP:
		dc_input_cur = dc_input_cur*95/100; // to compensate +/- 10% offset from PMIC ADC
		dc_input_cur = min(dc_input_cur, MAX_CURRENT_DCP_DEFAULT / 1000);
		chg_output_cur = MAX_CURRENT_CHG_OUTPUT_DCP_DEFAULT / 1000;
		break;
	case POWER_SUPPLY_TYPE_WIRELESS:
		dc_input_cur = dc_input_cur*95/100; // to compensate +/- 10% offset from PMIC ADC
		dc_input_cur = min(dc_input_cur, MAX_CURRENT_DCP_DEFAULT / 1000);
		chg_output_cur = min(dc_input_cur, MAX_CURRENT_CHG_OUTPUT_DCP_DEFAULT / 1000);
		break;
	case POWER_SUPPLY_TYPE_UNKNOWN:
	default:
		type_new = POWER_SUPPLY_TYPE_MAINS;
		dc_input_cur = min(dc_input_cur, MAX_CURRENT_SDP_DEFAULT / 1000);
		chg_output_cur = MAX_CURRENT_CHG_OUTPUT_SDP_DEFAULT / 1000;
	}

	dc_input_cur = min(dc_input_cur, CHG_DCIN_UPPER_LIMIT);
#if 0 //todo: current limit to be checked against battery max_current parameter limit
	if (max_current > 0) {
		dc_input_cur = min(dc_input_cur, (max_current / 1000));
	}
#endif
	dev_info(pwr->mfd->dev,"%s:%d event: %ld, setting: max_dcin:%dmA, ifst:%dmA\n", __func__, __LINE__, event, dc_input_cur, chg_output_cur);
	wd->chg_dcin_cur_limit = dc_input_cur;


	if (type_new != bd71827_ac_desc.type) {
		bd71827_ac_desc.type = type_new;
		bd71827_get_online(pwr, wd);
		power_supply_changed(pwr->ac);
		//reset pmic chgint when charger type changed, current ramp would happen after the reset
		if(!schedule_delayed_work(&pwr->bd_reset_chgint_work, msecs_to_jiffies(RESET_CHGINT_DELAY))) {
			cancel_delayed_work(&pwr->bd_reset_chgint_work);
			schedule_delayed_work(&pwr->bd_reset_chgint_work, msecs_to_jiffies(RESET_CHGINT_DELAY));
		}
	} else {
		// schedule current ramp up work
		if(!schedule_delayed_work(&pwr->bd_current_ramp_work, msecs_to_jiffies(10))) {
			cancel_delayed_work(&pwr->bd_current_ramp_work);
			schedule_delayed_work(&pwr->bd_current_ramp_work, msecs_to_jiffies(10));
		}
	}
	put_power();
	return 0;
}
EXPORT_SYMBOL(pmic_current_event_handler);

/**@ brief bd71827_get_battery_mah
 * @ param none
 * @ this function return the current battery mah
 */
int bd71827_get_battery_mah(void)
{
//	struct bd71827* mfd = pmic_data;
//	struct power_supply *psy = dev_get_drvdata(mfd->dev);
	struct bd71827_power *pwr = get_power();
	struct bd7182x_soc_data *wr = &pwr->d_r;
	int battery_mah = 0;

	if (IS_ERR(pwr)) {
		pr_err("%s called before device is ready\n", __func__);
	}
	else if(pwr) {
		dev_info(pwr->mfd->dev,"wr->clamp_soc:%d, full_cap:%d, designed_cap:%d\n",wr->clamp_soc, wr->full_cap, wr->designed_cap);
		battery_mah = wr->clamp_soc *A10s_mAh(wr->full_cap)/100;
	}

	put_power();
	return battery_mah;
}
EXPORT_SYMBOL(bd71827_get_battery_mah);

int bd71828_get_batt_temperature(void)
{
#define DEFAULT_BATT_TEMPERATURE	-255

	struct bd71827_power *pwr = get_power();
	int ret = 0;
	int temp = DEFAULT_BATT_TEMPERATURE;
        if( IS_ERR(pwr)) {
		dev_info(pwr->mfd->dev, "%s called before driver is ready, return default value(-255)\n", __func__);
                temp = DEFAULT_BATT_TEMPERATURE;
        }
        else if(pwr) {
		ret = pwr->get_temp(pwr, &temp);
		if (ret)
		{
			dev_info(pwr->mfd->dev, "%s failed to get battery temperature. return default value(-255)\n", __func__);
			temp = DEFAULT_BATT_TEMPERATURE;
		}
		dev_info(pwr->mfd->dev, "%s return value: %d\n", __func__, temp);
	}
	put_power();
	return temp;
}
EXPORT_SYMBOL(bd71828_get_batt_temperature);

static void bd71827_safe_charging_control(struct bd71827_power *pwr, int on_off)
{
	int ret=0;

	printk(KERN_INFO "%s on_off:%d\n",__func__, on_off);
	if(on_off) {
		//Set different stressed top of charge threshold based on different battery ID parameters.
		//Use 4.2v for all packs for now. May need to set to 4.1v for MURATA once the proper parameters are
		//available
		if(        use_load_bat_params == USE_BAT_PARAMS_15KOHM_ATL
			|| use_load_bat_params == USE_BAT_PARAMS_22KOHM_MURATA
			|| use_load_bat_params == USE_BAT_PARAMS_29KOHM_LIWINON )
		{
			dev_info(pwr->mfd->dev,"%s set top of charge to 4.2v\n", __func__);
			/*safe charge is on, set charging voltage to 4.2V*/
			ret = regmap_write(pwr->mfd->regmap, pwr->regs->chg_vbat_1,BD71828_CHG_VBAT_CHG_4P2V);
			if (ret)
				goto err;
			ret = regmap_write(pwr->mfd->regmap, pwr->regs->chg_vbat_2,BD71828_CHG_VBAT_CHG_2_DEFAULT); //This value does not change for HOT1 region, remain at 4.1v
			if (ret)
				goto err;
			ret = regmap_write(pwr->mfd->regmap, pwr->regs->chg_vbat_3,BD71828_CHG_VBAT_CHG_4P2V);
			if (ret)
				goto err;
		}
		else {
			dev_info(pwr->mfd->dev,"%s set top of charge to 4.1v\n", __func__);
			/*safe charge is on, set charging voltage to 4.1V*/
			ret = regmap_write(pwr->mfd->regmap, pwr->regs->chg_vbat_1,BD71828_CHG_VBAT_CHG_4P1V);
			if (ret)
				goto err;
			ret = regmap_write(pwr->mfd->regmap, pwr->regs->chg_vbat_2,BD71828_CHG_VBAT_CHG_2_DEFAULT); //This value does not change for HOT1 region, remain at 4.1v
			if (ret)
				goto err;
			ret = regmap_write(pwr->mfd->regmap, pwr->regs->chg_vbat_3,BD71828_CHG_VBAT_CHG_4P1V);
			if (ret)
				goto err;
		}
	}
	else {
	/*safe charge is off, restore the default setting*/
		dev_info(pwr->mfd->dev,"%s set top of charge to 4.4v\n", __func__);
		ret = regmap_write(pwr->mfd->regmap, pwr->regs->chg_vbat_1,BD71828_CHG_VBAT_CHG_1_DEFAULT);
		if (ret)
			goto err;
		ret = regmap_write(pwr->mfd->regmap, pwr->regs->chg_vbat_2,BD71828_CHG_VBAT_CHG_2_DEFAULT);
		if (ret)
			goto err;
		ret = regmap_write(pwr->mfd->regmap, pwr->regs->chg_vbat_3,BD71828_CHG_VBAT_CHG_3_DEFAULT);
		if (ret)
			goto err;
	}


	return;
err:
	dev_err(pwr->mfd->dev,"%s failed to write regs\n", __func__);
}

/**@ brief bd71827_get_battery_soc
 * @ param none
 * @ this function return the current battery soc 
 */
int bd71827_get_battery_soc(void)
{
        struct bd71827_power *pwr = get_power();
        struct bd7182x_soc_data *wr = &pwr->d_r;
	int battery_soc = 0;

        if (IS_ERR(pwr)) {
                pr_err("bd71827_chip_hibernate called before probe finished\n");
        }
        else if(pwr) {
		battery_soc = wr->clamp_soc;
        }

        put_power();
        return battery_soc;
}
EXPORT_SYMBOL(bd71827_get_battery_soc);

#endif

#define RSENS_CURR 10000000000LLU
static int bd7182x_set_chip_specifics(struct bd71827_power *pwr, int rsens_ohm)
{
	u64 tmp = RSENS_CURR;

	switch (pwr->mfd->chip_type) {
	case ROHM_CHIP_TYPE_BD71828:
		pwr->regs = &pwr_regs_bd71828;
		pwr->get_temp = bd71828_get_temp;
		break;
	case ROHM_CHIP_TYPE_BD71827:
		pwr->regs = &pwr_regs_bd71827;
		pwr->get_temp = bd71827_get_temp;
		dev_warn(pwr->mfd->dev, "BD71817 not tested\n");
		break;
	default:
		dev_err(pwr->mfd->dev, "Unknown PMIC\n");
		return -EINVAL;
	}

	/* Reg val to uA */
	do_div(tmp, rsens_ohm);

	pwr->curr_factor = tmp;
	dev_info(pwr->mfd->dev,"Setting curr-factor to %u\n", pwr->curr_factor);

	return 0;
}

static void bd_current_ramp_work_callback(struct work_struct *work)
{
	struct bd71827_power *pwr;
	struct delayed_work *delayed_work;
	struct bd7182x_soc_data *wd;
	int next_limit = -1;
	int target_limit = -1;
	int ret = 0;

	delayed_work = container_of(work, struct delayed_work, work);
	pwr = container_of(delayed_work, struct bd71827_power, bd_current_ramp_work);
	wd = &pwr->d_w;

	dev_info(pwr->mfd->dev,"%s: chg_dcin_chr_limit: %d mA, charge_control_limit: %d mA\n", __func__, wd->chg_dcin_cur_limit, wd->charge_control_limit);
	if(wd->chg_dcin_cur_limit != -1 && wd->charge_control_limit != -1) {
		if(wd->chg_dcin_cur_limit >= 0 && wd->charge_control_limit >=0 ) {
			target_limit = min(wd->chg_dcin_cur_limit, wd->charge_control_limit);
		} else {
			dev_err(pwr->mfd->dev,"%s: wrong dcin_cur_limit(%d) or charge_control_limit(%d)\n", __func__, wd->chg_dcin_cur_limit, wd->charge_control_limit);
			dev_info(pwr->mfd->dev,"%s: use default SDP limit:%d mA\n", __func__, MAX_CURRENT_SDP_DEFAULT/1000);
			target_limit = MAX_CURRENT_SDP_DEFAULT/1000;
		}
	}
	else if (wd->chg_dcin_cur_limit == -1 && wd->charge_control_limit == -1) {
		target_limit  = MAX_CURRENT_SDP_DEFAULT/1000;
	} else if (wd->chg_dcin_cur_limit != -1 && wd->charge_control_limit == -1) {
		target_limit = wd->chg_dcin_cur_limit;
	} else {
		//should not get here, dcin_chg_limit should always be available;
		target_limit = wd->charge_control_limit;
	}
	dev_info(pwr->mfd->dev,"%s: target_limit: %d mA\n", __func__, target_limit);

	if(wd->curr_dcin == target_limit) {
		dev_info(pwr->mfd->dev,"%s: no need to change current limit, dcin==target_limit: %d mA\n", __func__, wd->curr_dcin);
		return;
	}
	if(wd->curr_dcin < target_limit) {
		next_limit = min(wd->curr_dcin + CURRENT_RAMP_STEP, target_limit);
		dev_info(pwr->mfd->dev,"%s: ramping up current, %d mA --> %d mA\n", __func__, wd->curr_dcin, next_limit);
	}
	else if (wd->curr_dcin >  target_limit) {
		next_limit =  target_limit;
		dev_info(pwr->mfd->dev,"%s: reduce current,  %d mA -> %d mA\n", __func__, wd->curr_dcin, target_limit);
	}

	if(next_limit !=-1) {
		if(next_limit > CHG_DCIN_SET_STEP)
			wd->dcin_val = next_limit/CHG_DCIN_SET_STEP - 1;
		else
			wd->dcin_val = 0;
		wd->ifst_val = next_limit/CHG_IFST_STEP;
		//set up DCIN and ILIM for the next limit

		ret = regmap_update_bits(pwr->mfd->regmap,
				 pwr->regs->dcin_set,
				 BD7182x_MASK_DCIN_ILIM,
				 wd->dcin_val);
		if (ret)
			dev_err(pwr->mfd->dev,"%s - %d: update PMIC register DCIN_SET error.\n", __func__, __LINE__);

		ret = regmap_update_bits(pwr->mfd->regmap,
				 pwr->regs->chg_ifst,
				 BD7182x_MASK_CHG_IFST,
				 wd->ifst_val);
		if (ret)
			dev_err(pwr->mfd->dev,"%s - %d: update PMIC register CHG_IFST error.\n", __func__, __LINE__);

		wd->curr_dcin = next_limit;
		//schedule for the next ramp up
		if(!schedule_delayed_work(&pwr->bd_current_ramp_work, msecs_to_jiffies(CURRENT_RAMP_DELAY))) {
			cancel_delayed_work(&pwr->bd_current_ramp_work);
			schedule_delayed_work(&pwr->bd_current_ramp_work, msecs_to_jiffies(CURRENT_RAMP_DELAY));
		}
	}

	return;
}

static void bd_reset_chgint_work_callback(struct work_struct *work)
{
	struct bd71827_power *pwr;
	struct delayed_work *delayed_work;
	struct bd7182x_soc_data *wd;
	int ret = 0;
	int chg_wdt_stat_val = 0;


	delayed_work = container_of(work, struct delayed_work, work);
	pwr = container_of(delayed_work, struct bd71827_power, bd_reset_chgint_work);
	wd = &pwr->d_w;

	//If shipping mode is pending, do not do PMIC CHGRST
	if (bd71828_in_shpm) {
		dev_info(pwr->mfd->dev,"%s: shpm pending\n", __func__);
		return;
	}

	if(wd->ilim_dcin_recover_pending) {
		//save the charge watchdog timer value;
		ret = regmap_read(pwr->mfd->regmap, pwr->regs->chg_wdt_stat, &chg_wdt_stat_val);
		if(ret)
			dev_info(pwr->mfd->dev, "%s: failed reading chg_wdt_stat\n", __func__);
		dev_info(pwr->mfd->dev, "%s: chg_wdt_stat:0x%x\n", __func__, chg_wdt_stat_val);
		ret = regmap_update_bits(pwr->mfd->regmap,
				pwr->regs->chg_set1,
				BD7182x_MASK_WDT_DIS,
				BD7182x_MASK_WDT_DIS);
		if(ret)
			dev_info(pwr->mfd->dev, "%s: failed setting WDT_DIS\n", __func__);
	}
	dev_info(pwr->mfd->dev, "canceling ilim_dcin_recover_work\n");
	cancel_delayed_work(&pwr->bd_ilim_dcin_recover_work);

	//always set curr_dcin to 450 before chgint
	wd->curr_dcin = MAX_CURRENT_SDP_DEFAULT/1000;
	if(wd->curr_dcin > CHG_DCIN_SET_STEP)
		wd->dcin_val = wd->curr_dcin/CHG_DCIN_SET_STEP - 1;
	else
		wd->dcin_val = 0;
	wd->ifst_val =wd->curr_dcin/CHG_IFST_STEP;

	ret = regmap_update_bits(pwr->mfd->regmap,
			 pwr->regs->dcin_set,
			 BD7182x_MASK_DCIN_ILIM,
			 wd->dcin_val);
	if (ret)
		dev_err(pwr->mfd->dev,"%s - %d: update PMIC register DCIN_SET error.\n", __func__, __LINE__);

	ret = regmap_update_bits(pwr->mfd->regmap,
			 pwr->regs->chg_ifst,
			 BD7182x_MASK_CHG_IFST,
			 wd->ifst_val);
	if (ret)
		dev_err(pwr->mfd->dev,"%s - %d: update PMIC register CHG_IFST error.\n", __func__, __LINE__);

	dev_info(pwr->mfd->dev,"%s: curr_dcin:%d mA (0x%x), ifst:0x%x, before Resetting PMIC CHG_INIT\n", __func__, wd->curr_dcin, wd->dcin_val, wd->ifst_val);

	// Hold CHGRST bit for 10ms to avoid PMIC stucks in SUSPEND or Battart Assist Mode
	ret = regmap_update_bits(pwr->mfd->regmap,
					 pwr->regs->chg_init,
					 BD7182x_MASK_CHG_INIT_CHGRST,
					 BD7182x_MASK_CHG_INIT_CHGRST);
	if (ret)
		dev_err(pwr->mfd->dev,"%s: failed to set PMIC register chg_init CHGRST bit\n", __func__);
	msleep(10);
	ret = regmap_update_bits(pwr->mfd->regmap,
					 pwr->regs->chg_init,
					 BD7182x_MASK_CHG_INIT_CHGRST,
					 0);
	if (ret)
		dev_err(pwr->mfd->dev,"%s: failed to clear PMIC register chg_init CHGRST bit\n", __func__);
	dev_info(pwr->mfd->dev,"%s: Reset PMIC CHG_INIT done\n", __func__);

	if(wd->ilim_dcin_recover_pending){
		msleep(10);
		//restore charge watchdog timer value;
		if( chg_wdt_stat_val !=0 ){
			ret = regmap_write(pwr->mfd->regmap, pwr->regs->chg_wdt_fst, chg_wdt_stat_val);
			if(ret)
				dev_info(pwr->mfd->dev, "%s: failed writing chg_wdt_fst\n", __func__);
		}
		dev_info(pwr->mfd->dev, "%s: now clear WDT_DIS\n", __func__);
		ret = regmap_update_bits(pwr->mfd->regmap,
				pwr->regs->chg_set1,
				BD7182x_MASK_WDT_DIS,
				0);
		if(ret)
			dev_info(pwr->mfd->dev, "%s: failed setting WDT_DIS\n", __func__);

		wd->ilim_dcin_recover_pending = 0;
	} else {
		dev_info(pwr->mfd->dev, "%s: restore WDT_FST to default:0x%x\n", __func__, BD71828_CHG_WDT_FST_DEFAULT);
		ret = regmap_write(pwr->mfd->regmap, pwr->regs->chg_wdt_fst, BD71828_CHG_WDT_FST_DEFAULT);
		if(ret)
			dev_info(pwr->mfd->dev, "%s: failed writing chg_wdt_fst\n", __func__);
	}
	if(!schedule_delayed_work(&pwr->bd_current_ramp_work, msecs_to_jiffies(CURRENT_RAMP_DELAY))) {
		cancel_delayed_work(&pwr->bd_current_ramp_work);
		schedule_delayed_work(&pwr->bd_current_ramp_work, msecs_to_jiffies(CURRENT_RAMP_DELAY));
	}

	return;
}

#ifndef SAFE_CHARGING_CONTROLLED_BY_USERSPASE
static void bd_safe_charging_work_callback(struct work_struct *work)
{

	struct bd71827_power *pwr;
	struct delayed_work *delayed_work;

	delayed_work = container_of(work, struct delayed_work, work);
	pwr = container_of(delayed_work, struct bd71827_power, bd_safe_charging_work);
	battery_safe_charging = true;
	//DC has been inserted for DCIN_SAFT_CHARGING_DELAY (7days), need to set up safe charging
	bd71827_set_battery_parameters(pwr, battery_safe_charging); //use limited battery parameters as top of charge is reduced for safe charging
	bd71827_safe_charging_control(pwr, battery_safe_charging);
	dev_info(pwr->mfd->dev,"%s -- battery_safe_charging:%d.\n",__func__, battery_safe_charging);
}
#endif

static irqreturn_t bd7182x_short_push(int irq, void *data)
{
	struct bd71827_power *pwr = (struct bd71827_power *)data;

	kobject_uevent(&pwr->mfd->dev->parent->kobj, KOBJ_ONLINE);
	dev_info(pwr->mfd->dev,"POWERON_SHORT\n");

	return IRQ_HANDLED;
}

static irqreturn_t bd7182x_long_push(int irq, void *data)
{
	struct bd71827_power *pwr = (struct bd71827_power *)data;

	dev_info(pwr->mfd->dev,"POWERON_LONG\n");

	return IRQ_HANDLED;
}

/**
 * copy @size bytes from @buf to @filp with @offset
*/
static loff_t copy_buf_to_file(const char *buf, const loff_t size, struct file *filp, loff_t *offset)
{
	loff_t bytes_ret = 0;
	loff_t bytes_copied = 0;	// how many bytes have been copied from src to dst

	if(buf == NULL || filp == NULL) return -EINVAL;

	while(bytes_copied < size) {
		bytes_ret = vfs_write(filp, buf + bytes_copied, size - bytes_copied, offset);
		if(bytes_ret < 0) return bytes_ret;
		bytes_copied += bytes_ret;
	}

	return bytes_copied;
}

#define PSOTRE_LOG_DST_PATH		"/dev/mmcblk0p6"
#define PSOTRE_LOG_SAVE_AFTER_MS		5000
#define PSTORE_LOG_HEAD_MAGIC_NUMBER		0x1cd0927
#define PSTORE_LOG_HEAD_ADDR_IN_BYTE		SZ_8M

struct pstore_log_head
{
	ulong	head_magic;
	ulong	log_addr;	// offset in byte (DRAM)
	ulong	log_size;	// size in byte (DRAM)
};

/**
 * copy pstore log from DRAM to EMMC
 * The copied log(including log head) starts at address PSTORE_LOG_HEAD_ADDR_IN_BYTE
 */
static void bd_log_save_work_callback(struct work_struct *work)
{
	const char  *log_src = NULL;	// log file source
	struct file *log_dst = NULL;	// log file destination

	loff_t log_offset = 0;	// log offset in bytes

	struct rtc_time tm;
	struct timespec64 now;

	mm_segment_t pre_fs;	// for reading/writing file in kernel

	struct pstore_log_head head = {
		.head_magic	= PSTORE_LOG_HEAD_MAGIC_NUMBER,
		.log_addr	= CONFIG_PSTORE_MEM_ADDR,
		.log_size	= CONFIG_PSTORE_MEM_SIZE,
	};

	/* 1. open log */
	log_src = (char*)phys_to_virt((u32)CONFIG_PSTORE_MEM_ADDR);

	/* 2. open emmc */
	log_dst = filp_open(PSOTRE_LOG_DST_PATH, O_RDWR | O_SYNC, 0);
	if(log_dst == NULL || IS_ERR(log_dst)) return;	// error handle if necessary : miscdata partition open failed !

	pre_fs = get_fs();	// for kernel to access files
	set_fs(KERNEL_DS);	// for kernel to access files

	/* 3. record log time */
	ktime_get_real_ts64(&now);
	rtc_time64_to_tm(now.tv_sec, &tm);	// convert it into readable formate
	pr_info("Power button long press triggered reboot at %d-%02d-%02d %02d:%02d:%02d UTC (%lld)\n",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, (long long) now.tv_sec);

	/* 4. copy head & log to dst */
	log_offset = PSTORE_LOG_HEAD_ADDR_IN_BYTE;
	copy_buf_to_file((char*)(&head), sizeof(struct pstore_log_head), log_dst, &log_offset);
	copy_buf_to_file(log_src, head.log_size, log_dst, &log_offset);

	/* 5. release resource */
	vfs_fsync(log_dst, 0);
	filp_close(log_dst, NULL);

	set_fs(pre_fs);
}

static irqreturn_t bd7182x_mid_push(int irq, void *data)
{
	struct bd71827_power *pwr = (struct bd71827_power *)data;

	kobject_uevent(&pwr->mfd->dev->parent->kobj, KOBJ_OFFLINE);
	dev_info(pwr->mfd->dev,"POWERON_MID\n");

	schedule_delayed_work(&pwr->bd_log_save_work, msecs_to_jiffies(PSOTRE_LOG_SAVE_AFTER_MS));

	return IRQ_HANDLED;
}

#if 0
static irqreturn_t bd7182x_push(int irq, void *data)
{

	pr_info("POWERON_PRESS\n");

	return IRQ_HANDLED;
}
#endif

static irqreturn_t bd7182x_dcin_removed(int irq, void *data)
{
	struct bd71827_power *pwr = (struct bd71827_power *)data;
	struct bd7182x_soc_data *wd = &pwr->d_w;

	kobject_uevent(&pwr->mfd->dev->kobj, KOBJ_REMOVE);
	dev_info(pwr->mfd->dev, "\n~~~DCIN removed\n");

	//thermal charging limit only valid when charger is online
	dev_info(pwr->mfd->dev,"%s: clear charge_control_limit (%d->-1) as charger is not online\n", __func__, wd->charge_control_limit);
	wd->charge_control_limit = -1;

#ifndef SAFE_CHARGING_CONTROLLED_BY_USERSPASE
	cancel_delayed_work(&pwr->bd_safe_charging_work);
	if(!battery_stressed && battery_safe_charging){
	//use regular battery parameters as device is not stressed, and safe charging condition is now removed
		bd71827_safe_charging_control(pwr, false);
		bd71827_set_battery_parameters(pwr, false);
	}
	battery_safe_charging = false;
	dev_info(pwr->mfd->dev, "\nsafe charge settings disabled, work canceled\n");
#endif
	return IRQ_HANDLED;
}

static irqreturn_t bd7182x_dcin_detected(int irq, void *data)
{
	struct bd71827_power *pwr = (struct bd71827_power *)data;

	kobject_uevent(&pwr->mfd->dev->kobj, KOBJ_ADD);
	dev_info(pwr->mfd->dev, "\n~~~DCIN inserted\n");
#ifndef SAFE_CHARGING_CONTROLLED_BY_USERSPASE
	if(!schedule_delayed_work(&pwr->bd_safe_charging_work, msecs_to_jiffies(DCIN_SAFE_CHARGING_DELAY))) {
		cancel_delayed_work(&pwr->bd_safe_charging_work);
		schedule_delayed_work(&pwr->bd_safe_charging_work, msecs_to_jiffies(DCIN_SAFE_CHARGING_DELAY));
	}
	dev_info(pwr->mfd->dev, "\nsafe charge work scheduled\n");
#endif
	return IRQ_HANDLED;
}

static irqreturn_t bd7182x_dcin_ilim(int irq, void *data)
{
	struct bd71827_power *pwr = (struct bd71827_power *)data;

	dev_info(pwr->mfd->dev, "\n~~~DCIN ILIM detected\n");
	return IRQ_HANDLED;
}
static irqreturn_t bd71827_vbat_low_res(int irq, void *data)
{
	struct bd71827_power *pwr = (struct bd71827_power *)data;

	dev_info(pwr->mfd->dev, "\n~~~ VBAT LOW Resumed ...\n");
	return IRQ_HANDLED;
}

static irqreturn_t bd71827_vbat_low_det(int irq, void *data)
{
	struct bd71827_power *pwr = (struct bd71827_power *)data;

	dev_info(pwr->mfd->dev, "\n~~~ VBAT LOW Detected ...\n");
	if(!schedule_delayed_work(&pwr->bd_lobat_check_work, msecs_to_jiffies(0))) {
		cancel_delayed_work(&pwr->bd_lobat_check_work);
		schedule_delayed_work(&pwr->bd_lobat_check_work, msecs_to_jiffies(0));
	}

	return IRQ_HANDLED;
}

static irqreturn_t bd71827_temp_bat_hi_det(int irq, void *data)
{
	struct bd71827_power *pwr = (struct bd71827_power *)data;

	dev_info(pwr->mfd->dev, "\n~~~ Overtemp Detected ...\n");

	return IRQ_HANDLED;
}

static irqreturn_t bd71827_temp_bat_hi_res(int irq, void *data)
{
	struct bd71827_power *pwr = (struct bd71827_power *)data;

	dev_info(pwr->mfd->dev, "\n~~~ Overtemp Resumed ...\n");

	return IRQ_HANDLED;
}

static irqreturn_t bd71827_temp_bat_low_det(int irq, void *data)
{
	struct bd71827_power *pwr = (struct bd71827_power *)data;

	dev_info(pwr->mfd->dev, "\n~~~ Lowtemp Detected ...\n");

	return IRQ_HANDLED;
}

static irqreturn_t bd71827_temp_bat_low_res(int irq, void *data)
{
	struct bd71827_power *pwr = (struct bd71827_power *)data;

	dev_info(pwr->mfd->dev, "\n~~~ Lowtemp Resumed ...\n");

	return IRQ_HANDLED;
}

static irqreturn_t bd71827_temp_vf_det(int irq, void *data)
{
	struct bd71827_power *pwr = (struct bd71827_power *)data;

	dev_info(pwr->mfd->dev, "\n~~~ VF Detected ...\n");

	return IRQ_HANDLED;
}

static irqreturn_t bd71827_temp_vf_res(int irq, void *data)
{
	struct bd71827_power *pwr = (struct bd71827_power *)data;

	dev_info(pwr->mfd->dev, "\n~~~ VF Resumed ...\n");

	return IRQ_HANDLED;
}

static irqreturn_t bd71827_temp_vf125_det(int irq, void *data)
{
	struct bd71827_power *pwr = (struct bd71827_power *)data;

	dev_info(pwr->mfd->dev, "\n~~~ VF125 Detected ...\n");

	return IRQ_HANDLED;
}

static irqreturn_t bd71827_temp_vf125_res(int irq, void *data)
{
	struct bd71827_power *pwr = (struct bd71827_power *)data;

	dev_info(pwr->mfd->dev, "\n~~~ VF125 Resumed ...\n");

	return IRQ_HANDLED;
}

struct bd7182x_irq_res {
	const char *name;
	irq_handler_t handler;
};

#define BDIRQ(na, hn) { .name = (na), .handler = (hn) }

int bd7182x_get_irqs(struct platform_device *pdev, struct bd71827_power *pwr)
{
	int i, irq, ret;
	static const struct bd7182x_irq_res irqs[] = {
		BDIRQ("bd71828-pwr-longpush", bd7182x_long_push),
		BDIRQ("bd71828-pwr-midpush", bd7182x_mid_push),
		BDIRQ("bd71828-pwr-shortpush", bd7182x_short_push),
//		BDIRQ("bd71828-pwr-push", bd7182x_push),
		BDIRQ("bd71828-pwr-dcin-in", bd7182x_dcin_detected),
		BDIRQ("bd71828-pwr-dcin-out", bd7182x_dcin_removed),
		BDIRQ("bd71828-pwr-dcin-ilim", bd7182x_dcin_ilim),
		BDIRQ("bd71828-vbat-normal", bd71827_vbat_low_res),
		BDIRQ("bd71828-vbat-low", bd71827_vbat_low_det),
		BDIRQ("bd71828-btemp-hi", bd71827_temp_bat_hi_det),
		BDIRQ("bd71828-btemp-cool", bd71827_temp_bat_hi_res),
		BDIRQ("bd71828-btemp-lo", bd71827_temp_bat_low_det),
		BDIRQ("bd71828-btemp-warm", bd71827_temp_bat_low_res),
		BDIRQ("bd71828-temp-hi", bd71827_temp_vf_det),
		BDIRQ("bd71828-temp-norm", bd71827_temp_vf_res),
		BDIRQ("bd71828-temp-125-over", bd71827_temp_vf125_det),
		BDIRQ("bd71828-temp-125-under", bd71827_temp_vf125_res),
	};

	for (i = 0; i < ARRAY_SIZE(irqs); i++) {
		irq = platform_get_irq_byname(pdev, irqs[i].name);

		ret = devm_request_threaded_irq(&pdev->dev, irq, NULL,
						irqs[i].handler, 0,
						irqs[i].name, pwr);
		if (ret)
			break;
	}

	return ret;
}

#define RSENS_DEFAULT_30MOHM 30000000

int dt_get_rsens(struct device *dev, int *rsens_ohm)
{
	if (dev->of_node) {
		int ret;
		unsigned int rs;

		ret = of_property_read_u32(dev->of_node,
			"rohm,charger-sense-resistor", &rs);
		if (ret) {
			dev_err(dev, "Bad RSENS dt property\n");
			return ret;
		}

		*rsens_ohm = (int)rs;
	}
	return 0;
}

/* @brief probe pwr device
 * @param pdev platform device of bd71827_power
 * @retval 0 success
 * @retval negative fail
 */
static int bd71827_power_probe(struct platform_device *pdev)
{
//	struct bd71827 *bd71827 = dev_get_drvdata(pdev->dev.parent);
	struct rohm_regmap_dev *mfd;
	struct bd71827_power *pwr;
	struct power_supply_config ac_cfg = {};
	struct power_supply_config bat_cfg = {};
	int ret;
	int rsens_ohm = RSENS_DEFAULT_30MOHM;

	mfd = dev_get_drvdata(pdev->dev.parent);

	pwr = devm_kzalloc(&pdev->dev, sizeof(*pwr), GFP_KERNEL);
	if (!pwr)
		return -ENOMEM;

	pwr->mfd = mfd;
	pwr->mfd->dev = &pdev->dev;
	spin_lock_init(&pwr->dlock);

	ret = dt_get_rsens(pdev->dev.parent, &rsens_ohm);
	if (ret)
		return ret;

	dev_info(pwr->mfd->dev, "RSENS prop found %u\n", rsens_ohm);

	ret = bd7182x_set_chip_specifics(pwr, rsens_ohm);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, pwr);

	if (battery_cycle <= 0)
		battery_cycle = 0;

	battery_stressed = 0;
	dev_info(pwr->mfd->dev, "in probe, battery_cycle = %d, battery_stressed = %d\n", battery_cycle, battery_stressed);

	/* If the product often power up/down and the power down
	 * time is long, the Coulomb Counter may have a drift.
	 */
	/* If so, it may be better accuracy to enable Coulomb Counter
	 * using following commented out code
	 */
	/* for counting Coulomb when the product is power up(including
	 * sleep).
	 */
	/* The condition  */
	/* (1) Product often power up and down, the power down time
	 * is long and there is no power consumed in power down time.
	 */
	/* (2) Kernel must call this routin at power up time. */
	/* (3) Kernel must call this routin at charging time. */
	/* (4) Must use this code with "Stop Coulomb Counter"
	 * code in bd71827_power_remove() function
	 */
	/* Start Coulomb Counter */
	/* bd71827_set_bits(pwr->mfd, pwr->regs->coulomb_ctrl,
	 * BD7182x_MASK_CCNTENB);
	 */

	bd71827_set_battery_parameters(pwr, 0);

	bd71827_init_hardware(pwr, &pwr->d_w);

	bat_cfg.drv_data = pwr;
	pwr->bat = devm_power_supply_register(&pdev->dev, &bd71827_battery_desc,
					 &bat_cfg);
	if (IS_ERR(pwr->bat)) {
		ret = PTR_ERR(pwr->bat);
		dev_err(&pdev->dev, "failed to register bat: %d\n", ret);
		return ret;
	}

	ac_cfg.supplied_to = bd71827_ac_supplied_to;
	ac_cfg.num_supplicants = ARRAY_SIZE(bd71827_ac_supplied_to);
	ac_cfg.drv_data = pwr;
	pwr->ac = devm_power_supply_register(&pdev->dev,
		&bd71827_ac_desc, &ac_cfg);
	if (IS_ERR(pwr->ac)) {
		ret = PTR_ERR(pwr->ac);
		dev_err(&pdev->dev, "failed to register ac: %d\n", ret);
		return ret;
	}

	ret = bd7182x_get_irqs(pdev, pwr);
	if (ret) {
		dev_err(&pdev->dev, "failed to request IRQs: %d\n", ret);
		return ret;
	};

	/* Configure wakeup capable */
	device_set_wakeup_capable(pwr->mfd->dev, 1);
	device_set_wakeup_enable(pwr->mfd->dev, 1);

	ret = sysfs_create_group(&pwr->bat->dev.kobj,
				 &bd71827_sysfs_attr_group);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to register sysfs interface\n");
		return ret;
	}

	INIT_DELAYED_WORK(&pwr->bd_work, bd_work_callback);
	INIT_DELAYED_WORK(&pwr->bd_lobat_check_work, pmic_lobat_check_work);
#ifndef SAFE_CHARGING_CONTROLLED_BY_USERSPASE
	INIT_DELAYED_WORK(&pwr->bd_safe_charging_work, bd_safe_charging_work_callback);
#endif
	INIT_DELAYED_WORK(&pwr->bd_reset_chgint_work, bd_reset_chgint_work_callback);
	INIT_DELAYED_WORK(&pwr->bd_log_save_work, bd_log_save_work_callback);
	INIT_DELAYED_WORK(&pwr->bd_current_ramp_work, bd_current_ramp_work_callback);
	INIT_DELAYED_WORK(&pwr->bd_ilim_dcin_recover_work, bd_ilim_dcin_recover_work_callback);
#ifdef PWRCTRL_HACK
	mutex_init(&pwrlock);
	set_power(pwr);
#endif

	/* Schedule timer to check current status */
	pwr->gauge_delay = 0;
	smp_wmb(); /* wait for sync */
	bd71827_suspend_status = BD71827_RESUME;
	schedule_delayed_work(&pwr->bd_work, msecs_to_jiffies(0));

#ifdef CONFIG_CHARGER_HAL
	pwr->bd71827_pmic_dev = charger_hal_pmic_device_register(&pdev->dev, &bd71827_pmic_ops);
	if (IS_ERR(pwr->bd71827_pmic_dev)) {
		dev_err(&pdev->dev, "failed to register charger hal pmic device\n");
		return PTR_ERR(pwr->bd71827_pmic_dev);
	}
#endif
	pwr->bd71827_cooling_dev = bd71827_cooling_dev_register(
			BD71827_COOLING_DEV, (void *)&bd71827_cooling_dev_state,
			&bd71827_cooling_dev_ops);

	return 0;
}

/* @brief remove pwr device
 * @param pdev platform device of bd71827_power
 * @return 0
 */

static int bd71827_power_remove(struct platform_device *pdev)
{
	struct bd71827_power *pwr = platform_get_drvdata(pdev);

	/* If the product often power up/down and the power down time
	 * is long, the Coulomb Counter may have a drift.
	 */
	/* If so, it may be better accuracy to disable Coulomb Counter
	 * using following commented out code
	 */
	/* for stopping counting Coulomb when the product is power
	 * down(without sleep).
	 */
	/* The condition  */
	/* (1) Product often power up and down, the power down time
	 * is long and there is no power consumed in power down time.
	 */
	/* (2) Kernel must call this routin at power down time. */
	/* (3) Must use this code with "Start Coulomb Counter"
	 * code in bd71827_power_probe() function
	 */
	/* Stop Coulomb Counter */
	/* bd71827_clear_bits(pwr->mfd,
	 * pwr->regs->coulomb_ctrl, BD7182x_MASK_CCNTENB);
	 */

	sysfs_remove_group(&pwr->bat->dev.kobj, &bd71827_sysfs_attr_group);
	cancel_delayed_work(&pwr->bd_work);
	cancel_delayed_work(&pwr->bd_lobat_check_work);

#ifdef PWRCTRL_HACK
	free_power();
#endif
	return 0;
}

/* Register PM ops with SET_SYSTEM_SLEEP_PM_OPS so that driver resumes
   after I2C resumes.I2C PM ops is registered as
   SET_NOIRQ_SYSTEM_SLEEP_PM_OPS
*/
static int bd71827_power_suspend(struct device *dev)
{
	bd71827_suspend_status = BD71827_SUSPEND;
	return 0;
}

static int bd71827_power_resume(struct device *dev)
{
	bd71827_suspend_status = BD71827_RESUME;
	return 0;
}

static const struct dev_pm_ops bd71827_power_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(bd71827_power_suspend,
				      bd71827_power_resume)
};

static struct platform_driver bd71827_power_driver = {
	.driver = {
		.name = "bd71827-power",
		.owner = THIS_MODULE,
		.pm = &bd71827_power_pm_ops,
	},
	.probe = bd71827_power_probe,
	.remove = bd71827_power_remove,
};

module_platform_driver(bd71827_power_driver);

module_param(use_load_bat_params, int, 0444);
MODULE_PARM_DESC(use_load_bat_params, "use_load_bat_params:Use loading battery parameters");

module_param(battery_cap_mah, int, 0444);
MODULE_PARM_DESC(battery_cap_mah, "battery_cap_mah:Battery capacity (mAh)");

module_param(dgrd_cyc_cap, int, 0444);
MODULE_PARM_DESC(dgrd_cyc_cap, "dgrd_cyc_cap:Degraded capacity per cycle (uAh)");

module_param(soc_est_max_num, int, 0444);
MODULE_PARM_DESC(soc_est_max_num, "soc_est_max_num:SOC estimation max repeat number");

module_param(dgrd_temp_cap_h, int, 0444);
MODULE_PARM_DESC(dgrd_temp_cap_h, "dgrd_temp_cap_h:Degraded capacity at high temperature (uAh)");

module_param(dgrd_temp_cap_m, int, 0444);
MODULE_PARM_DESC(dgrd_temp_cap_m, "dgrd_temp_cap_m:Degraded capacity at middle temperature (uAh)");

module_param(dgrd_temp_cap_l, int, 0444);
MODULE_PARM_DESC(dgrd_temp_cap_l, "dgrd_temp_cap_l:Degraded capacity at low temperature (uAh)");

module_param(battery_cycle, uint, 0644);
MODULE_PARM_DESC(battery_parameters, "battery_cycle:battery charge/discharge cycles");

module_param_array(ocv_table, int, NULL, 0444);
MODULE_PARM_DESC(ocv_table, "ocv_table:Open Circuit Voltage table (uV)");

module_param_array(vdr_table_h, int, NULL, 0444);
MODULE_PARM_DESC(vdr_table_h, "vdr_table_h:Voltage Drop Ratio temperatyre high area table");

module_param_array(vdr_table_m, int, NULL, 0444);
MODULE_PARM_DESC(vdr_table_m, "vdr_table_m:Voltage Drop Ratio temperatyre middle area table");

module_param_array(vdr_table_l, int, NULL, 0444);
MODULE_PARM_DESC(vdr_table_l, "vdr_table_l:Voltage Drop Ratio temperatyre low area table");

module_param_array(vdr_table_vl, int, NULL, 0444);
MODULE_PARM_DESC(vdr_table_vl, "vdr_table_vl:Voltage Drop Ratio temperatyre very low area table");

MODULE_AUTHOR("Cong Pham <cpham2403@gmail.com>");
MODULE_DESCRIPTION("ROHM BD71827/BD71828 PMIC Battery Charger driver");
MODULE_LICENSE("GPL");
