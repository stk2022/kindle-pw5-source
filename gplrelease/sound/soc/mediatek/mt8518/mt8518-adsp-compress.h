/*
 * mt8518-adsp-compress.h  --  Mediatek 8518 adsp compress offload driver
 *
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Hidalgo Huang <hidalgo.huang@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _MT8518_ADSP_COMPRESS_H_
#define _MT8518_ADSP_COMPRESS_H_

struct ipi_msg_t;

extern const struct snd_compr_ops mt8518_adsp_compr_ops;

void mt8518_adsp_compr_ipi_recv_msg(struct ipi_msg_t *p_ipi_msg);

#endif
