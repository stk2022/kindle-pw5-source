/*
 * mt8512-evb.c  --  MT8512 machine driver
 *
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Mengge Wang <mengge.wang@mediatek.com>
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

#include <linux/module.h>
#include <linux/of_gpio.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include "mt8512-afe-common.h"
#ifdef CONFIG_MTK_HIFI4DSP_SUPPORT
#include "mt8512-adsp-utils.h"
#endif
#include "../../codecs/tlv320wn.h"

#define TEST_BACKEND_WITH_ENDPOINT

#define PREFIX	"mediatek,"
#define ENUM_TO_STR(enum) #enum

enum PINCTRL_PIN_STATE {
	PIN_STATE_DEFAULT = 0,
	PIN_STATE_EXT_SPK_AMP_ON,
	PIN_STATE_EXT_SPK_AMP_OFF,
	PIN_STATE_EXT_LO_AMP_ON,
	PIN_STATE_EXT_LO_AMP_OFF,
	PIN_STATE_MAX
};

static const char * const mt8512_evb_pin_str[PIN_STATE_MAX] = {
	"default",
	"ext_spk_amp_on",
	"ext_spk_amp_off",
	"ext_lineout_amp_on",
	"ext_lineout_amp_off",
};

struct mt8512_evb_etdm_ctrl_data {
	unsigned int mck_multp_in;
	unsigned int mck_multp_out;
	unsigned int lrck_width_in;
	unsigned int lrck_width_out;
	unsigned int fix_rate_in;
	unsigned int fix_rate_out;
	unsigned int fix_bit_width_in;
	unsigned int fix_bit_width_out;
	unsigned int fix_channels_in;
	unsigned int fix_channels_out;
	unsigned int tlv320_clock_mux;
};

struct mt8512_evb_dmic_ctrl_data {
	unsigned int fix_rate;
	unsigned int fix_channels;
	unsigned int fix_bit_width;
};

struct mt8512_evb_multi_in_ctrl_data {
	unsigned int fix_bit_width;
};

struct mt8512_evb_int_adda_ctrl_data {
	unsigned int fix_rate;
};

#ifdef CONFIG_SND_SOC_GAPP_AUDIO_CONTROL
enum mtkfile_pcm_state {
	MTKFILE_PCM_STATE_UNKNOWN = 0,
	MTKFILE_PCM_STATE_OPEN,
	MTKFILE_PCM_STATE_HW_PARAMS,
	MTKFILE_PCM_STATE_PREPARE,
	MTKFILE_PCM_STATE_START,
	MTKFILE_PCM_STATE_PAUSE,
	MTKFILE_PCM_STATE_RESUME,
	MTKFILE_PCM_STATE_DRAIN,
	MTKFILE_PCM_STATE_STOP,
	MTKFILE_PCM_STATE_HW_FREE,
	MTKFILE_PCM_STATE_CLOSE,
	MTKFILE_PCM_STATE_NUM,
};

static const char *const pcm_state_func[] = {
	ENUM_TO_STR(MTKFILE_PCM_STATE_UNKNOWN),
	ENUM_TO_STR(MTKFILE_PCM_STATE_OPEN),
	ENUM_TO_STR(MTKFILE_PCM_STATE_HW_PARAMS),
	ENUM_TO_STR(MTKFILE_PCM_STATE_PREPARE),
	ENUM_TO_STR(MTKFILE_PCM_STATE_START),
	ENUM_TO_STR(MTKFILE_PCM_STATE_PAUSE),
	ENUM_TO_STR(MTKFILE_PCM_STATE_RESUME),
	ENUM_TO_STR(MTKFILE_PCM_STATE_DRAIN),
	ENUM_TO_STR(MTKFILE_PCM_STATE_STOP),
	ENUM_TO_STR(MTKFILE_PCM_STATE_HW_FREE),
	ENUM_TO_STR(MTKFILE_PCM_STATE_CLOSE),
};

static SOC_ENUM_SINGLE_EXT_DECL(pcm_state_enums, pcm_state_func);

enum {
	MASTER_VOLUME_ID = 0,
	MASTER_VOLUMEX_ID,
	MASTER_SWITCH_ID,
	MASTER_SWITCHX_ID,
	PCM_STATE_ID,
	PCM_STATEX_ID,
	CTRL_NOTIFY_NUM,
	CTRL_NOTIFY_INVAL = 0xFFFF,
};

static const char *nfy_ctl_names[CTRL_NOTIFY_NUM] = {
	"Master Volume 1",
	"Master Volume X",
	"Master Switch",
	"Master Switch X",
	"PCM State",
	"PCM State X",
};

struct soc_ctlx_res {
	int master_volume;
	int master_switch;
	int pcm_state;
	struct snd_ctl_elem_id nfy_ids[CTRL_NOTIFY_NUM];
	struct mutex res_mutex;
	spinlock_t res_lock;
};
#endif

struct mt8512_evb_priv {
	struct pinctrl *pinctrl;
	struct pinctrl_state *pin_states[PIN_STATE_MAX];
	struct mt8512_evb_etdm_ctrl_data etdm_data[MT8512_ETDM_SETS];
	struct mt8512_evb_dmic_ctrl_data dmic_data;
	struct mt8512_evb_multi_in_ctrl_data multi_in_data;
	struct mt8512_evb_int_adda_ctrl_data int_adda_data;
};

struct mt8512_dai_link_prop {
	char *name;
	unsigned int link_id;
};

static int mt8512_evb_ext_spk_amp_wevent(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;
	struct mt8512_evb_priv *card_data = snd_soc_card_get_drvdata(card);
	int ret = 0;

	dev_dbg(card->dev, "%s event %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		if (!IS_ERR(card_data->pin_states[PIN_STATE_EXT_SPK_AMP_ON])) {
			ret = pinctrl_select_state(card_data->pinctrl,
			      card_data->pin_states[PIN_STATE_EXT_SPK_AMP_ON]);
			if (ret)
				dev_info(card->dev,
					"%s failed to select state %d\n",
					__func__, ret);
		} else {
			dev_info(card->dev,
				 "%s invalid pin state %s\n",
				 __func__,
				 mt8512_evb_pin_str[PIN_STATE_EXT_SPK_AMP_ON]);
		}
		break;
	case SND_SOC_DAPM_PRE_PMD:
		if (!IS_ERR(card_data->pin_states[PIN_STATE_EXT_SPK_AMP_OFF])) {
			ret = pinctrl_select_state(card_data->pinctrl,
			      card_data->pin_states[PIN_STATE_EXT_SPK_AMP_OFF]);
			if (ret)
				dev_info(card->dev,
					"%s failed to select state %d\n",
					__func__, ret);
		} else {
			dev_info(card->dev,
				 "%s invalid pin state %s\n",
				 __func__,
				 mt8512_evb_pin_str[PIN_STATE_EXT_SPK_AMP_OFF]);
		}
		break;
	default:
		break;
	}

	return 0;
}

static int mt8512_evb_ext_lineout_amp_wevent(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;
	struct mt8512_evb_priv *card_data = snd_soc_card_get_drvdata(card);
	int ret = 0;

	dev_dbg(card->dev, "%s event %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		if (!IS_ERR(card_data->pin_states[PIN_STATE_EXT_LO_AMP_ON])) {
			ret = pinctrl_select_state(card_data->pinctrl,
			      card_data->pin_states[PIN_STATE_EXT_LO_AMP_ON]);
			if (ret)
				dev_info(card->dev,
					"%s failed to select state %d\n",
					__func__, ret);
		} else {
			dev_info(card->dev,
				 "%s invalid pin state %s\n",
				 __func__,
				 mt8512_evb_pin_str[PIN_STATE_EXT_LO_AMP_ON]);
		}
		break;
	case SND_SOC_DAPM_PRE_PMD:
		if (!IS_ERR(card_data->pin_states[PIN_STATE_EXT_LO_AMP_OFF])) {
			ret = pinctrl_select_state(card_data->pinctrl,
			      card_data->pin_states[PIN_STATE_EXT_LO_AMP_OFF]);
			if (ret)
				dev_info(card->dev,
					"%s failed to select state %d\n",
					__func__, ret);
		} else {
			dev_info(card->dev,
				 "%s invalid pin state %s\n",
				 __func__,
				 mt8512_evb_pin_str[PIN_STATE_EXT_LO_AMP_OFF]);
		}
		break;
	default:
		break;
	}

	return 0;
}

/* Ext SPK Amp Switch */
static const struct snd_kcontrol_new mt8512_codec_ext_spk_amp_switch_ctrl =
	SOC_DAPM_SINGLE_VIRT("Switch", 1);

/* Ext Lineout Amp Switch */
static const struct snd_kcontrol_new mt8512_codec_ext_lineout_amp_switch_ctrl =
	SOC_DAPM_SINGLE_VIRT("Switch", 1);

static const struct snd_soc_dapm_widget mt8512_evb_widgets[] = {
	SND_SOC_DAPM_OUTPUT("HFP Out"),
	SND_SOC_DAPM_INPUT("HFP In"),
	SND_SOC_DAPM_INPUT("DMIC In"),
	SND_SOC_DAPM_SWITCH("Ext Spk Amp", SND_SOC_NOPM, 0, 0,
		&mt8512_codec_ext_spk_amp_switch_ctrl),
	SND_SOC_DAPM_SWITCH("Ext Lineout Amp", SND_SOC_NOPM, 0, 0,
		&mt8512_codec_ext_lineout_amp_switch_ctrl),
	SND_SOC_DAPM_SPK("Spk Amp", mt8512_evb_ext_spk_amp_wevent),
	SND_SOC_DAPM_SPK("Lineout Amp", mt8512_evb_ext_lineout_amp_wevent),
};

static const struct snd_soc_dapm_route mt8512_evb_routes[] = {
	{"HFP Out", NULL, "PCM1 Playback"},
	{"PCM1 Capture", NULL, "HFP In"},

#ifdef CONFIG_SND_SOC_MT8512_CODEC
	{"DIG_DAC_CLK", NULL, "AFE_DAC_CLK"},
	{"DIG_ADC_CLK", NULL, "AFE_ADC_CLK"},

	{"Ext Spk Amp", "Switch", "AU_VOL"},
	{"Ext Spk Amp", "Switch", "AU_VOR"},
	{"Spk Amp", NULL, "Ext Spk Amp"},

	{"Ext Lineout Amp", "Switch", "AU_VOL"},
	{"Ext Lineout Amp", "Switch", "AU_VOR"},
	{"Lineout Amp", NULL, "Ext Lineout Amp"},
#endif
};

enum {
	/* FE */
	DAI_LINK_AFE_FE_BASE = 0,
	DAI_LINK_DLM_PLAYBACK = DAI_LINK_AFE_FE_BASE,
	DAI_LINK_DL2_PLAYBACK,
	DAI_LINK_DL3_PLAYBACK,
	DAI_LINK_DL6_PLAYBACK,
	DAI_LINK_UL2_CAPTURE,
	DAI_LINK_UL3_CAPTURE,
	DAI_LINK_UL4_CAPTURE,
	DAI_LINK_UL5_CAPTURE,
	DAI_LINK_UL8_CAPTURE,
	DAI_LINK_UL9_CAPTURE,
	DAI_LINK_UL10_CAPTURE,
	DAI_LINK_UL1_CAPTURE,
#ifdef CONFIG_SND_SOC_MTK_BTCVSD
	DAI_LINK_BTCVSD,
	DAI_LINK_AFE_FE_END = DAI_LINK_BTCVSD,
#else
	DAI_LINK_AFE_FE_END = DAI_LINK_UL1_CAPTURE,
#endif
#ifdef CONFIG_MTK_HIFIXDSP_SUPPORT
	DAI_LINK_FE_HOSTLESS_VA,
	DAI_LINK_FE_VA,
	DAI_LINK_FE_MIC_RECORD,
#endif
	/* BE */
	DAI_LINK_AFE_BE_BASE,
	DAI_LINK_ETDM1_IN = DAI_LINK_AFE_BE_BASE,
	DAI_LINK_ETDM2_OUT,
	DAI_LINK_ETDM2_IN,
	DAI_LINK_PCM_INTF,
	DAI_LINK_VIRTUAL_DL_SOURCE,
	DAI_LINK_DMIC,
	DAI_LINK_INT_ADDA,
	DAI_LINK_GASRC0,
	DAI_LINK_GASRC1,
	DAI_LINK_GASRC2,
	DAI_LINK_GASRC3,
	DAI_LINK_SPDIF_IN,
	DAI_LINK_MULTI_IN,
	DAI_LINK_AFE_BE_END = DAI_LINK_MULTI_IN,
#ifdef CONFIG_MTK_HIFIXDSP_SUPPORT
	DAI_LINK_BE_TDM_IN,
	DAI_LINK_BE_UL9,
	DAI_LINK_BE_UL2,
#endif
	DAI_LINK_NUM
};

#ifdef CONFIG_MTK_HIFIXDSP_SUPPORT
static const unsigned int adsp_dai_links[] = {
	DAI_LINK_FE_HOSTLESS_VA,
	DAI_LINK_FE_VA,
	DAI_LINK_FE_MIC_RECORD,
	DAI_LINK_BE_TDM_IN,
	DAI_LINK_BE_UL9,
	DAI_LINK_BE_UL2
};

static bool is_adsp_dai_link(unsigned int dai)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(adsp_dai_links); i++)
		if (dai == adsp_dai_links[i])
			return true;

	return false;
}
#endif

static int link_to_dai(int link_id)
{
	switch (link_id) {
	case DAI_LINK_ETDM1_IN:
		return MT8512_AFE_IO_ETDM1_IN;
	case DAI_LINK_ETDM2_OUT:
		return MT8512_AFE_IO_ETDM2_OUT;
	case DAI_LINK_ETDM2_IN:
		return MT8512_AFE_IO_ETDM2_IN;
	case DAI_LINK_PCM_INTF:
		return MT8512_AFE_IO_PCM1;
	case DAI_LINK_DMIC:
		return MT8512_AFE_IO_DMIC;
	case DAI_LINK_INT_ADDA:
		return MT8512_AFE_IO_INT_ADDA;
	default:
		break;
	}
	return -1;
}
/* tlv320_clock_source:*/
/*    TLV320_PLL_ADC_FS_CLKIN_MCLK = 1, */
/*    TLV320_PLL_ADC_FS_CLKIN_BCLK = 2, */
/*   TLV320_PLL_ADC_FS_CLKIN_PLL_MCLK = 3, */
/*   TLV320_PLL_ADC_FS_CLKIN_PLL_BCLK = 4, */
/*    TLV320_ADC_FS_CLKIN_BCLK = 5, */

static int mt8512_evb_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt8512_evb_priv *priv = snd_soc_card_get_drvdata(rtd->card);
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int id = rtd->dai_link->id;
	struct mt8512_evb_etdm_ctrl_data *etdm;
	struct snd_soc_dai *codec_dai;

	unsigned int mclk_multiplier = 0;
	unsigned int mclk = 0;
	unsigned int lrck_width = 0;
	int slot = 0;
	int slot_width = 0;
	unsigned int slot_bitmask = 0;
	unsigned int tlv320_clock_source = 0;
	unsigned int idx;
	int ret;

	if (id == DAI_LINK_ETDM1_IN) {
		idx = MT8512_ETDM1;
		etdm = &priv->etdm_data[idx];
		mclk_multiplier = etdm->mck_multp_in;
		lrck_width = etdm->lrck_width_in;
		tlv320_clock_source = etdm->tlv320_clock_mux;
	} else if (id == DAI_LINK_ETDM2_OUT) {
		idx = MT8512_ETDM2;
		etdm = &priv->etdm_data[idx];
		mclk_multiplier = etdm->mck_multp_out;
		lrck_width = etdm->lrck_width_out;
	} else if (id == DAI_LINK_ETDM2_IN) {
		idx = MT8512_ETDM2;
		etdm = &priv->etdm_data[idx];
		mclk_multiplier = etdm->mck_multp_in;
		lrck_width = etdm->lrck_width_in;
	}

	if (mclk_multiplier > 0) {
		mclk = mclk_multiplier * params_rate(params);

		ret = snd_soc_dai_set_sysclk(cpu_dai, 0, mclk,
					     SND_SOC_CLOCK_OUT);
		if (ret)
			return ret;
	}

	slot_width = lrck_width;
	if (slot_width > 0) {
		slot = params_channels(params);
		slot_bitmask = GENMASK(slot - 1, 0);

		ret = snd_soc_dai_set_tdm_slot(cpu_dai,
					       slot_bitmask,
					       slot_bitmask,
					       slot,
					       slot_width);
		if (ret)
			return ret;
	}

	if (tlv320_clock_source) {

		codec_dai = rtd->codec_dai;

		if (codec_dai == NULL) {
			dev_info(rtd->dev, "tlv320: invalid dai parameter\n");
			return -EINVAL;
		}

		snd_soc_dai_set_pll(codec_dai, 0, tlv320_clock_source,
			snd_pcm_format_width(params_format(params))
			* params_channels(params) * params_rate(params)
			, params_rate(params));

		snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_CBS_CFS |
				SND_SOC_DAIFMT_DSP_B | SND_SOC_DAIFMT_NB_NF);

	}
	return 0;
}

static int mt8512_evb_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
	struct snd_pcm_hw_params *params)
{
	struct mt8512_evb_priv *priv = snd_soc_card_get_drvdata(rtd->card);
	int id = rtd->dai_link->id;
	struct mt8512_evb_etdm_ctrl_data *etdm;
	struct mt8512_evb_dmic_ctrl_data *dmic;
	struct mt8512_evb_multi_in_ctrl_data *multi_in;
	struct mt8512_evb_int_adda_ctrl_data *int_adda;

	unsigned int fix_rate = 0;
	unsigned int fix_bit_width = 0;
	unsigned int fix_channels = 0;
	unsigned int idx;

	if (id == DAI_LINK_ETDM1_IN) {
		idx = MT8512_ETDM1;
		etdm = &priv->etdm_data[idx];
		fix_rate = etdm->fix_rate_in;
		fix_bit_width = etdm->fix_bit_width_in;
		fix_channels = etdm->fix_channels_in;
	} else if (id == DAI_LINK_ETDM2_OUT) {
		idx = MT8512_ETDM2;
		etdm = &priv->etdm_data[idx];
		fix_rate = etdm->fix_rate_out;
		fix_bit_width = etdm->fix_bit_width_out;
		fix_channels = etdm->fix_channels_out;
	} else if (id == DAI_LINK_ETDM2_IN) {
		idx = MT8512_ETDM2;
		etdm = &priv->etdm_data[idx];
		fix_rate = etdm->fix_rate_in;
		fix_bit_width = etdm->fix_bit_width_in;
		fix_channels = etdm->fix_channels_in;
	} else if (id == DAI_LINK_DMIC) {
		dmic = &priv->dmic_data;
		fix_rate = dmic->fix_rate;
		fix_bit_width = dmic->fix_bit_width;
		fix_channels = dmic->fix_channels;
	} else if (id == DAI_LINK_MULTI_IN) {
		multi_in = &priv->multi_in_data;
		fix_bit_width = multi_in->fix_bit_width;
	} else if (id == DAI_LINK_INT_ADDA) {
		int_adda = &priv->int_adda_data;
		fix_rate = int_adda->fix_rate;
	}

	if (fix_rate > 0) {
		struct snd_interval *rate =
			hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE);

		rate->max = rate->min = fix_rate;
	}

	if (fix_bit_width > 0) {
		struct snd_mask *mask =
			hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT);

		if (fix_bit_width == 32) {
			snd_mask_none(mask);
			snd_mask_set(mask, SNDRV_PCM_FORMAT_S32_LE);
		} else if (fix_bit_width == 16) {
			snd_mask_none(mask);
			snd_mask_set(mask, SNDRV_PCM_FORMAT_S16_LE);
		}
	}

	if (fix_channels > 0) {
		struct snd_interval *channels = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_CHANNELS);

		channels->min = channels->max = fix_channels;
	}

	return 0;
}

static struct snd_soc_ops mt8512_evb_etdm_ops = {
	.hw_params = mt8512_evb_hw_params,
};

#ifdef CONFIG_MTK_HIFIXDSP_SUPPORT
static int mt8512_adsp_hostless_va_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dapm_context *dapm;
	struct snd_soc_card *card;
	struct snd_soc_pcm_runtime *afe_rtd;

	/* set ignore suspend for hostless path */
	dapm = snd_soc_component_get_dapm(rtd->cpu_dai->component);
	snd_soc_dapm_ignore_suspend(dapm, "FE_HOSTLESS_VA");
	snd_soc_dapm_ignore_suspend(dapm, "VA UL2 In");
	snd_soc_dapm_ignore_suspend(dapm, "VA TDMIN In");
	snd_soc_dapm_ignore_suspend(dapm, "VA UL9 In");

	card = rtd->card;

	list_for_each_entry(afe_rtd, &card->rtd_list, list) {
		if (!strcmp(afe_rtd->cpu_dai->name, "DMIC"))
			break;
	}
	if (afe_rtd != NULL) {
		dapm = snd_soc_component_get_dapm(afe_rtd->cpu_dai->component);
		snd_soc_dapm_ignore_suspend(dapm, "DMIC In");
		snd_soc_dapm_ignore_suspend(dapm, "ETDM2 In");
	}

	return 0;
}

static int mt8512_adsp_hostless_va_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_dpcm *dpcm;
	struct snd_soc_pcm_runtime *fe = substream->private_data;
	struct snd_soc_pcm_runtime *be;

	/* should set all the connected be to ignore suspend */
	/* all there will be a substream->ops action in suspend_all */

	list_for_each_entry(dpcm,
	    &fe->dpcm[SNDRV_PCM_STREAM_CAPTURE].be_clients, list_be) {
		be = dpcm->be;
		be->dai_link->ignore_suspend = 1;
	}
	return 0;
}

static void mt8512_adsp_hostless_va_shutdown(
	struct snd_pcm_substream *substream)
{
	struct snd_soc_dpcm *dpcm;
	struct snd_soc_pcm_runtime *fe = substream->private_data;
	struct snd_soc_pcm_runtime *be;

	/* should resume all the connected be */
	list_for_each_entry(dpcm,
	    &fe->dpcm[SNDRV_PCM_STREAM_CAPTURE].be_clients, list_be) {
		be = dpcm->be;
		be->dai_link->ignore_suspend = 0;
	}
}

static struct snd_soc_ops adsp_hostless_va_ops = {
	.startup = mt8512_adsp_hostless_va_startup,
	.shutdown = mt8512_adsp_hostless_va_shutdown,
};
#endif

/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link mt8512_evb_dais[] = {
	/* Front End DAI links */
	[DAI_LINK_DLM_PLAYBACK] = {
		.name = "DLM_FE",
		.stream_name = "DLM Playback",
		.cpu_dai_name = "DLM",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = DAI_LINK_DLM_PLAYBACK,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST
		},
		.dynamic = 1,
		.dpcm_playback = 1,
	},
	[DAI_LINK_DL2_PLAYBACK] = {
		.name = "DL2_FE",
		.stream_name = "DL2 Playback",
		.cpu_dai_name = "DL2",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = DAI_LINK_DL2_PLAYBACK,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST
		},
		.dynamic = 1,
		.dpcm_playback = 1,
	},
	[DAI_LINK_DL3_PLAYBACK] = {
		.name = "DL3_FE",
		.stream_name = "DL3 Playback",
		.cpu_dai_name = "DL3",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = DAI_LINK_DL3_PLAYBACK,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST
		},
		.dynamic = 1,
		.dpcm_playback = 1,
	},
	[DAI_LINK_DL6_PLAYBACK] = {
		.name = "DL6_FE",
		.stream_name = "DL6 Playback",
		.cpu_dai_name = "DL6",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = DAI_LINK_DL6_PLAYBACK,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST
		},
		.dynamic = 1,
		.dpcm_playback = 1,
	},
	[DAI_LINK_UL2_CAPTURE] = {
		.name = "UL2_FE",
		.stream_name = "UL2 Capture",
		.cpu_dai_name = "UL2",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = DAI_LINK_UL2_CAPTURE,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST
		},
		.dynamic = 1,
		.dpcm_capture = 1,
	},
	[DAI_LINK_UL3_CAPTURE] = {
		.name = "UL3_FE",
		.stream_name = "UL3 Capture",
		.cpu_dai_name = "UL3",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = DAI_LINK_UL3_CAPTURE,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST
		},
		.dynamic = 1,
		.dpcm_capture = 1,
	},
	[DAI_LINK_UL4_CAPTURE] = {
		.name = "UL4_FE",
		.stream_name = "UL4 Capture",
		.cpu_dai_name = "UL4",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = DAI_LINK_UL4_CAPTURE,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST
		},
		.dynamic = 1,
		.dpcm_capture = 1,
	},
	[DAI_LINK_UL5_CAPTURE] = {
		.name = "UL5_FE",
		.stream_name = "UL5 Capture",
		.cpu_dai_name = "UL5",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = DAI_LINK_UL5_CAPTURE,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST
		},
		.dynamic = 1,
		.dpcm_capture = 1,
	},
	[DAI_LINK_UL8_CAPTURE] = {
		.name = "UL8_FE",
		.stream_name = "UL8 Capture",
		.cpu_dai_name = "UL8",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = DAI_LINK_UL8_CAPTURE,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST
		},
		.dynamic = 1,
		.dpcm_capture = 1,
	},
	[DAI_LINK_UL9_CAPTURE] = {
		.name = "UL9_FE",
		.stream_name = "UL9 Capture",
		.cpu_dai_name = "UL9",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = DAI_LINK_UL9_CAPTURE,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST
		},
		.dynamic = 1,
		.dpcm_capture = 1,
	},
	[DAI_LINK_UL10_CAPTURE] = {
		.name = "UL10_FE",
		.stream_name = "UL10 Capture",
		.cpu_dai_name = "UL10",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = DAI_LINK_UL10_CAPTURE,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST
		},
		.dynamic = 1,
		.dpcm_capture = 1,
	},
	[DAI_LINK_UL1_CAPTURE] = {
		.name = "UL1_FE",
		.stream_name = "UL1 Capture",
		.cpu_dai_name = "UL1",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = DAI_LINK_UL1_CAPTURE,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_PRE,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.dynamic = 1,
		.dpcm_capture = 1,
	},
#ifdef CONFIG_SND_SOC_MTK_BTCVSD
	[DAI_LINK_BTCVSD] = {
		.name = "BTCVSD",
		.stream_name = "BTCVSD",
		.cpu_dai_name = "snd-soc-dummy-dai",
		.platform_name = "18050000.mtk-btcvsd-snd",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
	},
#endif
#ifdef CONFIG_MTK_HIFIXDSP_SUPPORT
	[DAI_LINK_FE_HOSTLESS_VA] = {
		.name = "ADSP HOSTLESS_VA",
		.stream_name = "HOSTLESS_VA",
		.cpu_dai_name = "FE_HOSTLESS_VA",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST
		},
		.dynamic = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		.init = mt8512_adsp_hostless_va_init,
		.ops = &adsp_hostless_va_ops,
	},
	[DAI_LINK_FE_VA] = {
		.name = "ADSP VA_FE",
		.stream_name = "VA_Capture",
		.cpu_dai_name = "FE_VA",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST
		},
		.dynamic = 1,
		.dpcm_capture = 1,
	},
	[DAI_LINK_FE_MIC_RECORD] = {
		.name = "ADSP MIC_RECORD",
		.stream_name = "MIC_RECORD",
		.cpu_dai_name = "FE_MICR",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST
		},
		.dynamic = 1,
		.dpcm_capture = 1,
	},
#endif
	/* Back End DAI links */
	[DAI_LINK_ETDM1_IN] = {
		.name = "ETDM1_IN BE",
		.cpu_dai_name = "ETDM1_IN",
		.no_pcm = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = DAI_LINK_ETDM1_IN,
		.dai_fmt = SND_SOC_DAIFMT_I2S |
			   SND_SOC_DAIFMT_NB_NF |
			   SND_SOC_DAIFMT_CBS_CFS,
		.ops = &mt8512_evb_etdm_ops,
		.dpcm_capture = 1,
	},
	[DAI_LINK_ETDM2_OUT] = {
		.name = "ETDM2_OUT BE",
		.cpu_dai_name = "ETDM2_OUT",
		.no_pcm = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = DAI_LINK_ETDM2_OUT,
		.dai_fmt = SND_SOC_DAIFMT_I2S |
			   SND_SOC_DAIFMT_NB_NF |
			   SND_SOC_DAIFMT_CBS_CFS,
		.ops = &mt8512_evb_etdm_ops,
		.dpcm_playback = 1,
	},
	[DAI_LINK_ETDM2_IN] = {
		.name = "ETDM2_IN BE",
		.cpu_dai_name = "ETDM2_IN",
		.no_pcm = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = DAI_LINK_ETDM2_IN,
		.dai_fmt = SND_SOC_DAIFMT_I2S |
			   SND_SOC_DAIFMT_NB_NF |
			   SND_SOC_DAIFMT_CBS_CFS,
		.ops = &mt8512_evb_etdm_ops,
		.dpcm_capture = 1,
	},
	[DAI_LINK_PCM_INTF] = {
		.name = "PCM1 BE",
		.cpu_dai_name = "PCM1",
		.no_pcm = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = DAI_LINK_PCM_INTF,
		.dai_fmt = SND_SOC_DAIFMT_I2S |
			   SND_SOC_DAIFMT_NB_NF |
			   SND_SOC_DAIFMT_CBS_CFS,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	[DAI_LINK_VIRTUAL_DL_SOURCE] = {
		.name = "VIRTUAL_DL_SRC BE",
		.cpu_dai_name = "VIRTUAL_DL_SRC",
		.no_pcm = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = DAI_LINK_VIRTUAL_DL_SOURCE,
		.dpcm_capture = 1,
	},
	[DAI_LINK_DMIC] = {
		.name = "DMIC BE",
		.cpu_dai_name = "DMIC",
		.no_pcm = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = DAI_LINK_DMIC,
		.dpcm_capture = 1,
	},
	[DAI_LINK_INT_ADDA] = {
		.name = "MTK Codec",
		.cpu_dai_name = "INT ADDA",
		.no_pcm = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = DAI_LINK_INT_ADDA,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	[DAI_LINK_GASRC0] = {
		.name = "GASRC0 BE",
		.cpu_dai_name = "GASRC0",
		.no_pcm = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = DAI_LINK_GASRC0,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	[DAI_LINK_GASRC1] = {
		.name = "GASRC1 BE",
		.cpu_dai_name = "GASRC1",
		.no_pcm = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = DAI_LINK_GASRC1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	[DAI_LINK_GASRC2] = {
		.name = "GASRC2 BE",
		.cpu_dai_name = "GASRC2",
		.no_pcm = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = DAI_LINK_GASRC2,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	[DAI_LINK_GASRC3] = {
		.name = "GASRC3 BE",
		.cpu_dai_name = "GASRC3",
		.no_pcm = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = DAI_LINK_GASRC3,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	[DAI_LINK_SPDIF_IN] = {
		.name = "SPDIF_IN BE",
		.cpu_dai_name = "SPDIF_IN",
		.no_pcm = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = DAI_LINK_SPDIF_IN,
		.dpcm_capture = 1,
	},
	[DAI_LINK_MULTI_IN] = {
		.name = "MULTI_IN BE",
		.cpu_dai_name = "MULTI_IN",
		.no_pcm = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = DAI_LINK_MULTI_IN,
		.dai_fmt = SND_SOC_DAIFMT_I2S |
			   SND_SOC_DAIFMT_NB_NF |
			   SND_SOC_DAIFMT_CBM_CFM,
		.dpcm_capture = 1,
	},
#ifdef CONFIG_MTK_HIFIXDSP_SUPPORT
	[DAI_LINK_BE_TDM_IN] = {
		.name = "ADSP_TDM_IN BE",
		.cpu_dai_name = "BE_TDM_IN",
		.no_pcm = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dpcm_playback = 0,
		.dpcm_capture = 1,
	},
	[DAI_LINK_BE_UL9] = {
		.name = "ADSP_UL9_IN BE",
		.cpu_dai_name = "BE_UL9_IN",
		.no_pcm = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dpcm_playback = 0,
		.dpcm_capture = 1,
	},
	[DAI_LINK_BE_UL2] = {
		.name = "ADSP_UL2_IN BE",
		.cpu_dai_name = "BE_UL2_IN",
		.no_pcm = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dpcm_playback = 0,
		.dpcm_capture = 1,
	},
#endif
};

static int mt8512_evb_gpio_probe(struct snd_soc_card *card)
{
	struct mt8512_evb_priv *priv = snd_soc_card_get_drvdata(card);
	int ret = 0;
	int i;

	priv->pinctrl = devm_pinctrl_get(card->dev);
	if (IS_ERR(priv->pinctrl)) {
		ret = PTR_ERR(priv->pinctrl);
		dev_info(card->dev, "%s devm_pinctrl_get failed %d\n",
			__func__, ret);
		return ret;
	}

	for (i = 0 ; i < PIN_STATE_MAX ; i++) {
		priv->pin_states[i] = pinctrl_lookup_state(priv->pinctrl,
			mt8512_evb_pin_str[i]);
		if (IS_ERR(priv->pin_states[i])) {
			ret = PTR_ERR(priv->pin_states[i]);
			dev_dbg(card->dev, "%s Can't find pin state %s %d\n",
				 __func__, mt8512_evb_pin_str[i], ret);
		}
	}

	if (IS_ERR(priv->pin_states[PIN_STATE_DEFAULT])) {
		dev_info(card->dev, "%s can't find default pin state\n",
			__func__);
		return 0;
	}

	/* default state */
	ret = pinctrl_select_state(priv->pinctrl,
				   priv->pin_states[PIN_STATE_DEFAULT]);
	if (ret)
		dev_info(card->dev, "%s failed to select default state %d\n",
			__func__, ret);

	/* turn off ext spk amp if exist */
	if (!IS_ERR(priv->pin_states[PIN_STATE_EXT_SPK_AMP_OFF])) {
		ret = pinctrl_select_state(priv->pinctrl,
			priv->pin_states[PIN_STATE_EXT_SPK_AMP_OFF]);
		if (ret)
			dev_info(card->dev,
				"%s failed to select ext_spk_amp off: %d\n",
				__func__, ret);
		else
			dev_info(card->dev,
				"%s select ext_spk_amp off OK\n",
				__func__);
	}

	/* turn off ext spk amp if exist */
	if (!IS_ERR(priv->pin_states[PIN_STATE_EXT_LO_AMP_OFF])) {
		ret = pinctrl_select_state(priv->pinctrl,
			priv->pin_states[PIN_STATE_EXT_LO_AMP_OFF]);
		if (ret)
			dev_info(card->dev,
				"%s failed to select lineout amp off: %d\n",
				__func__, ret);
		else
			dev_info(card->dev,
				"%s select lineout amp off OK\n",
				__func__);
	}

	return ret;
}

static void mt8512_evb_parse_of_codec(struct device *dev,
	struct device_node *np,
	struct snd_soc_dai_link *dai_link,
	char *name)
{
	char prop[128];
	int ret;
	unsigned int i, num_codecs;
	struct device_node *codec_node;

	snprintf(prop, sizeof(prop), PREFIX"%s-audio-codec-num", name);
	ret = of_property_read_u32(np, prop, &num_codecs);
	if (ret)
		goto single_codec;

	if (num_codecs == 0)
		return;

	dai_link->codecs = devm_kzalloc(dev,
		num_codecs * sizeof(struct snd_soc_dai_link_component),
		GFP_KERNEL);

	dai_link->num_codecs = num_codecs;
	dai_link->codec_name = NULL;
	dai_link->codec_of_node = NULL;
	dai_link->codec_dai_name = NULL;

	for (i = 0; i < num_codecs; i++) {
		codec_node = NULL;

		// parse codec_of_node
		snprintf(prop, sizeof(prop),
			 PREFIX"%s-audio-codec%u",
			 name, i);
		codec_node = of_parse_phandle(np, prop, 0);
		if (codec_node)
			dai_link->codecs[i].of_node = codec_node;
		else {
			// parse codec name
			snprintf(prop, sizeof(prop),
				 PREFIX"%s-codec-name%u",
				 name, i);
			of_property_read_string(np, prop,
				&dai_link->codecs[i].name);
		}

		// parse codec dai name
		snprintf(prop, sizeof(prop),
			 PREFIX"%s-codec-dai-name%u",
			 name, i);
		of_property_read_string(np, prop,
			&dai_link->codecs[i].dai_name);
	}

	return;

single_codec:
	// parse codec_of_node
	snprintf(prop, sizeof(prop), PREFIX"%s-audio-codec", name);
	codec_node = of_parse_phandle(np, prop, 0);
	if (codec_node) {
		dai_link->codec_of_node = codec_node;
		dai_link->codec_name = NULL;
	}

	// parse codec dai name
	snprintf(prop, sizeof(prop), PREFIX"%s-codec-dai-name", name);
	of_property_read_string(np, prop, &dai_link->codec_dai_name);
}

static void mt8512_evb_parse_of(struct snd_soc_card *card,
				struct device_node *np)
{
	struct mt8512_evb_priv *priv = snd_soc_card_get_drvdata(card);
	size_t i, j;
	int ret;
	char prop[128];
	const char *str;
	unsigned int val;

	static const struct mt8512_dai_link_prop of_dai_links_daifmt[] = {
		{ "dlm",	DAI_LINK_DLM_PLAYBACK },
		{ "dl2",	DAI_LINK_DL2_PLAYBACK },
		{ "dl3",	DAI_LINK_DL3_PLAYBACK },
		{ "dl6",	DAI_LINK_DL6_PLAYBACK },
		{ "ul2",	DAI_LINK_UL2_CAPTURE },
		{ "ul3",	DAI_LINK_UL3_CAPTURE },
		{ "ul4",	DAI_LINK_UL4_CAPTURE },
		{ "ul5",	DAI_LINK_UL5_CAPTURE },
		{ "ul8",	DAI_LINK_UL8_CAPTURE },
		{ "ul9",	DAI_LINK_UL9_CAPTURE },
		{ "ul10",	DAI_LINK_UL10_CAPTURE },
		{ "ul1",	DAI_LINK_UL1_CAPTURE },
		{ "etdm1-in",	DAI_LINK_ETDM1_IN },
		{ "etdm2-out",	DAI_LINK_ETDM2_OUT },
		{ "etdm2-in",	DAI_LINK_ETDM2_IN },
		{ "pcm-intf",	DAI_LINK_PCM_INTF },
		{ "multi-in",	DAI_LINK_MULTI_IN },
	};

	static const struct mt8512_dai_link_prop of_dai_links_etdm[] = {
		{ "etdm1-in",	DAI_LINK_ETDM1_IN },
		{ "etdm2-out",	DAI_LINK_ETDM2_OUT },
		{ "etdm2-in",	DAI_LINK_ETDM2_IN },
	};

	static const struct mt8512_dai_link_prop of_dai_links_dmic[] = {
		{ "dmic",	DAI_LINK_DMIC },
	};

#ifdef CONFIG_SND_SOC_MT8512_CODEC
	static const struct mt8512_dai_link_prop
		of_dai_links_int_adda[] = {
		{ "int-adda",	DAI_LINK_INT_ADDA },
	};
#endif

	struct {
		char *name;
		unsigned int val;
	} of_fmt_table[] = {
		{ "i2s",	SND_SOC_DAIFMT_I2S },
		{ "right_j",	SND_SOC_DAIFMT_RIGHT_J },
		{ "left_j",	SND_SOC_DAIFMT_LEFT_J },
		{ "dsp_a",	SND_SOC_DAIFMT_DSP_A },
		{ "dsp_b",	SND_SOC_DAIFMT_DSP_B },
		{ "ac97",	SND_SOC_DAIFMT_AC97 },
		{ "pdm",	SND_SOC_DAIFMT_PDM },
		{ "msb",	SND_SOC_DAIFMT_MSB },
		{ "lsb",	SND_SOC_DAIFMT_LSB },
	};

	snd_soc_of_parse_card_name(card, PREFIX"card-name");

	for (i = 0; i < ARRAY_SIZE(of_dai_links_daifmt); i++) {
		struct snd_soc_dai_link *dai_link =
			&mt8512_evb_dais[of_dai_links_daifmt[i].link_id];
		bool lrck_inverse = false;
		bool bck_inverse = false;

		/* parse format */
		snprintf(prop, sizeof(prop), PREFIX"%s-format",
			 of_dai_links_daifmt[i].name);
		ret = of_property_read_string(np, prop, &str);
		if (ret == 0) {
			unsigned int format = 0;

			for (j = 0; j < ARRAY_SIZE(of_fmt_table); j++) {
				if (strcmp(str, of_fmt_table[j].name) == 0) {
					format |= of_fmt_table[j].val;
					break;
				}
			}

			dai_link->dai_fmt &= ~SND_SOC_DAIFMT_FORMAT_MASK;
			dai_link->dai_fmt |= format;
		}

		/* parse clock mode */
		snprintf(prop, sizeof(prop), PREFIX"%s-master-clock",
			 of_dai_links_daifmt[i].name);
		ret = of_property_read_u32(np, prop, &val);
		if (ret == 0) {
			dai_link->dai_fmt &= ~SND_SOC_DAIFMT_MASTER_MASK;
			if (val)
				dai_link->dai_fmt |= SND_SOC_DAIFMT_CBS_CFS;
			else
				dai_link->dai_fmt |= SND_SOC_DAIFMT_CBM_CFM;
		}

		/* parse lrck inverse */
		snprintf(prop, sizeof(prop), PREFIX"%s-lrck-inverse",
			 of_dai_links_daifmt[i].name);
		lrck_inverse = of_property_read_bool(np, prop);

		/* parse bck inverse */
		snprintf(prop, sizeof(prop), PREFIX"%s-bck-inverse",
			 of_dai_links_daifmt[i].name);
		bck_inverse = of_property_read_bool(np, prop);

		dai_link->dai_fmt &= ~SND_SOC_DAIFMT_INV_MASK;

		if (lrck_inverse && bck_inverse)
			dai_link->dai_fmt |= SND_SOC_DAIFMT_IB_IF;
		else if (lrck_inverse && !bck_inverse)
			dai_link->dai_fmt |= SND_SOC_DAIFMT_NB_IF;
		else if (!lrck_inverse && bck_inverse)
			dai_link->dai_fmt |= SND_SOC_DAIFMT_IB_NF;
		else
			dai_link->dai_fmt |= SND_SOC_DAIFMT_NB_NF;
	}

	for (i = 0; i < ARRAY_SIZE(of_dai_links_etdm); i++) {
		unsigned int link_id = of_dai_links_etdm[i].link_id;
		struct snd_soc_dai_link *dai_link = &mt8512_evb_dais[link_id];
		struct mt8512_evb_etdm_ctrl_data *etdm;

		/* parse mclk multiplier */
		snprintf(prop, sizeof(prop), PREFIX"%s-mclk-multiplier",
			 of_dai_links_etdm[i].name);
		ret = of_property_read_u32(np, prop, &val);
		if (ret == 0) {
			switch (link_id) {
			case DAI_LINK_ETDM1_IN:
				etdm = &priv->etdm_data[MT8512_ETDM1];
				etdm->mck_multp_in = val;
				break;
			case DAI_LINK_ETDM2_OUT:
				etdm = &priv->etdm_data[MT8512_ETDM2];
				etdm->mck_multp_out = val;
				break;
			case DAI_LINK_ETDM2_IN:
				etdm = &priv->etdm_data[MT8512_ETDM2];
				etdm->mck_multp_in = val;
				break;
			default:
				break;
			}
		}

		/* parse tlv320 clocksource */
		snprintf(prop, sizeof(prop), PREFIX"%s-tlv320-clkmux",
			 of_dai_links_etdm[i].name);
		ret = of_property_read_u32(np, prop, &val);
		if (ret == 0) {
			switch (link_id) {
			case DAI_LINK_ETDM1_IN:
				etdm = &priv->etdm_data[MT8512_ETDM1];
				etdm->tlv320_clock_mux = val;
				break;
			default:
				break;
			}
		}

		/* parse lrck width */
		snprintf(prop, sizeof(prop), PREFIX"%s-lrck-width",
			 of_dai_links_etdm[i].name);
		ret = of_property_read_u32(np, prop, &val);
		if (ret == 0) {
			switch (link_id) {
			case DAI_LINK_ETDM1_IN:
				etdm = &priv->etdm_data[MT8512_ETDM1];
				etdm->lrck_width_in = val;
				break;
			case DAI_LINK_ETDM2_OUT:
				etdm = &priv->etdm_data[MT8512_ETDM2];
				etdm->lrck_width_out = val;
				break;
			case DAI_LINK_ETDM2_IN:
				etdm = &priv->etdm_data[MT8512_ETDM2];
				etdm->lrck_width_in = val;
				break;
			default:
				break;
			}
		}

		/* parse fix rate */
		snprintf(prop, sizeof(prop), PREFIX"%s-fix-rate",
			 of_dai_links_etdm[i].name);
		ret = of_property_read_u32(np, prop, &val);
		if (ret == 0 && mt8512_afe_rate_supported(val,
		    link_to_dai(link_id))) {
			switch (link_id) {
			case DAI_LINK_ETDM1_IN:
				etdm = &priv->etdm_data[MT8512_ETDM1];
				etdm->fix_rate_in = val;
				break;
			case DAI_LINK_ETDM2_OUT:
				etdm = &priv->etdm_data[MT8512_ETDM2];
				etdm->fix_rate_out = val;
				break;
			case DAI_LINK_ETDM2_IN:
				etdm = &priv->etdm_data[MT8512_ETDM2];
				etdm->fix_rate_in = val;
				break;
			default:
				break;
			}

			dai_link->be_hw_params_fixup =
				mt8512_evb_be_hw_params_fixup;
		}

		/* parse fix bit width */
		snprintf(prop, sizeof(prop), PREFIX"%s-fix-bit-width",
			 of_dai_links_etdm[i].name);
		ret = of_property_read_u32(np, prop, &val);
		if (ret == 0 && (val == 32 || val == 16)) {
			switch (link_id) {
			case DAI_LINK_ETDM1_IN:
				etdm = &priv->etdm_data[MT8512_ETDM1];
				etdm->fix_bit_width_in = val;
				break;
			case DAI_LINK_ETDM2_OUT:
				etdm = &priv->etdm_data[MT8512_ETDM2];
				etdm->fix_bit_width_out = val;
				break;
			case DAI_LINK_ETDM2_IN:
				etdm = &priv->etdm_data[MT8512_ETDM2];
				etdm->fix_bit_width_in = val;
				break;
			default:
				break;
			}

			dai_link->be_hw_params_fixup =
				mt8512_evb_be_hw_params_fixup;
		}

		/* parse fix channels */
		snprintf(prop, sizeof(prop), PREFIX"%s-fix-channels",
			 of_dai_links_etdm[i].name);
		ret = of_property_read_u32(np, prop, &val);
		if (ret == 0 && mt8512_afe_channel_supported(val,
		    link_to_dai(link_id))) {
			switch (link_id) {
			case DAI_LINK_ETDM1_IN:
				etdm = &priv->etdm_data[MT8512_ETDM1];
				etdm->fix_channels_in = val;
				break;
			case DAI_LINK_ETDM2_OUT:
				etdm = &priv->etdm_data[MT8512_ETDM2];
				etdm->fix_channels_out = val;
				break;
			case DAI_LINK_ETDM2_IN:
				etdm = &priv->etdm_data[MT8512_ETDM2];
				etdm->fix_channels_in = val;
				break;
			default:
				break;
			}

			dai_link->be_hw_params_fixup =
				mt8512_evb_be_hw_params_fixup;
		}

		mt8512_evb_parse_of_codec(card->dev, np, dai_link,
			of_dai_links_etdm[i].name);

		/* parse ignore pmdown time */
		snprintf(prop, sizeof(prop), PREFIX"%s-ignore-pmdown-time",
			 of_dai_links_etdm[i].name);
		if (of_property_read_bool(np, prop))
			dai_link->ignore_pmdown_time = 1;

		/* parse ignore suspend */
		snprintf(prop, sizeof(prop), PREFIX"%s-ignore-suspend",
			 of_dai_links_etdm[i].name);
		if (of_property_read_bool(np, prop))
			dai_link->ignore_suspend = 1;
	}

	for (i = 0; i < ARRAY_SIZE(of_dai_links_dmic); i++) {
		unsigned int link_id = of_dai_links_dmic[i].link_id;
		struct snd_soc_dai_link *dai_link = &mt8512_evb_dais[link_id];
		struct mt8512_evb_dmic_ctrl_data *dmic;

		/* parse fix rate */
		snprintf(prop, sizeof(prop), PREFIX"%s-fix-rate",
			 of_dai_links_dmic[i].name);
		ret = of_property_read_u32(np, prop, &val);
		if (ret == 0 && mt8512_afe_rate_supported(val,
		    link_to_dai(link_id))) {
			switch (link_id) {
			case DAI_LINK_DMIC:
				dmic = &priv->dmic_data;
				dmic->fix_rate = val;
				break;
			default:
				break;
			}

			dai_link->be_hw_params_fixup =
				mt8512_evb_be_hw_params_fixup;
		}

		/* parse fix bit width */
		snprintf(prop, sizeof(prop), PREFIX"%s-fix-bit-width",
			 of_dai_links_dmic[i].name);
		ret = of_property_read_u32(np, prop, &val);
		if (ret == 0 && (val == 32 || val == 16)) {
			switch (link_id) {
			case DAI_LINK_DMIC:
				dmic = &priv->dmic_data;
				dmic->fix_bit_width = val;
				break;
			default:
				break;
			}

			dai_link->be_hw_params_fixup =
				mt8512_evb_be_hw_params_fixup;
		}

		/* parse fix channels */
		snprintf(prop, sizeof(prop), PREFIX"%s-fix-channels",
			 of_dai_links_dmic[i].name);
		ret = of_property_read_u32(np, prop, &val);
		if (ret == 0 && mt8512_afe_channel_supported(val,
		    link_to_dai(link_id))) {
			switch (link_id) {
			case DAI_LINK_DMIC:
				dmic = &priv->dmic_data;
				dmic->fix_channels = val;
				break;
			default:
				break;
			}

			dai_link->be_hw_params_fixup =
				mt8512_evb_be_hw_params_fixup;
		}
	}

	/* parse fix bit width */
	snprintf(prop, sizeof(prop), PREFIX"%s-fix-bit-width", "multi-in");
	ret = of_property_read_u32(np, prop, &val);
	if (ret == 0 && (val == 32 || val == 16)) {
		priv->multi_in_data.fix_bit_width = val;
		mt8512_evb_dais[DAI_LINK_MULTI_IN].be_hw_params_fixup =
			mt8512_evb_be_hw_params_fixup;
	}

#ifdef CONFIG_SND_SOC_MT8512_CODEC
	for (i = 0; i < ARRAY_SIZE(of_dai_links_int_adda); i++) {
		unsigned int link_id = of_dai_links_int_adda[i].link_id;
		struct snd_soc_dai_link *dai_link = &mt8512_evb_dais[link_id];
		struct mt8512_evb_int_adda_ctrl_data *int_adda;

		/* parse fix rate */
		snprintf(prop, sizeof(prop), PREFIX"%s-fix-rate",
			 of_dai_links_int_adda[i].name);
		ret = of_property_read_u32(np, prop, &val);
		if (ret == 0 && mt8512_afe_rate_supported(val,
		    link_to_dai(link_id))) {
			switch (link_id) {
			case DAI_LINK_INT_ADDA:
				int_adda = &priv->int_adda_data;
				int_adda->fix_rate = val;
				break;
			default:
				break;
			}

			dai_link->be_hw_params_fixup =
				mt8512_evb_be_hw_params_fixup;
		}

		mt8512_evb_parse_of_codec(card->dev, np, dai_link,
			of_dai_links_int_adda[i].name);
	}
#endif

}

static struct snd_soc_card mt8512_evb_card = {
	.name = "mt-snd-card",
	.owner = THIS_MODULE,
	.dai_link = mt8512_evb_dais,
	.num_links = ARRAY_SIZE(mt8512_evb_dais),
	.dapm_widgets = mt8512_evb_widgets,
	.num_dapm_widgets = ARRAY_SIZE(mt8512_evb_widgets),
	.dapm_routes = mt8512_evb_routes,
	.num_dapm_routes = ARRAY_SIZE(mt8512_evb_routes),
};

static int mt8512_evb_dev_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &mt8512_evb_card;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *platform_node;
	struct mt8512_evb_priv *priv;
	int ret, id;
	size_t i;
	size_t dais_num = ARRAY_SIZE(mt8512_evb_dais);

	platform_node = of_parse_phandle(dev->of_node, "mediatek,platform", 0);
	if (!platform_node) {
		dev_info(dev, "Property 'platform' missing or invalid\n");
		return -EINVAL;
	}

	for (i = 0; i < dais_num; i++) {
#ifdef CONFIG_MTK_HIFIXDSP_SUPPORT
		if (is_adsp_dai_link(i))
			continue;
#endif
		if (mt8512_evb_dais[i].platform_name)
			continue;

		id = mt8512_evb_dais[i].id;

		if ((id >= DAI_LINK_AFE_FE_BASE &&
		     id <= DAI_LINK_AFE_FE_END) ||
		    (id >= DAI_LINK_AFE_BE_BASE &&
		     id <= DAI_LINK_AFE_BE_END)) {
			mt8512_evb_dais[i].platform_of_node = platform_node;
		}
	}

#ifdef CONFIG_MTK_HIFIXDSP_SUPPORT
	platform_node = of_parse_phandle(dev->of_node,
					 "mediatek,adsp-platform", 0);
	if (!platform_node) {
		dev_info(dev, "Property 'adsp-platform' missing or invalid\n");
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(adsp_dai_links); i++) {
		unsigned int dai = adsp_dai_links[i];

		if (mt8512_evb_dais[dai].platform_name)
			continue;
		mt8512_evb_dais[dai].platform_of_node = platform_node;
	}
#endif

	card->dev = dev;

	priv = devm_kzalloc(dev, sizeof(struct mt8512_evb_priv),
			    GFP_KERNEL);
	if (!priv) {
		ret = -ENOMEM;
		dev_info(dev, "%s allocate card private data fail %d\n",
			__func__, ret);
		return ret;
	}

	snd_soc_card_set_drvdata(card, priv);

	mt8512_evb_gpio_probe(card);

	mt8512_evb_parse_of(card, np);

	ret = devm_snd_soc_register_card(dev, card);
	if (ret) {
		dev_info(dev, "%s snd_soc_register_card fail %d\n",
			__func__, ret);
		return ret;
	}

	return ret;
}

static const struct of_device_id mt8512_evb_dt_match[] = {
	{ .compatible = "mediatek,mt8512-evb", },
	{ }
};
MODULE_DEVICE_TABLE(of, mt8512_evb_dt_match);

static struct platform_driver mt8512_evb_driver = {
	.driver = {
		   .name = "mt8512-evb",
		   .of_match_table = mt8512_evb_dt_match,
#ifdef CONFIG_PM
		   .pm = &snd_soc_pm_ops,
#endif
	},
	.probe = mt8512_evb_dev_probe,
};

module_platform_driver(mt8512_evb_driver);

/* Module information */
MODULE_DESCRIPTION("MT8512 EVB SoC machine driver");
MODULE_AUTHOR("Mengge Wang <mengge.wang@mediatek.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:mt8512-evb");

