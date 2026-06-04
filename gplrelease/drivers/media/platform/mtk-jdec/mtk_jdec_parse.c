/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Scott Wang <Scott.Wang@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/videodev2.h>

#include "mtk_jdec_parse.h"

#define TEM	0x01
#define SOF0	0xc0
#define SOF1	0xc1
#define SOF2	0xc2
#define SOF3	0xc3
#define DHT     0xc4
#define SOF5	0xc5
#define SOF6	0xc6
#define SOF7	0xc7
#define SOF8	0xc8
#define SOF9	0xc9
#define SOF10	0xca
#define SOF11	0xcb
#define SOF13	0xcd
#define SOF14	0xce
#define SOF15	0xcf
#define RST	0xd0
#define SOI	0xd8
#define EOI	0xd9
#define SOS	0xda
#define DQT	0xdb
#define DRI	0xdd

#define CHK(x)	(line = __LINE__, cond = #x, x)

static int read_byte(struct mtk_jdec_stream *stream, u8 *value)
{
	if (stream->curr + 1 > stream->size)
		return -1;
	*value = stream->addr[stream->curr++];

	return 0;
}

static int read_word(struct mtk_jdec_stream *stream, u16 *value)
{
	if (stream->curr + 2 > stream->size)
		return -1;
	*value = *((u16 *)(&stream->addr[stream->curr]));
	*value = be16_to_cpu(*value);
	stream->curr += 2;

	return 0;
}

static int read_skip(struct mtk_jdec_stream *stream, long len)
{
	if ((stream->curr + len) > stream->size)
		return -1;
	stream->curr += len;
	return 0;
}

static bool mtk_jdec_do_parse(struct mtk_jdec_dec_param *param)
{
	bool filestart = false, fileend = false;
	bool sof_exist = false, sos_exist = false;
	int line;
	const char *cond;
	struct mtk_jdec_dqt_param *dqt = &param->dqt;
	struct mtk_jdec_sof_param *sof = &param->sof;
	struct mtk_jdec_sos_param *sos = &param->sos;
	struct mtk_jdec_stream *stream = &param->stream;
	struct mtk_jdec_dht_param *dht = &param->dht;

	/* need check huffman for hardware enhance */
	while (!fileend && !sos_exist) {
		int i;
		u8 byte = 0;
		u16 length = 0;
		u8 table_class, table_id, size;
		struct mtk_jdec_dht_tab *dht_tab;

		if (CHK(read_byte(stream, &byte)))
			goto err;
		if (byte != 0xff)
			continue;
		do {
			if (CHK(read_byte(stream, &byte)))
				goto err;
		} while (byte == 0xff);
		if (byte == 0)
			continue;

		switch (byte) {
		case SOF0 ... SOF2:

			if (CHK(sof_exist))
				goto err;
			if (byte == SOF2) {
				param->dec_md =
					JDEC_DEC_MODE_PROGRESSIVE_SCAN_ENHANCE;
				pr_err("sof2: not support progressive jpeg\n");
				goto err;
			}
			else
				param->dec_md = JDEC_DEC_MODE_BASELINE_PIC;
			/* length */
			read_word(stream, &length);
			/* precision */
			read_byte(stream, &byte);
			read_word(stream, &sof->pic_h);
			read_word(stream, &sof->pic_w);
			if (CHK(0 == sof->pic_h || 0 == sof->pic_w))
				goto err;

			read_byte(stream, &sof->comp_num);
			if (CHK(sof->comp_num != 1 && sof->comp_num != 3))
				goto err;

			for (i = 0; i < sof->comp_num; i++) {
				read_byte(stream, &sof->comp_id[i]);

				/* sampling */
				read_byte(stream, &byte);
				sof->sampling_h[i] = byte >> 4;
				sof->sampling_v[i] = byte & 0xf;
				if (CHK(sof->sampling_h[i] == 0 ||
				    sof->sampling_v[i] == 0))
					goto err;
				read_byte(stream, &sof->qtbl_num[i]);
			}
			if (CHK(i != sof->comp_num))
				goto err;
			sof_exist = true;
			break;
		case SOF3:
		case SOF5 ... SOF11:
		case SOF13 ... SOF15:
			return false;
		case SOS:
			if (CHK(!sof_exist))
				goto err;
			read_word(stream, &length);
			read_byte(stream, &sos->comp_num);
			for (i = 0; i < sos->comp_num; i++) {
				read_byte(stream, &sos->comp_id[i]);
				read_byte(stream, &byte);
				sos->dc_id[i] = byte >> 4;
				sos->ac_id[i] = byte & 0xf;
			}
			read_byte(stream, &sos->ss);
			read_byte(stream, &sos->se);
			read_byte(stream, &byte);
			sos->ah = byte >> 4;
			sos->al = byte & 0xf;
			sos_exist = true;
			break;

		case SOI:
			filestart = true;
			break;
		case TEM:
		case RST ... RST + 7:
			break;
		case EOI:
			fileend = true;
			break;
		case DHT:
			read_word(stream, &length);
			length -= 2;
			while (length > 1 + 16) {
				read_byte(stream, &byte);
				table_class = byte >> 4;
				if (CHK(table_class != 0 && table_class != 1))
					goto err;
				table_id = byte & 0xf;
				if (CHK(table_id >= 4))
					goto err;
				dht_tab = table_class ?
					&dht->ac[table_id] : &dht->dc[table_id];
				for (i = 1; i < 17; i++) {
					read_byte(stream, &dht_tab->num[i]);
					dht_tab->val_cnt += dht_tab->num[i];
				}
				if (CHK(length < 1 + 16 + dht_tab->val_cnt))
					goto err;
				for (i = 0; i < dht_tab->val_cnt; i++)
					read_byte(stream, &dht_tab->val[i]);
				length -= 1 + 16 + dht_tab->val_cnt;
			}
			break;
		case DQT:
			read_word(stream, &length);
			while (length > 64) {
				read_byte(stream, &byte);
				table_id = byte & 0xf;
				if (CHK(table_id > 3))
					goto err;
				dqt->precision[table_id] = byte >> 4;
				size = dqt->precision[table_id] == 1 ? 128 : 64;
				memcpy(dqt->element[table_id],
					stream->addr + stream->curr, size);
				read_skip(stream, size);
				length -= (1 + size);
				dqt->num++;
				dqt->exist = true;
			}
			break;
		case DRI:
			read_word(stream, &length);
			read_word(stream, &param->dri.restart_interval);
			break;
		default:
			read_word(stream, &length);
			read_skip(stream, length - 2);
			break;
		}
	}
	if (sof_exist && filestart && dqt->exist)
		return true;

	pr_err("%s failed: SOI:%d, SOF:%d, dqh:%d, SOS:%d, end:%d\n",
		__func__, filestart, sof_exist,
		dqt->exist, sos_exist, fileend);
	return false;

err:
	pr_err("%s failed at L:%d '%s', stream:0x%p~0x%p, curr:0x%p\n",
		__func__, line, cond, stream->addr,
		stream->addr + stream->size, stream->addr + stream->curr);
	return false;
}

static int mtk_jdec_fill_dec_param(struct mtk_jdec_dec_param *param)
{
	int i;

	param->wid_per_mcu = param->sof.sampling_h[0] * 8;
	param->hei_per_mcu = param->sof.sampling_v[0] * 8;
	param->mcu_per_row =
		DIV_ROUND_UP(param->sof.pic_w, param->wid_per_mcu);
	param->total_mcu_row =
		DIV_ROUND_UP(param->sof.pic_h, param->hei_per_mcu);
	for (i = 0; i < param->sof.comp_num; i++) {
		param->out.pitch[i] =
			ALIGN(ALIGN(param->sof.pic_w, param->wid_per_mcu), 16);
		param->out.pitch[i] = ALIGN(param->out.pitch[i] /
			(param->sof.sampling_h[0] / param->sof.sampling_h[i]),
			16);
	}

	return 0;
}

bool mtk_jdec_parse(struct mtk_jdec_dec_param *param)
{
	if (!mtk_jdec_do_parse(param))
		return false;
	if (mtk_jdec_fill_dec_param(param))
		return false;

	return true;
}

