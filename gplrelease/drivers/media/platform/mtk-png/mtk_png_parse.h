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

#ifndef MTK_PNG_PARSE_H
#define MTK_PNG_PARSE_H

#define PNG_SIGNATURE		(0x89504E470D0A1A0AULL)
#define PNG_CHUNK_IHDR		(0x49484452)
#define PNG_CHUNK_IDAT		(0x49444154)
#define PNG_CHUNK_PLTE		(0x504C5445)
#define PNG_CHUNK_tRNS		(0x74524E53)
#define PNG_CHUNK_bKGD		(0x624B4744)
#define PNG_CHUNK_IEND		(0x49454E44)

struct mtk_png_ihdr_param {
	bool	ihdr_exist;
	u32	pic_w;
	u32	pic_h;
	u8	bit_depth;
	u8	color_type;
	u8	compression_method;
	u8	filter_method;
	u8	interlace_method;
};

struct mtk_png_idat_param {
	unsigned int	num;
	unsigned int	first_offset;
	unsigned int	size;
};

struct mtk_png_plte_param {
	bool		exist;
	unsigned int	offset;
	unsigned int	size;
};

struct mtk_png_trns_param {
	bool			exist;
	unsigned int	offset;
	unsigned int	size;
};

struct mtk_png_dec_param {
	bool	iend_exist;
	struct mtk_png_ihdr_param	ihdr;
	struct mtk_png_idat_param	idat;
	struct mtk_png_plte_param	plte;
	struct mtk_png_trns_param	trns;
};

bool mtk_png_parse(struct mtk_png_dec_param *param, u8 *src_addr_va,
		    u32 src_size);

#endif
