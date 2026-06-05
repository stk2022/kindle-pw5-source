/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */
#ifndef _ASM_ARM_FALCON_REVERT_H
#define _ASM_ARM_FALCON_REVERT_H

#include <linux/list.h>

struct falcon_revert_entry {
	struct list_head head;
	struct mm_struct *mm;
};

extern struct list_head falcon_revert_list;
extern spinlock_t falcon_revert_lock;

#endif
