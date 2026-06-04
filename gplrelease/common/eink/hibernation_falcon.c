// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Amazon.com
 */

#include <common.h>
#include <command.h>
#include <asm/libfboot.h>
#include <mmc.h>

/* */
#define MMU_BLOCK_SHIFT		(20)
#define MMU_BLOCK_SIZE		(1ULL << MMU_BLOCK_SHIFT)
#define MMU_BLOCK_MASK		(~(MMU_BLOCK_SIZE - 1))

#define MAX_PTE_ENTRIES		1024
#define PTE_TYPE_MASK		(3 << 0)
#define PTE_TYPE_TABLE		(3 << 0)


/*Setup ARM v7 MMU for QuickBoot */
#define _ARM_VMSA_SECTION	(0x00000002)
#define _ARM_VMSA_EXECUTE_NEVER	(0x00000010)
#define _ARM_VMSA_BUFFERABLE	(0x00000004)
#define _ARM_VMSA_CACHEABLE	(0x00000008)
#define _ARM_VMSA_AP_RW		(0x00000C00)

#define _ARM_VMSA_SHARABLE	(0x00010000)
#define _ARM_VMSA_TEX		(0x00001000)

#define _VMSA_ACCESS_FLAGS (_ARM_VMSA_AP_RW | _ARM_VMSA_EXECUTE_NEVER | _ARM_VMSA_SECTION )
#define _VMSA_ACCESS_FLAGS_LOWMEM (_ARM_VMSA_SHARABLE | _ARM_VMSA_TEX | _ARM_VMSA_AP_RW | _ARM_VMSA_CACHEABLE |_ARM_VMSA_BUFFERABLE | _ARM_VMSA_SECTION  )

#define MMU_TABLE_ADDR	\
		(KLOWMEM_TO_PHYS(CONFIG_FALCON_BIOS_ADDR + CONFIG_FALCON_BIOS_SIZE - CONFIG_FALCON_MMU_TABLE_SIZE))

extern int part_get_info_by_name_or_alias(struct blk_desc *dev_desc, const char *name, disk_partition_t *info);
extern void v7_inval_tlb(void);

DECLARE_GLOBAL_DATA_PTR;

static unsigned int saved_ttbcr = 0;
static int quickboot_init_done = 0;

void ft_fixup_quickboot(void *blob)
{
	if (quickboot_init_done) {
		do_fixup_by_path(blob, "/mmc@11230000", "compatible", "falcon_blk", sizeof("falcon_blk"), 1);
		printf("   Applied falcon mmc driver\n");
	}
}

static inline void set_mmu_block(u32 vaddr, u32 paddr, u64 size, unsigned int mask)
{
	u32 *page_table = (u32 *)MMU_TABLE_ADDR;
	u32 value;

	size = (size + MMU_BLOCK_SIZE - 1) & MMU_BLOCK_MASK;
	vaddr &= MMU_BLOCK_MASK;
	paddr &= MMU_BLOCK_MASK;

	while (size) {
		value  = paddr;
		value |= mask;
		page_table[vaddr >> MMU_BLOCK_SHIFT] = value;

		vaddr += MMU_BLOCK_SIZE;
		paddr += MMU_BLOCK_SIZE;
		size  -= MMU_BLOCK_SIZE;
	}
}

static void falcon_create_mmu_table(void)
{
	u32 virt = 0x00000000;
	enum dcache_opton {
		NOMAP = 0,
	};

	/* Map sdram as direct map */
	set_mmu_block(0x0, 0x0,	0xFFFFFFFF, _VMSA_ACCESS_FLAGS);

	/* Map lowmem */
	set_mmu_block(CONFIG_KERNEL_LOWMEM_ADDR, CONFIG_SYS_SDRAM_BASE,
			CONFIG_KERNEL_LOWMEM_SIZE, _VMSA_ACCESS_FLAGS_LOWMEM);

	/* Unmap BIOS and S-BIOS in U-Boot */
	mmu_set_region_dcache_behaviour(CONFIG_FALCON_BIOS_ADDR, CONFIG_FALCON_BIOS_SIZE, NOMAP);
	mmu_set_region_dcache_behaviour(CONFIG_FALCON_STORAGE_BIOS_ADDR, CONFIG_FALCON_STORAGE_BIOS_SIZE, NOMAP);

	/* Set VBAR to top of F-BIOS  */
	virt = CONFIG_FALCON_BIOS_ADDR;
	__asm__ __volatile__ (
			"mcr p15, 0, %0, c12, c0, 0\n" ::"r"(virt));
}

static void switch_mmu_fb(void)
{
	u32 reg;

	/* Save TTBCR */
	asm volatile("mrc p15, 0, %0, c2, c0, 2"
			: "=r"(saved_ttbcr) :: "memory");

	flush_dcache_all();
	v7_inval_tlb();

	/* Set TTBR0 */
	reg  = MMU_TABLE_ADDR & 0xffffc000;
	reg |= (TTBR0_RGN_WB | TTBR0_IRGN_WB);
	asm volatile("mcrr p15, 0, %0, %1, c2"
			: : "r"(reg), "r"(0) : "memory");

	/* Set MAIR0  0 .. Strongly-ordered */
	asm volatile("mcr p15, 0, %0, c10, c2, 0"
			: : "r"(0xffcc8800) : "memory");

	/* Set TTBCR EAE=0 */
	asm volatile("mcr p15, 0, %0, c2, c0, 2"
			: : "r"(0x00000020) : "memory");

	v7_inval_tlb();
}

static void switch_mmu_uboot(void)
{
	flush_dcache_all();
	v7_inval_tlb();
	asm volatile("mcr p15, 0, %0, c2, c0, 0"
			: : "r" (gd->arch.tlb_addr) : "memory");

	/* Set TTBCR EAE=0 */
	asm volatile("mcr p15, 0, %0, c2, c0, 2"
			: : "r"(saved_ttbcr) : "memory");
	v7_inval_tlb();
}

static int get_partition_info(const char *partition, struct blk_desc **dev_desc, disk_partition_t *info)
{
	struct blk_desc *desc;

	/* find partition */
	desc = blk_get_dev("mmc", 0);
	if (!desc) {
		pr_err("cannot find mmc device\n");
		return -ENODEV;
	}

	if (part_get_info_by_name_or_alias(desc, partition, info) <= 0) {
		pr_err("cannot find partition: %s\n", partition);
		return -EINVAL;
	}

	if (dev_desc)
		*dev_desc = desc;
	return 0;
}

int quickboot_load(const char* partition)
{
	int ret = 0;
	int n, conf_noffset;
	struct blk_desc *dev_desc;
	disk_partition_t info;
	void *load;
	bootm_headers_t images;
	const char *uname;

	load = (void*)load_addr;

	ret = get_partition_info(partition, &dev_desc, &info);
	if (ret) {
		return ret;
	}

	n = blk_dread(dev_desc, info.start, info.size, load);
	if (n != info.size) {
		pr_err("fail to load quickboot partition: %s\n", partition);
		return -EIO;
	}

	/* check fdt image */
	ret = fit_check_format(load, IMAGE_SIZE_INVAL);
	if (!ret) {
		pr_err("invalid fdt image\n");
		return ret;
	}

	conf_noffset = fit_conf_get_node(load, NULL);
	images.verify = 1;
	puts("   Verifying Hash Integrity ... ");
	if (fit_config_verify(load, conf_noffset)) {
		pr_err("Bad Data Hash\n");
		return -EPERM;
	}
	puts("OK\n");

	for (n = 0; (uname = fdt_stringlist_get(load, conf_noffset, FIT_LOADABLE_PROP, n, NULL)); n++) {
		ret = fit_image_load(&images, (ulong)load,
				&uname, NULL,
				IH_ARCH_DEFAULT, IH_TYPE_LOADABLE, -1,
				FIT_LOAD_REQUIRED, NULL, NULL);
		if (ret<0) {
			pr_err("fail to load quickboot image\n");
			return ret;
		}
	}
	return 0;
}

int quickboot_init(void)
{
	struct bootarg_t bootarg;
	struct bank_info bank_info = {0};
	disk_partition_t info;
	struct mmc *mmc;
	int ret;

	falcon_create_mmu_table();

	/* Get snapshot partition info for bank */
	ret = get_partition_info("snapshot", NULL, &info);
	if (!ret) {
		/* bank_info use block count to match output of get_storage_bank_info() */
		bank_info.start = info.start;
		bank_info.length = info.size;
		debug("overwrite bank info: start=%d, size=%d\n", bank_info.start, bank_info.length);
	}

	bootarg.bank = 0;
	bootarg.learn = 0;
	bootarg.learn_mode = 0;
	bootarg.bankinfo = &bank_info;

	switch_mmu_fb();
	ret = fb_bios_init(CONFIG_FALCON_BIOS_ADDR, CONFIG_FALCON_STORAGE_BIOS_ADDR, &bootarg);
	switch_mmu_uboot();

	switch(ret){
		case -1:
			pr_err("BIOS is not exist.\n");
			break;
		case -2:
			pr_err("Fatal: Falcon workarea is not found.\n");
			break;
		case -3:
			pr_err("BIOS initialization is failed.\n");
			break;
	}

	if (!ret) {
		quickboot_init_done = 1;
		debug("bank info: start=%d, size=%d\n", bank_info.start, bank_info.length);
		mmc = find_mmc_device(CONFIG_FASTBOOT_FLASH_MMC_DEV);
		mmc->has_init = 0;
	}
	return ret;
}

int quickboot_checkimg(void)
{
	int ret;

	if (!quickboot_init_done)
		return -EIO;

	switch_mmu_fb();
	ret = fb_is_valid_image();
	switch_mmu_uboot();
	if (!ret)
		return -ENOENT;

	return 0;
}

int quickboot_resume(void)
{
	if (!quickboot_init_done)
		return -EIO;

	switch_mmu_fb();
	fb_fastboot();
	switch_mmu_uboot();

	return -ENOENT;
}

static int do_quickboot_load(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	if (quickboot_load("quickboot")) {
		puts("Failed to load quickboot images\n");
		return CMD_RET_FAILURE;
	}
	puts("Loaded quickboot images\n");
	return CMD_RET_SUCCESS;
}

static int do_quickboot_init(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	if (quickboot_init()) {
		return CMD_RET_FAILURE;
	}
	puts("Falcon init done\n");
	return CMD_RET_SUCCESS;
}

static int do_quickboot_checkimg(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	if (quickboot_checkimg()) {
		puts("No snapshot image found\n");
		return CMD_RET_FAILURE;
	}
	puts("Snapshot image found\n");
	return CMD_RET_SUCCESS;
}

static int do_quickboot_resume(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	quickboot_resume();
	return CMD_RET_SUCCESS;
}

static cmd_tbl_t cmd_quickboot[] = {
	U_BOOT_CMD_MKENT(load, 1, 0, do_quickboot_load, "", ""),
	U_BOOT_CMD_MKENT(init, 1, 0, do_quickboot_init, "", ""),
	U_BOOT_CMD_MKENT(checkimg, 1, 0, do_quickboot_checkimg, "", ""),
	U_BOOT_CMD_MKENT(resume, 1, 0, do_quickboot_resume, "", ""),
};

static int do_quickboot_ops(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	cmd_tbl_t *cp;

	cp = find_cmd_tbl(argv[1], cmd_quickboot, ARRAY_SIZE(cmd_quickboot));

	/* Drop the quickboot command */
	argc--;
	argv++;

	if (cp == NULL || argc > cp->maxargs)
		return CMD_RET_USAGE;
	if (flag == CMD_FLAG_REPEAT && !cmd_is_repeatable(cp))
		return CMD_RET_SUCCESS;
	return cp->cmd(cmdtp, flag, argc, argv);
}

U_BOOT_CMD(qb, 4, 0, do_quickboot_ops,
		"quickboot system",
		"load\n"
		"    - load quickboot partition\n"
		"qb init\n"
		"    - init falcon\n"
		"qb resume\n"
		"    - resume from hibernation\n"
		"qb checkimg\n"
		"    - Check if snapshot image exist\n"
		);
