/*
 */

#ifndef __MT8110_P1_FALCON_H
#define __MT8110_P1_FALCON_H

/* */
#if CONFIG_KERNEL_LOWMEM_ADDR
#undef CONFIG_KERNEL_LOWMEM_ADDR
#endif
#define CONFIG_KERNEL_LOWMEM_ADDR	0xc0000000

#ifdef CONFIG_KERNEL_LOWMEM_SIZE
#undef CONFIG_KERNEL_LOWMEM_SIZE
#endif
#define CONFIG_KERNEL_LOWMEM_SIZE	0x1fa00000
#define PHYS_SDRAM			0x40000000
#define CONFIG_SYS_MEMTEST_START		0x50001000
#define CONFIG_SYS_MEMTEST_END			0x50002000

/* */
#define FALCON_BIOS_PADDR		0x5f9a0000
#define FALCON_SBIOS_PADDR		0x5f980000

#  define FALCON_MMU_TABLE_SIZE		0x4000
#  define FALCON_BIOS_LOAD_BLK_NUM	0x260	/* (CONFIG_FALCON_BIOS_SIZE-FALCON_MMU_TABLE_SIZE) / 512 */

#define MMU_TABLE_ADDR	\
		(CONFIG_FALCON_BIOS_ADDR + CONFIG_FALCON_BIOS_SIZE \
		 - FALCON_MMU_TABLE_SIZE \
		 - CONFIG_KERNEL_LOWMEM_ADDR + PHYS_SDRAM)

/* */
#ifdef CONFIG_FALCON_ENV_SETTINGS
#undef CONFIG_FALCON_ENV_SETTINGS
#endif
#define CONFIG_FALCON_ENV_SETTINGS \
	"bios_pos=0x0069b000\0" \
	"bios_size=" __stringify(FALCON_BIOS_LOAD_BLK_NUM) "\0" \
	"sbios_pos=0x0069b400\0" \
	"sbios_size=0x100\0" \
	"mmcdev=0\0" \
	"bios_paddr=" __stringify(FALCON_BIOS_PADDR) "\0" \
	"sbios_paddr=" __stringify(FALCON_SBIOS_PADDR) "\0" \
	"bios_vaddr=" __stringify(CONFIG_FALCON_BIOS_ADDR) "\0" \
	"sbios_vaddr=" __stringify(CONFIG_FALCON_STORAGE_BIOS_ADDR) "\0" \
	"bank=0\0" \
	"learn=0\0" \
	"learn_mode=0\0" \
	"loadbios=" \
		"dcache flush;" \
		"mmc dev ${mmcdev}; " \
		"mmc read ${bios_paddr} ${bios_pos} ${bios_size}; " \
		"mmc read ${sbios_paddr} ${sbios_pos} ${sbios_size}; " \
		"dcache flush; icache flush;\0" \
	"biosinit=biosinit ${bios_vaddr} ${sbios_vaddr} ${bank} ${learn} ${learn_mode}\0" \
	"fb=run loadbios biosinit; fb\0" \
	"boot=run loadbios; mmc dev ${mmcdev};mmc dev ${mmcdev}; if mmc rescan; " \
		"then if run loadbootscript; then run bootscript; " \
		"else if run loadimage; then run mmcboot_fb; else run netboot; " \
		" fi; fi; else run netboot; fi\0" \
	"mmcboot_fb=echo Booting from mmc ...; run mmcargs; " \
		"if test ${boot_fdt} = yes || test ${boot_fdt} = try; " \
		"then if run loadfdt; then run biosinit; " \
		"bootz ${loadaddr} - ${fdt_addr}; else if test ${boot_fdt} = try; " \
		"then bootz; else echo WARN: Cannot load the DT; fi; fi; else bootz; fi;\0" \
	"bootcmd=run fb; run mtk_boot\0"

#endif /* __MT8110_P1_FALCON_H */
