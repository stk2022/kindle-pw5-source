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

#ifndef __LINUX_POWER_RT9426_BATTERY_H
#define __LINUX_POWER_RT9426_BATTERY_H

#define RT9426_DRIVER_VER	0x0001

#define RT9426_Unseal_Key	0x12345678

#define RT9426_REG_CNTL		0x00
#define RT9426_REG_RSVD_Flag	0x02
#define RT9426_REG_CURR		0x04
#define RT9426_REG_TEMP		0x06
#define RT9426_REG_VBAT		0x08
#define RT9426_REG_FLAG1	0x0A
#define RT9426_REG_FLAG2	0x0C
#define RT9426_REG_RM		0x10
#define RT9426_REG_FCC		0x12
#define RT9426_REG_AI		0x14
#define RT9426_REG_TTE		0x16
#define RT9426_REG_DUMMY	0x1E
#define RT9426_REG_VER		0x20
#define RT9426_REG_VGCOMP12	0x24
#define RT9426_REG_VGCOMP34	0x26
#define RT9426_REG_INTT		0x28
#define RT9426_REG_CYC		0x2A
#define RT9426_REG_SOC		0x2C
#define RT9426_REG_SOH		0x2E
#define RT9426_REG_FLAG3	0x30
#define RT9426_REG_IRQ		0x36
#define RT9426_REG_DSNCAP	0x3C
#define RT9426_REG_EXTREGCNTL	0x3E
#define RT9426_REG_SWINDOW1	0x40
#define RT9426_REG_SWINDOW2	0x42
#define RT9426_REG_SWINDOW3	0x44
#define RT9426_REG_SWINDOW4	0x46
#define RT9426_REG_SWINDOW5	0x48
#define RT9426_REG_SWINDOW6	0x4A
#define RT9426_REG_SWINDOW7	0x4C
#define RT9426_REG_SWINDOW8	0x4E
#define RT9426_REG_SWINDOW9	0x50
#define RT9426_REG_SWINDOW10	0x52
#define RT9426_REG_SWINDOW11	0x54
#define RT9426_REG_SWINDOW12	0x56
#define RT9426_REG_SWINDOW13	0x58
#define RT9426_REG_SWINDOW14	0x5A
#define RT9426_REG_SWINDOW15	0x5C
#define RT9426_REG_SWINDOW16	0x5E
#define RT9426_REG_OCV		0x62
#define RT9426_REG_AV		0x64
#define RT9426_REG_AT		0x66
#define RT9426_REG_UN_FLT_SOC	0x70

#ifdef CONFIG_DEBUG_FS
enum {
	RT9426FG_SOC_OFFSET_SIZE,
	RT9426FG_SOC_OFFSET_DATA,
	RT9426FG_PARAM_LOCK,
	RT9426FG_OFFSET_IP_ORDER,
	RT9426FG_FIND_OFFSET_TEST,
	RT9426FG_PARAM_CHECK,
	RT9426FG_DENTRY_NR,
};
#endif /* CONFIG_DEBUG_FS */

#define RT9426_BATPRES_MASK		0x0040
#define RT9426_RI_MASK			0x0100
#define RT9426_BATEXIST_FLAG_MASK	0x8000
#define RT9426_USR_TBL_USED_MASK	0x0800
#define RT9426_CSCOMP1_OCV_MASK		0x0300
#define RT9426_UNSEAL_MASK		0x0003
#define RT9426_UNSEAL_STATUS		0x0001

#define RT9426_SMOOTH_POLL	20
#define RT9426_NORMAL_POLL	30
#define RT9426_SOCALRT_MASK	0x20
#define RT9426_SOCL_SHFT	0
#define RT9426_SOCL_MASK	0x1F
#define RT9426_SOCL_MAX		32
#define RT9426_SOCL_MIN		1

#define RT9426_RDY_MASK		0x0080

#define RT9426_UNSEAL_PASS	0
#define RT9426_UNSEAL_FAIL	1
#define RT9426_PAGE_0	0
#define RT9426_PAGE_1	1
#define RT9426_PAGE_2	2
#define RT9426_PAGE_3	3
#define RT9426_PAGE_4	4
#define RT9426_PAGE_5	5
#define RT9426_PAGE_6	6
#define RT9426_PAGE_7	7
#define RT9426_PAGE_8	8
#define RT9426_PAGE_9	9

struct data_point {
	union {
		int x;
		int voltage;
		int soc;
	};
	union {
		int y;
		int temperature;
	};
	union {
		int z;
		int curr;
	};
	union {
		int w;
		int offset;
	};
};

struct extreg_data_point {
	union {
		int extreg_page;
	};
	union {
		int extreg_addr;
	};
	union {
		int extreg_data;
	};
};

struct extreg_update_table {
	struct extreg_data_point *extreg_update_data;
};

struct soc_offset_table {
	int soc_voltnr;
	int tempnr;
	struct data_point *soc_offset_data;
};

enum { /* temperature source table */
	RT9426_TEMP_FROM_AP,
	RT9426_TEMP_FROM_IC,
};

struct fg_ocv_table {
	int data[8];
};

struct rt9426_platform_data {
	int soc_offset_size[2];
	struct soc_offset_table soc_offset;
	int extreg_size;
	struct extreg_update_table extreg_update;
	int offset_interpolation_order[2];
	struct fg_ocv_table ocv_table[10];

	char *bat_name;
	int boot_gpio;
	int chg_sts_gpio;
	int chg_inh_gpio;
	int chg_done_gpio;
	int design_capacity;
	int fcc;
	int battery_type;
	u32 uv_ov_threshold;
	u32 us_threshold;
	u32 otc_tth;
	u32 otc_chg_ith;
	u32 otd_tth;
	u32 otd_dchg_ith;
	u32 curr_db;

	u32 dtsi_version[2];
	u32 op_config[5];
	u32 fc_vth;
	u32 fc_ith;
	u32 fc_sth;
	u32 fd_vth;

	u32 temp_source;
	u32 volt_source;
	u32 curr_source;
};
#endif /* __LINUX_POWER_RT9426_BATTERY_H */
