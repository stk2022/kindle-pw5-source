// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2020
 * 
 */
#include <common.h>
#include <command.h>
#include <mapmem.h>
#include <memalign.h>
#include <asm/io.h>
#if 0
#include <asm/gpio.h>
#endif

#ifdef DIAG
#include <emi_bt_hdr.h>
#include <emi_mcu_hdr.h>
#include <emi_wifi_hdr.h>
#endif

#define INFRACFG_AO_BASE (0x10001000)
#define PERICFG_BASE (0x10003000)
#define SLEEP_BASE (0x10006000)
#define TOPRGU_BASE (0x10007000)

#define CONSYS_EMI_SIZE (0x400000)		//4M

#define CONSYS_CPU_SW_RST_REG (TOPRGU_BASE+0x18)
#define CONSYS_TOP_CLKCG_CLR_REG (0x10000084)
#define CONSYS_POWER_CONFIG_EN (SLEEP_BASE)
#define CONSYS_TOP1_PWR_CTRL_REG (SLEEP_BASE+0x32c)
#define CONSYS_PWR_CONN_ACK_REG (SLEEP_BASE+0x180)
#define CONSYS_PWR_CONN_ACK_S_REG (SLEEP_BASE+0x184)

#define CONSYS_VER_ID_REG (0x180B1010)
#define CONSYS_CFG_ID_REG (0x180B101c)
#define CONSYS_HW_ID_REG (0x18002000)
#define CONSYS_FW_ID_REG (0x18002004)
#define CONSYS_ROM_RAM_DELSEL_REG (0x18070114)
#define CONSYS_MCU_CFG_ACR_REG (0x18070110)
#define CONSYS_AFE_REG (0x180b0000)
#define CONSYS_TOPAXI_PROT_EN_SET      (INFRACFG_AO_BASE + 0x02A0)
#define CONSYS_TOPAXI_PROT_EN_CLR      (INFRACFG_AO_BASE + 0x02A4)
#define CONSYS_TOPAXI_PROT_EN_1_SET    (INFRACFG_AO_BASE + 0x02A8)
#define CONSYS_TOPAXI_PROT_EN_1_CLR    (INFRACFG_AO_BASE + 0x02AC)
#define CONSYS_TOPAXI_PROT_STA1        (INFRACFG_AO_BASE + 0x0228)
#define CONSYS_TOPAXI_PROT_STA1_1      (INFRACFG_AO_BASE + 0x0258)
#define CONSYS_SPM_APSRC_REG (SLEEP_BASE+0x6f8)
#define CONSYS_SPM_DDR_EN_REG (SLEEP_BASE+0x6fc)

#define CONSYS_SPM_APSRC_VALUE (0x00000005)
#define CONSYS_SPM_DDR_EN_VALUE (0x00050505)

/*CONSYS_CPU_SW_RST_REG*/
#define CONSYS_CONN_SW_RST_BIT (0x1 << 9)
#define CONSYS_CPU_SW_RST_BIT (0x1 << 12)
#define CONSYS_CPU_SW_RST_CTRL_KEY (0x88 << 24)
#define CONSYS_TOP_CLKCG_BIT (0x1 << 26)
#define CONSYS_POWER_CONFIG_EN_VAL (0x0b160001)
/*CONSYS_TOP1_PWR_CTRL_REG*/
#define CONSYS_SPM_PWR_RST_BIT (0x1 << 0)
#define CONSYS_SPM_PWR_ISO_S_BIT (0x1 << 1)
#define CONSYS_SPM_PWR_ON_BIT (0x1 << 2)
#define CONSYS_SPM_PWR_ON_S_BIT (0x1 << 3)
#define CONSYS_CLK_CTRL_BIT (0x1 << 4)
#define CONSYS_SRAM_CONN_PD_BIT (0x1 << 8)

/*CONSYS_PWR_CONN_ACK_REG*/
#define CONSYS_PWR_ON_ACK_BIT (0x1 << 1)

/*CONSYS_PWR_CONN_ACK_S_REG*/
#define CONSYS_PWR_ON_ACK_S_BIT (0x1 << 1)
#define CONSYS_TOP2_PWR_ON_ACK_BIT (0x1 << 30)

/*CONSYS_WD_SYS_RST_REG*/
#define CONSYS_WD_SYS_RST_CTRL_KEY (0x88 << 24)
#define CONSYS_WD_SYS_RST_BIT (0x1 << 9)

/*CONSYS_MCU_CFG_ACR_REG*/
#define CONSYS_MCU_CFG_ACR_MBIST_BIT (0x1 << 18)

#define CONSYS_AHB_RX_PROT_MASK    (0x1 << 21)//21
#define CONSYS_AHB_TX_PROT_MASK    (0x1 << 13)//13
#define CONSYS_AXI_RX_PROT_MASK    (0x1 << 14)//14
#define CONSYS_AXI_TX_PROT_MASK    (0x1 << 18)//18

#define DRV_Reg32(addr)				readl(addr)
#define DRV_WriteReg32(addr, value) writel(value, addr)
#define BTIF_INFO_LOG				printf

#define CONSYS_SET_BIT(REG, BITVAL) {\
	*((volatile u32*)(REG)) |= ((u32)(BITVAL));\
	dsb();\
}
#define CONSYS_CLR_BIT(REG, BITVAL){\
	(*(volatile u32*)(REG)) &= ~((u32)(BITVAL));\
	dsb();\
}
#define CONSYS_CLR_BIT_WITH_KEY(REG, BITVAL, KEY) {\
	u32 val = (*(volatile u32*)(REG)); \
	val &= ~((u32)(BITVAL)); \
	val |= ((u32)(KEY)); \
	(*(volatile u32*)(REG)) = val;\
	dsb();\
}

typedef struct _PATCH_INFO_{
	unsigned char *patchAddr;
	unsigned long patchLength;
}PATCH_INFO, *P_PATCH_INFO;

static u32 power_on_status = 0xFF;

/* Set pinmux for the interface between D-die and A-die */
static u32 wmt_set_spi_mode_pinmux(u32 enable)
{
	u32 iRet = 0;
	u32 tmp = 0;
	u32 addr = 0;

	/* TPCKGEN 32k */
	addr = 0x10000220;
	tmp = DRV_Reg32(addr);
	tmp = tmp & 0xbfffffff;  //Mask : 1011-1111  1111-1111	1111-1111  1111-1111
	tmp = tmp | 0x40000000;  //Value: 0100-0000  0000-0000	0000-0000  0000-0000
	DRV_WriteReg32(addr, tmp);
	BTIF_INFO_LOG("(RegAddr, RegVal):(0x%08x, 0x%08x)\n", addr, DRV_Reg32(addr));

	addr = 0x10000228;
	tmp = DRV_Reg32(addr);
	tmp = tmp & 0xffffdfff;  //Mask : 1111-1111  1111-1111	1101-1111  1111-1111
	tmp = tmp | 0x2000;      //Value: 0000-0000  0000-0000	0010-0000  0000-0000
	DRV_WriteReg32(addr, tmp);
	BTIF_INFO_LOG("(RegAddr, RegVal):(0x%08x, 0x%08x)\n", addr, DRV_Reg32(addr));

	/*Pinmux setting for MT6631 I/F*/
	if (enable != 0) {
		addr = 0x10005260;
		tmp = DRV_Reg32(addr);
		tmp = tmp & 0xc0ffffff;  //Mask : 1100-0000  1111-1111	1111-1111  1111-1111
		tmp = tmp | 0x1b000000;  //Value: 0001-1011  0000-0000	0000-0000  0000-0000
		DRV_WriteReg32(addr, tmp);
		BTIF_INFO_LOG("(RegAddr, RegVal):(0x%08x, 0x%08x)\n", addr, DRV_Reg32(addr));

		addr = 0x10005270;
		tmp = DRV_Reg32(addr);
		tmp = tmp & 0xffffffc0;  //Mask : 1111-1111  1111-1111	1111-1111  1100-0000
		tmp = tmp | 0x1b;		 //Value: 0000-0000  0000-0000	0000-0000  0001-1011
		DRV_WriteReg32(addr, tmp);
		BTIF_INFO_LOG("(RegAddr, RegVal):(0x%08x, 0x%08x)\n", addr, DRV_Reg32(addr));

		addr = 0x10005200;
		tmp = DRV_Reg32(addr);
		tmp = tmp & 0xfffc0007;  //Mask : 1111-1111  1111-1100	0000-0000  0000-0111
		tmp = tmp | 0x9248; 	 //Value: 0000-0000  0000-0000	1001-0010  0100-1000
		DRV_WriteReg32(addr, tmp);
		BTIF_INFO_LOG("(RegAddr, RegVal):(0x%08x, 0x%08x)\n", addr, DRV_Reg32(addr));

		addr = 0x10005750;
		tmp = DRV_Reg32(addr);
		tmp = tmp & 0xfffffff;	 //Mask : 0000-1111  1111-1111	1111-1111  1111-1111
		tmp = tmp | 0x70000000;  //Value: 0111-0000  0000-0000	0000-0000  0000-0000
		DRV_WriteReg32(addr, tmp);
		BTIF_INFO_LOG("(RegAddr, RegVal):(0x%08x, 0x%08x)\n", addr, DRV_Reg32(addr));

		addr = 0x10005710;
		tmp = DRV_Reg32(addr);
		tmp = tmp & 0xfffffff;	 //Mask : 0000-1111  1111-1111	1111-1111  1111-1111
		tmp = tmp | 0x70000000;  //Value: 0111-0000  0000-0000	0000-0000  0000-0000
		DRV_WriteReg32(addr, tmp);
		BTIF_INFO_LOG("(RegAddr, RegVal):(0x%08x, 0x%08x)\n", addr, DRV_Reg32(addr));
	} else {
		addr = 0x10005260;
		tmp = DRV_Reg32(addr);
		tmp = tmp & 0xc0ffffff;  //Mask : 1100-0000  1111-1111	1111-1111  1111-1111
		tmp = tmp | 0x0;		 //Value: 0000-0000  0000-0000	0000-0000  0000-0000
		DRV_WriteReg32(addr, tmp);
		BTIF_INFO_LOG("(RegAddr, RegVal):(0x%08x, 0x%08x)\n", addr, DRV_Reg32(addr));

		addr = 0x10005270;
		tmp = DRV_Reg32(addr);
		tmp = tmp & 0xffffffc0;  //Mask : 1111-1111  1111-1111	1111-1111  1100-0000
		tmp = tmp | 0x0;		 //Value: 0000-0000  0000-0000	0000-0000  0000-0000
		DRV_WriteReg32(addr, tmp);
		BTIF_INFO_LOG("(RegAddr, RegVal):(0x%08x, 0x%08x)\n", addr, DRV_Reg32(addr));

		addr = 0x10005200;
		tmp = DRV_Reg32(addr);
		tmp = tmp & 0xfffc0007;  //Mask : 1111-1111  1111-1100	0000-0000  0000-0111
		tmp = tmp | 0x0;		 //Value: 0000-0000  0000-0000	0000-0000  0000-0000
		DRV_WriteReg32(addr, tmp);
		BTIF_INFO_LOG("(RegAddr, RegVal):(0x%08x, 0x%08x)\n", addr, DRV_Reg32(addr));
	}

	return iRet;
}

#if 0
#define MT8110P1_WB_1V8_EN_GPIO 57
#define MT8110P1_WB_3V3_EN_GPIO 56
#define MT8110P2_WB_1V8_EN_GPIO 12
#define MT8110P2_WB_3V3_EN_GPIO 13
#endif

// Config 1V8 and 3V3 GPIO enable according to Board
static int consys_hw_vcn18_ctrl(u32 enable)
{
#if 0
	u32 gpio_bk_en1, gpio_bk_en2;
	int ret;
#endif

	BTIF_INFO_LOG("Current SYS_BoARD[%s]\n", CONFIG_SYS_BOARD);
	BTIF_INFO_LOG("1V8 3V3 always on, do not config\n");
	return -1;

#if 0
	if (!strcmp(CONFIG_SYS_BOARD, "mt8110_p1")) {
		gpio_bk_en1 = MT8110P1_WB_1V8_EN_GPIO;
		gpio_bk_en2 = MT8110P1_WB_3V3_EN_GPIO;
	} else if (!strcmp(CONFIG_SYS_BOARD, "mt8110_p2_d1")) {
		gpio_bk_en1 = MT8110P2_WB_1V8_EN_GPIO;
		gpio_bk_en2 = MT8110P2_WB_3V3_EN_GPIO;
	} else if (!strcmp(CONFIG_SYS_BOARD, "mt8110_p2_d2")) {
		gpio_bk_en1 = MT8110P2_WB_1V8_EN_GPIO;
		gpio_bk_en2 = MT8110P2_WB_3V3_EN_GPIO;
	} else {
		return -1;
	}

	ret = gpio_request(gpio_bk_en1, "consys 1v8_en");
	if(ret == 0) {
		BTIF_INFO_LOG("gpio_request %d ok\n", gpio_bk_en1);
	} else {
		BTIF_INFO_LOG("gpio_request %d error\n", gpio_bk_en1);
		return -1;
	}

	ret = gpio_request(gpio_bk_en2, "consys 3v3_en");
	if(ret == 0) {
		BTIF_INFO_LOG("gpio_request %d ok\n", gpio_bk_en2);
	} else {
		gpio_free(gpio_bk_en1);
		BTIF_INFO_LOG("gpio_request %d error\n", gpio_bk_en2);
		return -1;
	}

	if (enable) {
		gpio_direction_output(gpio_bk_en1, 1);	// pull up
		BTIF_INFO_LOG("enable VCN18 ok\n");

		udelay(10);

		gpio_direction_output(gpio_bk_en2, 1);	// pull up
		BTIF_INFO_LOG("enable VCN33 ok\n");
	} else {
		gpio_direction_output(gpio_bk_en2, 0);
		BTIF_INFO_LOG("disable VCN33 ok\n");

		gpio_direction_output(gpio_bk_en1, 0);
		BTIF_INFO_LOG("disable VCN18 ok\n");
	}

	gpio_free(gpio_bk_en1);
	gpio_free(gpio_bk_en2);
#endif

	return 0;
}

#ifdef DIAG
static void wmt_get_consys_emi_addr(u32* addr, u32* size)
{
	u32 emi_base;
	u32 map_base;
	//SZ_16M align u8 emi_data_buff[CONSYS_EMI_SIZE];
	ALLOC_ALIGN_BUFFER(u8, emi_data_buff, CONSYS_EMI_SIZE, SZ_16M);

	if(!emi_data_buff)
		BTIF_INFO_LOG("Allocate connsys EMI memory fail\n");
	else
		BTIF_INFO_LOG("emi base for connsys 0x%X\n", (u32)emi_data_buff);

	emi_base = (u32)emi_data_buff;
	map_base = (emi_base >> 24) & 0xFFF;

	DRV_WriteReg32(INFRACFG_AO_BASE + 0x380,(DRV_Reg32(INFRACFG_AO_BASE+0x380)&(~0xFFF)) | map_base);
	BTIF_INFO_LOG("mapping register value(0x%X)\n",DRV_Reg32(INFRACFG_AO_BASE+0x380));
	map_base = ((PERICFG_BASE >> 24) & 0xFFF) << 16;
	DRV_WriteReg32(INFRACFG_AO_BASE + 0x388,(DRV_Reg32(INFRACFG_AO_BASE+0x388)&(~0xFFF0000)) | map_base);
	BTIF_INFO_LOG("peri mapping register value(0x%X)\n",DRV_Reg32(INFRACFG_AO_BASE+0x388));

	*addr = emi_base;
	*size = CONSYS_EMI_SIZE;

}

static u32 wmt_rom_patch_download(unsigned char*patch, u32 size, u32 consys_emi_addr, u32 consys_emi_size)
{
	u32 patchSize;
	unsigned char ucDateTime[16];
	unsigned char ucPLat[4];
	u16 u2HwVer = 0;
	u16 u2SwVer = 0;
	u32 u4PatchAddr = 0;
	u32 u4PatchType = -1;

	memcpy(ucDateTime, patch, 15);
	ucDateTime[15] = '\0';
	ucPLat[0] = patch[16];
	ucPLat[1] = patch[17];
	ucPLat[2] = patch[18];
	ucPLat[3] = patch[19];
	u2HwVer = (patch[20] << 8) | patch[21];
	u2SwVer = (patch[22] << 8) | patch[23];
	u4PatchAddr = (patch[27] << 24) | (patch[26] << 16) | (patch[25] << 8) | patch[24];
	u4PatchType = patch[31];

	patchSize = size - 48;

	BTIF_INFO_LOG("===========================================\n");
	BTIF_INFO_LOG("[Combo Patch] Built Time = %s\n", ucDateTime);
	BTIF_INFO_LOG("[Combo Patch] Hw Ver = 0x%04x\n", u2HwVer);
	BTIF_INFO_LOG("[Combo Patch] Sw Ver = 0x%04x\n", u2SwVer);
	BTIF_INFO_LOG("[Combo Patch] Ph Addr = 0x%08x\n", u4PatchAddr);
	BTIF_INFO_LOG("[Combo Patch] Ph Type = 0x%08x\n", u4PatchType);
	BTIF_INFO_LOG("[Combo Patch] Platform = %c%c%c%c\n", ucPLat[0],
			ucPLat[1], ucPLat[2], ucPLat[3]);
	BTIF_INFO_LOG("===========================================\n");

	u4PatchAddr = u4PatchAddr & 0x00ffff00;

	BTIF_INFO_LOG("type(0x%x) entry address(0x%x)\n", u4PatchType, u4PatchAddr);
	switch (u4PatchType) {
	case 0:
		DRV_WriteReg32(0x18002504, u4PatchAddr | 0xF0000000);
		break;
	case 1:
		BTIF_INFO_LOG("type(0x%x) no emi patch\n", u4PatchType);
		break;
	case 2:
		DRV_WriteReg32(0x1800250C, u4PatchAddr | 0xF0000000);
		break;
	case 3:
		DRV_WriteReg32(0x18002508, u4PatchAddr | 0xF0000000);
		break;
	case 4:
		DRV_WriteReg32(0x18002500, u4PatchAddr | 0xF0000000);
		break;
	default:
		BTIF_INFO_LOG("type(0x%x) is error type\n", u4PatchType);
	}
	BTIF_INFO_LOG("patch addr(0x%x) size(0x%x) emi_size(0x%x)\n", u4PatchAddr, patchSize, consys_emi_size);
	if (u4PatchAddr + patchSize < consys_emi_size) {
		memcpy(consys_emi_addr + u4PatchAddr, &(patch[48]), patchSize);
	} else {
		BTIF_INFO_LOG("patch overflow addr(0x%x) size(0x%x) emi_size(0x%x)\n", u4PatchAddr, patchSize, consys_emi_size);
	}

	return 0;
}

static u32 wmt_rom_patch_down(void)
{
	unsigned char *patchAddr = NULL;
	unsigned long patchLength = 0;
	u32 iRet = 0;
	u32 index = 0;
	PATCH_INFO gPatchInfo[] = {
		[0] = {g_aucMcuEmiPthBuffer, g_u4McuEmiPthSize},
		[1] = {g_aucBtEmiPthBuffer, g_u4BtEmiPthSize},
		[2] = {g_aucWifiEmiPthBuffer, g_u4WifiEmiPthSize},
	};
	u32 consys_emi_addr;
	u32 consys_emi_size;

	wmt_get_consys_emi_addr(&consys_emi_addr, &consys_emi_size);
	if (!consys_emi_addr) {
		/* stop test because rom patch download fail */
		BTIF_INFO_LOG("Stop test because rom patch download fail\n");
		return -1;
	}

	for (index = 0; index < sizeof (gPatchInfo) / sizeof (PATCH_INFO); index++) {
		/*search correct patch*/
		patchAddr = gPatchInfo[index].patchAddr;
		patchLength = gPatchInfo[index].patchLength;

		iRet = wmt_rom_patch_download(patchAddr, patchLength, consys_emi_addr, consys_emi_size);
		if (iRet) {
			break;
		}
	}

	return iRet;
}

/*
***********************************************************
**
** power on consys mcu
**
*/
u32 wmt_pwr_on_consys_mcu(void)
{
	u32 iRet = 0;
	u32 u4VerId = 0x00;
	u32 tmp = 0;
	u32 addr = 0;
	u32 loop = 0;

	/* Set GPIO54 pinmux for TCXO mode */
	addr = 0x10005230;
	tmp = DRV_Reg32(addr);
	tmp = tmp & 0xffff8fff;  //Mask : 1111-1111  1111-1111	1000-1111  1111-1111
	tmp = tmp | 0x4000;		 //Value: 0000-0000  0000-0000	0100-0000  0000-0000
	DRV_WriteReg32(addr, tmp);
	BTIF_INFO_LOG("(RegAddr, RegVal):(0x%08x, 0x%08x)\n", addr, DRV_Reg32(addr));

	consys_hw_vcn18_ctrl(1);

	wmt_set_spi_mode_pinmux(1);

	//TODO 2.8V

	/* assert CONNSYS CPU SW reset (apply this for default value patching) */
	CONSYS_SET_BIT(CONSYS_CPU_SW_RST_REG, CONSYS_CPU_SW_RST_BIT | CONSYS_CPU_SW_RST_CTRL_KEY);
	/* turn on SPM clock (apply this for SPM's CONNSYS power control related CR accessing) */
	DRV_WriteReg32(CONSYS_POWER_CONFIG_EN, CONSYS_POWER_CONFIG_EN_VAL);

	/* assert "conn_top_on" primary part power on, set "connsys_on_domain_pwr_on"=1 */
	CONSYS_SET_BIT(CONSYS_TOP1_PWR_CTRL_REG, CONSYS_SPM_PWR_ON_BIT);
	/* check "conn_top_on" primary part power status, check "connsys_on_domain_pwr_ack"=1 */
	while (0 == (CONSYS_PWR_ON_ACK_BIT & DRV_Reg32(CONSYS_PWR_CONN_ACK_REG)) && loop < 10)
	{
	    BTIF_INFO_LOG("%s: power on ack not ready, reg value:0x%x, ack bit:0x%x\n", \
			           __func__, \
			           DRV_Reg32(CONSYS_PWR_CONN_ACK_REG), \
			           CONSYS_PWR_ON_ACK_BIT);
		/* wait 5ms */
		udelay(5000);
		loop++;
	}

	BTIF_INFO_LOG("%s: power on ack ready, reg value:0x%x, ack bit:0x%x\n", \
			       	   __func__, \
			           DRV_Reg32(CONSYS_PWR_CONN_ACK_REG), \
			           CONSYS_PWR_ON_ACK_BIT);
	/* assert "conn_top_on" secondary part power on, set "connsys_on_domain_pwr_on_s"=1 */
	CONSYS_SET_BIT(CONSYS_TOP1_PWR_CTRL_REG, CONSYS_SPM_PWR_ON_S_BIT);

	/* check "conn_top_on" secondary part power status, check "connsys_on_domain_pwr_ack_s"=1  */
	while (0 == (CONSYS_PWR_ON_ACK_S_BIT & DRV_Reg32(CONSYS_PWR_CONN_ACK_S_REG)) && loop < 10)
	{
	    BTIF_INFO_LOG("%s: power on ack_s not ready, reg value:0x%x, ack bit:0x%x\n", \
			           __func__, \
			           DRV_Reg32(CONSYS_PWR_CONN_ACK_S_REG), \
			           CONSYS_PWR_ON_ACK_S_BIT);
		/* wait 5ms */
		udelay(5000);
		loop++;
	}

	BTIF_INFO_LOG("%s: power on ack_s ready, reg value:0x%x, ack bit:0x%x\n", \
			       	   __func__, \
			           DRV_Reg32(CONSYS_PWR_CONN_ACK_S_REG), \
			           CONSYS_PWR_ON_ACK_S_BIT);
	/* turn on AP-to-CONNSYS bus clock, set "conn_clk_dis"=0 */
	CONSYS_CLR_BIT(CONSYS_TOP1_PWR_CTRL_REG, CONSYS_CLK_CTRL_BIT);
	/*7.wait for 1us*/
	udelay(3);

	/* de-assert "conn_top_on" isolation, set "connsys_iso_en"=0 */
	CONSYS_CLR_BIT(CONSYS_TOP1_PWR_CTRL_REG, CONSYS_SPM_PWR_ISO_S_BIT);

	/* de-assert CONNSYS S/W reset (TOP RGU CR), set "ap_sw_rst_b"=1 */
	CONSYS_CLR_BIT_WITH_KEY(CONSYS_CPU_SW_RST_REG, CONSYS_CONN_SW_RST_BIT , CONSYS_CPU_SW_RST_CTRL_KEY);

	/* de-assert CONNSYS S/W reset (SPM CR), set "ap_sw_rst_b"=1 */
	CONSYS_SET_BIT(CONSYS_TOP1_PWR_CTRL_REG, CONSYS_SPM_PWR_RST_BIT);

	/* check "conn_top_off" primary part power status, check "connsys_off_domain_pwr_ack"=1 */
	while (0 == (CONSYS_TOP2_PWR_ON_ACK_BIT & DRV_Reg32(CONSYS_PWR_CONN_ACK_REG)) && loop < 10)
	{
		BTIF_INFO_LOG("%s: connsys_off_domain_pwr_ack not ready, reg value:0x%x, ack bit:0x%x\n", \
			           __func__, \
			           DRV_Reg32(CONSYS_PWR_CONN_ACK_REG), \
			           CONSYS_TOP2_PWR_ON_ACK_BIT);
		/* wait 5ms */
		udelay(5000);
		loop++;
	}

	/* check "conn_top_off" secondary part power status, check "connsys_off_domain_pwr_ack_s"=1  */
	while (0 == (CONSYS_TOP2_PWR_ON_ACK_BIT & DRV_Reg32(CONSYS_PWR_CONN_ACK_S_REG)) && loop < 10)
	{
		BTIF_INFO_LOG("%s: power on ack_s not ready, reg value:0x%x, ack bit:0x%x\n", \
			           __func__, \
			           DRV_Reg32(CONSYS_PWR_CONN_ACK_S_REG), \
			           CONSYS_TOP2_PWR_ON_ACK_BIT);
		/* wait 5ms */
		udelay(5000);
		loop++;
	}

	/* Turn off AHB Rx bus sleep protect */
	DRV_WriteReg32(CONSYS_TOPAXI_PROT_EN_1_CLR, DRV_Reg32(CONSYS_TOPAXI_PROT_EN_1_CLR) | CONSYS_AHB_RX_PROT_MASK);

	/* check AHB Rx bus sleep protect turn off  */
	while (DRV_Reg32(CONSYS_TOPAXI_PROT_STA1_1) & CONSYS_AHB_RX_PROT_MASK);
	BTIF_INFO_LOG("disable ahb rx bus protect\n");

	/* Turn off AXI Rx bus sleep protect */
	DRV_WriteReg32(CONSYS_TOPAXI_PROT_EN_CLR, DRV_Reg32(CONSYS_TOPAXI_PROT_EN_CLR) | CONSYS_AXI_RX_PROT_MASK);

	/* check AXI Rx bus sleep protect turn off  */
	while (DRV_Reg32(CONSYS_TOPAXI_PROT_STA1) & CONSYS_AXI_RX_PROT_MASK);
	BTIF_INFO_LOG("disable axi rx bus protect\n");

	/* Turn off AXI Tx bus sleep protect */
	DRV_WriteReg32(CONSYS_TOPAXI_PROT_EN_1_CLR, DRV_Reg32(CONSYS_TOPAXI_PROT_EN_1_CLR) | CONSYS_AXI_TX_PROT_MASK);

	/* check AXI Tx bus sleep protect turn off */
	while (DRV_Reg32(CONSYS_TOPAXI_PROT_STA1_1) & CONSYS_AXI_TX_PROT_MASK);
	BTIF_INFO_LOG("disable axi tx bus protect\n");

	/* Turn off AHB TX bus sleep protect */
	DRV_WriteReg32(CONSYS_TOPAXI_PROT_EN_CLR, DRV_Reg32(CONSYS_TOPAXI_PROT_EN_CLR) | CONSYS_AHB_TX_PROT_MASK);

	/* check AHB Tx bus sleep protect turn off */
	while (DRV_Reg32(CONSYS_TOPAXI_PROT_STA1) & CONSYS_AHB_TX_PROT_MASK);
	BTIF_INFO_LOG("disable ahb tx bus protect\n");

	/* wait 5ms */
	udelay(5000);

	/*12.poll CONSYS CHIP until MT8110 is returned, <CONSYS_VER_ID_REG> */
	while (u4VerId != 0x10060000 && loop < 10)
	{
	    u4VerId = DRV_Reg32(CONSYS_VER_ID_REG);
		BTIF_INFO_LOG("consys Version Id(0x%x) not ready\n", u4VerId);

		/* wait 5ms */
		udelay(5000);
		loop++;
	}
	BTIF_INFO_LOG("consys Version Id(0x%x)\n", u4VerId);
	BTIF_INFO_LOG("consys configuration Id(0x%x)\n", DRV_Reg32(CONSYS_CFG_ID_REG));
	BTIF_INFO_LOG("consys HW Id(0x%x)\n", DRV_Reg32(CONSYS_HW_ID_REG));
	BTIF_INFO_LOG("consys FW Id(0x%x)\n", DRV_Reg32(CONSYS_FW_ID_REG));
	/*13.{default no need}update ROMDEL/PATCH RAM DELSEL if needed, <CONSYS_ROM_RAM_DELSEL_REG>*/

	//TODO
	//mtk_wcn_func_on(WMTDRV_TYPE_ROM_PTH_DW);
	wmt_rom_patch_down();

	/*default value update 2:  */
	/* AFE WBG CR  note that this CR must be backuped and restored by command batch engine */
    DRV_WriteReg32(0x180b3010, 0x4);
    BTIF_INFO_LOG("(RegAddr, RegVal):(0x%08x, 0x%08x)\n", 0x180b3010, DRV_Reg32(0x180b3010));

	DRV_WriteReg32(0x180b3024, 0x10);
	BTIF_INFO_LOG("(RegAddr, RegVal):(0x%08x, 0x%08x)\n", 0x180b3024, DRV_Reg32(0x180b3024));

	DRV_WriteReg32(0x180b3048, 0xefc82200);
    BTIF_INFO_LOG("(RegAddr, RegVal):(0x%08x, 0x%08x)\n", 0x180b3048, DRV_Reg32(0x180b3048));

	DRV_WriteReg32(0x180b3068, 0xbbf21108);
    BTIF_INFO_LOG("(RegAddr, RegVal):(0x%08x, 0x%08x)\n", 0x180b3068, DRV_Reg32(0x180b3068));

	/* write reserverd cr for identify adie is 6635 or 6631 */
    addr = 0x180c1130;
    tmp = DRV_Reg32(addr);
    tmp = tmp & 0xfffffeff;  //Mask : 1111-1111  1111-1111  1111-1110  1111-1111
    tmp = tmp | 0x100;       //Value: 0000-0000  0000-0000  0000-0001  0000-0000
    DRV_WriteReg32(addr, tmp);
    BTIF_INFO_LOG("(RegAddr, RegVal):(0x%08x, 0x%08x)\n", addr, DRV_Reg32(addr));

	/*connsys bus time out configure */
	/* enable AHB bus timeout*/
	DRV_WriteReg32(0x18002440, 0x80000101);
	BTIF_INFO_LOG("(RegAddr, RegVal):(0x%08x, 0x%08x)\n", 0x18002440, DRV_Reg32(0x18002440));

	/*16.deassert CONNSYS CPU SW reset	0x10211018	"[12]=1'b0[31:24]=8'h88 (key)"*/
	CONSYS_CLR_BIT_WITH_KEY(CONSYS_CPU_SW_RST_REG, CONSYS_CPU_SW_RST_BIT , CONSYS_CPU_SW_RST_CTRL_KEY);

	addr = 0x18002600;
	tmp = DRV_Reg32(addr);
	while (tmp != 0x1D1E && loop < 10)
	{
		BTIF_INFO_LOG("consys idle loop (0x%x) not ready\n", tmp);
		tmp = DRV_Reg32(addr);

		/* wait 5ms */
		udelay(5000);
		loop++;
	}
	tmp = DRV_Reg32(addr);
	BTIF_INFO_LOG("(RegAddr, RegVal):(0x%08x, 0x%08x)\n", addr, tmp);

	if (tmp == 0x1D1E) {
		BTIF_INFO_LOG("Consys power on Success, enter cos_idle_loop\n");
		iRet = 0;
	} else {
		BTIF_INFO_LOG("Consys power on Failed, do not enter cos_idle_loop\n");
		iRet = 1;
	}

	return iRet;
}
#endif
/*
***********************************************************
**
** power off consys mcu
**
*/
u32 wmt_pwr_off_consys_mcu(void)
{
	u32 iRet = 0;
	u32 count = 0;


	/*Turn on AHB Tx bus sleep protect (AP2CONN AHB Bus protect*/
	DRV_WriteReg32(CONSYS_TOPAXI_PROT_EN_SET, DRV_Reg32(CONSYS_TOPAXI_PROT_EN_SET) | CONSYS_AHB_TX_PROT_MASK);
	/*check AHB bus sleep protect turn on (polling "10 times")*/
	while ((DRV_Reg32(CONSYS_TOPAXI_PROT_STA1) & CONSYS_AHB_TX_PROT_MASK) == CONSYS_AHB_TX_PROT_MASK) {
		count++;
		if(count>100)
			break;
	}

	/*Turn on AXI Tx bus sleep protect */
	DRV_WriteReg32(CONSYS_TOPAXI_PROT_EN_1_SET, DRV_Reg32(CONSYS_TOPAXI_PROT_EN_1_SET) | CONSYS_AXI_TX_PROT_MASK);
	/*check AXI Tx bus sleep protect turn on (polling "100 times", polling interval is 1ms)*/
	while ((DRV_Reg32(CONSYS_TOPAXI_PROT_STA1) & CONSYS_AXI_TX_PROT_MASK) == CONSYS_AXI_TX_PROT_MASK) {
		count++;
		if(count>100)
			break;
	}

	/*Turn on AXI Rx bus sleep protect*/
	DRV_WriteReg32(CONSYS_TOPAXI_PROT_EN_SET, DRV_Reg32(CONSYS_TOPAXI_PROT_EN_SET) | CONSYS_AXI_RX_PROT_MASK);
	/*check AXI Rx bus sleep protect turn on (polling "100 times", polling interval is 1ms)*/
	while ((DRV_Reg32(CONSYS_TOPAXI_PROT_STA1) & CONSYS_AXI_RX_PROT_MASK) == CONSYS_AXI_RX_PROT_MASK) {
		count++;
		if(count>100)
			break;
	}

	/*Turn on AHB Rx bus sleep protect*/
	DRV_WriteReg32(CONSYS_TOPAXI_PROT_EN_1_SET, DRV_Reg32(CONSYS_TOPAXI_PROT_EN_1_SET) | CONSYS_AHB_RX_PROT_MASK);
	/*check AXI Rx bus sleep protect turn on (polling "100 times", polling interval is 1ms)*/
	while ((DRV_Reg32(CONSYS_TOPAXI_PROT_STA1) & CONSYS_AHB_RX_PROT_MASK) == CONSYS_AHB_RX_PROT_MASK) {
		count++;
		if(count>100)
			break;
	}

	/*release consys ISO, con_top1_iso_en = 1*/
	CONSYS_SET_BIT(CONSYS_TOP1_PWR_CTRL_REG, CONSYS_SPM_PWR_ISO_S_BIT);

	/*assert CONNSYS S/W reset(SPM CR), set "ap_sw_rst_b"=0*/
	CONSYS_CLR_BIT(CONSYS_TOP1_PWR_CTRL_REG, CONSYS_SPM_PWR_RST_BIT);

	/*write conn_clk_dis=1, disable connsys clock, <CONSYS_TOP1_PWR_CTRL_REG>, [4]1'b0-->CONSYS_CLK_CTRL_BIT*/
	CONSYS_SET_BIT(CONSYS_TOP1_PWR_CTRL_REG, CONSYS_CLK_CTRL_BIT);

	/*wait 1 us*/
	udelay(1);

	/*de-assert "conn_top_on" primary part power on, set "connsys_on_domain_pwr_on"=0*/
	/*de-assert "conn_top_on" secondary part power on, set "connsys_on_domain_pwr_on_s"=0*/
	CONSYS_CLR_BIT(CONSYS_TOP1_PWR_CTRL_REG, CONSYS_SPM_PWR_ON_BIT | CONSYS_SPM_PWR_ON_S_BIT);

	wmt_set_spi_mode_pinmux(0);

	consys_hw_vcn18_ctrl(0);
	BTIF_INFO_LOG("Consys power off Success\n");

	iRet = 0;
	return iRet;
}

static int do_consys(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
#ifdef DIAG
	if (!strcmp(argv[1], "on"))	{
		if (power_on_status == 0) {
			printf("consys already power on, skip\n");
		} else {
			printf("consys power on\n");
			power_on_status = wmt_pwr_on_consys_mcu();
		}
	} else
#endif
	if (!strcmp(argv[1], "off"))	{
		printf("consys power off\n");
		wmt_pwr_off_consys_mcu();
		power_on_status = 0xFF;
	} else {
		printf("Unkown cmd %s\n", argv[1]);
	}

	return 0;
}

U_BOOT_CMD(
	consys,	2, 1, do_consys,
	"consys power on/off test and check consys status",
	"support cmds:\n"
#ifdef DIAG
	"consys on   - power on consys and check consys enter idle_loop or not\n"
#endif
	"consys off  - power off consys\n"
);
