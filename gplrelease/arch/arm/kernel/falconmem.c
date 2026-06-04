/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/spinlock.h>
#include <linux/falconmem.h>

static DEFINE_SPINLOCK(lock);

LIST_HEAD(falcon_meminfo_head);
EXPORT_UNUSED_SYMBOL(falcon_meminfo_head);

int falcon_meminfo_pages;
EXPORT_UNUSED_SYMBOL(falcon_meminfo_pages);

DECLARE_BITMAP(falcon_bitmap, FALCON_MAX_PFN);
EXPORT_SYMBOL(falcon_bitmap);

void falcon_set_preload(phys_addr_t paddr, u32 num)
{
	unsigned int pfn = __phys_to_pfn(paddr);

	if ((paddr >> PAGE_SHIFT) > FALCON_MAX_PFN) {
		pr_err("%s paddr=%pa pfn=%x (%d)\n", __func__, &paddr, pfn, num);
		return;
	}

	bitmap_set(falcon_bitmap, pfn, num);

}
EXPORT_SYMBOL(falcon_set_preload);

void falcon_set_preload_range(void *vaddr, u32 size)
{
	void *v;

	for (v = (void *)((u32)vaddr & PAGE_MASK); v < (vaddr + size); v += PAGE_SIZE) {
		if (virt_addr_valid(v))
			falcon_set_preload(virt_to_phys(v), 1);
		else if (is_vmalloc_addr(v))
			falcon_set_preload(page_to_phys(vmalloc_to_page(v)), 1);
		else
			printk("%s vaddr=%p (%d) called from %pF\n", __func__, vaddr, size, __builtin_return_address(0));
	}
}
EXPORT_SYMBOL(falcon_set_preload_range);

void falcon_clr_preload(phys_addr_t paddr, u32 num)
{
	unsigned pfn = __phys_to_pfn(paddr);

	if ((paddr >> PAGE_SHIFT) > FALCON_MAX_PFN) {
		pr_err("%s paddr=%pa pfn=%x (%d)\n", __func__, &paddr, pfn, num);
		return;
	}

	bitmap_clear(falcon_bitmap, pfn, num);

}
EXPORT_SYMBOL(falcon_clr_preload);

void falcon_clr_preload_range(void *vaddr, u32 size)
{
	void *v;

	for (v = (void *)((u32)vaddr & PAGE_MASK); v < (vaddr + size); v += PAGE_SIZE) {
		if (virt_addr_valid(v))
			falcon_clr_preload(virt_to_phys(v), 1);
		else if (is_vmalloc_addr(v))
			falcon_clr_preload(page_to_phys(vmalloc_to_page(v)), 1);
		else
			printk("%s vaddr=%p (%d) called from %pF\n", __func__, vaddr, size, __builtin_return_address(0));
	}
}
EXPORT_SYMBOL(falcon_clr_preload_range);

void falcon_add_preload(phys_addr_t paddr, u32 order)
{
	unsigned long flags;
	struct falcon_meminfo_t *item;

	item = kmalloc(sizeof(struct falcon_meminfo_t),
		       GFP_KERNEL);
	item->paddr = paddr;
	item->order = order;

	spin_lock_irqsave(&lock, flags);
	list_add(&(item->mem_list), &falcon_meminfo_head);
	falcon_meminfo_pages += 1 << order;
	spin_unlock_irqrestore(&lock, flags);
}
EXPORT_UNUSED_SYMBOL(falcon_add_preload);

void falcon_del_preload(phys_addr_t paddr, u32 order)
{
	unsigned long flags;
	struct falcon_meminfo_t *item;

	spin_lock_irqsave(&lock, flags);
	list_for_each_entry(item, &falcon_meminfo_head, mem_list) {
		if (item->paddr == paddr && item->order == order) {

			list_del(&item->mem_list);
			falcon_meminfo_pages -= 1 << item->order;
			spin_unlock_irqrestore(&lock, flags);
			kfree(item);
			return;
		}
	}
	spin_unlock_irqrestore(&lock, flags);
#ifdef CONFIG_ARM_LPAE
	pr_warn("%s: no entry phys=%llx order=%d\n", __func__, paddr, order);
#else
	pr_warn("%s: no entry phys=%x order=%d\n", __func__, paddr, order);
#endif
}
EXPORT_UNUSED_SYMBOL(falcon_del_preload);
