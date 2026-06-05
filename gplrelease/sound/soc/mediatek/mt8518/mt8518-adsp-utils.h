/*
 * mt8518-adsp-utils.h  --  Mediatek 8518 adsp utility
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

#ifndef _MT8518_ADSP_UTILS_H_
#define _MT8518_ADSP_UTILS_H_

#include "audio_task_manager.h"
#include "audio_shared_info.h"

#define HOST_VOLUME_MAX_LEVEL 100

#ifdef CONFIG_SND_SOC_COMPRESS
#ifndef CONFIG_SND_SOC_COMPRESS_NR_PLAYBACK_STREAMS
#define CONFIG_SND_SOC_COMPRESS_NR_PLAYBACK_STREAMS 1
#endif
#endif

enum {
	MT8518_ADSP_FE_MIC_RECORD = 0,
#ifdef CONFIG_SND_SOC_MT8518_ADSP_VOICE_ASSIST
	MT8518_ADSP_FE_VA_HOSTLESS,
	MT8518_ADSP_FE_VA_UPLOAD,
#endif
#ifdef CONFIG_SND_SOC_MT8518_ADSP_PCM_PLAYBACK
	MT8518_ADSP_FE_PCM_PLAYBACK1,
#endif
#ifdef CONFIG_SND_SOC_COMPRESS
#if CONFIG_SND_SOC_COMPRESS_NR_PLAYBACK_STREAMS > 0
	MT8518_ADSP_FE_COMPR_PLAYBACK1,
#endif
#if CONFIG_SND_SOC_COMPRESS_NR_PLAYBACK_STREAMS > 1
	MT8518_ADSP_FE_COMPR_PLAYBACK2,
#endif
#if CONFIG_SND_SOC_COMPRESS_NR_PLAYBACK_STREAMS > 2
	MT8518_ADSP_FE_COMPR_PLAYBACK3,
#endif
#endif
	MT8518_ADSP_FE_CNT,
	MT8518_ADSP_BE_START = MT8518_ADSP_FE_CNT,
	MT8518_ADSP_BE_MIC_RECORD = MT8518_ADSP_BE_START,
	MT8518_ADSP_BE_PRIMARY_PLAYBACK,
	MT8518_ADSP_BE_END,
	MT8518_ADSP_BE_CNT = MT8518_ADSP_BE_END - MT8518_ADSP_BE_START,
#ifdef CONFIG_SND_SOC_MT8518_ADSP_PCM_PLAYBACK
	MT8518_ADSP_FE_PCM_BASE = MT8518_ADSP_FE_PCM_PLAYBACK1,
	MT8518_ADSP_FE_PCM_END = MT8518_ADSP_FE_PCM_PLAYBACK1,
	MT8518_ADSP_FE_PCM_CNT = MT8518_ADSP_FE_PCM_END -
				 MT8518_ADSP_FE_PCM_BASE + 1,
#endif
#ifdef CONFIG_SND_SOC_COMPRESS
	MT8518_ADSP_FE_COMPR_BASE = MT8518_ADSP_FE_COMPR_PLAYBACK1,
	MT8518_ADSP_FE_COMPR_END = MT8518_ADSP_FE_COMPR_PLAYBACK1 +
		CONFIG_SND_SOC_COMPRESS_NR_PLAYBACK_STREAMS - 1,
	MT8518_ADSP_FE_COMPR_CNT = CONFIG_SND_SOC_COMPRESS_NR_PLAYBACK_STREAMS,
#endif
};

struct mt8518_dsp_dma_buffer {
	uint32_t buf_paddr;
	uint32_t buf_size;
	uint32_t hw_offset_paddr;
	uint32_t appl_offset_paddr;
	uint32_t hw_offset;
	uint32_t appl_offset;
};

struct mt8518_host_dma_buffer {
	unsigned char *buf_vaddr;
	uint32_t buf_size;
	uint32_t hw_offset;
	uint32_t transfer;
};

int mt8518_adsp_get_scene_by_dai_id(int id);

bool mt8518_adsp_need_ul_dma_copy(int id);

bool mt8518_adsp_need_dl_dma_copy(int id);

bool mt8518_adsp_is_compress_scene(int scene);

bool mt8518_adsp_is_compress_playback_scene(int scene);

int mt8518_adsp_activate_compr(int id, atomic_t *active);

int mt8518_adsp_deactivate_compr(int id, atomic_t *active);

int mt8518_adsp_send_ipi_cmd(struct ipi_msg_t *p_msg,
			     uint8_t task_scene,
			     uint8_t target,
			     uint8_t data_type,
			     uint8_t ack_type,
			     uint16_t msg_id,
			     uint32_t param1,
			     uint32_t param2,
			     char *payload);

int mt8518_adsp_verify_ack_hw_param(struct host_ipc_msg_hw_param *host_param,
	struct ipi_msg_t *ack_msg,
	struct dsp_ipc_msg_hw_param *dsp_param);

int mt8518_adsp_verify_ack_dbg_param(struct host_debug_start_param *host_param,
	struct ipi_msg_t *ack_msg,
	struct dsp_debug_start_param *dsp_param);

int mt8518_adsp_verify_ack_vol_info(struct ipi_msg_t *ack_msg,
	struct dsp_vol_info *vol_info);

int mt8518_adsp_etdm_format(unsigned int format);

int mt8518_adsp_etdm_data_mode(unsigned int mode);

int mt8518_adsp_etdm_clock_mode(unsigned int mode);

int mt8518_adsp_init_dsp_dmab(struct mt8518_dsp_dma_buffer *dmab);

int mt8518_adsp_reset_dsp_dmab_offset(struct mt8518_dsp_dma_buffer *dmab);

int mt8518_adsp_init_host_dmab(struct mt8518_host_dma_buffer *dmab);

int mt8518_adsp_reset_host_dmab_offset(struct mt8518_host_dma_buffer *dmab);

int mt8518_adsp_notify_power_state(int scene);

int mt8518_adsp_clear_power_state(int scene);

int mt8518_adsp_host_to_dsp_vol(int level, struct dsp_vol_info *vol_info);

int mt8518_adsp_dsp_to_host_vol(struct dsp_vol_info *vol_info, int *level);

#endif
