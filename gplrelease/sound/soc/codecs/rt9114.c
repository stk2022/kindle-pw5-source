/*
 * Richtek RT9114 Sound AMP Driver
 *
 * Copyright (C) 2018, Richtek Technology Corp.
 * Author: CY Hunag <cy_huang@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <sound/soc-dai.h>
#include <sound/tlv.h>

#include "rt9114.h"

static inline int rt9114_chip_power_on(struct rt9114_info *ri)
{
	struct rt9114_platform_data *pdata = dev_get_platdata(ri->dev);

	dev_dbg(ri->dev, "%s ++\n", __func__);
	mutex_lock(&ri->var_lock);
	if (ri->pwr_cnt++ == 0) {
		dev_dbg(ri->dev,
			"do power on gpio [%d]\n", pdata->shutdown_gpio);
		if (pdata->shutdown_gpio >= 0)
			gpio_direction_output(pdata->shutdown_gpio, 1);
		mdelay(1);
	}
	mutex_unlock(&ri->var_lock);
	dev_dbg(ri->dev, "%s --\n", __func__);
	return 0;
}

static inline int rt9114_chip_power_off(struct rt9114_info *ri)
{
	struct rt9114_platform_data *pdata = dev_get_platdata(ri->dev);

	dev_dbg(ri->dev, "%s ++\n", __func__);
	mutex_lock(&ri->var_lock);
	if (--ri->pwr_cnt == 0) {
		dev_dbg(ri->dev,
			"do power off gpio [%d]\n", pdata->shutdown_gpio);
		if (pdata->shutdown_gpio >= 0)
			gpio_direction_output(pdata->shutdown_gpio, 0);
	}
	if (ri->pwr_cnt < 0) {
		dev_warn(ri->dev, "not paired on/off\n");
		ri->pwr_cnt = 0;
	}
	mutex_unlock(&ri->var_lock);
	dev_dbg(ri->dev, "%s --\n", __func__);
	return 0;
}

static unsigned int rt9114_codec_read(struct snd_soc_codec *codec,
				      unsigned int addr)
{
#ifdef CONFIG_RT_REGMAP
	struct rt9114_info *ri = snd_soc_codec_get_drvdata(codec);
	u8 reg_addr = RT9114_GET_ADDR(addr), reg_size = RT9114_GET_SIZE(addr);
	struct rt_reg_data rrd = {0};
	int ret;

	dev_dbg(codec->dev, "%s: addr 0x%08x\n", __func__, reg_addr);
	if (reg_size > 4 || reg_size == 0) {
		dev_err(codec->dev, "not supported codec read size %d\n", addr);
		return -ENOTSUPP;
	}
	ret = rt_regmap_reg_read(ri->regmap, &rrd, reg_addr);
	if (ret < 0)
		dev_err(ri->dev, "reg 0x%02x read fail\n", reg_addr);

	return rrd.rt_data.data_u32;
#else
	struct rt9114_info *ri = snd_soc_codec_get_drvdata(codec);
	u8 reg_addr = RT9114_GET_ADDR(addr), reg_size = RT9114_GET_SIZE(addr);
	u8 data[4] = {0};
	u32 reg_data = 0;
	int i, ret;

	dev_dbg(codec->dev, "%s: addr 0x%08x\n", __func__, reg_addr);
	if (reg_size > 4 || reg_size == 0) {
		dev_err(codec->dev, "not supported codec read size %d\n", addr);
		return -ENOTSUPP;
	}
	ret = i2c_smbus_read_i2c_block_data(ri->i2c, reg_addr, reg_size, data);
	if (ret < 0)
		return ret;
	for (i = 0; i < reg_size; i++) {
		reg_data <<= 8;
		reg_data |= data[i];
	}
	return reg_data;
#endif
}

static int rt9114_codec_write(struct snd_soc_codec *codec,
			      unsigned int addr, unsigned int val)
{
#ifdef CONFIG_RT_REGMAP
	struct rt9114_info *ri = snd_soc_codec_get_drvdata(codec);
	u8 reg_addr = RT9114_GET_ADDR(addr), reg_size = RT9114_GET_SIZE(addr);
	struct rt_reg_data rrd = {0};

	dev_dbg(codec->dev,
		"%s: addr 0x%08x val 0x%08x\n", __func__, reg_addr, val);
	if (reg_size > 4 || reg_size == 0) {
		dev_err(codec->dev,
			"not supported codec write size %d\n", addr);
		return -ENOTSUPP;
	}
	return rt_regmap_reg_write(ri->regmap, &rrd, reg_addr, val);
#else
	struct rt9114_info *ri = snd_soc_codec_get_drvdata(codec);
	u8 reg_addr = RT9114_GET_ADDR(addr), reg_size = RT9114_GET_SIZE(addr);
	u8 data[4] = {0};
	int i;

	dev_dbg(codec->dev,
		"%s: addr 0x%08x val 0x%08x\n", __func__, reg_addr, val);
	if (reg_size > 4 || reg_size == 0) {
		dev_err(codec->dev,
			"not supported codec write size %d\n", addr);
		return -ENOTSUPP;
	}
	for (i = 0; i < reg_size; i++)
		data[reg_size - i - 1] = (val >> (8 * i)) & 0xff;
	return i2c_smbus_write_i2c_block_data(ri->i2c,
					      reg_addr, reg_size, data);
#endif
}

static const u32 err_rpt_regs[] = {
	RT9114_REG_I2S_FMT_RPT,
	RT9114_REG_ERR_RPT1,
	RT9114_REG_AUTO_RCVRY,
	RT9114_REG_ERR_RPT2,
	RT9114_REG_ERR_LATCH,
	RT9114_REG_DC_PROT,
	RT9114_REG_CH1_RMS_RPT,
	RT9114_REG_CH2_RMS_RPT,
};

static const u32 err_wclr_regs[] = {
	RT9114_REG_ERR_RPT1,
	RT9114_REG_ERR_LATCH,
};

static ssize_t rt9114_codec_dev_attr_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct rt9114_info *ri = dev_get_drvdata(dev);
	u8 reg_addr;
	u32 reg_data;
	int i, ret;

	dev_dbg(dev, "%s: ++ attr [%s]", __func__, attr->attr.name);
	ret = mutex_lock_interruptible(&ri->var_lock);
	if (ret < 0)
		return ret;
	if (!ri->pwr_cnt) {
		dev_warn(dev, "amp not turn on, err flag not present\n");
		ret = -EINVAL;
		goto bypass_show_read;
	}
	ret = 0;
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "[=======debug=======\n");
	for (i = 0; i < ARRAY_SIZE(err_rpt_regs) && ret < PAGE_SIZE; i++) {
		reg_addr = RT9114_GET_ADDR(err_rpt_regs[i]);
		reg_data = snd_soc_read(ri->codec, err_rpt_regs[i]);
		ret += scnprintf(buf + ret, PAGE_SIZE - ret,
				 "[0x%02x] -> 0x%08x\n", reg_addr, reg_data);
	}
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "=======debug=======]\n");
	/* clear wr register */
	for (i = 0; i < ARRAY_SIZE(err_wclr_regs); i++)
		snd_soc_write(ri->codec, err_wclr_regs[i], 0);
bypass_show_read:
	mutex_unlock(&ri->var_lock);
	dev_dbg(dev, "%s: -- attr [%s]", __func__, attr->attr.name);
	return ret;
}

static const struct device_attribute rt9114_codec_dev_attrs[] = {
	__ATTR(amp_err_rpt, 0444 /*S_IRUGO*/,
	       rt9114_codec_dev_attr_show, NULL),
};

static int rt9114_codec_probe(struct snd_soc_codec *codec)
{
	struct rt9114_info *ri = snd_soc_codec_get_drvdata(codec);
	int i, ret;

	dev_dbg(codec->dev, "%s ++\n", __func__);
	ret = rt9114_chip_power_on(ri);
	if (ret < 0) {
		dev_info(codec->dev, "%s: power on fail\n", __func__);
		return ret;
	}
	/* sw reset first */
	ret = snd_soc_write(codec, RT9114_REG_SW_RESET, 0x80);
	if (ret < 0) {
		dev_err(codec->dev, "directly sw reset fail\n");
		return ret;
	}
	usleep_range(1000, 1100);
	/* set enable reg to default enable */
	ret = snd_soc_update_bits(codec, RT9114_REG_ENABLE, 0x40, 0);
	if (ret < 0) {
		dev_err(codec->dev, "config enable bit fail\n");
		return 0;
	}
	/* set master volume to 0dB */
	ret = snd_soc_write(codec, RT9114_REG_MS_VOL, 0x180);
	if (ret < 0) {
		dev_err(codec->dev, "config master volume 0dB fail\n");
		return ret;
	}
	/* config intersync */
	ret = snd_soc_update_bits(codec, RT9114_REG_PAD_DRV, 0x04, 0x00);
	if (ret < 0) {
		dev_info(codec->dev, "config intersync fail\n");
		return ret;
	}
	/* default format is I2S */
	ri->fmt_cache = SND_SOC_DAIFMT_I2S;
	ret = rt9114_chip_power_off(ri);
	if (ret < 0) {
		dev_info(codec->dev, "%s: power off fail\n", __func__);
		return ret;
	}
	/* create amp err flag read attribute */
	for (i = 0; i < ARRAY_SIZE(rt9114_codec_dev_attrs); i++) {
		ret = device_create_file(codec->dev,
					 rt9114_codec_dev_attrs + i);
		if (ret < 0) {
			dev_err(codec->dev, "create attr [%d] fail\n", i);
			goto codec_attr_fail;
		}
	}
	/* assign codec variable for future use */
	ri->codec = codec;
	dev_dbg(codec->dev, "%s --\n", __func__);
	return 0;
codec_attr_fail:
	while (--i >= 0)
		device_remove_file(codec->dev, rt9114_codec_dev_attrs + i);
	return ret;
}

static int rt9114_codec_remove(struct snd_soc_codec *codec)
{
	struct rt9114_info *ri = snd_soc_codec_get_drvdata(codec);
	int i;

	dev_dbg(codec->dev, "%s ++\n", __func__);
	ri->codec = NULL;
	for (i = 0; i < ARRAY_SIZE(rt9114_codec_dev_attrs); i++)
		device_remove_file(codec->dev, rt9114_codec_dev_attrs + i);
	dev_dbg(codec->dev, "%s --\n", __func__);
	return 0;
}

static int rt9114_codec_ctrl_get_val(struct snd_kcontrol *kctl,
				     struct snd_ctl_elem_value *uctl)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kctl);
	struct rt9114_info *ri = snd_soc_codec_get_drvdata(codec);
	int ret, val_ret;

	dev_dbg(codec->dev, "%s: ctl[%s] ++\n", __func__, kctl->id.name);
	ret = rt9114_chip_power_on(ri);
	if (ret < 0)
		dev_err(codec->dev, "%s: chip power on fail\n", __func__);
	val_ret = snd_soc_get_volsw(kctl, uctl);
	if (val_ret < 0)
		dev_err(codec->dev, "%s: get volsw fail\n", __func__);
	ret = rt9114_chip_power_off(ri);
	if (ret < 0)
		dev_err(codec->dev, "%s: chip power off fail\n", __func__);
	dev_dbg(codec->dev, "%s: ctl[%s] --\n", __func__, kctl->id.name);
	return val_ret;
}

static int rt9114_codec_ctrl_put_val(struct snd_kcontrol *kctl,
				     struct snd_ctl_elem_value *uctl)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kctl);
	struct rt9114_info *ri = snd_soc_codec_get_drvdata(codec);
	int ret, val_ret;

	dev_dbg(codec->dev, "%s: ctl[%s] ++\n", __func__, kctl->id.name);
	ret = rt9114_chip_power_on(ri);
	if (ret < 0)
		dev_err(codec->dev, "%s: chip power on fail\n", __func__);
	val_ret = snd_soc_put_volsw(kctl, uctl);
	if (val_ret < 0)
		dev_err(codec->dev, "%s: get volsw fail\n", __func__);
	ret = rt9114_chip_power_off(ri);
	if (ret < 0)
		dev_err(codec->dev, "%s: chip power off fail\n", __func__);
	dev_dbg(codec->dev, "%s: ctl[%s] --\n", __func__, kctl->id.name);
	return val_ret;
}

static int rt9114_codec_ctrl_put_enum(struct snd_kcontrol *kctl,
				     struct snd_ctl_elem_value *uctl)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kctl);
	struct rt9114_info *ri = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	ret = rt9114_chip_power_on(ri);
	if (ret < 0)
		goto err;

	ret = snd_soc_put_enum_double(kctl, uctl);
	if (ret < 0)
		goto err;

	ret = rt9114_chip_power_off(ri);
	if (ret < 0)
		goto err;

err:
	return ret;
}

static int rt9114_codec_ctrl_get_enum(struct snd_kcontrol *kctl,
				     struct snd_ctl_elem_value *uctl)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kctl);
	struct rt9114_info *ri = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	ret = rt9114_chip_power_on(ri);
	if (ret < 0)
		goto err;

	ret = snd_soc_get_enum_double(kctl, uctl);
	if (ret < 0)
		goto err;

	ret = rt9114_chip_power_off(ri);
	if (ret < 0)
		goto err;

err:
	return ret;
}

static const DECLARE_TLV_DB_SCALE(dpga_tlv, -10375, 25, 0);

static const char *const rt9114_spk_gain[] = {
	"3x",
	"3.5x",
	"4x",
	"4.5x",
	"5x",
	"5.5x",
	"6.15x",
	"8x",
};

static const struct soc_enum rt9114_codec_enum[] = {
	SOC_ENUM_SINGLE(RT9114_REG_SPK_GAIN, 4,
		ARRAY_SIZE(rt9114_spk_gain), rt9114_spk_gain),
};

static const struct snd_kcontrol_new rt9114_codec_snd_controls[] = {
	SOC_SINGLE_EXT_TLV("Master Volume", RT9114_REG_MS_VOL, 2, 511, 1,
			   rt9114_codec_ctrl_get_val, rt9114_codec_ctrl_put_val,
			   dpga_tlv),
	SOC_DOUBLE_R_EXT_TLV("CH Volume", RT9114_REG_CH1_VOL,
			     RT9114_REG_CH2_VOL, 2, 511, 1,
			     rt9114_codec_ctrl_get_val,
			     rt9114_codec_ctrl_put_val,
			     dpga_tlv),
	SOC_ENUM_EXT("SPK Gain", rt9114_codec_enum[0],
			     rt9114_codec_ctrl_get_enum,
			     rt9114_codec_ctrl_put_enum),
	SOC_SINGLE_EXT("Pre HPF Switch", RT9114_REG_FLTR_MISC, 7, 1, 0,
		       rt9114_codec_ctrl_get_val, rt9114_codec_ctrl_put_val),
	SOC_SINGLE_EXT("Post HPF Switch", RT9114_REG_FLTR_MISC, 6, 1, 0,
		       rt9114_codec_ctrl_get_val, rt9114_codec_ctrl_put_val),
	SOC_SINGLE_EXT("CompFilter Switch", RT9114_REG_FLTR_MISC, 4, 1, 0,
		       rt9114_codec_ctrl_get_val, rt9114_codec_ctrl_put_val),
	SOC_SINGLE_EXT("CH1 SoftMute Switch", RT9114_REG_CH_MUTE, 0, 1, 0,
		       rt9114_codec_ctrl_get_val, rt9114_codec_ctrl_put_val),
	SOC_SINGLE_EXT("CH2 SoftMute Switch", RT9114_REG_CH_MUTE, 1, 1, 0,
		       rt9114_codec_ctrl_get_val, rt9114_codec_ctrl_put_val),
	SOC_SINGLE_EXT("Skip VolRamp Switch", RT9114_REG_VOL_RAMP, 3, 1, 0,
		       rt9114_codec_ctrl_get_val, rt9114_codec_ctrl_put_val),
	SOC_SINGLE_EXT("VolRamp Sel", RT9114_REG_VOL_RAMP, 0, 3, 0,
		       rt9114_codec_ctrl_get_val, rt9114_codec_ctrl_put_val),
	SOC_SINGLE_EXT("CH1 SI Switch", RT9114_REG_SDIN_SEL, 2, 2, 0,
		       rt9114_codec_ctrl_get_val, rt9114_codec_ctrl_put_val),
	SOC_SINGLE_EXT("CH2 SI Switch", RT9114_REG_SDIN_SEL, 0, 2, 0,
		       rt9114_codec_ctrl_get_val, rt9114_codec_ctrl_put_val),
	SOC_SINGLE_EXT("SDO Sel Switch", RT9114_REG_SDO_SEL, 4, 4, 0,
		       rt9114_codec_ctrl_get_val, rt9114_codec_ctrl_put_val),
	SOC_SINGLE_EXT("PBTL Switch", RT9114_REG_PBTL_SS_OPT3, 2, 1, 0,
		       rt9114_codec_ctrl_get_val, rt9114_codec_ctrl_put_val),
};

static const struct snd_soc_dapm_widget rt9114_codec_dapm_widgets[] = {
	/* Digital : DSP Block */
	SND_SOC_DAPM_MIXER("Pre HPF", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("EQ", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("DRC2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("DRC3", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("DRC1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("FDRC", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("DRC4", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("Post HPF", SND_SOC_NOPM, 0, 0, NULL, 0),
	/* Analog : Out Block */
	SND_SOC_DAPM_DAC("Left DAC", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("Right DAC", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_OUT_DRV("Left SPK DRV", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_OUT_DRV("Right SPK DRV", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_OUTPUT("BSTPL"),
	SND_SOC_DAPM_OUTPUT("BSTNL"),
	SND_SOC_DAPM_OUTPUT("BSTPR"),
	SND_SOC_DAPM_OUTPUT("BSTNR"),
};

static const struct snd_soc_dapm_route rt9114_codec_dapm_routes[] = {
	{ "Pre HPF", NULL, "aif1_playback"},
	{ "EQ", NULL, "Pre HPF"},
	{ "DRC2", NULL, "EQ"},
	{ "DRC3", NULL, "EQ"},
	{ "DRC1", NULL, "EQ"},
	{ "FDRC", NULL, "DRC2"},
	{ "FDRC", NULL, "DRC3"},
	{ "FDRC", NULL, "DRC1"},
	{ "DRC4", NULL, "FDRC"},
	{ "Post HPF", NULL, "DRC4"},
	{ "Left DAC", NULL, "Post HPF"},
	{ "Right DAC", NULL, "Post HPF"},
	{ "Left SPK DRV", NULL, "Left DAC"},
	{ "Right SPK DRV", NULL, "Right DAC"},
	{ "BSTPL", NULL, "Left SPK DRV"},
	{ "BSTNL", NULL, "Left SPK DRV"},
	{ "BSTPR", NULL, "Right SPK DRV"},
	{ "BSTNR", NULL, "Right SPK DRV"},
};

static const struct snd_soc_codec_driver rt9114_codec_driver = {
	.probe = rt9114_codec_probe,
	.remove = rt9114_codec_remove,
	.read = rt9114_codec_read,
	.write = rt9114_codec_write,

#if 1 //(LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0))
	.component_driver =  {
		.controls = rt9114_codec_snd_controls,
		.num_controls = ARRAY_SIZE(rt9114_codec_snd_controls),
		.dapm_widgets = rt9114_codec_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(rt9114_codec_dapm_widgets),
		.dapm_routes = rt9114_codec_dapm_routes,
		.num_dapm_routes = ARRAY_SIZE(rt9114_codec_dapm_routes),
	},
#else
	.controls = rt9114_codec_snd_controls,
	.num_controls = ARRAY_SIZE(rt9114_codec_snd_controls),
	.dapm_widgets = rt9114_codec_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(rt9114_codec_dapm_widgets),
	.dapm_routes = rt9114_codec_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(rt9114_codec_dapm_routes),
#endif /* #if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0)) */

	.idle_bias_off = true,
};

static int rt9114_codec_dai_startup(struct snd_pcm_substream *substream,
				    struct snd_soc_dai *dai)
{
	dev_dbg(dai->dev, "%s\n", __func__);
	return 0;
}

static void rt9114_codec_dai_shutdown(struct snd_pcm_substream *substream,
				     struct snd_soc_dai *dai)
{
	dev_dbg(dai->dev, "%s\n", __func__);
}

static int rt9114_codec_dai_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct rt9114_info *ri = snd_soc_codec_get_drvdata(dai->codec);

	dev_dbg(dai->dev, "%s: fmt 0x%08x\n", __func__, fmt);
	mutex_lock(&ri->var_lock);
	ri->fmt_cache = fmt;
	mutex_unlock(&ri->var_lock);
	return 0;
}

static int rt9114_codec_dai_hw_params(struct snd_pcm_substream *substream,
		      struct snd_pcm_hw_params *param, struct snd_soc_dai *dai)
{
	struct rt9114_info *ri = snd_soc_codec_get_drvdata(dai->codec);
	int aud_bits = params_width(param), aud_fmt;
	u8 reg_data = 0;
	int ret = 0;

	dev_dbg(dai->dev, "%s ++\n", __func__);
	mutex_lock(&ri->var_lock);
	aud_fmt = ri->fmt_cache;
	mutex_unlock(&ri->var_lock);
	dev_dbg(dai->dev, "%s: fmt %d, bits %d\n", __func__, aud_fmt, aud_bits);
	switch (aud_fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		reg_data = 3;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		reg_data = 0;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		reg_data = 6;
		break;
	default:
		return -ENOTSUPP;
	}
	switch (aud_bits) {
	case 16:
		reg_data += 0;
		break;
	case 20:
		reg_data += 1;
		break;
	case 24:
	case 32:
		reg_data += 2;
		break;
	}
	ret = rt9114_chip_power_on(ri);
	if (ret < 0) {
		dev_info(dai->dev, "%s: power on fail\n", __func__);
		return ret;
	}
	ret = snd_soc_update_bits(dai->codec,
				  RT9114_REG_I2S_FMT, 0x0f, reg_data);
	if (ret < 0)
		dev_err(dai->dev, "config format fail\n");
	ret = rt9114_chip_power_off(ri);
	if (ret < 0) {
		dev_info(dai->dev, "%s: power off fail\n", __func__);
		return ret;
	}
	dev_dbg(dai->dev, "%s --\n", __func__);
	return 0;
}

static int rt9114_codec_dai_hw_free(struct snd_pcm_substream *substream,
				    struct snd_soc_dai *dai)
{
	dev_dbg(dai->dev, "%s\n", __func__);
	return 0;
}

static int rt9114_codec_dai_prepare(struct snd_pcm_substream *substream,
				    struct snd_soc_dai *dai)
{
	dev_dbg(dai->dev, "%s\n", __func__);
	return 0;
}

static int rt9114_codec_dai_trigger(struct snd_pcm_substream *substream,
				    int cmd, struct snd_soc_dai *dai)
{
	struct rt9114_info *ri = snd_soc_codec_get_drvdata(dai->codec);
	int capture = (substream->stream == SNDRV_PCM_STREAM_CAPTURE), ret;

	dev_dbg(dai->dev, "%s: cmd = %d\n", __func__, cmd);
	dev_dbg(dai->dev, "%s: %c\n", __func__, capture ? 'c' : 'p');
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		ret = rt9114_chip_power_on(ri);
		if (ret < 0) {
			dev_info(dai->dev, "%s: power on fail\n", __func__);
			return ret;
		}
		/* for rampping up */
		mdelay(80);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		ret = rt9114_chip_power_off(ri);
		if (ret < 0) {
			dev_info(dai->dev, "%s: power off fail\n", __func__);
			return ret;
		}
		/* for rampping down */
		mdelay(80);
		break;
	default:
		break;
	}
	return 0;
}

static const struct snd_soc_dai_ops rt9114_codec_dai_ops = {
	.startup = rt9114_codec_dai_startup,
	.shutdown = rt9114_codec_dai_shutdown,
	.set_fmt = rt9114_codec_dai_set_fmt,
	.hw_params = rt9114_codec_dai_hw_params,
	.hw_free = rt9114_codec_dai_hw_free,
	.prepare = rt9114_codec_dai_prepare,
	.trigger = rt9114_codec_dai_trigger,
};

#define RT9114_SUPPORT_RATES		SNDRV_PCM_RATE_8000_96000
#define RT9114_SUPPORT_FORMATS		(SNDRV_PCM_FMTBIT_S16 |\
					 SNDRV_PCM_FMTBIT_S24 |\
					 SNDRV_PCM_FMTBIT_S32)

static struct snd_soc_dai_driver rt9114_codec_dais[] = {
	{
		.name = "rt9114-aif1",
		.playback = {
			.stream_name = "aif1_playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT9114_SUPPORT_RATES,
			.formats = RT9114_SUPPORT_FORMATS,
		},
		.capture = {
			.stream_name = "aif1_capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT9114_SUPPORT_RATES,
			.formats = RT9114_SUPPORT_FORMATS,
		},
		.ops = &rt9114_codec_dai_ops,

		.symmetric_rates = 1,
		.symmetric_channels = 1,
		.symmetric_samplebits = 1,
	},
};

static inline int rt9114_chip_id_check(struct rt9114_info *ri)
{
	u8 addr = RT9114_GET_ADDR(RT9114_REG_DEV_ID);
	int ret = 0;

	dev_dbg(ri->dev, "%s ++\n", __func__);
	ret = i2c_smbus_read_byte_data(ri->i2c, addr);
	if (ret < 0)
		return ret;
	if (ret != 0x10)
		return -ENODEV;
	dev_dbg(ri->dev, "%s --\n", __func__);
	return 0;
}

static int rt9114_parse_dt_data(struct device *dev,
				struct rt9114_platform_data *pdata)
{
	pdata->shutdown_gpio = of_get_named_gpio(dev->of_node,
						 "shutdown_gpio", 0);
	if (pdata->shutdown_gpio < 0) {
		dev_warn(dev, "no property or get_gpio fail\n");
		dev_warn(dev, "treat it as default enable\n");
	}
	return 0;
}

static int rt9114_i2c_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	struct rt9114_platform_data *pdata = dev_get_platdata(&client->dev);
	struct rt9114_info *ri;
	bool use_dt = client->dev.of_node;
	int ret = 0;

	dev_dbg(&client->dev, "%s\n", __func__);
	if (use_dt) {
		pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata)
			return -ENOMEM;
		ret = rt9114_parse_dt_data(&client->dev, pdata);
		if (ret < 0) {
			dev_err(&client->dev, "parse dt fail\n");
			return ret;
		}
		client->dev.platform_data = pdata;
	}
	if (!pdata) {
		dev_err(&client->dev, "no platform data specified\n");
		return -EINVAL;
	}
	ri = devm_kzalloc(&client->dev, sizeof(*ri), GFP_KERNEL);
	if (!ri)
		return -ENOMEM;
	ri->i2c = client;
	ri->dev = &client->dev;
	mutex_init(&ri->var_lock);
	i2c_set_clientdata(client, ri);

	/* special config for i2c or chip id check */
	ret = rt9114_chip_power_on(ri);
	if (ret < 0) {
		dev_info(&client->dev, "%s: power on fail\n", __func__);
		goto err_codec;
	}
	ret = rt9114_chip_id_check(ri);
	if (ret < 0) {
		dev_err(&client->dev, "chip id check fail\n");
		goto err_codec;
	}
	ret = rt9114_chip_power_off(ri);
	if (ret < 0) {
		dev_info(&client->dev, "%s: power off fail\n", __func__);
		goto err_codec;
	}
	/* Richtek regmap register */
	ret = rt9114_regmap_register(ri);
	if (ret < 0) {
		dev_err(&client->dev, "regmap register fail\n");
		goto err_codec;
	}
	/* register codec */
	ret = snd_soc_register_codec(&client->dev,
				     &rt9114_codec_driver, rt9114_codec_dais,
				     ARRAY_SIZE(rt9114_codec_dais));
	if (ret < 0) {
		dev_err(&client->dev, "register codec fail\n");
		goto err_codec;
	}
	dev_info(&client->dev, "%s: successfully probed\n", __func__);
	return 0;
err_codec:
	mutex_destroy(&ri->var_lock);
	return ret;
}

static int rt9114_i2c_remove(struct i2c_client *client)
{
	struct rt9114_info *ri = i2c_get_clientdata(client);

	dev_dbg(ri->dev, "%s\n", __func__);
	snd_soc_unregister_codec(ri->dev);
	rt9114_regmap_unregister(ri);
	mutex_destroy(&ri->var_lock);
	return 0;
}

static int __maybe_unused rt9114_i2c_suspend(struct device *dev)
{
	dev_dbg(dev, "%s\n", __func__);
	return 0;
}

static int __maybe_unused rt9114_i2c_resume(struct device *dev)
{
	dev_dbg(dev, "%s\n", __func__);
	return 0;
}

static SIMPLE_DEV_PM_OPS(rt9114_pm_ops,
			 rt9114_i2c_suspend, rt9114_i2c_resume);

static const struct of_device_id __maybe_unused rt9114_of_id[] = {
	{ .compatible = "richtek,rt9114",},
	{},
};
MODULE_DEVICE_TABLE(of, rt9114_of_id);

static const struct i2c_device_id rt9114_i2c_id[] = {
	{ "rt9114", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, rt9114_i2c_id);

static struct i2c_driver rt9114_i2c_driver = {
	.driver = {
		.name = "rt9114",
		.owner = THIS_MODULE,
		.pm = &rt9114_pm_ops,
		.of_match_table = of_match_ptr(rt9114_of_id),
	},
	.probe = rt9114_i2c_probe,
	.remove = rt9114_i2c_remove,
	.id_table = rt9114_i2c_id,
};
module_i2c_driver(rt9114_i2c_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Richtek RT9114 Sound AMP Driver");
MODULE_AUTHOR("CY Huang <cy_huang@richtek.com>");
MODULE_VERSION("1.0.0");
