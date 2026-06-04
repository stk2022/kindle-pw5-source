/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2018 ROHM Semiconductors */


/*
 * RTC definitions shared between
 *
 * BD70528
 * and BD71828
 */

#define ROHM_BD1_MASK_RTC_SEC		0x7f
#define ROHM_BD1_MASK_RTC_MINUTE	0x7f
#define ROHM_BD1_MASK_RTC_HOUR_24H	0x80
#define ROHM_BD1_MASK_RTC_HOUR_PM	0x20
#define ROHM_BD1_MASK_RTC_HOUR		0x3f
#define ROHM_BD1_MASK_RTC_DAY		0x3f
#define ROHM_BD1_MASK_RTC_WEEK		0x07
#define ROHM_BD1_MASK_RTC_MONTH		0x1f
#define ROHM_BD1_MASK_RTC_YEAR		0xff
#define ROHM_BD1_MASK_ALM_EN		0x7f
