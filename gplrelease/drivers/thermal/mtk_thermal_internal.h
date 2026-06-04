/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef _THERMAL_INTERNAL_H
#define _THERMAL_INTERNAL_H

// APB Module pericfg
#define PERICFG_BASE	 (0x10003000)
// APB Module infracfg_ao
#define INFRACFG_AO_BASE (0x10001000)
// APB Module auxadc
#define AUXADC_BASE		 (0x11001000)
// APB Module therm_ctrl
#define THERM_CTRL_BASE	 (0x1100B000)
// APB Module apmixed
#define APMIXED_BASE	 (0x1000C000)

#define DRV_WriteReg32(addr, value)     (*(volatile unsigned int *)(addr) = (value))
#define WRITE_REG(val,addr)    ((*(volatile unsigned int *)(addr)) = (unsigned int)val)
#define WRITE_REG_W(val,addr)	WRITE_REG(val,addr)
#define mt65xx_reg_sync_writel(v, a) \
	do {    \
		WRITE_REG_W((v), (a));   \
		} while (0)

typedef enum {
	THERMAL_BANK0 = 0,
	THERMAL_BANK3 = 1,
	THERMAL_BANK4 = 2,
	THERMAL_BANK_NUM
} thermal_bank_name;

struct TS_PTPOD
{
	unsigned int ts_MTS;
	unsigned int ts_BTS;
};

extern int thermal_init(void);
extern void thermal_exit(void);
extern int mtktscpu_get_hw_temp(void);
extern int lvts_get_hw_temp(void);

extern void get_thermal_slope_intercept(struct TS_PTPOD *ts_info,thermal_bank_name ts_bank);

/*=============================================================
 * Structure Definition
 *=============================================================
 */
/* MT6771
 * Bank0	CA7LL		TSMCU4
 * Bank1	CA7BL		TSMCU5
 * Bank2	CCI			TSMCU4+5
 * Bank3	GPU		TSMCU2
 * Bank4	SoC+MD1	TSMCU1 + TSMCU3
 */
/*
 * TC0: 0x1100B000		(TS_MCU5, TS_MCU4, X, X)
 * TC1: 0x1100B100		(TS_MCU1, TS_MCU2, TS_MCU3, TS_ABB)
 */
typedef enum {
	THERMAL_CONTROLLER0 = 0,
	THERMAL_CONTROLLER_NUM
} thermal_controller_name;

typedef enum thermal_sensor_enum {
	TS_MCU1 = 0,
	TS_MCU2,
	TS_ENUM_MAX,
} ts_e;

struct thermal_controller_speed {
        unsigned int tempMonCtl1;
        unsigned int tempMonCtl2;
        unsigned int tempAhbPoll;
};

struct thermal_controller {
        ts_e ts[TS_ENUM_MAX];
        unsigned int ts_number;
        unsigned int tc_offset;
        struct thermal_controller_speed tc_speed;
};

/* TSCON1 bit table */
#define TSCON0_bit_6_7_00 0x00  /* TSCON0[7:6]=2'b00*/
#define TSCON0_bit_6_7_01 0x40  /* TSCON0[7:6]=2'b01*/
#define TSCON0_bit_6_7_10 0x80  /* TSCON0[7:6]=2'b10*/
#define TSCON0_bit_6_7_11 0xc0  /* TSCON0[7:6]=2'b11*/
#define TSCON0_bit_6_7_MASK 0xc0

#define TSCON1_bit_4_5_00 0x00  /* TSCON1[5:4]=2'b00*/
#define TSCON1_bit_4_5_01 0x10  /* TSCON1[5:4]=2'b01*/
#define TSCON1_bit_4_5_10 0x20  /* TSCON1[5:4]=2'b10*/
#define TSCON1_bit_4_5_11 0x30  /* TSCON1[5:4]=2'b11*/
#define TSCON1_bit_4_5_MASK 0x30

#define TSCON1_bit_0_2_000 0x00  /*TSCON1[2:0]=3'b000*/
#define TSCON1_bit_0_2_001 0x01  /*TSCON1[2:0]=3'b001*/
#define TSCON1_bit_0_2_010 0x02  /*TSCON1[2:0]=3'b010*/
#define TSCON1_bit_0_2_011 0x03  /*TSCON1[2:0]=3'b011*/
#define TSCON1_bit_0_2_100 0x04  /*TSCON1[2:0]=3'b100*/
#define TSCON1_bit_0_2_101 0x05  /*TSCON1[2:0]=3'b101*/
#define TSCON1_bit_0_2_110 0x06  /*TSCON1[2:0]=3'b110*/
#define TSCON1_bit_0_2_111 0x07  /*TSCON1[2:0]=3'b111*/
#define TSCON1_bit_0_2_MASK 0x07

#define TSCON0_bit_29_28_00   0x00000000  /* TSCON0[29:28]=2'b00*/
#define TSCON0_bit_29_28_01   0x10000000  /* TSCON0[29:28]=2'b01*/
#define TSCON0_bit_29_28_10   0x20000000  /* TSCON0[29:28]=2'b10*/
#define TSCON0_bit_29_28_11   0x30000000  /* TSCON0[29:28]=2'b11*/

#define TSCON0_bit_29_28_MASK 0x30000000


#if 1 //Cervino
/* ADC value to mcu */
/*chip dependent*/
#define TEMPADC_MCU1    ((TSCON0_bit_29_28_MASK&TSCON0_bit_29_28_00)|(0x07&TSCON1_bit_0_2_000))
#define TEMPADC_MCU2    ((TSCON0_bit_29_28_MASK&TSCON0_bit_29_28_00)|(0x07&TSCON1_bit_0_2_001))
#define TEMPADC_MCU3    ((TSCON0_bit_29_28_MASK&TSCON0_bit_29_28_00)|(0x07&TSCON1_bit_0_2_010))
#define TEMPADC_MCU4    ((TSCON0_bit_29_28_MASK&TSCON0_bit_29_28_00)|(0x07&TSCON1_bit_0_2_011))
#define TEMPADC_MCU5    ((TSCON0_bit_29_28_MASK&TSCON0_bit_29_28_00)|(0x07&TSCON1_bit_0_2_100))

#define TEMPADC_ABB     ((TSCON0_bit_29_28_MASK&TSCON0_bit_29_28_01)|(0x07&TSCON1_bit_0_2_000))
#else
/* ADC value to mcu */
/*chip dependent*/
#define TEMPADC_MCU1    ((0x30&TSCON1_bit_4_5_00)|(0x07&TSCON1_bit_0_2_000))
#define TEMPADC_MCU2    ((0x30&TSCON1_bit_4_5_00)|(0x07&TSCON1_bit_0_2_001))
#define TEMPADC_MCU3    ((0x30&TSCON1_bit_4_5_00)|(0x07&TSCON1_bit_0_2_010))
#define TEMPADC_MCU4    ((0x30&TSCON1_bit_4_5_00)|(0x07&TSCON1_bit_0_2_011))
#define TEMPADC_MCU5    ((0x30&TSCON1_bit_4_5_00)|(0x07&TSCON1_bit_0_2_100))

#define TEMPADC_ABB     ((0x30&TSCON1_bit_4_5_01)|(0x07&TSCON1_bit_0_2_000))
#endif
/*=============================================================
 * Macro
 *=============================================================
 */
#define THERMAL_HARDWARE_RESET_POINT (117000)

#ifdef __MTK_SLT_
#define THERMAL_LOG printf
#else
#define THERMAL_LOG printf
#endif

#define THERMAL_CRTI_LOG //dbg_print

#define _BIT_(_bit_)		(unsigned)(1 << (_bit_))
#define _BITMASK_(_bits_)	(((unsigned) -1 >> (31 - ((1) ? _bits_))) & ~((1U << ((0) ? _bits_)) - 1))
#define ARRAY_SIZE(x)   sizeof(x)/sizeof(x[0])

#define MIN(_a_, _b_) ((_a_) > (_b_) ? (_b_) : (_a_))
#define MAX(_a_, _b_) ((_a_) > (_b_) ? (_a_) : (_b_))

#define THERMAL_WRAP_WR32(val,addr)        mt65xx_reg_sync_writel((val), ((void *)addr))

#define thermal_readl(addr)         DRV_Reg32(addr)
#define thermal_writel(addr, val)   mt65xx_reg_sync_writel((val), ((void *)addr))
#define thermal_setl(addr, val)     mt65xx_reg_sync_writel(thermal_readl(addr) | (val), ((void *)addr))
#define thermal_clrl(addr, val)     mt65xx_reg_sync_writel(thermal_readl(addr) & ~(val), ((void *)addr))
/*=============================================================
 * extern value
 *=============================================================
 */
extern struct thermal_controller tscpu_g_tc[THERMAL_CONTROLLER_NUM];

/*=============================================================
 * Peripheral Configuration Register Definition
 *=============================================================
 */
#define PERI_GLOBALCON_RST0 (PERICFG_BASE + 0x000)
// APB Module infracfg_ao
#define INFRA_GLOBALCON_RST_0_SET (INFRACFG_AO_BASE + 0x120) //0x10001000
#define INFRA_GLOBALCON_RST_0_CLR (INFRACFG_AO_BASE + 0x124) //0x10001000
#define INFRA_GLOBALCON_RST_0_STA (INFRACFG_AO_BASE + 0x128) //0x10001000

/*=============================================================
 * Clock Gate Definition
 *=============================================================
 */
/*
 * CLK_INFRA_THERM_BCLK at bit 10
 * STA => Check the clock status. 0: clock on, 1: clock off
 * SET => Write 1 to disable the clock (enable clock gating)
 * CLR => Write 1 to enable the clock
 */
#define THERM_MODULE_SW_CG_0_STA (INFRACFG_AO_BASE + 0x0090)
#define THERM_MODULE_SW_CG_0_SET (INFRACFG_AO_BASE + 0x0080)
#define THERM_MODULE_SW_CG_0_CLR (INFRACFG_AO_BASE + 0x0084)

/*=============================================================
 * AUXADC Register Definition
 *=============================================================
 */
#define AUXADC_CON0         (AUXADC_BASE + 0x000)
#define AUXADC_CON1         (AUXADC_BASE + 0x004)
#define AUXADC_CON1_SET     (AUXADC_BASE + 0x008)
#define AUXADC_CON1_CLR     (AUXADC_BASE + 0x00C)
#define AUXADC_CON2         (AUXADC_BASE + 0x010)
#define AUXADC_CON3         (AUXADC_BASE + 0x014)
#define AUXADC_DAT0         (AUXADC_BASE + 0x018)
#define AUXADC_DAT1         (AUXADC_BASE + 0x01C)
#define AUXADC_DAT2         (AUXADC_BASE + 0x020)
#define AUXADC_DAT3         (AUXADC_BASE + 0x024)
#define AUXADC_DAT4         (AUXADC_BASE + 0x028)
#define AUXADC_DAT5         (AUXADC_BASE + 0x02C)
#define AUXADC_MISC         (AUXADC_BASE + 0x094)
#define AUXADC_DAT11	    (AUXADC_BASE + 0x040)

//APMIXED_BASE related with row data convert document( CY Chien)
#define TS_CON0             (APMIXED_BASE + 0x600)
#define TS_CON1             (APMIXED_BASE + 0x604)
/*=============================================================
 * Thermal Controller Register Definition
 *=============================================================
 */

#define TEMPMONCTL0         (THERM_CTRL_BASE + 0x000)
#define TEMPMONCTL1         (THERM_CTRL_BASE + 0x004)
#define TEMPMONCTL2         (THERM_CTRL_BASE + 0x008)
#define TEMPMONINT          (THERM_CTRL_BASE + 0x00C)
#define TEMPMONINTSTS       (THERM_CTRL_BASE + 0x010)
#define TEMPMONIDET0        (THERM_CTRL_BASE + 0x014)
#define TEMPMONIDET1        (THERM_CTRL_BASE + 0x018)
#define TEMPMONIDET2        (THERM_CTRL_BASE + 0x01C)
#define TEMPH2NTHRE         (THERM_CTRL_BASE + 0x024)
#define TEMPHTHRE           (THERM_CTRL_BASE + 0x028)
#define TEMPCTHRE           (THERM_CTRL_BASE + 0x02C)
#define TEMPOFFSETH         (THERM_CTRL_BASE + 0x030)
#define TEMPOFFSETL         (THERM_CTRL_BASE + 0x034)
#define TEMPMSRCTL0         (THERM_CTRL_BASE + 0x038)
#define TEMPMSRCTL1         (THERM_CTRL_BASE + 0x03C)
#define TEMPAHBPOLL         (THERM_CTRL_BASE + 0x040)
#define TEMPAHBTO           (THERM_CTRL_BASE + 0x044)
#define TEMPADCPNP0         (THERM_CTRL_BASE + 0x048)
#define TEMPADCPNP1         (THERM_CTRL_BASE + 0x04C)
#define TEMPADCPNP2         (THERM_CTRL_BASE + 0x050)

#define TEMPADCMUX          (THERM_CTRL_BASE + 0x054)
#define TEMPADCEXT          (THERM_CTRL_BASE + 0x058)
#define TEMPADCEXT1         (THERM_CTRL_BASE + 0x05C)
#define TEMPADCEN           (THERM_CTRL_BASE + 0x060)
#define TEMPPNPMUXADDR      (THERM_CTRL_BASE + 0x064)
#define TEMPADCMUXADDR      (THERM_CTRL_BASE + 0x068)
#define TEMPADCEXTADDR      (THERM_CTRL_BASE + 0x06C)
#define TEMPADCEXT1ADDR     (THERM_CTRL_BASE + 0x070)
#define TEMPADCENADDR       (THERM_CTRL_BASE + 0x074)
#define TEMPADCVALIDADDR    (THERM_CTRL_BASE + 0x078)
#define TEMPADCVOLTADDR     (THERM_CTRL_BASE + 0x07C)
#define TEMPRDCTRL          (THERM_CTRL_BASE + 0x080)
#define TEMPADCVALIDMASK    (THERM_CTRL_BASE + 0x084)
#define TEMPADCVOLTAGESHIFT (THERM_CTRL_BASE + 0x088)
#define TEMPADCWRITECTRL    (THERM_CTRL_BASE + 0x08C)
#define TEMPMSR0            (THERM_CTRL_BASE + 0x090)
#define TEMPMSR1            (THERM_CTRL_BASE + 0x094)
#define TEMPMSR2            (THERM_CTRL_BASE + 0x098)

#define TEMPADCHADDR        (THERM_CTRL_BASE + 0x09C)

#define TEMPIMMD0           (THERM_CTRL_BASE + 0x0A0)
#define TEMPIMMD1           (THERM_CTRL_BASE + 0x0A4)
#define TEMPIMMD2           (THERM_CTRL_BASE + 0x0A8)

#define TEMPMONIDET3        (THERM_CTRL_BASE + 0x0B0)
#define TEMPADCPNP3         (THERM_CTRL_BASE + 0x0B4)
#define TEMPMSR3            (THERM_CTRL_BASE + 0x0B8)
#define TEMPIMMD3           (THERM_CTRL_BASE + 0x0BC)

#define TEMPPROTCTL         (THERM_CTRL_BASE + 0x0C0)
#define TEMPPROTTA          (THERM_CTRL_BASE + 0x0C4)
#define TEMPPROTTB          (THERM_CTRL_BASE + 0x0C8)
#define TEMPPROTTC          (THERM_CTRL_BASE + 0x0CC)

#define TEMPSPARE0          (THERM_CTRL_BASE + 0x0F0)
#define TEMPSPARE1          (THERM_CTRL_BASE + 0x0F4)
#define TEMPSPARE2          (THERM_CTRL_BASE + 0x0F8)
#define TEMPSPARE3          (THERM_CTRL_BASE + 0x0FC)



#define TEMPMONCTL0_1         (THERM_CTRL_BASE + 0x100)
#define TEMPMONCTL1_1         (THERM_CTRL_BASE + 0x104)
#define TEMPMONCTL2_1         (THERM_CTRL_BASE + 0x108)
#define TEMPMONINT_1          (THERM_CTRL_BASE + 0x10C)
#define TEMPMONINTSTS_1       (THERM_CTRL_BASE + 0x110)
#define TEMPMONIDET0_1        (THERM_CTRL_BASE + 0x114)
#define TEMPMONIDET1_1        (THERM_CTRL_BASE + 0x118)
#define TEMPMONIDET2_1        (THERM_CTRL_BASE + 0x11C)
#define TEMPH2NTHRE_1         (THERM_CTRL_BASE + 0x124)
#define TEMPHTHRE_1           (THERM_CTRL_BASE + 0x128)
#define TEMPCTHRE_1           (THERM_CTRL_BASE + 0x12C)
#define TEMPOFFSETH_1         (THERM_CTRL_BASE + 0x130)
#define TEMPOFFSETL_1         (THERM_CTRL_BASE + 0x134)
#define TEMPMSRCTL0_1         (THERM_CTRL_BASE + 0x138)
#define TEMPMSRCTL1_1         (THERM_CTRL_BASE + 0x13C)
#define TEMPAHBPOLL_1         (THERM_CTRL_BASE + 0x140)
#define TEMPAHBTO_1           (THERM_CTRL_BASE + 0x144)
#define TEMPADCPNP0_1         (THERM_CTRL_BASE + 0x148)
#define TEMPADCPNP1_1         (THERM_CTRL_BASE + 0x14C)
#define TEMPADCPNP2_1         (THERM_CTRL_BASE + 0x150)

#define TEMPADCMUX_1          (THERM_CTRL_BASE + 0x154)
#define TEMPADCEXT_1          (THERM_CTRL_BASE + 0x158)
#define TEMPADCEXT1_1         (THERM_CTRL_BASE + 0x15C)
#define TEMPADCEN_1           (THERM_CTRL_BASE + 0x160)
#define TEMPPNPMUXADDR_1      (THERM_CTRL_BASE + 0x164)
#define TEMPADCMUXADDR_1      (THERM_CTRL_BASE + 0x168)
#define TEMPADCEXTADDR_1      (THERM_CTRL_BASE + 0x16C)
#define TEMPADCEXT1ADDR_1     (THERM_CTRL_BASE + 0x170)
#define TEMPADCENADDR_1       (THERM_CTRL_BASE + 0x174)
#define TEMPADCVALIDADDR_1    (THERM_CTRL_BASE + 0x178)
#define TEMPADCVOLTADDR_1     (THERM_CTRL_BASE + 0x17C)
#define TEMPRDCTRL_1          (THERM_CTRL_BASE + 0x180)
#define TEMPADCVALIDMASK_1    (THERM_CTRL_BASE + 0x184)
#define TEMPADCVOLTAGESHIFT_1 (THERM_CTRL_BASE + 0x188)
#define TEMPADCWRITECTRL_1    (THERM_CTRL_BASE + 0x18C)
#define TEMPMSR0_1            (THERM_CTRL_BASE + 0x190)
#define TEMPMSR1_1            (THERM_CTRL_BASE + 0x194)
#define TEMPMSR2_1            (THERM_CTRL_BASE + 0x198)
#define TEMPADCHADDR_1        (THERM_CTRL_BASE + 0x19C)

#define TEMPIMMD0_1           (THERM_CTRL_BASE + 0x1A0)
#define TEMPIMMD1_1           (THERM_CTRL_BASE + 0x1A4)
#define TEMPIMMD2_1           (THERM_CTRL_BASE + 0x1A8)

#define TEMPMONIDET3_1        (THERM_CTRL_BASE + 0x1B0)
#define TEMPADCPNP3_1         (THERM_CTRL_BASE + 0x1B4)
#define TEMPMSR3_1            (THERM_CTRL_BASE + 0x1B8)
#define TEMPIMMD3_1           (THERM_CTRL_BASE + 0x1BC)

#define TEMPPROTCTL_1         (THERM_CTRL_BASE + 0x1C0)
#define TEMPPROTTA_1          (THERM_CTRL_BASE + 0x1C4)
#define TEMPPROTTB_1          (THERM_CTRL_BASE + 0x1C8)
#define TEMPPROTTC_1          (THERM_CTRL_BASE + 0x1CC)

#define TEMPSPARE0_1          (THERM_CTRL_BASE + 0x1F0)
#define TEMPSPARE1_1          (THERM_CTRL_BASE + 0x1F4)
#define TEMPSPARE2_1          (THERM_CTRL_BASE + 0x1F8)
#define TEMPSPARE3_1          (THERM_CTRL_BASE + 0x1FC)

/* LVTS related registers. */
#define TEMPMONCTL0_2         (THERM_CTRL_BASE + 0x800)
#define TEMPMONCTL1_2         (THERM_CTRL_BASE + 0x804)
#define TEMPMONCTL2_2         (THERM_CTRL_BASE + 0x808)
#define TEMPMONINT_2          (THERM_CTRL_BASE + 0x80C)
#define TEMPMONINTSTS_2       (THERM_CTRL_BASE + 0x810)
#define TEMPMONIDET0_2        (THERM_CTRL_BASE + 0x814)
#define TEMPMONIDET1_2        (THERM_CTRL_BASE + 0x818)
#define TEMPMONIDET2_2        (THERM_CTRL_BASE + 0x81C)
#define LVTSMONIDET3_0			(THERM_CTRL_BASE + 0x820)
#define TEMPH2NTHRE_2         (THERM_CTRL_BASE + 0x824)
#define TEMPHTHRE_2           (THERM_CTRL_BASE + 0x828)
#define TEMPCTHRE_2           (THERM_CTRL_BASE + 0x82C)
#define TEMPOFFSETH_2         (THERM_CTRL_BASE + 0x830)
#define TEMPOFFSETL_2         (THERM_CTRL_BASE + 0x834)
#define TEMPMSRCTL0_2         (THERM_CTRL_BASE + 0x838)
#define TEMPMSRCTL1_2         (THERM_CTRL_BASE + 0x83C)
#define TEMPAHBPOLL_2         (THERM_CTRL_BASE + 0x840)
#define TEMPAHBTO_2           (THERM_CTRL_BASE + 0x844)
#define LVTSCALSCALE_0			(THERM_CTRL_BASE + 0x848)
#define LVTS_ID_0				(THERM_CTRL_BASE + 0x84C)

#define TS_CONFIG             (THERM_CTRL_BASE + 0x850)
#define TS0_RDATA             (THERM_CTRL_BASE + 0x854)
#define TS1_RDATA             (THERM_CTRL_BASE + 0x858)
#define TS2_RDATA             (THERM_CTRL_BASE + 0x85C)
#define TS3_RDATA             (THERM_CTRL_BASE + 0x860)
#define LVTSEDATA04_0			(THERM_CTRL_BASE + 0x864)
#define LVTSEDATA10_0			(THERM_CTRL_BASE + 0x868)
#define LVTSEDATA11_0			(THERM_CTRL_BASE + 0x86C)
#define LVTSEDATA12_0			(THERM_CTRL_BASE + 0x870)
#define LVTSEDATA13_0			(THERM_CTRL_BASE + 0x874)
#define LVTSEDATA14_0			(THERM_CTRL_BASE + 0x878)
#define LVTSEDATA20_0			(THERM_CTRL_BASE + 0x87C)
#define LVTSEDATA21_0			(THERM_CTRL_BASE + 0x880)
#define LVTSEDATA22_0			(THERM_CTRL_BASE + 0x884)
#define LVTSEDATA23_0			(THERM_CTRL_BASE + 0x888)
#define LVTSEDATA24_0			(THERM_CTRL_BASE + 0x88C)

#define TEMPMSR0_2            (THERM_CTRL_BASE + 0x890)
#define TEMPMSR1_2            (THERM_CTRL_BASE + 0x894)
#define TEMPMSR2_2            (THERM_CTRL_BASE + 0x898)
#define TEMPMSR3_2				(THERM_CTRL_BASE + 0x89C)

#define TEMPIMMD0_2           (THERM_CTRL_BASE + 0x8A0)
#define TEMPIMMD1_2           (THERM_CTRL_BASE + 0x8A4)
#define TEMPIMMD2_2           (THERM_CTRL_BASE + 0x8A8)
#define LVTSIMMD3_0				(THERM_CTRL_BASE + 0x8AC)

#define LVTSRDATA0_0			(THERM_CTRL_BASE + 0x8B0)
#define LVTSRDATA1_0			(THERM_CTRL_BASE + 0x8B4)
#define LVTSRDATA2_0			(THERM_CTRL_BASE + 0x8B8)
#define LVTSRDATA3_0			(THERM_CTRL_BASE + 0x8BC)

#define TEMPPROTCTL_2         (THERM_CTRL_BASE + 0x8C0)
#define TEMPPROTTA_2          (THERM_CTRL_BASE + 0x8C4)
#define TEMPPROTTB_2          (THERM_CTRL_BASE + 0x8C8)
#define TEMPPROTTC_2          (THERM_CTRL_BASE + 0x8CC)

#define LVTSEDATA30_0			(THERM_CTRL_BASE + 0x8D0)
#define LVTSEDATA31_0			(THERM_CTRL_BASE + 0x8D4)
#define LVTSEDATA32_0			(THERM_CTRL_BASE + 0x8D8)
#define LVTSEDATA33_0			(THERM_CTRL_BASE + 0x8DC)
#define LVTSEDATA34_0			(THERM_CTRL_BASE + 0x8E0)

#define TEMPSPARE0_2          (THERM_CTRL_BASE + 0x8F0)
#define TEMPSPARE1_2          (THERM_CTRL_BASE + 0x8F4)
#define TEMPSPARE2_2          (THERM_CTRL_BASE + 0x8F8)
#define TEMPSPARE3_2          (THERM_CTRL_BASE + 0x8FC)

#define PTPCORESEL            (THERM_CTRL_BASE + 0xF00)
#define THERMINTST            (THERM_CTRL_BASE + 0xF04)
#define PTPODINTST            (THERM_CTRL_BASE + 0xF08)
#define THSTAGE0ST            (THERM_CTRL_BASE + 0xF0C)
#define THSTAGE1ST            (THERM_CTRL_BASE + 0xF10)
#define THSTAGE2ST            (THERM_CTRL_BASE + 0xF14)
#define THAHBST0              (THERM_CTRL_BASE + 0xF18)
#define THAHBST1              (THERM_CTRL_BASE + 0xF1C)
#define PTPSPARE0             (THERM_CTRL_BASE + 0xF20)
#define PTPSPARE1             (THERM_CTRL_BASE + 0xF24)
#define PTPSPARE2             (THERM_CTRL_BASE + 0xF28)
#define PTPSPARE3             (THERM_CTRL_BASE + 0xF2C)
#define THSLPEVEB             (THERM_CTRL_BASE + 0xF30)

#define LVTS0VRCTL				(THERM_CTRL_BASE + 0xF40)
#define LVTS0VREF0				(THERM_CTRL_BASE + 0xF44)
#define LVTS0VREF1				(THERM_CTRL_BASE + 0xF48)
#define LVTS0VREF2				(THERM_CTRL_BASE + 0xF4C)
#define LVTS0VRDBG				(THERM_CTRL_BASE + 0xF50)
/*=============================================================
 * Thermal Controller Register Mask Definition
 *=============================================================
 */
#define THERMAL_ENABLE_SEN0     0x1
#define THERMAL_ENABLE_SEN1     0x2
#define THERMAL_ENABLE_SEN2     0x4
#define THERMAL_MONCTL0_MASK    0x00000007

#define THERMAL_PUNT_MASK       0x00000FFF
#define THERMAL_FSINTVL_MASK    0x03FF0000
#define THERMAL_SPINTVL_MASK    0x000003FF
#define THERMAL_MON_INT_MASK    0x0007FFFF

#define THERMAL_MON_CINTSTS0    0x00000001
#define THERMAL_MON_HINTSTS0    0x00000002
#define THERMAL_MON_LOINTSTS0   0x00000004
#define THERMAL_MON_HOINTSTS0   0x00000008
#define THERMAL_MON_NHINTSTS0   0x00000010

#define THERMAL_MON_CINTSTS1    0x00000020
#define THERMAL_MON_HINTSTS1    0x00000040
#define THERMAL_MON_LOINTSTS1   0x00000080
#define THERMAL_MON_HOINTSTS1   0x00000100
#define THERMAL_MON_NHINTSTS1   0x00000200

#define THERMAL_MON_CINTSTS2    0x00000400
#define THERMAL_MON_HINTSTS2    0x00000800
#define THERMAL_MON_LOINTSTS2   0x00001000
#define THERMAL_MON_HOINTSTS2   0x00002000
#define THERMAL_MON_NHINTSTS2   0x00004000

#define THERMAL_MON_TOINTSTS    0x00008000
#define THERMAL_MON_IMMDINTSTS0 0x00010000
#define THERMAL_MON_IMMDINTSTS1 0x00020000
#define THERMAL_MON_IMMDINTSTS2 0x00040000
#define THERMAL_MON_FILTINTSTS0 0x00080000
#define THERMAL_MON_FILTINTSTS1 0x00100000
#define THERMAL_MON_FILTINTSTS2 0x00200000

#define THERMAL_MON_CINTSTS3    0x00400000
#define THERMAL_MON_HINTSTS3    0x00800000
#define THERMAL_MON_LOINTSTS3   0x01000000
#define THERMAL_MON_HOINTSTS3   0x02000000
#define THERMAL_MON_NHINTSTS3   0x04000000

#define THERMAL_MON_IMMDINTSTS3 0x08000000
#define THERMAL_MON_FILTINTSTS3 0x10000000

#define THERMAL_MSRCTL0_MASK    0x00000007
#define THERMAL_MSRCTL1_MASK    0x00000038
#define THERMAL_MSRCTL2_MASK    0x000001C0


#define THM_AUXADC_DAT0                     (AUXADC_BASE + 0x014)
#define THM_AUXADC_CONFIG1                     (AUXADC_BASE + 0x004)
#define THM_AUXADC_CONFIG2                     (AUXADC_BASE + 0x010)

#define THM_AUXADC_DAT0                     (AUXADC_BASE + 0x014)



#endif


