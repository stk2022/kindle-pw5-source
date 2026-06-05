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

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/byteorder/generic.h>

#include "mtk_png_parse.h"

#define PNG_REACH_END		(stream->curr >= stream->size)

struct mtk_png_stream {
	unsigned char *addr;
	unsigned int size;
	unsigned int curr;
};

static bool read_byte(struct mtk_png_stream *stream, unsigned char *value)
{
	if ((stream->curr + 1) > stream->size)
		return false;
	*value = stream->addr[stream->curr];
	stream->curr++;

	return true;
}

static bool read_word(struct mtk_png_stream *stream, u32 *value)
{
	if ((stream->curr + 4) > stream->size)
		return false;
	*value = *((u32 *)(&stream->addr[stream->curr]));
	*value = be32_to_cpu(*value);
	stream->curr += 4;

	return true;
}

static bool read_dword(struct mtk_png_stream *stream, u64 *value)
{
	if ((stream->curr + 8) > stream->size)
		return false;
	*value = *((u64 *)(&stream->addr[stream->curr]));
	*value = be64_to_cpu(*value);
	stream->curr += 8;

	return true;
}

static bool read_skip(struct mtk_png_stream *stream, unsigned int len)
{
	if ((stream->curr + len) > stream->size)
		return false;
	stream->curr += len;
	return true;
}

static bool mtk_png_parse_chunk(struct mtk_png_dec_param *param,
				struct mtk_png_stream *stream)
{
	u32 chunk_size = 0;
	u32 chunk_type = 0;
	bool idat_found = false;
	bool ret = false;

	while (1) {
		if (PNG_REACH_END)
			return ret;

		if (!read_word(stream, &chunk_size))
			return false;
		if ((stream->curr + 4 + chunk_size + 4) > stream->size)
			return false;
		read_word(stream, &chunk_type);
		if (param->ihdr.ihdr_exist == false &&
		    chunk_type != PNG_CHUNK_IHDR)
			break;

		switch (chunk_type) {
		case PNG_CHUNK_IHDR:
			param->ihdr.ihdr_exist = true;
			read_word(stream, &param->ihdr.pic_w);
			read_word(stream, &param->ihdr.pic_h);
			read_byte(stream, &param->ihdr.bit_depth);
			read_byte(stream, &param->ihdr.color_type);
			read_byte(stream, &param->ihdr.compression_method);
			read_byte(stream, &param->ihdr.filter_method);
			read_byte(stream, &param->ihdr.interlace_method);
			break;
		case PNG_CHUNK_IDAT:
			param->idat.num++;
			if (!idat_found) {
				idat_found = true;
				param->idat.first_offset = stream->curr - 8;
				param->idat.size = chunk_size + 8 + 4;
			} else
				param->idat.size = stream->curr + chunk_size + 4
					- param->idat.first_offset;
			read_skip(stream, chunk_size);
			break;
		case PNG_CHUNK_PLTE:
			param->plte.exist = true;
			param->plte.offset = stream->curr;
			param->plte.size = chunk_size;
			read_skip(stream, chunk_size);
			break;
		case PNG_CHUNK_tRNS:
			param->trns.exist = true;
			param->trns.offset = stream->curr;
			param->trns.size = chunk_size;
			read_skip(stream, chunk_size);
			break;
		case PNG_CHUNK_IEND:
			param->iend_exist = true;
			break;
		default:
			read_skip(stream, chunk_size);
			break;
		}
		read_skip(stream, 4);
		if (param->iend_exist) {
			ret = true;
			break;
		}
	}
	return ret;
}

static bool mtk_png_parse_hdr(struct mtk_png_dec_param *param,
			struct mtk_png_stream *stream)
{
	u64 pattern_64;
	bool ret = 0;

	if (!read_dword(stream, &pattern_64))
		return false;
	if (pattern_64 != PNG_SIGNATURE)
		pr_err("PNG_SIGNATURE not found 0x%llx 0x%llx!\n",
			pattern_64, PNG_SIGNATURE);

	ret = mtk_png_parse_chunk(param, stream);
	if (!ret)
		pr_err("Reach end abnormally when parsing chunk, file maybe conrrupt!\n");

	if ((!param->ihdr.ihdr_exist) ||
	    (!param->iend_exist) ||
	    (param->idat.num == 0)) {
		pr_err("Critical chunk not found! IHDR:%d, IDAT:%d, IEND:%d\n",
			param->ihdr.ihdr_exist, param->iend_exist,
			param->idat.num);
		return false;
	}

	return true;
}

bool mtk_png_parse(struct mtk_png_dec_param *param, u8 *src_addr_va,
		    u32 src_size)
{
	struct mtk_png_stream stream;

	stream.addr = src_addr_va;
	stream.size = src_size;
	stream.curr = 0;

	if (!mtk_png_parse_hdr(param, &stream))
		return false;

	return true;
}

