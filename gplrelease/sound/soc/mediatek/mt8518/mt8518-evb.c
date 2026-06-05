/*
 * mt8518-evb.c  --  MT8518 machine driver
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

#include <linux/module.h>
#include <linux/of_gpio.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include "mt8518-afe-common.h"
#ifdef CONFIG_MTK_HIFI4DSP_SUPPORT
#include "mt8518-adsp-utils.h"
#endif

#define TEST_BACKEND_WITH_ENDPOINT

#define PREFIX	"mediatek,"
#define ENUM_TO_STR(enum) #enum

enum PINCTRL_PIN_STATE {
	PIN_STATE_DEFAULT = 0,
	PIN_STATE_EXTAMP_ON,
	PIN_STATE_EXTAMP_OFF,
	PIN_STATE_MAX
};

static const char * const mt8518_evb_pin_str[PIN_STATE_MAX] = {
	"default",
	"extamp_on",
	"extamp_off",
};

struct mt8518_evb_etdm_ctrl_data {
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
};

struct mt8518_evb_dmic_ctrl_data {
	unsigned int fix_rate;
	unsigned int fix_channels;
	unsigned int fix_bit_width;
};

struct mt8518_evb_multi_in_ctrl_data {
	unsigned int fix_bit_width;
};

#ifdef CONFIG_MTK_HIFI4DSP_SUPPORT
struct mt8518_evb_spi_be_ctrl_data {
	unsigned int mck_multp;
	unsigned int lrck_width;
	unsigned int fix_rate;
	unsigned int fix_channels;
	unsigned int fix_bit_width;
};
#endif

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

struct mt8518_evb_priv {
	struct pinctrl *pinctrl;
	struct pinctrl_state *pin_states[PIN_STATE_MAX];
	struct mt8518_evb_etdm_ctrl_data etdm_data[MT8518_ETDM_SETS];
	struct mt8518_evb_dmic_ctrl_data dmic_data;
	struct mt8518_evb_multi_in_ctrl_data multi_in_data;
#ifdef CONFIG_MTK_HIFI4DSP_SUPPORT
	struct mt8518_evb_spi_be_ctrl_data spi_be_data[MT8518_ADSP_BE_CNT];
#endif
#ifdef CONFIG_SND_SOC_GAPP_AUDIO_CONTROL
	struct soc_ctlx_res ctlx_res;
#endif
};

#ifdef CONFIG_SND_SOC_GAPP_AUDIO_CONTROL
static inline int soc_ctlx_init(struct soc_ctlx_res *ctlx_res,
	struct snd_soc_card *soc_card)
{
	int i;
	struct snd_card *card = soc_card->snd_card;
	struct snd_kcontrol *control;

	ctlx_res->master_volume = 100;
	ctlx_res->master_switch = 1;
	ctlx_res->pcm_state = MTKFILE_PCM_STATE_UNKNOWN;

	mutex_init(&ctlx_res->res_mutex);
	spin_lock_init(&ctlx_res->res_lock);

	for (i = 0; i < CTRL_NOTIFY_NUM; i++) {
		list_for_each_entry(control, &card->controls, list) {
			if (strcmp(control->id.name, nfy_ctl_names[i]))
				continue;
			ctlx_res->nfy_ids[i] = control->id;
		}
	}

	return 0;
}

static int soc_ctlx_get(struct snd_kcontrol *kctl,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kctl);
	struct mt8518_evb_priv *card_data = snd_soc_card_get_drvdata(card);
	struct soc_ctlx_res *res_mgr = &card_data->ctlx_res;
	int type;

	for (type = 0; type < CTRL_NOTIFY_NUM; type++) {
		if (kctl->id.numid == res_mgr->nfy_ids[type].numid)
			break;
	}

	if (type == CTRL_NOTIFY_NUM) {
		pr_notice("%s invalid mixer control(numid:%d)\n",
			  __func__, kctl->id.numid);
		return -EINVAL;
	}

	mutex_lock(&res_mgr->res_mutex);

	switch (type) {
	case MASTER_VOLUME_ID:
	case MASTER_VOLUMEX_ID:
		ucontrol->value.integer.value[0] = res_mgr->master_volume;
		break;
	case MASTER_SWITCH_ID:
	case MASTER_SWITCHX_ID:
		ucontrol->value.integer.value[0] = res_mgr->master_switch;
		break;
	default:
		break;
	}

	mutex_unlock(&res_mgr->res_mutex);

	pr_debug("%s (%s) value is:%ld\n",
		 __func__, kctl->id.name, ucontrol->value.integer.value[0]);
	return 0;
}

static int soc_ctlx_put(struct snd_kcontrol *kctl,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kctl);
	struct mt8518_evb_priv *card_data = snd_soc_card_get_drvdata(card);
	struct soc_ctlx_res *res_mgr = &card_data->ctlx_res;
	int type;
	int nfy_type;
	int need_notify_self = 0;
	int *value = NULL;

	for (type = 0; type < CTRL_NOTIFY_NUM; type++) {
		if (kctl->id.numid == res_mgr->nfy_ids[type].numid)
			break;
	}

	if (type == CTRL_NOTIFY_NUM) {
		pr_notice("%s invalid mixer control(numid:%d)\n",
			  __func__, kctl->id.numid);
		return -EINVAL;
	}

	mutex_lock(&res_mgr->res_mutex);

	switch (type) {
	case MASTER_VOLUME_ID:
		if ((res_mgr->master_switch == 1) ||
			(ucontrol->value.integer.value[0] != 0)) {
			nfy_type = MASTER_VOLUMEX_ID;
			value = &res_mgr->master_volume;
			need_notify_self = 1;
		}
		break;
	case MASTER_VOLUMEX_ID:
		nfy_type = MASTER_VOLUME_ID;
		value = &res_mgr->master_volume;
		break;
	case MASTER_SWITCH_ID:
		nfy_type = MASTER_SWITCHX_ID;
		value = &res_mgr->master_switch;
		need_notify_self = 1;
		break;
	case MASTER_SWITCHX_ID:
		nfy_type = MASTER_SWITCH_ID;
		value = &res_mgr->master_switch;
		break;
	default:
		break;
	}

	if (value != NULL) {
		*value = ucontrol->value.integer.value[0];
		snd_ctl_notify(card->snd_card, SNDRV_CTL_EVENT_MASK_VALUE,
			       &(res_mgr->nfy_ids[nfy_type]));
	} else {
		nfy_type = CTRL_NOTIFY_INVAL;
	}

	if (need_notify_self) {
		snd_ctl_notify(card->snd_card, SNDRV_CTL_EVENT_MASK_VALUE,
			       &(kctl->id));
	}

	mutex_unlock(&res_mgr->res_mutex);

	pr_debug("%s (%s) value is:%ld, notify id:%x, notify self:%d\n",
		 __func__, kctl->id.name, ucontrol->value.integer.value[0],
		 nfy_type, need_notify_self);
	return 0;
}

static int soc_pcm_state_get(struct snd_kcontrol *kctl,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kctl);
	struct mt8518_evb_priv *card_data = snd_soc_card_get_drvdata(card);
	struct soc_ctlx_res *res_mgr = &card_data->ctlx_res;
	unsigned long flags;

	spin_lock_irqsave(&res_mgr->res_lock, flags);
	ucontrol->value.integer.value[0] = res_mgr->pcm_state;
	spin_unlock_irqrestore(&res_mgr->res_lock, flags);

	pr_debug("%s (%s) value is:%ld\n",
		 __func__, kctl->id.name,
		 ucontrol->value.integer.value[0]);
	return 0;
}

static int soc_pcm_state_put(struct snd_kcontrol *kctl,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kctl);
	struct mt8518_evb_priv *card_data = snd_soc_card_get_drvdata(card);
	struct soc_ctlx_res *res_mgr = &card_data->ctlx_res;
	unsigned long flags;

	spin_lock_irqsave(&res_mgr->res_lock, flags);
	if (ucontrol->value.integer.value[0] != res_mgr->pcm_state) {
		res_mgr->pcm_state = ucontrol->value.integer.value[0];
		snd_ctl_notify(card->snd_card, SNDRV_CTL_EVENT_MASK_VALUE,
			       &(res_mgr->nfy_ids[PCM_STATEX_ID]));
	}
	spin_unlock_irqrestore(&res_mgr->res_lock, flags);

	pr_debug("%s (%s) value is:%ld\n",
		 __func__, kctl->id.name,
		 ucontrol->value.integer.value[0]);
	return 0;
}

static int dlm_playback_state_set(struct snd_pcm_substream *substream,
	int state)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct mt8518_evb_priv *card_data = snd_soc_card_get_drvdata(card);
	struct soc_ctlx_res *res_mgr = &card_data->ctlx_res;
	int nfy_type;
	unsigned long flags;

	nfy_type = PCM_STATEX_ID;

	spin_lock_irqsave(&res_mgr->res_lock, flags);
	if (res_mgr->pcm_state != state) {
		res_mgr->pcm_state = state;
		snd_ctl_notify(card->snd_card, SNDRV_CTL_EVENT_MASK_VALUE,
			       &(res_mgr->nfy_ids[nfy_type]));
	} else {
		nfy_type = CTRL_NOTIFY_INVAL;
	}
	spin_unlock_irqrestore(&res_mgr->res_lock, flags);

	return 0;
}

static int dlm_playback_startup(struct snd_pcm_substream *substream)
{
	dlm_playback_state_set(substream, MTKFILE_PCM_STATE_OPEN);
	return 0;
}

static void dlm_playback_shutdown(struct snd_pcm_substream *substream)
{
	dlm_playback_state_set(substream, MTKFILE_PCM_STATE_CLOSE);
}

static int dlm_playback_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	dlm_playback_state_set(substream, MTKFILE_PCM_STATE_HW_PARAMS);
	return 0;
}

static int dlm_playback_hw_free(struct snd_pcm_substream *substream)
{
	dlm_playback_state_set(substream, MTKFILE_PCM_STATE_HW_FREE);
	return 0;
}

static int dlm_playback_trigger(struct snd_pcm_substream *substream, int cmd)
{
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		dlm_playback_state_set(substream, MTKFILE_PCM_STATE_START);
		break;
	default:
		break;
	}
	return 0;
}

static struct snd_soc_ops dlm_playback_ops = {
	.startup = dlm_playback_startup,
	.shutdown = dlm_playback_shutdown,
	.hw_params = dlm_playback_hw_params,
	.hw_free = dlm_playback_hw_free,
	.trigger = dlm_playback_trigger,
};
#endif

struct mt8518_dai_link_prop {
	char *name;
	unsigned int link_id;
};

static int mt8518_evb_ext_spk_amp_wevent(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;
	struct mt8518_evb_priv *card_data = snd_soc_card_get_drvdata(card);
	int ret = 0;

	dev_dbg(card->dev, "%s event %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		if (!IS_ERR(card_data->pin_states[PIN_STATE_EXTAMP_ON])) {
			ret = pinctrl_select_state(card_data->pinctrl,
				card_data->pin_states[PIN_STATE_EXTAMP_ON]);
			if (ret)
				dev_err(card->dev,
					"%s failed to select state %d\n",
					__func__, ret);
		} else {
			dev_info(card->dev,
				 "%s invalid pin state %s\n",
				 __func__,
				 mt8518_evb_pin_str[PIN_STATE_EXTAMP_ON]);
		}
		break;
	case SND_SOC_DAPM_PRE_PMD:
		if (!IS_ERR(card_data->pin_states[PIN_STATE_EXTAMP_OFF])) {
			ret = pinctrl_select_state(card_data->pinctrl,
				card_data->pin_states[PIN_STATE_EXTAMP_OFF]);
			if (ret)
				dev_err(card->dev,
					"%s failed to select state %d\n",
					__func__, ret);
		} else {
			dev_info(card->dev,
				 "%s invalid pin state %s\n",
				 __func__,
				 mt8518_evb_pin_str[PIN_STATE_EXTAMP_OFF]);
		}
		break;
	default:
		break;
	}

	return 0;
}

static const struct snd_soc_dapm_widget mt8518_evb_widgets[] = {
	SND_SOC_DAPM_OUTPUT("HFP Out"),
	SND_SOC_DAPM_INPUT("HFP In"),
	SND_SOC_DAPM_INPUT("DMIC In"),
	SND_SOC_DAPM_SPK("Ext Spk Amp", mt8518_evb_ext_spk_amp_wevent),
#ifdef TEST_BACKEND_WITH_ENDPOINT
	SND_SOC_DAPM_OUTPUT("ETDM1 Out"),
	SND_SOC_DAPM_INPUT("ETDM1 In"),
	SND_SOC_DAPM_OUTPUT("ETDM2 Out"),
	SND_SOC_DAPM_INPUT("ETDM2 In"),
#endif
};

static const struct snd_soc_dapm_route mt8518_evb_routes[] = {
	{"HFP Out", NULL, "PCM1 Playback"},
	{"PCM1 Capture", NULL, "HFP In"},
#ifdef TEST_BACKEND_WITH_ENDPOINT
	{"ETDM1 Out", NULL, "ETDM1 Playback"},
	{"ETDM1 Capture", NULL, "ETDM1 In"},
	{"ETDM2 Out", NULL, "ETDM2 Playback"},
	{"ETDM2 Capture", NULL, "ETDM2 In"},
#endif
	{"DMIC Capture", NULL, "DMIC In"},

#ifdef CONFIG_SND_SOC_MT8518_CODEC
	{"DIG_DAC_CLK", NULL, "AFE_DAC_CLK"},
	{"DIG_ADC_CLK", NULL, "AFE_ADC_CLK"},
	{"Ext Spk Amp", NULL, "AU_LOL"},
#endif
};

enum {
	/* FE */
	DAI_LINK_AFE_FE_BASE = 0,
	DAI_LINK_DLM_PLAYBACK = DAI_LINK_AFE_FE_BASE,
	DAI_LINK_DL2_PLAYBACK,
	DAI_LINK_DL3_PLAYBACK,
	DAI_LINK_DL6_PLAYBACK,
	DAI_LINK_DL8_PLAYBACK,
	DAI_LINK_UL2_CAPTURE,
	DAI_LINK_UL3_CAPTURE,
	DAI_LINK_UL4_CAPTURE,
	DAI_LINK_UL5_CAPTURE,
	DAI_LINK_UL8_CAPTURE,
	DAI_LINK_UL9_CAPTURE,
	DAI_LINK_UL10_CAPTURE,
	DAI_LINK_DL7_PLAYBACK,
	DAI_LINK_UL1_CAPTURE,
	DAI_LINK_AFE_FE_END = DAI_LINK_UL1_CAPTURE,
#ifdef CONFIG_MTK_HIFI4DSP_SUPPORT
	DAI_LINK_SPI_FE_BASE,
	DAI_LINK_VA_HOSTLESS = DAI_LINK_SPI_FE_BASE,
	DAI_LINK_SPI_MIC_CAPTURE,
	DAI_LINK_VA_UPLOAD,
	DAI_LINK_SPI_RESERVE2,
	DAI_LINK_SPI_RESERVE3,
	DAI_LINK_SPI_RESERVE4,
	DAI_LINK_SPI_PCM_PLAYBACK1,
	DAI_LINK_SPI_RESERVE6,
	DAI_LINK_SPI_RESERVE7,
	DAI_LINK_SPI_RESERVE8,
	DAI_LINK_SPI_FE_END = DAI_LINK_SPI_RESERVE8,
	DAI_LINK_COMPRP_FE_BASE,
	DAI_LINK_COMPRP_FE1 = DAI_LINK_COMPRP_FE_BASE,
	DAI_LINK_COMPRP_FE2,
	DAI_LINK_COMPRP_FE3,
	DAI_LINK_COMPRP_FE_END = DAI_LINK_COMPRP_FE3,
#endif
	/* BE */
	DAI_LINK_AFE_BE_BASE,
	DAI_LINK_ETDM1_OUT = DAI_LINK_AFE_BE_BASE,
	DAI_LINK_ETDM1_IN,
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
	DAI_LINK_SPDIF_OUT,
	DAI_LINK_SPDIF_IN,
	DAI_LINK_MULTI_IN,
	DAI_LINK_AFE_BE_END = DAI_LINK_MULTI_IN,
#ifdef CONFIG_MTK_HIFI4DSP_SUPPORT
	DAI_LINK_SPI_BE_BASE,
	DAI_LINK_SPI_MIC_BE = DAI_LINK_SPI_BE_BASE,
	DAI_LINK_SPI_PRIMARY_PLAYBACK_BE,
	DAI_LINK_SPI_BE_END = DAI_LINK_SPI_PRIMARY_PLAYBACK_BE,
#endif
	DAI_LINK_NUM
};

static int link_to_dai(int link_id)
{
	switch (link_id) {
	case DAI_LINK_ETDM1_OUT:
		return MT8518_AFE_IO_ETDM1_OUT;
	case DAI_LINK_ETDM1_IN:
		return MT8518_AFE_IO_ETDM1_IN;
	case DAI_LINK_ETDM2_OUT:
		return MT8518_AFE_IO_ETDM2_OUT;
	case DAI_LINK_ETDM2_IN:
		return MT8518_AFE_IO_ETDM2_IN;
	case DAI_LINK_PCM_INTF:
		return MT8518_AFE_IO_PCM1;
	case DAI_LINK_DMIC:
		return MT8518_AFE_IO_DMIC;
	case DAI_LINK_INT_ADDA:
		return MT8518_AFE_IO_INT_ADDA;
	default:
		break;
	}
	return -1;
}

static int mt8518_evb_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt8518_evb_priv *priv = snd_soc_card_get_drvdata(rtd->card);
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int id = rtd->dai_link->id;
	struct mt8518_evb_etdm_ctrl_data *etdm;
#ifdef CONFIG_MTK_HIFI4DSP_SUPPORT
	struct mt8518_evb_spi_be_ctrl_data *spi_data;
#endif
	unsigned int mclk_multiplier = 0;
	unsigned int mclk = 0;
	unsigned int lrck_width = 0;
	int slot = 0;
	int slot_width = 0;
	unsigned int slot_bitmask = 0;
	unsigned int idx;
	int ret;

	if (id == DAI_LINK_ETDM1_OUT) {
		idx = MT8518_ETDM1;
		etdm = &priv->etdm_data[idx];
		mclk_multiplier = etdm->mck_multp_out;
		lrck_width = etdm->lrck_width_out;
	} else if (id == DAI_LINK_ETDM1_IN) {
		idx = MT8518_ETDM1;
		etdm = &priv->etdm_data[idx];
		mclk_multiplier = etdm->mck_multp_in;
		lrck_width = etdm->lrck_width_in;
	} else if (id == DAI_LINK_ETDM2_OUT) {
		idx = MT8518_ETDM2;
		etdm = &priv->etdm_data[idx];
		mclk_multiplier = etdm->mck_multp_out;
		lrck_width = etdm->lrck_width_out;
	} else if (id == DAI_LINK_ETDM2_IN) {
		idx = MT8518_ETDM2;
		etdm = &priv->etdm_data[idx];
		mclk_multiplier = etdm->mck_multp_in;
		lrck_width = etdm->lrck_width_in;
#ifdef CONFIG_MTK_HIFI4DSP_SUPPORT
	} else if (id == DAI_LINK_SPI_PRIMARY_PLAYBACK_BE) {
		idx = MT8518_ADSP_BE_PRIMARY_PLAYBACK - MT8518_ADSP_BE_START;
		spi_data = &priv->spi_be_data[idx];
		mclk_multiplier = spi_data->mck_multp;
		lrck_width = spi_data->lrck_width;
#endif
	}

	if (mclk_multiplier > 0) {
		mclk = mclk_multiplier * params_rate(params);

		ret = snd_soc_dai_set_sysclk(cpu_dai, 0, mclk,
					     SND_SOC_CLOCK_OUT);
		if (!ret)
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
		if (!ret)
			return ret;
	}

	return 0;
}

static int mt8518_evb_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
	struct snd_pcm_hw_params *params)
{
	struct mt8518_evb_priv *priv = snd_soc_card_get_drvdata(rtd->card);
	int id = rtd->dai_link->id;
	struct mt8518_evb_etdm_ctrl_data *etdm;
	struct mt8518_evb_dmic_ctrl_data *dmic;
	struct mt8518_evb_multi_in_ctrl_data *multi_in;
#ifdef CONFIG_MTK_HIFI4DSP_SUPPORT
	struct mt8518_evb_spi_be_ctrl_data *spi_data;
#endif
	unsigned int fix_rate = 0;
	unsigned int fix_bit_width = 0;
	unsigned int fix_channels = 0;
	unsigned int idx;

	if (id == DAI_LINK_ETDM1_OUT) {
		idx = MT8518_ETDM1;
		etdm = &priv->etdm_data[idx];
		fix_rate = etdm->fix_rate_out;
		fix_bit_width = etdm->fix_bit_width_out;
		fix_channels = etdm->fix_channels_out;
	} else if (id == DAI_LINK_ETDM1_IN) {
		idx = MT8518_ETDM1;
		etdm = &priv->etdm_data[idx];
		fix_rate = etdm->fix_rate_in;
		fix_bit_width = etdm->fix_bit_width_in;
		fix_channels = etdm->fix_channels_in;
	} else if (id == DAI_LINK_ETDM2_OUT) {
		idx = MT8518_ETDM2;
		etdm = &priv->etdm_data[idx];
		fix_rate = etdm->fix_rate_out;
		fix_bit_width = etdm->fix_bit_width_out;
		fix_channels = etdm->fix_channels_out;
	} else if (id == DAI_LINK_ETDM2_IN) {
		idx = MT8518_ETDM2;
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
#ifdef CONFIG_MTK_HIFI4DSP_SUPPORT
	} else if (id == DAI_LINK_SPI_MIC_BE) {
		idx = MT8518_ADSP_BE_MIC_RECORD - MT8518_ADSP_BE_START;
		spi_data = &priv->spi_be_data[idx];
		fix_rate = spi_data->fix_rate;
		fix_bit_width = spi_data->fix_bit_width;
		fix_channels = spi_data->fix_channels;
	} else if (id == DAI_LINK_SPI_PRIMARY_PLAYBACK_BE) {
		idx = MT8518_ADSP_BE_PRIMARY_PLAYBACK - MT8518_ADSP_BE_START;
		spi_data = &priv->spi_be_data[idx];
		fix_rate = spi_data->fix_rate;
		fix_bit_width = spi_data->fix_bit_width;
		fix_channels = spi_data->fix_channels;
#endif
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

static struct snd_soc_ops mt8518_evb_etdm_ops = {
	.hw_params = mt8518_evb_hw_params,
};

#ifdef CONFIG_MTK_HIFI4DSP_SUPPORT
#if defined(CONFIG_SND_SOC_MT8518_ADSP_PCM_PLAYBACK) || \
	defined(CONFIG_SND_SOC_COMPRESS)
static struct snd_soc_ops mt8518_evb_spi_be_ops = {
	.hw_params = mt8518_evb_hw_params,
};
#endif
#endif

#define RSV_DAI_LNIK(x) \
{ \
	.name = #x "_FE", \
	.stream_name = #x, \
	.cpu_dai_name = "snd-soc-dummy-dai", \
	.codec_name = "snd-soc-dummy", \
	.codec_dai_name = "snd-soc-dummy-dai", \
	.platform_name = "snd-soc-dummy" \
}

/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link mt8518_evb_dais[] = {
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
#ifdef CONFIG_SND_SOC_GAPP_AUDIO_CONTROL
		.ops = &dlm_playback_ops,
#endif
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
	[DAI_LINK_DL8_PLAYBACK] = {
		.name = "DL8_FE",
		.stream_name = "DL8 Playback",
		.cpu_dai_name = "DL8",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = DAI_LINK_DL8_PLAYBACK,
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
	[DAI_LINK_DL7_PLAYBACK] = {
		.name = "DL7_FE",
		.stream_name = "DL7 Playback",
		.cpu_dai_name = "DL7",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = DAI_LINK_DL7_PLAYBACK,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_PRE,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.dynamic = 1,
		.dpcm_playback = 1,
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
#ifdef CONFIG_MTK_HIFI4DSP_SUPPORT
#ifdef CONFIG_SND_SOC_MT8518_ADSP_VOICE_ASSIST
	[DAI_LINK_VA_HOSTLESS] = {
		.name = "VA_HL_FE",
		.stream_name = "VA Hostless FrontEnd",
		.cpu_dai_name = "FE_VA_HL",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = DAI_LINK_VA_HOSTLESS,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST
		},
		.dynamic = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
	},
#else
	[DAI_LINK_VA_HOSTLESS] = RSV_DAI_LNIK(SPI_RSV0),
#endif
	[DAI_LINK_SPI_MIC_CAPTURE] = {
		.name = "SPI_MIC_FE",
		.stream_name = "SPI MIC Capture",
		.cpu_dai_name = "FE_MICR",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = DAI_LINK_SPI_MIC_CAPTURE,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST
		},
		.dynamic = 1,
		.dpcm_capture = 1,
	},
#ifdef CONFIG_SND_SOC_MT8518_ADSP_VOICE_ASSIST
	[DAI_LINK_VA_UPLOAD] = {
		.name = "VA_UL_FE",
		.stream_name = "VA Upload Capture",
		.cpu_dai_name = "FE_VA_UL",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = DAI_LINK_VA_UPLOAD,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST
		},
		.dynamic = 1,
		.dpcm_capture = 1,
	},
#else
	[DAI_LINK_VA_UPLOAD] = RSV_DAI_LNIK(SPI_RSV1),
#endif
	[DAI_LINK_SPI_RESERVE2] = RSV_DAI_LNIK(SPI_RSV2),
	[DAI_LINK_SPI_RESERVE3] = RSV_DAI_LNIK(SPI_RSV3),
	[DAI_LINK_SPI_RESERVE4] = RSV_DAI_LNIK(SPI_RSV4),
#ifdef CONFIG_SND_SOC_MT8518_ADSP_PCM_PLAYBACK
	[DAI_LINK_SPI_PCM_PLAYBACK1] = {
		.name = "PCMP1 FE",
		.stream_name = "PCMP1",
		.cpu_dai_name = "FE_PCMP1",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = DAI_LINK_SPI_PCM_PLAYBACK1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST
		},
		.dynamic = 1,
		.dpcm_playback = 1,
	},
#else
	[DAI_LINK_SPI_PCM_PLAYBACK1] = RSV_DAI_LNIK(PCMP_RSV1),
#endif
	[DAI_LINK_SPI_RESERVE6] = RSV_DAI_LNIK(SPI_RSV6),
	[DAI_LINK_SPI_RESERVE7] = RSV_DAI_LNIK(SPI_RSV7),
	[DAI_LINK_SPI_RESERVE8] = RSV_DAI_LNIK(SPI_RSV8),
#ifdef CONFIG_SND_SOC_COMPRESS
#if CONFIG_SND_SOC_COMPRESS_NR_PLAYBACK_STREAMS > 0
	[DAI_LINK_COMPRP_FE1] = {
		.name = "CompressedP FE1",
		.stream_name = "CompressedP1",
		.cpu_dai_name = "FE_COMPRP1",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = DAI_LINK_COMPRP_FE1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST
		},
		.dynamic = 1,
		.dpcm_playback = 1,
	},
#else
	[DAI_LINK_COMPRP_FE1] = RSV_DAI_LNIK(COMRPP_RSV1),
#endif
#if CONFIG_SND_SOC_COMPRESS_NR_PLAYBACK_STREAMS > 1
	[DAI_LINK_COMPRP_FE2] = {
		.name = "CompressedP FE2",
		.stream_name = "CompressedP2",
		.cpu_dai_name = "FE_COMPRP2",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = DAI_LINK_COMPRP_FE2,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST
		},
		.dynamic = 1,
		.dpcm_playback = 1,
	},
#else
	[DAI_LINK_COMPRP_FE2] = RSV_DAI_LNIK(COMRPP_RSV2),
#endif
#if CONFIG_SND_SOC_COMPRESS_NR_PLAYBACK_STREAMS > 2
	[DAI_LINK_COMPRP_FE3] = {
		.name = "CompressedP FE3",
		.stream_name = "CompressedP3",
		.cpu_dai_name = "FE_COMPRP3",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = DAI_LINK_COMPRP_FE3,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST
		},
		.dynamic = 1,
		.dpcm_playback = 1,
	},
#else
	[DAI_LINK_COMPRP_FE3] = RSV_DAI_LNIK(COMRPP_RSV3),
#endif
#else
	[DAI_LINK_COMPRP_FE1] = RSV_DAI_LNIK(COMRPP_RSV1),
	[DAI_LINK_COMPRP_FE2] = RSV_DAI_LNIK(COMRPP_RSV2),
	[DAI_LINK_COMPRP_FE3] = RSV_DAI_LNIK(COMRPP_RSV3),
#endif
#endif
	/* Back End DAI links */
	[DAI_LINK_ETDM1_OUT] = {
		.name = "ETDM1_OUT BE",
		.cpu_dai_name = "ETDM1_OUT",
		.no_pcm = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = DAI_LINK_ETDM1_OUT,
		.dai_fmt = SND_SOC_DAIFMT_I2S |
			   SND_SOC_DAIFMT_NB_NF |
			   SND_SOC_DAIFMT_CBS_CFS,
		.ops = &mt8518_evb_etdm_ops,
		.dpcm_playback = 1,
	},
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
		.ops = &mt8518_evb_etdm_ops,
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
		.ops = &mt8518_evb_etdm_ops,
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
		.ops = &mt8518_evb_etdm_ops,
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
	[DAI_LINK_SPDIF_OUT] = {
		.name = "SPDIF_OUT BE",
		.cpu_dai_name = "SPDIF_OUT",
		.no_pcm = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = DAI_LINK_SPDIF_OUT,
		.dpcm_playback = 1,
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
#ifdef CONFIG_MTK_HIFI4DSP_SUPPORT
	[DAI_LINK_SPI_MIC_BE] = {
		.name = "SPI MIC BE",
		.cpu_dai_name = "BE_MICR",
		.no_pcm = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = DAI_LINK_SPI_MIC_BE,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
	},
#if defined(CONFIG_SND_SOC_MT8518_ADSP_PCM_PLAYBACK) || \
	defined(CONFIG_SND_SOC_COMPRESS)
	[DAI_LINK_SPI_PRIMARY_PLAYBACK_BE] = {
		.name = "Primary Playback BE",
		.cpu_dai_name = "BE_PRIP",
		.no_pcm = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = DAI_LINK_SPI_PRIMARY_PLAYBACK_BE,
		.dai_fmt = SND_SOC_DAIFMT_I2S |
			   SND_SOC_DAIFMT_NB_NF |
			   SND_SOC_DAIFMT_CBM_CFM,
		.ops = &mt8518_evb_spi_be_ops,
		.dpcm_playback = 1,
	},
#endif
#endif
};

static const struct snd_kcontrol_new mt8518_evb_controls[] = {
#ifdef CONFIG_SND_SOC_GAPP_AUDIO_CONTROL
	SOC_SINGLE_EXT("Master Volume 1", 0, 0, 100, 0,
		       soc_ctlx_get, soc_ctlx_put),
	SOC_SINGLE_EXT("Master Volume X", 0, 0, 100, 0,
		       soc_ctlx_get, soc_ctlx_put),
	SOC_SINGLE_BOOL_EXT("Master Switch", 0,
			    soc_ctlx_get, soc_ctlx_put),
	SOC_SINGLE_BOOL_EXT("Master Switch X", 0,
			    soc_ctlx_get, soc_ctlx_put),
	SOC_ENUM_EXT("PCM State", pcm_state_enums,
		     soc_pcm_state_get, soc_pcm_state_put),
	SOC_ENUM_EXT("PCM State X", pcm_state_enums,
		     soc_pcm_state_get, 0),
#endif
};

static int mt8518_evb_gpio_probe(struct snd_soc_card *card)
{
	struct mt8518_evb_priv *priv = snd_soc_card_get_drvdata(card);
	int ret = 0;
	int i;

	priv->pinctrl = devm_pinctrl_get(card->dev);
	if (IS_ERR(priv->pinctrl)) {
		ret = PTR_ERR(priv->pinctrl);
		dev_err(card->dev, "%s devm_pinctrl_get failed %d\n",
			__func__, ret);
		return ret;
	}

	for (i = 0 ; i < PIN_STATE_MAX ; i++) {
		priv->pin_states[i] = pinctrl_lookup_state(priv->pinctrl,
			mt8518_evb_pin_str[i]);
		if (IS_ERR(priv->pin_states[i])) {
			ret = PTR_ERR(priv->pin_states[i]);
			dev_dbg(card->dev, "%s Can't find pin state %s %d\n",
				 __func__, mt8518_evb_pin_str[i], ret);
		}
	}

	if (IS_ERR(priv->pin_states[PIN_STATE_DEFAULT])) {
		dev_err(card->dev, "%s can't find default pin state\n",
			__func__);
		return 0;
	}

	/* default state */
	ret = pinctrl_select_state(priv->pinctrl,
				   priv->pin_states[PIN_STATE_DEFAULT]);
	if (ret)
		dev_err(card->dev, "%s failed to select state %d\n",
			__func__, ret);

	/* turn off ext amp if exist */
	if (!IS_ERR(priv->pin_states[PIN_STATE_EXTAMP_OFF])) {
		ret = pinctrl_select_state(priv->pinctrl,
			priv->pin_states[PIN_STATE_EXTAMP_OFF]);
		if (ret)
			dev_dbg(card->dev,
				"%s failed to select state %d\n",
				__func__, ret);
	}

	return ret;
}

static void mt8518_evb_parse_of(struct snd_soc_card *card,
				struct device_node *np)
{
	struct mt8518_evb_priv *priv = snd_soc_card_get_drvdata(card);
	size_t i, j;
	int ret;
	char prop[128];
	const char *str;
	unsigned int val;

	static const struct mt8518_dai_link_prop of_dai_links_daifmt[] = {
		{ "dlm",	DAI_LINK_DLM_PLAYBACK },
		{ "dl2",	DAI_LINK_DL2_PLAYBACK },
		{ "dl3",	DAI_LINK_DL3_PLAYBACK },
		{ "dl6",	DAI_LINK_DL6_PLAYBACK },
		{ "dl8",	DAI_LINK_DL8_PLAYBACK },
		{ "ul2",	DAI_LINK_UL2_CAPTURE },
		{ "ul3",	DAI_LINK_UL3_CAPTURE },
		{ "ul4",	DAI_LINK_UL4_CAPTURE },
		{ "ul5",	DAI_LINK_UL5_CAPTURE },
		{ "ul8",	DAI_LINK_UL8_CAPTURE },
		{ "ul9",	DAI_LINK_UL9_CAPTURE },
		{ "ul10",	DAI_LINK_UL10_CAPTURE },
		{ "etdm1-out",	DAI_LINK_ETDM1_OUT },
		{ "etdm1-in",	DAI_LINK_ETDM1_IN },
		{ "etdm2-out",	DAI_LINK_ETDM2_OUT },
		{ "etdm2-in",	DAI_LINK_ETDM2_IN },
		{ "pcm-intf",	DAI_LINK_PCM_INTF },
		{ "multi-in",	DAI_LINK_MULTI_IN },
	};

	static const struct mt8518_dai_link_prop of_dai_links_etdm[] = {
		{ "etdm1-out",	DAI_LINK_ETDM1_OUT },
		{ "etdm1-in",	DAI_LINK_ETDM1_IN },
		{ "etdm2-out",	DAI_LINK_ETDM2_OUT },
		{ "etdm2-in",	DAI_LINK_ETDM2_IN },
	};

	static const struct mt8518_dai_link_prop of_dai_links_dmic[] = {
		{ "dmic",	DAI_LINK_DMIC },
	};

#ifdef CONFIG_SND_SOC_MT8518_CODEC
	static const struct mt8518_dai_link_prop
		of_dai_links_int_adda[] = {
		{ "int-adda",	DAI_LINK_INT_ADDA },
	};
#endif

#ifdef CONFIG_MTK_HIFI4DSP_SUPPORT
	static const struct mt8518_dai_link_prop
		of_dai_links_spi_be[] = {
		{ "spi-mic",	DAI_LINK_SPI_MIC_BE },
		{ "prip-be",	DAI_LINK_SPI_PRIMARY_PLAYBACK_BE },
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
			&mt8518_evb_dais[of_dai_links_daifmt[i].link_id];
		bool lrck_inverse = false;
		bool bck_inverse = false;

		// parse format
		snprintf(prop, sizeof(prop), PREFIX"%s-format",
			 of_dai_links_daifmt[i].name);
		ret = of_property_read_string(np, prop, &str);
		if (ret == 0) {
			unsigned int format = 0;

			for (j = 0; j < ARRAY_SIZE(of_fmt_table); j++) {
				if (strcmp(str, of_fmt_table[i].name) == 0) {
					format |= of_fmt_table[i].val;
					break;
				}
			}

			dai_link->dai_fmt &= ~SND_SOC_DAIFMT_FORMAT_MASK;
			dai_link->dai_fmt |= format;
		}

		// parse clock mode
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

		// parse lrck inverse
		snprintf(prop, sizeof(prop), PREFIX"%s-lrck-inverse",
			 of_dai_links_daifmt[i].name);
		lrck_inverse = of_property_read_bool(np, prop);

		// parse bck inverse
		snprintf(prop, sizeof(prop), PREFIX"%s-bck-inverse",
			 of_dai_links_daifmt[i].name);
		bck_inverse = of_property_read_bool(np, prop);

		dai_link->dai_fmt &= ~SND_SOC_DAIFMT_INV_MASK;

		if (lrck_inverse && bck_inverse)
			dai_link->dai_fmt |= SND_SOC_DAIFMT_IB_IF;
		else if (lrck_inverse && !bck_inverse)
			dai_link->dai_fmt |= SND_SOC_DAIFMT_IB_NF;
		else if (!lrck_inverse && bck_inverse)
			dai_link->dai_fmt |= SND_SOC_DAIFMT_NB_IF;
		else
			dai_link->dai_fmt |= SND_SOC_DAIFMT_NB_NF;
	}

	for (i = 0; i < ARRAY_SIZE(of_dai_links_etdm); i++) {
		unsigned int link_id = of_dai_links_etdm[i].link_id;
		struct snd_soc_dai_link *dai_link = &mt8518_evb_dais[link_id];
		struct mt8518_evb_etdm_ctrl_data *etdm;
		struct device_node *codec_node;

		// parse mclk multiplier
		snprintf(prop, sizeof(prop), PREFIX"%s-mclk-multiplier",
			 of_dai_links_etdm[i].name);
		ret = of_property_read_u32(np, prop, &val);
		if (ret == 0) {
			switch (link_id) {
			case DAI_LINK_ETDM1_OUT:
				etdm = &priv->etdm_data[MT8518_ETDM1];
				etdm->mck_multp_out = val;
				break;
			case DAI_LINK_ETDM1_IN:
				etdm = &priv->etdm_data[MT8518_ETDM1];
				etdm->mck_multp_in = val;
				break;
			case DAI_LINK_ETDM2_OUT:
				etdm = &priv->etdm_data[MT8518_ETDM2];
				etdm->mck_multp_out = val;
				break;
			case DAI_LINK_ETDM2_IN:
				etdm = &priv->etdm_data[MT8518_ETDM2];
				etdm->mck_multp_in = val;
				break;
			default:
				break;
			}
		}

		// parse lrck width
		snprintf(prop, sizeof(prop), PREFIX"%s-lrck-width",
			 of_dai_links_etdm[i].name);
		ret = of_property_read_u32(np, prop, &val);
		if (ret == 0) {
			switch (link_id) {
			case DAI_LINK_ETDM1_OUT:
				etdm = &priv->etdm_data[MT8518_ETDM1];
				etdm->lrck_width_out = val;
				break;
			case DAI_LINK_ETDM1_IN:
				etdm = &priv->etdm_data[MT8518_ETDM1];
				etdm->lrck_width_in = val;
				break;
			case DAI_LINK_ETDM2_OUT:
				etdm = &priv->etdm_data[MT8518_ETDM2];
				etdm->lrck_width_out = val;
				break;
			case DAI_LINK_ETDM2_IN:
				etdm = &priv->etdm_data[MT8518_ETDM2];
				etdm->lrck_width_in = val;
				break;
			default:
				break;
			}
		}

		// parse fix rate
		snprintf(prop, sizeof(prop), PREFIX"%s-fix-rate",
			 of_dai_links_etdm[i].name);
		ret = of_property_read_u32(np, prop, &val);
		if (ret == 0 && mt8518_afe_rate_supported(val,
		    link_to_dai(link_id))) {
			switch (link_id) {
			case DAI_LINK_ETDM1_OUT:
				etdm = &priv->etdm_data[MT8518_ETDM1];
				etdm->fix_rate_out = val;
				break;
			case DAI_LINK_ETDM1_IN:
				etdm = &priv->etdm_data[MT8518_ETDM1];
				etdm->fix_rate_in = val;
				break;
			case DAI_LINK_ETDM2_OUT:
				etdm = &priv->etdm_data[MT8518_ETDM2];
				etdm->fix_rate_out = val;
				break;
			case DAI_LINK_ETDM2_IN:
				etdm = &priv->etdm_data[MT8518_ETDM2];
				etdm->fix_rate_in = val;
				break;
			default:
				break;
			}

			dai_link->be_hw_params_fixup =
				mt8518_evb_be_hw_params_fixup;
		}

		// parse fix bit width
		snprintf(prop, sizeof(prop), PREFIX"%s-fix-bit-width",
			 of_dai_links_etdm[i].name);
		ret = of_property_read_u32(np, prop, &val);
		if (ret == 0 && (val == 32 || val == 16)) {
			switch (link_id) {
			case DAI_LINK_ETDM1_OUT:
				etdm = &priv->etdm_data[MT8518_ETDM1];
				etdm->fix_bit_width_out = val;
				break;
			case DAI_LINK_ETDM1_IN:
				etdm = &priv->etdm_data[MT8518_ETDM1];
				etdm->fix_bit_width_in = val;
				break;
			case DAI_LINK_ETDM2_OUT:
				etdm = &priv->etdm_data[MT8518_ETDM2];
				etdm->fix_bit_width_out = val;
				break;
			case DAI_LINK_ETDM2_IN:
				etdm = &priv->etdm_data[MT8518_ETDM2];
				etdm->fix_bit_width_in = val;
				break;
			default:
				break;
			}

			dai_link->be_hw_params_fixup =
				mt8518_evb_be_hw_params_fixup;
		}

		// parse fix channels
		snprintf(prop, sizeof(prop), PREFIX"%s-fix-channels",
			 of_dai_links_etdm[i].name);
		ret = of_property_read_u32(np, prop, &val);
		if (ret == 0 && mt8518_afe_channel_supported(val,
		    link_to_dai(link_id))) {
			switch (link_id) {
			case DAI_LINK_ETDM1_OUT:
				etdm = &priv->etdm_data[MT8518_ETDM1];
				etdm->fix_channels_out = val;
				break;
			case DAI_LINK_ETDM1_IN:
				etdm = &priv->etdm_data[MT8518_ETDM1];
				etdm->fix_channels_in = val;
				break;
			case DAI_LINK_ETDM2_OUT:
				etdm = &priv->etdm_data[MT8518_ETDM2];
				etdm->fix_channels_out = val;
				break;
			case DAI_LINK_ETDM2_IN:
				etdm = &priv->etdm_data[MT8518_ETDM2];
				etdm->fix_channels_in = val;
				break;
			default:
				break;
			}

			dai_link->be_hw_params_fixup =
				mt8518_evb_be_hw_params_fixup;
		}

		// parse codec_of_node
		snprintf(prop, sizeof(prop), PREFIX"%s-audio-codec",
			 of_dai_links_etdm[i].name);
		codec_node = of_parse_phandle(np, prop, 0);
		if (codec_node) {
			dai_link->codec_of_node = codec_node;
			dai_link->codec_name = NULL;
		}

		// parse codec dai name
		snprintf(prop, sizeof(prop), PREFIX"%s-codec-dai-name",
			 of_dai_links_etdm[i].name);
		of_property_read_string(np, prop, &dai_link->codec_dai_name);

		// parse ignore pmdown time
		snprintf(prop, sizeof(prop), PREFIX"%s-ignore-pmdown-time",
			 of_dai_links_etdm[i].name);
		if (of_property_read_bool(np, prop))
			dai_link->ignore_pmdown_time = 1;

		// parse ignore suspend
		snprintf(prop, sizeof(prop), PREFIX"%s-ignore-suspend",
			 of_dai_links_etdm[i].name);
		if (of_property_read_bool(np, prop))
			dai_link->ignore_suspend = 1;
	}

	for (i = 0; i < ARRAY_SIZE(of_dai_links_dmic); i++) {
		unsigned int link_id = of_dai_links_dmic[i].link_id;
		struct snd_soc_dai_link *dai_link = &mt8518_evb_dais[link_id];
		struct mt8518_evb_dmic_ctrl_data *dmic;

		// parse fix rate
		snprintf(prop, sizeof(prop), PREFIX"%s-fix-rate",
			 of_dai_links_dmic[i].name);
		ret = of_property_read_u32(np, prop, &val);
		if (ret == 0 && mt8518_afe_rate_supported(val,
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
				mt8518_evb_be_hw_params_fixup;
		}

		// parse fix bit width
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
				mt8518_evb_be_hw_params_fixup;
		}

		// parse fix channels
		snprintf(prop, sizeof(prop), PREFIX"%s-fix-channels",
			 of_dai_links_dmic[i].name);
		ret = of_property_read_u32(np, prop, &val);
		if (ret == 0 && mt8518_afe_channel_supported(val,
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
				mt8518_evb_be_hw_params_fixup;
		}
	}

	// parse fix bit width
	snprintf(prop, sizeof(prop), PREFIX"%s-fix-bit-width", "multi-in");
	ret = of_property_read_u32(np, prop, &val);
	if (ret == 0 && (val == 32 || val == 16)) {
		priv->multi_in_data.fix_bit_width = val;
		mt8518_evb_dais[DAI_LINK_MULTI_IN].be_hw_params_fixup =
			mt8518_evb_be_hw_params_fixup;
	}

#ifdef CONFIG_SND_SOC_MT8518_CODEC
	for (i = 0; i < ARRAY_SIZE(of_dai_links_int_adda); i++) {
		unsigned int link_id = of_dai_links_int_adda[i].link_id;
		struct snd_soc_dai_link *dai_link = &mt8518_evb_dais[link_id];
		struct device_node *codec_node;

		// parse codec_of_node
		snprintf(prop, sizeof(prop), PREFIX"%s-audio-codec",
			 of_dai_links_int_adda[i].name);
		codec_node = of_parse_phandle(np, prop, 0);

		if (codec_node) {
			dai_link->codec_of_node = codec_node;
			dai_link->codec_name = NULL;
		}

		// parse codec dai name
		snprintf(prop, sizeof(prop), PREFIX"%s-codec-dai-name",
			 of_dai_links_int_adda[i].name);
		of_property_read_string(np, prop, &dai_link->codec_dai_name);
	}
#endif

#ifdef CONFIG_MTK_HIFI4DSP_SUPPORT
	for (i = 0; i < ARRAY_SIZE(of_dai_links_spi_be); i++) {
		unsigned int link_id = of_dai_links_spi_be[i].link_id;
		struct snd_soc_dai_link *dai_link = &mt8518_evb_dais[link_id];
		struct mt8518_evb_spi_be_ctrl_data *spi_data;
		struct device_node *codec_node;
		unsigned int idx;

		// parse mclk multiplier
		snprintf(prop, sizeof(prop), PREFIX"%s-mclk-multiplier",
			 of_dai_links_spi_be[i].name);
		ret = of_property_read_u32(np, prop, &val);
		if (ret == 0) {
			switch (link_id) {
			case DAI_LINK_SPI_PRIMARY_PLAYBACK_BE:
				idx = MT8518_ADSP_BE_PRIMARY_PLAYBACK -
				      MT8518_ADSP_BE_START;
				spi_data = &priv->spi_be_data[idx];
				spi_data->mck_multp = val;
				break;
			default:
				break;
			}
		}

		// parse lrck width
		snprintf(prop, sizeof(prop), PREFIX"%s-lrck-width",
			 of_dai_links_spi_be[i].name);
		ret = of_property_read_u32(np, prop, &val);
		if (ret == 0) {
			switch (link_id) {
			case DAI_LINK_SPI_PRIMARY_PLAYBACK_BE:
				idx = MT8518_ADSP_BE_PRIMARY_PLAYBACK -
				      MT8518_ADSP_BE_START;
				spi_data = &priv->spi_be_data[idx];
				spi_data->lrck_width = val;
				break;
			default:
				break;
			}
		}

		// parse fix rate
		snprintf(prop, sizeof(prop), PREFIX"%s-fix-rate",
			 of_dai_links_spi_be[i].name);
		ret = of_property_read_u32(np, prop, &val);
		if (ret == 0) {
			switch (link_id) {
			case DAI_LINK_SPI_MIC_BE:
				idx = MT8518_ADSP_BE_MIC_RECORD -
				      MT8518_ADSP_BE_START;
				spi_data = &priv->spi_be_data[idx];
				spi_data->fix_rate = val;
				break;
			case DAI_LINK_SPI_PRIMARY_PLAYBACK_BE:
				idx = MT8518_ADSP_BE_PRIMARY_PLAYBACK -
				      MT8518_ADSP_BE_START;
				spi_data = &priv->spi_be_data[idx];
				spi_data->fix_rate = val;
				break;
			default:
				break;
			}

			dai_link->be_hw_params_fixup =
				mt8518_evb_be_hw_params_fixup;
		}

		// parse fix bit width
		snprintf(prop, sizeof(prop), PREFIX"%s-fix-bit-width",
			 of_dai_links_spi_be[i].name);
		ret = of_property_read_u32(np, prop, &val);
		if (ret == 0 && (val == 32 || val == 16)) {
			switch (link_id) {
			case DAI_LINK_SPI_MIC_BE:
				idx = MT8518_ADSP_BE_MIC_RECORD -
				      MT8518_ADSP_BE_START;
				spi_data = &priv->spi_be_data[idx];
				spi_data->fix_bit_width = val;
				break;
			case DAI_LINK_SPI_PRIMARY_PLAYBACK_BE:
				idx = MT8518_ADSP_BE_PRIMARY_PLAYBACK -
				      MT8518_ADSP_BE_START;
				spi_data = &priv->spi_be_data[idx];
				spi_data->fix_bit_width = val;
				break;
			default:
				break;
			}

			dai_link->be_hw_params_fixup =
				mt8518_evb_be_hw_params_fixup;
		}

		// parse fix channels
		snprintf(prop, sizeof(prop), PREFIX"%s-fix-channels",
			 of_dai_links_spi_be[i].name);
		ret = of_property_read_u32(np, prop, &val);
		if (ret == 0) {
			switch (link_id) {
			case DAI_LINK_SPI_MIC_BE:
				idx = MT8518_ADSP_BE_MIC_RECORD -
				      MT8518_ADSP_BE_START;
				spi_data = &priv->spi_be_data[idx];
				spi_data->fix_channels = val;
				break;
			case DAI_LINK_SPI_PRIMARY_PLAYBACK_BE:
				idx = MT8518_ADSP_BE_PRIMARY_PLAYBACK -
				      MT8518_ADSP_BE_START;
				spi_data = &priv->spi_be_data[idx];
				spi_data->fix_channels = val;
				break;
			default:
				break;
			}

			dai_link->be_hw_params_fixup =
				mt8518_evb_be_hw_params_fixup;
		}

		// parse codec_of_node
		snprintf(prop, sizeof(prop), PREFIX"%s-audio-codec",
			 of_dai_links_spi_be[i].name);
		codec_node = of_parse_phandle(np, prop, 0);

		if (codec_node) {
			dai_link->codec_of_node = codec_node;
			dai_link->codec_name = NULL;
		}

		// parse codec dai name
		snprintf(prop, sizeof(prop), PREFIX"%s-codec-dai-name",
			 of_dai_links_spi_be[i].name);
		of_property_read_string(np, prop, &dai_link->codec_dai_name);
	}
#endif
}

static struct snd_soc_card mt8518_evb_card = {
	.name = "mt8518-evb-card",
	.owner = THIS_MODULE,
	.dai_link = mt8518_evb_dais,
	.num_links = ARRAY_SIZE(mt8518_evb_dais),
	.controls = mt8518_evb_controls,
	.num_controls = ARRAY_SIZE(mt8518_evb_controls),
	.dapm_widgets = mt8518_evb_widgets,
	.num_dapm_widgets = ARRAY_SIZE(mt8518_evb_widgets),
	.dapm_routes = mt8518_evb_routes,
	.num_dapm_routes = ARRAY_SIZE(mt8518_evb_routes),
};

static int mt8518_evb_dev_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &mt8518_evb_card;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *afe_plat_node;
#ifdef CONFIG_MTK_HIFI4DSP_SUPPORT
	struct device_node *spi_plat_node;
#endif
	struct mt8518_evb_priv *priv;
	int ret, id;
	size_t i;
	size_t dais_num = ARRAY_SIZE(mt8518_evb_dais);

	afe_plat_node = of_parse_phandle(dev->of_node, "mediatek,platform", 0);
	if (!afe_plat_node) {
		dev_info(dev, "Property 'platform' missing or invalid\n");
		return -EINVAL;
	}

#ifdef CONFIG_MTK_HIFI4DSP_SUPPORT
	spi_plat_node = of_parse_phandle(dev->of_node,
		"mediatek,spi-platform", 0);
	if (!spi_plat_node) {
		dev_info(dev, "Property 'spi-platform' missing or invalid\n");
		return -EINVAL;
	}
#endif

	for (i = 0; i < dais_num; i++) {
		if (mt8518_evb_dais[i].platform_name)
			continue;

		id = mt8518_evb_dais[i].id;

		if ((id >= DAI_LINK_AFE_FE_BASE &&
		     id <= DAI_LINK_AFE_FE_END) ||
		    (id >= DAI_LINK_AFE_BE_BASE &&
		     id <= DAI_LINK_AFE_BE_END)) {
			mt8518_evb_dais[i].platform_of_node = afe_plat_node;
		}

#ifdef CONFIG_MTK_HIFI4DSP_SUPPORT
		if ((id >= DAI_LINK_SPI_FE_BASE &&
		     id <= DAI_LINK_SPI_FE_END) ||
		    (id >= DAI_LINK_SPI_BE_BASE &&
		     id <= DAI_LINK_SPI_BE_END) ||
		    (id >= DAI_LINK_COMPRP_FE_BASE &&
		     id <= DAI_LINK_COMPRP_FE_END)) {
			mt8518_evb_dais[i].platform_of_node = spi_plat_node;
		}
#endif
	}

	card->dev = dev;

	priv = devm_kzalloc(dev, sizeof(struct mt8518_evb_priv),
			    GFP_KERNEL);
	if (!priv) {
		ret = -ENOMEM;
		dev_err(dev, "%s allocate card private data fail %d\n",
			__func__, ret);
		return ret;
	}

	snd_soc_card_set_drvdata(card, priv);

	mt8518_evb_gpio_probe(card);

	mt8518_evb_parse_of(card, np);

	ret = devm_snd_soc_register_card(dev, card);
	if (ret) {
		dev_err(dev, "%s snd_soc_register_card fail %d\n",
			__func__, ret);
		return ret;
	}

#ifdef CONFIG_SND_SOC_GAPP_AUDIO_CONTROL
	soc_ctlx_init(&priv->ctlx_res, card);
#endif

	return ret;
}

static const struct of_device_id mt8518_evb_dt_match[] = {
	{ .compatible = "mediatek,mt8518-evb", },
	{ }
};
MODULE_DEVICE_TABLE(of, mt8518_evb_dt_match);

static struct platform_driver mt8518_evb_driver = {
	.driver = {
		   .name = "mt8518-evb",
		   .of_match_table = mt8518_evb_dt_match,
#ifdef CONFIG_PM
		   .pm = &snd_soc_pm_ops,
#endif
	},
	.probe = mt8518_evb_dev_probe,
};

module_platform_driver(mt8518_evb_driver);

/* Module information */
MODULE_DESCRIPTION("MT8518 EVB SoC machine driver");
MODULE_AUTHOR("Hidalgo Huang <hidalgo.huang@mediatek.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:mt8518-evb");

