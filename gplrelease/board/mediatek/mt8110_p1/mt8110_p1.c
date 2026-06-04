// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <common.h>
#include <dm.h>
#include <wdt.h>
#ifdef CONFIG_MTK_THERMAL
#include <thermal.h>
#endif
DECLARE_GLOBAL_DATA_PTR;
#ifdef CONFIG_FALCON
static void create_mmu_table_for_falcon(void);
#endif /* CONFIG_FALCON */

int board_init(void)
{
	/* address of boot parameters */
	gd->bd->bi_boot_params = CONFIG_SYS_SDRAM_BASE + 0x100;

	debug("gd->fdt_blob is %p\n", gd->fdt_blob);

#ifdef CONFIG_FALCON
	create_mmu_table_for_falcon();
#endif /* CONFIG_FALCON */

	return 0;
}

int board_late_init(void)
{
#if (CONFIG_USB_FUNCTION_FASTBOOT & CONFIG_WDT_MTK)
	/* check if we need to enter fastboot */
	pr_info("Check Fastboot...\n");
	if (check_fastboot_mode()) {
		pr_info("Clear Fastboot flag...\n");
		set_clr_fastboot_mode(0);
		if (run_command("fastboot usb 0", 0))
			pr_err("Failed to execute the fastboot command\n");
	}
#endif
#ifdef CONFIG_MTK_THERMAL
	thermal_init();
#endif
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

#ifdef CONFIG_FALCON
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

static void create_mmu_table_for_falcon(void)
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
#endif /* CONFIG_FALCON */
