/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018 MediaTek Inc.
 */
#if !defined(__AEE_PLATFORM_DEBUG_H__)
#define __AEE_PLATFORM_DEBUG_H__

#include "include/types.h"

typedef unsigned long long u64;
typedef unsigned long long (*CALLBACK)(void *data, unsigned long sz);

enum {
	AEE_PLAT_DFD20,
	AEE_PLAT_DRAM,
	AEE_PLAT_CPU_BUS,
	AEE_PLAT_SPM_DATA,
	AEE_PLAT_SPM_SRAM_DATA,
	AEE_PLAT_ATF_LAST_LOG,
	AEE_PLAT_ATF_CRASH_REPORT,
	AEE_PLAT_ATF_RAW_LOG,
	AEE_PLAT_ATF_RDUMP_LOG,
	AEE_PLAT_HVFS,
#ifdef MTK_TINYSYS_SSPM_SUPPORT
	AEE_PLAT_SSPM_COREDUMP,
	AEE_PLAT_SSPM_DATA,
	AEE_PLAT_SSPM_XFILE,
	AEE_PLAT_SSPM_LAST_LOG,
#endif
	AEE_PLAT_PLLK_LAST_LOG,
	AEE_PLAT_LOG_DUR_LKDUMP,
	AEE_PLAT_MCDI_DATA,
	AEE_PLAT_SCP_COREDUMP,
	AEE_PLAT_DEBUG_NUM
};

/* function pointers */
void modify_plat_dfd20_get(void *func);
void modify_plat_dram_get(void *func);
void modify_plat_cpu_bus_get(void *func);
void modify_plat_spm_data_get(void *func);
void modify_plat_spm_sram_data_get(void *func);
void modify_plat_hvfs_get(void *func);
#ifdef MTK_TINYSYS_SSPM_SUPPORT
void modify_plat_sspm_coredump_get(void *func);
void modify_plat_sspm_data_get(void *func);
void modify_plat_sspm_xfile_get(void *func);
void modify_plat_sspm_log_get(void *func);
#endif
void modify_plat_pllk_last_log_get(void *func);
void modify_plat_dur_lkdump_get(void *func);
void modify_plat_mcdi_get(void *func);
#ifdef MTK_TINYSYS_SCP_SUPPORT
void modify_plat_scp_coredump_get(void *func);
#endif

/* DRAM KLOG at MRDUMP area of expdb,
 * offset from bottom = 3145728 - 16384 = 3129344
 */
#define MRDUMP_EXPDB_BOTTOM_OFFSET 2097152
#define MRDUMP_EXPDB_DRAM_KLOG_OFFSET 3129344

/* common api */
unsigned int kedump_plat_savelog(int condition,
				 u64 offset,
				 int *len,
				 CALLBACK dev_write);

/* common interface for platform */
void mrdump_write_log(u64 offset_dst, void *data, int len);
void mrdump_read_log(void *data, int len, u64 offset);

int lkdump_debug_init(void);
/* common interface from platform */
int platform_debug_init(void);
void platform_lastpc_postinit(void);

int systracker_get(void **data, int *len, unsigned int entry_num);
void systracker_put(void **data);

//extern struct aee_db_file_info *get_file_info(void);

/* db filename and max size */
struct aee_db_file_info {
		char filename[32];
		unsigned int filesize;
		unsigned int step;
};

#ifdef CONFIG_MTK_AEE_SAVE_DEBUGINFO_RESERVED_BUFFER
struct kedump_reserved_buffer {
        u64 sig;
        u32 log_offset;
};
#endif

#endif /* __AEE_PLATFORM_DEBUG_H__ */

