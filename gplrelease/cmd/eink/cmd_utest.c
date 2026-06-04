/*
 * cmd_utest.c
 *
 * Copyright 2016-2020 Amazon.com, Inc. or its affiliates.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <common.h>
#include <command.h>
#include <asm/gpio.h>
#include <i2c.h>
#include <errno.h>
#include <dm.h>
#include <adc.h>


#ifdef CONFIG_TARGET_MT8110_BELLATRIX


#define CONFIG_PMIC_I2C_BUS	1
#define CONFIG_PMIC_I2C_SLAVE	0x4B



#define BD71828_REG_PS_CTRL_1	0x04
#define BD71828_REG_LDO2_ON	0x3B
#define BD71828_REG_LDO3_ON     0x3D
#define BD71828_REG_VBAT_U	0x8C
#define BD71828_REG_BTMP_U	0xA1
#define BD71828_REG_CURCD_U	0xB0
#define BD71828_REG_CCNTD_U	0xB5
#define BD71828_REG_CC_FULL	0xBD
#define BD71828_REG_CC_CTRL	0xC4
#define BD71828_REG_HALL_STATUS	0xED
#define BD71828_REG_LDO5_ON	0x41
#define BD71828_REG_LDO6_ON     0x44
#define BD71828_REG_CHG_IFST	0x7A
#define BD71828_REG_CHG_EN	0x6F
#define BD71828_REG_EPD_EN	0x48

#define HWID_BELLATRIX  1
#define HWID_PROTO      2
#define HWID_HVT_POR    3
#define HWID_HVT_DOE    4
#define HWID_HVT1_1_POR 5
#define HWID_DVT        9
static int utest_i2c_read(unsigned char busnum, unsigned char saddr,unsigned int reg, unsigned int alen, unsigned char *val, unsigned int len)
{
        struct udevice *bus, *dev;
        int ret;

        ret = uclass_get_device_by_seq(UCLASS_I2C, busnum, &bus);
        if (ret) {
                printf("%s: No bus %d\n", __func__, busnum);
                return ret;
        }

	ret = i2c_get_chip(bus, saddr, 1, &dev);
	if (!ret)
                ret = dm_i2c_read(dev, reg, val, len);
	
	if(ret) {
		printf("ERROR: %s\n", __FUNCTION__);
		return -1;
	}


	return ret;
}

static int utest_i2c_write(unsigned char busnum, unsigned char saddr,unsigned int reg, unsigned int alen, unsigned char *val, unsigned int len)
{
        struct udevice *bus, *dev;
        int ret;

        ret = uclass_get_device_by_seq(UCLASS_I2C, busnum, &bus);
        if (ret) {
                printf("%s: No bus %d\n", __func__, busnum);
                return ret;
        }

        ret = i2c_get_chip(bus, saddr, 1, &dev);
        if (!ret)
                ret = dm_i2c_write(dev, reg, val, len);

	
	if(ret){
		printf("ERROR: %s\n", __FUNCTION__);
		return -1;
	}


        return ret;
}


static int board_pmic_init(void);
static unsigned short i2c_reg_read16(unsigned char bus, unsigned char saddr,unsigned int reg) {

        int ret;
        unsigned char v1,v2;

        ret = utest_i2c_read(bus, saddr, reg, 1, &v1, 1);
        if (ret)
           return 0;
        ret = utest_i2c_read(bus, saddr, reg+1, 1, &v2, 1);
        if (ret)
           return 0;

	//printf("reg:%x:v_u:v_l=0x%x:0x%x\n",reg,v1,v2);
        return (unsigned short)(v1<<8 | v2);

}

static unsigned int i2c_reg_read32(unsigned char bus, unsigned char saddr,unsigned int reg) {

        int ret;
        unsigned char v1,v2,v3,v4;


        ret = utest_i2c_read(bus, saddr, reg, 1, &v1, 1);
        if (ret)
           return 0;
        ret = utest_i2c_read(bus, saddr, reg+1, 1, &v2, 1);
        if (ret)
           return 0;

        ret = utest_i2c_read(bus, saddr, reg+2, 1, &v3, 1);
        if (ret)
           return 0;
        ret = utest_i2c_read(bus, saddr, reg+3, 1, &v4, 1);
        if (ret)
           return 0;


        return (unsigned int)(v1<<24 | v2 << 16 | v3 << 8 | v4);

}


static int board_pmic_get_fg_voltage(void)
{
        u16 voltage = 0;

        voltage = i2c_reg_read16(CONFIG_PMIC_I2C_BUS, CONFIG_PMIC_I2C_SLAVE, BD71828_REG_VBAT_U);
        return voltage;
}

static int board_pmic_get_fg_current(void)
{
        u32 current = 0;

        current = i2c_reg_read16(CONFIG_PMIC_I2C_BUS, CONFIG_PMIC_I2C_SLAVE, BD71828_REG_CURCD_U);
	current = current & 0x3fff;
	current = current*330;
	
        return current;
}

static int board_pmic_get_fg_charging_status(void)
{
        u16 chrg = 0;

        chrg = i2c_reg_read16(CONFIG_PMIC_I2C_BUS, CONFIG_PMIC_I2C_SLAVE, BD71828_REG_CURCD_U);

        chrg = chrg >> 15;

        return chrg;
}


static int board_pmic_get_fg_temp(void)
{
        u16 temp = 0;

        temp = i2c_reg_read16(CONFIG_PMIC_I2C_BUS, CONFIG_PMIC_I2C_SLAVE, BD71828_REG_BTMP_U);

	temp = (temp & 0xfff) >> 4;
	
	temp = 200 - temp;
        return temp;
}

static int board_pmic_get_fg_cc(void)
{
        u16 cap = 0;

        cap = i2c_reg_read16(CONFIG_PMIC_I2C_BUS, CONFIG_PMIC_I2C_SLAVE, BD71828_REG_CCNTD_U);

        cap = (cap & 0xfff);

        return cap;
}

static int board_pmic_touch_pwr(int enable)
{
	u8 value;

	board_pmic_init();

	if( 0 == enable) {
		//ldo2
		value=0x0;
		utest_i2c_write(CONFIG_PMIC_I2C_BUS, CONFIG_PMIC_I2C_SLAVE, BD71828_REG_LDO2_ON, 1, &value, 1);

		value=0x0;
		utest_i2c_write(CONFIG_PMIC_I2C_BUS, CONFIG_PMIC_I2C_SLAVE, BD71828_REG_LDO6_ON, 1, &value, 1);
	}
	else {
		value=0xE;
                utest_i2c_write(CONFIG_PMIC_I2C_BUS, CONFIG_PMIC_I2C_SLAVE, BD71828_REG_LDO2_ON, 1, &value, 1);

                value=0xE;
                utest_i2c_write(CONFIG_PMIC_I2C_BUS, CONFIG_PMIC_I2C_SLAVE, BD71828_REG_LDO6_ON, 1, &value, 1);
	}

	return 0;
}

static int board_pmic_adc_pwr(int enable)
{

        board_pmic_init();

#if 0                
	value = 0x10 | (enable<<3);
	utest_i2c_write(CONFIG_PMIC_I2C_BUS, CONFIG_PMIC_I2C_SLAVE, BD71828_REG_LDO3_ON, 1, &value, 1);
#endif	
	return 0;
}


static int board_pmic_init(void)
{
	u8 value;

	//just set a defaut cc value for testing only, no meaning
	if(0 == board_pmic_get_fg_cc())
	{
        	value=0x0;
		utest_i2c_write(CONFIG_PMIC_I2C_BUS, CONFIG_PMIC_I2C_SLAVE, BD71828_REG_CC_CTRL, 1, &value, 1);
		value=0xf;
                utest_i2c_write(CONFIG_PMIC_I2C_BUS, CONFIG_PMIC_I2C_SLAVE, BD71828_REG_CC_FULL, 1, &value, 1);

		value=0x1;
		utest_i2c_write(CONFIG_PMIC_I2C_BUS, CONFIG_PMIC_I2C_SLAVE, BD71828_REG_CCNTD_U, 1, &value, 1);
		value=0x40;
		utest_i2c_write(CONFIG_PMIC_I2C_BUS, CONFIG_PMIC_I2C_SLAVE, BD71828_REG_CC_CTRL, 1, &value, 1);
	}

        return 0;
}

static int board_enter_hib(void)
{
	u8 value;

	printf("Set power mode to hib\n");
	board_pmic_init();
	value=0x2;
        utest_i2c_write(CONFIG_PMIC_I2C_BUS, CONFIG_PMIC_I2C_SLAVE, BD71828_REG_PS_CTRL_1, 1, &value, 1);
	
	return 0;
}

static int board_enter_ship(void)
{
	u8 value;

	printf("Set power mode to ship\n");
	board_pmic_init();
        value=0x1;
        utest_i2c_write(CONFIG_PMIC_I2C_BUS, CONFIG_PMIC_I2C_SLAVE, BD71828_REG_PS_CTRL_1, 1, &value, 1);

	printf("Please plug out USB to let PMIC enter ship mode\n");

	while(1);

	return 0;
}

static int board_charger_ctrl(int status)
{
        u8 value;

        printf("Set charger mode to %d\n", status);
        board_pmic_init();

	//disable it first
	value = 0x0;
        utest_i2c_write(CONFIG_PMIC_I2C_BUS, CONFIG_PMIC_I2C_SLAVE, BD71828_REG_CHG_EN, 1, &value, 1);

	value = 0x40 + 0x12; //default value, about 550;
	if (status == 2)
		value=0x40 + 58;
	
        utest_i2c_write(CONFIG_PMIC_I2C_BUS, CONFIG_PMIC_I2C_SLAVE, BD71828_REG_CHG_IFST, 1, &value, 1);

	if ( 0 != status )
	{
		value=0x1;
        	utest_i2c_write(CONFIG_PMIC_I2C_BUS, CONFIG_PMIC_I2C_SLAVE, BD71828_REG_CHG_EN, 1, &value, 1);
	}

        return 0;
}


#define CONFIG_EPD_PMIC_I2C_BUS 0
#define CONFIG_EPD_PMIC_I2C_SLAVE 0x18
#define FITI_REG_VPOS	0x02
#define FITI_REG_TEMP	0x00



#define GPIO_EPD_EN      11
#define GPIO_EPD_TS_EN   9
static unsigned char epc_pmic_bus=CONFIG_EPD_PMIC_I2C_BUS;
static int board_epd_pmic_init(void)
{
	int hwid;
        char cmd[64];

        hwid = board_get_hwid(cmd);
        if(2 == hwid){
                epc_pmic_bus = 3;
        }

	return 0;
}

static int board_epd_pmic_nm(int enable)
{
        unsigned char val;
        board_epd_pmic_init();

        val = enable;
        utest_i2c_write(epc_pmic_bus, CONFIG_EPD_PMIC_I2C_SLAVE, FITI_REG_VPOS, 1, &val, 1);

	gpio_request(GPIO_EPD_EN,"epd_en");
        gpio_direction_output(GPIO_EPD_EN,0);
	val = 0;
        utest_i2c_write(CONFIG_PMIC_I2C_BUS, CONFIG_PMIC_I2C_SLAVE, BD71828_REG_EPD_EN, 1, &val, 1);
	udelay(2000 * 1000);
	gpio_direction_output(GPIO_EPD_EN, 1);
	val = 0x03;
        utest_i2c_write(CONFIG_PMIC_I2C_BUS, CONFIG_PMIC_I2C_SLAVE, BD71828_REG_EPD_EN, 1, &val, 1);

	gpio_free(GPIO_EPD_EN);
	return 0;
}

static int board_epd_get_temp(void)
{
	int ret;
        unsigned char val;

	board_epd_pmic_init();
	gpio_request(GPIO_EPD_TS_EN, "epd_ts");
	gpio_direction_output(GPIO_EPD_TS_EN,1);

	udelay(100 * 1000);

        ret = utest_i2c_read(epc_pmic_bus, CONFIG_EPD_PMIC_I2C_SLAVE, FITI_REG_TEMP, 1, &val, 1);
        if (ret)
           return 0;

	gpio_free(GPIO_EPD_TS_EN);
	return val;
}

static int board_epd_pmic_pwr(int enable)
{
	unsigned char val;
	board_epd_pmic_init();
	
	gpio_request(GPIO_EPD_EN, "epd_en");
	val = 0x28;
	utest_i2c_write(epc_pmic_bus, CONFIG_EPD_PMIC_I2C_SLAVE, FITI_REG_VPOS, 1, &val, 1);
	val = 0x02 | enable;
        utest_i2c_write(CONFIG_PMIC_I2C_BUS, CONFIG_PMIC_I2C_SLAVE, BD71828_REG_EPD_EN, 1, &val, 1);
	gpio_direction_output(GPIO_EPD_EN,enable);
	
	gpio_free(GPIO_EPD_EN);
	return 0;
}

struct ntc_compensation {
        int             temp_c;
        unsigned int    ohm;
};

static const struct ntc_compensation ncpXXwf104[] = {
        { .temp_c       = -40, .ohm     = 4397119 },
        { .temp_c       = -35, .ohm     = 3088599 },
        { .temp_c       = -30, .ohm     = 2197225 },
        { .temp_c       = -25, .ohm     = 1581881 },
        { .temp_c       = -20, .ohm     = 1151037 },
        { .temp_c       = -15, .ohm     = 846579 },
        { .temp_c       = -10, .ohm     = 628988 },
        { .temp_c       = -5, .ohm      = 471632 },
        { .temp_c       = 0, .ohm       = 357012 },
        { .temp_c       = 5, .ohm       = 272500 },
        { .temp_c       = 10, .ohm      = 209710 },
        { .temp_c       = 15, .ohm      = 162651 },
        { .temp_c       = 20, .ohm      = 127080 },
        { .temp_c       = 25, .ohm      = 100000 },
	{ .temp_c       = 26, .ohm      = 953981 },
	{ .temp_c       = 27, .ohm      = 910322 },
	{ .temp_c       = 28, .ohm      = 86889 },
	{ .temp_c       = 29, .ohm      = 82956 },
        { .temp_c       = 30, .ohm      = 79222 },
	{ .temp_c       = 31, .ohm      = 75675 },
        { .temp_c       = 32, .ohm      = 72306 },
        { .temp_c       = 33, .ohm      = 69104 },
        { .temp_c       = 34, .ohm      = 66061 },
        { .temp_c       = 35, .ohm      = 63167 },
        { .temp_c       = 40, .ohm      = 50677 },
        { .temp_c       = 45, .ohm      = 40904 },
        { .temp_c       = 50, .ohm      = 33195 },
        { .temp_c       = 55, .ohm      = 27091 },
        { .temp_c       = 60, .ohm      = 22224 },
        { .temp_c       = 65, .ohm      = 18323 },
        { .temp_c       = 70, .ohm      = 15184 },
        { .temp_c       = 75, .ohm      = 12635 },
        { .temp_c       = 80, .ohm      = 10566 },
        { .temp_c       = 85, .ohm      = 8873 },
        { .temp_c       = 90, .ohm      = 7481 },
        { .temp_c       = 95, .ohm      = 6337 },
        { .temp_c       = 100, .ohm     = 5384 },
        { .temp_c       = 105, .ohm     = 4594 },
        { .temp_c       = 110, .ohm     = 3934 },
        { .temp_c       = 115, .ohm     = 3380 },
        { .temp_c       = 120, .ohm     = 2916 },
        { .temp_c       = 125, .ohm     = 2522 },
};

static int ntc_to_temp(int ntc)
{
	int i;

	for(i=0;i<ARRAY_SIZE(ncpXXwf104);i++)
	{
		if(ncpXXwf104[i].ohm <= ntc)
			return ncpXXwf104[i].temp_c;
	}

	return 130;
}

#define ADC_MAX 4096
#define ADC_BIAS_OHM 100000
#define ADC_VOLTAGESAMPLING_RANGE 1500
#define ADC_VDD 1800
static int adc_to_ntc(int adc, int adc_bias)
{
	int ADCVoltage = (ADC_VOLTAGESAMPLING_RANGE * adc / ADC_MAX);
	int TRes =(int)(adc_bias * ADCVoltage) / (ADC_VDD - ADCVoltage); 
	return TRes;
}

extern int mtktscpu_get_hw_temp();
static int mtk_cpu_temp()
{
	int soc_temp;
	soc_temp = mtktscpu_get_hw_temp();
	printf("CPU Temp: %d C\n", soc_temp/1000);

	return 0;
}



#define GPIO_WIFI_3V3_EN 13
#define GPIO_WIFI_2V8_EN 54
#define GPIO_WIFI_1V8_EN 12
static int board_wifi_pwr(int enable)
{
	gpio_request(GPIO_WIFI_3V3_EN, "wifi_3v3");
	gpio_request(GPIO_WIFI_2V8_EN, "wifi_2v8");
	gpio_request(GPIO_WIFI_1V8_EN, "wifi_1v8");

        gpio_direction_output(GPIO_WIFI_3V3_EN, enable);
        udelay(100 * 1000);
        gpio_direction_output(GPIO_WIFI_2V8_EN, enable);
        gpio_direction_output(GPIO_WIFI_1V8_EN, enable);
	
	return 0;
}


#define TOTAL_I2C_BUS 3

static void board_fl_gpio_init(void);
static int do_utest_inventory(cmd_tbl_t *cmdtp, int flag,
        int argc, char *const argv[])
{
        int i=0;
	int hwid;
	char cmd[64];
	int total_bus=TOTAL_I2C_BUS;
	board_fl_gpio_init();
	
	hwid = board_get_hwid(cmd);
	if(2 == hwid){
		i = 1;
		total_bus = 4;
	}

	for(i; i<total_bus; i++)
	{
	        printf("I2C BUS%d: \n", i);
        	        memset((void *)cmd, 0 , sizeof(cmd));
                	sprintf(cmd, "i2c dev %d; i2c probe", i);
                	run_command(cmd, 0);
	        printf("\n");
	}

        return CMD_RET_SUCCESS;
}

#define BOARD_MMC_DEV 0
static int do_utest_board_info(cmd_tbl_t *cmdtp, int flag,
        int argc, char *const argv[])
{
	char cmd[64];
	u32 dev_no = BOARD_MMC_DEV;

	printf("HW boardid: \n");
		memset((void *)cmd, 0 , sizeof(cmd));
        	sprintf(cmd, "hwid");
        	run_command(cmd, 0);
	printf("\n");

	printf("MMC Info:\n");
		memset((void *)cmd, 0 , sizeof(cmd));
		sprintf(cmd, "mmc dev %d; mmcinfo", dev_no);
        	run_command(cmd, 0);
	printf("\n");
	
	

        return CMD_RET_SUCCESS;
}

static int utest_wifi_pwr(cmd_tbl_t *cmdtp, int flag,
        int argc, char *const argv[]);
static int do_utest_wifi(cmd_tbl_t *cmdtp, int flag,
        int argc, char *const argv[])
{
	if(strcmp(argv[1], "info") == 0)
		printf("Not implement\n");

	return utest_wifi_pwr(cmdtp, flag, argc, argv);

        return CMD_RET_SUCCESS;
}

static int do_utest_batt(cmd_tbl_t *cmdtp, int flag,
        int argc, char *const argv[])
{
	board_pmic_init();

	printf("Battery Info:\n");
	printf("Valtage: %d mV\n", board_pmic_get_fg_voltage());
	printf("Current: %d uA\n", board_pmic_get_fg_current());
	printf("Charging Status: %s\n",board_pmic_get_fg_charging_status()?"Discharging":"Charging");
	printf("Temperature: %d C\n", board_pmic_get_fg_temp());
	printf("CC Value(Cap): %d(no meaning here, please check in/decrease)\n", board_pmic_get_fg_cc());  

        return CMD_RET_SUCCESS;
}

static int board_cpu_temp(cmd_tbl_t *cmdtp, int flag,
        int argc, char *const argv[])
{
	mtk_cpu_temp();

	return CMD_RET_SUCCESS;
}

static int board_battery_temp(cmd_tbl_t *cmdtp, int flag,
        int argc, char *const argv[])
{
        printf("Battery Temp: %d C\n", board_pmic_get_fg_temp());
        return CMD_RET_SUCCESS;
}

static int board_epd_temp(cmd_tbl_t *cmdtp, int flag,
        int argc, char *const argv[])
{
        printf("EPD Temp: %d C\n", board_epd_get_temp());
        return CMD_RET_SUCCESS;
}

#define BOARD_ADC_CHANNEL 4
#define WPC_ADC_NO 1
#define WPC_ADC_PULLUP_REST 47000
static int board_adc_temp(cmd_tbl_t *cmdtp, int flag,
        int argc, char *const argv[])
{
	unsigned int data;
	char cmd[64];
	int hwid = board_get_hwid(cmd);
	int adc_bias[BOARD_ADC_CHANNEL]={ADC_BIAS_OHM, ADC_BIAS_OHM, ADC_BIAS_OHM, ADC_BIAS_OHM};
	int ret, i;
	if ( hwid >= HWID_DVT )
	{
		adc_bias[WPC_ADC_NO] = WPC_ADC_PULLUP_REST;
	}

	for(i=0; i<BOARD_ADC_CHANNEL; i++) {
		ret = adc_channel_single_shot("adc@11001000", i,
                                      &data);
        	if (ret)
                	printf("Error getting single shot for channel %d\n",
                       i);
		printf("ADC Temp%i: %d C\n", i, ntc_to_temp(adc_to_ntc(data, adc_bias[i])));
	}

        return CMD_RET_SUCCESS;
        //return mtk_board_adc_temp();
}

static cmd_tbl_t temp_info_table[] = {
        U_BOOT_CMD_MKENT(cpu, 2, 0, board_cpu_temp, "", ""),
        U_BOOT_CMD_MKENT(battery, 2, 0, board_battery_temp, "", ""),
        U_BOOT_CMD_MKENT(epd, 2, 0, board_epd_temp, "", ""),
	U_BOOT_CMD_MKENT(adc, 2, 0, board_adc_temp, "", ""),
};

static int do_utest_temp_info(cmd_tbl_t *cmdtp, int flag,
        int argc, char *const argv[])
{
        cmd_tbl_t *c;
        int ret = CMD_RET_USAGE;
	int i;

        /* Strip off leading 'temp' command argument */
        argc--;
        argv++;


        if(1 > argc)
        {
		for(i=0; i < ARRAY_SIZE(temp_info_table); i++)
		{
			c = &temp_info_table[i];
			if(c)
				ret = c->cmd(cmdtp, flag, argc, argv);

		}
	}else {
        	c = find_cmd_tbl(argv[0], &temp_info_table[0], ARRAY_SIZE(temp_info_table));

        	if (c)
                	ret = c->cmd(cmdtp, flag, argc, argv);
	}

        return ret;
}

//Hall sensor

static u8 board_hall_status()
{
	u8 value;
	
	board_pmic_init();
        if (utest_i2c_read(CONFIG_PMIC_I2C_BUS, CONFIG_PMIC_I2C_SLAVE, BD71828_REG_HALL_STATUS, 1, &value ,1))
		printf("Read ERROR\n");

	return (value & 0x1);
}

static int do_utest_hall_info(cmd_tbl_t *cmdtp, int flag,
        int argc, char *const argv[])
{
	u8 status;
 
	status = board_hall_status();
        printf("Hall sensor status: %d (%s magnet detected )\n",status, status?"No":"A");

        return CMD_RET_SUCCESS;
}

//End Hall sensor



//Front Light
#define GPIO_FL_AMBER_EN_BELLATRIX 32
#define GPIO_FL_WHITE_EN_BELLATRIX 115
#define GPIO_FL_AMBER_EN_MALBEC 45
#define GPIO_FL_WHITE_EN_MALBEC 47

static void board_fl_gpio_init(void)
{
        gpio_request(GPIO_FL_AMBER_EN_MALBEC, "amber_fl");
        gpio_request(GPIO_FL_WHITE_EN_MALBEC, "white_fl");
	gpio_request(GPIO_FL_AMBER_EN_BELLATRIX, "amber_fl1");
        gpio_request(GPIO_FL_WHITE_EN_BELLATRIX, "white_fl1");

        gpio_direction_output(GPIO_FL_WHITE_EN_MALBEC, 1);
        gpio_direction_output(GPIO_FL_AMBER_EN_MALBEC, 1);
	gpio_direction_output(GPIO_FL_WHITE_EN_BELLATRIX, 1);
        gpio_direction_output(GPIO_FL_AMBER_EN_BELLATRIX, 1);
        gpio_free(GPIO_FL_AMBER_EN_BELLATRIX);
        gpio_free(GPIO_FL_WHITE_EN_BELLATRIX);
	gpio_free(GPIO_FL_AMBER_EN_MALBEC);
        gpio_free(GPIO_FL_WHITE_EN_MALBEC);
}

 
#if 0

#define FL_MAX_BRIGHTNESS	2047

#define LM3692X_REV             0x0
#define LM3692X_RESET           0x1
#define LM3692X_EN              0x10
#define LM3692X_BRT_CTRL        0x11
#define LM3692X_PWM_CTRL        0x12
#define LM3692X_BOOST_CTRL      0x13
#define LM3692X_AUTO_FREQ_HI    0x15
#define LM3692X_AUTO_FREQ_LO    0x16
#define LM3692X_BL_ADJ_THRESH   0x17
#define LM3692X_BRT_LSB         0x18
#define LM3692X_BRT_MSB         0x19
#define LM3692X_FAULT_CTRL      0x1e
#define LM3692X_FAULT_FLAGS     0x1f

#define LM3692X_SW_RESET        BIT(0)
#define LM3692X_DEVICE_EN       BIT(0)
#define LM3692X_LED1_EN         BIT(1)
#define LM3692X_LED2_EN         BIT(2)
#define LM36923_LED3_EN         BIT(3)
#define LM3692X_ENABLE_MASK     (LM3692X_DEVICE_EN | LM3692X_LED1_EN | \
                                 LM3692X_LED2_EN | LM36923_LED3_EN)


static int lm3692x_brightness(u8 bus_num, u8 addr, int brightness)
{
        u8 val = 0;
	int ret;

        if(0 == brightness)
        {
                val = 0x00;
                ret = utest_i2c_write(bus_num, addr, LM3692X_EN, 1, &val, 1);
        }else{

                val = 0x0F;
                ret = utest_i2c_write(bus_num, addr, LM3692X_EN, 1, &val, 1);
        }

        val = 0x85;
        ret = utest_i2c_write(bus_num, addr, LM3692X_BRT_CTRL, 1, &val, 1);

        val = brightness & 0x07;
        ret = utest_i2c_write(bus_num, addr, LM3692X_BRT_LSB, 1, &val, 1);
        val = (brightness>>3) & 0xff;
        ret = utest_i2c_write(bus_num, addr, LM3692X_BRT_MSB, 1, &val, 1);

	return ret;
}


#define CONFIG_AMBER_I2C_BUS 2
#define CONFIG_AMBER_I2C_SLAVE 0x36
#define GPIO_FL_AMBER_EN 32
#define GPIO_FL_WHITE_EN 115
#define CONFIG_WHITE_I2C_BUS 2
#define CONFIG_WHITE_I2C_SLAVE 0x37


static void board_fl_gpio_init(void)
{
        gpio_request(GPIO_FL_AMBER_EN, "amber_fl");
        gpio_request(GPIO_FL_WHITE_EN, "white_fl");

        gpio_direction_output(GPIO_FL_WHITE_EN, 1);
        gpio_direction_output(GPIO_FL_AMBER_EN, 1);
	gpio_free(GPIO_FL_AMBER_EN);
	gpio_free(GPIO_FL_WHITE_EN);
}


static int board_amber_fl(int brightness)
{
	board_fl_gpio_init();
	return lm3692x_brightness(CONFIG_AMBER_I2C_BUS, CONFIG_AMBER_I2C_SLAVE, brightness);
	
}

static int board_white_fl(int brightness)
{
	board_fl_gpio_init();
	return lm3692x_brightness(CONFIG_WHITE_I2C_BUS, CONFIG_WHITE_I2C_SLAVE, brightness);

}


static int do_utest_fl(cmd_tbl_t *cmdtp, int flag,
        int argc, char *const argv[])
{
        int brightness;

	if(3 > argc)
	{
		return CMD_RET_USAGE;
	}

	brightness = (int)simple_strtoul (argv[2], NULL, 10);
	printf("Set %s to %d\n", argv[1], brightness);
	if(FL_MAX_BRIGHTNESS < brightness )
	{
		return CMD_RET_USAGE;
	}


	if (strcmp(argv[1], "white") == 0) 
	{
		board_white_fl(brightness);
	}else if(strcmp(argv[1], "amber") == 0){
		board_amber_fl(brightness);
	}else
		return CMD_RET_USAGE;


        return CMD_RET_SUCCESS;
}
#else
static int do_utest_fl(cmd_tbl_t *cmdtp, int flag,
        int argc, char *const argv[])
{
	char cmd[64];
	int hwid; 
	char *fl_white, *fl_amber;


	hwid = board_get_hwid(cmd);

	switch(hwid){
		case HWID_PROTO:
			fl_white = "frontlight-malbec-proto-white";
			fl_amber = "frontlight-malbec-proto-amber";
			break;

		case HWID_BELLATRIX:
			fl_white = "frontlight-bellatrix-white";
                        fl_amber = "frontlight-bellatrix-amber";
			break;

		case HWID_HVT_POR:
			fl_white = "frontlight-lm3692x-white";
                	fl_amber = "frontlight-lm3692x-amber";
                        break;

		default:
			fl_white = "frontlight-fp9966-white";
                        fl_amber = "frontlight-fp9966-amber";
			break;
	}


	memset((void *)cmd, 0 , sizeof(cmd));

        if(3 > argc)
        {
                return CMD_RET_USAGE;
        }

        printf("Set %s to %s\n", argv[1], argv[2]);


        if (strcmp(argv[1], "white") == 0)
        {
                sprintf(cmd, "fl dev %s; fl set %s",fl_white, argv[2]);
        	run_command(cmd, 0);
        }else if(strcmp(argv[1], "amber") == 0){
                sprintf(cmd, "fl dev %s; fl set %s",fl_amber, argv[2]);
                run_command(cmd, 0);
        }else
                return CMD_RET_USAGE;


        return CMD_RET_SUCCESS;
}

#endif
//End Front Light

//POWER CONTROL

static int utest_wifi_pwr(cmd_tbl_t *cmdtp, int flag,
        int argc, char *const argv[])
{
        printf("%s\n",__FUNCTION__);
	if (strcmp(argv[1], "on") == 0)
                board_wifi_pwr(1);
        else if (strcmp(argv[1], "off") == 0)
                board_wifi_pwr(0);
        else
                return CMD_RET_USAGE;

	return CMD_RET_SUCCESS;
}

static int utest_touch_pwr(cmd_tbl_t *cmdtp, int flag,
        int argc, char *const argv[])
{
        if (strcmp(argv[1], "on") == 0)
		board_pmic_touch_pwr(1);
	else if (strcmp(argv[1], "off") == 0)
		board_pmic_touch_pwr(0);
	else
		return CMD_RET_USAGE;

	return CMD_RET_SUCCESS;
}


static int utest_epd_pwr(cmd_tbl_t *cmdtp, int flag,
        int argc, char *const argv[])
{
	if (strcmp(argv[1], "on") == 0)
        	board_epd_pmic_pwr(1);
	else if (strcmp(argv[1], "off") == 0)
        	board_epd_pmic_pwr(0);
	else if (strcmp(argv[1], "nightmode") == 0)
		board_epd_pmic_nm(1);
	else
		return CMD_RET_USAGE;

	return CMD_RET_SUCCESS;
}

static int utest_adc_pwr(cmd_tbl_t *cmdtp, int flag,
        int argc, char *const argv[])
{
        if (strcmp(argv[1], "on") == 0)
                board_pmic_adc_pwr(1);
        else if (strcmp(argv[1], "off") == 0)
                board_pmic_adc_pwr(0);
        else
                return CMD_RET_USAGE;

        return CMD_RET_SUCCESS;
}

static cmd_tbl_t pwr_ctrl_table[] = {
	U_BOOT_CMD_MKENT(wifi, 2, 0, utest_wifi_pwr, "", ""),
	U_BOOT_CMD_MKENT(touch, 2, 0, utest_touch_pwr, "", ""),
	U_BOOT_CMD_MKENT(epd, 2, 0, utest_epd_pwr, "", ""),
	U_BOOT_CMD_MKENT(adc, 2, 0, utest_adc_pwr, "", ""),
};


static int do_utest_power(cmd_tbl_t *cmdtp, int flag,
        int argc, char *const argv[])
{
        cmd_tbl_t *c;
        int ret = CMD_RET_USAGE;

	if (3 > argc)
		return CMD_RET_USAGE;

        /* Strip off leading 'power' command argument */
        argc--;
        argv++;
	printf("Turn %s %s\n", argv[0], argv[1]);
        c = find_cmd_tbl(argv[0], &pwr_ctrl_table[0], ARRAY_SIZE(pwr_ctrl_table));

        if (c)
                ret = c->cmd(cmdtp, flag, argc, argv);

        return ret;
}


//End POWER CONTROL

//CHARGER CONTROL

static int board_usb_charger(char *const argv)
{
        if (strcmp(argv, "on") == 0)
		board_charger_ctrl(1);
	else if (strcmp(argv, "fast") == 0)
		board_charger_ctrl(2);
	else if (strcmp(argv, "off") == 0)
		board_charger_ctrl(0);
        return CMD_RET_SUCCESS;

}

static int board_wireless_charger(char *const argv)
{

	return board_usb_charger(argv);
}


static int do_utest_charger(cmd_tbl_t *cmdtp, int flag,
        int argc, char *const argv[])
{
        int ret = CMD_RET_USAGE;

        if(3 > argc)
        {
                return CMD_RET_USAGE;
        }


        if (strcmp(argv[1], "usb") == 0)
        {
                ret = board_usb_charger(argv[2]);
        }else if(strcmp(argv[1], "wireless") == 0){
                ret = board_wireless_charger(argv[2]);
        }


        return ret;
}



//End CHARGER CONTROL


static int do_utest_lpm(cmd_tbl_t *cmdtp, int flag,
        int argc, char *const argv[])
{
        cmd_tbl_t *c;

#if 0
	if(strcmp(argv[1], "hib") == 0){
                board_enter_hib();
        }else  if(strcmp(argv[1], "ship") == 0){
                return board_enter_ship();
        }
#endif
        c = find_cmd(argv[0]);

        if (c)
                return  c->cmd(cmdtp, flag, argc, argv);
        else
                return CMD_RET_USAGE;


        return CMD_RET_SUCCESS;
}

extern int fastboot_handle_command(char *cmd_string, char *response);
static int do_utest_erase_dram(cmd_tbl_t *cmdtp, int flag,
        int argc, char *const argv[])
{
	char res[100];

	fastboot_handle_command("erase:dram", res);

        return CMD_RET_SUCCESS;
}


static cmd_tbl_t cmd_utest_sub[] = {
        U_BOOT_CMD_MKENT(inventory, 5, 0, do_utest_inventory, "", ""),
        U_BOOT_CMD_MKENT(wifi, 5, 0, do_utest_wifi, "", ""),
	U_BOOT_CMD_MKENT(battery, 5, 0, do_utest_batt, "", ""),
	U_BOOT_CMD_MKENT(board_info, 5, 0, do_utest_board_info, "", ""),
	U_BOOT_CMD_MKENT(temp_info, 5, 0, do_utest_temp_info, "", ""),
	U_BOOT_CMD_MKENT(hall_info, 5, 0, do_utest_hall_info, "", ""),
        U_BOOT_CMD_MKENT(fl, 5, 0, do_utest_fl, "", ""),
	U_BOOT_CMD_MKENT(power, 5, 0, do_utest_power, "", ""),
	U_BOOT_CMD_MKENT(lpm, 5, 0, do_utest_lpm, "", ""),
	U_BOOT_CMD_MKENT(charger, 5, 0, do_utest_charger, "", ""),
	U_BOOT_CMD_MKENT(erase, 5, 0, do_utest_erase_dram, "", ""),

};


static int do_utest(cmd_tbl_t *cmdtp, int flag,
        int argc, char *const argv[])
{
        cmd_tbl_t *c;
	int ret = CMD_RET_USAGE;
        /* Strip off leading 'utest' command argument */
        argc--;
        argv++;

        c = find_cmd_tbl(argv[0], &cmd_utest_sub[0], ARRAY_SIZE(cmd_utest_sub));

        if (c)
                ret = c->cmd(cmdtp, flag, argc, argv);
        
	if(CMD_RET_SUCCESS == ret)
		printf("\r\nUTEST_TEST_PASS\r\n\n\n");
	else
		printf("\r\nUTEST_TEST_FAIL\r\n\n\n");
        
        return ret;
}




U_BOOT_CMD(
        utest,    8,      0,      do_utest,
        "Diags tests suite",
	"utest board_info				--show board infomation\n"
	"utest inventory				--check and list all components\n"
	"utest wifi [on | off | info ]				--Wifi power control\n"
	"utest battery [ info | charging ]		--Battery test\n"
	"utest charger [ type | status | enable/disable ]	--Charger test\n"
	"utest fl [ white | amber ] [ brightness ]		--fl test\n"
	"utest power [power_rails] [on | off ]		--power contorl\n"
	"utest temp <cpu | battery | epd | adc				--show all temp sensor info\n"
	"utest hall_info				--show hall sensor status\n"
	"utest lpm [ idle | suspend | hib | ship ]	--low power mode tests suite\n"
);


#endif

