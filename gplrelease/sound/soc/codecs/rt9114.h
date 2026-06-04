/*
 *  sound/soc/codecs/rt9114.h
 *
 *  Copyright (C) 2018 Richtek Technology Corp.
 *  cy_huang <cy_huang@richtek.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __SND_SOC_RT5510_H
#define __SND_SOC_RT5510_H

#include <linux/mutex.h>
#include <mt-plat/rt-regmap.h>

struct rt9114_platform_data {
	int shutdown_gpio;
};

struct rt9114_info {
	struct i2c_client *i2c;
	struct device *dev;
	struct snd_soc_codec *codec;
	struct rt_regmap_device *regmap;
	struct mutex var_lock;
	int pwr_cnt;
	unsigned int fmt_cache;
};

extern int rt9114_regmap_register(struct rt9114_info *info);
extern void rt9114_regmap_unregister(struct rt9114_info *info);

/* reg address divide into two parts [16:8] size, [7:0] addr */
#define RT9114_GET_ADDR(_addr)		((_addr) & 0xff)
#define RT9114_GET_SIZE(_addr)		(((_addr) >> 8) & 0xff)

#define RT9114_REG_I2S_FMT_RPT		(0x100)
#define RT9114_REG_DEV_ID		(0x101)
#define RT9114_REG_ERR_RPT1		(0x102)
#define RT9114_REG_FLTR_MISC		(0x103)
#define RT9114_REG_I2S_FMT		(0x104)
#define RT9114_REG_ENABLE		(0x105)
#define RT9114_REG_CH_MUTE		(0x106)
#define RT9114_REG_MS_VOL		(0x207)
#define RT9114_REG_CH1_VOL		(0x208)
#define RT9114_REG_CH2_VOL		(0x209)
#define RT9114_REG_DRC4_DELAY		(0x10A)
#define RT9114_REG_VOL_RAMP		(0x10C)
#define RT9114_REG_SDIN_SEL		(0x10D)
#define RT9114_REG_AUTO_RCVRY		(0x10E)
#define RT9114_REG_ERR_RPT2		(0x10F)
#define RT9114_REG_DAC_OPT1		(0x111)
#define RT9114_REG_CLASS_D_OPT1		(0x112)
#define RT9114_REG_TEST_MODE		(0x113)
#define RT9114_REG_INTER_PWR_CTRL	(0x114)
#define RT9114_REG_OFFSET_CAL1		(0x115)
#define RT9114_REG_PWM_SS_OPT1		(0x116)
#define RT9114_REG_ANA_BIAS1		(0x117)
#define RT9114_REG_OC_TEST_MODE		(0x118)
#define RT9114_REG_PWM_SS_OPT2		(0x119)
#define RT9114_REG_SPK_GAIN		(0x11A)
#define RT9114_REG_PLL_CONFIG1		(0x11B)
#define RT9114_REG_PLL_CONFIG2		(0x11C)
#define RT9114_REG_PLL_CONFIG3		(0x11D)
#define RT9114_REG_PLL_CONFIG4		(0x11E)
#define RT9114_REG_PLL_CONFIG5		(0x420)
#define RT9114_REG_COMP_FLTR1		(0x421)
#define RT9114_REG_COMP_FLTR2		(0x422)
#define RT9114_REG_CH1_BQ1		(0x1426)
#define RT9114_REG_CH1_BQ2		(0x1427)
#define RT9114_REG_CH1_BQ3		(0x1428)
#define RT9114_REG_CH1_BQ4		(0x1429)
#define RT9114_REG_CH1_BQ5		(0x142A)
#define RT9114_REG_CH1_BQ6		(0x142B)
#define RT9114_REG_CH1_BQ7		(0x142C)
#define RT9114_REG_CH1_BQ8		(0x142D)
#define RT9114_REG_CH1_BQ9		(0x142E)
#define RT9114_REG_CH1_BQ10		(0x142F)
#define RT9114_REG_CH2_BQ1		(0x1430)
#define RT9114_REG_CH2_BQ2		(0x1431)
#define RT9114_REG_CH2_BQ3		(0x1432)
#define RT9114_REG_CH2_BQ4		(0x1433)
#define RT9114_REG_CH2_BQ5		(0x1434)
#define RT9114_REG_CH2_BQ6		(0x1435)
#define RT9114_REG_CH2_BQ7		(0x1436)
#define RT9114_REG_CH2_BQ8		(0x1437)
#define RT9114_REG_CH2_BQ9		(0x1438)
#define RT9114_REG_CH2_BQ10		(0x1439)
#define RT9114_REG_DRC1_RMS_AE		(0x83A)
#define RT9114_REG_DRC1_GAIN_AA		(0x83B)
#define RT9114_REG_DRC1_GAIN_AD		(0x83C)
#define RT9114_REG_DRC2_RMS_AE		(0x83D)
#define RT9114_REG_DRC2_GAIN_AA		(0x83E)
#define RT9114_REG_DRC2_GAIN_AD		(0x83F)
#define RT9114_REG_DRC1_TH		(0x440)
#define RT9114_REG_DRC1_RATIO		(0x441)
#define RT9114_REG_DRC1_OFFSET		(0x442)
#define RT9114_REG_DRC2_TH		(0x443)
#define RT9114_REG_DRC2_RATIO		(0x444)
#define RT9114_REG_DRC2_OFFSET		(0x445)
#define RT9114_REG_EQ_DRC_EN		(0x446)
#define RT9114_REG_DRC3_RMS_AE		(0x847)
#define RT9114_REG_DRC3_GAIN_AA		(0x848)
#define RT9114_REG_DRC3_GAIN_AD		(0x849)
#define RT9114_REG_DRC3_TH		(0x44A)
#define RT9114_REG_DRC3_RATIO		(0x44B)
#define RT9114_REG_DRC3_OFFSET		(0x44C)
#define RT9114_REG_CH1_OUT_MIX		(0x851)
#define RT9114_REG_CH2_OUT_MIX		(0x852)
#define RT9114_REG_CH1_IN_MIX		(0x853)
#define RT9114_REG_CH2_IN_MIX		(0x854)
#define RT9114_REG_POST_SCALE		(0x456)
#define RT9114_REG_PRE_SCALE		(0x457)
#define RT9114_REG_CH1_BQ11		(0x1458)
#define RT9114_REG_CH1_BQ12		(0x1459)
#define RT9114_REG_CH2_BQ11		(0x145A)
#define RT9114_REG_CH2_BQ12		(0x145B)
#define RT9114_REG_LB_CH1_BQ1		(0x145C)
#define RT9114_REG_LB_CH1_BQ2		(0x145D)
#define RT9114_REG_LB_CH2_BQ1		(0x145E)
#define RT9114_REG_LB_CH2_BQ2		(0x145F)
#define RT9114_REG_CH1_MBDRC_MIX	(0xC60)
#define RT9114_REG_CH2_MBDRC_MIX	(0xC61)
#define RT9114_REG_HARD_CLIP		(0x462)
#define RT9114_REG_DRC_CK_MODE		(0x170)
#define RT9114_REG_ERR_LATCH		(0x171)
#define RT9114_REG_DRC_ENABLE		(0x172)
#define RT9114_REG_SDO_SEL		(0x173)
#define RT9114_REG_OC_LEVEL		(0x174)
#define RT9114_REG_UVP_AD		(0x175)
#define RT9114_REG_DC_PROT		(0x176)
#define RT9114_REG_EQ_GAIN_BOOST1	(0x177)
#define RT9114_REG_MB_CH1_BQ1		(0x1478)
#define RT9114_REG_MB_CH1_BQ2		(0x1479)
#define RT9114_REG_MB_CH2_BQ1		(0x147A)
#define RT9114_REG_MB_CH2_BQ2		(0x147B)
#define RT9114_REG_HB_CH1_BQ1		(0x147C)
#define RT9114_REG_HB_CH1_BQ2		(0x147D)
#define RT9114_REG_HB_CH2_BQ1		(0x147E)
#define RT9114_REG_HB_CH2_BQ2		(0x147F)
#define RT9114_REG_SW_RESET		(0x180)
#define RT9114_REG_PAD_DRV		(0x181)
#define RT9114_REG_SDM_OPT		(0x182)
#define RT9114_REG_CK_TEST_MODE		(0x183)
#define RT9114_REG_PBTL_SS_OPT3		(0x184)
#define RT9114_REG_OFFSET_CAL1_L	(0x185)
#define RT9114_REG_OFFSET_CAL1_R	(0x186)
#define RT9114_REG_OFFSET_RPT1_L	(0x187)
#define RT9114_REG_OFFSET_RPT1_R	(0x188)
#define RT9114_REG_OFFSET_RPT2_L	(0x189)
#define RT9114_REG_OFFSET_RPT2_R	(0x18A)
#define RT9114_REG_BIST_RPT		(0x18D)
#define RT9114_REG_BIST_EN		(0x18E)
#define RT9114_REG_SCAN_MODE		(0x18F)
#define RT9114_REG_DRC_NG_TH		(0x4A2)
#define RT9114_REG_DRC4_TH		(0x4A6)
#define RT9114_REG_DRC4_RATIO		(0x4A7)
#define RT9114_REG_DRC4_OFFSET		(0x4A8)
#define RT9114_REG_DRC4_AE		(0x8AC)
#define RT9114_REG_DRC4_AA		(0x8AD)
#define RT9114_REG_DRC4_AD		(0x8AE)
#define RT9114_REG_OUT_LEVEL_A		(0x8AF)
#define RT9114_REG_CH1_RMS_RPT		(0x4B0)
#define RT9114_REG_CH2_RMS_RPT		(0x4B1)
#define RT9114_REG_GAIN_OPT		(0x1C0)
#define RT9114_REG_SKIP_BQ1		(0x1C1)
#define RT9114_REG_SKIP_BQ2		(0x1C2)
#define RT9114_REG_EQ_L_GAIN_BOOST	(0x1C3)
#define RT9114_REG_EQ_R_GAIN_BOOST	(0x1C4)
#define RT9114_REG_DF_GAIN		(0x1C5)
#define RT9114_REG_ERR_MASK		(0x1D0)
#define RT9114_REG_ERR_TYPE		(0x1D1)
#define RT9114_REG_DIG_TEST1		(0x1F0)
#define RT9114_REG_DIG_TEST2		(0x1F1)
#define RT9114_REG_DIG_TEST3		(0x1F2)
#define RT9114_REG_DIG_TEST4		(0x1F3)
#define RT9114_REG_DIG_TEST5		(0x1F4)

#endif /* #ifndef __SND_SOC_RT5510_H */
