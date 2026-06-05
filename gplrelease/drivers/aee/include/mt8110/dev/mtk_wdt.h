/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2008-2012 Travis Geiselbrecht
 */

/* Reboot reason */
#define RE_BOOT_REASON_UNKNOWN          (0x00)
#define RE_BOOT_BY_WDT_HW               (0x01)
#define RE_BOOT_BY_WDT_SW               (0x02)
#define RE_BOOT_WITH_INTTERUPT          (0x04)
#define RE_BOOT_BY_SPM_THERMAL          (0x08)
#define RE_BOOT_BY_SPM                  (0x10)
#define RE_BOOT_BY_THERMAL_DIRECT       (0x20)
#define RE_BOOT_BY_DEBUG                (0x40)
#define RE_BOOT_BY_SECURITY             (0x80)
#define RE_BOOT_BY_PMIC_FULL_RST        (0x800)

/*WDT_STATUS*/
#define MTK_WDT_STATUS_HWWDT_RST_WITH_IRQ    (0xA0000000)
#define MTK_WDT_STATUS_HWWDT_RST             (0x80000000)
#define MTK_WDT_STATUS_SWWDT_RST             (0x40000000)
#define MTK_WDT_STATUS_IRQWDT_RST            (0x20000000)
#define MTK_WDT_STATUS_DEBUGWDT_RST          (0x00080000)
#define MTK_WDT_STATUS_SPMWDT_RST            (0x0002)
#define MTK_WDT_STATUS_SPM_THERMAL_RST       (0x0001)
#define MTK_WDT_STATUS_SECURITY_RST          (0x10000000)
#define MTK_WDT_STATUS_THERMAL_CTL_RST       (0x40000)
