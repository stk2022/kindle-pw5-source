/*
 * Copyright 2020 Amazon Technologies, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

/* #define DEBUG */
#include <common.h>
#include <memalign.h>
#include <bmp_layout.h>
#include <asm/unaligned.h>
#include <watchdog.h>
#include <fs.h>

#include "display.h"
#include "miscdata.h"
#include "hwtcon.h"
#include "spm_mtcmos.h"
#include "common/panel_setting.h"

#include "v1/wf_lut_config.h"
#include "v1/pipeline_config.h"
#include "v2/wf_lut_config.h"
#include "v2/pipeline_config.h"

#ifdef CONFIG_HIBERNATION
#include "hibernation.h"
#endif

#ifdef CONFIG_MISCDATA_FS
static const char* splash_image  = "hibernate.bmp.gz";
static const char* waveform_name = "current.wrf.gz";
#endif
#define WAVEFORM_BUF_SIZE 0x300000lu

struct cmdqRecStruct;
extern const struct platform_info_struct *platform;
extern void pmic_control_init(bool);

static void (*hwtcon_screen_update)(int waveform_mode, int update_mode, struct pp_rect *region, bool wait);

void hwtcon_screen_update_v1(int waveform_mode, int update_mode, struct pp_rect *region, bool wait)
{
	hwtcon_update_region_v1(waveform_mode, update_mode, region);
	wf_lut_wait_for_framedone_v1();
}

void hwtcon_screen_update_v2(int waveform_mode, int update_mode, struct pp_rect *region, bool wait)
{
	hwtcon_update_region_v2(waveform_mode, update_mode, region);
	if (wait)
		wf_lut_wait_end_all_irq();
}

__weak const struct platform_info_struct* board_eink_display_info(void)
{
	return NULL;
}

static int eink_setup_working_buffer(void)
{
	int ret = 0;
	unsigned long load_size;

#ifdef CONFIG_HIBERNATION
	if (get_hibernation_info()->working_buffer_addr) {
		g_buffer_info.wb_buffer_1 = get_hibernation_info()->working_buffer_addr;
	}
#ifndef CONFIG_MISCDATA_FS
	load_size = get_hibernation_info()->working_buffer_size;
	if (load_size <= 0) {
		printf("no working buffer saved.\n");
		goto out;
	}

	if (misc_block_read((void*)g_buffer_info.wb_buffer_1, get_hibernation_info()->working_buffer_offset, load_size/512+1) <= 0) {
		printf("cannot working buffer file\n");
		goto out;
	}
	flush_cache((unsigned long)g_buffer_info.wf_file_buffer, ALIGN(load_size, ARCH_DMA_MINALIGN));
	debug("loaded working buffer to 0x%08x, size=%lu\n", g_buffer_info.wb_buffer_1, load_size);
#endif
#endif

out:
	return ret;
}

static int draw_raw_image(void *src, unsigned long size, int *x, int *y, int *w, int *h)
{
	int width = ALIGN(platform->PANEL_HEIGHT, 16);
	int height = ALIGN(platform->PANEL_WIDTH, 16);
	int padded_src = width - platform->PANEL_HEIGHT;
	uchar *raw = src, *dest;
	int i, j;

	if (size != (width * height)) {
		printf("ERROR: Only support raw data of full screen, image size=%lu\n", size);
		return -EINVAL;
	}

	debug("image info: w=%d h=%d pad=%d\n", width, height, padded_src);
	for (i = 0; i < platform->PANEL_WIDTH; i++) {
		WATCHDOG_RESET();
		for (j = 0; j < platform->PANEL_HEIGHT; j++) {
			dest = (void*)g_buffer_info.image_buffer + platform->PANEL_WIDTH * (platform->PANEL_HEIGHT - j - 1) + i;
			*dest = *raw++ & 0xF0;
		}
		raw += padded_src;
	}

	*x = *y = 0;
	*w = platform->PANEL_WIDTH;
	*h = platform->PANEL_HEIGHT;

	flush_cache(g_buffer_info.image_buffer, platform->PANEL_HEIGHT*platform->PANEL_WIDTH);
	debug("raw image loaded to: x=%d, y=%d, w=%d, h=%d\n", *x, *y, *w, *h);
	return 0;
}

#define WFM_PARTITION "wfm"
#define PATH_MAX 512
static int eink_load_file_from_waveform_partition(const char *path, const char *name_pattern, void *buffer_addr, unsigned long *buffer_size)
{
	static struct blk_desc *dev_desc;
	static int partnum = 0;

	int ret;
	disk_partition_t info;
	void *file_buf = NULL;
	loff_t file_length = 0;
	unsigned long file_size;
	char name[PATH_MAX];

	struct fs_dir_stream *dirs;
	struct fs_dirent *dent;

	if (partnum <= 0) {
		dev_desc = blk_get_dev("mmc", 0);
		if (!dev_desc) {
			printf("ERROR: invalid mmc device\n");
			return -ENODEV;
		}

		partnum = part_get_info_by_name(dev_desc, WFM_PARTITION, &info);
		if (partnum <= 0) {
			printf("ERROR: cannot find '%s' partition\n", WFM_PARTITION);
			return -ENODEV;
		}
	}

	blk_dselect_hwpart(dev_desc, 0);

	ret = fs_set_blk_dev_with_part(dev_desc, partnum);
	if (ret)
		return ret;

	/* find the file */
	dirs = fs_opendir(path);
	if (!dirs)
		return -errno;

	while ((dent = fs_readdir(dirs))) {
		if ((dent->type != FS_DT_DIR) && (strstr(dent->name, name_pattern))) {
			snprintf(name, PATH_MAX, "%s/%s", path, dent->name);
			file_length = dent->size;
			break;
		}
	}
	fs_closedir(dirs);

	if (!file_length) {
		printf("ERROR: Cannot find file %s in %s\n", name_pattern, path);
		return -ENOENT;
	}

	file_buf = malloc_cache_aligned(file_length);
	if (!file_buf)
		return -ENOMEM;

	ret = fs_set_blk_dev_with_part(dev_desc, partnum);
	if (ret)
		return ret;

	ret = fs_read(name, (unsigned long)file_buf, 0, 0, &file_length);
	if (ret < 0) {
		printf("ERROR: Failed to load file %s, error: %d\n", name, ret);
		goto error;
	}
	file_size = (unsigned long)file_length;
	debug("load file: %s, length=%lu\n", name, file_size);

	if (strstr(name, ".gz")) {
		debug("unzip file %s from 0x%p to 0x%p, buffer_size=%lu\n", name, file_buf, buffer_addr, *buffer_size);
		ret = gunzip(buffer_addr, *buffer_size, file_buf, &file_size);
		if (ret) {
			printf("failed to unzip file %s, error: %d", name, ret);
			goto error;
		}
		debug("unzip file %s done, size: %ld/%ld\n", name, file_size, *buffer_size);
		*buffer_size = file_size;
	} else if (*buffer_size >= file_size) {
		memcpy(buffer_addr, file_buf, file_size);
		*buffer_size = file_size;
		ret = 0;
	} else {
		debug("buffer size is not enough\n");
		ret = -ENOMEM;
	}

error:
	if (file_buf)
		free(file_buf);
	flush_cache((unsigned long)buffer_addr, *buffer_size);
	return ret;
}

#define RAW_FILE_PATH "/images"
#define WAVEFORM_FILE_PATH "/waveform_to_use"

#define CRITICAL_FILE "critbatt.raw.gz"
#define WAVEFORM_FILE ".gz"

int eink_load_raw_image(const char* name, int *x, int *y, int *width, int *height)
{
	int ret;
	void *buffer = NULL;
	unsigned long buffer_size;

	platform = board_eink_display_info();
	if (!platform)
		return -ENODEV;

	buffer_size = ALIGN(platform->PANEL_HEIGHT, 16) * ALIGN(platform->PANEL_WIDTH, 16);
	buffer = malloc_cache_aligned(buffer_size);

	if (!buffer)
		return -ENOMEM;

	ret = eink_load_file_from_waveform_partition(RAW_FILE_PATH, name, buffer, &buffer_size);

	if (!ret) {
		ret = draw_raw_image(buffer, buffer_size, x, y, width, height);
	}

	free(buffer);
	return ret;
}

static int eink_setup_waveform_from_partition(void)
{
	unsigned long buffer_size;

	platform = board_eink_display_info();
	if (!platform)
		return -ENODEV;

	buffer_size = WAVEFORM_BUF_SIZE;
	return eink_load_file_from_waveform_partition(WAVEFORM_FILE_PATH, WAVEFORM_FILE, (void*)g_buffer_info.wf_file_buffer, &buffer_size);
}

static int eink_setup_waveform(void)
{
	int ret;
	void *waveform_load_buf;
	unsigned long load_size;

	ret = 1;
#ifdef CONFIG_HIBERNATION
	if (get_hibernation_info()->waveform_addr) {
		g_buffer_info.wf_file_buffer = get_hibernation_info()->waveform_addr;
	}
#ifdef CONFIG_MISCDATA_FS
	/* load zipped waveform to another address*/
	if (strstr(waveform_name, ".wrf.gz")) {
		waveform_load_buf = malloc_cache_aligned(WAVEFORM_BUF_SIZE);
	} else {
		waveform_load_buf = (void*)g_buffer_info.wf_file_buffer;
	}

	/* load waveform */
	if (load_misc_file(waveform_name, waveform_load_buf, &load_size)) {
		printf("cannot load waveform file\n");
		goto cleanup;
	}

	/* unzip waveform */
	if (waveform_load_buf != (void*)g_buffer_info.wf_file_buffer) {
		debug("unzip waveform from 0x%p to 0x%08x, size=%lu\n", waveform_load_buf, g_buffer_info.wf_file_buffer, load_size);
		if (gunzip((void*)g_buffer_info.wf_file_buffer, WAVEFORM_BUF_SIZE, waveform_load_buf, &load_size)) {
			free(waveform_load_buf);
			printf("failed to unzip waveform\n");
			goto cleanup;
		}
		debug("unzip waveform done, size=%ld/%ld\n", load_size, WAVEFORM_BUF_SIZE);
		free(waveform_load_buf);
	}
#else
	load_size = get_hibernation_info()->waveform_size;
	if (load_size <= 0) {
		printf("no waveform file saved in misc partition, try waveform partition\n");
		ret = eink_setup_waveform_from_partition();
		if (ret) {
			printf("cannot load from waveform partition.\n");
		}
		return ret;
	}

	waveform_load_buf = (void*)g_buffer_info.wf_file_buffer - 0x1000000;

	if (misc_block_read(waveform_load_buf, get_hibernation_info()->waveform_offset, load_size/512+1) <= 0) {
		printf("cannot load waveform file\n");
		goto cleanup;
	}

	debug("unzip waveform from 0x%p to 0x%08x, size=%lu\n", waveform_load_buf, g_buffer_info.wf_file_buffer, load_size);
	if (gunzip((void*)g_buffer_info.wf_file_buffer, WAVEFORM_BUF_SIZE, waveform_load_buf, &load_size)) {
		printf("failed to unzip waveform\n");
		goto cleanup;
	}
	debug("unzip waveform done, size=%ld/%ld\n", load_size, WAVEFORM_BUF_SIZE);
#endif
	flush_cache(g_buffer_info.wf_file_buffer, ALIGN(load_size, ARCH_DMA_MINALIGN));

	ret = 0;
#endif /* CONFIG_HIBERNATION */
cleanup:
	return ret;
}

int eink_setup_buffer(void)
{
	static bool eink_setup_memory = 0;

	if (!eink_setup_memory) {
		/* default buffer addresses */
		g_buffer_info.wf_file_buffer = 0x5E900000;
		g_buffer_info.image_buffer   = 0x5E000000;
		g_buffer_info.wb_buffer_0    = 0x5E500000;
		g_buffer_info.wb_buffer_1    = 0x5EC00000;

		if (eink_setup_working_buffer()) {
			printf("failed to setup working buffer, continue.\n");
			/* not critical */
		}

		if (eink_setup_waveform()) {
			printf("failed to setup waveform buffer\n");
			return -ENODEV;
		}
		eink_setup_memory = true;
	}

	debug("image_buffer: 0x%08x, waveform_buffer: 0x%08x, working_buffer: 0x%08x\n",
			g_buffer_info.image_buffer, g_buffer_info.wf_file_buffer, g_buffer_info.wb_buffer_1);
	return 0;
}

int eink_setup_hw(void)
{
	if (eink_setup_buffer()) {
		return -ENODEV;
	}

	/* get platform_info */
	platform = board_eink_display_info();
	if (!platform) {
		printf("unknown display\n");
		return -ENODEV;
	}

	spm_mtcmos_ctrl_mm(STA_POWER_ON);
	spm_mtcmos_ctrl_img(STA_POWER_ON);
	pp_write(NULL,MMSYS_CG_CON0, 0);
	pp_write(NULL, IMGSYS_CG_CON0, 0);

	pmic_control_init(true);

	/* config timing */
	rdma_config_smi_setting(NULL);
	if (hwtcon_get_hw_ver() != HW_VERSION_MT8113) {
		hwtcon_screen_update = hwtcon_screen_update_v1;

		/* config timing */
		pp_func_init_pipeline_and_dpi_setting(g_buffer_info.image_buffer,
				g_buffer_info.wb_buffer_0,
				g_buffer_info.wb_buffer_1,
				g_buffer_info.wf_file_buffer,
				platform->PANEL_WIDTH, platform->PANEL_HEIGHT,
				MAIN_SOF_MODE_IMG_LAST_UPDATE_AND_DPI_VSYNC, 1);
		/* init slot */
		wf_lut_waveform_day_mode_slot_v1(NULL);
	} else {
		hwtcon_screen_update = hwtcon_screen_update_v2;

		wf_lut_config_context_init_for_pipeline();
		wf_lut_waveform_day_mode_slot_v2(NULL);
	}

	return 0;
}

static int draw_splash_image(void *src, int middle, int *x, int *y, int *w, int *h)
{
	uchar *bmap, *dest;
	struct bmp_image *bmp = (struct bmp_image *)src;
	unsigned long width, height;
	unsigned bmp_bpix;
	unsigned padded_src;
	int location;
	int i, j;

	if ((bmp->header.signature[0]!='B') || (bmp->header.signature[1]!='M')) {
		printf("Error: not a valid bmp file\n");
		return -EINVAL;
	}

	width = get_unaligned_le32(&bmp->header.width);
	height = get_unaligned_le32(&bmp->header.height);
	bmp_bpix = get_unaligned_le16(&bmp->header.bit_count);

	bmap = (uchar *)src + get_unaligned_le32(&bmp->header.data_offset);
	padded_src = (width & 0x3 ? (width & ~0x3) + 4 : width) - width;

	debug("bitmap size: w=%ld, h=%ld, bmp_bpix=%d\n", width, height, bmp_bpix);

	if (width > platform->PANEL_HEIGHT || height > platform->PANEL_WIDTH) {
		printf("Error: bmp file larger than screen size\n");
		return -EINVAL;
	}

	if (bmp_bpix != 8) {
		debug("bmp_bpix %d is not supported\n", bmp_bpix);
		return -EINVAL;
	}

	if (middle)
		location = (platform->PANEL_WIDTH + height) / 2;
	else
		location = platform->PANEL_WIDTH;

	for (i = 0; i < height; i++) {
		WATCHDOG_RESET();
		for (j = 0; j < width; j++) {
			dest = (void*)g_buffer_info.image_buffer + platform->PANEL_WIDTH * (platform->PANEL_HEIGHT - j - 1) + (location - i - 1);
			*dest = *bmap++ & 0xF0;
		}
		bmap += padded_src;
	}

	*x = location - height;
	*y = platform->PANEL_HEIGHT - width;
	*w = height;
	*h = width;

	flush_cache(g_buffer_info.image_buffer, ALIGN(platform->PANEL_HEIGHT*platform->PANEL_WIDTH, ARCH_DMA_MINALIGN));
	debug("bitmap loaded to: x=%d, y=%d, w=%d, h=%d\n", *x, *y, *w, *h);
	return 0;
}

int eink_load_splash(int middle, int *x, int *y, int *width, int *height)
{
	void *file_buf, *unzip_file_buf;
	unsigned long load_size;
	unsigned long buffer_size;
	int ret = -ENOENT;

	platform = board_eink_display_info();
	if (!platform)
		return -ENODEV;

#ifdef CONFIG_HIBERNATION
	buffer_size = platform->PANEL_WIDTH * platform->PANEL_HEIGHT * 2;
	file_buf = malloc_cache_aligned(buffer_size);

#ifdef CONFIG_MISCDATA_FS
	if (load_misc_file(splash_image, file_buf, &load_size)) {
		printf("cannot find splash image");
		goto cleanup;
	}

	if (strstr(splash_image, ".gz")) {
		/* unzip splash */
		unzip_file_buf = malloc_cache_aligned(buffer_size);
		debug("unzip splash from 0x%p to 0x%p, size=%lu\n", file_buf, unzip_file_buf, load_size);
		if (gunzip(unzip_file_buf, buffer_size, file_buf, &load_size)) {
			printf("failed to unzip splash image\n");
		} else {
			ret = draw_splash_image(unzip_file_buf, middle, x, y, width, height);
		}
		free(unzip_file_buf);
	} else {
		ret = draw_splash_image(file_buf, middle, x, y, width, height);
	}
#else
	load_size = get_hibernation_info()->splash_size;
	if (load_size <= 0) {
		printf("no splash file saved.\n");
		goto cleanup;
	}
	if (misc_block_read(file_buf, get_hibernation_info()->splash_offset, load_size/512+1) <= 0) {
		printf("Failed to load splash image\n");
		goto cleanup;
	}

	/* unzip splash */
	unzip_file_buf = malloc_cache_aligned(buffer_size);
	debug("unzip splash from 0x%p to 0x%p, size=%lu\n", file_buf, unzip_file_buf, load_size);
	if (gunzip(unzip_file_buf, buffer_size, file_buf, &load_size)) {
		printf("failed to unzip splash image\n");
	} else {
		ret = draw_splash_image(unzip_file_buf, middle, x, y, width, height);
	}
	free(unzip_file_buf);
#endif
cleanup:
	free(file_buf);
#endif /* CONFIG_HIBERNATION */
	return ret;
}

int eink_update_screen(int x, int y, int width, int height, bool wait)
{
	struct pp_rect region = {0};

	if (!platform)
		return -ENODEV;

	region.rect_x = x;
	region.rect_y = y;
	region.rect_width = width;
	region.rect_height = height;

	debug("screen update: x=%d, y=%d, w=%d, h=%d, wait=%d\n", x, y, width, height, wait);
	hwtcon_screen_update(WAVEFORM_MODE_GC16, UPDATE_MODE_FULL, &region, wait);
	debug("screen update finish\n");
	return 0;
}

int eink_update_screen_mode0(void)
{
	struct pp_rect region = {0};

	if (!platform)
		return -ENODEV;

	region.rect_x = 0;
	region.rect_y = 0;
	region.rect_width = platform->PANEL_WIDTH;
	region.rect_height = platform->PANEL_HEIGHT;

	hwtcon_screen_update(WAVEFORM_MODE_INIT, UPDATE_MODE_FULL, &region, true);
	return 0;
}

int eink_update_screen_raw(const char *name)
{
	int ret;
	int img_x, img_y, img_width, img_height;

	ret = eink_load_raw_image(name, &img_x, &img_y, &img_width, &img_height);
	if (ret) {
		printf("failed to load raw image: %s\n", name);
		return ret;
	}

	ret = eink_update_screen_mode0();
	if (ret) {
		printf("failed to clear screen\n");
		return ret;
	}

	ret = eink_update_screen(img_x, img_y, img_width, img_height, true);
	if (ret) {
		printf("failed to display raw image: %s\n", name);
		return ret;
	}
	return ret;
}

static int do_eink_display_init(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	if (eink_setup_hw()) {
		return CMD_RET_FAILURE;
	}
	return CMD_RET_SUCCESS;
}

static int do_eink_display_mode0(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	if (eink_setup_hw()) {
		return CMD_RET_FAILURE;
	}
	if (eink_update_screen_mode0()) {
		printf("failed to mode0\n");
		return CMD_RET_FAILURE;
	}
	pmic_control_init(false);
	return CMD_RET_SUCCESS;
}

static int do_eink_display_load_splash(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int splash_x, splash_y, splash_width, splash_height;

	if (eink_load_splash(argc >= 2, &splash_x, &splash_y, &splash_width, &splash_height)) {
		printf("failed to load splash image\n");
		return CMD_RET_FAILURE;
	}
	return CMD_RET_SUCCESS;
}

static int do_eink_display_splash(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int splash_x, splash_y, splash_width, splash_height;

	if (eink_setup_hw()) {
		return CMD_RET_FAILURE;
	}

	if (eink_load_splash(argc >= 2, &splash_x, &splash_y, &splash_width, &splash_height)) {
		printf("failed to load splash image\n");
		return CMD_RET_FAILURE;
	}

	if (eink_update_screen(splash_x, splash_y, splash_width, splash_height, true)) {
		printf("failed to show splash image\n");
		return CMD_RET_FAILURE;
	}
	pmic_control_init(false);
	return CMD_RET_SUCCESS;
}

static int do_eink_display_raw(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	if (argc < 2)
		return CMD_RET_USAGE;

	if (eink_setup_hw()) {
		return CMD_RET_FAILURE;
	}
	if (eink_update_screen_raw(argv[1])) {
		return CMD_RET_FAILURE;
	}
	pmic_control_init(false);
	return CMD_RET_SUCCESS;
}

static int do_eink_display_critical_battery(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	if (eink_setup_hw()) {
		return CMD_RET_FAILURE;
	}
	if (eink_update_screen_raw(CRITICAL_FILE)) {
		return CMD_RET_FAILURE;
	}
	pmic_control_init(false);
	return CMD_RET_SUCCESS;
}

static cmd_tbl_t cmd_eink_display[] = {
	U_BOOT_CMD_MKENT(init, 1, 0, do_eink_display_init, "", ""),
	U_BOOT_CMD_MKENT(mode0, 1, 0, do_eink_display_mode0, "", ""),
	U_BOOT_CMD_MKENT(load_splash, 2, 0, do_eink_display_load_splash, "", ""),
	U_BOOT_CMD_MKENT(raw, 2, 0, do_eink_display_raw, "", ""),
	U_BOOT_CMD_MKENT(splash, 2, 0, do_eink_display_splash, "", ""),
	U_BOOT_CMD_MKENT(critical_battery, 1, 0, do_eink_display_critical_battery, "", ""),
};

static int do_eink_display_ops(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	cmd_tbl_t *cp;

	cp = find_cmd_tbl(argv[1], cmd_eink_display, ARRAY_SIZE(cmd_eink_display));

	/* Drop the eink_display command */
	argc--;
	argv++;

	if (cp == NULL || argc > cp->maxargs)
		return CMD_RET_USAGE;
	if (flag == CMD_FLAG_REPEAT && !cmd_is_repeatable(cp))
		return CMD_RET_SUCCESS;
	return cp->cmd(cmdtp, flag, argc, argv);
}

U_BOOT_CMD(eink, 4, 0, do_eink_display_ops,
		"eink display system",
		"init\n"
		"    - init eink display\n"
		"eink load_splash\n"
		"    - load splash image into image buffer\n"
		"eink mode0\n"
		"    - mode0\n"
		"eink raw FILENAME\n"
		"    - display raw image in wfm partition\n"
		"eink splash\n"
		"    - display wakeup splash\n"
		"eink critical_battery\n"
		"    - display critical_battery screen\n"
		);

