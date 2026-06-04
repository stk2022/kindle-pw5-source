#ifndef __HWTCON_DEF_H__
#define __HWTCON_DEF_H__

#include <common.h>
#include <dm.h>
#include <asm/io.h>
#include <stdarg.h>
#include <linux/kernel.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

#define TCON_ERR(string, args...) \
	printf("[HWTCON ERR]"string" @%s,%u\n", ##args, __func__, __LINE__)

#ifdef HWTCON_LOG
#define TCON_LOG(string, args...) \
	printf("[HWTCON LOG]"string" @%s,%u\n", ##args, __func__, __LINE__)
#else
#define TCON_LOG(string, args...)
#endif


/* NOTE: n > m */
#define BIT_MASKS(n, m) (~(BIT_MASK(m) - 1) & ((BIT_MASK(n) - 1) | BIT_MASK(n)))

struct pp_rect {
	u32 rect_x;
	u32 rect_y;
	u32 rect_width;
	u32 rect_height;
};

enum GRAY_MODE_ENUM {
	GRAY_MODE_2_GRAY_LEVEL = 0,
	GRAY_MODE_4_GRAY_LEVEL = 1,
	GRAY_MODE_8_GRAY_LEVEL = 2,
	GRAY_MODE_16_GRAY_LEVEL = 3,
	GRAY_MODE_32_GRAY_LEVEL = 4,
};

struct wf_lut_waveform {
	unsigned int start_addr;
	unsigned char *start_addr_va;
	unsigned int len;
	unsigned int waveform_mode;
	unsigned int temperature_zone;
};

struct wf_lut_wb_rdma {
	unsigned int start_addr;
	unsigned int x;
	unsigned int y;
	unsigned int width;
	unsigned int height;
};


#endif /* endof __HWTCON_DEF_H__ */
