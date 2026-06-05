/*
 * Copyright (C) 2020 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */


#include<linux/fs.h>
#include<linux/uaccess.h>
#include<linux/types.h>
#include<linux/dma-mapping.h>
#include<linux/of_address.h>


#include "mtk_mdp_base.h"
#include "mtk_mdp_path.h"

int log_level;
int dump_data_enable;
u32 mdp_test = 0;

static const char STR_HELP[] = "USAGE:\n"
"       echo [ACTION]>/d/mdp_debug\n"
"ACTION:\n";

struct device *mdp_m2m_dev;

struct mtk_mdp_test_file {
	char *src_path;
	char *dst_path;
};

struct mtk_mdp_test_file mdp_test_file[] = {
	{
		.src_path = "/data/256x256_Y8.raw",
		.dst_path = "/data/256x256_Y8_dst.raw",
	},
	{
		.src_path = "/data/256x256_Y8.raw",
		.dst_path = "/data/256x256_Y4_M0_dst.raw",
	}
};

static unsigned short einkfb_8bpp_gray[256] =
{

    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, // 0x00 Black
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, //
    0x0000,                                                         // 0x10

    0x1111, 0x1111, 0x1111, 0x1111, 0x1111, 0x1111, 0x1111, 0x1111, // 0x11
    0x1111, 0x1111, 0x1111, 0x1111, 0x1111, 0x1111, 0x1111, 0x1111, //
    0x1111,                                                         // 0x21

    0x2222, 0x2222, 0x2222, 0x2222, 0x2222, 0x2222, 0x2222, 0x2222, // 0x22
    0x2222, 0x2222, 0x2222, 0x2222, 0x2222, 0x2222, 0x2222, 0x2222, //
    0x2222,                                                         // 0x32

    0x3333, 0x3333, 0x3333, 0x3333, 0x3333, 0x3333, 0x3333, 0x3333, // 0x33
    0x3333, 0x3333, 0x3333, 0x3333, 0x3333, 0x3333, 0x3333, 0x3333, //
    0x3333,                                                         // 0x43

    0x4444, 0x4444, 0x4444, 0x4444, 0x4444, 0x4444, 0x4444, 0x4444, // 0x44
    0x4444, 0x4444, 0x4444, 0x4444, 0x4444, 0x4444, 0x4444, 0x4444, //
    0x4444,                                                         // 0x54

    0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555, // 0x55
    0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555, //
    0x5555,                                                         // 0x65

    0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, // 0x66
    0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, //
    0x6666,                                                         // 0x76

    0x7777, 0x7777, 0x7777, 0x7777, 0x7777, 0x7777, 0x7777, 0x7777, // 0x77
    0x7777, 0x7777, 0x7777, 0x7777, 0x7777, 0x7777, 0x7777, 0x7777, //
    0x7777,                                                         // 0x87

    0x8888, 0x8888, 0x8888, 0x8888, 0x8888, 0x8888, 0x8888, 0x8888, // 0x88
    0x8888, 0x8888, 0x8888, 0x8888, 0x8888, 0x8888, 0x8888, 0x8888, //
    0x8888,                                                         // 0x98

    0x9999, 0x9999, 0x9999, 0x9999, 0x9999, 0x9999, 0x9999, 0x9999, // 0x99
    0x9999, 0x9999, 0x9999, 0x9999, 0x9999, 0x9999, 0x9999, 0x9999, //
    0x9999,                                                         // 0xA9

    0xAAAA, 0xAAAA, 0xAAAA, 0xAAAA, 0xAAAA, 0xAAAA, 0xAAAA, 0xAAAA, // 0xAA
    0xAAAA, 0xAAAA, 0xAAAA, 0xAAAA, 0xAAAA, 0xAAAA, 0xAAAA, 0xAAAA, //
    0xAAAA,                                                         // 0xBA

    0xBBBB, 0xBBBB, 0xBBBB, 0xBBBB, 0xBBBB, 0xBBBB, 0xBBBB, 0xBBBB, // 0xBB
    0xBBBB, 0xBBBB, 0xBBBB, 0xBBBB, 0xBBBB, 0xBBBB, 0xBBBB, 0xBBBB, //
    0xBBBB,                                                         // 0xCB

    0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC, // 0xCC
    0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC, //
    0xCCCC,                                                         // 0xDC

    0xDDDD, 0xDDDD, 0xDDDD, 0xDDDD, 0xDDDD, 0xDDDD, 0xDDDD, 0xDDDD, // 0xDD
    0xDDDD, 0xDDDD, 0xDDDD, 0xDDDD, 0xDDDD, 0xDDDD, 0xDDDD, 0xDDDD, //
    0xDDDD,                                                         // 0xED

    0xEEEE, 0xEEEE, 0xEEEE, 0xEEEE, 0xEEEE, 0xEEEE, 0xEEEE, 0xEEEE, // 0xEE
    0xEEEE, 0xEEEE, 0xEEEE, 0xEEEE, 0xEEEE, 0xEEEE, 0xEEEE, 0xEEEE, //
    0xEEEE,                                                         // 0xFE

    0xFFFF                                                          // 0xFF White
};

struct mtk_mdp_frame_config  mdp_test_case[] = {
	{
		.src_width = 256,
		.src_height = 256,
		.src_pitch = 256,
		.src_fmt = V4L2_PIX_FMT_Y8,
		.src_addr = 0,
		.dst_width = 256,
		.dst_height = 256,
		.dst_pitch = 256,
		.dst_fmt = V4L2_PIX_FMT_Y8,
		.dst_addr = 0,
		.crop_x = 0,
		.crop_y = 0,
		.crop_w = 256,
		.crop_h = 256,
		.rotate = 0,
		.invert = 0,
		.dth_en = 0,
		.dth_algo = 0,
	},
	{
		.src_width = 256,
		.src_height = 256,
		.src_pitch = 256,
		.src_fmt = V4L2_PIX_FMT_Y8,
		.src_addr = 0,
		.dst_width = 256,
		.dst_height = 256,
		.dst_pitch = 256,
		.dst_fmt = V4L2_PIX_FMT_Y4_M0,
		.crop_x = 0,
		.crop_y = 0,
		.crop_w = 256,
		.crop_h = 256,
		.dst_addr = 0,
		.rotate = 0,
		.invert = 0,
		.dth_en = 0,
		.dth_algo = 0x102,//MDP_DITHER_ALGO_Y8_Y4_S
	},
};

void mdp_ut_test(u32 testcase_id)
{
	u32 max_id = ARRAY_SIZE(mdp_test_case);
	struct mtk_mdp_frame_config *config;
	struct mtk_mdp_test_file *file;

	void *src_addr_va;
	dma_addr_t src_addr_pa;

	void *dst_addr_va;
	dma_addr_t dst_addr_pa;

	u32 src_size;

	unsigned long dma_attrs = 0;

	char *src_path;
	char *dst_path;

	if (testcase_id >= max_id) {
		DDPMSG("invalid id %d %d\n", testcase_id, max_id);
		return;
	}

	config = &mdp_test_case[testcase_id];
	file = &mdp_test_file[testcase_id];

	if (testcase_id < max_id) {
		src_path = file->src_path;
		dst_path = file->dst_path;
		DDPMSG("%s -> %s\n", src_path, dst_path);

		src_size = config->src_height * config->src_pitch;
	}

	dma_attrs |= DMA_ATTR_WRITE_COMBINE;

	/* alloc input/outpout buff */
	src_addr_va =
		dma_alloc_attrs(mdp_m2m_dev, src_size, &src_addr_pa,
				   GFP_KERNEL, dma_attrs);
	dst_addr_va =
		dma_alloc_attrs(mdp_m2m_dev, src_size, &dst_addr_pa,
				   GFP_KERNEL, dma_attrs);

	DDPMSG("dev %s addr 0x%p(0x%X) 0x%p(0x%X) size %u test %u max_id %u\n",
	       dev_name(mdp_m2m_dev),
	       src_addr_va, src_addr_pa,
	       dst_addr_va, dst_addr_pa,
	       src_size, testcase_id, max_id);

	if (!src_addr_va || !dst_addr_va) {
		DDPMSG("memory alloc fail!\n");
		return;
	}

	config->src_addr = src_addr_pa;
	config->dst_addr = dst_addr_pa;

	/* read input buff */
	read_file(src_path, src_addr_va, src_size);

	DDPMSG("src buffer %08X %08X %08X %08X\n",
		*((u32 *)src_addr_va),
		*((u32 *)src_addr_va + 1),
		*((u32 *)src_addr_va + 2),
		*((u32 *)src_addr_va + 3));

	DDPMSG("dst buffer %08X %08X %08X %08X\n",
		*((u32 *)dst_addr_va),
		*((u32 *)dst_addr_va + 1),
		*((u32 *)dst_addr_va + 2),
		*((u32 *)dst_addr_va + 3));

	config_path(config);

	DDPMSG("dst buffer %08X %08X %08X %08X\n",
		*((u32 *)dst_addr_va),
		*((u32 *)dst_addr_va + 1),
		*((u32 *)dst_addr_va + 2),
		*((u32 *)dst_addr_va + 3));

	/* dump output buff*/
	write_file(dst_path, dst_addr_va, src_size);

	DDPMSG("dst buffer %08X %08X %08X %08X\n",
		*((u32 *)dst_addr_va),
		*((u32 *)dst_addr_va + 1),
		*((u32 *)dst_addr_va + 2),
		*((u32 *)dst_addr_va + 3));

	/* release buf */
	dma_free_coherent(mdp_m2m_dev, src_size, src_addr_va, src_addr_pa);
	dma_free_coherent(mdp_m2m_dev, src_size, dst_addr_va, dst_addr_pa);
}

static void mdp_draw_gray_scale(char *buf, u32 w, u32 h, u32 gray_w, u32 pitch)
{
#if 1
	u32 i;
	u32 j;

	for (i = 0; i < w; i++)
	{
		for (j = 0; j < h; j++)
		{
			buf[pitch * j + i] = ((i / 16) % 16) * 0xF;
		}
	}
#else
	u32 i;
	u32 j;
	u32 step = w / gray_w;
	u32 gray_step;
	u32 val = 255;

	if (gray_w == 0)
		gray_w = 1;

	step = w / gray_w;
	gray_step = (val + 1)/step;

	DDPMSG("%dx%d gray_w %d step %d\n", w, h, gray_w, step);

	for (i = 0; i < step; i++) {
		if (val >= gray_step)
			val -= gray_step;
		for (j = 0; j < gray_w; j++) {
			buf[i*gray_w + j] = val;
		};
	}

	DDPMSG("src buf %08X %08X %08X %08X\n",
		*((u32 *)buf),
		*((u32 *)buf + 1),
		*((u32 *)buf + 2),
		*((u32 *)buf + 3));

	for (i = 1; i < h; i++)
		memcpy(buf + i * w, buf, w);

	/* border line */
	memset(buf, 0x80, w);
	memset(buf + (h - 1) * w, 0x80, w);
	for (i = 1; i < h; i++) {
		buf[i * w] = 0x80;
		buf[i * w + w - 1] = 0x80;
	}
#endif
}

void mdp_draw_gray_scale_crop(char *buf, u32 w, u32 h)
{
	u32 i,j;
	u32 val;
	u32 c_w = 512;
	u32 c_h = 512;

	DDPMSG("%dx%d crop %dx%d\n", w, h, c_w, c_h);

	for (i = 0; i < h; i++) {
		for (j = 0; j < w; j++) {
			if (j < c_w)
				val = (i < c_h) ? 0x00 : 0x40;
			else if (j < (2 * c_w))
				val = (i < c_h) ? 0x40 : 0x80;
			else if (j < (3 * c_w))
				val = (i < c_h) ? 0x80 : 0xC0;
			else if (j < (4 * c_w))
				val = (i < c_h) ? 0xC0 : 0xFF;
			else
				val = (i < c_h) ? 0xff : 0x00;

			buf[i * w + j] = val;
		}
	}

	DDPMSG("src buf %08X %08X %08X %08X\n",
		*((u32 *)buf),
		*((u32 *)buf + 1),
		*((u32 *)buf + 2),
		*((u32 *)buf + 3));
}

extern int easy_mtk_mdp_func(
		u32 src_w, u32 src_h,
		u32 dst_w, u32 dst_h,
		u32 src_fmt, u32 dst_fmt,
		u32 src_pitch, u32 dst_pitch,
		dma_addr_t src_mva, dma_addr_t dst_mva,
		u32 crop_x, u32 crop_y,
		u32 crop_w, u32 crop_h,
		u8 dth_en, u32 dth_algo, u8 invert,
		u32 rotate, u32 gamma_flag);

void mdp_ut(const char *opt)
{
	void *src_addr_va;
	dma_addr_t src_addr_pa;
	void *dst_addr_va;
	dma_addr_t dst_addr_pa;

	u32 src_size;

	unsigned long dma_attrs = 0;

	int ret;

	char src_path[50] = "/data/mdp_src.raw";
	char dst_path[50] = "/data/mdp_dst.raw";
	u32 src_width;
	u32 src_height;
	u32 src_pitch;
	u32 src_fmt = V4L2_PIX_FMT_Y8;
	u32 dst_width;
	u32 dst_height;
	u32 dst_pitch;
	u32 dst_fmt = V4L2_PIX_FMT_Y8;
	u32 rotate;
	u32 invert = 0;
	u32 dth_en = 0;
	u32 dth_algo = 0;
	dma_addr_t src_addr;
	dma_addr_t dst_addr;
	struct mtk_mdp_frame_config config;
	u32 gamma_flag;
	u32 cmap_update = 0;

	u32 crop_x;
	u32 crop_y;
	u32 crop_w;
	u32 crop_h;
	u32 gray_w = 16;

	ret = sscanf(opt, "mdp_ut:%dx%d-%dx%d,%d,%d,%dx%d,%dx%d,%d,%d,%d,%d,%d",
		     &src_width, &src_height, &dst_width, &dst_height,
		     &src_pitch, &dst_pitch,
		     &crop_x, &crop_y, &crop_w, &crop_h, &rotate, &invert,
		     &gray_w, &gamma_flag, &cmap_update);
	if (ret < 11) {
		DDPMSG("error to parse cmd %s sscanf ret %d\n", opt, ret);
		return;
	}
	DDPMSG("%s -> %s\n", src_path, dst_path);
	DDPMSG("%dx%d[%d %d %d %d] rotate %d invert %d gray_w %d "
	       "gamma_flag 0x%X cmap_update %d\n",
	       src_width, src_height,
	       crop_x, crop_y, crop_w, crop_h,
	       rotate, invert, gray_w, gamma_flag, cmap_update);

	src_size = src_height * src_pitch;

	dma_attrs |= DMA_ATTR_WRITE_COMBINE;

	/* alloc input/outpout buff */
	src_addr_va =
		dma_alloc_attrs(mdp_m2m_dev, src_size, &src_addr_pa,
				   GFP_KERNEL, dma_attrs);
	dst_addr_va =
		dma_alloc_attrs(mdp_m2m_dev, src_size, &dst_addr_pa,
				   GFP_KERNEL, dma_attrs);

	DDPMSG("dev %s addr 0x%p(0x%X) 0x%p(0x%X) size %u\n",
	       dev_name(mdp_m2m_dev),
	       src_addr_va, src_addr_pa,
	       dst_addr_va, dst_addr_pa,
	       src_size);

	if (!src_addr_va || !dst_addr_va) {
		DDPMSG("memory alloc fail!\n");
		return;
	}

	src_addr = src_addr_pa;
	dst_addr = dst_addr_pa;

	/* read input buff */
	snprintf(src_path, 50, "/data/mdp_src_%dx%d_y8.raw",
		 src_width, src_height);
	DDPMSG("src_path %s\n", src_path);

	mdp_draw_gray_scale(src_addr_va, src_width, src_height, gray_w, src_pitch);
	write_file(src_path, src_addr_va, src_size);

#if 1 //for special test
	if (src_pitch == 1648 && src_height == 1236)
		read_file("/data/1648x1236.raw", src_addr_va, src_size);
	if (src_pitch == 1248 && src_height == 1648)
		read_file("/data/1248x1648.raw", src_addr_va, src_size);
	if (src_pitch == 1236 && src_height == 1648)
		read_file("/data/1236x1648.raw", src_addr_va, src_size);
#endif

	DDPMSG("src buffer %08X %08X %08X %08X\n",
		*((u32 *)src_addr_va),
		*((u32 *)src_addr_va + 1),
		*((u32 *)src_addr_va + 2),
		*((u32 *)src_addr_va + 3));

	DDPMSG("dst buffer %08X %08X %08X %08X\n",
		*((u32 *)dst_addr_va),
		*((u32 *)dst_addr_va + 1),
		*((u32 *)dst_addr_va + 2),
		*((u32 *)dst_addr_va + 3));

	config.src_width = src_width;
	config.src_height = src_height;
	config.src_pitch = src_pitch;
	config.src_fmt = src_fmt;
	config.src_addr = src_addr_pa;

	config.dst_width = dst_width;
	config.dst_height= dst_height;
	config.dst_pitch = dst_pitch;
	config.dst_fmt = dst_fmt;
	config.dst_addr = dst_addr_pa;

	config.crop_x = crop_x;
	config.crop_y = crop_y;
	config.crop_w = crop_w;
	config.crop_h = crop_h;

	config.rotate = rotate;
	config.invert = invert;
	config.dth_en = dth_en;
	config.dth_algo = dth_algo;
	config.gamma_flag = gamma_flag;

	DDPMSG("dst buffer %08X %08X %08X %08X\n",
		*((u32 *)dst_addr_va),
		*((u32 *)dst_addr_va + 1),
		*((u32 *)dst_addr_va + 2),
		*((u32 *)dst_addr_va + 3));

	if (cmap_update) {
		u16 *gamma_lut = einkfb_8bpp_gray;
		mdp_update_cmap_lut(gamma_lut, 256);
	}

	//mdp_sync_config(&config);
	easy_mtk_mdp_func(
		src_width, src_height,
		dst_width, dst_height,
		src_fmt, dst_fmt,
		src_pitch, dst_pitch,
		src_addr_pa, dst_addr_pa,
		crop_x, crop_y,
		crop_w, crop_h,
		dth_en, dth_algo, invert,
		rotate, gamma_flag);


	/* dump output buff*/
	snprintf(dst_path, 50, "/data/mdp_dst_%dx%d_y8_r%d.raw",
		 src_width, src_height, rotate);
	DDPMSG("dst_path %s\n", dst_path);
	write_file(dst_path, dst_addr_va, src_size);

	DDPMSG("dst buffer %08X %08X %08X %08X\n",
		*((u32 *)dst_addr_va),
		*((u32 *)dst_addr_va + 1),
		*((u32 *)dst_addr_va + 2),
		*((u32 *)dst_addr_va + 3));

	/* release buf */
	dma_free_coherent(mdp_m2m_dev, src_size, src_addr_va, src_addr_pa);
	dma_free_coherent(mdp_m2m_dev, src_size, dst_addr_va, dst_addr_pa);
}

char *fmt_2_string(int fmt)
{
	switch (fmt) {
	case V4L2_PIX_FMT_Y8:
		return "Y8";
	case V4L2_PIX_FMT_Y4_M0:
		return "Y4_M0";
	case V4L2_PIX_FMT_Y2_M0:
		return "Y2_M0";
	case V4L2_PIX_FMT_Y1_M0:
		return "Y1_M0";
	default:
		return "Unknown fmt";
	}
}

void read_file(char *path, char *buffer, u32 length)
{
	mm_segment_t fs;
	struct file *fp = NULL;

	fs = get_fs();
	set_fs(KERNEL_DS);
	fp = filp_open(path, O_RDONLY, 0x0);
	if (IS_ERR(fp)) {
		DDPMSG("open file %s fail\n", path);
		return;
	}

	DDPMSG("read file %s len %d\n", path, length);
	/* read date */
	vfs_read(fp, buffer, length, &fp->f_pos);

	filp_close(fp, NULL);
	set_fs(fs);
}

int write_file(char *path, char *buffer, unsigned int length)
{
	struct file *fp;
	mm_segment_t fs;
	loff_t pos;
	int ret;

	fp = filp_open(path, O_RDWR | O_CREAT, 0644);
	if (IS_ERR(fp)) {
		MDP_LOG_ERR("Open/Create file fail\n");
		return -1;
	}

	fs = get_fs();
	set_fs(KERNEL_DS);

	pos = 0;
	ret = vfs_write(fp, buffer, length, &pos);

	filp_close(fp, NULL);
	set_fs(fs);

	DDPMSG("write file %s len %d\n", path, length);

	return ret;
}


#include <linux/debugfs.h>

static char dbg_buf[2048];

static void process_dbg_opt(const char *opt)
{
	char *buf = dbg_buf + strlen(dbg_buf);
	int ret;
	int buf_size_left = ARRAY_SIZE(dbg_buf) - strlen(dbg_buf) - 10;

	if (strncmp(opt, "get_reg", 7) == 0) {
		unsigned long pa;
		unsigned int *va;

		ret = sscanf(opt, "get_reg:0x%lx\n", &pa);
		if (ret != 1) {
			snprintf(buf, 50, "error to parse cmd %s\n", opt);
			return;
		}
		va = ioremap(pa, 4);
		DDPMSG("get_reg: 0x%lx = 0x%08X\n", pa, DISP_REG_GET(va));
		snprintf(buf, buf_size_left, "get_reg: 0x%lx = 0x%08X\n", pa,
			 DISP_REG_GET(va));
		iounmap(va);
		return;
	} else if (strncmp(opt, "set_reg", 7) == 0) {
		unsigned long pa;
		unsigned int *va, val;

		ret = sscanf(opt, "set_reg:0x%lx,0x%x", &pa, &val);
		if (ret != 2) {
			snprintf(buf, 50, "error to parse cmd %s\n", opt);
			return;
		}
		va = ioremap(pa, 4);
		DISP_CPU_REG_SET(va, val);
		DDPMSG("set_reg: 0x%lx = 0x%08X(0x%x)\n", pa, DISP_REG_GET(va),
		       val);
		snprintf(buf, buf_size_left, "set_reg: 0x%lx = 0x%08X(0x%x)\n",
			 pa, DISP_REG_GET(va), val);
		iounmap(va);
		return;
	} else if (strncmp(opt, "log_level:", 10) == 0) {
		char *p = (char *)opt + 10;

		ret = kstrtouint(p, 0, &log_level);
		if (ret) {
			snprintf(buf, 50, "error to parse cmd %s\n", opt);
			return;
		}

		sprintf(buf, "log_level: %d\n", log_level);
	} else if (strncmp(opt, "dump_data:", 10) == 0) {
		char *p = (char *)opt + 10;

		ret = kstrtouint(p, 0, &dump_data_enable);
		if (ret) {
			snprintf(buf, 50, "error to parse cmd %s\n", opt);
			return;
		}

		sprintf(buf, "dump_data_enable: %d\n", dump_data_enable);
	} else if (strncmp(opt, "test_mdp:", 9) == 0) {
		char *p = (char *)opt + 9;
		int testcase_id = 0;

		ret = kstrtouint(p, 0, &testcase_id);
		if (ret) {
			snprintf(buf, 50, "error to parse cmd %s\n", opt);
			return;
		}

		sprintf(buf, "testcase_id: %d\n", testcase_id);

		mdp_ut_test(testcase_id);
	} else if (strncmp(opt, "mdp_ut:", 7) == 0) {
		mdp_ut(opt);
	}
}

static int debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}


static char cmd_buf[512];

static ssize_t debug_read(struct file *file, char __user *ubuf, size_t count,
			  loff_t *ppos)
{
	if (strlen(dbg_buf))
		return simple_read_from_buffer(ubuf, count, ppos, dbg_buf,
					       strlen(dbg_buf));
	else
		return simple_read_from_buffer(ubuf, count, ppos, STR_HELP,
					       strlen(STR_HELP));

}


static ssize_t debug_write(struct file *file, const char __user *ubuf,
			   size_t count, loff_t *ppos)
{
	const int debug_bufmax = sizeof(cmd_buf) - 1;
	size_t ret;

	ret = count;

	if (count > debug_bufmax)
		count = debug_bufmax;

	if (copy_from_user(&cmd_buf, ubuf, count))
		return -EFAULT;

	cmd_buf[count] = 0;

	process_dbg_opt(cmd_buf);

	return ret;
}


static const struct file_operations debug_fops = {
	.read = debug_read,
	.write = debug_write,
	.open = debug_open,
};

int mdp_debugfs_init(struct device *dev)
{
	static struct dentry *debugfs;

	debugfs = debugfs_create_file("mdp_debug", 0444, NULL, (void *)0,
				      &debug_fops);

	mdp_m2m_dev = dev;
	return 0;
}

void __iomem *mdp_init_comp(const char *compat, u32 *phy_base)
{
	struct device_node *node;
	void __iomem *reg = 0;
	struct resource res;

	node = of_find_compatible_node(NULL, NULL, compat);
	if (node) {
		reg = of_iomap(node, 0);
		of_address_to_resource(node, 0, &res);
		*phy_base = res.start;
	}

	return reg;
}
