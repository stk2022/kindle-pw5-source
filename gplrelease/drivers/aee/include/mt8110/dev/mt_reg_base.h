/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#pragma once

#include "dev/mt8512.h"

#define PERIPHERAL_BASE_PHYS (0x10000000)
#define PERIPHERAL_BASE_SIZE (0x0d000000UL)
#define PERIPHERAL_BASE_VIRT PERIPHERAL_BASE_PHYS

/* I/O mapping */
#define IO_PHYS             PERIPHERAL_BASE_VIRT
#define IO_SIZE             PERIPHERAL_BASE_SIZE

/* IO register definitions */
#define EMI_BASE            (IO_PHYS + 0x00219000)
#define GPIO_BASE           (IO_PHYS + 0x00005000)
#define MCUSYS_CFGREG_BASE  (IO_PHYS + 0x00200000)
#define TRNG_BASE           (IO_PHYS + 0x0020F000)

// gic
#define GIC_DIST_BASE       (MEMORY_BASE_VIRT + 0x0C0F0000)
#define GIC_REDIS_BASE      (MEMORY_BASE_VIRT + 0x0C170000)

// APB Module mcucfg
#define MCUCFG_BASE         (IO_PHYS + 0x00200000)

//SYSTRACKER
#define BUS_DBG_BASE		(IO_PHYS + 0x00208000)

#define UART1_BASE          (IO_PHYS + 0x01002000)
#define UART2_BASE          (IO_PHYS + 0x01003000)
#define UART3_BASE          (IO_PHYS + 0x01004000)
#define UART4_BASE          (IO_PHYS + 0x01004000)

#define MSDC0_BASE          (IO_PHYS + 0x01230000)
#define MSDC1_BASE          (IO_PHYS + 0x01240000)
#define MSDC0_TOP_BASE	    (IO_PHYS + 0x01cd0000)
#define MSDC1_TOP_BASE	    (IO_PHYS + 0x01c90000)

/* APB Module ssusb_top */
#define USB3_BASE           (IO_PHYS + 0x1210000)
#define USB3_SIF_BASE       (IO_PHYS + 0x1CC0000)
#define USB3_IPPC_BASE      (IO_PHYS + 0x1213E00)

#define USB_BASE            (USB0_BASE)
#define RGU_BASE            (IO_PHYS + 0x7000)

#define NFI_BASE            (IO_PHYS + 0x1018000)
#define NFIECC_BASE         (IO_PHYS + 0x1019000)

#define SEJ_BASE            (IO_PHYS + 0x000A000)
#define SCP_BASE_SRAM       (IO_PHYS + 0x1800000)
#define SCP_BASE_CFG        (IO_PHYS + 0x1820000)
#define SLEEP_BASE        	(IO_PHYS + 0x0006000)

#define PWM_BASE            (IO_PHYS + 0x0001D000)
