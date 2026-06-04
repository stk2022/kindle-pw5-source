/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/stddef.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/pm_wakeup.h>
#include <mtk_hifi4dsp_api.h>
#include "adsp_ipi.h"
#include "adsp_ipi_queue.h"
#include "adsp_ipi_platform.h"

char *adsp_core_ids[ADSP_CORE_TOTAL] = {"MT8570 CORE 0"};
struct adsp_common_info adsp_info[ADSP_CORE_TOTAL];
static u32 adsp_info_addr[ADSP_CORE_TOTAL];
static int adsp_is_ready[ADSP_CORE_TOTAL];

/*
 * memory copy from adsp sram
 * @param trg: trg address
 * @param src: src address
 * @param size: memory size
 */
void init_adsp_common_info(enum adsp_core_id core_id)
{
	if (adsp_info_addr[core_id] == 0) {
		if (core_id == ADSP_CORE_0_ID)
			spi_read_register(ADSP_CORE_0_COMMON_INFO_ADDR,
				&adsp_info_addr[core_id], SPI_SPEED_LOW);
		else
			pr_notice("invaild adsp_core_id %d\n", core_id);
	}
}

unsigned int update_adsp_commmon_info(enum adsp_core_id core_id)
{
	init_adsp_common_info(core_id);
	dsp_spi_read(adsp_info_addr[core_id], &adsp_info[core_id],
		sizeof(struct adsp_common_info), SPI_SPEED_LOW);
	if (adsp_info[core_id].magic != ADSP_COMMON_INFO_MAGIC) {
		pr_notice("adsp %d info magic is incorrect!\n", core_id);
		return 0;
	}
	return 1;
}

u32 get_host_to_adsp_status(enum adsp_core_id core_id)
{
	u32 status;
	u32 offset = offsetof(struct adsp_common_info, host_to_adsp_status);

	spi_read_register(adsp_info_addr[core_id]+offset, &status,
						SPI_SPEED_LOW);
	return status;
}

void set_host_to_adsp_status(enum adsp_core_id core_id, u32 status)
{
	u32 offset = offsetof(struct adsp_common_info, host_to_adsp_status);

	spi_set_register32(adsp_info_addr[core_id]+offset, status,
						SPI_SPEED_LOW);
}

void clr_host_to_adsp_status(enum adsp_core_id core_id, u32 status)
{
	u32 offset = offsetof(struct adsp_common_info, host_to_adsp_status);

	spi_clr_register32(adsp_info_addr[core_id]+offset, status,
						SPI_SPEED_LOW);
}

u32 get_adsp_to_host_status(enum adsp_core_id core_id)
{
	u32 status;
	u32 offset = offsetof(struct adsp_common_info, adsp_to_host_status);

	spi_read_register(adsp_info_addr[core_id]+offset, &status,
						SPI_SPEED_LOW);
	pr_notice("adsp core %d adsp_to_host_status is %u\n", core_id, status);
	return status;
}

void set_adsp_to_host_status(enum adsp_core_id core_id, u32 status)
{
	u32 offset = offsetof(struct adsp_common_info, adsp_to_host_status);

	spi_set_register32(adsp_info_addr[core_id]+offset, status,
						SPI_SPEED_LOW);
}

void clr_adsp_to_host_status(enum adsp_core_id core_id, u32 status)
{
	u32 offset = offsetof(struct adsp_common_info, adsp_to_host_status);

	spi_clr_register32(adsp_info_addr[core_id]+offset, status,
						SPI_SPEED_LOW);
}

void memcpy_from_adsp(enum adsp_core_id core_id, void *trg, u32 src, int size)
{
	dsp_spi_read(src, trg, size, SPI_SPEED_LOW);
	clr_adsp_to_host_status(core_id, IPC_MESSAGE_READY);
}

#if IPC_NOTIFY_METHOD == IPC_NOTIFY_BY_EINT
static struct pinctrl *pctrl;
static struct pinctrl_state *pin_state_low;
static struct pinctrl_state *pin_state_high;

static void trigger_host_to_adsp_intertupt(void)
{
	pinctrl_select_state(pctrl, pin_state_low);
	udelay(1000);
	pinctrl_select_state(pctrl, pin_state_high);
	udelay(1000);
	pinctrl_select_state(pctrl, pin_state_low);
	udelay(1000);
}
#endif

void memcpy_to_adsp(enum adsp_core_id core_id, u32 trg, void *src, int size)
{
	dsp_spi_write(trg, src, size, SPI_SPEED_LOW);
	set_host_to_adsp_status(core_id, IPC_MESSAGE_READY);

#if IPC_NOTIFY_METHOD == IPC_NOTIFY_BY_EINT
	/*
	 *trigger interrupt to notify adsp receive message
	 */
	trigger_host_to_adsp_intertupt();
#endif
}

#define IPC_RETRY_LIMIT 5
unsigned int is_ipi_busy(enum adsp_core_id core_id)
{
	u32 status;
	static int retry_cnt;

	status = get_host_to_adsp_status(core_id);
	if (status & IPC_MESSAGE_READY) {
		/*
		 * ipi busy may because interrupt to dsp is lost
		 * try to send intterupt again in eint case
		 */
		if (retry_cnt >= 0)
			retry_cnt++;
		if (retry_cnt % 5 == 0 && retry_cnt / 5 <= IPC_RETRY_LIMIT) {
			pr_notice("interrupt to 8570 may be lost!\n");
#if IPC_NOTIFY_METHOD == IPC_NOTIFY_BY_EINT
			trigger_host_to_adsp_intertupt();
#endif
		}
		if (retry_cnt / 5 > IPC_RETRY_LIMIT)
			retry_cnt = -1;
		udelay(1000);
	} else {
		retry_cnt = 0;
	}
	return (status&IPC_MESSAGE_READY);
}

unsigned int is_adsp_ready(enum adsp_core_id core_id)
{
	if (adsp_is_ready[core_id])
		return 1;

	return adsp_is_ready[core_id] = update_adsp_commmon_info(core_id);
}

/*
 * dispatch adsp irq
 * reset adsp and generate exception if needed
 * @param irq:      irq id
 * @param dev_id:   should be NULL
 */
static struct workqueue_struct *ipi_queue;
static struct work_struct ipi_work;
static struct delayed_work ipi_delayed_work;

void mt8570_ipi_work_handler(struct work_struct *unused)
{
	mt8570_ipi_handler(ADSP_CORE_0_ID);
}

irqreturn_t mt8570_core_0_irq_handler(int irq, void *dev_id)
{
	if (is_from_suspend) {
		pr_notice("resuming flow , queue delayed work!\n");
		queue_delayed_work(ipi_queue,
			&ipi_delayed_work,
			msecs_to_jiffies(50));
		is_from_suspend = 0;
	} else
		queue_work(ipi_queue, &ipi_work);
	return IRQ_HANDLED;
}

int mt8570_ipi_platform_init(struct platform_device *pdev)
{
	ipi_queue = create_singlethread_workqueue("ipi_kworker");
	INIT_WORK(&ipi_work, mt8570_ipi_work_handler);
	INIT_DELAYED_WORK(&ipi_delayed_work, mt8570_ipi_work_handler);

#if IPC_NOTIFY_METHOD == IPC_NOTIFY_BY_EINT
	pctrl = devm_pinctrl_get(&pdev->dev);
	pin_state_low = pinctrl_lookup_state(pctrl, "low");
	pin_state_high = pinctrl_lookup_state(pctrl, "high");

	/* make sure pin state is low at initial */
	pinctrl_select_state(pctrl, pin_state_low);
#endif
	return 0;
}


