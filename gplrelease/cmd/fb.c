#include <common.h>
#include <command.h>
#include <asm/libfboot.h>

#define MMU_TABLE_ADDR	\
		(KLOWMEM_TO_PHYS(CONFIG_FALCON_BIOS_ADDR + CONFIG_FALCON_BIOS_SIZE - CONFIG_FALCON_MMU_TABLE_SIZE))

DECLARE_GLOBAL_DATA_PTR;

static unsigned long bios_addr = 0;
static unsigned long sbios_addr = 0;

static struct bootarg_t bootarg;
static struct bank_info bank_info = {};

extern unsigned char work_buff[];

static unsigned int saved_ttbcr = 0;
void switch_mmu_fb(void);
void switch_mmu_uboot(void);
extern void v7_inval_tlb(void);

void switch_mmu_fb(void)
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

void switch_mmu_uboot(void)
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

/*
 * do_bios_init :
 */
static int do_bios_init(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int bank = 0;
	int learn = 0;
	int learn_mode = 0;
	char *errmsg = NULL;
	int ret = 1;

	if (!(4 <= argc && argc <= 6)) {
		cmd_usage(cmdtp);
		return ret;
	}

	bios_addr = simple_strtoul(argv[1], NULL, 16);
	sbios_addr = simple_strtoul(argv[2], NULL, 16);
	bank = simple_strtol(argv[3], NULL, 16);

	if (argc >= 5)
		learn = simple_strtol(argv[4], NULL, 10);
	if (argc >= 6)
		learn_mode = simple_strtol(argv[5], NULL, 16);

	printf("BIOS: %lx  S-BIOS:%lx\n", (unsigned long)bios_addr, (unsigned long)sbios_addr);
	printf("use bank %lx\n", (unsigned long)bank);
	printf("learning stop at %d\n", learn);
	printf("learning mode at %d\n", learn_mode);

	bootarg.bank = bank;
	bootarg.learn = learn;
	bootarg.learn_mode = learn_mode;
	bootarg.bankinfo = &bank_info;

	switch_mmu_fb();

	ret = fb_bios_init(bios_addr, sbios_addr, &bootarg);

	switch(ret){
	case -1:
		errmsg = "BIOS is not exist.\n";
		break;
	case -2:
		errmsg = "Fatal: Falcon workarea is not found.\n";
		break;
	case -3:
		errmsg = "BIOS initialization is failed.\n";
		break;
	}

	switch_mmu_uboot();

	if (errmsg)
		printf("%s", errmsg);

	return 	ret ? 1 : 0;
}

U_BOOT_CMD(biosinit, 6, 0, do_bios_init,
		   "biosinit <F-BIOS ADDR> <S-BIOS ADDR> <BANK> [<LEARNING>=0] [<LEARNING_EN>=0]",
		   "initialize bios before fast boot"
	);

static int read_bank(void *buff)
{
	int ret = 1;

	char *arg_str[4];
	char arg_str1[64];
	char arg_str2[64];
	char arg_str3[64];
	int new_argc = 4;

	if (!bank_info.start && !bank_info.length) {
//		printf("BIOS is not initialized yet.");
		return ret;
	}

	sprintf(arg_str1, "%p", buff);
	sprintf(arg_str2, "%x", bank_info.start);
	sprintf(arg_str3, "%x", 8); // 1page

	arg_str[1] = arg_str1;
	arg_str[2] = arg_str2;
	arg_str[3] = arg_str3;

	extern int do_mmc_read(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[]);

	ret = do_mmc_read(NULL, 0, new_argc, arg_str);

	return ret;
}

/*
 * do_fboot:
 */
static int do_fboot(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	char *errmsg = NULL;
	switch_mmu_fb();

	if (!bios_addr || !sbios_addr ||
		!fb_is_exist_bios(bios_addr, sbios_addr)) {
		errmsg = "BIOS is not loaded or not found.\n";
		goto err;
	}

	void * buff;
	buff = (void*) PAGE_ALIGN((unsigned long)work_buff);

	if (read_bank(buff)) {
		errmsg = "read bank failure.\n";
		goto err;
	}

	if (!fb_is_valid_image_buff(buff)) {
		errmsg = "image is invalid.\n";
		goto err;
	}

	fb_fastboot();

  err:
	switch_mmu_uboot();

	if (errmsg)
		printf("%s", errmsg);

	return 1;
}

U_BOOT_CMD(fb, 1, 0, do_fboot,
		   "fb",
		   "fastboot from saved image."
);
