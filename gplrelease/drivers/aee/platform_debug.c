// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#include "include/aee_platform_debug.h"
#include "include/ram_console.h"

unsigned int (*plat_dfd20_get)(u64 offset, int *len,
			       CALLBACK dev_write) = NULL;
unsigned int (*plat_dram_get)(u64 offset, int *len,
			      CALLBACK dev_write) = NULL;
unsigned int (*plat_cpu_bus_get)(u64 offset, int *len,
				 CALLBACK dev_write) = NULL;
unsigned int (*plat_spm_data_get)(u64 offset, int *len,
				  CALLBACK dev_write) = NULL;
unsigned int (*plat_spm_sram_data_get)(u64 offset, int *len,
				       CALLBACK dev_write) = NULL;
unsigned int (*plat_atf_log_get)(u64 offset, int *len,
				 CALLBACK dev_write) = NULL;
unsigned int (*plat_atf_crash_get)(u64 offset, int *len,
				   CALLBACK dev_write) = NULL;
unsigned int (*plat_atf_raw_log_get)(u64 offset, int *len,
				     CALLBACK dev_write) = NULL;
unsigned int (*plat_atf_rdump_get)(u64 offset, int *len,
				   CALLBACK dev_write) = NULL;
unsigned int (*plat_hvfs_get)(u64 offset, int *len,
			      CALLBACK dev_write) = NULL;
#ifdef MTK_TINYSYS_SSPM_SUPPORT
unsigned int (*plat_sspm_coredump_get)(u64 offset, int *len,
				       CALLBACK dev_write) = NULL;
unsigned int (*plat_sspm_data_get)(u64 offset, int *len,
				   CALLBACK dev_write) = NULL;
unsigned int (*plat_sspm_xfile_get)(u64 offset, int *len,
				    CALLBACK dev_write) = NULL;
unsigned int (*plat_sspm_log_get)(u64 offset, int *len,
				  CALLBACK dev_write) = NULL;
#endif
unsigned int (*plat_pllk_last_log_get)(u64 offset, int *len,
				       CALLBACK dev_write) = NULL;
unsigned int (*plat_dur_lkdump_get)(u64 offset, int *len,
				    CALLBACK dev_write) = NULL;
unsigned int (*plat_mcdi_get)(u64 offset, int *len,
			      CALLBACK dev_write) = NULL;
#ifdef MTK_TINYSYS_SCP_SUPPORT
unsigned int (*plat_scp_coredump_get)(u64 offset, int *len,
				      CALLBACK dev_write) = NULL;
#endif

#ifdef CONFIG_MTK_AEE_SAVE_DEBUGINFO_RESERVED_BUFFER
#define KEDUMP_BUFFER_SIG 0x504D5544454B  //"KEDUMP"

static struct kedump_reserved_buffer *kedump_reserved_buffer = NULL;
#endif

void modify_plat_dfd20_get(void *func)
{
	plat_dfd20_get = func;
}

void modify_plat_dram_get(void *func)
{
	plat_dram_get = func;
}

void modify_plat_cpu_bus_get(void *func)
{
	plat_cpu_bus_get = func;
}

void modify_plat_spm_data_get(void *func)
{
	plat_spm_data_get = func;
}

void modify_plat_spm_sram_data_get(void *func)
{
	plat_spm_sram_data_get = func;
}

void modify_plat_atf_log_get(void *func)
{
	plat_atf_log_get = func;
}

void modify_plat_atf_crash_get(void *func)
{
	plat_atf_crash_get = func;
}

void modify_plat_atf_raw_log_get(void *func)
{
	plat_atf_raw_log_get = func;
}

void modify_plat_atf_rdump_get(void *func)
{
	plat_atf_rdump_get = func;
}

void modify_plat_hvfs_get(void *func)
{
	plat_hvfs_get = func;
}

#ifdef MTK_TINYSYS_SSPM_SUPPORT
void modify_plat_sspm_coredump_get(void *func)
{
	plat_sspm_coredump_get = func;
}

void modify_plat_sspm_data_get(void *func)
{
	plat_sspm_data_get = func;
}

void modify_plat_sspm_xfile_get(void *func)
{
	plat_sspm_xfile_get = func;
}

void modify_plat_sspm_log_get(void *func)
{
	plat_sspm_log_get = func;
}
#endif

void modify_plat_pllk_last_log_get(void *func)
{
	plat_pllk_last_log_get = func;
}

void modify_plat_dur_lkdump_get(void *func)
{
	plat_dur_lkdump_get = func;
}

void modify_plat_mcdi_get(void *func)
{
	plat_mcdi_get = func;
}

#ifdef MTK_TINYSYS_SCP_SUPPORT
void modify_plat_scp_coredump_get(void *func)
{
	plat_scp_coredump_get = func;
}
#endif

/* in case that platform didn't support platform_debug_init() */
int platform_debug_init(void) __attribute__((weak));
int platform_debug_init(void)
{
	return 0;
}

int lkdump_debug_init(void)
{
	//if (g_boot_arg->boot_mode != DOWNLOAD_BOOT)
	//    atf_log_init();

#ifdef CONFIG_MTK_AEE_SAVE_DEBUGINFO_RESERVED_BUFFER
	kedump_reserved_buffer = PA_TO_VA((unsigned long)KEDUMP_BUFFER_ADDR);
	memset(kedump_reserved_buffer, 0, KEDUMP_BUFFER_SIZE);
	kedump_reserved_buffer->sig = KEDUMP_BUFFER_SIG;
	kedump_reserved_buffer->log_offset = sizeof(struct kedump_reserved_buffer);
#endif

	return platform_debug_init();
}

/* function pointer should be set after platform_debug_init() */
unsigned int kedump_plat_savelog(int condition, u64 offset,
				 int *len, CALLBACK dev_write)
{
	switch (condition) {
	case AEE_PLAT_DFD20:
		return (!plat_dfd20_get ? 0 :
			plat_dfd20_get(offset, len, dev_write));
	case AEE_PLAT_DRAM:
		return (!plat_dram_get ? 0 :
			plat_dram_get(offset, len, dev_write));
	case AEE_PLAT_CPU_BUS:
		return (!plat_cpu_bus_get ? 0 :
			plat_cpu_bus_get(offset, len, dev_write));
	case AEE_PLAT_SPM_DATA:
		return (!plat_spm_data_get ? 0 :
			plat_spm_data_get(offset, len, dev_write));
	case AEE_PLAT_SPM_SRAM_DATA:
		return (!plat_spm_sram_data_get ? 0 :
			plat_spm_sram_data_get(offset, len, dev_write));
	case AEE_PLAT_ATF_LAST_LOG:
		return (!plat_atf_log_get ? 0 :
			plat_atf_log_get(offset, len, dev_write));
	case AEE_PLAT_ATF_CRASH_REPORT:
		return (!plat_atf_crash_get ? 0 :
			plat_atf_crash_get(offset, len, dev_write));
	case AEE_PLAT_ATF_RAW_LOG:
		return (!plat_atf_raw_log_get ? 0 :
			plat_atf_raw_log_get(offset, len, dev_write));
	case AEE_PLAT_ATF_RDUMP_LOG:
		return (!plat_atf_rdump_get ? 0 :
			plat_atf_rdump_get(offset, len, dev_write));
	case AEE_PLAT_HVFS:
		return (!plat_hvfs_get ? 0 :
			plat_hvfs_get(offset, len, dev_write));
#ifdef MTK_TINYSYS_SSPM_SUPPORT
	case AEE_PLAT_SSPM_COREDUMP:
		return (!plat_sspm_coredump_get ? 0 :
			plat_sspm_coredump_get(offset, len, dev_write));
	case AEE_PLAT_SSPM_DATA:
		return (!plat_sspm_data_get ? 0 :
			plat_sspm_data_get(offset, len, dev_write));
	case AEE_PLAT_SSPM_XFILE:
		return (!plat_sspm_xfile_get ? 0 :
			plat_sspm_xfile_get(offset, len, dev_write));
	case AEE_PLAT_SSPM_LAST_LOG:
		return (!plat_sspm_log_get ? 0 :
			plat_sspm_log_get(offset, len, dev_write));
#endif
	case AEE_PLAT_PLLK_LAST_LOG:
		return (!plat_pllk_last_log_get ? 0 :
			plat_pllk_last_log_get(offset, len, dev_write));
	case AEE_PLAT_LOG_DUR_LKDUMP:
		return (!plat_dur_lkdump_get ? 0 :
			plat_dur_lkdump_get(offset, len, dev_write));
	case AEE_PLAT_MCDI_DATA:
		return (!plat_mcdi_get ? 0 :
			plat_mcdi_get(offset, len, dev_write));
#ifdef MTK_TINYSYS_SCP_SUPPORT
	case AEE_PLAT_SCP_COREDUMP:
		return (!plat_scp_coredump_get ? 0 :
			plat_scp_coredump_get(offset, len, dev_write));
#endif
	default:
		break;
	}
	return 0;
}

#ifdef CONFIG_MTK_AEE_SAVE_DEBUGINFO_RESERVED_BUFFER
unsigned long long kedump_write_reserved_buffer(unsigned long long offset, u64 data, unsigned long sz)
{
        if (kedump_reserved_buffer) {
                aee_pr_crit("%s: offset:0x%llx, data addr:0x%0x, size:0x%0lx\n",
                        __FUNCTION__, offset, data, sz);

                if (offset >= KEDUMP_BUFFER_SIZE) {
                        aee_pr_crit("%s: write offset %0x >= %0x, not write\n",
                                __FUNCTION__, offset, KEDUMP_BUFFER_SIZE);
                        offset = KEDUMP_BUFFER_SIZE;
                        return 0;
                }

                if(sz > KEDUMP_BUFFER_SIZE - offset) {
                        aee_pr_crit("%s: write oversize: %0lx > %0x - %0x, only write to the rest part\n",
                                __FUNCTION__, sz, KEDUMP_BUFFER_SIZE, offset);
                        sz = KEDUMP_BUFFER_SIZE - offset;
                }
                memcpy(((char*)kedump_reserved_buffer + offset), data, sz);

                aee_pr_crit("%s: buffer addr:0x%0x, actual size:0x%0lx\n",
                        __FUNCTION__, (char*)kedump_reserved_buffer + offset, sz);

                offset += sz;

                return sz;
        } else {
                aee_pr_crit("%s: buffer not ready\n", __FUNCTION__);
                return 0;
        }
}
#endif

