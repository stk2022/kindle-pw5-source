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

#ifndef __SCP_IPI_H
#define __SCP_IPI_H

#include "scp_reg.h"

#define SHARE_BUF_SIZE 288
/* scp awake timout count definition*/
#define SCP_AWAKE_TIMEOUT 100000
#define SCP_IPI_STAMP_SUPPORT 0

/* scp Core ID definition*/
enum scp_core_id {
	SCP_A_ID = 0,
	SCP_CORE_TOTAL = 1,
};

/* scp ipi ID definition
 * need to sync with SCP-side
 */
#define IPI_WDT			0
#define IPI_TEST1		1
#define IPI_LOGGER_ENABLE	2
#define IPI_LOGGER_WAKEUP	3
#define IPI_LOGGER_INIT_A	4
#define IPI_APCCCI		5
#define IPI_SCP_A_RAM_DUMP	6
#define IPI_SCP_A_READY		7
#define IPI_SCP_PLL_CTRL	8
#define IPI_MET_SCP		9
#define SCP_NR_IPI		10


enum scp_ipi_status {
	SCP_IPI_ERROR = -1,
	SCP_IPI_DONE,
	SCP_IPI_BUSY,
};



struct scp_ipi_desc {
	void (*handler)(int id, void *data, unsigned int len);
#if SCP_IPI_STAMP_SUPPORT
#define SCP_IPI_ID_STAMP_SIZE 5
	unsigned long long recv_timestamp[SCP_IPI_ID_STAMP_SIZE];
	unsigned long long handler_timestamp[SCP_IPI_ID_STAMP_SIZE];
	unsigned long long send_timestamp[SCP_IPI_ID_STAMP_SIZE];
	unsigned int recv_flag[SCP_IPI_ID_STAMP_SIZE];
	unsigned int send_flag[SCP_IPI_ID_STAMP_SIZE];
#endif
	unsigned int recv_count;
	unsigned int success_count;
	unsigned int busy_count;
	unsigned int error_count;
	const char *name;
};


struct share_obj {
	unsigned char id;
	unsigned int len;
	unsigned char reserve[8];
	unsigned char share_buf[SHARE_BUF_SIZE - 16];
};

extern enum scp_ipi_status scp_ipi_registration(unsigned char id,
	void (*ipi_handler)(int id, void *data, unsigned int len),
	const char *name);
extern enum scp_ipi_status scp_ipi_unregistration(unsigned char id);
extern enum scp_ipi_status scp_ipi_send(unsigned char id, void *buf,
	unsigned int len, unsigned int wait, enum scp_core_id scp_id);

extern void scp_A_ipi_handler(void);
extern int wake_up_scp(void);

extern unsigned char *scp_send_buff;
extern unsigned char *scp_recv_buff;
extern char *core_ids;

extern unsigned int is_scp_ready(void);
extern void scp_ipi_status_dump(void);
extern void scp_ipi_status_dump_id(unsigned char id);
#endif
