/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mtu3_dr.h - dual role switch and host glue layer header
 *
 * Copyright (C) 2016 MediaTek Inc.
 *
 * Author: Chunfeng Yun <chunfeng.yun@mediatek.com>
 */

#ifndef _MTU3_DR_H_
#define _MTU3_DR_H_

#if IS_ENABLED(CONFIG_USB_MTU3_HOST)

int ssusb_host_init(struct ssusb_mtk *ssusb,
		    const struct device_node *parent_dn);
void ssusb_host_exit(struct ssusb_mtk *ssusb);
int ssusb_host_enable(struct ssusb_mtk *ssusb);
int ssusb_host_disable(struct ssusb_mtk *ssusb, bool suspend);

#else

static inline int
ssusb_host_init(struct ssusb_mtk *ssusb, const struct device_node *parent_dn)
{
	return 0;
}

static inline void ssusb_host_exit(struct ssusb_mtk *ssusb)
{}

static inline int ssusb_host_enable(struct ssusb_mtk *ssusb)
{
	return 0;
}

static inline int ssusb_host_disable(struct ssusb_mtk *ssusb, bool suspend)
{
	return 0;
}
#endif

#if IS_ENABLED(CONFIG_USB_MTU3_GADGET)
int ssusb_gadget_init(struct ssusb_mtk *ssusb);
void ssusb_gadget_exit(struct ssusb_mtk *ssusb);
#else
static inline int ssusb_gadget_init(struct ssusb_mtk *ssusb)
{
	return 0;
}

static inline void ssusb_gadget_exit(struct ssusb_mtk *ssusb)
{}
#endif

void ssusb_set_force_mode(struct ssusb_mtk *ssusb,
			  enum mtu3_dr_force_mode mode);

#endif		/* _MTU3_DR_H_ */
