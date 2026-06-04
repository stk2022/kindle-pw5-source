/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef __SCP_REG_H
#define __SCP_REG_H

#define	SCP_RESET_CTL			(scpreg.cfg)
    #define CPU_RST_SW			(0x1 << 0)

#define	SCP_CLKGATE_CTL			(scpreg.cfg + 0x0008)
    #define CPU_CLK_EN			(0x1 << 0)

/*#define SCP_BASE			(scpreg.cfg)*/
#define SCP_AP_RESOURCE			(scpreg.cfg + 0x0004)
#define SCP_BUS_RESOURCE		(scpreg.cfg + 0x0008)

#define SCP_A_TO_HOST_STA		(scpreg.cfg + 0x0010)
	#define TRI_SCP2HOST		(0x35)
	#define TRI_HOST2SCP		(0x33)
#define SCP_A_TO_HOST_REG		(scpreg.cfg + 0x0014)
	#define SCP_IRQ_SCP2HOST	(1 << 1)
	#define HOST_IRQ_IRQ2SCP	(1 << 2)

#define SCP_SEMAPHORE			(scpreg.cfg  + 0x38)

#define SCP_A_DEBUG_PC_REG		(scpreg.cfg + 0x00B8)
#define SCP_A_DEBUG_PSP_REG		(scpreg.cfg + 0x00B4)
#define SCP_A_DEBUG_LR_REG		(scpreg.cfg + 0x00B0)
#define SCP_A_DEBUG_SP_REG		(scpreg.cfg + 0x00AC)
#define SCP_A_WDT_REG			(scpreg.cfg + 0x0084)

#define SCP_A_GENERAL_REG0		(scpreg.cfg + 0x0050)
#define SCP_A_GENERAL_REG1		(scpreg.cfg + 0x0054)
#define SCP_A_GENERAL_REG2		(scpreg.cfg + 0x0058)
/*EXPECTED_FREQ_REG*/
#define SCP_A_GENERAL_REG3		(scpreg.cfg + 0x005C)
#define EXPECTED_FREQ_REG		(scpreg.cfg  + 0x5C)
/*CURRENT_FREQ_REG*/
#define SCP_A_GENERAL_REG4		(scpreg.cfg + 0x0060)
#define CURRENT_FREQ_REG		(scpreg.cfg  + 0x60)
/*SCP_GPR_CM4_A_REBOOT*/
#define SCP_A_GENERAL_REG5		(scpreg.cfg + 0x0064)
#define SCP_GPR_CM4_A_REBOOT		(scpreg.cfg + 0x64)
	#define CM4_A_READY_TO_REBOOT	0x34
	#define CM4_A_REBOOT_OK		0x1
#define SCP_A_GENERAL_REG6		(scpreg.cfg + 0x0068)
#define SCP_A_GENERAL_REG7		(scpreg.cfg + 0x006C)

#define SCP_SCP2SPM_VOL_LV		(scpreg.cfg + 0x0094)


#endif
