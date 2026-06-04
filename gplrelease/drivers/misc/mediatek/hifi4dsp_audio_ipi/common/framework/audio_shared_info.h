/*
 * audio_shared_info.h
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

#ifndef __AUDIO_SHARED_INFO_H__
#define __AUDIO_SHARED_INFO_H__
enum {
	TASK_SCENE_AUDIO_CONTROLLER = 0,
	TASK_SCENE_VA,			// Voice Assistant
	TASK_SCENE_INTERLINK_HOST_TO_DSP,
	TASK_SCENE_VA_AEC = TASK_SCENE_INTERLINK_HOST_TO_DSP,
	TASK_SCENE_COMPRESS_PLAYBACK1,
	TASK_SCENE_PCM_PLAYBACK,
	TASK_SCENE_MIXER_PRIMARY,
	TASK_SCENE_MIXER_BT,
	TASK_SCENE_VA_HOSTLESS,
	TASK_SCENE_VA_UPLOAD,
	TASK_SCENE_COMPRESS_PLAYBACK2,
	TASK_SCENE_COMPRESS_PLAYBACK3,
	TASK_SCENE_DEBUG_DUMP1,
	TASK_SCENE_DEBUG_DUMP2,
	TASK_SCENE_DEBUG_DUMP3,
	TASK_SCENE_SIZE,
	TASK_SCENE_INVALID
};

/* Voice Assistant task state */
enum {
	VA_STATE_IDLE,
	VA_STATE_VAD,
	VA_STATE_PREPROCESSING,
	VA_STATE_AEC,
	VA_STATE_KEYWORD,
	VA_STATE_UPLOAD,
};

/* Voice Assitant type*/
enum {
	VA_RECORD = 0,
	VA_VAD,
	VA_AEC,
	VA_KEYWORD,
	VA_PREPROCESSING,
};

enum {
	VA_NOTIFY_VAD_PASS,
	VA_NOTIFY_WAKEWORD_PASS,
};

/* etdm format */
enum {
	ETDM_FORMAT_I2S = 0,
	ETDM_FORMAT_LJ,
	ETDM_FORMAT_RJ,
	ETDM_FORMAT_EIAJ,
	ETDM_FORMAT_DSPA,
	ETDM_FORMAT_DSPB,
};

/* etdm data_mode */
enum {
	ETDM_DATA_ONE_PIN = 0,
	ETDM_DATA_MULTI_PIN,
};

/* etdm clock_mode */
enum {
	ETDM_SEPARATE_CLOCK = 0,
	ETDM_SHARED_CLOCK,
};

/*Voice Assistant State Machine*/
#define VA_STATE(x)			(1 << x)

enum {
	RING_BUF_TYPE_RECORD,
	RING_BUF_TYPE_PLAYBACK,
};

struct io_ipc_ring_buf_shared {
	uint32_t start_addr;
	uint32_t size_bytes;
	uint32_t ptr_to_hw_offset_bytes;
	uint32_t ptr_to_appl_offset_bytes;

	//ring_buffer_type: record or playback
	uint32_t ring_buffer_dir;

	/* hw_offset_flag:
	 * We treat hw_offset==appl_offset as buffer empty when record.
	 * If the buffer is full the hw_offset will be one bytes behind the
	 * appl_offset and the hw_offset_flag will set to one.
	 * When playback, hw_offset==appl_offset will be treated as full. If
	 * the buffer is empty, the hw_offset will be one bytes behind the
	 * appl_offset and the hw_offset_flag will set to one.
	 */
	uint32_t hw_offset_flag;
};

struct TDM_config_shared {
	uint32_t mclk_freq;
	uint32_t lrck_width;
	uint8_t slave_mode;	/* true or false */
	uint8_t format;
	uint8_t lrck_invert;	/* true or false */
	uint8_t bck_invert;	/* true or false */
	uint8_t data_mode;
	uint8_t clock_mode;
	uint8_t bck_per_channel;
};

/* compress offload start*/
struct audio_compressed_buffer {
	uint32_t fragment_size;
	uint32_t fragments;
};

struct audio_compressed_params {
	uint32_t codec;
	uint32_t profile;
	uint32_t format;
	/* don't wake on fragment elapsed */
	uint8_t no_wake_mode;
	struct audio_compressed_buffer aud_compr_buf;
};

struct audio_compressed_metadata {
	uint32_t key;
	uint32_t value[8];
};

struct audio_pcm_tstamp {
	uint64_t copied_total;
	uint32_t pcm_frames;
	uint32_t pcm_io_frames;
	uint32_t sampling_rate;
};

struct audio_compressed_codec_cap {
	uint32_t codec_support;
	uint32_t aac_profiles;
	uint32_t aac_stream_formats;
	uint32_t aac_dec_max_ch;
};

enum {
	AUDIOCODEC_MP3 = 1,
	AUDIOCODEC_AAC = 2,
	AUDIOCODEC_SBC = 4,
};

enum {
	AAC_PROFILE_MAIN = 0x1,
	AAC_PROFILE_LC = 0x2,
	AAC_PROFILE_SSR = 0x4,
	AAC_PROFILE_LTP = 0x8,
	AAC_PROFILE_HE = 0x10,
	AAC_PROFILE_SCALABLE = 0x20,
	AAC_PROFILE_ERLC = 0x40,
	AAC_PROFILE_LD = 0x80,
	AAC_PROFILE_HE_PS = 0x100,
	AAC_PROFILE_HE_MPS = 0x200,
};

enum {
	AAC_STREAMFORMAT_MP2ADTS = 0x1,
	AAC_STREAMFORMAT_MP4ADTS = 0x2,
	AAC_STREAMFORMAT_MP4LOAS = 0x4,
	AAC_STREAMFORMAT_MP4LATM = 0x8,
	AAC_STREAMFORMAT_ADIF = 0x10,
	AAC_STREAMFORMAT_MP4FF = 0x20,
	AAC_STREAMFORMAT_RAW = 0x40,
};

/* compress offload end*/

enum {
	PWR_STATE_NORMAL,
	PWR_STATE_LOW_PWR,
	PWR_STATE_DONT_CARE,
	PWR_STATE_NUM,
};

/* information struct from host */
struct host_ipc_msg_hw_param {
	uint32_t sample_rate;
	uint8_t channel_num;
	uint8_t bitwidth; /* 16bits or 32bits */
	uint32_t period_size; /* in frames */
	uint32_t period_count;
	union {
		struct TDM_config_shared tdm_config;
		struct audio_compressed_params compr_params;
	};
	uint8_t irq_no_update;
};

/* information struct to host */
struct dsp_ipc_msg_hw_param {
	uint32_t sample_rate;
	uint8_t channel_num;
	uint8_t bitwidth; /* 16bits or 32bits */
	uint32_t period_size; /* in frames */
	uint32_t period_count;
	struct io_ipc_ring_buf_shared SharedRingBuffer;
};

struct host_startup_param {
	void *host_handler;
};

struct ipc_va_params {
	uint32_t va_type;
	uint8_t enable_flag;
};

struct dsp_mem_ifo {
	uint32_t mem_size;
	uint32_t mem_alignment;
	uint32_t mem_min_transfer;
	uint32_t period_time_us_alignment;
};

struct dsp_irq_param {
	void *host_handler;
};

struct dsp_drain_done_param {
	void *host_handler;
};

struct dsp_fragment_done_param {
	void *host_handler;
};

struct dsp_power_state {
	uint8_t task_scene;
	uint8_t power_state;
};

struct dsp_ipc_va_notify {
	void *host_handler;
	uint32_t type;
	char wakeword[32];
};

struct host_debug_start_param {
	void *host_handler;
	uint32_t task_param;
	uint32_t priv_param[4];
	uint32_t max_byte;
};

struct dsp_debug_start_param {
	void *host_handler;
	uint32_t max_byte;
	uint32_t period_size_bytes;
	uint32_t period_count;
	struct io_ipc_ring_buf_shared SharedRingBuffer;
};

struct host_debug_stop_param {
	void *host_handler;
	uint32_t task_param;
	uint32_t priv_param[4];
};

struct dsp_debug_irq_param {
	void *host_handler;
	uint32_t irq_notify_done;
};

struct dsp_vol_info {
	int max_vol;
	union {
		int master_vol;
		int input_vol;
	};
};

#define AUDIO_IPC_COPY_DSP_HW_PARAM(src, dst) \
	memcpy((void *)dst, (void *)src, sizeof(struct dsp_ipc_msg_hw_param))

#define AUDIO_IPC_COPY_HOST_HW_PARAM(src, dst) \
	memcpy((void *)dst, (void *)src, sizeof(struct host_ipc_msg_hw_param))

#define AUDIO_IPC_COPY_HOST_STARTUP_PARAM(src, dst) \
	memcpy((void *)dst, (void *)src, sizeof(struct host_startup_param))

#define AUDIO_COPY_SHARED_BUFFER_INFO(src, dst) \
	memcpy((void *)dst, (void *)src, \
	sizeof(struct io_ipc_ring_buf_shared))

#define AUDIO_COPY_VA_PARAMS_SRC2DST(src, dst) \
	memcpy((void *)dst, (void *)src, \
	sizeof(struct ipc_va_params))

#define AUDIO_COPY_DSP_MEM_INFO(src, dst) \
	memcpy((void *)dst, (void *)src, \
	sizeof(struct dsp_mem_ifo))

#define AUDIO_COPY_COMPR_PARAM(src, dst) \
	memcpy((void *)dst, (void *)src, \
	sizeof(struct audio_compressed_params))

#define AUDIO_COPY_COMPR_META(src, dst) \
	memcpy((void *)dst, (void *)src, \
	sizeof(struct audio_compressed_metadata))

#define AUDIO_COPY_PCM_TSTAMP(src, dst) \
	memcpy((void *)dst, (void *)src, \
	sizeof(struct audio_pcm_tstamp))

#define AUDIO_COPY_COMPR_CODEC_CAP(src, dst) \
	memcpy((void *)dst, (void *)src, \
	sizeof(struct audio_compressed_codec_cap))

#define AUDIO_COPY_DSP_IRQ_PARAM(src, dst) \
	memcpy((void *)dst, (void *)src, \
	sizeof(struct dsp_irq_param))

#define AUDIO_COPY_DSP_DRAIN_DONE_PARAM(src, dst) \
	memcpy((void *)dst, (void *)src, \
	sizeof(struct dsp_drain_done_param))

#define AUDIO_COPY_DSP_FRAGMENT_DONE_PARAM(src, dst) \
	memcpy((void *)dst, (void *)src, \
	sizeof(struct dsp_fragment_done_param))

#define AUDIO_COPY_DSP_POWER_STATE(src, dst) \
	memcpy((void *)dst, (void *)src, \
	sizeof(struct dsp_power_state))

#define AUDIO_COPY_DSP_VA_NOTIFY(src, dst) \
	memcpy((void *)dst, (void *)src, \
	sizeof(struct dsp_ipc_va_notify))

#define AUDIO_IPC_COPY_DSP_DEBUG_START_PARAM(src, dst) \
	memcpy((void *)dst, (void *)src, sizeof(struct dsp_debug_start_param))

#define AUDIO_COPY_DSP_DEUBG_IRQ_PARAM(src, dst) \
	memcpy((void *)dst, (void *)src, \
	sizeof(struct dsp_debug_irq_param))

//if the host can't wait, it should not ack.
enum {
	MSG_TO_DSP_TOP_BASE = 0,
	MSG_TO_DSP_CREATE_VA_T = MSG_TO_DSP_TOP_BASE,//create va task
	MSG_TO_DSP_DESTROY_VA_T,	//destroy voice assistant(va) task
	MSG_TO_DSP_SCENE_VA_VAD,	//Enable Voice Activity Detection
	MSG_TO_DSP_SCENE_VA_KEYWORD,	//Enable Keyword detection
	MSG_TO_DSP_SCENE_VA_AEC,		//Enable AEC
	MSG_TO_DSP_SCENE_VA_PREPROCESSING,	// Enable Pre-processing
	MSG_TO_DSP_SCENE_VA_RECORD,		// Do Record
	MSG_TO_DSP_CREATE_CMP_OFFLOAD_T,//create compress offload task
	MSG_TO_DSP_DESTROY_CMP_OFFLOAD_T,//destroy compress offload task
	MSG_TO_DSP_CREATE_PCM_PLAYBACK_T,//create pcm playback task
	MSG_TO_DSP_DESTROY_PCM_PLAYBACK_T,//destroy pcm playback task
	MSG_TO_DSP_CREATE_CMP_OFFLOAD_2_T,//create compress offload task
	MSG_TO_DSP_DESTROY_CMP_OFFLOAD_2_T,//destroy compress offload task
	MSG_TO_DSP_CREATE_CMP_OFFLOAD_3_T,//create compress offload task
	MSG_TO_DSP_DESTROY_CMP_OFFLOAD_3_T,//destroy compress offload task
	MSG_TO_DSP_TOP_SET = MSG_TO_DSP_TOP_BASE + 0x200,
	MSG_TO_DSP_SET_POWER_STATE = MSG_TO_DSP_TOP_SET,// Set dsp power state
	MSG_TO_DSP_TOP_GET = MSG_TO_DSP_TOP_BASE + 0x400,
	MSG_TO_DSP_GET_POWER_STATE = MSG_TO_DSP_TOP_GET,// Get dsp power state
	MSG_TO_DSP_VA_BASE = MSG_TO_DSP_TOP_BASE + 0x600,
	MSG_TO_DSP_VA_VOICE_UPLOAD_DONE = MSG_TO_DSP_VA_BASE,
	MSG_TO_DSP_VA_BEAMFORMING, // get beamforming info
	MSG_TO_DSP_FE_BASE = 0x1000,
	MSG_TO_DSP_HOST_PORT_STARTUP = MSG_TO_DSP_FE_BASE,
	MSG_TO_DSP_HOST_HW_PARAMS,	//should ack
	MSG_TO_DSP_HOST_PREPARE,	//should ack
	MSG_TO_DSP_HOST_TRIGGER_START,	//should not ack
	MSG_TO_DSP_HOST_TRIGGER_PAUSE,	//should not ack
	MSG_TO_DSP_HOST_TRIGGER_STOP,	//should not ack
	MSG_TO_DSP_HOST_TRIGGER_RESUME,	//should not ack
	MSG_TO_DSP_HOST_TRIGGER_DRAIN,
	MSG_TO_DSP_HOST_TRIGGER_PARTIAL_DRAIN,
	MSG_TO_DSP_HOST_HW_FREE,	//should ack
	MSG_TO_DSP_HOST_CLOSE,		//should ack
	MSG_TO_DSP_HOST_QUERY_MEM_INFO,	//should ack
	MSG_TO_DSP_HOST_SET_METADATA,
	MSG_TO_DSP_HOST_GET_METADATA,
	MSG_TO_DSP_HOST_TRIGGER_NEXT_TRACK,//gapless
	MSG_TO_DSP_HOST_QUERY_CODEC_CAP,
	MSG_TO_DSP_HOST_QUERY_PCM_TSTAMP,	//should ack
	MSG_TO_DSP_HOST_QUERY_SUPPORT_FORMAT,//should ack
	MSG_TO_DSP_HOST_QUERY_PERIOD_SIZE,
	MSG_TO_DSP_HOST_SET_VOLUME,
	MSG_TO_DSP_HOST_GET_VOLUME,
	MSG_TO_DSP_BE_BASE = 0x2000,
	MSG_TO_DSP_DSP_PORT_STARTUP = MSG_TO_DSP_BE_BASE,
	MSG_TO_DSP_DSP_HW_PARAMS,	//should ack
	MSG_TO_DSP_DSP_PREPARE,		//should ack
	MSG_TO_DSP_DSP_TRIGGER_START,	//should not ack
	MSG_TO_DSP_DSP_TRIGGER_PAUSE,	//should not ack
	MSG_TO_DSP_DSP_TRIGGER_STOP,	//should not ack
	MSG_TO_DSP_DSP_TRIGGER_RESUME,	//should not ack
	MSG_TO_DSP_DSP_TRIGGER_DRAIN,
	MSG_TO_DSP_DSP_TRIGGER_PARTIAL_DRAIN,
	MSG_TO_DSP_DSP_HW_FREE,		//should ack
	MSG_TO_DSP_DSP_CLOSE,		//should ack
	MSG_TO_DSP_DSP_SET_VOLUME,
	MSG_TO_DSP_DSP_GET_VOLUME,
	MSG_TO_DSP_DEBUG_BASE = 0x4000,
	MSG_TO_DSP_DEBUG_START = MSG_TO_DSP_DEBUG_BASE,
	MSG_TO_DSP_DEBUG_STOP,
	MSG_TO_HOST_BASE = 0xf000,
	// message from DSP to host
	MSG_TO_HOST_DSP_IRQUL = MSG_TO_HOST_BASE,	// Uplink IRQ
	MSG_TO_HOST_DSP_IRQDL,			// Downlink IRQ
	MSG_TO_HOST_VA_NOTIFY,	// notify Host keyword detection or vad pass
	MSG_TO_HOST_DSP_AUDIO_READY,	// DSP notify Host that DSP is ready.
	MSG_TO_HOST_DSP_DRAIN_DONE,
	MSG_TO_HOST_DSP_PARTIAL_DRAIN_DONE,
	MSG_TO_HOST_DSP_FRAGMENT_DONE,
	MSG_TO_HOST_DSP_DEBUG_IRQ,
};

#endif // end of __AUDIO_SHARED_INFO_H__

