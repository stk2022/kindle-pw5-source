/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#ifndef __PARTITION_H__
#define __PARTITION_H__

#include <platform/mmc_core.h>

#define BIMG_HEADER_SZ              (0x800)
#define MKIMG_HEADER_SZ             (0x200)

#define PART_HDR_DATA_SIZE  512

#define PART_KERNEL     "KERNEL"
#define PART_ROOTFS     "ROOTFS"

#define PART_MAGIC          0x58881688
#define PART_EXT_MAGIC      0x58891689
#define PART_MAX_COUNT      128
#define PART_PRELOADER_SIZE (0x400)

#define FRP_NAME        "frp"

union part_hdr_t {
	struct {
		unsigned int magic;        /* partition magic */
		unsigned int dsize;        /* partition data size */
		char         name[32];     /* partition name */
		unsigned int maddr;        /* partition memory address */
		unsigned int mode;      /* maddr is counted from
					 * the beginning or end of RAM
					 */
		/* extension */
		unsigned int ext_magic;    /* always EXT_MAGIC */
		unsigned int hdr_size;     /* header size is 512 bytes
					    * currently, but may extend in the
					    * future
					    */
		unsigned int hdr_version;  /* see HDR_VERSION */
		unsigned int img_type;     /* please refer to #define
					    * beginning with IMG_TYPE_
					    */
		unsigned int img_list_end; /* end of image list? 0: this image
					    * is followed by another image 1:
					    * end
					    */
		unsigned int align_size;   /* image size alignment setting in
					    * bytes, 16 by default for
					    * AES encryption
					    */
		unsigned int dsize_extend; /* high word of image size for
					    * 64 bit address support
					    */
		unsigned int maddr_extend; /* high word of image load address
					    * in RAM for 64 bit address support
					    */
	} info;
	unsigned char data[PART_HDR_DATA_SIZE];
};
#endif /* __PARTITION_H__ */

