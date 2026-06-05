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

#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>

#include "mtk_jdec_hw.h"
#include "mtk_jdec_reg.h"

static int debug;
module_param(debug, int, 0644);

#define MTK_JDEC_POLL_TIMEOUT	1000
#define MTK_JDEC_DUMP_TRIG_REG	BIT(0)
#define MTK_JDEC_DUMP_DONE_REG	BIT(1)
#define MTK_JDEC_SIM_LOG	BIT(2)

#define jdec_writel(val, addr)						\
	do {								\
		if (debug & MTK_JDEC_SIM_LOG)				\
			pr_info("RISCWrite(h'%p, 32h'%x)\n", addr, val);\
		writel_relaxed(val, addr);				\
	} while (0)

#define jdec_writel_mb(val, addr)					\
	do {								\
		if (debug & MTK_JDEC_SIM_LOG)				\
			pr_info("RISCWrite(h'%p, 32h'%x)\n", addr, val);\
		writel(val, addr);					\
	} while (0)


static int mtk_jdec_g_comp_idx(struct mtk_jdec_dec_param *param,
			int sos_comp_idx)
{
	int sof_comp_idx = 0;
	bool comp_found = false;

	for (; sof_comp_idx < param->sof.comp_num; sof_comp_idx++)
		if (param->sof.comp_id[sof_comp_idx] ==
		    param->sos.comp_id[sos_comp_idx]) {
			comp_found = true;
			break;
		}

	if (comp_found &&
	    sof_comp_idx >= 0 && sof_comp_idx < MTK_JPEG_COMP_MAX)
		return sof_comp_idx;
	else {
		pr_warning("%s error comp id:%d found:%d idx:%d\n",
			__func__, param->sos.comp_id[sos_comp_idx],
			comp_found, sof_comp_idx);
		return 0;
	}
}

static int mtk_jdec_calc_derived_dht(struct mtk_jdec_dec_param *param,
				struct mtk_jdec_dht_tab *dht_tab)
{
	u8 size, huff_size[257] = {0};
	u16 code = 0, huff_code[257] = {0};
	int k = 0, bit_len, huff_code_num;

	/* Generate Huffman code size table, refer to Figure C.1 */
	for (bit_len = 1; bit_len < 17; bit_len++) {
		huff_code_num = dht_tab->num[bit_len];
		while (huff_code_num--) {
			if (k > 256)
				return -EINVAL;
			huff_size[k++] = bit_len;
		}
	}
	huff_size[k] = 0;

	/* Generate Huffman code table, refer to Figure C.2 */
	k = 0;
	size = huff_size[0];
	while (1) {
		do
			huff_code[k++] = code++;
		while (huff_size[k] == size);
		if (huff_size[k] == 0)
			break;

		do {
			code <<= 1;
			size++;
		} while (huff_size[k] != size);
	}

	/* generate maxcode & valoffset*/
	k = 0;
	for (bit_len = 1; bit_len < 17; bit_len++) {
		if (dht_tab->num[bit_len] == 0) {
			dht_tab->val_oft[bit_len] = 0;
			dht_tab->max_code[bit_len] = -1;
			continue;
		}
		dht_tab->val_oft[bit_len] = k - (int)huff_code[k];
		k += dht_tab->num[bit_len];
		if (k > 257 || k < 1) {
			pr_err("%s L:%d fail.\n", __func__, __LINE__);
			return -EINVAL;
		}
		dht_tab->max_code[bit_len] = huff_code[k - 1];
	}

	return 0;
}

void mtk_jdec_hw_dump_reg(void __iomem *base)
{
	int temp = 0xc0;
	uint32_t dump_sz = 0x380;

	pr_info("Begin to dump jdec reg\n");

	for (; temp < dump_sz; temp += 0x10) {
		pr_info("[%p]0x%8x   0x%8x   0x%8x   0x%8x\n",
			base + temp,
			readl_relaxed(base + temp),
			readl_relaxed(base + temp + 0x4),
			readl_relaxed(base + temp + 0x8),
			readl_relaxed(base + temp + 0xc));
	}

	base += 0xf00;
	for (temp = 0; temp < 0x30; temp += 0x10) {
		pr_info("[%p]0x%8x   0x%8x   0x%8x   0x%8x\n",
			base + temp,
			temp ? readl_relaxed(base + temp) : temp,
			readl_relaxed(base + temp + 0x4),
			readl_relaxed(base + temp + 0x8),
			readl_relaxed(base + temp + 0xc));
	}
}


static void mtk_jdec_hw_power_on(void __iomem *base, bool on)
{
	if (on)
		jdec_writel_mb(0, base + RW_JDEC_PWR_DOWN);
	else
		jdec_writel_mb(3, base + RW_JDEC_PWR_DOWN);
}

static void mtk_jdec_hw_reset(void __iomem *base)
{
	jdec_writel_mb(JDEC_DEC_SW_RST | JDEC_LARB_SW_RST | JDEC_AFIFO_SW_RST |
			JDEC_IOMMU_SW_RST, base + WO_JDEC_SW_RESET);
	jdec_writel_mb(JDEC_SW_RST_CLR, base + WO_JDEC_SW_RESET);
}

static void mtk_jdec_hw_set_misc(void __iomem *base)
{
	u32 imgrz_id = 0, val;

	val = readl_relaxed(base + RW_JDEC_MISC);
	val |= imgrz_id << 8;
	jdec_writel_mb(val, base + RW_JDEC_MISC);
}

static void mtk_jdec_hw_en_int(void __iomem *base)
{
	u32 val = readl_relaxed(base + RW_JDEC_HOST);

	jdec_writel(val | JDEC_INT_ENABLE | JDEC_INT_WAIT_BITS_RD,
			base + RW_JDEC_HOST);
}

void mtk_jdec_hw_init(void __iomem *base)
{
	mtk_jdec_hw_power_on(base, true);
	mtk_jdec_hw_reset(base);
	mtk_jdec_hw_set_misc(base);
	mtk_jdec_hw_en_int(base);
}

void mtk_jdec_hw_unint(void __iomem *base)
{
	mtk_jdec_hw_reset(base);
	mtk_jdec_hw_power_on(base, false);
}

void mtk_jdec_hw_clr_irq(void __iomem *base)
{
	u32 val = readl_relaxed(base + RW_JDEC_HOST);

	jdec_writel(val & (~JDEC_INT_ENABLE), base + RW_JDEC_HOST);
	jdec_writel(val | JDEC_INT_ENABLE, base + RW_JDEC_HOST);

	if (debug & MTK_JDEC_DUMP_DONE_REG) {
		pr_info("dump jdec reg after dec done\n");
		mtk_jdec_hw_dump_reg(base);
	}
}

static void mtk_jdec_hw_shift_barrel(void __iomem *base, u32 shift_bits)
{
	int i;
	u32 shift_bits_lo, shift_bits_hi;

	shift_bits_lo = shift_bits & 0x1f;
	shift_bits_hi = (shift_bits >> 5) & 0x3;

	for (i = 0; i < shift_bits_hi; i++)
		readl(base + RO_JDEC_BARL + (32 << 2));

	readl(base + RO_JDEC_BARL + (shift_bits_lo << 2));
}

static void mtk_jdec_hw_set_pic_size(void __iomem *base,
			struct mtk_jdec_dec_param *param)
{
	u32 idx = 0;
	u16 mcu_per_row = param->mcu_per_row;
	u16 total_mcu_row = param->total_mcu_row;

	if (param->sos.comp_num == 1) {
		idx = mtk_jdec_g_comp_idx(param, 0);
		mcu_per_row = DIV_ROUND_UP(param->sof.pic_w,
			param->wid_per_mcu / param->sof.sampling_h[idx]);
		total_mcu_row = DIV_ROUND_UP(param->sof.pic_h,
			param->hei_per_mcu / param->sof.sampling_v[idx]);
	}
	jdec_writel((mcu_per_row << JDEC_PIC_WIDTH_SHIFT) | total_mcu_row,
		base + RW_JDEC_PIC_SIZE);

}

static void mtk_jdec_hw_set_rst_interval(void __iomem *base,
			struct mtk_jdec_dec_param *param)
{
	if (param->dri.restart_interval)
		jdec_writel((param->dri.restart_interval - 1) | JDEC_DRI_ENABLE,
			base + RW_JDEC_DRI);
}


static void mtk_jdec_hw_set_dec_mode(void __iomem *base,
			enum mtk_jdec_dec_mode mode)
{
	u32 val = readl_relaxed(base + RW_JDEC_MB_ROW_DEC_SWITCH) & (~0x3);

	val |= JDEC_ROW_DEC_PIC_LEVEL;
	jdec_writel(val, base + RW_JDEC_MB_ROW_DEC_SWITCH);

	val = readl_relaxed(base + RW_JDEC_PRG_MODE);
	switch (mode) {
	case JDEC_DEC_MODE_BASELINE_PIC:
	case JDEC_DEC_MODE_PROGRESSIVE_SCAN_MULTI_COLLECT:
		jdec_writel(JDEC_COEF_GEN_NZ_MD_MC | JDEC_MSRAM_PWRSAVE |
				JDEC_HSRAM_PWRSAVE | JDEC_QSRAM_PWRSAVE,
				base + RW_JDEC_NEW_DEC);
		val &= ~JDEC_PRG_DEC;
		jdec_writel(val, base + RW_JDEC_PRG_MODE);
		break;
	case JDEC_DEC_MODE_PROGRESSIVE_SCAN_ENHANCE:
		jdec_writel(JDEC_COEF_GEN_NZ_MD_EH | JDEC_MSRAM_PWRSAVE |
				JDEC_HSRAM_PWRSAVE | JDEC_QSRAM_PWRSAVE,
				base + RW_JDEC_NEW_DEC);
		val |= JDEC_PRG_DEC;
		val |= JDEC_EOB_AUTO_FILL | JDEC_COEF_ZERO_IN;
		jdec_writel(val, base + RW_JDEC_PRG_MODE);
		break;
	default:
		break;
	}

}

static void mtk_jdec_hw_set_qtl(void __iomem *base,
			struct mtk_jdec_dec_param *param)
{
	int i, idx;
	u32 *qtl;

	jdec_writel_mb(0, base + RW_JDEC_BITS_START);
	jdec_writel_mb(0xffffffff, base + RW_JDEC_BITS_VEND);

	/* HW only support 2 quantization table */
	for (idx = 0; idx < 2; idx++) {
		jdec_writel_mb(idx << JDEC_Q_TAB_IDX_SHIFT,
				base + RW_JDEC_Q_TAB_IDX_IN);
		qtl = (u32 *)&param->dqt.element[idx];
		for (i = 0; i < 16; i++)
			jdec_writel_mb(*(qtl + i), base + RW_JDEC_Q_TAB_DAT_IN);
	}
}

static void mtk_jdec_hw_set_input_buf(void __iomem *base, u32 addr, u32 size)
{
	jdec_writel_mb(addr >> 6, base + RW_JDEC_BITS_START);
	jdec_writel_mb(ALIGN(addr + size, 64) >> 6, base + RW_JDEC_BITS_VEND);

}

static void mtk_jdec_hw_set_barrel_shifter_addr(void __iomem *base, u32 addr)
{
	int i = 0;
	u32 val = 0;

	if (readl(base + RO_JDEC_BS_INIT_ST) & JDEC_B_INIT_VALID) {
		if (readl_poll_timeout_atomic(base + RO_JDEC_BS_DEBUG, val,
			val & JDEC_BITS_PROC_RDY, 0, MTK_JDEC_POLL_TIMEOUT)) {
			pr_err("%s fail L:%d\n", __func__, __LINE__);
			mtk_jdec_hw_dump_reg(base);
		}
	}

	jdec_writel_mb(addr & (~0xf), base + RW_JDEC_BITS_RPTR);
	jdec_writel_mb(JDEC_AFIFO_SW_RST, base + WO_JDEC_SW_RESET);
	jdec_writel_mb(JDEC_SW_RST_CLR, base + WO_JDEC_SW_RESET);

	jdec_writel_mb(JDEC_INIT_FETCH, base + RW_JDEC_BS_INIT);
	i = 0;
	if (readl_poll_timeout_atomic(base + RO_JDEC_BS_INIT_ST, val,
			val & JDEC_FETCH_READY, 0, MTK_JDEC_POLL_TIMEOUT)) {
		pr_err("%s fail L:%d\n", __func__, __LINE__);
		mtk_jdec_hw_dump_reg(base);
	}

	jdec_writel_mb(JDEC_INIT_BS, base + RW_JDEC_BS_INIT);
	i = 0;
	while (!(readl(base + RO_JDEC_BS_INIT_ST) & JDEC_INIT_VALID) ||
	       (readl(base + RO_JDEC_BS_DEBUG) & JDEC_BITS_DMA_PROC)) {
		if (i++ > 100000) {
			pr_err("%s fail L:%d\n", __func__, __LINE__);
			mtk_jdec_hw_dump_reg(base);
		}
	}

	mtk_jdec_hw_shift_barrel(base, (addr & 0xf) << 3);
}

static void mtk_jdec_hw_set_sw_wr_ptr(void __iomem *base, u32 addr)
{
	jdec_writel_mb(ALIGN(addr, 16), base + RW_JDEC_SW_WP);
}

static void mtk_jdec_hw_set_pitch_per_row(void __iomem *base,
			struct mtk_jdec_dec_param *param)
{
	u16 comp0_pitch, comp1_pitch, comp2_pitch;
	bool last_scan;

	if (param->sos.comp_num == 1) {
		comp0_pitch =
			param->out.pitch[mtk_jdec_g_comp_idx(param, 0)];
		comp1_pitch = 0;
		comp2_pitch = 0;
	} else {
		comp0_pitch = param->out.pitch[0];
		comp1_pitch = param->out.pitch[1];
		comp2_pitch = param->out.pitch[2];
	}

	jdec_writel(comp0_pitch >> 2 | (comp1_pitch >> 2) << 14,
			base + RW_JDEC_IDCT_WIDTH01);

	last_scan = param->dec_md == JDEC_DEC_MODE_PROGRESSIVE_SCAN_ENHANCE ||
		(param->sos.se == 63 && param->sos.al == 0);
	if (last_scan) {
		jdec_writel(comp2_pitch >> 2 | (comp0_pitch >> 2) << 14,
			base + RW_JDEC_IDCT_WIDTH2_COEF_WIDTH0);
		jdec_writel((comp1_pitch >> 2) | (comp2_pitch >> 2) << 14,
			base + RW_JDEC_COEF_WIDTH12);

	} else if (param->dec_md ==
			JDEC_DEC_MODE_PROGRESSIVE_SCAN_MULTI_COLLECT) {

		jdec_writel(comp2_pitch >> 2 | (comp0_pitch >> 1) << 14,
			base + RW_JDEC_IDCT_WIDTH2_COEF_WIDTH0);
		jdec_writel((comp1_pitch >> 1) | (comp2_pitch >> 1) << 14,
			base + RW_JDEC_COEF_WIDTH12);

	} else
		jdec_writel(comp2_pitch >> 2,
				base + RW_JDEC_IDCT_WIDTH2_COEF_WIDTH0);
}

static void mtk_jdec_hw_init_scan(void __iomem *base)
{
	jdec_writel_mb(0, base + RW_JDEC_RESTART);
	jdec_writel_mb(JDEC_MCU_RST | JDEC_SOS_CLR | JDEC_MKR_CLR,
			base + RW_JDEC_RESTART);
	jdec_writel_mb(0, base + RW_JDEC_RESTART);
}

static void mtk_jdec_hw_set_maxcode_tbl(void __iomem *base,
				struct mtk_jdec_dht_tab *dht_tab)
{
	int i;

	for (i = 1; i < 17; i++)
		jdec_writel_mb(dht_tab->max_code[i],
				base + WO_JDEC_MAXCODE_VALOFT_DAT);
}

static void mtk_jdec_hw_set_valoft_tbl(void __iomem *base,
				struct mtk_jdec_dht_tab *dht_tab)
{
	int i;

	for (i = 1; i < 17; i++)
		jdec_writel_mb(dht_tab->val_oft[i],
				base + WO_JDEC_MAXCODE_VALOFT_DAT);
}

static void mtk_jdec_hw_set_huff_val(void __iomem *base,
			bool dc, u8 idx, u8 *huff_val)
{
	u32 i, val;

	val = readl(base + RW_JDEC_HUFF_TAB_SEL);
	jdec_writel_mb(val | idx << 8 |	(dc ? JDEC_DC_TABLE : JDEC_AC_TABLE),
			base + RW_JDEC_HUFF_TAB_SEL);
	for (i = 0; i < 256; i += 4) {
		val = ((((u32)huff_val[i + 3]) << 24) |
			(((u32)huff_val[i + 2]) << 16) |
			(((u32)huff_val[i + 1]) << 8) | huff_val[i]);
		jdec_writel_mb(val, base + RW_JDEC_HUFF_VAL);
	}

}

static void mtk_jdec_hw_set_dht(void __iomem *base,
			struct mtk_jdec_dec_param *param)
{
	int i;

	jdec_writel_mb(0, base + RW_JDEC_MAXCODE_VALOFT_IDX);

	for (i = 0; i < 4; i++) {
		mtk_jdec_calc_derived_dht(param, &param->dht.dc[i]);
		mtk_jdec_calc_derived_dht(param, &param->dht.ac[i]);
	}

	for (i = 0; i < 4; i++)
		mtk_jdec_hw_set_maxcode_tbl(base, &param->dht.dc[i]);
	for (i = 0; i < 4; i++)
		mtk_jdec_hw_set_valoft_tbl(base, &param->dht.dc[i]);

	for (i = 0; i < 4; i++)
		mtk_jdec_hw_set_maxcode_tbl(base, &param->dht.ac[i]);
	for (i = 0; i < 4; i++)
		mtk_jdec_hw_set_valoft_tbl(base, &param->dht.ac[i]);

	for (i = 0; i < 4; i++)
		mtk_jdec_hw_set_huff_val(base, true, i, param->dht.dc[i].val);
	for (i = 0; i < 4; i++)
		mtk_jdec_hw_set_huff_val(base, false, i, param->dht.ac[i].val);
}

static void mtk_jdec_hw_set_curr_qtl_id(void __iomem *base,
			struct mtk_jdec_dec_param *param)
{
	u32 i, comp_idx, val = 0;

	for (i = 0; i < param->sos.comp_num; i++) {
		comp_idx = mtk_jdec_g_comp_idx(param, i);
		val |= param->sof.qtbl_num[comp_idx] << (i << 1);
		jdec_writel(val, base + RW_JDEC_Q_TBL_NO);
	}
}

static void mtk_jdec_hw_set_ss_se(void __iomem *base,
			struct mtk_jdec_dec_param *param)
{
	jdec_writel((param->sos.ss << 8) | param->sos.se, base + RW_JDEC_SS_SE);
}

static void mtk_jdec_hw_set_ah_al(void __iomem *base,
			struct mtk_jdec_dec_param *param)
{
	jdec_writel((param->sos.ah << 8) | param->sos.al, base + RW_JDEC_AH_AL);
}

static void mtk_jdec_hw_set_dc_ac(void __iomem *base,
			struct mtk_jdec_dec_param *param)
{
	u32 val = readl(base + RW_JDEC_PRG_MODE) & (~0x7);

	if (param->sos.ah == 0)
		val |= param->sos.ss == 0 ? JDEC_DC_FIRST : JDEC_AC_FIRST;
	else
		val |= param->sos.ss == 0 ? JDEC_DC_REFINE : JDEC_AC_REFINE;
	jdec_writel(val, base + RW_JDEC_PRG_MODE);
}

static void mtk_jdec_hw_set_last_scan(void __iomem *base,
			struct mtk_jdec_dec_param *param)
{
	bool last_scan = true;
	u32 val = readl(base + RW_JDEC_PRG_MODE);

	if (param->dec_md == JDEC_DEC_MODE_PROGRESSIVE_SCAN_MULTI_COLLECT &&
	    !(param->sos.se == 63 && param->sos.al == 0))
		last_scan = false;

	if (last_scan)
		val |= JDEC_IDCT_OUT;
	else
		val &= ~JDEC_IDCT_OUT;
	jdec_writel(val, base + RW_JDEC_PRG_MODE);
}

static void mtk_jdec_hw_set_blk_param(void __iomem *base,
			struct mtk_jdec_dec_param *param)
{
	u32 i, x, y, val, comp_idx, blk_num, blk = 0, total_blk = 0;
	u32 dc_tlist = 0, ac_tlist = 0, mcu_mlist = 0;
	u32 dc_needed_list = 0, x_pos_list = 0, y_pos_list = 0;

	for (i = 0; i < param->sos.comp_num; i++) {
		comp_idx = mtk_jdec_g_comp_idx(param, i);
		if (param->sos.comp_num == 1)
			blk_num = 1;
		else
			blk_num = param->sof.sampling_h[comp_idx] *
				  param->sof.sampling_v[comp_idx];

		total_blk += blk_num;
		for (y = 0; y < param->sof.sampling_v[comp_idx]; y++)
			for (x = 0; x < param->sof.sampling_h[comp_idx]; x++) {
				dc_tlist |= param->sos.dc_id[i] << (blk * 2);
				ac_tlist |= param->sos.ac_id[i] << (blk * 2);
				mcu_mlist |= i << (blk * 2);
				dc_needed_list |= 1 << blk;
				x_pos_list |= (x & 0x7) << (blk * 3);
				y_pos_list |= (y & 0x7) << (blk * 3);
				blk++;
			}
	}
	jdec_writel(dc_tlist, base + RW_JDEC_BLK_DC_TBL);
	jdec_writel(ac_tlist, base + RW_JDEC_BLK_AC_TBL);
	jdec_writel(mcu_mlist, base + RW_JDEC_MCU_MLIST);
	val = readl(base + RW_JDEC_BLK_PLIST) & (~0xf0003ff);
	val |= (dc_needed_list & 0x3ff) | ((total_blk & 0xf) << 24);
	jdec_writel(val, base + RW_JDEC_BLK_PLIST);
	jdec_writel(x_pos_list, base + RW_JDEC_X_IN_MCU);
	jdec_writel(y_pos_list, base + RW_JDEC_Y_IN_MCU);
}

static void mtk_jdec_hw_set_nonzero_history_buf(void __iomem *base,
			struct mtk_jdec_dec_param *param)
{
	jdec_writel(param->out.nonzero_history, base + RW_JDEC_NZ_HIST);
}

static void mtk_jdec_hw_set_mcu_h_size(void __iomem *base,
			struct mtk_jdec_dec_param *param)
{
	u32 i, val = 0, comp_idx;

	if (param->sos.comp_num == 1)
		val = (1 << 8) + (1 << 4) + 1;
	else
		for (i = 0; i < param->sos.comp_num; i++) {
			comp_idx = mtk_jdec_g_comp_idx(param, i);
			val |= param->sof.sampling_h[comp_idx] << (i * 4);
		}
	jdec_writel(val, base + RW_JDEC_MCU_H_SIZE);
}

static void mtk_jdec_hw_set_coef_pitch(void __iomem *base,
			struct mtk_jdec_dec_param *param)
{
	u32 i, comp_idx, coef_pitch[3] = {0};

	for (i = 0; i < param->sof.comp_num; i++)
		coef_pitch[i] = param->mcu_per_row *
				param->sof.sampling_h[i] *
				param->sof.sampling_v[i] * 128 / 4;
	if (param->sos.comp_num == 1) {
		comp_idx = mtk_jdec_g_comp_idx(param, 0);
		jdec_writel(coef_pitch[comp_idx] /
			param->sof.sampling_v[comp_idx],
			base + RW_JDEC_COEF_PITCH_0);
		return;
	}

	jdec_writel(coef_pitch[0], base + RW_JDEC_COEF_PITCH_0);
	jdec_writel(coef_pitch[1], base + RW_JDEC_COEF_PITCH_1);
	jdec_writel(coef_pitch[2], base + RW_JDEC_COEF_PITCH_2);

}

static void mtk_jdec_hw_set_coef_buf(void __iomem *base,
			struct mtk_jdec_dec_param *param)
{
	if (param->sos.comp_num == 1) {
		jdec_writel(param->out.coef_buf[mtk_jdec_g_comp_idx(param, 0)]
				>> 2, base + RW_JDEC_COEF_ADDR0);
		return;
	}

	jdec_writel(param->out.coef_buf[0] >> 2, base + RW_JDEC_COEF_ADDR0);
	jdec_writel(param->out.coef_buf[1] >> 2, base + RW_JDEC_COEF_ADDR1);
	jdec_writel(param->out.coef_buf[2] >> 2, base + RW_JDEC_COEF_ADDR2);
}

static void mtk_jdec_hw_set_out_buf(void __iomem *base,
			struct mtk_jdec_dec_param *param)
{
	if (param->sos.comp_num == 1) {
		jdec_writel(param->out.out_buf[mtk_jdec_g_comp_idx(param, 0)]
				>> 2, base + RW_JDEC_WR_ADDR0);
		return;
	}

	jdec_writel(param->out.out_buf[0] >> 2, base + RW_JDEC_WR_ADDR0);
	jdec_writel(param->out.out_buf[1] >> 2, base + RW_JDEC_WR_ADDR1);
	jdec_writel(param->out.out_buf[2] >> 2, base + RW_JDEC_WR_ADDR2);
}

static void mtk_jdec_hw_set_bank0_buf(void __iomem *base,
			struct mtk_jdec_dec_param *param)
{
	u32 val = readl(base + RW_JDEC_MB_ROW_DEC_SWITCH);

	val |= JDEC_ROW_DEC_WR_ADDR;
	val &= ~(JDEC_ROW_DEC_WR_BANK1_ADDR);
	jdec_writel_mb(val, base + RW_JDEC_MB_ROW_DEC_SWITCH);

	if (param->sos.comp_num == 1) {
		jdec_writel(
			param->out.bank0[mtk_jdec_g_comp_idx(param, 0)] >> 2,
			base + RW_JDEC_ROW_DEC_COMP0_ADDR);
		jdec_writel(
			param->out.bank0[mtk_jdec_g_comp_idx(param, 0)] >> 2,
			base + RW_JDEC_ROW_DEC_COMP1_ADDR);
		jdec_writel(
			param->out.bank0[mtk_jdec_g_comp_idx(param, 0)] >> 2,
			base + RW_JDEC_ROW_DEC_COMP2_ADDR);
		return;
	}
	jdec_writel(param->out.bank0[0] >> 2,
			base + RW_JDEC_ROW_DEC_COMP0_ADDR);
	jdec_writel(param->out.bank0[1] >> 2,
			base + RW_JDEC_ROW_DEC_COMP1_ADDR);
	jdec_writel(param->out.bank0[2] >> 2,
			base + RW_JDEC_ROW_DEC_COMP2_ADDR);
}

static void mtk_jdec_hw_set_bank1_buf(void __iomem *base,
			struct mtk_jdec_dec_param *param)
{
	u32 val = readl(base + RW_JDEC_MB_ROW_DEC_SWITCH);

	val |= JDEC_ROW_DEC_WR_ADDR | JDEC_ROW_DEC_WR_BANK1_ADDR;
	jdec_writel_mb(val, base + RW_JDEC_MB_ROW_DEC_SWITCH);

	if (param->sos.comp_num == 1) {
		jdec_writel(param->out.bank1[mtk_jdec_g_comp_idx(param, 0)]
				>> 2, base + RW_JDEC_ROW_DEC_COMP0_ADDR);
		jdec_writel(param->out.bank1[mtk_jdec_g_comp_idx(param, 0)]
				>> 2, base + RW_JDEC_ROW_DEC_COMP1_ADDR);
		jdec_writel(param->out.bank1[mtk_jdec_g_comp_idx(param, 0)]
				>> 2, base + RW_JDEC_ROW_DEC_COMP2_ADDR);
		return;
	}
	jdec_writel(param->out.bank1[0] >> 2,
			base + RW_JDEC_ROW_DEC_COMP0_ADDR);
	jdec_writel(param->out.bank1[1] >> 2,
			base + RW_JDEC_ROW_DEC_COMP1_ADDR);
	jdec_writel(param->out.bank1[2] >> 2,
			base + RW_JDEC_ROW_DEC_COMP2_ADDR);
}

static void __attribute__((unused))
mtk_jdec_hw_set_err_conceal(void __iomem *base,
			struct mtk_jdec_dec_param *param)
{
	u32 val = readl(base + RW_JDEC_ERROR_CONCEAL);

	if (param->err_conceal)
		val |= (JDEC_EC_COEF_DEC |
			JDEC_EC_BLK_OVERFLOW | JDEC_EC_RST_MKR);
	else
		val &= ~(JDEC_EC_COEF_DEC |
			JDEC_EC_BLK_OVERFLOW | JDEC_EC_RST_MKR);
	jdec_writel(val, base + RW_JDEC_ERROR_CONCEAL);
}

int mtk_jdec_hw_get_err_status(void __iomem *base)
{
	return 0;
}

void mtk_jdec_hw_trig_dec(void __iomem *base,
			struct mtk_jdec_dec_param *param)
{
	if (debug & MTK_JDEC_DUMP_TRIG_REG) {
		pr_info("dump jdec reg before dec trigger\n");
		mtk_jdec_hw_dump_reg(base);
	}
	jdec_writel_mb(JDEC_DEC_FIRST_ROW, base + RW_JDEC_DEC_MCU_ROW_TRIG);
	jdec_writel_mb(0, base + RW_JDEC_DEC_MCU_ROW_TRIG);
}

void mtk_jdec_hw_config(void __iomem *base,
			struct mtk_jdec_dec_param *param)
{
	mtk_jdec_hw_set_dec_mode(base, param->dec_md);
	mtk_jdec_hw_set_rst_interval(base, param);
	mtk_jdec_hw_set_qtl(base, param);
	mtk_jdec_hw_set_input_buf(base,
			param->stream.dma_addr, param->stream.size);
	mtk_jdec_hw_set_barrel_shifter_addr(base,
			param->stream.dma_addr + param->stream.curr);
	mtk_jdec_hw_set_sw_wr_ptr(base,
			param->stream.dma_addr + param->stream.size);

	/* SCAN start*/
	mtk_jdec_hw_init_scan(base);
	mtk_jdec_hw_set_dht(base, param);
	mtk_jdec_hw_set_curr_qtl_id(base, param);
	if (param->dec_md != JDEC_DEC_MODE_BASELINE_PIC) {
		mtk_jdec_hw_set_ss_se(base, param);
		mtk_jdec_hw_set_ah_al(base, param);
		mtk_jdec_hw_set_dc_ac(base, param);
		mtk_jdec_hw_set_coef_pitch(base, param);
	}
	mtk_jdec_hw_set_last_scan(base, param);
	mtk_jdec_hw_set_blk_param(base, param);
	mtk_jdec_hw_set_pitch_per_row(base, param);
	mtk_jdec_hw_set_mcu_h_size(base, param);
	mtk_jdec_hw_set_pic_size(base, param);
	if (param->dec_md == JDEC_DEC_MODE_PROGRESSIVE_SCAN_ENHANCE)
		mtk_jdec_hw_set_nonzero_history_buf(base, param);
	else if (param->dec_md == JDEC_DEC_MODE_BASELINE_PIC)
		mtk_jdec_hw_set_coef_buf(base, param);
	/* SCAN end*/
	mtk_jdec_hw_set_out_buf(base, param);
	mtk_jdec_hw_set_bank0_buf(base, param);
	mtk_jdec_hw_set_bank1_buf(base, param);
}
