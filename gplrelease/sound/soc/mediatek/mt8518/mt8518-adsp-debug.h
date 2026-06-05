/*
 * mt8518-adsp-debug.h  --  Mediatek 8518 adsp debug function
 *
 * Copyright (c) 2019 MediaTek Inc.
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

#ifndef __MT8518_ADSP_DEBUG_H__
#define __MT8518_ADSP_DEBUG_H__

struct mt8518_audio_spi_priv;
struct ipi_msg_t;

void mt8518_adsp_init_debug_dump(struct mt8518_audio_spi_priv *priv);

void mt8518_adsp_cleanup_debug_dump(struct mt8518_audio_spi_priv *priv);

void mt8518_adsp_handle_debug_dump_irq(struct ipi_msg_t *p_ipi_msg);

#endif
