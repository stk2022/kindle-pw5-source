/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Qiqi Wang <Qiqi.wang@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __PWM_REGULATOR_MEDIATEK_H__
#define __PWM_REGULATOR_MEDIATEK_H__

extern int mtk_pwm_set_thresh(struct pwm_device *pwm, int thresh);
extern int mtk_pwm_get_thresh(struct pwm_device *pwm);

#endif
