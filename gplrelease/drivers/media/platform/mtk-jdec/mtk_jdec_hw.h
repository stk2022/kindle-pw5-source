/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Scott Wang <Scott.Wang@mediatek.com>
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

#ifndef _MTK_JDEC_HW_H
#define _MTK_JDEC_HW_H

#include "mtk_jdec_common.h"

void mtk_jdec_hw_init(void __iomem *base);
void mtk_jdec_hw_unint(void __iomem *base);
void mtk_jdec_hw_clr_irq(void __iomem *base);
void mtk_jdec_hw_trig_dec(void __iomem *base,
			struct mtk_jdec_dec_param *param);
void mtk_jdec_hw_config(void __iomem *base,
			struct mtk_jdec_dec_param *param);
void mtk_jdec_hw_dump_reg(void __iomem *base);
int mtk_jdec_hw_get_err_status(void __iomem *base);

#endif /* _MTK_JDEC_HW_H */

