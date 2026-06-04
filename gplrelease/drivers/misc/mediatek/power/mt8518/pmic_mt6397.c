/*
 * Copyright (c) 2017 MediaTek Inc.
 * Author: Chen Zhong <chen.zhong@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irqdomain.h>
#include <linux/kernel.h>
#include <linux/kernel.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/mfd/mt6397/registers.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/version.h>

#include <mt-plat/upmu_common.h>
#include <mt-plat/mtk_chip.h>

#define E_PWR_INVALID_DATA 33

struct regmap *pwrap_regmap;

unsigned int pmic_read_interface(unsigned int RegNum, unsigned int *val,
				 unsigned int MASK, unsigned int SHIFT)
{
	unsigned int return_value = 0;
	unsigned int pmic_reg = 0;

	return_value = regmap_read(pwrap_regmap, RegNum, &pmic_reg);
	if (return_value != 0) {
		pr_notice("[Power/PMIC][%s] Reg[%x]=pmic_wrap read data fail\n",
				__func__, RegNum);
		return return_value;
	}

	pmic_reg &= (MASK << SHIFT);
	*val = (pmic_reg >> SHIFT);

	return return_value;
}

unsigned int pmic_config_interface(unsigned int RegNum, unsigned int val,
				   unsigned int MASK, unsigned int SHIFT)
{
	unsigned int return_value = 0;
	unsigned int pmic_reg = 0;

	if (val > MASK) {
		pr_notice("[Power/PMIC][%s] Invalid data, Reg[%x]:MASK = 0x%x, val = 0x%x\n",
				__func__, RegNum, MASK, val);
		return E_PWR_INVALID_DATA;
	}

	return_value = regmap_read(pwrap_regmap, RegNum, &pmic_reg);
	if (return_value != 0) {
		pr_notice("[Power/PMIC][%s] Reg[%x]=pmic_wrap read data fail\n",
				__func__, RegNum);
		return return_value;
	}

	pmic_reg &= ~(MASK << SHIFT);
	pmic_reg |= (val << SHIFT);

	return_value = regmap_write(pwrap_regmap, RegNum, pmic_reg);
	if (return_value != 0) {
		pr_notice("[Power/PMIC][%s] Reg[%x]= pmic_wrap read data fail\n",
				__func__, RegNum);
		return return_value;
	}

	return return_value;
}

u32 upmu_get_reg_value(u32 reg)
{
	u32 reg_val = 0;

	pmic_read_interface(reg, &reg_val, 0xFFFF, 0x0);

	return reg_val;
}
EXPORT_SYMBOL(upmu_get_reg_value);

void upmu_set_reg_value(u32 reg, u32 reg_val)
{
	pmic_config_interface(reg, reg_val, 0xFFFF, 0x0);
}

u32 g_reg_value;
static ssize_t show_pmic_access(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	pr_notice("[show_pmic_access] 0x%x\n", g_reg_value);
	return sprintf(buf, "%04X\n", g_reg_value);
}

static ssize_t store_pmic_access(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t size)
{
	int ret = 0;
	char temp_buf[32];
	char *pvalue;
	unsigned int reg_value = 0;
	unsigned int reg_address = 0;

	strncpy(temp_buf, buf, sizeof(temp_buf));
	temp_buf[sizeof(temp_buf) - 1] = 0;
	pvalue = temp_buf;

	if (size != 0) {
		if (size > 5) {
			ret = kstrtouint(strsep(&pvalue, " "), 16,
					 &reg_address);
			if (ret)
				return ret;
			ret = kstrtouint(pvalue, 16, &reg_value);
			if (ret)
				return ret;
			pr_notice("[%s] write PMU reg 0x%x with value 0x%x !\n",
					__func__, reg_address, reg_value);
			ret = pmic_config_interface(reg_address, reg_value,
						    0xFFFF, 0x0);
		} else {
			ret = kstrtouint(pvalue, 16, &reg_address);
			if (ret)
				return ret;
			ret = pmic_read_interface(reg_address, &g_reg_value,
						  0xFFFF, 0x0);
			pr_notice("[%s] read PMU reg 0x%x with value 0x%x !\n",
					__func__, reg_address, g_reg_value);
			pr_notice("[%s] Please use \"cat pmic_access\" to get value\r\n",
				__func__);
		}
	}
	return size;
}

static DEVICE_ATTR(pmic_access, 0664, show_pmic_access,
		   store_pmic_access); /* 664 */

void PMIC_INIT_SETTING_V1(void)
{
	unsigned int chip_version = 0;
	unsigned int ret = 0;

	/* [0:0]: RG_VCDT_HV_EN; Disable HV. Only compare LV threshold. */
	ret = pmic_config_interface(0x0, 0x0, 0x1, 0);

	/* put init setting from DE/SA */
	chip_version = upmu_get_cid();

	switch (chip_version & 0xFF) {
	case 0x91:
		/* [7:4]: RG_VCDT_HV_VTH; 7V OVP */
		ret = pmic_config_interface(0x2, 0xC, 0xF, 4);
		/* [11:10]: QI_VCORE_VSLEEP; sleep mode only (0.7V) */
		ret = pmic_config_interface(0x210, 0x0, 0x3, 10);
		break;
	case 0x97:
		/* [7:4]: RG_VCDT_HV_VTH; 7V OVP */
		ret = pmic_config_interface(0x2, 0xB, 0xF, 4);
		/* [11:10]: QI_VCORE_VSLEEP; sleep mode only (0.7V) */
		ret = pmic_config_interface(0x210, 0x1, 0x3, 10);
		break;
	default:
		pr_notice("[Power/PMIC] Error chip ID %d\r\n", chip_version);
		break;
	}

	/* [3:1]: RG_VBAT_OV_VTH; VBAT_OV=4.3V */
	ret = pmic_config_interface(0xC, 0x1, 0x7, 1);
	/* [1:1]: RG_BC11_RST; */
	ret = pmic_config_interface(0x24, 0x1, 0x1, 1);
	/* [6:4]: RG_CSDAC_STP; align to 6250's setting */
	ret = pmic_config_interface(0x2A, 0x0, 0x7, 4);
	/* [7:7]: RG_ULC_DET_EN; */
	ret = pmic_config_interface(0x2E, 0x1, 0x1, 7);
	/* [6:6]: RG_HWCV_EN; */
	ret = pmic_config_interface(0x2E, 0x1, 0x1, 6);
	/* [2:2]: RG_CSDAC_MODE; */
	ret = pmic_config_interface(0x2E, 0x1, 0x1, 2);
	/* [3:3]: RG_PWMOC_CK_PDN; For OC protection */
	ret = pmic_config_interface(0x102, 0x0, 0x1, 3);
	/* [4:4]: VIO18_DEG_EN; */
	ret = pmic_config_interface(0x130, 0x1, 0x1, 4);
	/* [2:2]: VCORE_DEG_EN; */
	ret = pmic_config_interface(0x130, 0x1, 0x1, 2);
	/* [0:0]: VCA15_DEG_EN; */
	ret = pmic_config_interface(0x130, 0x1, 0x1, 0);
	/* [12:0]: BUCK_RSV; for OC protection */
	ret = pmic_config_interface(0x206, 0x600, 0x0FFF, 0);
	/* [13:12]: RG_VCA15_CSL2; for OC protection */
	ret = pmic_config_interface(0x216, 0x0, 0x3, 12);
	/* [11:10]: RG_VCA15_CSL1; for OC protection */
	ret = pmic_config_interface(0x216, 0x0, 0x3, 10);
	/* [15:15]: VCA15_SFCHG_REN; soft change rising enable */
	ret = pmic_config_interface(0x224, 0x1, 0x1, 15);
	/* [14:8]: VCA15_SFCHG_RRATE; soft change rising step=0.5us */
	ret = pmic_config_interface(0x224, 0x5, 0x7F, 8);
	/* [7:7]: VCA15_SFCHG_FEN; soft change falling enable */
	ret = pmic_config_interface(0x224, 0x1, 0x1, 7);
	/* [6:0]: VCA15_SFCHG_FRATE; soft change falling step=2us */
	ret = pmic_config_interface(0x224, 0x17, 0x7F, 0);
	/* [5:4]: VCA15_VOSEL_TRANS_EN; rising & falling enable */
	ret = pmic_config_interface(0x238, 0x3, 0x3, 4);
	/* [15:15]: VCORE_SFCHG_REN; */
	ret = pmic_config_interface(0x276, 0x1, 0x1, 15);
	/* [14:8]: VCORE_SFCHG_RRATE; */
	ret = pmic_config_interface(0x276, 0x5, 0x7F, 8);
	/* [6:0]: VCORE_SFCHG_FRATE; */
	ret = pmic_config_interface(0x276, 0x17, 0x7F, 0);
	/* [5:4]: VCORE_VOSEL_TRANS_EN; Follows MT6320 VCORE setting. */
	ret = pmic_config_interface(0x28A, 0x0, 0x3, 4);
	/* [1:0]: VCORE_TRANSTD; */
	ret = pmic_config_interface(0x28A, 0x3, 0x3, 0);
	/* [2:2]: VIBR_THER_SHEN_EN; */
	ret = pmic_config_interface(0x440, 0x1, 0x1, 2);
	/* [5:5]: THR_HWPDN_EN; */
	ret = pmic_config_interface(0x500, 0x1, 0x1, 5);
	/* [3:3]: RG_RST_DRVSEL; */
	ret = pmic_config_interface(0x502, 0x1, 0x1, 3);
	/* [2:2]: RG_EN_DRVSEL; */
	ret = pmic_config_interface(0x502, 0x1, 0x1, 2);
	/* [1:1]: PWRBB_DEB_EN; */
	ret = pmic_config_interface(0x508, 0x1, 0x1, 1);
	/* [11:11]: VPCA15_PG_H2L_EN; */
	ret = pmic_config_interface(0x50C, 0x1, 0x1, 11);
	/* [10:10]: VCORE_PG_H2L_EN; */
	ret = pmic_config_interface(0x50C, 0x1, 0x1, 10);
	/* [1:1]: STRUP_PWROFF_PREOFF_EN; */
	ret = pmic_config_interface(0x512, 0x1, 0x1, 1);
	/* [0:0]: STRUP_PWROFF_SEQ_EN; */
	ret = pmic_config_interface(0x512, 0x1, 0x1, 0);
	/* [15:8]: RG_ADC_TRIM_CH_SEL; */
	ret = pmic_config_interface(0x55E, 0xFC, 0xFF, 8);
	/* [1:1]: FLASH_THER_SHDN_EN; */
	ret = pmic_config_interface(0x560, 0x1, 0x1, 1);
	/* [1:1]: KPLED_THER_SHDN_EN; */
	ret = pmic_config_interface(0x566, 0x1, 0x1, 1);
	/* [9:9]: SPK_THER_SHDN_L_EN; */
	ret = pmic_config_interface(0x600, 0x1, 0x1, 9);
	/* [0:0]: RG_SPK_INTG_RST_L; */
	ret = pmic_config_interface(0x604, 0x1, 0x1, 0);
	/* [9:9]: SPK_THER_SHDN_R_EN; */
	ret = pmic_config_interface(0x606, 0x1, 0x1, 9);
	/* [14:11]: RG_SPKPGA_GAINR; */
	ret = pmic_config_interface(0x60A, 0x1, 0xF, 11);
	/* [11:8]: RG_SPKPGA_GAINL; */
	ret = pmic_config_interface(0x612, 0x1, 0xF, 8);
	/* [8:8]: FG_SLP_EN; */
	ret = pmic_config_interface(0x632, 0x1, 0x1, 8);
	/* [15:0]: FG_SLP_CUR_TH; */
	ret = pmic_config_interface(0x638, 0xFFC2, 0xFFFF, 0);
	/* [7:0]: FG_SLP_TIME; */
	ret = pmic_config_interface(0x63A, 0x14, 0xFF, 0);
	/* [15:8]: FG_DET_TIME; */
	ret = pmic_config_interface(0x63C, 0xFF, 0xFF, 8);
	/* [7:7]: RG_LCLDO_ENC_REMOTE_SENSE_VA28; */
	ret = pmic_config_interface(0x714, 0x1, 0x1, 7);
	/* [4:4]: RG_LCLDO_REMOTE_SENSE_VA33; */
	ret = pmic_config_interface(0x714, 0x1, 0x1, 4);
	/* [1:1]: RG_HCLDO_REMOTE_SENSE_VA33; */
	ret = pmic_config_interface(0x714, 0x1, 0x1, 1);
	/* [15:15]: RG_NCP_REMOTE_SENSE_VA18; */
	ret = pmic_config_interface(0x71A, 0x1, 0x1, 15);
	/* [3:2]: VCA15 OC; */
	ret = pmic_config_interface(0x134, 0x3, 0x3, 2);
	/* [6]: RG_VMCH_STB_SEL; */
	ret = pmic_config_interface(0x432, 0x0, 0x1, 6);
	/* [15]: RG_STB_SEL; */
	ret = pmic_config_interface(0x44E, 0x0, 0x1, 15);
	/* [2]: RG_VMCH_OCFB; */
	ret = pmic_config_interface(0x432, 0x1, 0x1, 2);

#if defined(CONFIG_MACH_MT8518)
	/*auxadc enable control setting for different version chip*/
	if (mt_get_chip_hw_ver() == 0xCA00)
		/*for E1 IC need to set auxadc detection controlled by SW mode*/
		ret = pmic_config_interface(0x510, 0xf, 0xf, 3);
#endif
}


void PMIC_CUSTOM_SETTING_V1(void)
{
	/* disable RF1 26MHz clock */
	pmic_config_interface(0x83a, 0x1, 0x1, 13);
	/* clock off for internal 32K */
	pmic_config_interface(0x800, 0x0, 0x1, 8);
	/* clock off for external 32K */
	pmic_config_interface(0x824, 0x0, 0x1, 13);

	/* disable RF2 26MHz clock */
	pmic_config_interface(0x83a, 0x1, 0x1, 14);
	/* clock off for internal 32K */
	pmic_config_interface(0x800, 0x0, 0x1, 10);
	/* clock off for external 32K */
	pmic_config_interface(0x824, 0x0, 0x1, 15);
}

#if defined(CONFIG_MACH_MT8518)
void pmic_suspend_mode_init_E1(void)
{
	pmic_config_interface(0x228, 0x0, 0x7F, 0);  //VCA15_VOSEL_ON 0.7V
	pmic_config_interface(0x27A, 0x0, 0x7F, 0);  //VCORE_VOSEL_ON 0.7V
	pmic_config_interface(0x27C, 0x30, 0x7F, 0);  //VCORE_VOSEL_SLEEP 1.0V
	pmic_config_interface(0x128, 0x1, 0x1, 9);   //RG_SRCVOLT_HW_AUTO_EN
	pmic_config_interface(0x21E, 0x1, 0x1, 1);//VCA15_VOSEL_CTRL HW control
	pmic_config_interface(0x270, 0x1, 0x1, 1);//VCORE_VOSEL_CTRL HW control
}

void pmic_suspend_mode_init_E2(void)
{
	pmic_config_interface(0x22A, 0x0, 0x7F, 0);//VCA15_VOSEL_SLEEP 0.7V
	pmic_config_interface(0x27C, 0x0, 0x7F, 0);//VCORE_VOSEL_SLEEP 0.7V
	pmic_config_interface(0x210, 0x1, 0x3, 0);//QI_VCA15_VSLEEP 0.7V
	/*set sleep mode reference voltage from R2R to V2V */
	pmic_config_interface(0x238, 0x1, 0x1, 8);//VCA15_VSLEEP_EN
	/*Sleep mode HW control  R2R to VtoV */
	pmic_config_interface(0x28A, 0x1, 0x1, 8);//VCORE_VSLEEP_EN
	pmic_config_interface(0x128, 0x1, 0x1, 9);//RG_SRCVOLT_HW_AUTO_EN
	/* [1:0]: VCA15_VOSEL_CTRL, VCA15_EN_CTRL HW control; */
	pmic_config_interface(0x21E, 0x3, 0x3, 0);
	pmic_config_interface(0x270, 0x1, 0x1, 1);//VCORE_VOSEL_CTRL HW control
}
#endif

static irqreturn_t thr_h_int_handler(int irq, void *dev_id)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pr_notice("%s!\n", __func__);

	/* Read PMIC test register to get VMCH PG status */
	pmic_config_interface(0x13A, 0x0101, 0xFFFF, 0);
	ret = pmic_read_interface(0x150, &val, 0x1, 14);
	if (val == 0) {
		/* VMCH is not good */
		pmic_config_interface(0x416, 0x0, 0x1, 14);
		pr_notice("%s: VMCH not good with status: 0x%x, turn off!\n",
		       __func__, upmu_get_reg_value(0x150));
	}

	return IRQ_HANDLED;
}

static irqreturn_t thr_l_int_handler(int irq, void *dev_id)
{
	pr_notice("%s!\n", __func__);

	return IRQ_HANDLED;
}

static int mt6397_pmic_probe(struct platform_device *dev)
{
	struct resource *res;
	int ret_val = 0;
	int irq_thr_l, irq_thr_h;
	struct mt6397_chip *mt6397_chip = dev_get_drvdata(dev->dev.parent);

	pr_debug("[Power/PMIC] ******** MT6397 pmic driver probe!! ********\n");

	pwrap_regmap = mt6397_chip->regmap;

	/* get PMIC CID */
	ret_val = upmu_get_cid();
	pr_notice("Power/PMIC MT6397 PMIC CID=0x%x\n", ret_val);

	/* pmic initial setting */
	PMIC_INIT_SETTING_V1();
	pr_debug("[Power/PMIC][PMIC_INIT_SETTING_V1] Done\n");

	PMIC_CUSTOM_SETTING_V1();
	pr_debug("[Power/PMIC][PMIC_CUSTOM_SETTING_V1] Done\n");

#if defined(CONFIG_MACH_MT8518)
	/*low power setting for different version chip*/
	if (mt_get_chip_hw_ver() == 0xCA00)
		pmic_suspend_mode_init_E1();
	else
		pmic_suspend_mode_init_E2();
#endif

	res = platform_get_resource(dev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_info(&dev->dev, "no IRQ resource\n");
		return -ENODEV;
	}

	irq_thr_l = irq_create_mapping(mt6397_chip->irq_domain, res->start);
	if (irq_thr_l <= 0)
		return -EINVAL;

	ret_val = request_threaded_irq(irq_thr_l, NULL, thr_l_int_handler,
				       IRQF_ONESHOT | IRQF_TRIGGER_HIGH,
				       "mt6397-thr_l", &dev->dev);
	if (ret_val) {
		dev_info(&dev->dev,
			 "Failed to request mt6397-thr_l IRQ: %d: %d\n",
			 irq_thr_l, ret_val);
	}

	irq_thr_h = irq_create_mapping(mt6397_chip->irq_domain, res->end);
	if (irq_thr_h <= 0)
		return -EINVAL;

	ret_val = request_threaded_irq(irq_thr_h, NULL, thr_h_int_handler,
				       IRQF_ONESHOT | IRQF_TRIGGER_HIGH,
				       "mt6397-thr_h", &dev->dev);
	if (ret_val) {
		dev_info(&dev->dev,
			 "Failed to request mt6397-thr_h IRQ: %d: %d\n",
			 irq_thr_h, ret_val);
	}

	device_create_file(&(dev->dev), &dev_attr_pmic_access);
	return 0;
}

static const struct platform_device_id mt6397_pmic_ids[] = {
	{"mt6397-pmic", 0}, {/* sentinel */},
};
MODULE_DEVICE_TABLE(platform, mt6397_pmic_ids);

static const struct of_device_id mt6397_pmic_of_match[] = {
	{
		.compatible = "mediatek,mt6397-pmic",
	},
	{/* sentinel */},
};
MODULE_DEVICE_TABLE(of, mt6397_pmic_of_match);

static struct platform_driver mt6397_pmic_driver = {
	.driver = {

			.name = "mt6397-pmic",
			.of_match_table = of_match_ptr(mt6397_pmic_of_match),
		},
	.probe = mt6397_pmic_probe,
	.id_table = mt6397_pmic_ids,
};

module_platform_driver(mt6397_pmic_driver);

MODULE_AUTHOR("Chen Zhong <chen.zhong@mediatek.com>");
MODULE_DESCRIPTION("PMIC Misc Setting Driver for MediaTek MT6397 PMIC");
MODULE_LICENSE("GPL v2");
