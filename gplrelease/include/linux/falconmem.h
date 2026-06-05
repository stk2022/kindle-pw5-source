/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */
#ifndef _LINUX_FALCONMEM_H_
#define _LINUX_FALCONMEM_H_

struct falcon_meminfo_t {
	phys_addr_t paddr;
	u32 order;
	struct list_head mem_list;
};

void falcon_add_preload(phys_addr_t paddr, u32 order) __attribute__ ((deprecated));
void falcon_del_preload(phys_addr_t paddr, u32 order) __attribute__ ((deprecated));

void falcon_set_preload(phys_addr_t paddr, u32 num);
void falcon_clr_preload(phys_addr_t paddr, u32 num);

void falcon_set_preload_range(void *vaddr, u32 size);
void falcon_clr_preload_range(void *vaddr, u32 size);

#define falcon_set_preload_bytes(addr, size)	\
	do {					\
		u32 page = (PAGE_ALIGN((addr) + (size)) - ((addr) & PAGE_MASK)) >> PAGE_SHIFT; \
		falcon_set_preload((addr), page);			\
	} while(0)

extern struct list_head falcon_meminfo_head;
extern int falcon_meminfo_pages;

#define FALCON_MAX_PFN   ((1ULL << 32) >> PAGE_SHIFT)

extern unsigned long falcon_bitmap[];

#endif
