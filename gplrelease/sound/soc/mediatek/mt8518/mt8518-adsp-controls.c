/*
 * mt8518-adsp-controls.c  --  Mediatek 8518 adsp controls
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

#include "mt8518-adsp-controls.h"
#include "mt8518-adsp-common.h"
#include <linux/string.h>
#include <sound/soc.h>


static int mt8518_adsp_keyword_detect_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mt8518_audio_spi_priv *priv =
		snd_soc_platform_get_drvdata(plat);
	struct mt8518_adsp_control_data *data = &priv->ctrl_data;

	ucontrol->value.integer.value[0] = data->wakeword_detect_en;

	return 0;
}

static int mt8518_adsp_keyword_detect_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mt8518_audio_spi_priv *priv =
		snd_soc_platform_get_drvdata(plat);
	struct mt8518_adsp_control_data *data = &priv->ctrl_data;

	data->wakeword_detect_en = (ucontrol->value.integer.value[0]) ?
				   true : false;

	return 0;
}

#if defined(CONFIG_SND_SOC_MT8518_ADSP_PCM_PLAYBACK) || \
	defined(CONFIG_SND_SOC_COMPRESS)
static int mt8518_adsp_master_volume_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mt8518_audio_spi_priv *priv =
		snd_soc_platform_get_drvdata(plat);
	struct mt8518_adsp_control_data *data = &priv->ctrl_data;

	ucontrol->value.integer.value[0] = data->master_vol;

	return 0;
}

static int mt8518_adsp_master_volume_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mt8518_audio_spi_priv *priv =
		snd_soc_platform_get_drvdata(plat);
	struct dsp_vol_info *vol_info = &priv->adsp_vol.master;
	struct mt8518_adsp_control_data *data = &priv->ctrl_data;
	struct ipi_msg_t ipi_msg;
	int level = ucontrol->value.integer.value[0];

	if (level > HOST_VOLUME_MAX_LEVEL || level < 0)
		return -EINVAL;

	mt8518_adsp_host_to_dsp_vol(level, vol_info);

	memset(&ipi_msg, 0, sizeof(ipi_msg));

	dev_dbg(priv->dev, "%s level %d vol %d\n",
		__func__, level, vol_info->master_vol);

	mt8518_adsp_send_ipi_cmd(&ipi_msg,
		TASK_SCENE_AUDIO_CONTROLLER,
		AUDIO_IPI_LAYER_TO_DSP,
		AUDIO_IPI_MSG_ONLY,
		AUDIO_IPI_MSG_NEED_ACK,
		MSG_TO_DSP_DSP_SET_VOLUME,
		vol_info->master_vol,
		TASK_SCENE_MIXER_PRIMARY, NULL);

	data->master_vol = level;

	return 0;
}
#endif

#ifdef CONFIG_SND_SOC_COMPRESS
static int get_compress_volume_index(const char *ctl_name)
{
	int index = -1;

#if CONFIG_SND_SOC_COMPRESS_NR_PLAYBACK_STREAMS > 0
	if (strstr(ctl_name, "Volume 1")) {
		index = MT8518_ADSP_FE_COMPR_PLAYBACK1 -
			MT8518_ADSP_FE_COMPR_BASE;
		return index;
	}
#endif

#if CONFIG_SND_SOC_COMPRESS_NR_PLAYBACK_STREAMS > 1
	if (strstr(ctl_name, "Volume 2")) {
		index = MT8518_ADSP_FE_COMPR_PLAYBACK2 -
			MT8518_ADSP_FE_COMPR_BASE;
		return index;
	}
#endif

#if CONFIG_SND_SOC_COMPRESS_NR_PLAYBACK_STREAMS > 2
	if (strstr(ctl_name, "Volume 3")) {
		index = MT8518_ADSP_FE_COMPR_PLAYBACK3 -
			MT8518_ADSP_FE_COMPR_BASE;
		return index;
	}
#endif

	return index;
}

static int mt8518_adsp_compress_volume_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mt8518_audio_spi_priv *priv =
		snd_soc_platform_get_drvdata(plat);
	struct mt8518_adsp_control_data *data = &priv->ctrl_data;
	int index;

	index = get_compress_volume_index(kcontrol->id.name);

	if (index < 0 || index > MT8518_ADSP_FE_COMPR_CNT)
		return -EINVAL;

	ucontrol->value.integer.value[0] = data->compress_vol[index];

	return 0;
}

static int mt8518_adsp_compress_volume_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mt8518_audio_spi_priv *priv =
		snd_soc_platform_get_drvdata(plat);
	struct mt8518_adsp_control_data *data = &priv->ctrl_data;
	struct dsp_vol_info *vol_info;
	struct ipi_msg_t ipi_msg;
	int level = ucontrol->value.integer.value[0];
	int index;
	int scene;

	if (level > HOST_VOLUME_MAX_LEVEL || level < 0)
		return -EINVAL;

	index = get_compress_volume_index(kcontrol->id.name);

	if (index < 0 || index > MT8518_ADSP_FE_COMPR_CNT)
		return -EINVAL;

	vol_info = &priv->adsp_vol.compress[index];

	mt8518_adsp_host_to_dsp_vol(level, vol_info);

	scene =	mt8518_adsp_get_scene_by_dai_id(MT8518_ADSP_FE_COMPR_BASE +
		index);

	memset(&ipi_msg, 0, sizeof(ipi_msg));

	dev_dbg(priv->dev, "%s [#%d] level %d vol %d\n",
		__func__, scene, level, vol_info->input_vol);

	mt8518_adsp_send_ipi_cmd(&ipi_msg,
		TASK_SCENE_AUDIO_CONTROLLER,
		AUDIO_IPI_LAYER_TO_DSP,
		AUDIO_IPI_MSG_ONLY,
		AUDIO_IPI_MSG_NEED_ACK,
		MSG_TO_DSP_HOST_SET_VOLUME,
		vol_info->input_vol,
		scene, NULL);

	data->compress_vol[index] = level;

	return 0;
}
#endif


#ifdef CONFIG_SND_SOC_MT8518_ADSP_PCM_PLAYBACK
static int get_pcm_volume_index(const char *ctl_name)
{
	int index = -1;

	if (strstr(ctl_name, "Volume 1")) {
		index = MT8518_ADSP_FE_PCM_PLAYBACK1 -
			MT8518_ADSP_FE_PCM_BASE;
	}

	return index;
}

static int mt8518_adsp_pcm_volume_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mt8518_audio_spi_priv *priv =
		snd_soc_platform_get_drvdata(plat);
	struct mt8518_adsp_control_data *data = &priv->ctrl_data;
	int index;

	index = get_pcm_volume_index(kcontrol->id.name);

	if (index < 0 || index > MT8518_ADSP_FE_PCM_CNT)
		return -EINVAL;

	ucontrol->value.integer.value[0] = data->pcm_vol[index];

	return 0;
}

static int mt8518_adsp_pcm_volume_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mt8518_audio_spi_priv *priv =
		snd_soc_platform_get_drvdata(plat);
	struct mt8518_adsp_control_data *data = &priv->ctrl_data;
	struct dsp_vol_info *vol_info;
	struct ipi_msg_t ipi_msg;
	int level = ucontrol->value.integer.value[0];
	int index;
	int scene;

	if (level > HOST_VOLUME_MAX_LEVEL || level < 0)
		return -EINVAL;

	index = get_pcm_volume_index(kcontrol->id.name);

	if (index < 0 || index > MT8518_ADSP_FE_PCM_CNT)
		return -EINVAL;

	vol_info = &priv->adsp_vol.pcm[index];

	mt8518_adsp_host_to_dsp_vol(level, vol_info);

	scene =	mt8518_adsp_get_scene_by_dai_id(MT8518_ADSP_FE_PCM_BASE +
		index);

	memset(&ipi_msg, 0, sizeof(ipi_msg));

	dev_dbg(priv->dev, "%s [#%d] level %d vol %d\n",
		__func__, scene, level, vol_info->input_vol);

	mt8518_adsp_send_ipi_cmd(&ipi_msg,
		TASK_SCENE_AUDIO_CONTROLLER,
		AUDIO_IPI_LAYER_TO_DSP,
		AUDIO_IPI_MSG_ONLY,
		AUDIO_IPI_MSG_NEED_ACK,
		MSG_TO_DSP_HOST_SET_VOLUME,
		vol_info->input_vol,
		scene, NULL);

	data->pcm_vol[index] = level;

	return 0;
}
#endif

static const struct snd_kcontrol_new mt8518_adsp_controls[] = {
	SOC_SINGLE_BOOL_EXT(WWE_DETECT_STS_CTL_NAME,
		0,
		mt8518_adsp_keyword_detect_get,
		mt8518_adsp_keyword_detect_put),
#if defined(CONFIG_SND_SOC_MT8518_ADSP_PCM_PLAYBACK) || \
	defined(CONFIG_SND_SOC_COMPRESS)
	SOC_SINGLE_EXT("ADSP Master Volume",
		SND_SOC_NOPM, 0, HOST_VOLUME_MAX_LEVEL, 0,
		mt8518_adsp_master_volume_get,
		mt8518_adsp_master_volume_put),
#endif
#ifdef CONFIG_SND_SOC_MT8518_ADSP_PCM_PLAYBACK
	SOC_SINGLE_EXT("ADSP PCM Volume 1",
		SND_SOC_NOPM, 0, HOST_VOLUME_MAX_LEVEL, 0,
		mt8518_adsp_pcm_volume_get,
		mt8518_adsp_pcm_volume_put),
#endif
#ifdef CONFIG_SND_SOC_COMPRESS
#if CONFIG_SND_SOC_COMPRESS_NR_PLAYBACK_STREAMS > 0
	SOC_SINGLE_EXT("ADSP Compress Volume 1",
		SND_SOC_NOPM, 0, HOST_VOLUME_MAX_LEVEL, 0,
		mt8518_adsp_compress_volume_get,
		mt8518_adsp_compress_volume_put),
#endif
#if CONFIG_SND_SOC_COMPRESS_NR_PLAYBACK_STREAMS > 1
	SOC_SINGLE_EXT("ADSP Compress Volume 2",
		SND_SOC_NOPM, 0, HOST_VOLUME_MAX_LEVEL, 0,
		mt8518_adsp_compress_volume_get,
		mt8518_adsp_compress_volume_put),
#endif
#if CONFIG_SND_SOC_COMPRESS_NR_PLAYBACK_STREAMS > 2
	SOC_SINGLE_EXT("ADSP Compress Volume 3",
		SND_SOC_NOPM, 0, HOST_VOLUME_MAX_LEVEL, 0,
		mt8518_adsp_compress_volume_get,
		mt8518_adsp_compress_volume_put),
#endif
#endif
};

int mt8518_adsp_add_controls(struct snd_soc_platform *platform)
{
	return snd_soc_add_platform_controls(platform,
		mt8518_adsp_controls,
		ARRAY_SIZE(mt8518_adsp_controls));
}

