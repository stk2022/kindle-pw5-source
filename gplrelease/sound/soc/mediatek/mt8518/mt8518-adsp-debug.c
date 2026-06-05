/*
 * mt8518-adsp-debug.c  --  Mediatek 8518 adsp debug function
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

#include <linux/sysfs.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <sound/core.h>
#include <sound/memalloc.h>
#include "mt8518-adsp-debug.h"
#include "mt8518-adsp-common.h"
#include "mach/mtk_hifi4dsp_api.h"


#ifdef CONFIG_DEBUG_FS
/* #define MT8518_AUDIO_SPI_DUMP_DMA_DEBUG */

#define DEFAULT_DUMP_FILE_SIZE		(1*1024*1024)
#define DEFAULT_DUMP_BUFFER_SIZE	(64*1024)
#define DEFAULT_DUMP_WAIT_TIMEOUT_SEC	(30)

struct mt8518_adsp_debugfs_attr {
	char *fs_name;
	const struct file_operations *fops;
	u32 scene;
};

struct mt8518_adsp_debugfs_data {
	struct device *device;
	struct dentry *root;
	u64 size;
	u64 ack_size;
	u32 scene;
	u32 task;
	u32 priv[4];
	u32 buffer_size;
	u32 timeout;
	bool enable;
	struct mt8518_dsp_dma_buffer dsp_dmab;
	struct snd_dma_buffer host_dmab;
	uint32_t host_hw_off;
	uint32_t host_appl_off;
	struct work_struct dump_work;
	wait_queue_head_t wait_data;
};

static uint32_t dump_data_from_dsp(struct mt8518_adsp_debugfs_data *dump)
{
	struct mt8518_dsp_dma_buffer *dsp_dmab = &dump->dsp_dmab;
	struct snd_dma_buffer *host_dmab = &dump->host_dmab;
	uint32_t adsp_dma_buf_paddr = dsp_dmab->buf_paddr;
	uint32_t adsp_dma_buf_size = dsp_dmab->buf_size;
	uint32_t adsp_dma_hw_off = 0;
	uint32_t adsp_dma_appl_off = dsp_dmab->appl_offset;
	unsigned char *host_dma_buf_vaddr = host_dmab->area;
	uint32_t host_dma_buf_size = host_dmab->bytes;
	uint32_t host_dma_hw_off = dump->host_hw_off;
	uint32_t host_appl_off = dump->host_appl_off;
	uint32_t avail;
	uint32_t dst_avail;
	uint32_t copy;
	uint32_t copied = 0;

	dsp_spi_read(dsp_dmab->hw_offset_paddr,
		     &adsp_dma_hw_off,
		     sizeof(adsp_dma_hw_off), SPI_SPEED_HIGH);

	if (adsp_dma_hw_off >= adsp_dma_appl_off) {
		avail = adsp_dma_hw_off - adsp_dma_appl_off;
	} else {
		avail = adsp_dma_buf_size - adsp_dma_appl_off +
			adsp_dma_hw_off;
	}

	if (host_dma_hw_off >= host_appl_off) {
		dst_avail = host_dma_buf_size - host_appl_off +
			    host_dma_hw_off;
	} else {
		dst_avail = host_dma_hw_off - host_appl_off;
	}

	copy = min(avail, dst_avail);

	if (copy == 0)
		return 0;

	while (copy > 0) {
		uint32_t from = 0;

		if (adsp_dma_hw_off >= adsp_dma_appl_off)
			from = adsp_dma_hw_off - adsp_dma_appl_off;
		else
			from = adsp_dma_buf_size - adsp_dma_appl_off;

		if (from > copy)
			from = copy;

		while (from > 0) {
			uint32_t to = 0;

			if (host_dma_hw_off + from <= host_dma_buf_size)
				to = from;
			else
				to = host_dma_buf_size - host_dma_hw_off;

#ifdef MT8518_AUDIO_SPI_DUMP_DMA_DEBUG
			pr_info("%s [%u] copy %u bytes from 0x%x to %p\n",
				__func__, dump->scene, to,
				adsp_dma_buf_paddr + adsp_dma_appl_off,
				host_dma_buf_vaddr + host_dma_hw_off);
#endif

			dsp_spi_read_ex(adsp_dma_buf_paddr + adsp_dma_appl_off,
					host_dma_buf_vaddr + host_dma_hw_off,
					to, SPI_SPEED_HIGH);

			from -= to;
			copy -= to;
			copied += to;

			host_dma_hw_off = (host_dma_hw_off + to) %
				host_dma_buf_size;
			adsp_dma_appl_off = (adsp_dma_appl_off + to) %
				adsp_dma_buf_size;
		}
	}

	dsp_spi_write(dsp_dmab->appl_offset_paddr,
		      &adsp_dma_appl_off,
		      sizeof(adsp_dma_appl_off), SPI_SPEED_HIGH);

	dsp_dmab->appl_offset = adsp_dma_appl_off;
	dsp_dmab->hw_offset = adsp_dma_hw_off;
	dump->host_hw_off = host_dma_hw_off;

	return copied;
}

static void dump_process(struct work_struct *work)
{
	struct mt8518_adsp_debugfs_data *dump = container_of(work,
		struct mt8518_adsp_debugfs_data, dump_work);

	if (dump_data_from_dsp(dump) > 0)
		wake_up_interruptible(&dump->wait_data);
}

static ssize_t mt8518_adsp_dump_read_from_dsp(char __user *buf,
	size_t size,
	struct mt8518_adsp_debugfs_data *dump)
{
	unsigned char *host_dma_buf_vaddr = dump->host_dmab.area;
	uint32_t host_dma_buf_size = dump->host_dmab.bytes;
	uint32_t host_dma_hw_off = dump->host_hw_off;
	uint32_t host_appl_off = dump->host_appl_off;
	uint32_t timeout_ms = dump->timeout * MSEC_PER_SEC;
	ssize_t read = 0;
	size_t copy = size;
	long ret;

	while (copy > 0) {
		uint32_t from = 0;

		host_dma_hw_off = dump->host_hw_off;

		if (host_dma_hw_off >= host_appl_off)
			from = host_dma_hw_off - host_appl_off;
		else
			from = host_dma_buf_size - host_appl_off;

		if (from > copy)
			from = copy;

		if (from > 0) {
#ifdef MT8518_AUDIO_SPI_DUMP_DMA_DEBUG
			pr_info("%s [%u] copy %u bytes from %p to %p\n",
				__func__, dump->scene, from,
				host_dma_buf_vaddr + host_appl_off,
				buf + read);
#endif
			if (copy_to_user_fromio(buf + read,
						host_dma_buf_vaddr +
						host_appl_off, from)) {
				pr_info("%s copy error\n", __func__);
				goto endofdump;
			}

			copy -= from;
			read += from;
			host_appl_off += from;
			host_appl_off %= host_dma_buf_size;

			continue;
		}

		ret = wait_event_interruptible_timeout(dump->wait_data,
			(dump->host_hw_off != host_appl_off),
			msecs_to_jiffies(timeout_ms));
		if (ret == 0) {
			pr_info("%s timeout\n", __func__);
			break;
		}
	}

endofdump:
	dump->host_appl_off = host_appl_off;

	return read;
}

static int mt8518_adsp_dump_open(struct inode *inode,
	struct file *file)
{
	struct mt8518_adsp_debugfs_data *dump = inode->i_private;
	struct ipi_msg_t ipi_msg;
	struct host_debug_start_param param;
	struct dsp_debug_start_param ack_param;
	struct io_ipc_ring_buf_shared *shared_buf;
	int ret;

	ret = simple_open(inode, file);
	if (ret)
		return ret;

	dump->ack_size = 0;

	mt8518_adsp_notify_power_state(dump->scene);

	mt8518_adsp_init_dsp_dmab(&dump->dsp_dmab);

	ret = snd_dma_alloc_pages(SNDRV_DMA_TYPE_DEV,
		dump->device,
		dump->buffer_size,
		&dump->host_dmab);
	if (ret)
		return ret;

	memset(&param, 0, sizeof(param));
	memset(&ipi_msg, 0, sizeof(ipi_msg));

	param.host_handler = dump;
	param.task_param = dump->task;
	param.max_byte = dump->size;
	memcpy(param.priv_param, dump->priv, sizeof(param.priv_param));

	ret = mt8518_adsp_send_ipi_cmd(&ipi_msg,
		TASK_SCENE_AUDIO_CONTROLLER,
		AUDIO_IPI_LAYER_TO_DSP,
		AUDIO_IPI_PAYLOAD,
		AUDIO_IPI_MSG_NEED_ACK,
		MSG_TO_DSP_DEBUG_START,
		sizeof(param),
		0,
		(char *)&param);
	if (ret)
		return ret;

	ret = mt8518_adsp_verify_ack_dbg_param(&param, &ipi_msg, &ack_param);
	if (ret)
		return ret;

	shared_buf = &ack_param.SharedRingBuffer;

	dump->dsp_dmab.buf_paddr = shared_buf->start_addr;
	dump->dsp_dmab.buf_size = shared_buf->size_bytes;
	dump->dsp_dmab.hw_offset_paddr = shared_buf->ptr_to_hw_offset_bytes;
	dump->dsp_dmab.appl_offset_paddr =
		shared_buf->ptr_to_appl_offset_bytes;

	dump->ack_size = ack_param.max_byte;

	dump->enable = true;

	return 0;
}

static int mt8518_adsp_dump_release(struct inode *inode,
	struct file *file)
{
	struct mt8518_adsp_debugfs_data *dump = file->private_data;

	cancel_work_sync(&dump->dump_work);

	if (dump->enable) {
		struct host_debug_stop_param param;

		param.host_handler = dump;
		param.task_param = dump->task;
		memcpy(param.priv_param, dump->priv,
		       sizeof(param.priv_param));

		mt8518_adsp_send_ipi_cmd(NULL,
			TASK_SCENE_AUDIO_CONTROLLER,
			AUDIO_IPI_LAYER_TO_DSP,
			AUDIO_IPI_PAYLOAD,
			AUDIO_IPI_MSG_NEED_ACK,
			MSG_TO_DSP_DEBUG_STOP,
			sizeof(param),
			0,
			(char *)&param);
	}

	mt8518_adsp_clear_power_state(dump->scene);

	if (dump->host_dmab.bytes > 0) {
		snd_dma_free_pages(&dump->host_dmab);
		memset(&dump->host_dmab, 0, sizeof(dump->host_dmab));
	}

	dump->host_hw_off = 0;
	dump->host_appl_off = 0;
	dump->enable = false;

	return 0;
}

static ssize_t mt8518_adsp_dump_read(struct file *file,
	char __user *buf, size_t len, loff_t *ppos)
{
	struct mt8518_adsp_debugfs_data *dump = file->private_data;
	ssize_t read = 0;
	ssize_t to_read = 0;

	if (*ppos < 0 || !len)
		return -EINVAL;

	if (dump->ack_size > 0)
		to_read = (dump->ack_size - *ppos);
	else
		to_read = (dump->size - *ppos);

	if (to_read > len)
		to_read = len;
	else if (to_read < 0)
		to_read = 0;

	read = mt8518_adsp_dump_read_from_dsp(buf + read,
		to_read, dump);
	if (read > 0)
		*ppos += read;

	return read;
}

static const struct file_operations mt8518_adsp_dump_fops = {
	.open = mt8518_adsp_dump_open,
	.read = mt8518_adsp_dump_read,
	.llseek = noop_llseek,
	.release = mt8518_adsp_dump_release,
};

static const struct mt8518_adsp_debugfs_attr adsp_debugfs_attrs[] = {
	{"adsp_dump_1", &mt8518_adsp_dump_fops, TASK_SCENE_DEBUG_DUMP1},
	{"adsp_dump_2", &mt8518_adsp_dump_fops, TASK_SCENE_DEBUG_DUMP2},
	{"adsp_dump_3", &mt8518_adsp_dump_fops, TASK_SCENE_DEBUG_DUMP3},
};
#endif

void mt8518_adsp_init_debug_dump(struct mt8518_audio_spi_priv *priv)
{
#ifdef CONFIG_DEBUG_FS
	size_t i, j;
	size_t debugfs_count = ARRAY_SIZE(adsp_debugfs_attrs);
	size_t data_size = sizeof(struct mt8518_adsp_debugfs_data) *
			   debugfs_count;
	struct mt8518_adsp_debugfs_data *dump_data_base;
	struct mt8518_adsp_debugfs_data *dump_data;
	const struct mt8518_adsp_debugfs_attr *attr;

	priv->dbg_data = devm_kzalloc(priv->dev, data_size, GFP_KERNEL);
	if (!priv->dbg_data)
		return;

	dump_data_base = priv->dbg_data;

	for (i = 0; i < debugfs_count; i++) {
		dump_data = dump_data_base + i;
		attr = &adsp_debugfs_attrs[i];

		dump_data->device = priv->dev;
		dump_data->size = DEFAULT_DUMP_FILE_SIZE;
		dump_data->buffer_size = DEFAULT_DUMP_BUFFER_SIZE;
		dump_data->timeout = DEFAULT_DUMP_WAIT_TIMEOUT_SEC;
		dump_data->scene = attr->scene;

		INIT_WORK(&dump_data->dump_work, dump_process);
		init_waitqueue_head(&dump_data->wait_data);

		dump_data->root = debugfs_create_dir(attr->fs_name, NULL);
		if (IS_ERR_OR_NULL(dump_data->root))
			continue;

		debugfs_create_u64("size",
			0644,
			dump_data->root,
			&dump_data->size);

		debugfs_create_u64("ack_size",
			0444,
			dump_data->root,
			&dump_data->ack_size);

		debugfs_create_u32("scene",
			0444,
			dump_data->root,
			&dump_data->scene);

		debugfs_create_u32("task",
			0644,
			dump_data->root,
			&dump_data->task);

		for (j = 0; j < ARRAY_SIZE(dump_data->priv); j++) {
			char priv_name[16];

			snprintf(priv_name, sizeof(priv_name),
				 "priv[%zu]", j);

			debugfs_create_u32(priv_name,
				0644,
				dump_data->root,
				&dump_data->priv[j]);
		}

		debugfs_create_u32("buffer_size",
			0644,
			dump_data->root,
			&dump_data->buffer_size);

		debugfs_create_u32("timeout",
			0644,
			dump_data->root,
			&dump_data->timeout);

		debugfs_create_file_size("data",
			0444,
			dump_data->root,
			dump_data,
			attr->fops,
			0);

		debugfs_create_bool("enable",
			0444,
			dump_data->root,
			&dump_data->enable);
	}
#endif
}

void mt8518_adsp_cleanup_debug_dump(struct mt8518_audio_spi_priv *priv)
{
#ifdef CONFIG_DEBUG_FS
	size_t i;
	size_t debugfs_count = ARRAY_SIZE(adsp_debugfs_attrs);
	struct mt8518_adsp_debugfs_data *dump_data_base;
	struct mt8518_adsp_debugfs_data *dump_data;

	dump_data_base = priv->dbg_data;
	if (!dump_data_base)
		return;

	for (i = 0; i < debugfs_count; i++) {
		dump_data = dump_data_base + i;

		debugfs_remove_recursive(dump_data->root);

		dump_data->root = NULL;
	}
#endif
}

void mt8518_adsp_handle_debug_dump_irq(struct ipi_msg_t *p_ipi_msg)
{
#ifdef CONFIG_DEBUG_FS
	struct dsp_debug_irq_param param;
	struct mt8518_adsp_debugfs_data *dump;

	AUDIO_COPY_DSP_DEUBG_IRQ_PARAM(p_ipi_msg->payload, &param);

	dump = param.host_handler;
	if (!dump) {
		pr_info("%s invalid handle\n", __func__);
		return;
	}

	queue_work(system_unbound_wq, &dump->dump_work);
#endif
}

