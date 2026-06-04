/*
 * mt8518-adsp-controls.h  --  Mediatek 8518 adsp controls
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

#ifndef _MT8518_ADSP_CONTROLS_H_
#define _MT8518_ADSP_CONTROLS_H_

struct snd_soc_platform;

#define WWE_DETECT_STS_CTL_NAME "Wakeword_Detect_Status"

int mt8518_adsp_add_controls(struct snd_soc_platform *platform);

#endif
