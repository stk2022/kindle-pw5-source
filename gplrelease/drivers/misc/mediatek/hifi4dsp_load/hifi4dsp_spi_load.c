/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/firmware.h>
#include <linux/freezer.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <mach/mtk_hifi4dsp_api.h>
#include <mt-plat/mtk_chip.h>

/*
 * hifi4dsp registers and bits
 */
#define SCPSYS_REG_BASE            (0x1D00B000)
#define REG_SLEEP_DSP0             (SCPSYS_REG_BASE + 0x004)
#define REG_SLEEP_DSP1             (SCPSYS_REG_BASE + 0x008)
#define REG_SLEEP_PWR_STA          (SCPSYS_REG_BASE + 0x060)
#define REG_SLEEP_PWR_STAS         (SCPSYS_REG_BASE + 0x064)

#define DSP0_REG_BASE              (0x1D062000)
#define DSP1_REG_BASE              (0x1D063000)
    #define OFFSET_DSP0_1          (DSP1_REG_BASE - DSP0_REG_BASE)
#define DSP1_GPR1f                 (DSP1_REG_BASE + 0xAC)
#define REG_ALT_RESET_VEC(n)       (DSP0_REG_BASE + 0x04 + OFFSET_DSP0_1 * n)
#define REG_P_DEBUG_BUS0(n)        (DSP0_REG_BASE + 0x0C + OFFSET_DSP0_1 * n)
	#define PDEBUG_ENABLE        0
#define REG_SEL_RESET_SW(n)        (DSP0_REG_BASE + 0x24 + OFFSET_DSP0_1 * n)
	/*reset sw*/
	#define BRESET_SW            0
	#define DRESET_SW            1
	#define PRESET_SW            2
	#define RUNSTALL             3
	#define STATVECTOR_SEL       4
	#define AUTO_BOOT_SW_RESET_B 5

#define DSP_ROMCODE_BASE           (0x1E030000)
#define DSP_SRAM_BASE              (0x1FC00000)
#define MTK_CHIP_HW_VER_1          (0xCA00)
#define MTK_CHIP_HW_VER_2          (0xCB00)

/*
 * hifi4dsp image package and format.
 *
 * |------------|-----------|----------------|----------------|
 *
 * [romcode header]  [preloader.bin]  [DSP_A.bin]  [DSP_B.bin]
 *
 */

struct hifi4dsp_image_info {
	int dual_core;
	u32 boot_addr;
};

struct hifi4dsp_debug_reg {
	char cmd[10];
	u32 addr;
	u32 data;
};

#define HIFI4DSP_IMAGE_NAME	"hifi4dsp_load.bin"
#define MAX_HIFI4DSP_IMAGE_SIZE  (0x200000)

#define XIP_CODE_48K_SIZE  (48 * 1024)
#define HIFI4DSP_BIN_MAGIC ((u64)(0x4D495053444B544D))


static struct hifi4dsp_image_info image_info;
static int hifi4dsp_bootup_done;
static callback_fn user_callback_fn;
static void *callback_arg;
static struct hifi4dsp_debug_reg hifi4dsp_debug_reg_cmd;

static int hifi4dsp_debug_reg_show(struct seq_file *m, void *data)
{
	struct hifi4dsp_debug_reg reg = hifi4dsp_debug_reg_cmd;
	int i;
	u32 val;
	u32 len = reg.data;

	if (strncmp(reg.cmd, "read", 4) == 0) {
		if (len % 4) {
			seq_puts(m, "the length is not 4bytes aligned\n");
			return 0;
		}
		for (i = 0; i < len; i += 4) {
			spi_read_register(reg.addr + i, &val, SPI_SPEED_HIGH);
			if (i % 16 == 0)
				seq_printf(m, "0x%08x: %08x",
					reg.addr + i, val);
			else if (i % 16 == 12)
				seq_printf(m, " %08x\n", val);
			else
				seq_printf(m, " %08x", val);
		}
		seq_puts(m, "\n");
	} else if (strncmp(reg.cmd, "write", 5) == 0) {
		spi_read_register(reg.addr, &val, SPI_SPEED_HIGH);
		seq_printf(m, "0x%08x: 0x%08x\n", reg.addr, val);
	}

	return 0;
}

static int hifi4dsp_debug_reg_open(struct inode *inode, struct file *file)
{
	return single_open(file, hifi4dsp_debug_reg_show, inode->i_private);
}

static ssize_t hifi4dsp_debug_reg_write(struct file *file,
				const char __user *ubuf,
				size_t len, loff_t *offp)
{
	struct hifi4dsp_debug_reg *reg = &hifi4dsp_debug_reg_cmd;
	char buf[30];

	if (len >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, ubuf, len))
		return -EFAULT;

	buf[len] = '\0';

	if (sscanf(buf, "%s 0x%x 0x%x", reg->cmd, &(reg->addr), &(reg->data))
		!= 3)
		return -EINVAL;

	if (strncmp(reg->cmd, "write", 5) == 0) {
		spi_write_register(reg->addr, reg->data, SPI_SPEED_HIGH);
		return len;
	}

	return len;
}

static const struct file_operations hifi4dsp_debug_reg_fops = {
	.open = hifi4dsp_debug_reg_open,
	.read = seq_read,
	.write = hifi4dsp_debug_reg_write,
};

static int debugfs_init(void)
{
	struct dentry *hifi4dsp_dent;
	struct dentry *reg_rw_dent;

	hifi4dsp_dent = debugfs_create_dir("hifi4dsp", NULL);
	if (!hifi4dsp_dent)
		return -ENOMEM;

	reg_rw_dent = debugfs_create_file("reg", 0644,
			hifi4dsp_dent, NULL, &hifi4dsp_debug_reg_fops);
	if (!reg_rw_dent)
		goto out_err;

	return 0;

out_err:
	debugfs_remove_recursive(hifi4dsp_dent);
	return -ENOMEM;
}

static int check_image_header_info(u8 *data, int size)
{
	int ret = 0;
	u64 magic;

	/* check miminal header size : 0x800 */
	if (size < 0x800) {
		ret = -1;
		return ret;
	}

	magic = *(u64 *)data;
	if (magic != HIFI4DSP_BIN_MAGIC) {
		ret = -2;
		pr_err("HIFI4DSP_BIN_MAGIC error : 0x%llX\n", magic);
	}

	return ret;
}

static
int load_image_hifi4dsp(u32 ldr, void *data, u32 len)
{
	int err = 0;

	err = dsp_spi_write_ex(ldr, data, (int)len, SPI_LOAD_IMAGE_SPEED);
	/*
	 * Notice:
	 * 1. this load-address is setting to reg DSP1_GPR1f
	 *	for DSP1 using only.
	 * 2. we assume the lastest binary send is HIFI4DSP1.bin
	 */
	spi_write_register(DSP1_GPR1f, ldr, SPI_SPEED_HIGH);

	return err;
}

static int parse_and_load_image(const struct firmware *fw)
{
	int err;
	int loop;
	u8 *fw_data;
	u8 *data;
	int total_image_len;
	int signature;
	u64 img_bin_tb_inf;
	int boot_adr_num;
	u32 dsp_boot_adr;
	int img_bin_inf_num;
	u32 section_off;
	u32 section_len;
	u32 section_ldr;
	u32 fix_offset;
	const char fwname[] = HIFI4DSP_IMAGE_NAME;
	struct hifi4dsp_image_info *info = &image_info;

	debugfs_init();

	/*
	 * step1: check image header and magic.
	 */
	if (fw->size > MAX_HIFI4DSP_IMAGE_SIZE) {
		err = -EFBIG;
		pr_err("firmware %s size too large!\n", fwname);
		goto tail;
	}
#if 0
	if (fw->size <= XIP_CODE_48K_SIZE) {
		err = -1;
		pr_err("firmware %s size too small!\n", fwname);
		goto tail;
	}
#endif
	pr_info("firmware %s load success, size = %d\n",
		fwname, (int)fw->size);
#if 0
	fw_data = (u8 *)fw->data + XIP_CODE_48K_SIZE;
	total_image_len = (int)fw->size - XIP_CODE_48K_SIZE;
#else
	fw_data = (u8 *)fw->data;
	total_image_len = (int)fw->size;
#endif

	err = check_image_header_info(fw_data, total_image_len);
	if (err) {
		pr_err("firmware %s may be corrupted!\n", fwname);
		goto tail;
	}

	/*
	 * step2: Parse Image Header
	 * 1), check image signature or not,
	 * 2), bypass encrypted image with spi-write() if signatured;
	 * 3), check single core or dual core if no signature;
	 */
	img_bin_tb_inf = *(u64 *)(fw_data + 16);
	/*
	 * sizeof (TB_INF) = 8bytes.
	 * TB_INF[7]: authentication type for IMG_BIN_INF_TB
	 * TB_INF[6]: authentication type for IMG_BIN
	 * TB_INF[5]: encryption type for IMG_BIN
	 * TB_INF[4:2]: reserved
	 * TB_INF[1]:
	 * bit0: indicate if enabling authentication (1) or not (0)
	 *	for IMG_BIN_INF_TB
	 * TB_INF[0]: version of IMG_BIN_INF_TB
	 */
	signature = (img_bin_tb_inf) ? 1 : 0;
	if (signature || mt_get_chip_hw_ver() == MTK_CHIP_HW_VER_2) {
		/*
		 * Send all fw_data.bin to sram for DSP-romcode parsing
		 * if authentication enabled
		 */
		err = load_image_hifi4dsp(DSP_SRAM_BASE, fw_data,
					total_image_len);
		if (err) {
			pr_notice("%s write all fw_data.bin (%d bytes) fail!\n",
				__func__, total_image_len);
		}
		info->boot_addr = DSP_ROMCODE_BASE;
		goto tail;
	}

	/* 28bytes =
	 * |BIN_MAGIC|BIN_TOTAL_SZ|IMG_BIN_INF_TB_SZ|TB_INF|TB_LD_ADR|
	 */
	/* BOOT_ADR_NO(M) */
	boot_adr_num = *(u32 *)(fw_data + 28);
	/* DSP_1_ADR for DSP0 bootup entry, other *_ADR ignored */
	dsp_boot_adr = *(u32 *)(fw_data + 28 + 4);
	/* IMG_BIN_INF_NO(N) */
	img_bin_inf_num = *(u32 *)(fw_data + 32 + 4 * boot_adr_num);

	info->boot_addr = dsp_boot_adr;
	info->dual_core = (boot_adr_num == 1) ? 0 : 1;

	/*
	 * skip preloader.bin first.
	 * MG_BIN_INF_X (20bytes, loop read info)
	 */
	for (loop = 1; loop < img_bin_inf_num; loop++) {
		fix_offset = 32 + (4 * boot_adr_num) + 8 + (20 * loop);
		/* IMG_BIN_OFST */
		section_off = *(u32 *)(fw_data + fix_offset + 4);
		/* IMG_SZ */
		section_len = *(u32 *)(fw_data + fix_offset + 12);
		/* LD_ADR */
		section_ldr = *(u32 *)(fw_data + fix_offset + 16);

		pr_info(
			"section%d: load_addr = 0x%08X, offset = 0x%08X, len = %u\n",
			loop, section_ldr, section_off, section_len);

		/*
		 * IMG_BIN_OFST: start from beginning of IMG_BINS.
		 * #0x00000814 = Total_HEAD_len
		 *	= 8(BIN_MAGIC) + 4(BIN_TOTAL_SZ) + 4(IMG_BIN_INF_TB_SZ)
		 *	+ 0x800(IMG_BIN_INF_TB) + 4(IMG_BINS_SZ)
		 */
		data = fw_data + 0x00000814 + section_off;
		err = load_image_hifi4dsp(section_ldr, data, section_len);
		if (err) {
			pr_notice("%s write section%d.bin (%d bytes) fail!\n",
				__func__, loop, section_len);
			goto tail;
		}
	}
tail:
	return err;
}

/*
 * 1. Request firmware from fs bin.
 * 2. Check and parse image header info.
 * 3. Write binary to hifi4dsp ITCM/SRAM each image.
 */
static
int load_hifi4dsp_firmware(void *dev, int *size)
{
	int err;
	struct device *device = dev;
	const struct firmware *fw;
	const char fwname[] = HIFI4DSP_IMAGE_NAME;

	/*
	 * request_firmware() with sync wait.
	 */
	err = request_firmware(&fw, fwname, device);
	if (err != 0) {
		pr_err("fw %s not available, err : %d\n", fwname, err);
		return err;
	}

	err = parse_and_load_image(fw);

	release_firmware(fw);

	return err;
}


/*
 * 1. Release DSP0 core.
 * 2. Maybe power-on DSP1 if need (not must).
 */
static void hifi4dsp_poweron(void)
{
	u32 val;
	u32 boot_addr = image_info.boot_addr;
	int dual_core = image_info.dual_core;

	/* bootup from ROMCODE or SRAM base */
	spi_write_register(REG_ALT_RESET_VEC(0), boot_addr, SPI_SPEED_HIGH);
	spi_read_register(REG_ALT_RESET_VEC(0), &val, SPI_SPEED_HIGH);
	pr_info("[HIFI4DSP] DSP0 boot from base addr : 0x%08X\n", val);

	/*
	 * 1.STATVECTOR_SEL pull high to
	 * select external reset vector : altReserVec
	 * 2. RunStall pull high
	 */
	spi_set_register32(REG_SEL_RESET_SW(0),
			(0x1 << STATVECTOR_SEL) | (0x1 << RUNSTALL),
			SPI_SPEED_HIGH);

	/* DReset & BReset pull high */
	spi_set_register32(REG_SEL_RESET_SW(0),
			(0x1 << BRESET_SW) | (0x1 << DRESET_SW),
			SPI_SPEED_HIGH);
	/* DReset & BReset pull low */
	spi_clr_register32(REG_SEL_RESET_SW(0),
			(0x1 << BRESET_SW) | (0x1 << DRESET_SW),
			SPI_SPEED_HIGH);

	/* Enable PDebug */
	spi_set_register32(REG_P_DEBUG_BUS0(0),
			(0x1 << PDEBUG_ENABLE), SPI_SPEED_HIGH);

	if (!dual_core)
		goto tail;

	/* hifi4_dsp1_power_on() by DPS0 */
	pr_notice("[HIFI4DSP] dual-core power-on\n");

tail:
	/*
	 * Hifi4_ReleaseDSP, only for DSP0.
	 */
	/* DSP RESET B as high to release DSP */
	spi_set_register32(REG_SEL_RESET_SW(0),
			(0x1 << AUTO_BOOT_SW_RESET_B), SPI_SPEED_HIGH);

	/* RUN_STALL pull down */
	spi_clr_register32(REG_SEL_RESET_SW(0),
			(0x1 << RUNSTALL), SPI_SPEED_HIGH);
}

static void fixup_hifi4dsp_early_setting(void)
{
	u32 old, new;
	u32 spis_mclk, reg_val;
	u32 spis1_clk;
	/*
	 * Must first spi-write for DSP SPI-SMT setting.
	 * Disable DSP watchdog subsequently.
	 */
	spi_write_register(0x1D00DA04, 0x1800, SPI_SPEED_LOW);
	spi_write_register(0x1D010000, 0x22000200, SPI_SPEED_LOW);

	/* 400M XTAL */
	spi_read_register(0x1d00c000, &reg_val, SPI_SPEED_LOW);
	reg_val |= (1 << 0);
	spi_write_register(0x1d00c000, reg_val, SPI_SPEED_LOW);
	spi_read_register(0x1d00c000, &reg_val, SPI_SPEED_LOW);
	reg_val &= ~(1 << 1);
	spi_write_register(0x1d00c000, reg_val, SPI_SPEED_LOW);

	/* 400M PLL */
	spi_read_register(0x1d00c170, &reg_val, SPI_SPEED_LOW);
	reg_val |= (1 << 0);
	spi_write_register(0x1d00c170, reg_val, SPI_SPEED_LOW);
	spi_read_register(0x1d00c170, &reg_val, SPI_SPEED_LOW);
	reg_val &= 0xfffffffd;
	spi_write_register(0x1d00c170, reg_val, SPI_SPEED_LOW);
	spis1_clk = 0x820f6276;
	spi_write_register(0x1d00c164, spis1_clk, SPI_SPEED_LOW);
	spi_read_register(0x1d00c160, &spis1_clk, SPI_SPEED_LOW);
	spis1_clk |= (1 << 0);
	spi_write_register(0x1d00c160, spis1_clk, SPI_SPEED_LOW);

	/* SPIS1 module clk to 400MHz */
	spi_read_register(0x1D00E0CC, &spis_mclk, SPI_SPEED_LOW);
	reg_val = spis_mclk & (~(0xff << 16));
	reg_val |= (0x2 << 16);
	spi_write_register(0x1D00E0CC, reg_val, SPI_SPEED_LOW);

	/* DSP & Bus module clk to 400MHz */
	spi_read_register(0x1D00E0D4, &reg_val, SPI_SPEED_LOW);
	reg_val &= ~(0xf << 0);
	reg_val |= (0x2 << 0);
	spi_write_register(0x1D00E0D4, reg_val, SPI_SPEED_LOW);

	/*
	 * Work for DSP 32K I/D cache random issue,
	 * Must setting before DSP core running.
	 */
#define FIXED_ADDRESS (u32)(0x1D060024)
	spi_read_register(FIXED_ADDRESS, &old, SPI_SPEED_HIGH);
	spi_write_register(FIXED_ADDRESS, 0xAAAAAAAA, SPI_SPEED_HIGH);
	spi_read_register(FIXED_ADDRESS, &new, SPI_SPEED_HIGH);
	pr_err("Readback, address: 0x%X: 0x%X ===>>> 0x%X\n",
			FIXED_ADDRESS, old, new);
	/*
	 * Poweron DSP_SRAM1/2/3/4/5/6/7,
	 * total 4M SRAM to run dual-core.
	 */
	mtcmos_init();
}

static void set_hifi4dsp_run_status(void)
{
	hifi4dsp_bootup_done = 1;
	pr_notice("[HIFI4DSP] load bin done and start to run now.\n");
}

/*
 * HIFI4DSP has boot done or not.
 */
inline int hifi4dsp_run_status(void)
{
	return hifi4dsp_bootup_done;
}

/*
 * Assume called by audio system only.
 */
int load_hifi4dsp_bin_and_run(void)
{
	int ret = 0;
	int try = 0;
	int try_cnt = 0;
	void *data;
	struct spi_device *spi;
	struct mtk_hifi4dsp_private_data *pri_data;

	if (hifi4dsp_run_status()) {
		pr_err("[HIFI4DSP] error: bootup two times!\n");
		return ret;
	}

	/* default use SPI bus0 device */
	pri_data = get_hifi4dsp_private_data();
	spi = (struct spi_device *)(pri_data->spi_bus_data[0]);
	data = (void *)&spi->dev;

	do {
		try = 0;
		try_cnt++;

		fixup_hifi4dsp_early_setting();

		/*
		 * request_firmware to sram for hifi4dsp_load.bin,
		 * most try 5 times for loading binary.
		 */
		ret = load_hifi4dsp_firmware(data, NULL);
		if (ret && try_cnt < 5)
			try = 1;

		/*
		 * Power-on HIFI4DSP and wait bootup ok.
		 */
		if (!try) {
			msleep(100);
			hifi4dsp_poweron();
			msleep(100);
		}
	} while (try);

	if (!ret)
		set_hifi4dsp_run_status();

	return ret;
}

static
void async_load_hifi4dsp_and_run(
			const struct firmware *fw,
			void *context)
{
	int ret;

	if (!fw) {
		pr_notice("[HIFI4DSP] error: fw == NULL!\n");
		pr_notice("request_firmware_nowait (%s) not available.\n",
				HIFI4DSP_IMAGE_NAME);
		return;
	}
	/*
	 * Step1:
	 * Fixup setting before load-bin
	 */
	fixup_hifi4dsp_early_setting();

	/*
	 * Step2:
	 * Load binary to DSP SRAM bank0/1
	 */
	ret = parse_and_load_image(fw);
	release_firmware(fw);
	if (ret) {
		pr_notice("[HIFI4DSP] firmware_async_load Error!\n");
		return;
	}

	/*
	 * Step3:
	 * Power-on DSP boot sequence
	 */
	msleep(50);
	hifi4dsp_poweron();

	set_hifi4dsp_run_status();

	/* callback function for user */
	if (user_callback_fn)
		user_callback_fn(callback_arg);
}

/*
 * Assume called by audio system only.
 */
int async_load_hifi4dsp_bin_and_run(
			callback_fn callback, void *param)
{
	int ret = 0;
	struct spi_device *spi;
	struct mtk_hifi4dsp_private_data *pri_data;

	if (hifi4dsp_run_status()) {
		pr_err("[HIFI4DSP] error: bootup two times!\n");
		return ret;
	}

	user_callback_fn = callback;
	callback_arg = param;

	/* default use SPI bus0 device */
	pri_data = get_hifi4dsp_private_data();
	spi = (struct spi_device *)(pri_data->spi_bus_data[0]);

	/* Async load firmware and run hifi4dsp */
	ret = request_firmware_nowait(THIS_MODULE,
			true, HIFI4DSP_IMAGE_NAME, &spi->dev,
			GFP_KERNEL, NULL, async_load_hifi4dsp_and_run);

	return ret;
}

/*
 * This kthread is only to breed HIFI4DSP for test.
 */
int breed_hifi4dsp(void *data)
{
	int ret = 0;

	set_freezable();

	/* sleep() or wait_mutex() */
	msleep(500);
	ret = load_hifi4dsp_bin_and_run();
	msleep(100);

	/*
	 * bootup mission is now accomplished,
	 * the kthead will end self-life.
	 */
	return ret;
}

