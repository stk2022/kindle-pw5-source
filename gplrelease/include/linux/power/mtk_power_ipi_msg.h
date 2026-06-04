/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __MTK_POWER_IPI_MSG_H
#define __MTK_POWER_IPI_MSG_H

/* Need to sync with DSP side */
enum mtk_power_ipi_msg {
	MTK_POWER_IPIMSG_EINT_INIT = 0,
	MTK_POWER_IPIMSG_EINT_EVT,
	MTK_POWER_IPIMSG_MAX,
};

#endif /* __MTK_POWER_IPI_MSG_H */
