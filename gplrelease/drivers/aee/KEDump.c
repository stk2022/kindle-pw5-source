// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#include "include/KEHeader.h"
#include "include/elf.h"
#include "dev/ram_console_def.h"
#include "include/ram_console.h"
#include "include/aee_platform_debug.h"

#ifdef MTK_3LEVEL_PAGETABLE
#include <target.h>
#endif

#include "dev/boot_mode.h"
#include "dev/mtk_wdt.h"

#include <part.h>
#include <blk.h>

#include <jffs2/load_kernel.h>
#include <linux/mtd/mtd.h>
#include <nand.h>

extern char logbuf[SZLOG];

enum {
	AEE_LKDUMP_CLEAR = 0,
	AEE_LKDUMP_RAMCONSOLE_RAW,
	AEE_LKDUMP_PSTORE_RAW,
	AEE_LKDUMP_KEDUMP_CRC,
	AEE_LKDUMP_MINI_RDUMP,
	AEE_LKDUMP_PROC_CUR_TSK,    //5
	AEE_LKDUMP_KERNEL_LOG_RAW,
	AEE_LKDUMP_DISP_DEBUG_RAW,
	AEE_LKDUMP_DFD20,
	AEE_LKDUMP_LAST_DRAM,
	AEE_LKDUMP_LAST_CPU_BUS,    //10
	AEE_LKDUMP_LAST_SPM_DATA,
	AEE_LKDUMP_LAST_SPM_SRAM_DATA,
	AEE_LKDUMP_ATF_LAST,
	AEE_LKDUMP_ATF_CRASH,
	AEE_LKDUMP_ATF_RAW,
	AEE_LKDUMP_ATF_RDUMP,   //16
	AEE_LKDUMP_CPU_HVFS_RAW,
	AEE_LKDUMP_SSPM_COREDUMP,
	AEE_LKDUMP_SSPM_DATA,
	AEE_LKDUMP_SSPM_XFILE,
	AEE_LKDUMP_SSPM_LAST_LOG,   //21
	AEE_LKDUMP_PLLK_LAST_LOG,
	AEE_LKDUMP_MCDI_DATA,
	AEE_LKDUMP_SCP_COREDUMP,
	AEE_LKDUMP_ZAEE_LOG,
	AEE_LKDUMP_HEADER,      //26
	AEE_LKDUMP_UNKNOWN
};

static struct aee_db_file_info adfi[AEE_PLAT_DEBUG_NUM] = {
	[AEE_PLAT_DFD20]          = { "DFD20.dfd",
		0x40000, AEE_LKDUMP_DFD20},         /* 256 KB */
	[AEE_PLAT_DRAM]           = { "SYS_LAST_DRAM",
		0x2400,  AEE_LKDUMP_LAST_DRAM},       /*   9 KB */
	[AEE_PLAT_CPU_BUS]        = { "SYS_LAST_CPU_BUS",
		0x10000, AEE_LKDUMP_LAST_CPU_BUS}, /*  64 KB */
	[AEE_PLAT_SPM_DATA]       = { "SYS_LAST_SPM_DATA",
		0x1000,  AEE_LKDUMP_LAST_SPM_DATA}, /*  4 KB */
	[AEE_PLAT_SPM_SRAM_DATA]  = { "SYS_LAST_SPM_SRAM_DATA",
		0x1000,  AEE_LKDUMP_LAST_SPM_SRAM_DATA}, /*  4 KB */
	[AEE_PLAT_ATF_LAST_LOG]   = { "SYS_ATF_LAST",
		0x20000, AEE_LKDUMP_ATF_LAST}, /* 128KB */
	[AEE_PLAT_ATF_CRASH_REPORT] = { "SYS_ATF_CRASH",
		0x30000, AEE_LKDUMP_ATF_CRASH}, /* 64KB+128KB */
	[AEE_PLAT_ATF_RAW_LOG]    = { "SYS_ATF_RAW_LOG",
		0x60000, AEE_LKDUMP_ATF_RAW}, /* 384 KB */
	[AEE_PLAT_ATF_RDUMP_LOG]  = { "SYS_ATF_RDUMP",
		0x80000, AEE_LKDUMP_ATF_RDUMP }, /* 512KB */
	[AEE_PLAT_HVFS]           = { "SYS_CPUHVFS_RAW",
		0x3000,  AEE_LKDUMP_CPU_HVFS_RAW}, /* 12 KB */
#ifdef MTK_TINYSYS_SSPM_SUPPORT
	[AEE_PLAT_SSPM_COREDUMP]  = { "SYS_SSPM_COREDUMP",
		0x40080, AEE_LKDUMP_SSPM_COREDUMP}, /* 256KB + 128Byte */
	[AEE_PLAT_SSPM_DATA]      = { "SYS_SSPM_DATA",
		0x400,   AEE_LKDUMP_SSPM_DATA}, /* 1KB */
	[AEE_PLAT_SSPM_XFILE]     = { "SYS_SSPM_XFILE",
		0xA0000, AEE_LKDUMP_SSPM_XFILE}, /* 640KB */
	[AEE_PLAT_SSPM_LAST_LOG]  = { "SYS_SSPM_LAST_LOG",
		0x400,   AEE_LKDUMP_SSPM_LAST_LOG}, /* 1KB */
#endif
	[AEE_PLAT_PLLK_LAST_LOG]  = { "SYS_PLLK_LAST_LOG",
		0x40000, AEE_LKDUMP_PLLK_LAST_LOG}, /* 256KB */
	[AEE_PLAT_LOG_DUR_LKDUMP] = { "SYS_LOG_DUR_LKDUMP",
		0x40000, AEE_PLAT_LOG_DUR_LKDUMP}, /* 256KB */
	[AEE_PLAT_MCDI_DATA]      = { "SYS_MCDI_DATA",
		0x800,   AEE_LKDUMP_MCDI_DATA},
		/* 2KB, will modified by plat. */
#ifdef MTK_TINYSYS_SCP_SUPPORT
	[AEE_PLAT_SCP_COREDUMP]   = { "SYS_SCP_DUMP.gz",
		0xA0000, AEE_LKDUMP_SCP_COREDUMP} /* 640KB */
#endif
};

/* reserved expdb for control block and pl/lk log */
#define EXPDB_RESERVED_OTHER    (3 * 1024 * 1024)
#define MEM_EXPDB_SIZE  0x300000
static char *mem_expdb;

static struct blk_desc *dev_desc;
static disk_partition_t info;

struct elfhdr {
	void *start;
	unsigned int e_machine;
	unsigned int e_phoff;
	unsigned int e_phnum;
};

struct kedump_crc {
	unsigned int ram_console_crc;
	unsigned int pstore_crc;
};

static struct kedump_crc kc;

int check_ram_console_is_abnormal_boot(void)
{
	return ram_console_is_abnormal_boot();
}

static unsigned int last_dump_step;

#define elf_note    elf32_note

#define PHDR_PTR(ehdr, phdr, mem)       \
	(ehdr->e_machine == EM_ARM ? ((struct elf32_phdr *)phdr)->mem : \
	((struct elf64_phdr *)phdr)->mem)

#define PHDR_TYPE(ehdr, phdr) PHDR_PTR(ehdr, phdr, p_type)
#define PHDR_VADDR(ehdr, phdr) PHDR_PTR(ehdr, phdr, p_vaddr)
#define PHDR_ADDR(ehdr, phdr) PHDR_PTR(ehdr, phdr, p_paddr)
#define PHDR_SIZE(ehdr, phdr) PHDR_PTR(ehdr, phdr, p_filesz)
#define PHDR_OFF(ehdr, phdr) PHDR_PTR(ehdr, phdr, p_offset)

#define PHDR_INDEX(ehdr, i)         \
	(ehdr->e_machine == EM_ARM ? ehdr->start +   \
	ehdr->e_phoff + sizeof(struct elf32_phdr) * i :    \
	ehdr->start + ehdr->e_phoff + sizeof(struct elf64_phdr) * i)

static unsigned int calculate_crc32(void *data, unsigned int len)
{
	unsigned int crc_value = 0;

	crc_value = crc32(0L, data, len);
	aee_pr_crit("kedump: crc = 0x%x\n", crc_value);
	return crc_value;
}

static struct elfhdr *kedump_elf_hdr(void)
{
	char *ei;
	static struct elfhdr kehdr;
	static struct elfhdr *ehdr = (void *)-1;

	if (ehdr != (void *)-1)
		return ehdr;
	ehdr = NULL;

	kehdr.start = PA_TO_VA((unsigned long)KE_RESERVED_MEM_ADDR);
	aee_pr_crit("kedump: KEHeader %p\n", kehdr.start);
	if (kehdr.start) {
		ei = (char *)kehdr.start; //elf_hdr.e_ident
		aee_pr_crit("kedump: read header 0x%p[0x%x%x%x%x]\n",
			    ei, ei[0], ei[1], ei[2], ei[3]);
		/* valid elf header */
		if (ei[0] == 0x7f && ei[1] == 'E' &&
		    ei[2] == 'L' && ei[3] == 'F') {
			kehdr.e_machine = ((struct elf32_hdr *)
				(kehdr.start))->e_machine;
			if (kehdr.e_machine == EM_ARM) {
				kehdr.e_phnum =	((struct elf32_hdr *)
					(kehdr.start))->e_phnum;
				kehdr.e_phoff = ((struct elf32_hdr *)
					(kehdr.start))->e_phoff;
				ehdr = &kehdr;
			} else if (kehdr.e_machine == EM_AARCH64) {
				kehdr.e_phnum = ((struct elf64_hdr *)
					(kehdr.start))->e_phnum;
				kehdr.e_phoff = ((struct elf64_hdr *)
					(kehdr.start))->e_phoff;
				ehdr = &kehdr;
			}
		}
		if (!ehdr)
			aee_pr_crit("kedump: invalid header[0x%x%x%x%x]\n",
				    ei[0], ei[1], ei[2], ei[3]);
	}
	aee_pr_crit("kedump: mach[0x%x], phnum[0x%x], phoff[0x%x]\n",
		    kehdr.e_machine, kehdr.e_phnum, kehdr.e_phoff);
	return ehdr;
}

static int kedump_dev_open(void)
{
	int ret;

	dev_desc = blk_get_dev("mmc", 0);
	if (!dev_desc || dev_desc->type == DEV_TYPE_UNKNOWN) {
		aee_pr_crit("invalid mmc device\n");
		return -1;
	}

	ret = part_get_info_by_name(dev_desc, AEE_IPANIC_PLABLE, &info);
	if (ret < 0) {
		aee_pr_crit("cannot find partition: '%s'\n", AEE_IPANIC_PLABLE);
		return -1;
	}

	if (info.size * info.blksz < EXPDB_RESERVED_OTHER) {
		aee_pr_crit("kedump: part size(%llx) < reserved!(%llx)\n",
			    info.size * info.blksz,
			    (unsigned long long)EXPDB_RESERVED_OTHER);
		return -1;
	}
	return 0;
}

static unsigned long long mem_expdb_write(void *data,
					  unsigned long long offset,
					  unsigned long sz)
{
	if ((offset + sz) > MEM_EXPDB_SIZE) {
		aee_pr_crit("kedump: %s overflow!\n", __func__);
		return 0;
	}

	memcpy(mem_expdb + offset, data, sz);
	return sz;
}

#define TRUNK 0x8000
static unsigned long long kedump_dev_write(unsigned long long offset,
					   u64 data, unsigned long sz)
{
	unsigned long long size_wrote = 0;
	u8 *memsrc = (u8 *)data;
	unsigned long rest = sz;
	unsigned int ret = 0;
	unsigned long long mini_size = info.size * info.blksz
				- EXPDB_RESERVED_OTHER;

	aee_pr_crit("kedump: offset:0x%llx, data:0x%llx, size:0x%lx\n",
		    offset, data, sz);

	if (offset >= mini_size || sz > (mini_size - offset)) {
		aee_pr_crit("kedump: %s overflow\n", __func__);
		return 0;
	}
	while (rest > 0) {
		unsigned long write_sz;

		if (rest <= TRUNK)
			write_sz = rest;
		else
			write_sz = TRUNK;
		aee_pr_crit("kedump: offset:0x%llx, memsrc:0x%lx, write_sz:0x%lx\n",
			    offset, (size_t)memsrc, write_sz);

		ret = mem_expdb_write(memsrc, offset, write_sz);
		if (ret <= 0) {
			aee_pr_crit("kedump: %s fail\n", __func__);
			break;
		}

		size_wrote += ret;
		offset += write_sz;
		rest -= write_sz;
		memsrc += write_sz;
	}
	if ((long long)size_wrote <= 0) {
		aee_pr_crit("kedump: write failed(%llx), %lx@%llx -> %llx\n",
			    size_wrote, sz, data, offset);
		size_wrote = 0;
	}

	aee_pr_crit("kedump: kedump_dev_write done\n");
	return size_wrote;
}

static unsigned long long offset_plat_debug;
static unsigned long length_plat_debug;
static unsigned long long kedump_plat_write(void *data, unsigned long sz)
{
	unsigned long long datasize = 0;

#ifdef CONFIG_MTK_AEE_SAVE_DEBUGINFO_RESERVED_BUFFER
	datasize = kedump_write_reserved_buffer(offset_plat_debug, (uint64_t)data, sz);
#else
	datasize = kedump_dev_write(offset_plat_debug, (uint64_t)data, sz);
#endif
	offset_plat_debug += datasize;
	length_plat_debug += sz;

	return datasize;
}

/* the min offset reserved for the header's size. */
static unsigned long kedump_mrdump_header_size(struct elfhdr *ehdr)
{
	void *phdr = PHDR_INDEX(ehdr, 1);

	return ALIGN(PHDR_OFF(ehdr, phdr) + PHDR_SIZE(ehdr, phdr), PAGE_SIZE);
}

static unsigned int kedump_mini_rdump(struct elfhdr *ehdr,
				      unsigned long long offset)
{
	void *phdr;
	unsigned long long addr;
	void *vaddr;
	unsigned long size;
	unsigned int i;
	unsigned int total = 0;
	unsigned long elfoff = kedump_mrdump_header_size(ehdr);
	unsigned long sz_header = elfoff;

	for (i = 0; i < ehdr->e_phnum; i++) {
		phdr = PHDR_INDEX(ehdr, i);
		if (PHDR_SIZE(ehdr, phdr) != 0 || PHDR_TYPE(ehdr, phdr) != 0)
			LOGD("kedump: PT[%d] %x@%llx -> %x(%x)\n",
			     PHDR_TYPE(ehdr, phdr), PHDR_SIZE(ehdr, phdr),
			     PHDR_ADDR(ehdr, phdr), elfoff,
			     (unsigned int)PHDR_OFF(ehdr, phdr));
		if (PHDR_TYPE(ehdr, phdr) != PT_LOAD)
			continue;
		addr = PHDR_ADDR(ehdr, phdr);

		if (addr < DRAM_BASE_PHY) {
			aee_pr_crit("kedump: skip dump non-dram PA:%llx, VA:%llx\n",
				    addr, PHDR_VADDR(ehdr, phdr));
			continue;
		}

		size = PHDR_SIZE(ehdr, phdr);
		if (size == 0 || elfoff == 0)
			aee_pr_crit("kedump: dump addr 0x%llx, size 0x%lx\n",
				    addr, size);
		if (ehdr->e_machine == EM_ARM)
			((struct elf32_phdr *)phdr)->p_offset = elfoff;
		else
			((struct elf64_phdr *)phdr)->p_offset = elfoff;
		if (size != 0 && elfoff != 0) {
			vaddr = PA_TO_VA((unsigned long)addr);
#ifdef CONFIG_MTK_AEE_SAVE_DEBUGINFO_RESERVED_BUFFER
			total += kedump_write_reserved_buffer(offset + elfoff,
			  (uint64_t)vaddr, size);

#else
			total += kedump_dev_write(offset + elfoff,
						  (uint64_t)vaddr, size);
#endif
		}
		elfoff += size;
	}
#ifdef CONFIG_MTK_AEE_SAVE_DEBUGINFO_RESERVED_BUFFER
	total += kedump_write_reserved_buffer(offset, (uint64_t)(ehdr->start), sz_header);

#else
	total += kedump_dev_write(offset, (uint64_t)(ehdr->start), sz_header);
#endif
	return total;
}

static unsigned int kedump_misc(unsigned long long addr,
				unsigned int start, unsigned int size,
				unsigned long long offset)
{
	unsigned int total;

	aee_pr_crit("kedump: misc data %x@%llx+%x\n", size, addr, start);
	if (start >= size)
		start = start % size;
	total = kedump_dev_write(offset, (uint64_t)(addr + start),
				 size - start);
	if (start)
		total += kedump_dev_write(offset + total, (uint64_t)addr,
					  start);
	return total;
}

static unsigned int kedump_misc32(struct mrdump_mini_misc_data32 *data,
				  unsigned long long offset)
{
	if (data) {
		unsigned int addr = data->paddr;
		unsigned int start = 0;
		void *vaddr;
		unsigned int size = data->size;

		vaddr = PA_TO_VA((unsigned long)addr);

		start = data->start ? *(unsigned int *)(unsigned long)
			(PA_TO_VA(data->start)) : 0;

		return kedump_misc((uint64_t)vaddr, start, size, offset);
	} else {
		return 0;
	}
}

static unsigned int kedump_misc64(struct mrdump_mini_misc_data64 *data,
				  unsigned long long offset)
{
	if (data) {
		unsigned long long addr = (unsigned long long)data->paddr;
		unsigned int start = 0;
		void *vaddr;
		unsigned int size = (unsigned int)data->size;

		vaddr = PA_TO_VA((unsigned long)addr);

		start = data->start ? *(unsigned int *)(unsigned long)
			(PA_TO_VA((unsigned long)data->start)) : 0;

		return kedump_misc((uint64_t)vaddr, start, size, offset);
	} else {
		return 0;
	}
}

struct ipanic_header panic_header;
static unsigned long long header_off;
static void kedump_add2hdr(unsigned int offset,
			   unsigned int size,
			   unsigned int datasize,
			   const char *name)
{
	struct ipanic_data_header *pdata;
	int i;

	for (i = 0; i < IPANIC_NR_SECTIONS; i++) {
		pdata = &panic_header.data_hdr[i];
		if (pdata->valid == 0)
			break;
	}
	aee_pr_crit("kedump add: %s[%d] %x/%x@%x\n",
		    name, i, datasize, size, offset);
	if (i < IPANIC_NR_SECTIONS) {
		pdata->offset = offset;
		pdata->total = size;
		pdata->used = datasize;
		strlcpy((char *)pdata->name, name, sizeof(pdata->name));
		pdata->valid = 1;
	}
#ifdef CONFIG_MTK_AEE_SAVE_DEBUGINFO_RESERVED_BUFFER
	header_off += kedump_write_reserved_buffer(header_off, (uint64_t)(pdata),
				       sizeof(struct ipanic_data_header));
#else
	header_off += kedump_dev_write(header_off, (uint64_t)(pdata),
				       sizeof(struct ipanic_data_header));
#endif
}

static int kedump_kernel_info(unsigned long long *offset)
{
	struct elfhdr *ehdr;
	unsigned long sz_misc;
	void *phdr_misc;
	struct elf_note *misc, *miscs;
	char *m_name;
	void *m_data;
	char name[32];
	unsigned int size, datasize;
	unsigned int i;
	unsigned int start_tmp;

	ehdr = kedump_elf_hdr();
	if (ehdr == 0)
		return -1;
	ram_console_set_dump_step(AEE_LKDUMP_MINI_RDUMP);
	datasize = kedump_mini_rdump(ehdr, *offset);
	size = datasize;
	kedump_add2hdr(*offset, size, datasize, "SYS_MINI_RDUMP");
	*offset += datasize;
	phdr_misc = PHDR_INDEX(ehdr, 1);
	miscs = (struct elf_note *)(ehdr->start + PHDR_OFF(ehdr, phdr_misc));
	LOGD("kedump: misc[%p] %llx@%llx\n", phdr_misc,
	     PHDR_SIZE(ehdr, phdr_misc),
	     PHDR_OFF(ehdr, phdr_misc));
	sz_misc = sizeof(struct elf_note) + miscs->n_namesz + miscs->n_descsz;
	LOGD("kedump: miscs[%p], size %x\n", miscs, sz_misc);
	for (i = 0; i < (PHDR_SIZE(ehdr, phdr_misc)) / sz_misc; i++) {
		char klog_first[16];

		memset(klog_first, 0x0, sizeof(klog_first));
		misc = (struct elf_note *)((void *)miscs + sz_misc * i);
		m_name = (char *)misc + sizeof(struct elf_note);
		if (m_name[0] == 'N' && m_name[1] == 'A' && m_name[2] == '\0')
			break;
		m_data = (void *)misc + sizeof(struct elf_note)
					+ misc->n_namesz;
		if (misc->n_descsz == sizeof(struct mrdump_mini_misc_data32)) {
			if (strcmp(m_name, "_KERNEL_LOG_") == 0) {
				start_tmp = ((struct mrdump_mini_misc_data32 *)
						m_data)->start;
				sprintf(klog_first, "_%u", start_tmp ?
					*(unsigned int *)
					(PA_TO_VA((unsigned long)start_tmp))
					: 0);
				((struct mrdump_mini_misc_data32 *)m_data)
				->start = 0;
			}
			datasize = kedump_misc32((struct
						 mrdump_mini_misc_data32 *)
						 m_data, *offset);
			size = ((struct mrdump_mini_misc_data32 *)m_data)->size;
		} else {
			if (strcmp(m_name, "_KERNEL_LOG_") == 0) {
				start_tmp = ((struct
					     mrdump_mini_misc_data64 *)
					     m_data)->start;
				sprintf(klog_first, "_%u", start_tmp ?
					*(unsigned int *)
					(PA_TO_VA((unsigned long)start_tmp))
					: 0);
				((struct mrdump_mini_misc_data64 *)m_data)
				 ->start = 0;
			}
			datasize = kedump_misc64((struct
						 mrdump_mini_misc_data64 *)
						 m_data, *offset);
			size = ((struct mrdump_mini_misc_data64 *)m_data)->size;
		}

		memset(name, 0x0, sizeof(klog_first));
		/* [SYS_]MISC[_RAW] */
		if (m_name[0] == '_')
			strlcpy(name, "SYS", sizeof(name));
		else
			name[0] = 0;
		strlcat (name, m_name, sizeof(name));
		if (m_name[strlen(m_name)-1] == '_')
			strlcat (name,  "RAW", sizeof(name));
		if (klog_first[0] != 0)
			strlcat(name, klog_first, sizeof(name));
		kedump_add2hdr(*offset, size, datasize, name);
		*offset += datasize;
	}
	return 0;
}

static int kedump_ram_console(unsigned long long *offset)
{
	unsigned long sz_misc, addr_misc;
	unsigned int datasize;

	/* ram_console raw log */
	ram_console_set_dump_step(AEE_LKDUMP_RAMCONSOLE_RAW);
	ram_console_addr_size(&addr_misc, &sz_misc);
	if (addr_misc && sz_misc) {
		datasize = kedump_misc((unsigned long long)addr_misc,
				       0, (unsigned int)sz_misc, *offset);
		kc.ram_console_crc = calculate_crc32((void *)addr_misc,
						     sz_misc);
		kedump_add2hdr(*offset, (unsigned long long)sz_misc,
			       datasize, "SYS_RAMCONSOLE_RAW");
		*offset += datasize;
	}
#ifdef MTK_PMIC_FULL_RESET
	/* pstore raw log*/
	ram_console_set_dump_step(AEE_LKDUMP_PSTORE_RAW);
	datasize = kedump_misc((unsigned long long)
			       PA_TO_VA((unsigned long)PSTORE_ADDR), 0,
			       (unsigned int)PSTORE_SIZE, *offset);
	kc.pstore_crc = calculate_crc32((void *)
			PA_TO_VA((unsigned long)PSTORE_ADDR), PSTORE_SIZE);
	kedump_add2hdr(*offset, (unsigned int)PSTORE_SIZE,
		       datasize, "SYS_PSTORE_RAW");
	*offset += datasize;
#endif
	/* save crc data*/
	ram_console_set_dump_step(AEE_LKDUMP_KEDUMP_CRC);
	datasize = kedump_dev_write(*offset, (uint64_t)(&kc),
				    sizeof(struct kedump_crc));
	kedump_add2hdr(*offset, sizeof(struct kedump_crc),
		       datasize, "KEDUMP_CRC");
	*offset += datasize;
	return 0;
}

static int kedump_platform_debug(unsigned long long *offset)
{
	/* platform debug */
	int len = 0;
	unsigned int datasize;
	unsigned int i;

	for (i = 0; i < AEE_PLAT_DEBUG_NUM; i++) {
		offset_plat_debug = *offset;
		length_plat_debug = 0;
		ram_console_set_dump_step(adfi[i].step);
		datasize = kedump_plat_savelog(i, offset_plat_debug,
					       &len, kedump_plat_write);
		if (datasize > 0 && datasize <= adfi[i].filesize) {
			kedump_add2hdr(*offset, length_plat_debug,
				       datasize, adfi[i].filename);
			*offset += datasize;
		}
	}
	return 0;
}

/**
 * fb_mmc_blk_write() - Write/erase MMC in chunks of FASTBOOT_MAX_BLK_WRITE
 *
 * @block_dev: Pointer to block device
 * @start: First block to write/erase
 * @blkcnt: Count of blocks
 * @buffer: Pointer to data buffer for write or NULL for erase
 */

#if CONFIG_IS_ENABLED(MTK_KEDUMP_FLASH_MMC)

#define MAX_BLK_WRITE 16384
static lbaint_t flash_expdb_write_mmc(struct blk_desc *block_dev,
				      lbaint_t start,
				      lbaint_t blkcnt, const void *buffer)
{
	lbaint_t blk = start;
	lbaint_t blks_written;
	lbaint_t cur_blkcnt;
	lbaint_t blks = 0;
	int i;

	aee_pr_crit("info:  blksz: %x ,  size: %x,  start:%x\n", info.blksz,
		    info.size, info.start);
	for (i = 0; i < blkcnt; i += MAX_BLK_WRITE) {
		cur_blkcnt = min((int)blkcnt - i, MAX_BLK_WRITE);

		if (buffer) {
			blks_written = blk_dwrite(block_dev, blk, cur_blkcnt,
						  buffer +
						  (i * block_dev->blksz));
		} else {
			blks_written = blk_derase(block_dev, blk, cur_blkcnt);
		}
		blk += blks_written;
		blks += blks_written;
	}
	return blks;
}
#endif

#if CONFIG_IS_ENABLED(MTK_KEDUMP_FLASH_NAND)
static int _fb_nand_erase(struct mtd_info *mtd, struct part_info *part)
{
	nand_erase_options_t opts;
	int ret;

	memset(&opts, 0, sizeof(opts));
	opts.offset = part->offset;
	opts.length = part->size;
	opts.quiet = 1;

	printf("Erasing blocks 0x%llx to 0x%llx\n",
	       part->offset, part->offset + part->size);

	ret = nand_erase_opts(mtd, &opts);
	if (ret)
		return ret;

	printf(" erased 0x%llx bytes from '%s'\n",
	       part->size, part->name);

	return 0;
}

static int _fb_nand_write(struct mtd_info *mtd, struct part_info *part,
			  void *buffer, u32 offset,
			  size_t length, size_t *written)
{
	int flags = WITH_WR_VERIFY;

#ifdef CONFIG_FASTBOOT_FLASH_NAND_TRIMFFS
	flags |= WITH_DROP_FFS;
#endif

	return nand_write_skip_bad(mtd, offset, &length, written,
				   part->size - (offset - part->offset),
				   buffer, flags);
}

static int fb_nand_lookup(const char *partname,
			  struct mtd_info **mtd,
			  struct part_info **part)
{
	struct mtd_device *dev;
	int ret;
	u8 pnum;

	ret = mtdparts_init();
	if (ret) {
		aee_pr_crit("Cannot initialize MTD partitions\n");
		return ret;
	}

	ret = find_dev_and_part(partname, &dev, &pnum, part);
	if (ret) {
		aee_pr_crit("cannot find partition: '%s'", partname);
		return ret;
	}

	if (dev->id->type != MTD_DEV_TYPE_NAND) {
		aee_pr_crit("partition '%s' is not stored on a NAND device",
			    partname);
		return -EINVAL;
	}

	*mtd = get_nand_dev_by_index(dev->id->num);
	return 0;
}

static lbaint_t flash_expdb_write_nand(lbaint_t start,
				       lbaint_t bytes, const void *buffer)
{
	struct part_info *part;
	struct mtd_info *mtd = NULL;
	int ret;

	ret = fb_nand_lookup("expdb", &mtd, &part);
	if (ret) {
		aee_pr_crit("invalid NAND device");
		return;
	}

	aee_pr_crit("Erasing raw image at offset 0x%llx\n", part->offset);
	ret = _fb_nand_erase(mtd, part);

	if (ret) {
		aee_pr_crit("error erasing the expdb image");
		return;
	}

	aee_pr_crit("Flashing raw image at offset 0x%llx\n", part->offset);

	ret = _fb_nand_write(mtd, part, buffer, 0, bytes, NULL);

	aee_pr_crit(" wrote %u bytes to '%s'\n", bytes, part->name);

	if (ret) {
		aee_pr_crit("error writing the expdb image");
		return;
	}
}
#endif

#ifdef CONFIG_MTK_AEE_SAVE_DEBUGINFO_RESERVED_BUFFER
static void kedump_to_reserved_buffer(void)
{
	unsigned long long offset;
	struct elfhdr *ehdr;
	unsigned int size, datasize;

	header_off = sizeof(struct kedump_reserved_buffer);

	offset = header_off + sizeof(struct ipanic_data_header)
			* IPANIC_NR_SECTIONS;

	/* minidump */
	ehdr = kedump_elf_hdr();
	if (ehdr == 0)
		return -1;
	ram_console_set_dump_step(AEE_LKDUMP_MINI_RDUMP);
	datasize = kedump_mini_rdump(ehdr, offset);
	size = datasize;
	kedump_add2hdr(offset, size, datasize, "SYS_MINI_RDUMP");
	offset += datasize;

	/* platform debug */
	kedump_platform_debug(&offset);
}
#endif

static int kedump_to_expdb(void)
{
	unsigned long long offset;
	unsigned int datasize;
#if CONFIG_IS_ENABLED(MTK_KEDUMP_FLASH_MMC)

	if (kedump_dev_open() != 0)
		return -1;
#endif

	last_dump_step = ram_console_get_dump_step();
	if (last_dump_step != AEE_LKDUMP_CLEAR)
		aee_pr_crit("kedump: last lk dump is not finished at step %u\n",
			    last_dump_step);

	mem_expdb = malloc(MEM_EXPDB_SIZE);
	if (!mem_expdb) {
		aee_pr_crit("kedump: mem_expdb malloc fail!\n");
		return -1;
	}
	aee_pr_crit("kedump: mem_expdb malloc success %p size is 0x%x\n",
		    mem_expdb, MEM_EXPDB_SIZE);
	memset(mem_expdb, 0x0, MEM_EXPDB_SIZE);

	//write header firstly
	panic_header.magic = AEE_IPANIC_MAGIC;
	panic_header.version = AEE_IPANIC_PHDR_VERSION;
	panic_header.size = sizeof(panic_header);
	panic_header.blksize = info.blksz;
	panic_header.partsize = info.size * info.blksz - EXPDB_RESERVED_OTHER;
	aee_pr_crit("kedump: expdb write panic header panic_header addr:0x%llx\n",
		    (uint64_t)(&panic_header));
	kedump_dev_write(0, (uint64_t)(&panic_header), sizeof(panic_header));
	header_off = sizeof(panic_header) - sizeof(struct ipanic_data_header)
		* IPANIC_NR_SECTIONS;
	aee_pr_crit("kedump: block size:0x%zx\n", info.blksz);

	/* reserve space in expdb for panic header */
	offset = ALIGN(sizeof(panic_header), info.blksz);

	kedump_ram_console(&offset);
	kedump_kernel_info(&offset);
	kedump_platform_debug(&offset);

	/* save KEdump flow logs */
	datasize = kedump_dev_write(offset, (uint64_t)logbuf, SZLOG);
	kedump_add2hdr(offset, SZLOG, datasize, "ZAEE_LOG");
	offset += datasize;

	unsigned long long offset_blks = offset / info.blksz;

	if (offset > MEM_EXPDB_SIZE) {
		aee_pr_crit("kedump: total offset 0x%llx over MEM_EXPDB_SIZE\n",
			    offset);
		free((void *)mem_expdb);
		return -1;
	}

	/* in the end, really write info from dram to flash */
#if CONFIG_IS_ENABLED(MTK_KEDUMP_FLASH_MMC)
	lbaint_t blks;

	blks = flash_expdb_write_mmc(dev_desc, info.start, offset_blks, NULL);
	if (blks != offset_blks)
		aee_pr_crit("kedump: to erase 0x%llx but only erase %zd\n",
			    offset_blks, blks);

	blks = flash_expdb_write_mmc(dev_desc, info.start, offset_blks,
				     mem_expdb);
	if (blks != offset_blks)
		aee_pr_crit("kedump: to write 0x%llx but only write %zd\n",
			    offset_blks, blks);
#endif

#if CONFIG_IS_ENABLED(MTK_KEDUMP_FLASH_NAND)
	flash_expdb_write_nand(0, offset, mem_expdb);
#endif

	free((void *)mem_expdb);
	ram_console_set_dump_step(AEE_LKDUMP_CLEAR);
	return 0;
}

static int kedump_avail(void)
{
	void *flag = PA_TO_VA((unsigned long)KE_RESERVED_MEM_ADDR);

	if (((char *)flag)[0] == 0x81 && ((char *)flag)[1] == 'E' &&
	    ((char *)flag)[2] == 'L' && ((char *)flag)[3] == 'F') {
		aee_pr_crit("kedump: already dumped in lk\n");
		return -1;
	}

	if (((char *)flag)[0] == 0x0 && ((char *)flag)[1] == 'E' &&
	    ((char *)flag)[2] == 'L' && ((char *)flag)[3] == 'F') {
		aee_pr_crit("kedump: already dumped in kernel\n");
		return -1;
	}
	return 0;
}

static int kedump_done(void)
{
	void *flag = PA_TO_VA((unsigned long)KE_RESERVED_MEM_ADDR);

	((char *)flag)[0] = 0x81;
	((char *)flag)[1] = 'E';
	((char *)flag)[2] = 'L';
	((char *)flag)[3] = 'F';
	//arch_clean_cache_range((vaddr_t)flag, sizeof(struct elfhdr));

	return 0;
}

/* Dump KE information to expdb */
/*  1: has expception, 0: has no exception */
int kedump_mini(void)
{
	const char *status;

	aee_pr_crit("kedump mini start\n");

	if (lkdump_debug_init())
		aee_pr_crit("kedump: lkdump debug init ok\n");
	else
		aee_pr_crit("kedump: lkdump debug not ready\n");

	aee_pr_crit("kedump avail\n");
	if (kedump_avail())
		return 0;

#ifdef CONFIG_MTK_AEE_SAVE_DEBUGINFO_RESERVED_BUFFER
	aee_pr_crit("kedump to debuginfo_buffer\n");
	kedump_to_reserved_buffer();
#else
	aee_pr_crit("kedump to expdb\n");
	kedump_to_expdb();
#endif

	kedump_done();
	aee_pr_crit("kedump mini done\n");
	return 1;
}
