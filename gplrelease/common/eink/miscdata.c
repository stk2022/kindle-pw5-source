/*
 * Copyright 2017-2020 Amazon Technologies, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

/* #define DEBUG */
#include <common.h>
#include <errno.h>
#include <linux/sizes.h>
#include <mmc.h>
#include <part.h>
#include <fs.h>
#include <miscdata.h>

extern int part_get_info_by_name_or_alias(struct blk_desc *dev_desc, const char *name, disk_partition_t *info);

/* ======================== */
/* miscdata partition utils */
/* ======================== */
#ifdef CONFIG_MISCDATA
static struct blk_desc *misc_dev_desc = NULL;
static int misc_part_num = -1;
static unsigned long misc_part_start = 0;

static struct blk_desc* miscdata_prepare(void)
{
	struct blk_desc *dev_desc = NULL;
	disk_partition_t info;
	int partnum;

	if (!misc_dev_desc) {
		dev_desc = blk_get_dev("mmc", 0);
		if (!dev_desc) {
			printf("ERROR: invalid mmc device\n");
			return NULL;
		}

		partnum = part_get_info_by_name_or_alias(dev_desc, CONFIG_MISCDATA_PARTITION, &info);
		if (partnum <= 0) {
			printf("ERROR: cannot find '%s' partition\n", CONFIG_MISCDATA_PARTITION);
			return NULL;
		}

		misc_part_num = partnum;
		misc_part_start = info.start;
		misc_dev_desc = dev_desc;
	}
	blk_dselect_hwpart(misc_dev_desc, 0);
	return misc_dev_desc;
}

#ifdef CONFIG_MISCDATA_FS
int misc_file_exist(const char *name) {
	int ret;
	struct blk_desc *dev_desc;

	dev_desc = miscdata_prepare();
	if (!dev_desc)
		return -ENODEV;
	ret = fs_set_blk_dev_with_part(dev_desc, misc_part_num);
	if (ret) {
		printf("ERROR: Cannot find partition\n\n");
		return ret;
	}
	return fs_exists(name);
}

int load_misc_file(const char *name, void *addr, unsigned long *len_read) {
	int ret;
	struct blk_desc *dev_desc;
	loff_t length;

	dev_desc = miscdata_prepare();
	if (!dev_desc)
		return -ENODEV;
	ret = fs_set_blk_dev_with_part(dev_desc, misc_part_num);
	if (ret)
		return ret;

	ret = fs_read(name, addr, 0, 0, &length);
	if (ret < 0) {
		printf("ERROR: Failed to load file\n");
		return ret;
	}

	*len_read = (unsigned long)length;
	debug("load file: %s, length=%lu\n", name, *len_read);
	return 0;
}

#else /* CONFIG_MISCDATA_FS */

int misc_block_read(void *addr, unsigned long offset, unsigned long length) {
	int ret;
	struct blk_desc *dev_desc;

	dev_desc = miscdata_prepare();
	if (!dev_desc)
		return -ENODEV;

	debug("mmc read: addr=0x%p, offset=0x%lx (0x%lx+0x%lx), length=0x%lx\n", addr, misc_part_start+offset, misc_part_start, offset, length);
	ret = blk_dread(dev_desc, misc_part_start+offset, length, addr);
	return ret;
}

int misc_block_write(void *addr, unsigned long offset, unsigned long length) {
	int ret;
	struct blk_desc *dev_desc;

	dev_desc = miscdata_prepare();
	if (!dev_desc)
		return -ENODEV;

	debug("mmc write: addr=0x%p, offset=0x%lx (0x%lx+0x%lx), length=0x%lx\n", addr, misc_part_start+offset, misc_part_start, offset, length);
	ret = blk_dwrite(dev_desc, misc_part_start+offset, length, addr);
	return ret;
}

#endif /* CONFIG_MISCDATA_FS */
#endif
