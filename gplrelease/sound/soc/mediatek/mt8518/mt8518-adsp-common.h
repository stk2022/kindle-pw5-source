/*
 * mt8518-adsp-common.h  --  Mediatek 8518 adsp driver common definitions
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

#ifndef _MT8518_ADSP_COMMON_H_
#define _MT8518_ADSP_COMMON_H_

#include "mt8518-adsp-utils.h"

struct mt8518_adsp_dai_memory {
	int id;
	struct snd_pcm_substream *substream;
	struct mt8518_dsp_dma_buffer dsp_dmab;
	struct mt8518_host_dma_buffer host_dmab;
	struct work_struct dma_work;
	struct workqueue_struct *dma_wq;
	struct hrtimer dma_elapse_hrt;
	unsigned long period_time_ns;
	atomic_t dma_pending_elapse;
	struct timespec prev_elapse_ts;
	spinlock_t host_dma_lock;
	struct device *dev;
};

struct mt8518_audio_spi_be_data {
	bool slave_mode;
	bool lrck_inv;
	bool bck_inv;
	unsigned int lrck_width;
	unsigned int data_mode;
	unsigned int format;
	unsigned int mclk_freq;
	unsigned int clock_mode;
};

struct mt8518_adsp_control_data {
	bool wakeword_detect_en;
	int master_vol;
#ifdef CONFIG_SND_SOC_MT8518_ADSP_PCM_PLAYBACK
	int pcm_vol[MT8518_ADSP_FE_PCM_CNT];
#endif
#ifdef CONFIG_SND_SOC_COMPRESS
	int compress_vol[MT8518_ADSP_FE_COMPR_CNT];
#endif
};

struct mt8518_adsp_volume {
	struct dsp_vol_info master;
#ifdef CONFIG_SND_SOC_MT8518_ADSP_PCM_PLAYBACK
	struct dsp_vol_info pcm[MT8518_ADSP_FE_PCM_CNT];
#endif
#ifdef CONFIG_SND_SOC_COMPRESS
	struct dsp_vol_info compress[MT8518_ADSP_FE_COMPR_CNT];
#endif
};

struct mt8518_audio_spi_priv {
	struct device *dev;
	phys_addr_t reserved_memory_paddr;
	void __iomem *reserved_memory_vaddr;
	u64 reserved_memory_size;
	wait_queue_head_t wait_dsp;
	bool dsp_ready;
	bool dsp_init;
	struct mt8518_adsp_dai_memory dai_mem[MT8518_ADSP_FE_CNT];
	struct dsp_mem_ifo mem_info[MT8518_ADSP_FE_CNT];
	struct mt8518_audio_spi_be_data be_data[MT8518_ADSP_BE_CNT];
	struct mt8518_adsp_volume adsp_vol;
	struct mt8518_adsp_control_data ctrl_data;
#ifdef CONFIG_SND_SOC_COMPRESS
	atomic_t compr_active[MT8518_ADSP_FE_COMPR_CNT];
#endif
#ifdef CONFIG_DEBUG_FS
	void *dbg_data;
#endif
};

#endif
