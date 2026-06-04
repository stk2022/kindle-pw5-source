/*
 * mt8518-adsp-utils.c  --  Mediatek 8518 adsp utility
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

#include "mt8518-adsp-utils.h"
#include "mt8518-afe-common.h"

#ifdef CONFIG_SND_SOC_MT8518_ADSP_VOICE_ASSIST
#define MT8518_ADSP_POWER_STATE_CONTROL_SUPPORT
#endif

int mt8518_adsp_get_scene_by_dai_id(int id)
{
	switch (id) {
	case MT8518_ADSP_FE_MIC_RECORD:
		/* FALLTHROUGH */
	case MT8518_ADSP_BE_MIC_RECORD:
		return TASK_SCENE_VA;
	case MT8518_ADSP_BE_PRIMARY_PLAYBACK:
		return TASK_SCENE_MIXER_PRIMARY;
#ifdef CONFIG_SND_SOC_MT8518_ADSP_VOICE_ASSIST
	case MT8518_ADSP_FE_VA_HOSTLESS:
		return TASK_SCENE_VA_HOSTLESS;
	case MT8518_ADSP_FE_VA_UPLOAD:
		return TASK_SCENE_VA_UPLOAD;
#endif
#ifdef CONFIG_SND_SOC_MT8518_ADSP_PCM_PLAYBACK
	case MT8518_ADSP_FE_PCM_PLAYBACK1:
		return TASK_SCENE_PCM_PLAYBACK;
#endif
#ifdef CONFIG_SND_SOC_COMPRESS
#if CONFIG_SND_SOC_COMPRESS_NR_PLAYBACK_STREAMS > 0
	case MT8518_ADSP_FE_COMPR_PLAYBACK1:
		return TASK_SCENE_COMPRESS_PLAYBACK1;
#endif
#if CONFIG_SND_SOC_COMPRESS_NR_PLAYBACK_STREAMS > 1
	case MT8518_ADSP_FE_COMPR_PLAYBACK2:
		return TASK_SCENE_COMPRESS_PLAYBACK2;
#endif
#if CONFIG_SND_SOC_COMPRESS_NR_PLAYBACK_STREAMS > 2
	case MT8518_ADSP_FE_COMPR_PLAYBACK3:
		return TASK_SCENE_COMPRESS_PLAYBACK3;
#endif
#endif
	default:
		break;
	}

	return -1;
}

bool mt8518_adsp_need_ul_dma_copy(int id)
{
	switch (id) {
	case MT8518_ADSP_FE_MIC_RECORD:
#ifdef CONFIG_SND_SOC_MT8518_ADSP_VOICE_ASSIST
	case MT8518_ADSP_FE_VA_UPLOAD:
#endif
		return true;
	default:
		break;
	}

	return false;
}

bool mt8518_adsp_need_dl_dma_copy(int id)
{
#ifdef CONFIG_SND_SOC_MT8518_ADSP_PCM_PLAYBACK
	switch (id) {
	case MT8518_ADSP_FE_PCM_PLAYBACK1:
		return true;
	default:
		break;
	}
#endif

	return false;
}

bool mt8518_adsp_is_compress_scene(int scene)
{
#ifdef CONFIG_SND_SOC_COMPRESS
	if (mt8518_adsp_is_compress_playback_scene(scene))
		return true;
#endif

	return false;
}

bool mt8518_adsp_is_compress_playback_scene(int scene)
{
#ifdef CONFIG_SND_SOC_COMPRESS
	switch (scene) {
#if CONFIG_SND_SOC_COMPRESS_NR_PLAYBACK_STREAMS > 0
	case TASK_SCENE_COMPRESS_PLAYBACK1:
#endif
#if CONFIG_SND_SOC_COMPRESS_NR_PLAYBACK_STREAMS > 1
	case TASK_SCENE_COMPRESS_PLAYBACK2:
#endif
#if CONFIG_SND_SOC_COMPRESS_NR_PLAYBACK_STREAMS > 2
	case TASK_SCENE_COMPRESS_PLAYBACK3:
#endif
		return true;
	default:
		break;
	}
#endif

	return false;
}

int mt8518_adsp_activate_compr(int id, atomic_t *active)
{
#ifdef CONFIG_SND_SOC_COMPRESS
	int index;

	if (!active)
		return -EINVAL;

	if (id < MT8518_ADSP_FE_COMPR_BASE ||
	    id > MT8518_ADSP_FE_COMPR_END)
		return -EINVAL;

	index = id - MT8518_ADSP_FE_COMPR_BASE;

	return atomic_add_return(1, &active[index]);
#else
	return 0;
#endif
}

int mt8518_adsp_deactivate_compr(int id, atomic_t *active)
{
#ifdef CONFIG_SND_SOC_COMPRESS
	int index;

	if (!active)
		return -EINVAL;

	if (id < MT8518_ADSP_FE_COMPR_BASE ||
	    id > MT8518_ADSP_FE_COMPR_END)
		return -EINVAL;

	index = id - MT8518_ADSP_FE_COMPR_BASE;

	if (atomic_read(&active[index]) > 0)
		return atomic_sub_return(1, &active[index]);
#endif

	return 0;
}

int mt8518_adsp_send_ipi_cmd(struct ipi_msg_t *p_msg,
			     uint8_t task_scene,
			     uint8_t target,
			     uint8_t data_type,
			     uint8_t ack_type,
			     uint16_t msg_id,
			     uint32_t param1,
			     uint32_t param2,
			     char *payload)
{
	struct ipi_msg_t ipi_msg;
	struct ipi_msg_t *msg;
	int ret = 0;

	if (p_msg) {
		msg = p_msg;
	} else {
		memset((void *)&ipi_msg, 0, sizeof(struct ipi_msg_t));
		msg = &ipi_msg;
	}

	ret = audio_send_ipi_msg(msg, task_scene, target, data_type,
				 ack_type, msg_id, param1, param2,
				 (char *)payload);
	if (ret != 0)
		pr_err("%s audio_send_ipi_msg (%d-%d-%d-%d) fail %d\n",
			__func__, task_scene, data_type,
			ack_type, msg_id, ret);

	return ret;
}

static int verify_common_hw_param(struct host_ipc_msg_hw_param *host,
	struct dsp_ipc_msg_hw_param *dsp)
{
	if ((host->sample_rate > 0) &&
	    (host->sample_rate != dsp->sample_rate)) {
		pr_notice("%s host/dsp rate %u/%u mismatch\n",
			  __func__, host->sample_rate, dsp->sample_rate);
		return -EINVAL;
	}

	if ((host->channel_num > 0) &&
	    (host->channel_num != dsp->channel_num)) {
		pr_notice("%s host/dsp ch %u/%u mismatch\n",
			  __func__, host->channel_num, dsp->channel_num);
		return -EINVAL;
	}

	if ((host->bitwidth > 0) &&
	    (host->bitwidth != dsp->bitwidth)) {
		pr_notice("%s host/dsp bitwidth %u/%u mismatch\n",
			  __func__, host->bitwidth, dsp->bitwidth);
		return -EINVAL;
	}

	if ((host->period_size > 0) &&
	    (host->period_size != dsp->period_size)) {
		pr_notice("%s host/dsp period_size %u/%u mismatch\n",
			  __func__, host->period_size, dsp->period_size);
		return -EINVAL;
	}

	if ((host->period_count > 0) &&
	    (host->period_count != dsp->period_count)) {
		pr_notice("%s host/dsp period_count %u/%u mismatch\n",
			  __func__, host->period_count, dsp->period_count);
		return -EINVAL;
	}

	return 0;
}

static int verify_compress_hw_param(struct host_ipc_msg_hw_param *host,
	struct dsp_ipc_msg_hw_param *dsp)
{
	struct audio_compressed_buffer *compress_buffer =
		&host->compr_params.aud_compr_buf;

	if ((compress_buffer->fragment_size > 0) &&
	    (compress_buffer->fragment_size != dsp->period_size)) {
		pr_notice("%s host/dsp fragment_size %u/%u mismatch\n",
			  __func__, compress_buffer->fragment_size,
			  dsp->period_size);
		return -EINVAL;
	}

	if ((compress_buffer->fragments > 0) &&
	    (compress_buffer->fragments != dsp->period_count)) {
		pr_notice("%s host/dsp fragments %u/%u mismatch\n",
			  __func__, compress_buffer->fragments,
			  dsp->period_count);
		return -EINVAL;
	}

	return 0;
}

int mt8518_adsp_verify_ack_hw_param(struct host_ipc_msg_hw_param *host_param,
	struct ipi_msg_t *ack_msg,
	struct dsp_ipc_msg_hw_param *dsp_param)
{
	int ret = 0;

	if (!host_param || !ack_msg)
		return -EINVAL;

	if (ack_msg->ack_type != AUDIO_IPI_MSG_ACK_BACK) {
		pr_info("%s unexpected ack type %u\n",
			__func__, ack_msg->ack_type);
		return -EINVAL;
	}

	if (!dsp_param)
		return ret;

	AUDIO_IPC_COPY_DSP_HW_PARAM(ack_msg->payload, dsp_param);

	if (mt8518_adsp_is_compress_scene(ack_msg->task_scene))
		ret = verify_compress_hw_param(host_param, dsp_param);
	else
		ret = verify_common_hw_param(host_param, dsp_param);

	return ret;
}

int mt8518_adsp_verify_ack_dbg_param(struct host_debug_start_param *host_param,
	struct ipi_msg_t *ack_msg,
	struct dsp_debug_start_param *dsp_param)
{
	int ret = 0;

	if (!host_param || !ack_msg)
		return -EINVAL;

	if (ack_msg->ack_type != AUDIO_IPI_MSG_ACK_BACK) {
		pr_info("%s unexpected ack type %u\n",
			__func__, ack_msg->ack_type);
		return -EINVAL;
	}

	if (!dsp_param)
		return ret;

	AUDIO_IPC_COPY_DSP_DEBUG_START_PARAM(ack_msg->payload, dsp_param);

	return 0;
}

int mt8518_adsp_verify_ack_vol_info(struct ipi_msg_t *ack_msg,
	struct dsp_vol_info *vol_info)
{
	int ret = 0;

	if (!ack_msg)
		return -EINVAL;

	if (ack_msg->ack_type != AUDIO_IPI_MSG_ACK_BACK) {
		pr_info("%s unexpected ack type %u\n",
			__func__, ack_msg->ack_type);
		return -EINVAL;
	}

	if (!vol_info)
		return ret;

	memcpy(vol_info, ack_msg->payload, sizeof(struct dsp_vol_info));

	return ret;
}

int mt8518_adsp_etdm_format(unsigned int format)
{
	switch (format) {
	case MT8518_ETDM_FORMAT_I2S:
		return ETDM_FORMAT_I2S;
	case MT8518_ETDM_FORMAT_LJ:
		return ETDM_FORMAT_LJ;
	case MT8518_ETDM_FORMAT_RJ:
		return ETDM_FORMAT_RJ;
	case MT8518_ETDM_FORMAT_EIAJ:
		return ETDM_FORMAT_EIAJ;
	case MT8518_ETDM_FORMAT_DSPA:
		return ETDM_FORMAT_DSPA;
	case MT8518_ETDM_FORMAT_DSPB:
		return ETDM_FORMAT_DSPB;
	default:
		return -1;
	}
}

int mt8518_adsp_etdm_data_mode(unsigned int mode)
{
	switch (mode) {
	case MT8518_ETDM_DATA_ONE_PIN:
		return ETDM_DATA_ONE_PIN;
	case MT8518_ETDM_DATA_MULTI_PIN:
		return ETDM_DATA_MULTI_PIN;
	default:
		return -1;
	}
}

int mt8518_adsp_etdm_clock_mode(unsigned int mode)
{
	switch (mode) {
	case MT8518_ETDM_SEPARATE_CLOCK:
		return ETDM_SEPARATE_CLOCK;
	case MT8518_ETDM_SHARED_CLOCK:
		return ETDM_SHARED_CLOCK;
	default:
		return -1;
	}
}

int mt8518_adsp_init_dsp_dmab(struct mt8518_dsp_dma_buffer *dmab)
{
	dmab->buf_paddr = 0;
	dmab->buf_size = 0;
	dmab->hw_offset_paddr = 0;
	dmab->appl_offset_paddr = 0;
	dmab->hw_offset = 0;
	dmab->appl_offset = 0;

	return 0;
}

int mt8518_adsp_reset_dsp_dmab_offset(struct mt8518_dsp_dma_buffer *dmab)
{
	dmab->hw_offset = 0;
	dmab->appl_offset = 0;

	return 0;
}

int mt8518_adsp_init_host_dmab(struct mt8518_host_dma_buffer *dmab)
{
	dmab->buf_vaddr = NULL;
	dmab->buf_size = 0;
	dmab->hw_offset = 0;
	dmab->transfer = 0;

	return 0;
}

int mt8518_adsp_reset_host_dmab_offset(struct mt8518_host_dma_buffer *dmab)
{
	dmab->hw_offset = 0;

	return 0;
}

#ifdef MT8518_ADSP_POWER_STATE_CONTROL_SUPPORT
static bool ignore_power_state_switch(int scene)
{
	switch (scene) {
	case TASK_SCENE_AUDIO_CONTROLLER:
	case TASK_SCENE_MIXER_BT:
	case TASK_SCENE_VA_HOSTLESS:
		return true;
	default:
		return false;
	}
}

static int get_active_power_state(int scene)
{
	return PWR_STATE_NORMAL;
}
#endif

int mt8518_adsp_notify_power_state(int scene)
{
#ifdef MT8518_ADSP_POWER_STATE_CONTROL_SUPPORT
	struct dsp_power_state state;
	int ret;

	if (ignore_power_state_switch(scene))
		return 0;

	state.task_scene = scene;
	state.power_state = get_active_power_state(scene);

	ret = mt8518_adsp_send_ipi_cmd(NULL,
		TASK_SCENE_AUDIO_CONTROLLER,
		AUDIO_IPI_LAYER_TO_DSP,
		AUDIO_IPI_PAYLOAD,
		AUDIO_IPI_MSG_NEED_ACK,
		MSG_TO_DSP_SET_POWER_STATE,
		sizeof(state),
		0,
		(char *)&state);

	return ret;
#else
	return 0;
#endif
}

int mt8518_adsp_clear_power_state(int scene)
{
#ifdef MT8518_ADSP_POWER_STATE_CONTROL_SUPPORT
	struct dsp_power_state state;
	int ret;

	if (ignore_power_state_switch(scene))
		return 0;

	state.task_scene = scene;
	state.power_state = PWR_STATE_DONT_CARE;

	ret = mt8518_adsp_send_ipi_cmd(NULL,
		TASK_SCENE_AUDIO_CONTROLLER,
		AUDIO_IPI_LAYER_TO_DSP,
		AUDIO_IPI_PAYLOAD,
		AUDIO_IPI_MSG_NEED_ACK,
		MSG_TO_DSP_SET_POWER_STATE,
		sizeof(state),
		0,
		(char *)&state);

	return ret;
#else
	return 0;
#endif
}

int mt8518_adsp_host_to_dsp_vol(int level, struct dsp_vol_info *vol_info)
{
	vol_info->input_vol = level * vol_info->max_vol /
		HOST_VOLUME_MAX_LEVEL;

	return 0;
}

int mt8518_adsp_dsp_to_host_vol(struct dsp_vol_info *vol_info, int *level)
{
	if (vol_info->max_vol > HOST_VOLUME_MAX_LEVEL)
		*level = (vol_info->input_vol + 1) * HOST_VOLUME_MAX_LEVEL /
			vol_info->max_vol;
	else
		*level = vol_info->input_vol * HOST_VOLUME_MAX_LEVEL /
			vol_info->max_vol;

	return 0;
}

