/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/mutex.h>
#include <mt-plat/sync_write.h>
#include "scp_ipi.h"
#include "scp_helper.h"
#include "scp_excep.h"

#define PRINT_THRESHOLD 10000
unsigned char scp_ipi_id_record;
unsigned int scp_ipi_id_record_count;
unsigned int scp_to_ap_ipi_count;
unsigned int ap_to_scp_ipi_count;

struct scp_ipi_desc scp_ipi_desc[SCP_NR_IPI];
struct share_obj *scp_send_obj;
struct share_obj *scp_rcv_obj;
struct mutex scp_ipi_mutex;
/*
 * find an ipi handler and invoke it
 */
void scp_A_ipi_handler(void)
{
#if SCP_IPI_STAMP_SUPPORT
	unsigned int flag = 0;
#endif
	unsigned char scp_id;

	scp_id = scp_rcv_obj->id;
	//pr_err("scp A ipi handler %d\n", scp_id);
	if (scp_id >= SCP_NR_IPI || scp_id <= 0) {
		/* ipi id abnormal*/
		pr_debug("[SCP] A ipi handler id abnormal, id=%d\n", scp_id);
	} else if (scp_ipi_desc[scp_id].handler) {
		memcpy_from_scp(scp_recv_buff,
			(void *)scp_rcv_obj->share_buf,
			scp_rcv_obj->len);

		scp_ipi_desc[scp_id].recv_count++;
		scp_to_ap_ipi_count++;
#if SCP_IPI_STAMP_SUPPORT
		flag = scp_ipi_desc[scp_id].recv_count % SCP_IPI_ID_STAMP_SIZE;
		if (flag < SCP_IPI_ID_STAMP_SIZE) {
			scp_ipi_desc[scp_id].recv_flag[flag] =
					scp_ipi_desc[scp_id].recv_count;
			scp_ipi_desc[scp_id].handler_timestamp[flag] = 0;
			scp_ipi_desc[scp_id].recv_timestamp[flag] =
					arch_counter_get_cntvct();
		}
#endif
		scp_ipi_desc[scp_id].handler(scp_id, scp_recv_buff,
			scp_rcv_obj->len);
#if SCP_IPI_STAMP_SUPPORT
		if (flag < SCP_IPI_ID_STAMP_SIZE)
			scp_ipi_desc[scp_id].handler_timestamp[flag] =
					arch_counter_get_cntvct();
#endif
	} else {
		/* scp_ipi_handler is null or ipi id abnormal */
		pr_debug("[SCP] A ipi handler is null or abnormal, id=%d\n"
								, scp_id);
	}

	/*pr_debug("scp_ipi_handler done\n");*/
}

/*
 * ipi initialize
 */
void scp_A_ipi_init(void)
{
	int i = 0;
#if SCP_IPI_STAMP_SUPPORT
	int j = 0;
#endif

	mutex_init(&scp_ipi_mutex);
	scp_rcv_obj = SCP_A_SHARE_BUFFER;
	scp_send_obj = scp_rcv_obj + 1;
	pr_debug("scp_rcv_obj[SCP_A_ID] = 0x%p\n", scp_rcv_obj);
	pr_debug("scp_send_obj[SCP_A_ID] = 0x%p\n", scp_send_obj);
	memset_io(scp_send_obj, 0, SHARE_BUF_SIZE);
	scp_to_ap_ipi_count = 0;
	ap_to_scp_ipi_count = 0;

	for (i = 0; i < SCP_NR_IPI; i++) {
		scp_ipi_desc[i].recv_count	 = 0;
		scp_ipi_desc[i].success_count  = 0;
		scp_ipi_desc[i].busy_count	 = 0;
		scp_ipi_desc[i].error_count	= 0;
#if SCP_IPI_STAMP_SUPPORT
		for (j = 0; j < SCP_IPI_ID_STAMP_SIZE; j++) {
			scp_ipi_desc[i].recv_timestamp[j] = 0;
			scp_ipi_desc[i].send_timestamp[j] = 0;
			scp_ipi_desc[i].recv_flag[j] = 0;
			scp_ipi_desc[i].send_flag[j] = 0;
			scp_ipi_desc[i].handler_timestamp[j] = 0;
		}
#endif
	}
}


/*
 * API let apps can register an ipi handler to receive IPI
 * @param id:	   IPI ID
 * @param handler:  IPI handler
 * @param name:	 IPI name
 */
enum scp_ipi_status scp_ipi_registration(unsigned char id,
	void (*ipi_handler)(int id, void *data, unsigned int len),
	const char *name)
{
	if (id < SCP_NR_IPI) {
		scp_ipi_desc[id].name = name;

		if (ipi_handler == NULL)
			return SCP_IPI_ERROR;

		scp_ipi_desc[id].handler = ipi_handler;
		return SCP_IPI_DONE;
	} else {
		return SCP_IPI_ERROR;
	}
}
EXPORT_SYMBOL_GPL(scp_ipi_registration);

/*
 * API let apps unregister an ipi handler
 * @param id:	   IPI ID
 */
enum scp_ipi_status scp_ipi_unregistration(unsigned char id)
{
	if (id < SCP_NR_IPI) {
		scp_ipi_desc[id].name = "";
		scp_ipi_desc[id].handler = NULL;
		return SCP_IPI_DONE;
	} else {
		return SCP_IPI_ERROR;
	}
}
EXPORT_SYMBOL_GPL(scp_ipi_unregistration);

/*
 * API for apps to send an IPI to scp
 * @param id:   IPI ID
 * @param buf:  the pointer of data
 * @param len:  data length
 * @param wait: If true, wait (atomically) until data have been gotten by Host
 * @param len:  data length
 */
enum scp_ipi_status scp_ipi_send(unsigned char id, void *buf,
	unsigned int  len, unsigned int wait, enum scp_core_id scp_id)
{
	unsigned int reg;
#if SCP_IPI_STAMP_SUPPORT
	unsigned long flag = 0;
#endif
    /* the variable is for reading back the id from sram
     * to check the if the sram is ready for accesses.
     */
	unsigned char rb_id;

	/*avoid scp log print too much*/
	if (scp_ipi_id_record == id)
		scp_ipi_id_record_count++;
	else
		scp_ipi_id_record_count = 0;

	scp_ipi_id_record = id;
	if (in_interrupt()) {
		if (wait) {
			pr_err("scp_ipi_send: cannot use in isr\n");
			scp_ipi_desc[id].error_count++;
			return SCP_IPI_ERROR;
		}
	}

	if (id >= SCP_NR_IPI) {
		pr_err("scp_ipi_send: ipi id %d wrong\n", id);
		return SCP_IPI_ERROR;
	}
	if (is_scp_ready() == 0) {
		/* pr_err("scp_ipi_send: %s not enabled, id=%d\n"
		 *					, core_ids[scp_id], id);
		 */
		scp_ipi_desc[id].error_count++;
		return SCP_IPI_ERROR;
	}
	if (len > sizeof(scp_send_obj->share_buf) || buf == NULL) {
		pr_err("scp_ipi_send: %s buffer error\n", core_ids);
		scp_ipi_desc[id].error_count++;
		return SCP_IPI_ERROR;
	}
#if SCP_RECOVERY_SUPPORT
	if (atomic_read(&scp_reset_status) == RESET_STATUS_START) {
		scp_ipi_desc[id].error_count++;
		return SCP_IPI_ERROR;
	}
#endif
	if (mutex_trylock(&scp_ipi_mutex) == 0) {
		/*avoid scp ipi send log print too much*/
		if ((scp_ipi_id_record_count % PRINT_THRESHOLD == 0) ||
			(scp_ipi_id_record_count % PRINT_THRESHOLD == 1)) {
			pr_err("scp_ipi_send:%s %d mutex busy\n",
				core_ids, id);
		}
		scp_ipi_desc[id].busy_count++;
		return SCP_IPI_BUSY;
	}

	/*get scp ipi mutex owner*/
	reg = readl(SCP_A_TO_HOST_STA);
	if (!(reg & HOST_IRQ_IRQ2SCP)) {
		/*avoid scp ipi send log print too much*/
		if ((scp_ipi_id_record_count % PRINT_THRESHOLD == 0) ||
			(scp_ipi_id_record_count % PRINT_THRESHOLD == 1)) {
			pr_err("scp_ipi_send:%s %d ap->scp busy\n",
				core_ids, id);
			scp_A_dump_regs();

		}

		scp_ipi_desc[id].busy_count++;
		mutex_unlock(&scp_ipi_mutex);
		return SCP_IPI_BUSY;
	}

	memcpy(scp_send_buff, buf, len);
	memcpy_to_scp((void *)scp_send_obj->share_buf,
					scp_send_buff, len);

	scp_send_obj->len = len;
	scp_send_obj->id = id;

	/*
	 * read the value back to quarantee that scp's sram is ready.
	 */
	rb_id = readl(&(scp_send_obj->id));
	if (rb_id != id) {
		pr_debug("[SCP]ERR: write/read id failed, %d, %d\n",
				id, rb_id);
		WARN_ON(1);
	}

	dsb(SY);
	/*record timestamp*/
	scp_ipi_desc[id].success_count++;
	ap_to_scp_ipi_count++;

#if SCP_IPI_STAMP_SUPPORT
	flag = scp_ipi_desc[id].success_count % SCP_IPI_ID_STAMP_SIZE;
	if (flag < SCP_IPI_ID_STAMP_SIZE) {
		scp_ipi_desc[id].send_flag[flag] =
				scp_ipi_desc[id].success_count;
		scp_ipi_desc[id].send_timestamp[flag] =
				arch_counter_get_cntvct();
	}
#endif
	/*send host to scp ipi*/
	/*pr_debug("scp_ipi_send: SCP A send host to scp ipi\n");*/
	writel(TRI_HOST2SCP, SCP_A_TO_HOST_STA);
	if (wait) {
		reg = readl(SCP_A_TO_HOST_STA);
		while (!(reg & HOST_IRQ_IRQ2SCP))
			reg = readl(SCP_A_TO_HOST_STA);
	}

	mutex_unlock(&scp_ipi_mutex);

	return SCP_IPI_DONE;
}
EXPORT_SYMBOL_GPL(scp_ipi_send);

void scp_ipi_info_dump(unsigned char id)
{
	pr_debug("%u\t%u\t%u\t%u\t%u\t%s\n\r",
				id,
				scp_ipi_desc[id].recv_count,
				scp_ipi_desc[id].success_count,
				scp_ipi_desc[id].busy_count,
				scp_ipi_desc[id].error_count,
				scp_ipi_desc[id].name);
#if SCP_IPI_STAMP_SUPPORT
	/*time stamp*/
	for (i = 0; i < SCP_IPI_ID_STAMP_SIZE; i++) {
		if (scp_ipi_desc[id].recv_timestamp[i] != 0) {
			pr_debug("[SCP]scp->ap recv count:%u, ap recv:%llu, handler fin:%llu\n",
					scp_ipi_desc[id].recv_flag[i],
					scp_ipi_desc[id].recv_timestamp[i],
					scp_ipi_desc[id].handler_timestamp[i]);
		}
	}
	for (i = 0; i < SCP_IPI_ID_STAMP_SIZE; i++) {
		if (scp_ipi_desc[id].send_timestamp[i] != 0) {
			pr_debug("ap->scp send count:%u send time:%llu\n",
					scp_ipi_desc[id].send_flag[i],
					scp_ipi_desc[id].send_timestamp[i]);
		}
	}
#endif

}

void scp_ipi_status_dump_id(unsigned char id)
{
#if SCP_IPI_STAMP_SUPPORT
	/*time stamp*/
	unsigned int i;
#endif

	pr_debug("[SCP]id\trecv\tsuccess\tbusy\terror\tname\n\r");
	scp_ipi_info_dump(id);

}


void scp_ipi_status_dump(void)
{
	unsigned char id;
#if SCP_IPI_STAMP_SUPPORT
	/*time stamp*/
	unsigned int i;
#endif

	pr_debug("[SCP]id\trecv\tsuccess\tbusy\terror\tname\n\r");
	for (id = 0; id < SCP_NR_IPI; id++) {
		if (scp_ipi_desc[id].recv_count > 0 ||
			scp_ipi_desc[id].success_count > 0 ||
			scp_ipi_desc[id].busy_count > 0 ||
			scp_ipi_desc[id].error_count > 0)
			scp_ipi_info_dump(id);
	}
	pr_debug("ap->scp total=%u scp->ap total=%u\n\r",
			ap_to_scp_ipi_count,
			scp_to_ap_ipi_count);
}

