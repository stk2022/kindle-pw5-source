// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#include "include/ram_console.h"
#include "include/types.h"
#include "dev/mtk_wdt.h"
#include <stdarg.h>
#include <linux/kernel.h>

#define RAM_CONSOLE_SIG (0x43474244) /* DBGC */
#define MOD "RAM_CONSOLE"

char logbuf[SZLOG];

struct ram_console_buffer {
	u32 sig;
	/* for size comptible */
	u32 off_pl;
	u32 off_lpl;
	u32 sz_pl;
	u32 off_lk;
	u32 off_llk; /* last lk */
	u32 sz_lk;
	u32 padding[2]; /* size = 2 * 16 = 32 byte */
	u32 dump_step;
	u32 sz_buffer;
	u32 off_linux;
	u32 off_console;
	u32 padding2[3];
};

#define RAM_CONSOLE_PL_SIZE 3
struct reboot_reason_pl {
	u32 wdt_status;
	u32 last_func[RAM_CONSOLE_PL_SIZE];
};

#define RAM_CONSOLE_LK_SIZE 16
struct reboot_reason_lk {
	u32 last_func[RAM_CONSOLE_LK_SIZE];
};

struct reboot_reason_kernel {
	u32 fiq_step;
	/* 0xaeedeadX: X=1 (HWT), X=2 (KE), X=3 (nested panic) */
	/* details see enum AEE_EXP_TYPE_NUM in ram_console_common.h */
	u32 exp_type;
	u32 others[0];
};

static struct ram_console_buffer *ram_console;
static u32 ram_console_size;

#define U_VAR(type) rc_##type
static unsigned int U_VAR(wdt_status), U_VAR(fiq_step), U_VAR(exp_type);
static bool U_VAR(set_flag);

static void ram_console_ptr_init(void)
{
	if (ram_console && ram_console_size) {
		aee_pr_crit("%s. already init start: 0x%zx, size: 0x%x, sig: 0x%x\n",
			    MOD, (uint32_t)ram_console, ram_console_size,
			    ram_console->sig);
		return;
	}

	ram_console = (struct ram_console_buffer *)
		PA_TO_VA((unsigned long)RAM_CONSOLE_DEF_ADDR);
	ram_console_size = RAM_CONSOLE_DEF_SIZE;
	aee_pr_crit("%s. pa start: 0x%zx, size: 0x%x\n",
		    MOD, (uint32_t)RAM_CONSOLE_DEF_ADDR, RAM_CONSOLE_DEF_SIZE);
}

int SLOG(const char *fmt, ...)
{
	va_list args;
	static int pos;

	va_start(args, fmt);
	if (pos < SZLOG - 1) /* vsnprintf bug */
		pos += vsnprintf(logbuf + pos, SZLOG - pos - 1, fmt, args);
	va_end(args);
	return 0;
}

void pl_ram_console_init(void)
{
	int i;
	struct reboot_reason_pl *rr_pl;

	ram_console = (struct ram_console_buffer *)
		PA_TO_VA((unsigned long)RAM_CONSOLE_DEF_ADDR);
	ram_console_size = RAM_CONSOLE_DEF_SIZE;
	aee_pr_crit("%s ram_console pa: 0x%zx, va:0x%zx, size: 0x%x\n",
		    __func__, (uint32_t)RAM_CONSOLE_DEF_ADDR,
		    (uint32_t)ram_console, RAM_CONSOLE_DEF_SIZE);

	if (ram_console->sig == RAM_CONSOLE_SIG &&
	    ram_console->sz_pl == sizeof(struct reboot_reason_pl) &&
	    ram_console->off_pl + ALIGN(ram_console->sz_pl, 64)
	    == ram_console->off_lpl) {
		rr_pl = (void *)ram_console + ram_console->off_pl;
		for (i = 0; i < RAM_CONSOLE_PL_SIZE; i++)
			aee_pr_crit("0x%x ", rr_pl->last_func[i]);
		aee_pr_crit("\n");
		memcpy((void *)ram_console + ram_console->off_lpl,
		       (void *)ram_console + ram_console->off_pl,
			ram_console->sz_pl);
	} else {
		memset(ram_console, 0, ram_console_size);
		ram_console->sig = RAM_CONSOLE_SIG;
		ram_console->off_pl = sizeof(struct ram_console_buffer);
		ram_console->sz_pl = sizeof(struct reboot_reason_pl);
		ram_console->off_lpl = ram_console->off_pl +
			ALIGN(ram_console->sz_pl, 64);
		aee_pr_crit("%s first init\n", __func__);
	}
}

void ram_console_init(void)
{
	int i;
	struct reboot_reason_lk *rr_lk;

	ram_console_ptr_init();
	if (ram_console) {
		aee_pr_crit("%s. start: 0x%lx, size: 0x%x\n",
			    MOD, (uint32_t)ram_console, ram_console_size);
	} else {
		aee_pr_crit("%s. sig not match\n", MOD);
		return;
	}

	ram_console->off_lk = ram_console->off_lpl +
		ALIGN(ram_console->sz_pl, 64);
	ram_console->off_llk = ram_console->off_lk +
		ALIGN(sizeof(struct reboot_reason_lk), 64);
	ram_console->sz_lk = sizeof(struct reboot_reason_lk);
	if (ram_console->sz_lk == sizeof(struct reboot_reason_lk) &&
	    (ram_console->off_lk +
	    ram_console->sz_lk == ram_console->off_llk)) {
		aee_pr_crit("%s. lk last status: ", MOD);
		rr_lk = (void *)ram_console + ram_console->off_lk;
		for (i = 0; i < RAM_CONSOLE_LK_SIZE; i++)
			aee_pr_crit("0x%x ", rr_lk->last_func[i]);
		aee_pr_crit("\n");
		memcpy((void *)ram_console + ram_console->off_llk,
		       (void *)ram_console +
		       ram_console->off_lk, ram_console->sz_lk);
	} else {
		aee_pr_crit("%s. lk size mismatch %x + %x != %x\n",
			    MOD, ram_console->sz_lk, ram_console->off_lk,
			    ram_console->off_llk);
		ram_console->sz_lk = sizeof(struct reboot_reason_lk);
	}
	ram_console->off_linux = ram_console->off_llk +
		ALIGN(ram_console->sz_lk, 64);
}

void ram_console_reboot_reason_save(u32 rgu_status)
{
	struct reboot_reason_pl *rr_pl;

	if (ram_console) {
		rr_pl = (void *)ram_console + ram_console->off_pl;
		rr_pl->wdt_status = rgu_status;
		aee_pr_crit("%s wdt status (0x%x)=0x%x\n",
			    MOD, rr_pl->wdt_status, rgu_status);
	}
}

//#define RE_BOOT_BY_WDT_SW 2
#define RE_BOOT_NORMAL_BOOT 0
#define RE_BOOT_BY_EINT 256/*we can find the definition from preloader ,
			    *this value should  sync with preloader incase
			    *issue happened
			    */
#define RE_BOOT_BY_SYSRST 512/*we can find the definition from preloader ,
			      *this value should  sync with preloader incase
			      *issue happened
			      */

#ifdef MTK_PMIC_FULL_RESET
#define RE_BOOT_FULL_PMIC 0x800
#endif

int ram_console_reboot_by_mrdump_key(void)
{
	unsigned int wdt_status;

	wdt_status = ((struct reboot_reason_pl *)
		((void *)ram_console + ram_console->off_pl))->wdt_status;
	return ((wdt_status & RE_BOOT_BY_EINT) |
		(wdt_status & RE_BOOT_BY_SYSRST)) ? true : false;
}

#ifdef MTK_PMIC_FULL_RESET
bool ram_console_reboot_by_cold_reset(void)
{
	unsigned int wdt_status;

	wdt_status = ((struct reboot_reason_pl *)
	((void *)ram_console + ram_console->off_pl))->wdt_status;
	return (wdt_status & RE_BOOT_FULL_PMIC) ? true : false;
}
#endif

int ram_console_get_wdt_status(unsigned int *wdt_status)
{
	if (wdt_status && U_VAR(set_flag)) {
		*wdt_status = U_VAR(wdt_status);
		return true;
	}
	return false;
}

int ram_console_get_fiq_step(unsigned int *fiq_step)
{
	if (fiq_step && U_VAR(set_flag)) {
		*fiq_step = U_VAR(fiq_step);
		return true;
	}
	return false;
}

int ram_console_get_exp_type(unsigned int *exp_type)
{
	if (exp_type && U_VAR(set_flag)) {
		*exp_type = U_VAR(exp_type);
		return true;
	}
	return false;
}

int ram_console_set_exp_type(unsigned int exp_type)
{
	bool ret = false;

	if (ram_console && ram_console->off_linux &&
	    (ram_console->off_linux ==
	    (ram_console->off_llk + ALIGN(ram_console->sz_lk, 64))) &&
	    ram_console->off_pl == sizeof(struct ram_console_buffer)) {
		if (exp_type < 16) {
			exp_type = exp_type ^ RAM_CONSOLE_EXP_TYPE_MAGIC;
			((struct reboot_reason_kernel *)((void *)ram_console
			+ ram_console->off_linux))->exp_type = exp_type;
			ret = true;
		} else {
			aee_pr_crit("%s. set exp type failed: off_linux:0x%x, off_llk:0x%x, off_pl:0x%x, exp type:%d\n",
				    MOD, ram_console->off_linux,
				    ram_console->off_llk,
				    ram_console->off_pl, exp_type);
		}
	}
	return ret;
}

int ram_console_is_abnormal_boot(void)
{
	unsigned int fiq_step, wdt_status, exp_type;
	int reinit_flag = 0;

	if (!ram_console) {
		ram_console_ptr_init();
		reinit_flag = 1;
	}
	if (ram_console && ram_console->off_linux &&
	    (ram_console->off_linux ==
	    (ram_console->off_llk + ALIGN(ram_console->sz_lk, 64))) &&
	    ram_console->off_pl == sizeof(struct ram_console_buffer)) {
		wdt_status = ((struct reboot_reason_pl *)((void *)ram_console
			      + ram_console->off_pl))->wdt_status;
		fiq_step = ((struct reboot_reason_kernel *)((void *)ram_console
			    + ram_console->off_linux))->fiq_step;
		exp_type = ((struct reboot_reason_kernel *)((void *)ram_console
			    + ram_console->off_linux))->exp_type;
		aee_pr_crit("%s. wdt_status 0x%x, fiq_step 0x%x, exp_type 0x%x\n",
			    MOD, wdt_status, fiq_step,
			    RAM_CONSOLE_EXP_TYPE_DEC(exp_type));
		if (fiq_step != 0 &&
		    (exp_type ^ RAM_CONSOLE_EXP_TYPE_MAGIC) >= 16) {
			fiq_step = 0;
			((struct reboot_reason_kernel *)((void *)ram_console
				+ ram_console->off_linux))->fiq_step = fiq_step;
		}
		U_VAR(wdt_status) = wdt_status;
		U_VAR(fiq_step) = fiq_step;
		U_VAR(exp_type) = RAM_CONSOLE_EXP_TYPE_DEC(exp_type);
		U_VAR(set_flag) = true;
		aee_pr_crit("%s. set reboot reason info done\n", MOD);

		if (wdt_status == RE_BOOT_BY_WDT_SW &&
		    fiq_step == 0 && U_VAR(exp_type) == 0)
			return false; /* adb reboot */
#ifdef MTK_PMIC_FULL_RESET
		else if (wdt_status == RE_BOOT_FULL_PMIC && fiq_step == 0)
			return false; /* full pmic reset */
#endif
		else if (wdt_status == RE_BOOT_NORMAL_BOOT) /* power off->on */
			return false;
		else
			return true;
	} else {
		if (ram_console) {
			aee_pr_crit("%s. set reboot reason info failed: off_linux:0x%x, off_llk:0x%x, off_pl:0x%x, reinit flag:%d\n",
				    MOD, ram_console->off_linux,
				    ram_console->off_llk,
				    ram_console->off_pl, reinit_flag);
		} else {
			aee_pr_crit("%s. ram console buffer NULL\n", MOD);
		}
	}
	return false;
}

void ram_console_lk_save(unsigned int val, int index)
{
	struct reboot_reason_lk *rr_lk;

	if (ram_console && ram_console->off_lk < ram_console_size) {
		rr_lk = (void *)ram_console + ram_console->off_lk;
		if (index < RAM_CONSOLE_LK_SIZE)
			rr_lk->last_func[index] = val;
	}
}

void ram_console_addr_size(unsigned long *addr, unsigned long *size)
{
	*addr = (unsigned long)ram_console;
	*size = ram_console_size;
}

void ram_console_set_dump_step(unsigned int step)
{
	if (ram_console)
		ram_console->dump_step = step;
}

int ram_console_get_dump_step(void)
{
	if (ram_console)
		return ram_console->dump_step;
	aee_pr_crit("%s. ram_console not ready\n", MOD);
	return 0;
}
