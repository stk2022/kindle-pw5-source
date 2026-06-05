/*
 * mt8518-adsp-compress.c  --  Mediatek 8518 adsp compress offload driver
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

#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/math64.h>
#include <linux/dma-mapping.h>
#include <linux/atomic.h>
#include <sound/compress_driver.h>
#include <sound/soc.h>
#include "mt8518-adsp-common.h"
#include "mach/mtk_hifi4dsp_api.h"


struct mt8518_adsp_compr_stream {
	struct snd_compr_stream *cstream;
	int id;
	int scene;
	struct dsp_mem_ifo mem_info;
	struct audio_compressed_codec_cap codec_cap;
	struct snd_compr_caps compr_caps;
	struct snd_compr_codec_caps *compr_codec_caps;
	struct mt8518_dsp_dma_buffer dsp_dmab;
	struct snd_dma_buffer host_dmab;
	uint64_t total_copied_bytes;
};

struct mt8518_adsp_def_pair {
	unsigned int host;
	unsigned int dsp;
};

static void handle_fragment_done(struct ipi_msg_t *p_ipi_msg)
{
	struct snd_compr_stream *cstream;
	struct mt8518_adsp_compr_stream *stream;
	struct dsp_fragment_done_param param;

	AUDIO_COPY_DSP_FRAGMENT_DONE_PARAM(p_ipi_msg->payload, &param);

	cstream = param.host_handler;
	if (!cstream) {
		pr_info("handle_fragment_done invalid handle\n");
		return;
	}

	stream = cstream->runtime->private_data;

	dev_dbg(&cstream->device->dev, "%s [#%d] ack\n",
		__func__, stream->id);

	snd_compr_fragment_elapsed(cstream);
}

static void handle_drain_done(struct ipi_msg_t *p_ipi_msg)
{
	struct snd_compr_stream *cstream;
	struct mt8518_adsp_compr_stream *stream;
	struct snd_soc_pcm_runtime *rtd;
	struct dsp_drain_done_param param;

	AUDIO_COPY_DSP_DRAIN_DONE_PARAM(p_ipi_msg->payload, &param);

	cstream = param.host_handler;
	if (!cstream) {
		pr_info("handle_drain_done invalid handle\n");
		return;
	}

	stream = cstream->runtime->private_data;

	dev_dbg(&cstream->device->dev, "%s [#%d] ack\n",
		__func__, stream->id);

	rtd = cstream->private_data;

	if (rtd && rtd->fe_compr && cstream->ops->trigger &&
	    (cstream->direction == SND_COMPRESS_PLAYBACK))
		cstream->ops->trigger(cstream, SNDRV_PCM_TRIGGER_STOP);

	snd_compr_drain_notify(cstream);
}

void mt8518_adsp_compr_ipi_recv_msg(struct ipi_msg_t *p_ipi_msg)
{
	if (mt8518_adsp_is_compress_playback_scene(p_ipi_msg->task_scene)) {
		switch (p_ipi_msg->msg_id) {
		case MSG_TO_HOST_DSP_FRAGMENT_DONE:
			handle_fragment_done(p_ipi_msg);
			break;
		case MSG_TO_HOST_DSP_DRAIN_DONE:
			handle_drain_done(p_ipi_msg);
			break;
		case MSG_TO_HOST_DSP_IRQDL:
			break;
		default:
			break;
		}
	}
}

static const struct mt8518_adsp_def_pair codec_list[] = {
	{SND_AUDIOCODEC_MP3, AUDIOCODEC_MP3},
	{SND_AUDIOCODEC_AAC, AUDIOCODEC_AAC},
};

static const struct mt8518_adsp_def_pair aac_format_list[] = {
	{SND_AUDIOSTREAMFORMAT_MP2ADTS, AAC_STREAMFORMAT_MP2ADTS},
	{SND_AUDIOSTREAMFORMAT_MP4ADTS, AAC_STREAMFORMAT_MP4ADTS},
	{SND_AUDIOSTREAMFORMAT_MP4LOAS, AAC_STREAMFORMAT_MP4LOAS},
	{SND_AUDIOSTREAMFORMAT_MP4LATM, AAC_STREAMFORMAT_MP4LATM},
	{SND_AUDIOSTREAMFORMAT_ADIF, AAC_STREAMFORMAT_ADIF},
	{SND_AUDIOSTREAMFORMAT_MP4FF, AAC_STREAMFORMAT_MP4FF},
	{SND_AUDIOSTREAMFORMAT_RAW, AAC_STREAMFORMAT_RAW},
};

static const struct mt8518_adsp_def_pair aac_mode_list[] = {
	{SND_AUDIOMODE_AAC_MAIN, AAC_PROFILE_MAIN},
	{SND_AUDIOMODE_AAC_LC, AAC_PROFILE_LC},
	{SND_AUDIOMODE_AAC_SSR, AAC_PROFILE_SSR},
	{SND_AUDIOMODE_AAC_LTP, AAC_PROFILE_LTP},
	{SND_AUDIOMODE_AAC_HE, AAC_PROFILE_HE},
	{SND_AUDIOMODE_AAC_SCALABLE, AAC_PROFILE_SCALABLE},
	{SND_AUDIOMODE_AAC_ERLC, AAC_PROFILE_ERLC},
	{SND_AUDIOMODE_AAC_LD, AAC_PROFILE_LD},
	{SND_AUDIOMODE_AAC_HE_PS, AAC_PROFILE_HE_PS},
	{SND_AUDIOMODE_AAC_HE_MPS, AAC_PROFILE_HE_MPS},
};

static unsigned int cal_supported_codec_num(unsigned int codec_support)
{
	size_t i;
	unsigned int num = 0;

	for (i = 0; i < ARRAY_SIZE(codec_list); i++) {
		if (codec_support & codec_list[i].dsp)
			num++;
	}

	return num;
}

static void fill_mp3_desc(struct snd_codec_desc *desc)
{
	desc->max_ch = 2;
	desc->sample_rates[0] = 48000;
	desc->sample_rates[1] = 44100;
	desc->sample_rates[2] = 32000;
	desc->sample_rates[3] = 24000;
	desc->sample_rates[4] = 22050;
	desc->sample_rates[5] = 16000;
	desc->sample_rates[6] = 8000;
	desc->num_sample_rates = 7;
	desc->profiles = 0;
	desc->modes = SND_AUDIOCHANMODE_MP3_STEREO;
	desc->formats = 0;
}

static void fill_aac_desc(struct snd_codec_desc *desc,
	unsigned int max_ch,
	unsigned int profiles,
	unsigned int modes,
	unsigned int formats)
{
	desc->max_ch = max_ch;
	desc->sample_rates[0] = 96000;
	desc->sample_rates[1] = 88200;
	desc->sample_rates[2] = 64000;
	desc->sample_rates[3] = 48000;
	desc->sample_rates[4] = 44100;
	desc->sample_rates[5] = 32000;
	desc->sample_rates[6] = 24000;
	desc->sample_rates[7] = 22050;
	desc->sample_rates[8] = 16000;
	desc->sample_rates[9] = 12000;
	desc->sample_rates[10] = 11025;
	desc->sample_rates[11] = 8000;
	desc->num_sample_rates = 12;
	desc->profiles = profiles;
	desc->modes = modes;
	desc->formats = formats;
}

static unsigned int get_aac_audio_modes(unsigned int prfiles)
{
	size_t i;
	unsigned int modes = 0;

	for (i = 0; i < ARRAY_SIZE(aac_mode_list); i++) {
		if (prfiles & aac_mode_list[i].dsp)
			modes |= aac_mode_list[i].host;
	}

	return modes;
}

static unsigned int mt8518_adsp_codec_id(unsigned int codec)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(codec_list); i++) {
		if (codec == codec_list[i].host)
			return codec_list[i].dsp;
	}

	pr_notice("%s unexpected codec %u\n", __func__, codec);
	return 0;
}

static unsigned int mt8518_adsp_aac_format(unsigned int format)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(aac_format_list); i++) {
		if (format == aac_format_list[i].host)
			return aac_format_list[i].dsp;
	}

	pr_notice("%s unexpected format %u\n", __func__, format);
	return 0;
}

static int mt8518_update_compr_caps(struct snd_compr_stream *cstream)
{
	struct mt8518_adsp_compr_stream *stream =
		cstream->runtime->private_data;
	struct snd_compr_caps *compr_caps = &stream->compr_caps;
	struct snd_compr_codec_caps *compr_codec_caps;
	unsigned int max_buffer_size = stream->mem_info.mem_size;
	unsigned int min_fragment_size = stream->mem_info.mem_min_transfer;
	unsigned int codec_support = stream->codec_cap.codec_support;
	size_t codec_idx = 0;
	size_t desc_idx = 0;
	struct snd_codec_desc *desc;
	size_t i;

	if (max_buffer_size == 0)
		max_buffer_size = 1024 * 8;
	if (min_fragment_size == 0)
		min_fragment_size = 512;

	compr_caps->direction = SND_COMPRESS_PLAYBACK;
	compr_caps->min_fragments = 2;
	compr_caps->min_fragment_size = min_fragment_size;
	compr_caps->max_fragment_size = max_buffer_size /
		compr_caps->min_fragments;
	compr_caps->max_fragments = max_buffer_size /
		compr_caps->min_fragment_size;

	compr_caps->num_codecs = cal_supported_codec_num(codec_support);
	if (compr_caps->num_codecs == 0)
		return 0;

	stream->compr_codec_caps = kzalloc(compr_caps->num_codecs *
		sizeof(struct snd_compr_codec_caps), GFP_KERNEL);

	if (codec_support & AUDIOCODEC_MP3) {
		compr_caps->codecs[codec_idx] = SND_AUDIOCODEC_MP3;
		compr_codec_caps = &stream->compr_codec_caps[codec_idx];
		compr_codec_caps->codec = SND_AUDIOCODEC_MP3;
		fill_mp3_desc(&compr_codec_caps->descriptor[0]);
		compr_codec_caps->num_descriptors = 1;
		codec_idx++;
	}

	if (codec_support & AUDIOCODEC_AAC) {
		unsigned int formats = stream->codec_cap.aac_stream_formats;
		unsigned int modes =
			get_aac_audio_modes(stream->codec_cap.aac_profiles);
		unsigned int max_ch = stream->codec_cap.aac_dec_max_ch;

		compr_caps->codecs[codec_idx] = SND_AUDIOCODEC_AAC;
		compr_codec_caps = &stream->compr_codec_caps[codec_idx];
		compr_codec_caps->codec = SND_AUDIOCODEC_AAC;

		desc_idx = 0;

		for (i = 0; i < ARRAY_SIZE(aac_format_list); i++) {
			desc = &compr_codec_caps->descriptor[desc_idx];

			if (formats & aac_format_list[i].dsp) {
				fill_aac_desc(desc,
					max_ch,
					SND_AUDIOPROFILE_AAC,
					modes,
					aac_format_list[i].host);
				desc_idx++;
			}
		}

		compr_codec_caps->num_descriptors = desc_idx;
		codec_idx++;
	}

	return 0;
}

static void mt8518_adsp_free_priv_data(struct mt8518_adsp_compr_stream *stream)
{
	if (stream) {
		kfree(stream->compr_codec_caps);
		kfree(stream);
	}
}

static int mt8518_adsp_compr_open(struct snd_compr_stream *cstream)
{
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct mt8518_audio_spi_priv *priv =
		snd_soc_platform_get_drvdata(rtd->platform);
	struct mt8518_adsp_compr_stream *stream;
	struct ipi_msg_t ipi_msg;
	struct host_startup_param startup_param;
	int scene;
	int ret;

	dev_dbg(&cstream->device->dev,
		"%s [#%d]\n", __func__, rtd->cpu_dai->id);

	stream = kzalloc(sizeof(struct mt8518_adsp_compr_stream), GFP_KERNEL);
	if (!stream)
		return -ENOMEM;

	stream->cstream = cstream;

	runtime->private_data = stream;

	stream->id = rtd->cpu_dai->id;

	scene = mt8518_adsp_get_scene_by_dai_id(stream->id);
	if (scene < 0) {
		ret = -EINVAL;
		goto free_stream;
	}

	if (mt8518_adsp_activate_compr(stream->id, priv->compr_active) != 1) {
		dev_dbg(&cstream->device->dev, "%s [#%d] busy\n",
			__func__, rtd->cpu_dai->id);
		ret = -EBUSY;
		goto handle_deactivate;
	}

	stream->scene = scene;

	mt8518_adsp_init_dsp_dmab(&stream->dsp_dmab);

	memset(&ipi_msg, 0, sizeof(ipi_msg));

	ret = mt8518_adsp_send_ipi_cmd(&ipi_msg,
		scene,
		AUDIO_IPI_LAYER_TO_DSP,
		AUDIO_IPI_MSG_ONLY,
		AUDIO_IPI_MSG_NEED_ACK,
		MSG_TO_DSP_HOST_QUERY_MEM_INFO,
		0, 0, NULL);
	if (ret) {
		ret = -EINVAL;
		goto handle_deactivate;
	}

	AUDIO_COPY_DSP_MEM_INFO(ipi_msg.payload, &stream->mem_info);

	memset(&ipi_msg, 0, sizeof(ipi_msg));

	ret = mt8518_adsp_send_ipi_cmd(&ipi_msg,
		scene,
		AUDIO_IPI_LAYER_TO_DSP,
		AUDIO_IPI_MSG_ONLY,
		AUDIO_IPI_MSG_NEED_ACK,
		MSG_TO_DSP_HOST_QUERY_CODEC_CAP,
		0, 0, NULL);
	if (ret) {
		ret = -EINVAL;
		goto handle_deactivate;
	}

	AUDIO_COPY_COMPR_CODEC_CAP(ipi_msg.payload, &stream->codec_cap);

	ret = mt8518_update_compr_caps(cstream);
	if (ret) {
		ret = -EINVAL;
		goto handle_deactivate;
	}

	memset(&startup_param, 0, sizeof(startup_param));

	startup_param.host_handler = cstream;

	ret = mt8518_adsp_notify_power_state(scene);
	if (ret) {
		ret = -EINVAL;
		goto handle_deactivate;
	}

	ret = mt8518_adsp_send_ipi_cmd(NULL,
		scene,
		AUDIO_IPI_LAYER_TO_DSP,
		AUDIO_IPI_PAYLOAD,
		AUDIO_IPI_MSG_NEED_ACK,
		MSG_TO_DSP_HOST_PORT_STARTUP,
		sizeof(startup_param),
		0,
		(char *)&startup_param);
	if (ret) {
		ret = -EINVAL;
		goto handle_deactivate;
	}

	return 0;

handle_deactivate:
	mt8518_adsp_deactivate_compr(stream->id, priv->compr_active);
free_stream:
	mt8518_adsp_free_priv_data(stream);

	return ret;
}

static int mt8518_adsp_compr_free(struct snd_compr_stream *cstream)
{
	struct mt8518_adsp_compr_stream *stream =
		cstream->runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct mt8518_audio_spi_priv *priv =
		snd_soc_platform_get_drvdata(rtd->platform);

	int scene = stream->scene;

	dev_dbg(&cstream->device->dev, "%s [#%d]\n", __func__, stream->id);

	if (scene >= 0) {
		mt8518_adsp_send_ipi_cmd(NULL,
			scene,
			AUDIO_IPI_LAYER_TO_DSP,
			AUDIO_IPI_MSG_ONLY,
			AUDIO_IPI_MSG_NEED_ACK,
			MSG_TO_DSP_HOST_CLOSE,
			0, 0, NULL);

		mt8518_adsp_clear_power_state(scene);
	}

	if (stream->host_dmab.bytes > 0)
		snd_dma_free_pages(&stream->host_dmab);

	mt8518_adsp_deactivate_compr(stream->id, priv->compr_active);

	mt8518_adsp_free_priv_data(stream);

	return 0;
}

static int mt8518_adsp_compr_set_metadata(struct snd_compr_stream *cstream,
	struct snd_compr_metadata *metadata)
{
	struct mt8518_adsp_compr_stream *stream =
		cstream->runtime->private_data;
	int scene = stream->scene;
	struct audio_compressed_metadata compr_meta;
	int ret;

	if (scene < 0)
		return -EINVAL;

	AUDIO_COPY_COMPR_META(metadata, &compr_meta);

	ret = mt8518_adsp_send_ipi_cmd(NULL,
		scene,
		AUDIO_IPI_LAYER_TO_DSP,
		AUDIO_IPI_PAYLOAD,
		AUDIO_IPI_MSG_NEED_ACK,
		MSG_TO_DSP_HOST_SET_METADATA,
		sizeof(compr_meta),
		0,
		(char *)&compr_meta);

	if (ret)
		return ret;

	return 0;
}

static int mt8518_adsp_compr_get_metadata(struct snd_compr_stream *cstream,
	struct snd_compr_metadata *metadata)
{
	struct mt8518_adsp_compr_stream *stream =
		cstream->runtime->private_data;
	int scene = stream->scene;
	struct ipi_msg_t ipi_msg;
	struct audio_compressed_metadata compr_meta;
	int ret;

	if (scene < 0)
		return -EINVAL;

	memset(&ipi_msg, 0, sizeof(ipi_msg));

	ret = mt8518_adsp_send_ipi_cmd(&ipi_msg,
		scene,
		AUDIO_IPI_LAYER_TO_DSP,
		AUDIO_IPI_MSG_ONLY,
		AUDIO_IPI_MSG_NEED_ACK,
		MSG_TO_DSP_HOST_GET_METADATA,
		0, 0, NULL);

	if (ret)
		return ret;

	AUDIO_COPY_COMPR_META(ipi_msg.payload, &compr_meta);

	return 0;
}

static int mt8518_adsp_compr_set_params(struct snd_compr_stream *cstream,
	struct snd_compr_params *params)
{
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct device *dev = rtd->compr->card->dev;
	struct mt8518_adsp_compr_stream *stream =
		cstream->runtime->private_data;
	struct audio_compressed_codec_cap *codec_cap = &stream->codec_cap;
	int scene = stream->scene;
	struct ipi_msg_t ipi_msg;
	struct host_ipc_msg_hw_param ipc_hw_param;
	struct dsp_ipc_msg_hw_param ack_hw_param;
	struct audio_compressed_params *compr_params;
	struct io_ipc_ring_buf_shared *shared_buf;
	struct mt8518_dsp_dma_buffer *dsp_dmab;
	unsigned int codec_id;
	unsigned int stream_format = 0;
	int ret;

	dev_dbg(&cstream->device->dev,
		"%s [#%d] fragment(%u-%u) no_wake_mode(%u)\n",
		__func__, stream->id, params->buffer.fragment_size,
		params->buffer.fragments, params->no_wake_mode);

	dev_dbg(&cstream->device->dev,
		"%s [#%d] codec(%u) profile(%u) format(%u)\n",
		__func__, stream->id, params->codec.id,
		params->codec.profile, params->codec.format);

	dev_dbg(&cstream->device->dev,
		"%s [#%d] rate(%u) ch(%u-%u)\n",
		__func__, stream->id, params->codec.sample_rate,
		params->codec.ch_in, params->codec.ch_out);

	if (scene < 0)
		return -EINVAL;

	codec_id = mt8518_adsp_codec_id(params->codec.id);
	if (!(codec_id & codec_cap->codec_support)) {
		dev_info(&cstream->device->dev,
			 "%s [#%d] codec(%u) not supported\n",
			 __func__, stream->id, params->codec.id);
		return -EINVAL;
	}

	if (codec_id == AUDIOCODEC_AAC) {
		stream_format = mt8518_adsp_aac_format(params->codec.format);
		if (!(stream_format & codec_cap->aac_stream_formats)) {
			dev_info(&cstream->device->dev,
				 "%s [#%d] aac format(%u) not supported\n",
				 __func__, stream->id, params->codec.format);
			return -EINVAL;
		}

		if (params->codec.ch_in > codec_cap->aac_dec_max_ch) {
			dev_info(&cstream->device->dev,
				 "%s [#%d] aac ch(%u) not supported\n",
				 __func__, stream->id, params->codec.ch_in);
			return -EINVAL;
		}
	}

	memset(&ipi_msg, 0, sizeof(ipi_msg));
	memset(&ipc_hw_param, 0, sizeof(ipc_hw_param));

	ipc_hw_param.sample_rate = params->codec.sample_rate;
	ipc_hw_param.channel_num = params->codec.ch_in;
	ipc_hw_param.irq_no_update = 1;

	compr_params = &ipc_hw_param.compr_params;
	compr_params->codec = codec_id;
	compr_params->profile = params->codec.profile;
	compr_params->format = stream_format;
	compr_params->no_wake_mode = params->no_wake_mode;
	compr_params->aud_compr_buf.fragments = params->buffer.fragments;
	compr_params->aud_compr_buf.fragment_size =
		params->buffer.fragment_size;

	ret = mt8518_adsp_send_ipi_cmd(&ipi_msg,
		scene,
		AUDIO_IPI_LAYER_TO_DSP,
		AUDIO_IPI_PAYLOAD,
		AUDIO_IPI_MSG_NEED_ACK,
		MSG_TO_DSP_HOST_HW_PARAMS,
		sizeof(ipc_hw_param),
		0,
		(char *)&ipc_hw_param);
	if (ret)
		return ret;

	ret = mt8518_adsp_verify_ack_hw_param(&ipc_hw_param,
		&ipi_msg, &ack_hw_param);

	if (ret != 0)
		return ret;

	shared_buf = &ack_hw_param.SharedRingBuffer;
	dsp_dmab = &stream->dsp_dmab;

	dsp_dmab->buf_paddr = shared_buf->start_addr;
	dsp_dmab->buf_size = shared_buf->size_bytes;
	dsp_dmab->hw_offset_paddr = shared_buf->ptr_to_hw_offset_bytes;
	dsp_dmab->appl_offset_paddr = shared_buf->ptr_to_appl_offset_bytes;

	if (stream->host_dmab.bytes > 0) {
		snd_dma_free_pages(&stream->host_dmab);
		memset(&stream->host_dmab, 0, sizeof(struct snd_dma_buffer));
	}

	ret = snd_dma_alloc_pages(SNDRV_DMA_TYPE_DEV,
				  dev,
				  params->buffer.fragment_size *
				  params->buffer.fragments,
				  &stream->host_dmab);
	if (ret)
		return ret;

	return 0;
}

static int mt8518_adsp_compr_trigger(struct snd_compr_stream *cstream, int cmd)
{
	struct mt8518_adsp_compr_stream *stream =
		cstream->runtime->private_data;
	int scene = stream->scene;

	dev_dbg(&cstream->device->dev, "%s [#%d] cmd %d\n",
		__func__, stream->id, cmd);

	if (scene < 0)
		return -EINVAL;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		mt8518_adsp_send_ipi_cmd(NULL,
			scene,
			AUDIO_IPI_LAYER_TO_DSP,
			AUDIO_IPI_MSG_ONLY,
			AUDIO_IPI_MSG_DIRECT_SEND,
			MSG_TO_DSP_HOST_TRIGGER_START,
			0, 0, NULL);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		mt8518_adsp_send_ipi_cmd(NULL,
			scene,
			AUDIO_IPI_LAYER_TO_DSP,
			AUDIO_IPI_MSG_ONLY,
			AUDIO_IPI_MSG_DIRECT_SEND,
			MSG_TO_DSP_HOST_TRIGGER_STOP,
			0, 0, NULL);

		mt8518_adsp_reset_dsp_dmab_offset(&stream->dsp_dmab);
		break;
	case SND_COMPR_TRIGGER_DRAIN:
		mt8518_adsp_send_ipi_cmd(NULL,
			scene,
			AUDIO_IPI_LAYER_TO_DSP,
			AUDIO_IPI_MSG_ONLY,
			AUDIO_IPI_MSG_DIRECT_SEND,
			MSG_TO_DSP_HOST_TRIGGER_DRAIN,
			0, 0, NULL);

		break;
	case SND_COMPR_TRIGGER_PARTIAL_DRAIN:
		mt8518_adsp_send_ipi_cmd(NULL,
			scene,
			AUDIO_IPI_LAYER_TO_DSP,
			AUDIO_IPI_MSG_ONLY,
			AUDIO_IPI_MSG_DIRECT_SEND,
			MSG_TO_DSP_HOST_TRIGGER_PARTIAL_DRAIN,
			0, 0, NULL);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		mt8518_adsp_send_ipi_cmd(NULL,
			scene,
			AUDIO_IPI_LAYER_TO_DSP,
			AUDIO_IPI_MSG_ONLY,
			AUDIO_IPI_MSG_DIRECT_SEND,
			MSG_TO_DSP_HOST_TRIGGER_PAUSE,
			0, 0, NULL);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		mt8518_adsp_send_ipi_cmd(NULL,
			scene,
			AUDIO_IPI_LAYER_TO_DSP,
			AUDIO_IPI_MSG_ONLY,
			AUDIO_IPI_MSG_DIRECT_SEND,
			MSG_TO_DSP_HOST_TRIGGER_RESUME,
			0, 0, NULL);
		break;
	}

	return 0;
}

static int mt8518_adsp_compr_pointer(struct snd_compr_stream *cstream,
	struct snd_compr_tstamp *tstamp)
{
	struct mt8518_adsp_compr_stream *stream =
		cstream->runtime->private_data;
	struct mt8518_dsp_dma_buffer *dsp_dmab = &stream->dsp_dmab;
	struct ipi_msg_t ipi_msg;
	struct audio_pcm_tstamp pcm_tstamp;
	int scene = stream->scene;
	int ret;

	if (scene < 0)
		return -EINVAL;

	memset(&ipi_msg, 0, sizeof(ipi_msg));

	ret = mt8518_adsp_send_ipi_cmd(&ipi_msg,
		scene,
		AUDIO_IPI_LAYER_TO_DSP,
		AUDIO_IPI_MSG_ONLY,
		AUDIO_IPI_MSG_NEED_ACK,
		MSG_TO_DSP_HOST_QUERY_PCM_TSTAMP,
		0, 0, NULL);
	if (ret)
		return -EINVAL;

	AUDIO_COPY_PCM_TSTAMP(ipi_msg.payload, &pcm_tstamp);

	tstamp->copied_total = pcm_tstamp.copied_total;
	tstamp->pcm_frames = pcm_tstamp.pcm_frames;
	tstamp->pcm_io_frames = pcm_tstamp.pcm_io_frames;
	tstamp->sampling_rate = pcm_tstamp.sampling_rate;

	/* derive byte offset */
	div_u64_rem(pcm_tstamp.copied_total,
		    dsp_dmab->buf_size,
		    &tstamp->byte_offset);

	dev_dbg(&cstream->device->dev,
		"%s [#%d] offset(%u) copied(%u) frames(%u-%u) rate(%u)\n",
		__func__, stream->id, tstamp->byte_offset,
		tstamp->copied_total, tstamp->pcm_frames,
		tstamp->pcm_io_frames, tstamp->sampling_rate);

	return 0;
}

static int mt8518_adsp_compr_copy(struct snd_compr_stream *cstream,
	char __user *buf, size_t count)
{
	struct mt8518_adsp_compr_stream *stream =
		cstream->runtime->private_data;
	struct mt8518_dsp_dma_buffer *dsp_dmab = &stream->dsp_dmab;
	struct snd_dma_buffer *host_dmab = &stream->host_dmab;
	uint32_t adsp_dma_buf_paddr = dsp_dmab->buf_paddr;
	uint32_t adsp_dma_buf_size = dsp_dmab->buf_size;
	uint32_t adsp_dma_hw_off = 0;
	uint32_t adsp_dma_appl_off = dsp_dmab->appl_offset;
	unsigned char *host_dma_buf_vaddr = host_dmab->area;
	uint32_t host_dma_buf_size = host_dmab->bytes;
	uint32_t host_dma_offset = 0;
	uint32_t avail_bytes;
	uint32_t copy_bytes;

	dev_dbg(&cstream->device->dev, "%s count = %zu\n", __func__, count);

	dsp_spi_read(dsp_dmab->hw_offset_paddr,
		     &adsp_dma_hw_off,
		     sizeof(adsp_dma_hw_off),
		     SPI_SPEED_HIGH);

	if (adsp_dma_hw_off > adsp_dma_appl_off) {
		avail_bytes = adsp_dma_hw_off - adsp_dma_appl_off;
	} else {
		avail_bytes = adsp_dma_buf_size - adsp_dma_appl_off +
			adsp_dma_hw_off;
	}

	copy_bytes = count;
	if (copy_bytes > host_dmab->bytes)
		copy_bytes = host_dmab->bytes;
	if (copy_bytes > avail_bytes)
		copy_bytes = avail_bytes;

	/* copy data from user memory to kernel non-cached buffer */
	if (copy_from_user_toio(host_dma_buf_vaddr, buf, copy_bytes))
		return -EFAULT;

	count = copy_bytes;

	/* write data from kernel non-cached buffer to ADSP sram via SPI */
	while (copy_bytes > 0) {
		uint32_t to_bytes = 0;

		if (adsp_dma_hw_off > adsp_dma_appl_off)
			to_bytes = adsp_dma_hw_off - adsp_dma_appl_off;
		else
			to_bytes = adsp_dma_buf_size - adsp_dma_appl_off;

		if (to_bytes > copy_bytes)
			to_bytes = copy_bytes;

		dev_dbg(&cstream->device->dev,
			"%s [#%d] copy %u bytes from %p to 0x%x\n",
			__func__, stream->id, to_bytes,
			host_dma_buf_vaddr + host_dma_offset,
			adsp_dma_buf_paddr + adsp_dma_appl_off);

		dsp_spi_write_ex(adsp_dma_buf_paddr + adsp_dma_appl_off,
				 host_dma_buf_vaddr + host_dma_offset,
				 to_bytes, SPI_SPEED_HIGH);

		host_dma_offset = (host_dma_offset + to_bytes) %
			host_dma_buf_size;
		adsp_dma_appl_off = (adsp_dma_appl_off + to_bytes) %
			adsp_dma_buf_size;

		copy_bytes -= to_bytes;
	}

	dsp_spi_write(dsp_dmab->appl_offset_paddr,
		      &adsp_dma_appl_off,
		      sizeof(adsp_dma_appl_off),
		      SPI_SPEED_HIGH);
	dsp_dmab->appl_offset = adsp_dma_appl_off;
	dsp_dmab->hw_offset = adsp_dma_hw_off;

	stream->total_copied_bytes += count;

	return count;
}

static int mt8518_adsp_compr_get_caps(struct snd_compr_stream *cstream,
	struct snd_compr_caps *caps)
{
	struct mt8518_adsp_compr_stream *stream;

	stream = cstream->runtime->private_data;

	memcpy(caps, &stream->compr_caps, sizeof(struct snd_compr_caps));

	return 0;
}

static int mt8518_adsp_compr_get_codec_caps(struct snd_compr_stream *cstream,
	struct snd_compr_codec_caps *codec)
{
	struct mt8518_adsp_compr_stream *stream =
		cstream->runtime->private_data;
	unsigned int codec_caps_num = stream->compr_caps.num_codecs;
	unsigned int i;

	for (i = 0; i < codec_caps_num; i++) {
		struct snd_compr_codec_caps *compr_codec_caps =
			&stream->compr_codec_caps[i];
		if (codec->codec == compr_codec_caps->codec) {
			*codec = *compr_codec_caps;
			return 0;
		}
	}

	return -EINVAL;
}

const struct snd_compr_ops mt8518_adsp_compr_ops = {
	.open = mt8518_adsp_compr_open,
	.free = mt8518_adsp_compr_free,
	.set_params = mt8518_adsp_compr_set_params,
	.set_metadata = mt8518_adsp_compr_set_metadata,
	.get_metadata = mt8518_adsp_compr_get_metadata,
	.trigger = mt8518_adsp_compr_trigger,
	.pointer = mt8518_adsp_compr_pointer,
	.copy = mt8518_adsp_compr_copy,
	.get_caps = mt8518_adsp_compr_get_caps,
	.get_codec_caps = mt8518_adsp_compr_get_codec_caps,
};

