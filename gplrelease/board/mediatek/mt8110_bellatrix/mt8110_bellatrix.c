// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Amazon.com Inc.
 */

#include <config.h>
#include <common.h>
#include <command.h>
#include <part.h>
#include <wdt.h>
#include <asm/gpio.h>
#include <asm/arch/boot_args.h>
#ifdef CONFIG_FRONTLIGHT
#include <frontlight.h>
#endif
#ifdef CONFIG_UFBL
#include <ufbl.h>
#endif
#ifdef UFBL_FEATURE_IDME
#include <idme.h>
#define DSN_LEN 16
#endif
#ifdef CONFIG_PMIC_BD71828
#include <power/bd71828.h>
#endif
#if defined(CONFIG_CHARGER_DET_MAX20342)
#include <power/max20342.h>
#endif


enum board_type {
	BOARD_TYPE_BELLATRIX,
	BOARD_TYPE_BELLATRIX_6,
	BOARD_TYPE_BELLATRIX_7,
	BOARD_TYPE_MALBEC_PROTO,
	BOARD_TYPE_MALBEC_HVT,
	BOARD_TYPE_MALBEC,
	BOARD_TYPE_COUNT,
};

/* board type */
static enum board_type board_type_value = 0;
static const char* board_config_mapping[BOARD_TYPE_COUNT] = {
	[BOARD_TYPE_BELLATRIX]    = "mt8110-bellatrix",
	[BOARD_TYPE_BELLATRIX_6]  = "mt8110-bellatrix-6",
	[BOARD_TYPE_BELLATRIX_7]  = "mt8110-bellatrix-7",
	[BOARD_TYPE_MALBEC_PROTO] = "mt8110-malbec-proto",
	[BOARD_TYPE_MALBEC_HVT]   = "mt8110-malbec",
	[BOARD_TYPE_MALBEC]       = "mt8110-malbec",
};
#ifdef CONFIG_FRONTLIGHT
static const char* board_frontlight_mapping[BOARD_TYPE_COUNT] = {
	[BOARD_TYPE_BELLATRIX]    = "frontlight-bellatrix-white",
	[BOARD_TYPE_MALBEC_PROTO] = "frontlight-malbec-proto-white",
	[BOARD_TYPE_MALBEC_HVT]   = "frontlight-lm3692x-white",
	[BOARD_TYPE_MALBEC]       = "frontlight-fp9966-white",
};
static int board_frontlight_brightness_mapping[BOARD_TYPE_COUNT] = {
	[BOARD_TYPE_BELLATRIX]    = 650,
	[BOARD_TYPE_MALBEC_PROTO] = 650,
	[BOARD_TYPE_MALBEC_HVT]   = 650,
};
#endif

/* Bellatrix 7" */
static char* tattoo_code_bellatrix_7[] = {"BL7", NULL};
/* Bellatrix 6" */
static char* tattoo_code_bellatrix_6[] = {"BL6", NULL};
/* Malbec Proto */
static char* tattoo_code_malbec_proto[] = {"23X", "248", "249", NULL};
/* Malbec HVT */
static char* tattoo_code_malbec_hvt[] = {"26J", "26K", "26L", NULL};
/* Malbec */
static char* tattoo_code_malbec[] = {"2R9", "2NM", "2NN", "2EX", "2EP", "2EQ", "2EE", "2EF", "2DU", "2DV", "2DH", "2DG", "2DF",
	"2C8", "2C9", "2CB", "2B1", "2B2", "2B3", "2B4", "29J", "29K", "29L", "27A", "27B", NULL};

static int check_tattoo_code(const char *tattoo, char **code)
{
	while (*code) {
		if (!strncmp(tattoo, *code, 3)) {
			return 1;
		}
		code++;
	}
	return 0;
}

static char* hwid_string[] = {
	"BELLATIX V1", "BELLATRIX V2", "Proto", "HVT",      /*  0  1  2  3 */
	"HVT(DOE)", "HVT1.1", "EVT", "EVT(DOE)",            /*  4  5  6  7 */
	"EVT1.1", "DVT", "DVT(DOE1)", "DVT-ALT",            /*  8  9 10 11 */
	"DVT(DOE3)", "DVT(DOE2)", "DVT1.1", NULL,           /* 12 13 14 15 */
	NULL
};

int board_get_hwid(char* string)
{
	int gpios[] = {14,15,16,17,-1};
	int value;

	gpio_claim_vector(gpios, "hwid%d");
	value = gpio_get_values_as_int(gpios);

	if (string) {
		if (value < ARRAY_SIZE(hwid_string)) {
			sprintf(string, hwid_string[value]);
		} else {
			sprintf(string, "Unknown(%d)", value);
		}
	}
	return value;
}

#ifdef CONFIG_UFBL
static bool is_locked = false;
#endif
static void board_check_type(void)
{
	static char const *board_id = NULL;
	char const *tattoo_code = NULL;

#ifdef UFBL_FEATURE_IDME
	if (!board_id) {
		idme_initialize();
		board_id = idme_board_id();
	}
#endif

	board_type_value = BOARD_TYPE_BELLATRIX;
	if (board_id == NULL) {
		printf("No board_id\n");
	} else {
		tattoo_code = board_id + 3;
		if (check_tattoo_code(tattoo_code, tattoo_code_malbec)) {
			board_type_value = BOARD_TYPE_MALBEC;
        } else if (check_tattoo_code(tattoo_code, tattoo_code_malbec_hvt)) {
			board_type_value = BOARD_TYPE_MALBEC_HVT;
        } else if (check_tattoo_code(tattoo_code, tattoo_code_malbec_proto)) {
			board_type_value = BOARD_TYPE_MALBEC_PROTO;
		} else if (check_tattoo_code(tattoo_code, tattoo_code_bellatrix_7)) {
			board_type_value = BOARD_TYPE_BELLATRIX_7;
		} else if (check_tattoo_code(tattoo_code, tattoo_code_bellatrix_6)) {
			board_type_value = BOARD_TYPE_BELLATRIX_6;
		}
	}
}

#ifdef CONFIG_FRONTLIGHT
const char* board_frontlight_device(void)
{
	return board_frontlight_mapping[board_type_value];
}

int board_frontlight_brightness(void)
{
	return board_frontlight_brightness_mapping[board_type_value];
}
#endif

#ifdef CONFIG_EINK_DISPLAY
#include "panel_setting.h"
static const struct platform_info_struct *board_display_mapping[BOARD_TYPE_COUNT] = {
	[BOARD_TYPE_BELLATRIX]    = &panel_1648_1236_info,
	[BOARD_TYPE_MALBEC_PROTO] = &panel_1648_1236_info,
	[BOARD_TYPE_MALBEC_HVT]   = &panel_1648_1236_info,
	[BOARD_TYPE_MALBEC]       = &panel_1648_1236_info,
};

const struct platform_info_struct* board_eink_display_info(void)
{
	return board_display_mapping[board_type_value];
}
#endif

extern int spm_mtcmos_ctrl_audio(int state);
#define STA_POWER_DOWN  0

int board_init(void)
{
#if defined(CONFIG_CHARGER_DET_MAX20342)
        // 1. Set USB/UART switch back to UART
        // 2. Enable max20342 low power mode
        max20342_initialize();
#endif

	/* power off AUDAFE for power saving after hiberantion */
	spm_mtcmos_ctrl_audio(STA_POWER_DOWN);

	/* address of boot parameters */
	gd->bd->bi_boot_params = CONFIG_SYS_SDRAM_BASE + 0x100;

	debug("gd->fdt_blob is %p\n", gd->fdt_blob);
	return 0;
}

void board_mmc_post_init(void)
{
	board_check_type();
#ifdef CONFIG_UFBL
	is_locked = ufbl_is_locked_production_device();
#endif

	/* Disable console */
	if (is_locked)
		gd->flags |= GD_FLG_DISABLE_CONSOLE;

#ifdef CONFIG_HIBERNATION
	extern int hibernation_resume(void);
	hibernation_resume();
#endif
#ifdef CONFIG_FRONTLIGHT
	frontlight_enable(true, true);
#endif
}

#define EMMC_BLOCK_SIZE_IN_BYTE		512

#define PSTORE_LOG_HEAD_MAGIC_NUMBER		0x1cd0927
#define PSTORE_LOG_HEAD_ADDR_IN_BYTE		SZ_8M

struct pstore_log_head
{
	ulong	head_magic;
	ulong	log_addr;	// offset in byte (DRAM)
	ulong	log_size;	// size in byte (DRAM)
};

int board_is_abnormal_reset(void);	// add here to avoid compile warning
static void restore_pstore_log(void)
{
	void* buf = NULL;
	ulong log_addr, log_size;
	struct pstore_log_head *head = NULL;

	if(board_is_abnormal_reset()) {
		/* 1. get and check log head */
		buf = (void*)malloc(SZ_1M);
		if(buf == NULL) return;

		misc_block_read(buf, PSTORE_LOG_HEAD_ADDR_IN_BYTE/EMMC_BLOCK_SIZE_IN_BYTE, SZ_1M/EMMC_BLOCK_SIZE_IN_BYTE);

		head = (struct pstore_log_head*)buf;
		if(head->head_magic != PSTORE_LOG_HEAD_MAGIC_NUMBER) { free(buf); return; }	// check head magic number
		log_addr = head->log_addr;
		log_size = head->log_size + sizeof(struct pstore_log_head);	// buf is about to be freed

		if(log_size > SZ_1M) {
			void* tmp = (void*)realloc(buf, log_size);
			if(tmp == NULL) { free(buf); return; }
			buf = tmp;
			misc_block_read(buf + SZ_1M, (PSTORE_LOG_HEAD_ADDR_IN_BYTE/EMMC_BLOCK_SIZE_IN_BYTE) + (SZ_1M/EMMC_BLOCK_SIZE_IN_BYTE), ((log_size/EMMC_BLOCK_SIZE_IN_BYTE) + 1 - (SZ_1M/EMMC_BLOCK_SIZE_IN_BYTE)));	// copy the rest
		}

		/* 2. copy log body */
		memcpy((void*)log_addr, buf + sizeof(struct pstore_log_head), log_size - sizeof(struct pstore_log_head));

		free(buf);
	}
}

int board_late_init(void)
{
	int ret;
	char hwid_name[16], idme_hwid[16];
	int hwid;

	/* claim HW_ID gpios */
	hwid = board_get_hwid(hwid_name);
	printf("Hardware Board: %s\n", hwid_name);

#ifdef UFBL_FEATURE_IDME
	char dsn[DSN_LEN + 1];
	memset(dsn, 0, sizeof(dsn));
	/* load serial number */
	ret = idme_get_var_external("serial", dsn, DSN_LEN);
	if (ret) {
		pr_err("Error reading DSN from IDME\n");
	} else {
		env_set("serial#", dsn);
	}

	if (!idme_get_var_external("hwid", idme_hwid, sizeof(idme_hwid))) {
		if (hwid != simple_strtoul(idme_hwid, NULL, 10)) {
			sprintf(idme_hwid, "%u", hwid);
			if (!idme_update_var_ex("hwid", (const char *)idme_hwid, 4)) {
				pr_info("Set hwid to IDME\n");
			} else {
				pr_err("Failed to set hwid to IDME\n");
			}
		}
	}

#endif

#if (CONFIG_USB_FUNCTION_FASTBOOT & CONFIG_WDT_MTK)
	/* check if we need to enter fastboot */
	pr_info("Check Fastboot...\n");
	if (check_fastboot_mode()) {
		pr_info("Clear Fastboot flag...\n");
		set_clr_fastboot_mode(0);
#ifdef CONFIG_FRONTLIGHT
		frontlight_enable(false, false);
#endif
		if (run_command("fastboot usb 0", 0))
			pr_err("Failed to execute the fastboot command\n");
	}
#endif

#ifdef CONFIG_UFBL
	printf("Secure Info: secure_cpu: %d, production: %d, unlocked: %d\n",
			ufbl_is_secure_cpu(), ufbl_is_production_device(), !is_locked);
	if (is_locked) {
		env_set("bootdelay", "0");
	}
	/* Always enable console */
	gd->flags &= ~GD_FLG_DISABLE_CONSOLE;
#endif

	restore_pstore_log();
	return 0;
}

#if (CONFIG_USB_FUNCTION_FASTBOOT & CONFIG_WDT_MTK)
int fastboot_set_reboot_flag(void)
{
	pr_info("Set Fastboot flag...\n");
	set_clr_fastboot_mode(1);
	return 0;
}
#endif

/* return 1 to use default in boot.img */
int board_get_config(char *config) {
	strcpy(config, board_config_mapping[board_type_value]);
	return 0;
}

#ifdef CONFIG_PMIC_BD71828
#ifndef SEJ_BASE
#define SEJ_BASE (0x1000A000)
#endif

#define PMIC_LOW_VBAT_TIMEOUT   3
#define PMIC_LOW_VBAT_THRESHOLD 3500 /* 3.5V */
int power_init_board(void)
{
	struct udevice *wdt;

	int ret;
	uint16_t voltage;
	bool dcin;
	bool critical_shown = false;

	/* Skip battery check if booting from download mode */
	if ((readl(SEJ_BASE) & 0xF) == 0x3) {
		debug("Skip battery check if booting from download mode.\n");
		return 0;
	}

	ret = bd71828_check_vbat(&voltage, &dcin, &critical_shown);
	if (ret) {
		/* no battery or failed to get voltage, skip checking */
		return 0;
	}

	if (voltage < PMIC_LOW_VBAT_THRESHOLD) {
		uclass_first_device(UCLASS_WDT, &wdt);
		if (wdt)
			wdt_stop(wdt);
		printf("voltage=%d please charge the device, or device will shutdown in %d seconds\n", voltage, PMIC_LOW_VBAT_TIMEOUT);
#ifdef CONFIG_FRONTLIGHT
		board_check_type();
		frontlight_enable(false, false);
#endif
		if (!critical_shown) {
			run_command("eink critical_battery", 0);
			critical_shown = true;
		}
	}

	while (voltage < PMIC_LOW_VBAT_THRESHOLD) {
		mdelay(PMIC_LOW_VBAT_TIMEOUT * 1000);

		ret = bd71828_check_vbat(&voltage, &dcin, &critical_shown);
		if (ret) return 0;

		printf("voltage=%d below threshold, device ", voltage);
		if (!dcin) {
			printf("not charging, shutdown the device.\n");
			bd71828_power_off();
			while(1);
		} else {
			printf("charging, retry in %d seconds.\n", PMIC_LOW_VBAT_TIMEOUT);
		}
	}
	return 0;
}

#define BD71828_BOOT_REASON_HIBERNATION 2
#define BD71828_BOOT_REASON_NO_LIGHT    7
#define BD71828_BOOT_REASON_BOOT_PWRON  8
#define BD71828_BOOT_REASON_BOOT_DCIN   9
#define BD71828_BOOT_REASON_BOOT_ALM    10
#define BD71828_BOOT_REASON_BOOT_HALL   11
#define BD71828_BOOT_REASON_LONG_RST    16
#define BD71828_BOOT_REASON_WDOGB_RST   17
#define BD71828_BOOT_REASON_DCIN_STAT   24
#define BD71828_BOOT_REASON_HALL_STAT   25

int board_is_abnormal_reset(void)
{
	return get_boot_reason_from_boot_args() & BIT(BD71828_BOOT_REASON_LONG_RST); /* PWRON_LONG_RST of RESETSRC */
}

int board_is_rtc_wakeup(void)
{
	return get_boot_reason_from_boot_args() & BIT(BD71828_BOOT_REASON_BOOT_ALM); /* RTC_ALM0_BT of BOOTSRC */
}

int board_is_usb_wakeup(void)
{
	return get_boot_reason_from_boot_args() & BIT(BD71828_BOOT_REASON_BOOT_DCIN); /* DCINOK_BT of BOOTSRC */
}

int board_is_hall_wakeup(void)
{
	return get_boot_reason_from_boot_args() & BIT(BD71828_BOOT_REASON_BOOT_HALL); /* HALL_DET_BT of BOOTSRC */
}

int board_is_usb_connected(void)
{
	return get_boot_reason_from_boot_args() & BIT(BD71828_BOOT_REASON_DCIN_STAT); /* DCIN_DET of DCIN_STAT */
}

int board_is_hall_closed(void)
{
	return !(get_boot_reason_from_boot_args() & BIT(BD71828_BOOT_REASON_HALL_STAT)); /* HALL_STAT of IO_STAT, active low */
}
#endif

#ifdef CONFIG_UFBL

#define FUSE_INFO(name, index, len) \
	FUSE_ ## name ## _INDEX = index, \
	FUSE_ ## name ## _LEN = len

enum {
	FUSE_INFO(SBC_EN, 45, 1),
	FUSE_INFO(AR_EN,  16, 1),
	FUSE_INFO(HUID,   12, 4),
};
#define FUSE_HUID_COUNT 4

bool board_is_secure_cpu(void)
{
	unsigned char data[FUSE_SBC_EN_LEN] = {0};
	bool ret = true;

	if (efuse_read_index(FUSE_SBC_EN_INDEX, data) == 0) {
		ret = !!data[0];
	} else {
		pr_err("%s:error in reading SBC_EN status, assuming a secure device \n", __func__);
	}

	return ret;
}

bool board_is_production_device(void)
{
	unsigned char data[FUSE_AR_EN_LEN] = {0};
	bool ret = true;

	if (efuse_read_index(FUSE_AR_EN_INDEX, data) == 0) {
		ret = !!data[0];
	} else {
		pr_err("%s:error in reading AR_EN status, assuming a secure device \n", __func__);
	}

	return ret;
}

int board_get_hw_serial(uint32_t *serial, int count)
{
	int i;
	int ret = 0;
	unsigned char data[FUSE_HUID_LEN] = {0};

	if (!serial || count > FUSE_HUID_COUNT)
		return -EINVAL;

	for (i = 0; i < count; i++) {
		ret = efuse_read_index(FUSE_HUID_INDEX + i, data);
		if (ret) {
			pr_err("%s:error in reading HUID %d, assuming a secure device \n", __func__, i);
			break;
		}
		serial[count-i-1] = data[3] << 24 | data[2] << 16 | data[1] << 8 | data[0];
	}

	return ret;
}

static const unsigned char unlock_key_malbec[] = {
	0x30, 0x82, 0x01, 0x22, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86,
	0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0f, 0x00,
	0x30, 0x82, 0x01, 0x0a, 0x02, 0x82, 0x01, 0x01, 0x00, 0xc9, 0xca, 0x57,
	0x02, 0x6a, 0x84, 0x78, 0x62, 0xeb, 0xa1, 0x11, 0x44, 0xb7, 0xa6, 0xd6,
	0xe5, 0x24, 0xa4, 0xa6, 0x8f, 0xe9, 0xb2, 0x4d, 0x17, 0x29, 0xfb, 0xe5,
	0xe8, 0xd2, 0xe0, 0x44, 0x23, 0xa3, 0x7d, 0x4a, 0x7d, 0x78, 0x7c, 0xd8,
	0xee, 0x5b, 0x4a, 0x1a, 0x13, 0x72, 0x9f, 0x87, 0x5a, 0x9d, 0x14, 0x10,
	0xa5, 0x11, 0x95, 0x8d, 0x6c, 0xb0, 0x20, 0x04, 0x8f, 0xfb, 0x10, 0x46,
	0xb6, 0xc9, 0x7f, 0xc3, 0xc8, 0x63, 0x3c, 0x9b, 0x7e, 0x8b, 0x53, 0x07,
	0xbb, 0xec, 0x97, 0x83, 0x82, 0x19, 0x51, 0xf5, 0x21, 0x9c, 0x55, 0x67,
	0x08, 0x9a, 0xd0, 0xf0, 0xc1, 0x44, 0x3a, 0xa9, 0x3c, 0x38, 0x7f, 0xb2,
	0xf7, 0xb9, 0x75, 0x7e, 0x6c, 0x1e, 0xaf, 0x70, 0x58, 0xb6, 0xb6, 0x21,
	0x41, 0x20, 0x67, 0x38, 0x51, 0x54, 0xec, 0x3c, 0x9b, 0x04, 0xff, 0x7e,
	0x99, 0x70, 0xf6, 0x99, 0x3f, 0x5c, 0xe6, 0x43, 0xcd, 0x3c, 0x95, 0x55,
	0xc6, 0x00, 0x2d, 0xe3, 0x7f, 0xb8, 0x30, 0x8e, 0xa2, 0xef, 0x74, 0x8d,
	0x67, 0xe6, 0x86, 0xf6, 0x03, 0x01, 0x8b, 0xc5, 0xd9, 0x04, 0x15, 0x94,
	0x83, 0xe3, 0x60, 0x18, 0xef, 0x77, 0x03, 0xf9, 0x09, 0x71, 0x23, 0x4e,
	0x6b, 0x39, 0x5b, 0x9e, 0x93, 0x02, 0x22, 0x1c, 0xe4, 0xf1, 0xb2, 0x1e,
	0x03, 0xe7, 0x7d, 0x85, 0x40, 0x83, 0x00, 0xdb, 0xfd, 0x3c, 0xff, 0xa8,
	0x35, 0x84, 0xb9, 0x93, 0x13, 0xc0, 0x76, 0x24, 0x14, 0xd7, 0x94, 0xf4,
	0x1d, 0xac, 0x25, 0xf3, 0x71, 0x84, 0x90, 0x41, 0xda, 0xba, 0xb8, 0x67,
	0x09, 0x86, 0x94, 0x7b, 0x2f, 0xf7, 0x88, 0x49, 0x62, 0xf3, 0x1e, 0x4a,
	0x0c, 0xbb, 0xb7, 0x59, 0xe9, 0xac, 0xb8, 0x94, 0x47, 0xae, 0x0c, 0x6b,
	0x90, 0x1a, 0xb3, 0xad, 0x65, 0xfb, 0x64, 0xc6, 0x8d, 0xca, 0xd3, 0x6a,
	0xf1, 0x02, 0x03, 0x01, 0x00, 0x01
};

const unsigned char *board_get_unlock_key(unsigned int *key_len)
{
	if (!key_len)
		return NULL;

	switch (board_type_value) {
		case BOARD_TYPE_MALBEC_PROTO:
		case BOARD_TYPE_MALBEC_HVT:
		case BOARD_TYPE_MALBEC:
		default:
			*key_len = sizeof(unlock_key_malbec);
			return unlock_key_malbec;
			break;
	}
	return NULL;
}

static const unsigned char onetime_unlock_root_pubkey_malbec[] = {
	0x30, 0x82, 0x01, 0x22, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86,
	0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0f, 0x00,
	0x30, 0x82, 0x01, 0x0a, 0x02, 0x82, 0x01, 0x01, 0x00, 0xef, 0xc0, 0xa9,
	0xe1, 0x0f, 0x5e, 0xbe, 0x98, 0x0e, 0x97, 0xda, 0x06, 0xc0, 0xd8, 0xdc,
	0x39, 0x73, 0x60, 0x8b, 0xc4, 0x5a, 0x75, 0x11, 0x89, 0x01, 0xbc, 0xdc,
	0x4a, 0x96, 0x82, 0x7e, 0x5a, 0xd3, 0xdd, 0x8e, 0x9f, 0x80, 0x74, 0x5c,
	0x72, 0x1f, 0x40, 0xd2, 0x75, 0x67, 0x49, 0xec, 0x65, 0xdb, 0x52, 0x43,
	0xdb, 0x49, 0x8d, 0x34, 0xe4, 0x07, 0x49, 0xc7, 0x37, 0x92, 0xfd, 0xff,
	0x10, 0xe5, 0x76, 0x05, 0x1a, 0x4f, 0x2d, 0x21, 0x09, 0x41, 0x93, 0xc4,
	0xf9, 0x59, 0x46, 0x21, 0xd8, 0x7f, 0x05, 0xf8, 0xa7, 0xae, 0x19, 0x14,
	0x54, 0x74, 0x89, 0xba, 0x7b, 0xbb, 0x56, 0x9b, 0x75, 0xd8, 0x8f, 0xe2,
	0x0e, 0x60, 0x8f, 0xa1, 0x43, 0x2b, 0x3d, 0xb6, 0x31, 0x02, 0x03, 0x92,
	0xb4, 0x5d, 0xb7, 0xef, 0xcb, 0x5b, 0x6b, 0x50, 0x20, 0xa8, 0x75, 0x14,
	0xe7, 0xbc, 0x04, 0x37, 0x11, 0x4f, 0xd2, 0x25, 0xd2, 0xf9, 0x9e, 0x4c,
	0x48, 0xc4, 0xd2, 0x56, 0x09, 0xd2, 0xaa, 0xc2, 0x75, 0xfd, 0xfb, 0x90,
	0x46, 0xe5, 0xf6, 0x0f, 0x16, 0x95, 0x16, 0x88, 0x08, 0xd2, 0x03, 0xee,
	0x68, 0x3c, 0x70, 0x10, 0xf3, 0x0b, 0x83, 0x37, 0xba, 0xfe, 0xdd, 0x6f,
	0x6a, 0xea, 0x1e, 0xe0, 0xe8, 0x33, 0x54, 0xc6, 0x71, 0x64, 0x42, 0x4e,
	0xc8, 0x5a, 0x36, 0xca, 0x55, 0xd0, 0x95, 0x28, 0x76, 0x7b, 0x28, 0x4d,
	0x9d, 0x17, 0xe2, 0x2b, 0xd9, 0xdb, 0xfc, 0xe7, 0xcd, 0xa2, 0x4d, 0xd8,
	0xe1, 0x8f, 0xaf, 0x84, 0xee, 0x39, 0x5f, 0x59, 0x75, 0x25, 0x8b, 0x82,
	0x0d, 0x7a, 0x18, 0x91, 0x3a, 0x92, 0x58, 0x3a, 0x37, 0x85, 0xfd, 0x47,
	0xf4, 0xb1, 0x38, 0x95, 0xed, 0x7e, 0xf4, 0xd3, 0x9e, 0xf7, 0xf5, 0xbd,
	0xc2, 0xbd, 0xdf, 0x33, 0x16, 0xfe, 0xa0, 0xed, 0x45, 0x38, 0xe0, 0x35,
	0xfd, 0x02, 0x03, 0x01, 0x00, 0x01
};

const unsigned char *board_get_onetime_unlock_root_pubkey(unsigned int *key_len)
{
	if (!key_len)
		return NULL;

	switch (board_type_value) {
		case BOARD_TYPE_MALBEC_PROTO:
		case BOARD_TYPE_MALBEC_HVT:
		case BOARD_TYPE_MALBEC:
			*key_len = sizeof(onetime_unlock_root_pubkey_malbec);
			return onetime_unlock_root_pubkey_malbec;
		default:
			break;
	}
	return NULL;
}

#endif
