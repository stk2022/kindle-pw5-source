// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek power ctrl driver
 *
 * Copyright (C) 2020 MediaTek Inc.
 */
#include "pwr_ctrl.h"
#include "spm_mtcmos.h"
#include <power/bd71828.h>

#define DRV_WriteReg32(addr, value) writel(value,addr)
#define DRV_Reg32(addr) readl(addr)

void subsys_cg_all_off(void)
{
	unsigned int temp;

	//infra module clk gated begin
	temp = DRV_Reg32(MODULE_SW_CG_0_SET);
	DRV_WriteReg32(MODULE_SW_CG_0_SET, temp | 0x09800200);
	temp = DRV_Reg32(MODULE_SW_CG_1_SET);
	DRV_WriteReg32(MODULE_SW_CG_1_SET, temp | 0x31040314);
	temp = DRV_Reg32(MODULE_SW_CG_2_SET);
	DRV_WriteReg32(MODULE_SW_CG_2_SET, temp | 0x08000040);
	temp = DRV_Reg32(MODULE_SW_CG_3_SET);
	DRV_WriteReg32(MODULE_SW_CG_3_SET, temp | 0x00000580);
	temp = DRV_Reg32(MODULE_SW_CG_4_SET);
	DRV_WriteReg32(MODULE_SW_CG_4_SET, temp | 0x0000000f);
}

void topck_all_off(void)
{
	unsigned int temp;

	temp = DRV_Reg32(CLK_CFG_0);
	DRV_WriteReg32(CLK_CFG_0_SET, temp | 0x80808000);
	temp = DRV_Reg32(CLK_CFG_1);
	DRV_WriteReg32(CLK_CFG_1_SET, temp | 0x80808080);
	temp = DRV_Reg32(CLK_CFG_2);
	DRV_WriteReg32(CLK_CFG_2_SET, temp | 0x80808080);
	temp = DRV_Reg32(CLK_CFG_3);
	DRV_WriteReg32(CLK_CFG_3_SET, temp | 0x80808080);
	temp = DRV_Reg32(CLK_CFG_4);
	DRV_WriteReg32(CLK_CFG_4_SET, temp | 0x80808080);
	temp = DRV_Reg32(CLK_CFG_5);
	DRV_WriteReg32(CLK_CFG_5_SET, temp | 0x80808080);
	temp = DRV_Reg32(CLK_CFG_6);
	DRV_WriteReg32(CLK_CFG_6_SET, temp | 0x80800000);// skip spm_sel, i2c
	DRV_WriteReg32(CLK_CFG_6_CLR, 0x700);
	temp = DRV_Reg32(CLK_CFG_7);
	DRV_WriteReg32(CLK_CFG_7_SET, temp | 0x80808080);
	temp = DRV_Reg32(CLK_CFG_8);
	DRV_WriteReg32(CLK_CFG_8_SET, temp | 0x80808080);
	temp = DRV_Reg32(CLK_CFG_9);
	DRV_WriteReg32(CLK_CFG_9_SET, temp | 0x80808080);
	temp = DRV_Reg32(CLK_CFG_10);
	DRV_WriteReg32(CLK_CFG_10_SET, temp | 0x80808080);
	temp = DRV_Reg32(CLK_CFG_11);
	DRV_WriteReg32(CLK_CFG_11_SET, temp | 0x00000080);

	DRV_WriteReg32(CLK_CFG_UPDATE, 0xffffffff);
	DRV_WriteReg32(CLK_CFG_UPDATE1, 0x00001fff);

	temp = DRV_Reg32(CLK_IPPLL_CFG);
	DRV_WriteReg32(CLK_IPPLL_CFG, temp & 0xFFFFFFEF);
}

void pll_all_off(int active)
{
	unsigned int temp;

	DRV_WriteReg32(PLLON_CON0, 0x1111f0f0); //spm control armpll-mpll-mainpll
	DRV_WriteReg32(AP_PLL_CON3, DRV_Reg32(AP_PLL_CON3) & ~0x6);

	temp = DRV_Reg32(AP_PLLGP1_CON1);
	DRV_WriteReg32(AP_PLLGP1_CON1, temp & ~0x3);
	printf("AP_PLLGP1_CON1= 0x%x\n", DRV_Reg32(AP_PLLGP1_CON1));

	temp = DRV_Reg32(APLL1_CON0);
	DRV_WriteReg32(APLL1_CON0, temp & ~0x1);
	temp = DRV_Reg32(MSDCPLL_CON0);
	DRV_WriteReg32(MSDCPLL_CON0, temp & ~0x1);
	temp = DRV_Reg32(APLL2_CON0);
	DRV_WriteReg32(APLL2_CON0, temp & ~0x1);
	temp = DRV_Reg32(IPPLL_CON0);
	DRV_WriteReg32(IPPLL_CON0, temp & ~0x1);
	temp = DRV_Reg32(DSPPLL_CON0);
	DRV_WriteReg32(DSPPLL_CON0, temp & ~0x1);
	if (!active) {
		temp = DRV_Reg32(UNIVPLL_CON0);
		DRV_WriteReg32(UNIVPLL_CON0, temp & ~0x1);
		temp = DRV_Reg32(TCONPLL_CON0);
		DRV_WriteReg32(TCONPLL_CON0, temp & ~0x1);
	}

	temp = DRV_Reg32(APLL1_CON4);
	DRV_WriteReg32(APLL1_CON4, temp | 0x2);
	temp = DRV_Reg32(MSDCPLL_CON3);
	DRV_WriteReg32(MSDCPLL_CON3, temp | 0x2);
	temp = DRV_Reg32(APLL2_CON4);
	DRV_WriteReg32(APLL2_CON4, temp | 0x2);
	temp = DRV_Reg32(IPPLL_CON3);
	DRV_WriteReg32(IPPLL_CON3, temp | 0x2);
	temp = DRV_Reg32(DSPPLL_CON3);
	DRV_WriteReg32(DSPPLL_CON3, temp | 0x2);
	if (!active) {
		temp = DRV_Reg32(UNIVPLL_CON3);
		DRV_WriteReg32(UNIVPLL_CON3, temp | 0x2);
		temp = DRV_Reg32(TCONPLL_CON3);
		DRV_WriteReg32(TCONPLL_CON3, temp | 0x2);
	}

	temp = DRV_Reg32(APLL1_CON4);
	DRV_WriteReg32(APLL1_CON4, temp & ~0x1);
	temp = DRV_Reg32(MSDCPLL_CON3);
	DRV_WriteReg32(MSDCPLL_CON3, temp & ~0x1);
	temp = DRV_Reg32(APLL2_CON4);
	DRV_WriteReg32(APLL2_CON4, temp & ~0x1);
	temp = DRV_Reg32(IPPLL_CON3);
	DRV_WriteReg32(IPPLL_CON3, temp & ~0x1);
	temp = DRV_Reg32(DSPPLL_CON3);
	DRV_WriteReg32(DSPPLL_CON3, temp & ~0x1);
	if (!active) {
		temp = DRV_Reg32(UNIVPLL_CON3);
		DRV_WriteReg32(UNIVPLL_CON3, temp & ~0x1);
		temp = DRV_Reg32(TCONPLL_CON3);
		DRV_WriteReg32(TCONPLL_CON3, temp & ~0x1);
	}

}

void analog_off(void)
{
	unsigned int temp;

	//Audio ADC Power Down Setting
	DRV_WriteReg32(0x1000C768, 0x00081050);
	DRV_WriteReg32(0x1000C768, 0x00081050);
	DRV_WriteReg32(0x1000C768, 0x00081050);
	DRV_WriteReg32(0x10005280, 0x00000007);
	DRV_WriteReg32(0x10005280, 0x0000003F);
	DRV_WriteReg32(0x10005280, 0x000001FF);
	DRV_WriteReg32(0x10005280, 0x00000FFF);
	DRV_WriteReg32(0x1000C734, 0x00000000);
	DRV_WriteReg32(0x1000C768, 0x00081050);
	DRV_WriteReg32(0x1000C768, 0x00081050);
	DRV_WriteReg32(0x1000C768, 0x00081050);
	DRV_WriteReg32(0x1000C770, 0x00180000);
	DRV_WriteReg32(0x1000C774, 0x00061300);
	DRV_WriteReg32(0x1000C770, 0x00180000);
	DRV_WriteReg32(0x1000C774, 0x00061300);
	DRV_WriteReg32(0x1000C710, 0x08000030);
	DRV_WriteReg32(0x1000C720, 0x80000030);
	DRV_WriteReg32(0x1000C770, 0x00180000);
	DRV_WriteReg32(0x1000C774, 0x00061300);
	DRV_WriteReg32(0x1000C770, 0x00180000);
	DRV_WriteReg32(0x1000C774, 0x00061300);
	DRV_WriteReg32(0x1000C770, 0x00180000);
	DRV_WriteReg32(0x1000C720, 0x80000030);
	DRV_WriteReg32(0x1000C710, 0x08000030);
	DRV_WriteReg32(0x1000C720, 0x80000030);
	DRV_WriteReg32(0x1000C770, 0x00180000);
	DRV_WriteReg32(0x1000C778, 0x09800018);
	DRV_WriteReg32(0x1000C770, 0x00180000);
	DRV_WriteReg32(0x1000C778, 0x09800018);
	DRV_WriteReg32(0x1000C770, 0x00180000);
	DRV_WriteReg32(0x1000C770, 0x00180000);

	//Audio_DAC_Power_Down_Setting
	DRV_WriteReg32(0x1000C700, 0x00020103);
	DRV_WriteReg32(0x1000C700, 0x00020103);
	DRV_WriteReg32(0x1000C704, 0x00000140);
	DRV_WriteReg32(0x1000C704, 0x00000140);
	DRV_WriteReg32(0x1000C704, 0x00000140);
	DRV_WriteReg32(0x1000C704, 0x00000140);
	DRV_WriteReg32(0x1000C700, 0x00020103);
	DRV_WriteReg32(0x1000C760, 0x26100280);
	DRV_WriteReg32(0x1000C760, 0x26100280);
	DRV_WriteReg32(0x1000C700, 0x00020103);
	DRV_WriteReg32(0x1000C760, 0x26100280);
	DRV_WriteReg32(0x1000C700, 0x00020103);
	DRV_WriteReg32(0x1000C700, 0x00020103);
	DRV_WriteReg32(0x1000C700, 0x00020103);
	DRV_WriteReg32(0x1000C760, 0x26100280);
	DRV_WriteReg32(0x1000C760, 0x26100280);

	//USB20_P0
	temp = DRV_Reg32(0x11CC0368);
	DRV_WriteReg32(0x11CC0368, temp & ~(1<<26));
	temp = DRV_Reg32(0x11CC036C);
	DRV_WriteReg32(0x11CC036C, temp & ~(1<<16));
	temp = DRV_Reg32(0x11CC0320);
	DRV_WriteReg32(0x11CC0320, temp & ~(1<<9));
	temp = DRV_Reg32(0x11CC0320);
	DRV_WriteReg32(0x11CC0320, temp & ~(1<<8));
	temp = DRV_Reg32(0x11CC0368);
	DRV_WriteReg32(0x11CC0368, temp | (1<<3));
	temp = DRV_Reg32(0x11CC0368);
	DRV_WriteReg32(0x11CC0368, temp | (1<<18));
	temp = DRV_Reg32(0x11CC0368);
	DRV_WriteReg32(0x11CC0368, temp | (1<<6));
	temp = DRV_Reg32(0x11CC0368);
	DRV_WriteReg32(0x11CC0368, temp | (1<<7));
	temp = DRV_Reg32(0x11CC0368);
	DRV_WriteReg32(0x11CC0368, (temp & ~(0x3<<4)) | (0x1<<4));
	temp = DRV_Reg32(0x11CC0368);
	DRV_WriteReg32(0x11CC0368, temp | (1<<2));
	temp = DRV_Reg32(0x11CC0368);
	DRV_WriteReg32(0x11CC0368, temp & ~(0xf<<10));
	temp = DRV_Reg32(0x11CC0368);
	DRV_WriteReg32(0x11CC0368, temp | (1<<20));
	temp = DRV_Reg32(0x11CC0368);
	DRV_WriteReg32(0x11CC0368, temp | (1<<21));
	temp = DRV_Reg32(0x11CC0368);
	DRV_WriteReg32(0x11CC0368, temp | (1<<19));
	temp = DRV_Reg32(0x11CC0368);
	DRV_WriteReg32(0x11CC0368, temp | (1<<17));
	temp = DRV_Reg32(0x11CC0368);
	DRV_WriteReg32(0x11CC0368, temp | (1<<23));
	temp = DRV_Reg32(0x11CC0318);
	DRV_WriteReg32(0x11CC0318, temp & ~(1<<23));
	temp = DRV_Reg32(0x11CC0318);
	DRV_WriteReg32(0x11CC0318, temp & ~(1<<20));
	temp = DRV_Reg32(0x11CC0368);
	DRV_WriteReg32(0x11CC0368, temp & ~(1<<3));

	//USB20_P1
	temp = DRV_Reg32(0x11C40368);
	DRV_WriteReg32(0x11C40368, temp & ~(1<<26));
	temp = DRV_Reg32(0x11C4036C);
	DRV_WriteReg32(0x11C4036C, temp & ~(1<<16));
	temp = DRV_Reg32(0x11C40320);
	DRV_WriteReg32(0x11C40320, temp & ~(1<<9));
	temp = DRV_Reg32(0x11C40320);
	DRV_WriteReg32(0x11C40320, temp & ~(1<<8));
	temp = DRV_Reg32(0x11C40368);
	DRV_WriteReg32(0x11C40368, temp | (1<<3));
	temp = DRV_Reg32(0x11C40368);
	DRV_WriteReg32(0x11C40368, temp | (1<<18));
	temp = DRV_Reg32(0x11C40368);
	DRV_WriteReg32(0x11C40368, temp | (1<<6));
	temp = DRV_Reg32(0x11C40368);
	DRV_WriteReg32(0x11C40368, temp | (1<<7));
	temp = DRV_Reg32(0x11C40368);
	DRV_WriteReg32(0x11C40368, (temp & ~(0x3<<4)) | (0x1<<4));
	temp = DRV_Reg32(0x11C40368);
	DRV_WriteReg32(0x11C40368, temp | (1<<2));
	temp = DRV_Reg32(0x11C40368);
	DRV_WriteReg32(0x11C40368, temp & ~(0xf<<10));
	temp = DRV_Reg32(0x11C40368);
	DRV_WriteReg32(0x11C40368, temp | (1<<20));
	temp = DRV_Reg32(0x11C40368);
	DRV_WriteReg32(0x11C40368, temp | (1<<21));
	temp = DRV_Reg32(0x11C40368);
	DRV_WriteReg32(0x11C40368, temp | (1<<19));
	temp = DRV_Reg32(0x11C40368);
	DRV_WriteReg32(0x11C40368, temp | (1<<17));
	temp = DRV_Reg32(0x11C40368);
	DRV_WriteReg32(0x11C40368, temp | (1<<23));
	temp = DRV_Reg32(0x11C40318);
	DRV_WriteReg32(0x11C40318, temp & ~(1<<23));
	temp = DRV_Reg32(0x11C40318);
	DRV_WriteReg32(0x11C40318, temp & ~(1<<20));
	temp = DRV_Reg32(0x11C40368);
	DRV_WriteReg32(0x11C40368, temp & ~(1<<3));
}

void dcm_all_on(void)
{
	unsigned int temp;
	//infra bus dcm
	temp = DRV_Reg32(INFRA_BUS_DCM_CTRL);
	DRV_WriteReg32(INFRA_BUS_DCM_CTRL, temp | 0x40F00603);
	temp = DRV_Reg32(INFRA_BUS_DCM_CTRL);
	DRV_WriteReg32(INFRA_BUS_DCM_CTRL, temp & 0xFFFF8603);
	//peri bus dcm
	temp = DRV_Reg32(PERI_BUS_DCM_CTRL);
	DRV_WriteReg32(PERI_BUS_DCM_CTRL, temp | 0xB07F83E3);
	temp = DRV_Reg32(PERI_BUS_DCM_CTRL);
	DRV_WriteReg32(PERI_BUS_DCM_CTRL, temp & 0xF07F83E7);
	//l2c dcm
	temp = DRV_Reg32(INFRA_GLOBALCON_DCMCTL);
	DRV_WriteReg32(INFRA_GLOBALCON_DCMCTL, temp | 0x200);
	//axi dcm
	temp = DRV_Reg32(INFRA_GLOBALCON_DCMCTL);
	DRV_WriteReg32(INFRA_GLOBALCON_DCMCTL, temp | 0x100);
	//mem dcm
	temp = DRV_Reg32(MEM_DCM_CTRL);
	DRV_WriteReg32(MEM_DCM_CTRL, temp | 0x0BE00180);
	temp = DRV_Reg32(MEM_DCM_CTRL);
	DRV_WriteReg32(MEM_DCM_CTRL, temp & 0x4BE0FFBE);
	//dfs_mem dcm
	temp = DRV_Reg32(DFS_MEM_DCM_CTRL);
	DRV_WriteReg32(DFS_MEM_DCM_CTRL, temp | 0x03E00180);
	temp = DRV_Reg32(DFS_MEM_DCM_CTRL);
	DRV_WriteReg32(DFS_MEM_DCM_CTRL, temp & 0xFBE0FFBE);
	//p2p
	temp = DRV_Reg32(P2P_RX_CLK_ON);
	DRV_WriteReg32(P2P_RX_CLK_ON, temp & 0xFFFFFFF0);
	//sej
	temp = DRV_Reg32(SEJ_CON);
	DRV_WriteReg32(SEJ_CON, temp | 0x80000000);
	//i2c0
	temp = DRV_Reg32(I2C0_I2CREG_HW_CG_EN);
	DRV_WriteReg32(I2C0_I2CREG_HW_CG_EN, temp | 0x00000001);
	//mp0 rgu
	temp = DRV_Reg32(0x10200088);
	DRV_WriteReg32(0x10200088, temp | 0x00000001);
	//l2c sram
	temp = DRV_Reg32(L2C_SRAM_CTRL);
	DRV_WriteReg32(L2C_SRAM_CTRL, temp | 0x00000001);
	//cci sram
	temp = DRV_Reg32(0x10200660);
	DRV_WriteReg32(0x10200660, temp | 0x00000001);
	//mcusys bus
	temp = DRV_Reg32(0x10200668);
	DRV_WriteReg32(0x10200668, temp | 0x00241F3F);
	//mcu misc
	temp = DRV_Reg32(MCU_MISC_DCM_CTRL);
	DRV_WriteReg32(MCU_MISC_DCM_CTRL, temp | 0x00000001);
	//mp cci
	temp = DRV_Reg32(0x10200740);
	DRV_WriteReg32(0x10200740, temp | 0x00000045);
	//gic
	temp = DRV_Reg32(0x10200758);
	DRV_WriteReg32(0x10200758, temp | 0x00000001);
	//bus pll divider
	temp = DRV_Reg32(BUS_PLL_DIVIDER_CFG);
	DRV_WriteReg32(BUS_PLL_DIVIDER_CFG, temp | 0x00000800);
	//cq dma
	temp = DRV_Reg32(CQ_DMA_G_DMA_0_DCM_EN);
	DRV_WriteReg32(CQ_DMA_G_DMA_0_DCM_EN, temp | 0x00000001);
	temp = DRV_Reg32(CQ_DMA_G_DMA_1_DCM_EN);
	DRV_WriteReg32(CQ_DMA_G_DMA_1_DCM_EN, temp | 0x00000001);
	temp = DRV_Reg32(CQ_DMA_G_DMA_2_DCM_EN);
	DRV_WriteReg32(CQ_DMA_G_DMA_2_DCM_EN, temp | 0x00000001);
	//emi
	temp = DRV_Reg32(0x10219060);
	DRV_WriteReg32(0x10219060, temp & 0x00FFFFFF);
	temp = DRV_Reg32(0x10219068);
	DRV_WriteReg32(0x10219068, temp & 0x00FFFFFF);
	temp = DRV_Reg32(0x1022F008);
	DRV_WriteReg32(0x1022F008, temp & 0x00FFFFFF);
	//misc
	temp = DRV_Reg32(0x10228284);
	DRV_WriteReg32(0x10228284, temp & 0xFFF400FF);
	temp = DRV_Reg32(0x1022828C);
	DRV_WriteReg32(0x1022828C, temp | 0x01000000);
	temp = DRV_Reg32(0x1022828C);
	DRV_WriteReg32(0x1022828C, temp & 0xF91FFF3F);
	temp = DRV_Reg32(0x102282A8);
	DRV_WriteReg32(0x102282A8, temp & 0xF3FFFFFF);
	temp = DRV_Reg32(0x1022831C);
	DRV_WriteReg32(0x1022831C, temp & 0xF3FFFFFF);
	//dramc
	temp = DRV_Reg32(0x1022C038);
	DRV_WriteReg32(0x1022C038, temp | 0xC0000007);
	temp = DRV_Reg32(0x1022C038);
	DRV_WriteReg32(0x1022C038, temp & 0xFBFFFFFF);
	temp = DRV_Reg32(0x1022C03C);
	DRV_WriteReg32(0x1022C03C, temp | 0x80000000);
	temp = DRV_Reg32(0x1022F008);
	DRV_WriteReg32(0x1022F008, temp & 0x00FFFFFF);
	temp = DRV_Reg32(0x1022C038);
	DRV_WriteReg32(0x1022C038, temp | 0x00002000);
	//tx cg
	temp = DRV_Reg32(0x1023223C);
	DRV_WriteReg32(0x1023223C, temp | 0x00000002);
	//gce
	temp = DRV_Reg32(GCE_CTL_INT0);
	DRV_WriteReg32(GCE_CTL_INT0, temp | 0x0000FFFF);
	//ap dma
	temp = DRV_Reg32(0x11000020);
	DRV_WriteReg32(0x11000020, temp | 0x00000FFF);
	//i2c0
	temp = DRV_Reg32(I2C0_I2CREG_HW_CG_EN);
	DRV_WriteReg32(I2C0_I2CREG_HW_CG_EN, temp | 0x00000001);
	//
	temp = DRV_Reg32(0x11010728);
	DRV_WriteReg32(0x11010728, temp & 0xFFFFFFFC);
	//hdma
	temp = DRV_Reg32(0x11210950);
	DRV_WriteReg32(0x11210950, temp | 0x01000000);
	//usb
	temp = DRV_Reg32(0x11213E00);
	DRV_WriteReg32(0x11213E00, temp | 0x00010000);
	temp = DRV_Reg32(0x11213E04);
	DRV_WriteReg32(0x11213E04, temp | 0x00000001);
	temp = DRV_Reg32(0x11213E08);
	DRV_WriteReg32(0x11213E08, temp | 0x00000001);
	temp = DRV_Reg32(0x11213E0C);
	DRV_WriteReg32(0x11213E0C, temp | 0x00000001);
	temp = DRV_Reg32(0x11213E30);
	DRV_WriteReg32(0x11213E30, temp | 0x0000000A);
	temp = DRV_Reg32(0x11213E38);
	DRV_WriteReg32(0x11213E38, temp | 0x0000000A);
	temp = DRV_Reg32(0x11213E50);
	DRV_WriteReg32(0x11213E50, temp | 0x0000000A);
	temp = DRV_Reg32(0x11213E58);
	DRV_WriteReg32(0x11213E58, temp | 0x0000000A);
	temp = DRV_Reg32(0x11213E60);
	DRV_WriteReg32(0x11213E60, temp | 0x0000000A);
	temp = DRV_Reg32(0x11213E68);
	DRV_WriteReg32(0x11213E68, temp | 0x0000000A);
	temp = DRV_Reg32(0x11213E70);
	DRV_WriteReg32(0x11213E70, temp | 0x0000000A);
	temp = DRV_Reg32(0x11213E88);
	DRV_WriteReg32(0x11213E88, temp | 0x00000003);
	temp = DRV_Reg32(0x11213E8C);
	DRV_WriteReg32(0x11213E8C, temp | 0x0000001F);
}

void dcm_all_off(void)
{
	unsigned int temp;

	//infra bus dcm
	temp = DRV_Reg32(INFRA_BUS_DCM_CTRL);
	DRV_WriteReg32(INFRA_BUS_DCM_CTRL, temp | 0x00600600);
	temp = DRV_Reg32(INFRA_BUS_DCM_CTRL);
	DRV_WriteReg32(INFRA_BUS_DCM_CTRL, temp & 0xBF6F8600);

	//peri bus dcm
	temp = DRV_Reg32(PERI_BUS_DCM_CTRL);
	DRV_WriteReg32(PERI_BUS_DCM_CTRL, temp | 0x000003E0);
	temp = DRV_Reg32(PERI_BUS_DCM_CTRL);
	DRV_WriteReg32(PERI_BUS_DCM_CTRL, temp & 0x400003E4);

	//l2c dcm
	temp = DRV_Reg32(INFRA_GLOBALCON_DCMCTL);
	DRV_WriteReg32(INFRA_GLOBALCON_DCMCTL, temp & 0xFFFFFDFF);

	//axi dcm
	temp = DRV_Reg32(INFRA_GLOBALCON_DCMCTL);
	DRV_WriteReg32(INFRA_GLOBALCON_DCMCTL, temp & 0xFFFFFEFF);
}

void mtcmos_all_off(int active)
{
	unsigned int temp;

	//power down DSP sram
	temp = DRV_Reg32(0x10006368);
	DRV_WriteReg32(0x10006368, temp | (1<<15) | (1<<14) | (1<<13) | (1<<12));

	if (!active) {
		spm_mtcmos_ctrl_mm(STA_POWER_DOWN);
		spm_mtcmos_ctrl_img(STA_POWER_DOWN);
	}
	spm_mtcmos_ctrl_ip0(STA_POWER_DOWN);
	spm_mtcmos_ctrl_ip1(STA_POWER_DOWN);
	spm_mtcmos_ctrl_ip2(STA_POWER_DOWN);
	spm_mtcmos_ctrl_usb_mac_p1(STA_POWER_DOWN);
	spm_mtcmos_ctrl_dsp(STA_POWER_DOWN);
	spm_mtcmos_ctrl_audio(STA_POWER_DOWN);
	spm_mtcmos_ctrl_asrc(STA_POWER_DOWN);
	spm_mtcmos_ctrl_conn(STA_POWER_DOWN);
}

void pmic_ldo_power_off(void)
{
#if defined(CONFIG_PMIC_BD71828)
	bd71828_enable_ldo(LDO2, 0);
	bd71828_enable_ldo(LDO3, 0);
	bd71828_enable_ldo(LDO4, 0);
	bd71828_enable_ldo(LDO6, 0);
	bd71828_enable_ldo(LDO_SNVS, 0);
#endif
}
