// SPDX-License-Identifier: GPL-2.0
/*
 * Thermal read temperature for MediaTek MT8110 SoC
 *
 * Copyright (C) 2020 MediaTek Inc.
 * Author: Michael Kao <michael.kao@mediatek.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <common.h>
#include <config.h>
#include <command.h>
#include <dm.h>
#include <errno.h>
#include <div64.h>
#include <dm/lists.h>
#include <dm/device-internal.h>
#include <dm/uclass-internal.h>
#include <clk.h>
#include <clk-uclass.h>

#include "mtk_thermal_internal.h"

typedef signed int		kal_int32;
typedef unsigned int		kal_uint32;
typedef unsigned short		kal_uint16;

typedef signed int		S32;
typedef unsigned int		U32;

#define UINT32			volatile unsigned int
#define DRV_Reg16(addr)	(*(volatile kal_uint16 *)(addr))
#define DRV_Reg32(addr)	(*(volatile kal_uint32 *)(addr))


/*
 * TC0: (TS_MCU1, TS_MCU2, TS_MCU3)
 */
static int tscpu_ts_temp[TS_ENUM_MAX];
struct thermal_controller tscpu_g_tc[THERMAL_CONTROLLER_NUM] = {
	[0] = {
		.ts = {TS_MCU1, TS_MCU2},
		.ts_number = 2,
		.tc_offset = 0x0,
		.tc_speed = {
			0x0000000C,
			0x0001003B,
			0x0000030D
		} /* 4.9ms */
	}
};


static kal_int32 g_adc_ge_t = 0;
static kal_int32 g_adc_oe_t = 0;
static kal_int32 g_o_vtsmcu1 = 0;
static kal_int32 g_o_vtsmcu2 = 0;
static kal_int32 g_o_vtsmcu3 = 0;

static kal_int32 g_degc_cali = 0;
static kal_int32 g_adc_cali_en_t = 0;
static kal_int32 g_o_slope_sign = 0;
static kal_int32 g_o_slope = 0;
static kal_int32 g_id = 0;

static kal_int32 g_ge = 0;
static kal_int32 g_oe = 0;
static kal_int32 g_gain = 0;

static kal_int32 g_x_roomt[TS_ENUM_MAX] = {0};
/*=============================================================
 * Function Prototype
 *=============================================================
 */
static void thermal_cal_prepare(void);
static void thermal_cal_prepare_2(void);
static S32 temperature_to_raw_room(U32 ret, ts_e ts_name);
static S32 raw_to_temperature_roomt(U32 ret, ts_e ts_name);
static void thermal_reset_and_initial(int tc_num);
static int thermal_fast_init(int tc_num);
static void tscpu_fast_initial_sw_workaround(void);
static void tscpu_thermal_tempADCPNP(int tc_num, int adc, int order);
static void tscpu_thermal_enable_all_periodoc_sensing_point(int tc_num);
static void thermal_disable_all_periodoc_temp_sensing(void);
static void thermal_pause_all_periodoc_temp_sensing(void);
static void thermal_release_all_periodoc_temp_sensing(void);
static void thermal_initial_all_tc(void);
static int read_tc_raw_and_temp(volatile u32 *tempmsr_name, ts_e ts_name);
static void tscpu_thermal_read_tc_temp(int tc_num, ts_e type, int order);
static void read_all_tc_temperature(void);
static void set_tc_trigger_hw_protect(int temperature, int temperature2, int tc_num);
static void tscpu_config_all_tc_hw_protect(int temperature, int temperature2);
static int tscpu_max_temperature(void);
static void print_mcu_temp(void);



/*=============================================================
 * Local Function
 *=============================================================
 */
static void thermal_cal_prepare(void)
{

	kal_uint32 temp0, temp1, temp2, temp3, temp4;

	/* Cervino
	100 M_ANALOG0 (0x11C50180)
	101 M_ANALOG1 (0x11C50184)
	102 M_ANALOG2 (0x11C50188)
	*/
	temp0 = DRV_Reg32(0x11c50180);
	temp1 = DRV_Reg32(0x11c50184);
	temp2 = DRV_Reg32(0x11c50188);
	temp3 = DRV_Reg32(0x11c5018c);
	temp4 = DRV_Reg32(0x11c50190);

	g_adc_cali_en_t = (temp1 & _BIT_(30) >> 30);

	if (g_adc_cali_en_t == 1) {

		g_adc_ge_t = ((temp0 & _BITMASK_(21:12)) >> 12);
		g_adc_oe_t = ((temp0 & _BITMASK_(31:22)) >> 22);

		g_o_vtsmcu1 = ((temp1 & _BITMASK_(20:12)) >> 12);
		g_o_vtsmcu2 = ((temp1 & _BITMASK_(29:21)) >> 21);
		g_o_vtsmcu3 = (temp2 & _BITMASK_(8:0));

		g_degc_cali = ((temp4 & _BITMASK_(27:22)) >> 22);

		g_o_slope_sign = ((temp1 & _BIT_(31)) >> 31);
		g_o_slope = ((temp4 & _BITMASK_(21:16)) >> 16);

		g_id = ((temp3 & _BIT_(10)) >> 10);

		if (g_id == 0)
			g_o_slope = 0;

	} else {
		THERMAL_LOG("This sample is not Thermal calibrated\n");
		g_adc_ge_t = 512;
		g_adc_oe_t = 512;
		g_o_vtsmcu1 = 260;
		g_o_vtsmcu2 = 260;
		//g_o_vtsmcu3 = 260;
		g_degc_cali = 40;
		g_o_slope_sign = 0;
		g_o_slope = 0;
	}

	THERMAL_LOG("[Power/CPU_Thermal] [calibration] g_adc_ge_t      = %d\n",g_adc_ge_t);
	THERMAL_LOG("[Power/CPU_Thermal] [calibration] g_adc_oe_t      = %d\n",g_adc_oe_t);
	THERMAL_LOG("[Power/CPU_Thermal] [calibration] g_degc_cali     = %d\n",g_degc_cali);
	THERMAL_LOG("[Power/CPU_Thermal] [calibration] g_adc_cali_en_t = %d\n",g_adc_cali_en_t);
	THERMAL_LOG("[Power/CPU_Thermal] [calibration] g_o_slope       = %d\n",g_o_slope);
	THERMAL_LOG("[Power/CPU_Thermal] [calibration] g_o_slope_sign  = %d\n",g_o_slope_sign);
	THERMAL_LOG("[Power/CPU_Thermal] [calibration] g_id            = %d\n",g_id);

	THERMAL_LOG("[Power/CPU_Thermal] [calibration] g_o_vtsmcu1     = %d\n",g_o_vtsmcu1);
	THERMAL_LOG("[Power/CPU_Thermal] [calibration] g_o_vtsmcu2     = %d\n",g_o_vtsmcu2);
	THERMAL_LOG("[Power/CPU_Thermal] [calibration] g_o_vtsmcu3     = %d\n",g_o_vtsmcu3);
}

static void thermal_cal_prepare_2(void)
{
	int i;
	kal_int32 format[TS_ENUM_MAX];

	THERMAL_LOG("thermal_cal_prepare_2\n");

	g_ge = ((g_adc_ge_t - 512) * 10000 ) / 4096; // ge * 10000
	g_oe =  (g_adc_oe_t - 512);

	g_gain = (10000 + g_ge);

	format[0]   = (g_o_vtsmcu1 + 3350 - g_oe);
	format[1]   = (g_o_vtsmcu2 + 3350 - g_oe);
	//format[2]   = (g_o_vtsmcu3 + 3350 - g_oe);


	for (i = 0; i < TS_ENUM_MAX; i++)
		g_x_roomt[i] = (((format[i] * 10000) / 4096) * 10000) / g_gain;	/* x_roomt * 10000 */

	THERMAL_CRTI_LOG("[calibration] g_ge         = 0x%x\n", g_ge);
	THERMAL_CRTI_LOG("[calibration] g_gain       = 0x%x\n", g_gain);
	for (i = 0; i < TS_ENUM_MAX; i++)
		THERMAL_CRTI_LOG("[T_De][cal] g_x_roomt%d   = %d\n", i, g_x_roomt[i]);
}

static S32 temperature_to_raw_room(U32 ret, ts_e ts_name)
{
	/* Ycurr = [(Tcurr - DEGC_cali/2)*(1653+O_slope*10)/10*(18/15)*(1/10000)+X_roomtabb]*Gain*4096 + OE */

	S32 t_curr = ret;
	S32 format_1 = 0;
	S32 format_2 = 0;
	S32 format_3 = 0;
	S32 format_4 = 0;

	/* tscpu_dprintk("temperature_to_raw_room\n"); */
	if (g_o_slope_sign == 0) {	/* O_SLOPE is Positive. */
		format_1 = t_curr - (g_degc_cali * 1000 / 2);
		format_2 = format_1 * (1653 + g_o_slope * 10) / 10 * 18 / 15;
		format_2 = format_2 - 2 * format_2;

		format_3 = format_2 / 1000 + g_x_roomt[ts_name] * 10;
		format_4 = (format_3 * 4096 / 10000 * g_gain) / 100000 + g_oe;
	} else {		/* O_SLOPE is Negative. */
		format_1 = t_curr - (g_degc_cali * 1000 / 2);
		format_2 = format_1 * (1653 - g_o_slope * 10) / 10 * 18 / 15;
		format_2 = format_2 - 2 * format_2;

		format_3 = format_2 / 1000 + g_x_roomt[ts_name] * 10;
		format_4 = (format_3 * 4096 / 10000 * g_gain) / 100000 + g_oe;
	}

	return format_4;
}

static S32 raw_to_temperature_roomt(U32 ret, ts_e ts_name)
{
	S32 t_current = 0;
	S32 y_curr = ret;
	S32 format_1 = 0;
	S32 format_2 = 0;
	S32 format_3 = 0;
	S32 format_4 = 0;
	S32 xtoomt=0;

	xtoomt = g_x_roomt[ts_name];

	if(ret==0)
		return 0;

	format_1 = ((g_degc_cali*10) >> 1);
	format_2 = (y_curr - g_oe);

	format_3 = (((((format_2) * 10000) >> 12 ) * 10000) / g_gain) - xtoomt;
	format_3 = format_3 * 15/18;

	if(g_o_slope_sign==0)
		format_4 = ((format_3 * 1000) / (1653 + g_o_slope * 10)); // uint = 0.1 deg
	else
		format_4 = ((format_3 * 1000) / (1653 - g_o_slope * 10)); // uint = 0.1 deg

	format_4 = format_4 - (format_4 << 1);

	t_current = format_1 + format_4; // uint = 0.1 deg

	return t_current;
}

static void thermal_reset_and_initial(int tc_num)
{
	int offset, tempMonCtl1, tempMonCtl2, tempAhbPoll;
	//dbg_print("[Reset and init thermal controller]\n");

	offset = tscpu_g_tc[tc_num].tc_offset;
	tempMonCtl1 = tscpu_g_tc[tc_num].tc_speed.tempMonCtl1;
	tempMonCtl2 = tscpu_g_tc[tc_num].tc_speed.tempMonCtl2;
	tempAhbPoll = tscpu_g_tc[tc_num].tc_speed.tempAhbPoll;

	THERMAL_WRAP_WR32(tempMonCtl1, offset + TEMPMONCTL1);    // bus clock 66M counting unit is 4*15.15ns* 256 = 15513.6 ms=15.5us
	THERMAL_WRAP_WR32(tempMonCtl2, offset + TEMPMONCTL2);	    // filter interval is 1023 * 15.5us ~ 15.86ms
	THERMAL_WRAP_WR32(tempAhbPoll, offset + TEMPAHBPOLL);		// poll is set to 254.17ms

	THERMAL_WRAP_WR32(0x00000000, offset + TEMPMSRCTL0);      // temperature sampling control, 1 sample
	THERMAL_WRAP_WR32(0xFFFFFFFF, offset + TEMPAHBTO);      // exceed this polling time, IRQ would be inserted

	THERMAL_WRAP_WR32(0x00000000, offset + TEMPMONIDET0);   // times for interrupt occurrance
	THERMAL_WRAP_WR32(0x00000000, offset + TEMPMONIDET1);   // times for interrupt occurrance
	THERMAL_WRAP_WR32(0x00000000, offset + TEMPMONIDET2);   // times for interrupt occurrance

	THERMAL_WRAP_WR32(0x800, offset + TEMPADCMUX);                         // this value will be stored to TEMPPNPMUXADDR (TEMPSPARE0) automatically by hw
	THERMAL_WRAP_WR32((int) AUXADC_CON1_CLR, offset + TEMPADCMUXADDR);// AHB address for auxadc mux selection

	THERMAL_WRAP_WR32(0x800, offset + TEMPADCEN);                          // AHB value for auxadc enable
	THERMAL_WRAP_WR32((int) AUXADC_CON1_SET, offset + TEMPADCENADDR); // AHB address for auxadc enable (channel 0 immediate mode selected)
	// this value will be stored to TEMPADCENADDR automatically by hw

	THERMAL_WRAP_WR32((int) AUXADC_DAT11, offset + TEMPADCVALIDADDR); // AHB address for auxadc valid bit
	THERMAL_WRAP_WR32((int) AUXADC_DAT11, offset + TEMPADCVOLTADDR);  // AHB address for auxadc voltage output
	THERMAL_WRAP_WR32(0x0, offset + TEMPRDCTRL);               			  // read valid & voltage are at the same register
	THERMAL_WRAP_WR32(0x0000002C, offset + TEMPADCVALIDMASK);              // indicate where the valid bit is (the 12th bit is valid bit and 1 is valid)
	THERMAL_WRAP_WR32(0x0, offset + TEMPADCVOLTAGESHIFT);                  // do not need to shift
	THERMAL_WRAP_WR32(0x2, offset + TEMPADCWRITECTRL);                     // enable auxadc mux write transaction
}



void tscpu_reset_thermal(void)
{
	int temp = 0;

	//reset thremal ctrl
	temp = DRV_Reg32(INFRA_GLOBALCON_RST_0_SET);
	temp |= 0x00000001;	//1: Enables thermal control software reset
	THERMAL_WRAP_WR32(temp, INFRA_GLOBALCON_RST_0_SET);

	//un reset
	temp = DRV_Reg32(INFRA_GLOBALCON_RST_0_CLR);
	temp |= 0x00000001;	//1: Enable reset Disables thermal control software reset
	THERMAL_WRAP_WR32(temp, INFRA_GLOBALCON_RST_0_CLR);
}


static int thermal_fast_init(int tc_num)
{
	UINT32 temp = 0, cunt = 0, offset = 0;

	offset = tscpu_g_tc[tc_num].tc_offset;

	THERMAL_LOG("thermal_fast_init\n");

	temp = 0xDA1;
	DRV_WriteReg32(offset + PTPSPARE2, (0x00001000 + temp));//write temp to spare register

	DRV_WriteReg32(offset + TEMPMONCTL1, 1);                // counting unit is 320 * 31.25us = 10ms
	DRV_WriteReg32(offset + TEMPMONCTL2, 1);                // sensing interval is 200 * 10ms = 2000ms
	DRV_WriteReg32(offset + TEMPAHBPOLL, 1);                // polling interval to check if temperature sense is ready

	DRV_WriteReg32(offset + TEMPAHBTO,    0x000000FF);               // exceed this polling time, IRQ would be inserted
	DRV_WriteReg32(offset + TEMPMONIDET0, 0x00000000);               // times for interrupt occurrance
	DRV_WriteReg32(offset + TEMPMONIDET1, 0x00000000);               // times for interrupt occurrance

	DRV_WriteReg32(offset + TEMPMSRCTL0, 0x0000000);                 // temperature measurement sampling control

	DRV_WriteReg32(offset + TEMPADCPNP0, 0x1);                       // this value will be stored to TEMPPNPMUXADDR (TEMPSPARE0) automatically by hw
	DRV_WriteReg32(offset + TEMPADCPNP1, 0x2);                       // this value will be stored to TEMPPNPMUXADDR (TEMPSPARE0) automatically by hw
	DRV_WriteReg32(offset + TEMPADCPNP2, 0x3);
	DRV_WriteReg32(offset + TEMPADCPNP3, 0x4);

	DRV_WriteReg32(offset + TEMPPNPMUXADDR, (UINT32) (PTPSPARE0));    // AHB address for pnp sensor mux selection
	DRV_WriteReg32(offset + TEMPADCMUXADDR, (UINT32) (PTPSPARE0));    // AHB address for auxadc mux selection
	DRV_WriteReg32(offset + TEMPADCENADDR,  (UINT32) (PTPSPARE1));     // AHB address for auxadc enable
	DRV_WriteReg32(offset + TEMPADCVALIDADDR, (UINT32) (PTPSPARE2));  // AHB address for auxadc valid bit
	DRV_WriteReg32(offset + TEMPADCVOLTADDR, (UINT32) (PTPSPARE2));   // AHB address for auxadc voltage output

	DRV_WriteReg32(offset + TEMPRDCTRL, 0x0);                        // read valid & voltage are at the same register
	DRV_WriteReg32(offset + TEMPADCVALIDMASK, 0x0000002C);           // indicate where the valid bit is (the 12th bit is valid bit and 1 is valid)
	DRV_WriteReg32(offset + TEMPADCVOLTAGESHIFT, 0x0);               // do not need to shift
	DRV_WriteReg32(offset + TEMPADCWRITECTRL, 0x3);                  // enable auxadc mux & pnp write transaction

	DRV_WriteReg32(offset + TEMPMONINT, 0x00000000);                 // enable all interrupt except filter sense and immediate sense interrupt

	DRV_WriteReg32(offset + TEMPMONCTL0, 0x0000000F);                // enable all sensing point (sensing point 2 is unused)

	cunt=0;
	temp = DRV_Reg32(offset + TEMPMSR0)& 0x0fff;
	while(temp!=0xDA1 && cunt <20){
		cunt++;
		THERMAL_LOG("[Power/CPU_Thermal]0 temp=%d,cunt=%d(%d)\n",temp,cunt,__LINE__);
		temp = DRV_Reg32(offset + TEMPMSR0)& 0x0fff;
	}
	THERMAL_LOG("[Power/CPU_Thermal]0 temp=%d,cunt=%d(%d)\n",temp,cunt,__LINE__);

	cunt=0;
	temp = DRV_Reg32(offset + TEMPMSR1)& 0x0fff;
	while(temp!=0xDA1 &&  cunt <20){
		cunt++;
		THERMAL_LOG("[Power/CPU_Thermal]1 temp=%d,cunt=%d(%d)\n",temp,cunt,__LINE__);
		temp = DRV_Reg32(offset + TEMPMSR1)& 0x0fff;
	}
	THERMAL_LOG("[Power/CPU_Thermal]1 temp=%d,cunt=%d(%d)\n",temp,cunt,__LINE__);

	cunt=0;
	temp = DRV_Reg32(offset + TEMPMSR2)& 0x0fff;
	while(temp!=0xDA1 &&  cunt <20){
		cunt++;
		THERMAL_LOG("[Power/CPU_Thermal]2 temp=%d,cunt=%d(%d)\n",temp,cunt,__LINE__);
		temp = DRV_Reg32(offset + TEMPMSR2)& 0x0fff;
	}
	THERMAL_LOG("[Power/CPU_Thermal]2 temp=%d,cunt=%d(%d)\n",temp,cunt,__LINE__);

	cunt=0;
	temp = DRV_Reg32(offset + TEMPMSR3)& 0x0fff;
	while(temp!=0xDA1 &&  cunt <20){
		cunt++;
		THERMAL_LOG("[Power/CPU_Thermal]3 temp=%d,cunt=%d(%d)\n",temp,cunt,__LINE__);
		temp = DRV_Reg32(offset + TEMPMSR3)& 0x0fff;
	}
	THERMAL_LOG("[Power/CPU_Thermal]3 temp=%d,cunt=%d(%d)\n",temp,cunt,__LINE__);

	return 0;
}

static void tscpu_fast_initial_sw_workaround(void)
{
	unsigned int i = 0;

	for(i = 0; i < ARRAY_SIZE(tscpu_g_tc); i++)
		thermal_fast_init(i);
}

static int tscpu_thermal_ADCValueOfMcu(enum thermal_sensor_enum type)
{
	switch (type) {
	case TS_MCU1:
		return TEMPADC_MCU1;
	case TS_MCU2:
		return TEMPADC_MCU2;
	default:
		return TEMPADC_MCU1;
	}
}

static void tscpu_thermal_tempADCPNP(int tc_num, int adc, int order)
{
	int offset;

	offset = tscpu_g_tc[tc_num].tc_offset;

	//THERMAL_LOG("%s adc %x, order %d\n", __func__, adc, order);

	switch (order) {
	case 0:
		DRV_WriteReg32(offset + TEMPADCPNP0, adc);
		break;
	case 1:
		DRV_WriteReg32(offset + TEMPADCPNP1, adc);
		break;
	case 2:
		DRV_WriteReg32(offset + TEMPADCPNP2, adc);
		break;
	case 3:
		DRV_WriteReg32(offset + TEMPADCPNP3, adc);
		break;
	default:
		DRV_WriteReg32(offset + TEMPADCPNP0, adc);
		break;
	}
}

static void tscpu_thermal_enable_all_periodoc_sensing_point(int tc_num)
{
	int offset;

	offset = tscpu_g_tc[tc_num].tc_offset;

	switch (tscpu_g_tc[tc_num].ts_number) {
	case 1:
		/* enable periodoc temperature sensing point 0 */
		DRV_WriteReg32(offset + TEMPMONCTL0, 0x00000001);
		break;
	case 2:
		/* enable periodoc temperature sensing point 0,1 */
		DRV_WriteReg32(offset + TEMPMONCTL0, 0x00000003);
		break;
	case 3:
		/* enable periodoc temperature sensing point 0,1,2 */
		DRV_WriteReg32(offset + TEMPMONCTL0, 0x00000007);
		break;
	case 4:
		/* enable periodoc temperature sensing point 0,1,2,3 */
		DRV_WriteReg32(offset + TEMPMONCTL0, 0x0000000F);
		break;
	default:
		THERMAL_CRTI_LOG("Error at %s ,tc_num = %d\n", __func__, tc_num);
		break;
	}
}

//disable ALL periodoc temperature sensing point
static void thermal_disable_all_periodoc_temp_sensing(void)
{
	unsigned int i = 0, offset;

	THERMAL_LOG("thermal_disable_all_periodoc_temp_sensing\n");

	for(i = 0; i < ARRAY_SIZE(tscpu_g_tc); i++) {
		offset = tscpu_g_tc[i].tc_offset;
		THERMAL_WRAP_WR32(0x00000000, offset + TEMPMONCTL0);
	}

}

//pause ALL periodoc temperature sensing point
static void thermal_pause_all_periodoc_temp_sensing(void)
{
	unsigned int i = 0, temp, offset;

	THERMAL_LOG("thermal_pause_all_periodoc_temp_sensing\n");

	for(i = 0; i < ARRAY_SIZE(tscpu_g_tc); i++) {
		offset = tscpu_g_tc[i].tc_offset;

		temp = DRV_Reg32(offset + TEMPMSRCTL1);
		//set bit8=bit1=bit2=bit3=1 to pause sensing point 0,1,2,3
		DRV_WriteReg32(offset + TEMPMSRCTL1, (temp | 0x10E));
	}
}

static void thermal_release_all_periodoc_temp_sensing(void)
{
	unsigned int i = 0, temp, offset;

	THERMAL_LOG("thermal_release_all_periodoc_temp_sensing\n");

	for(i = 0; i < ARRAY_SIZE(tscpu_g_tc); i++) {
		offset = tscpu_g_tc[i].tc_offset;

		temp = DRV_Reg32(offset + TEMPMSRCTL1);
		//set bit8 = bit1=bit2=bit3=1 to pause sensing point 0,1,2
		DRV_WriteReg32(offset + TEMPMSRCTL1, ( (temp & (~0x10E)) ));
	}
}

static void thermal_initial_all_tc(void)
{
	int i, j = 0, temp = 0, offset;

	/* AuxADC Initialization,ref MT6592_AUXADC.doc // TODO: check this line */
	temp = DRV_Reg32(AUXADC_CON0);	/* Auto set enable for CH11 */
	temp &= 0xFFFFF7FF;	/* 0: Not AUTOSET mode */
	DRV_WriteReg32(AUXADC_CON0, temp);	/* disable auxadc channel 11 synchronous mode */
	DRV_WriteReg32(AUXADC_CON1_CLR, 0x800);	/* disable auxadc channel 11 immediate mode */

	for (i = 0; i < ARRAY_SIZE(tscpu_g_tc); i++) {
		if (tscpu_g_tc[i].ts_number == 0)
			continue;

		offset = tscpu_g_tc[i].tc_offset;
		thermal_reset_and_initial(i);

		for (j = 0; j < tscpu_g_tc[i].ts_number; j++)
			tscpu_thermal_tempADCPNP(i, tscpu_thermal_ADCValueOfMcu
					(tscpu_g_tc[i].ts[j]), j);

		DRV_WriteReg32(offset + TEMPPNPMUXADDR, TS_CON1);
		DRV_WriteReg32(offset + TEMPADCWRITECTRL, 0x3);
	}

	DRV_WriteReg32(AUXADC_CON1_SET, 0x800);

	for (i = 0; i < ARRAY_SIZE(tscpu_g_tc); i++)
		tscpu_thermal_enable_all_periodoc_sensing_point(i);

}

static int read_tc_raw_and_temp(volatile u32 *tempmsr_name, ts_e ts_name)
{
	int temp = 0, raw = 0;

	if (tempmsr_name == 0)
		return 0;

	raw = DRV_Reg32((tempmsr_name)) & 0x0fff;
	temp = raw_to_temperature_roomt(raw, ts_name);

	THERMAL_LOG("read_tc_raw_temp,ts_raw=%d,temp=%d\n", raw, temp * 100);

	return temp * 100;
}

static void tscpu_thermal_read_tc_temp(int tc_num, ts_e type, int order)
{
	int offset;

	//THERMAL_LOG("%s tc_num %d type %d order %d\n", __func__, tc_num, type, order);
	offset = tscpu_g_tc[tc_num].tc_offset;

	switch (order) {
	case 0:
		tscpu_ts_temp[type] =
		    read_tc_raw_and_temp((volatile u32 *)(offset + TEMPMSR0), type);
		//THERMAL_LOG("%s order %d tc_num %d type %d temp %d\n",
		//	      __func__, order, tc_num, type, tscpu_ts_temp[type]);
		break;
	case 1:
		tscpu_ts_temp[type] =
		    read_tc_raw_and_temp((volatile u32 *)(offset + TEMPMSR1), type);
		//THERMAL_LOG("%s order %d tc_num %d type %d temp %d\n",
		//	      __func__, order, tc_num, type, tscpu_ts_temp[type]);
		break;
	case 2:
		tscpu_ts_temp[type] =
		    read_tc_raw_and_temp((volatile u32 *)(offset + TEMPMSR2), type);
		//THERMAL_LOG("%s order %d tc_num %d type %d temp %d\n",
		//	      __func__, order, tc_num, type, tscpu_ts_temp[type]);
		break;
	case 3:
		tscpu_ts_temp[type] =
		    read_tc_raw_and_temp((volatile u32 *)(offset + TEMPMSR3), type);
		//THERMAL_LOG("%s order %d tc_num %d type %d temp %d\n",
		//	      __func__, order, tc_num, type, tscpu_ts_temp[type]);
		break;
	default:
		tscpu_ts_temp[type] =
		    read_tc_raw_and_temp((volatile u32 *)(offset + TEMPMSR0), type);
		//THERMAL_LOG("%s order %d tc_num %d type %d temp %d\n",
		//	      __func__, order, tc_num, type, tscpu_ts_temp[type]);
		break;
	}
}

static void read_all_tc_temperature(void)
{
        int i = 0, j = 0;

        for (i = 0; i < ARRAY_SIZE(tscpu_g_tc); i++)
                for (j = 0; j < tscpu_g_tc[i].ts_number; j++)
                        tscpu_thermal_read_tc_temp(i, tscpu_g_tc[i].ts[j], j);
}

static void set_tc_trigger_hw_protect(int temperature, int temperature2, int tc_num)
{
	int temp = 0, offset;
	int raw_high;
	ts_e ts_name;

	THERMAL_CRTI_LOG("set_tc_trigger_hw_protect t1=%d t2=%d\n", temperature, temperature2);

	offset = tscpu_g_tc[tc_num].tc_offset;
	ts_name = tscpu_g_tc[tc_num].ts[0];

	raw_high   = temperature_to_raw_room(temperature, ts_name);

	temp = DRV_Reg32(TEMPMONINT);
	THERMAL_WRAP_WR32(temp & 0x00000000, TEMPMONINT);	// disable trigger SPM interrupt

	THERMAL_WRAP_WR32(0x20000, TEMPPROTCTL);// set hot to wakeup event control
	THERMAL_WRAP_WR32(raw_high, TEMPPROTTC);// set hot to HOT wakeup event
	THERMAL_WRAP_WR32(temp | 0x80000000, TEMPMONINT);	// enable trigger Hot SPM interrupt
}

static void tscpu_config_all_tc_hw_protect(int temperature, int temperature2)
{
	unsigned int i = 0;

	THERMAL_CRTI_LOG( "tscpu_config_all_tc_hw_protect, temperature=%d, temperature2=%d,\n", temperature, temperature2);

	/*Thermal need to config to direct reset mode
	  this API provide by Weiqi Fu(RGU SW owner).*/

	/* The way of setting hardware protection is different than Android load. */
	//mtk_wdt_request_en_set(MTK_WDT_REQ_MODE_THERMAL, WD_REQ_EN);

        for (i = 0; i < ARRAY_SIZE(tscpu_g_tc); i++)
		set_tc_trigger_hw_protect(temperature, temperature2, i); // Move thermal HW protection ahead...

	//mtk_wdt_request_mode_set(MTK_WDT_REQ_MODE_THERMAL, WD_REQ_RST_MODE);
}

static int tscpu_max_temperature(void)
{
    int i, j;
	int max = 0;

        //THERMAL_LOG("tscpu_get_temp %s, %d\n", __func__, __LINE__);

        for (i = 0; i < ARRAY_SIZE(tscpu_g_tc); i++) {
                for (j = 0; j < tscpu_g_tc[i].ts_number; j++) {
                        if (i == 0 && j == 0) {
                                max = tscpu_ts_temp[tscpu_g_tc[i].ts[j]];
                        } else {
                                if (max < tscpu_ts_temp[tscpu_g_tc[i].ts[j]])
                                        max = tscpu_ts_temp[tscpu_g_tc[i].ts[j]];
                        }
                }
        }

        return max;
}

static void print_mcu_temp(void)
{
	int i;

	for (i = 0; i < TS_ENUM_MAX; i++)
		THERMAL_CRTI_LOG("TS_MCU%d = %d\n", (i + 1), tscpu_ts_temp[i]);
}
/*=============================================================
 * Global Function
 *=============================================================
 */

void get_thermal_slope_intercept(struct TS_PTPOD *ts_info,thermal_bank_name ts_bank)
{
	unsigned int temp0, temp1, temp2;
	struct TS_PTPOD ts_ptpod;
	S32 x_roomt;

	THERMAL_LOG("get_thermal_slope_intercept\n");

	switch (ts_bank) {
	case THERMAL_BANK0:
		x_roomt = g_x_roomt[TS_MCU1];
		break;
	case THERMAL_BANK3:
		x_roomt = g_x_roomt[TS_MCU2];
		break;
	default:		/*choose high temp */
		x_roomt = g_x_roomt[TS_MCU1];
		break;
	}

	/*
	The equations in this function are confirmed by Thermal DE Alfred Tsai.
	Don't have to change until using next generation thermal sensors.
	*/

	temp0 = (10000 * 100000 / g_gain) * 15 / 18;

	if (g_o_slope_sign == 0)
		temp1 = (temp0 * 10) / (1653 + g_o_slope * 10);
	else
		temp1 = (temp0 * 10) / (1653 - g_o_slope * 10);

	ts_ptpod.ts_MTS = temp1;

	temp0 = (g_degc_cali * 10 / 2);
	temp1 = ((10000 * 100000 / 4096 / g_gain) * g_oe + x_roomt * 10) * 15 / 18;

	if (g_o_slope_sign == 0)
		temp2 = temp1 * 100 / (1653 + g_o_slope * 10);
	else
		temp2 = temp1 * 100 / (1653 - g_o_slope * 10);

	ts_ptpod.ts_BTS = (temp0 + temp2 - 250) * 4 / 10;


	ts_info->ts_MTS = ts_ptpod.ts_MTS;
	ts_info->ts_BTS = ts_ptpod.ts_BTS;
	THERMAL_LOG("ts_MTS=%d, ts_BTS=%d\n",ts_ptpod.ts_MTS, ts_ptpod.ts_BTS);

	return;
}

int thermal_init(void){

	struct udevice *dev;
	int idx = 0;
	/* Probe devices with DM compliant drivers */
	while (!uclass_get_device(UCLASS_THERMAL, idx, &dev) && dev) {
		idx++;
	}

return 0;
}


static int mtk_thermal_probe(struct udevice *dev)
{
	int ret,temp;
	int cnt=0;
	struct clk clk;

	THERMAL_LOG("thermal: thermal probe\n");

	THERMAL_LOG("thermal: name: %s\n", dev->name);

	ret = clk_get_by_name(dev, "therm", &clk);
	if(ret < 0)
		THERMAL_LOG("fail to get thermal clock!!\n");
	else
		THERMAL_LOG("Success to get thermal clock!!\n");
	clk_enable(&clk);


	ret = clk_get_by_name(dev, "auxadc", &clk);
	if(ret < 0)
		THERMAL_LOG("fail to get auxadc clock!!\n");
	else
		THERMAL_LOG("Success to get auxadc clock!!\n");
	clk_enable(&clk);

	thermal_cal_prepare();
	thermal_cal_prepare_2();

	tscpu_reset_thermal();
	THERMAL_LOG("thermal: tscpu_reset_thermal done\n");

	/*
	   TS_CON1 default is 0x30, this is buffer off
	   we should turn on this buffer berore we use thermal sensor,
	   or this buffer off will let TC read a very small value from auxadc
	   and this small value will trigger thermal reboot
	 */

	THERMAL_WRAP_WR32(0x302022A8, TS_CON0);
	udelay(200);

	temp = DRV_Reg32(TS_CON0);
	temp &=~(0x30000000); //TS_CON0[29:28]=2'b00,   00: Buffer on, TSMCU to AUXADC
	THERMAL_WRAP_WR32(temp, TS_CON0);
	udelay(200);

	tscpu_fast_initial_sw_workaround();

	while(cnt < 50)
	{
		temp = (DRV_Reg32(THAHBST0) >> 16);
		if(cnt>20)
			THERMAL_LOG("THAHBST0 = 0x%x,cnt=%d, %d\n", temp,cnt,__LINE__);
		if(temp == 0x0){
			// pause all periodoc temperature sensing point 0~2
			thermal_pause_all_periodoc_temp_sensing();//TEMPMSRCTL1
			break;
		}
		udelay(2);
		cnt++;
	}
	//disable periodic temp measurement on sensor 0~2
	thermal_disable_all_periodoc_temp_sensing();//TEMPMONCTL0
	thermal_initial_all_tc();
	thermal_release_all_periodoc_temp_sensing();//must release before start

	read_all_tc_temperature();
	tscpu_config_all_tc_hw_protect(THERMAL_HARDWARE_RESET_POINT, 100000);

	return 0;
}

int mtktscpu_get_hw_temp(void)
{

	int curr_temp;

	thermal_init();
	mdelay(100);

	read_all_tc_temperature();
	print_mcu_temp();
	curr_temp = tscpu_max_temperature();
	THERMAL_LOG("Max temp= %d\n", curr_temp);
	return curr_temp;

}

U_BOOT_CMD(
	thermal_get_temp, 1, 0, mtktscpu_get_hw_temp,
	"get cpu temperature",
	"thermal_cpu_temp"
);

static const struct udevice_id mtk_thermal_match[] = {
	{ .compatible = "mediatek,mt8512-thermal", },
	{ }
};

U_BOOT_DRIVER(mtk_thermal)  = {
	.name = "mtk_thermal",
	.id = UCLASS_THERMAL,
	.of_match = mtk_thermal_match,
	.probe = mtk_thermal_probe,
};





