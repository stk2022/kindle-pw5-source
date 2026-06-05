/* Copyright Statement:
*
* This software/firmware and related documentation ("MediaTek Software") are
* protected under relevant copyright laws. The information contained herein
* is confidential and proprietary to MediaTek Inc. and/or its licensors.
* Without the prior written permission of MediaTek inc. and/or its licensors,
* any reproduction, modification, use or disclosure of MediaTek Software,
* and information contained herein, in whole or in part, shall be strictly prohibited.
*/
/* MediaTek Inc. (C) 2016. All rights reserved.
*
* BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
* THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
* RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
* AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
* NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
* SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
* SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
* THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
* THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
* CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
* SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
* STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
* CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
* AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
* OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
* MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
*/
//#include <reg.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <linux/io.h>
#include "include/mt8110/dev/mt_reg_base.h"
#include "include/aee_platform_debug.h"
#include "include/mt8110/dev/systracker.h"

#define TRACKER_VALID_S		26
#define TRACKER_VALID_E		26
#define TRACKER_SECURE_S	21
#define TRACKER_SECURE_E	21
#define TRACKER_ID_S		8
#define TRACKER_ID_E		20
#define TRACKER_DATA_SIZE_S	4
#define TRACKER_DATA_SIZE_E	6
#define TRACKER_BURST_LEN_S	0
#define TRACKER_BURST_LEN_E	3

static inline unsigned int extract_n2mbits(unsigned int input, unsigned int n, unsigned int m)
{
	/*
	 * 1. ~0 = 1111 1111 1111 1111 1111 1111 1111 1111
	 * 2. ~0 << (m - n + 1) = 1111 1111 1111 1111 1100 0000 0000 0000
	 * // assuming we are extracting 14 bits, the +1 is added for inclusive selection
	 * 3. ~(~0 << (m - n + 1)) = 0000 0000 0000 0000 0011 1111 1111 1111
	 */
	int mask;

	if (n > m) {
		n = n + m;
		m = n - m;
		n = n - m;
	}
	mask = ~(~0U << (m - n + 1));
	return (input >> n) & mask;
}

static int systracker_dump(char *buf, int *wp, unsigned int entry_num)
{
	unsigned int i;
	unsigned int reg_value;
	unsigned int entry_valid;
	unsigned int entry_secure;
	unsigned int entry_id;
	unsigned int entry_address;
	unsigned int entry_data_size;
	unsigned int entry_burst_length;

	if (buf == NULL || wp == NULL)
		return -1;

	/* Get tracker info and save to buf */

	/* check if we got AP tracker timeout */
	if (readl(BUS_DBG_CON) & BUS_DBG_CON_TIMEOUT) {
		*wp += snprintf(buf + *wp, SYSTRACKER_BUF_LENGTH - *wp, "\n*************************** systracker ***************************\n");

		for (i = 0; i < entry_num; i++) {
			entry_address = readl(BUS_DBG_AR_TRACK_L(i));
			reg_value = readl(BUS_DBG_AR_TRACK_H(i));
			entry_valid = extract_n2mbits(reg_value, TRACKER_VALID_S, TRACKER_VALID_E);
			entry_secure = extract_n2mbits(reg_value, TRACKER_SECURE_S, TRACKER_SECURE_E);
			entry_id = extract_n2mbits(reg_value, TRACKER_ID_S, TRACKER_ID_E);
			entry_data_size = extract_n2mbits(reg_value, TRACKER_DATA_SIZE_S, TRACKER_DATA_SIZE_E);
			entry_burst_length = extract_n2mbits(reg_value, TRACKER_BURST_LEN_S, TRACKER_BURST_LEN_E);

			*wp += snprintf(buf + *wp, SYSTRACKER_BUF_LENGTH - *wp,
						   "read entry = %d, valid = 0x%x, non-secure = 0x%x, read id = 0x%x, address = 0x%x, data_size = 0x%x, burst_length = 0x%x\n",
						   i, entry_valid, entry_secure, entry_id,
						   entry_address, entry_data_size, entry_burst_length);
		}

		for (i = 0; i < entry_num; i++) {
			entry_address = readl(BUS_DBG_AW_TRACK_L(i));
			reg_value = readl(BUS_DBG_AW_TRACK_H(i));
			entry_valid = extract_n2mbits(reg_value, TRACKER_VALID_S, TRACKER_VALID_E);
			entry_secure = extract_n2mbits(reg_value, TRACKER_SECURE_S, TRACKER_SECURE_E);
			entry_id = extract_n2mbits(reg_value, TRACKER_ID_S, TRACKER_ID_E);
			entry_data_size = extract_n2mbits(reg_value, TRACKER_DATA_SIZE_S, TRACKER_DATA_SIZE_E);
			entry_burst_length = extract_n2mbits(reg_value, TRACKER_BURST_LEN_S, TRACKER_BURST_LEN_E);

			*wp += snprintf(buf + *wp, SYSTRACKER_BUF_LENGTH - *wp,
						   "write entry = %d, valid = 0x%x, non-secure = 0x%x, write id = 0x%x, address = 0x%x, data_size = 0x%x, burst_length = 0x%x\n",
						   i, entry_valid, entry_secure, entry_id, entry_address,
						   entry_data_size, entry_burst_length);
		}
	}
	return strlen(buf);
}

int systracker_get(void **data, int *len, unsigned int entry_num)
{
	int ret;

	*len = 0;
	*data = malloc(SYSTRACKER_BUF_LENGTH);
	if (*data == NULL)
		return 0;

	ret = systracker_dump(*data, len, entry_num);
	if (ret < 0 || *len > SYSTRACKER_BUF_LENGTH) {
		*len = (*len > SYSTRACKER_BUF_LENGTH) ? SYSTRACKER_BUF_LENGTH : *len;
		return ret;
	}

	return 1;
}

void systracker_put(void **data)
{
	free(*data);
}

